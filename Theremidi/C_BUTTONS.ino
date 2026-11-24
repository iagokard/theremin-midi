void initializeButtons() {
  for (byte i = 0; i < N_MAIN_BUTTONS; i++) {
    pinMode(MAIN_BUTTON_PINS[i], INPUT_PULLUP);
    buttonStates[i].rawState = HIGH;
    buttonStates[i].stableState = HIGH;
    buttonStates[i].lastChangeAt = 0;
  }

#if ENABLE_CC_STEP10_BUTTONS
  pinMode(OPTIONAL_CC_MINUS_10_PIN, INPUT_PULLUP);
  pinMode(OPTIONAL_CC_PLUS_10_PIN, INPUT_PULLUP);
  buttonStates[BTN_CC_MINUS_10].rawState = HIGH;
  buttonStates[BTN_CC_MINUS_10].stableState = HIGH;
  buttonStates[BTN_CC_MINUS_10].lastChangeAt = 0;
  buttonStates[BTN_CC_PLUS_10].rawState = HIGH;
  buttonStates[BTN_CC_PLUS_10].stableState = HIGH;
  buttonStates[BTN_CC_PLUS_10].lastChangeAt = 0;
#endif
}

void buttons() {
  for (byte i = 0; i < N_MAIN_BUTTONS; i++) {
    serviceOneButton(i, MAIN_BUTTON_PINS[i]);
  }

#if ENABLE_CC_STEP10_BUTTONS
  serviceOneButton(BTN_CC_MINUS_10, OPTIONAL_CC_MINUS_10_PIN);
  serviceOneButton(BTN_CC_PLUS_10, OPTIONAL_CC_PLUS_10_PIN);
#endif
}

void serviceOneButton(byte buttonId, byte pin) {
  const byte reading = digitalRead(pin);
  const unsigned long now = millis();

  if (reading != buttonStates[buttonId].rawState) {
    buttonStates[buttonId].rawState = reading;
    buttonStates[buttonId].lastChangeAt = now;
  }

  if ((unsigned long)(now - buttonStates[buttonId].lastChangeAt) >= BUTTON_DEBOUNCE_MS &&
      reading != buttonStates[buttonId].stableState) {
    buttonStates[buttonId].stableState = reading;

    if (reading == LOW) {
      handleButtonPressed(buttonId);
    } else {
      handleButtonReleased(buttonId);
    }
  }
}

void handleButtonPressed(byte buttonId) {
  switch (buttonId) {
    case BTN_SENSOR_SELECT:
      selectedSensor = (selectedSensor + 1) % N_ULTRASONIC_SENSORS;
      requestConfigDisplay();
      break;

    case BTN_SENSOR_MODE:
      cycleSelectedSensorMode();
      requestConfigDisplay();
      break;

    case BTN_CC_MINUS:
      changeSelectedCCNumber(-1);
      requestConfigDisplay();
      break;

    case BTN_CC_PLUS:
      changeSelectedCCNumber(1);
      requestConfigDisplay();
      break;

    case BTN_OCTAVE_MINUS:
      if (octaveIndex > 0) octaveIndex--;
      refreshActiveNotes();
      requestConfigDisplay();
      break;

    case BTN_OCTAVE_PLUS:
      if (octaveIndex < 4) octaveIndex++;
      refreshActiveNotes();
      requestConfigDisplay();
      break;

    case BTN_CHANNEL_MINUS:
      changeMidiChannel(-1);
      requestConfigDisplay();
      break;

    case BTN_CHANNEL_PLUS:
      changeMidiChannel(1);
      requestConfigDisplay();
      break;

    case BTN_ROOT_MINUS:
      rootPitchClass = (rootPitchClass + 11) % 12;
      refreshActiveNotes();
      requestConfigDisplay();
      break;

    case BTN_ROOT_PLUS:
      rootPitchClass = (rootPitchClass + 1) % 12;
      refreshActiveNotes();
      requestConfigDisplay();
      break;

    case BTN_SCALE_MODE:
      scaleModeIndex = (scaleModeIndex + 1) % SCALE_COUNT;
      refreshActiveNotes();
      requestConfigDisplay();
      break;

    case BTN_SUSTAIN:
      if (!sustainIsPressed) {
        sustainIsPressed = true;
        sustainChannel = midiChannel;
        controlChange(sustainChannel, SUSTAIN_CC, 127);
        MidiUSB.flush();
      }
      break;

    case BTN_CC_MINUS_10:
#if ENABLE_CC_STEP10_BUTTONS
      changeSelectedCCNumber(-10);
      requestConfigDisplay();
#endif
      break;

    case BTN_CC_PLUS_10:
#if ENABLE_CC_STEP10_BUTTONS
      changeSelectedCCNumber(10);
      requestConfigDisplay();
#endif
      break;
  }
}

void handleButtonReleased(byte buttonId) {
  if (buttonId == BTN_SUSTAIN && sustainIsPressed) {
    controlChange(sustainChannel, SUSTAIN_CC, 0);
    MidiUSB.flush();
    sustainIsPressed = false;
  }
}

void cycleSelectedSensorMode() {
  const byte oldMode = sensorMode[selectedSensor];
  const byte newMode = (oldMode + 1) % 3;

  if (oldMode == SENSOR_MODE_NOTES) {
    stopNoteForSensor(selectedSensor);
  } else if (oldMode == SENSOR_MODE_CC) {
    clearSensorCC(selectedSensor);
  }

  sensorMode[selectedSensor] = newMode;

  if (newMode == SENSOR_MODE_NOTES && ultrasonicSensorActive[selectedSensor]) {
    processNoteSensor(selectedSensor);
  } else if (newMode == SENSOR_MODE_CC) {
    processCCSensor(selectedSensor);
  }

#ifdef DEBUG
  Serial.print(F("Sensor "));
  Serial.print(selectedSensor + 1);
  Serial.print(F(" mode: "));
  Serial.println(sensorModeNames[newMode]);
#endif
}

void changeSelectedCCNumber(int amount) {
  clearSensorCC(selectedSensor);
  int newNumber = (int)sensorCCNumber[selectedSensor] + amount;
  while (newNumber < 0) newNumber += 128;
  while (newNumber > 127) newNumber -= 128;
  sensorCCNumber[selectedSensor] = (byte)newNumber;

  if (sensorMode[selectedSensor] == SENSOR_MODE_CC) {
    processCCSensor(selectedSensor);
  }
}

void changeMidiChannel(int amount) {
  // Clear currently active CC values on their previous channels before changing channel.
  for (byte i = 0; i < N_ULTRASONIC_SENSORS; i++) {
    clearSensorCC(i);
  }

  int nextChannel = (int)midiChannel + amount;
  if (nextChannel < 0) nextChannel = 0;
  if (nextChannel > 15) nextChannel = 15;
  midiChannel = (byte)nextChannel;

  refreshActiveNotes();
  for (byte i = 0; i < N_ULTRASONIC_SENSORS; i++) {
    if (sensorMode[i] == SENSOR_MODE_CC) {
      processCCSensor(i);
    }
  }
}

void refreshActiveNotes() {
  for (byte i = 0; i < N_ULTRASONIC_SENSORS; i++) {
    if (sensorMode[i] == SENSOR_MODE_NOTES && ultrasonicSensorActive[i]) {
      processNoteSensor(i);
    }
  }
}
