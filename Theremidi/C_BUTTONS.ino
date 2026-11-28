void initializeButtons() {
  for (byte i = 0; i < N_MAIN_BUTTONS; i++) {
    pinMode(MAIN_BUTTON_PINS[i], INPUT_PULLUP);
    buttonStates[i].rawState = HIGH;
    buttonStates[i].stableState = HIGH;
    buttonStates[i].lastChangeAt = 0;
  }

#if ENABLE_DISTANCE_CALIBRATION_BUTTONS
  pinMode(DISTANCE_MAX_MINUS_PIN, INPUT_PULLUP);
  pinMode(DISTANCE_MAX_PLUS_PIN, INPUT_PULLUP);
  buttonStates[BTN_DISTANCE_MAX_MINUS].rawState = HIGH;
  buttonStates[BTN_DISTANCE_MAX_MINUS].stableState = HIGH;
  buttonStates[BTN_DISTANCE_MAX_MINUS].lastChangeAt = 0;
  buttonStates[BTN_DISTANCE_MAX_PLUS].rawState = HIGH;
  buttonStates[BTN_DISTANCE_MAX_PLUS].stableState = HIGH;
  buttonStates[BTN_DISTANCE_MAX_PLUS].lastChangeAt = 0;
#endif
}

void buttons() {
  for (byte i = 0; i < N_MAIN_BUTTONS; i++) {
    serviceOneButton(i, MAIN_BUTTON_PINS[i]);
  }

#if ENABLE_DISTANCE_CALIBRATION_BUTTONS
  serviceOneButton(BTN_DISTANCE_MAX_MINUS, DISTANCE_MAX_MINUS_PIN);
  serviceOneButton(BTN_DISTANCE_MAX_PLUS, DISTANCE_MAX_PLUS_PIN);
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
      changeSelectedOctave(-1);
      requestConfigDisplay();
      break;

    case BTN_OCTAVE_PLUS:
      changeSelectedOctave(1);
      requestConfigDisplay();
      break;

    case BTN_CHANNEL_MINUS:
      changeSelectedMidiChannel(-1);
      requestConfigDisplay();
      break;

    case BTN_CHANNEL_PLUS:
      changeSelectedMidiChannel(1);
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
      for (byte i = 0; i < N_ULTRASONIC_SENSORS; i++) {
        currentScalePosition[i] = -1;
      }
      refreshActiveNotes();
      requestConfigDisplay();
      break;

    case BTN_SUSTAIN:
      toggleGlobalSustain();
      requestConfigDisplay();
      break;

    case BTN_DISTANCE_MAX_MINUS:
#if ENABLE_DISTANCE_CALIBRATION_BUTTONS
      adjustSelectedSensorMaximumDistance(-DISTANCE_CALIBRATION_STEP_CM);
      requestConfigDisplay();
#endif
      break;

    case BTN_DISTANCE_MAX_PLUS:
#if ENABLE_DISTANCE_CALIBRATION_BUTTONS
      adjustSelectedSensorMaximumDistance(DISTANCE_CALIBRATION_STEP_CM);
      requestConfigDisplay();
#endif
      break;
  }
}

// Sustain is a toggle: the press event changes its state; release does nothing.
void handleButtonReleased(byte buttonId) {
  (void)buttonId;
}

void toggleGlobalSustain() {
  if (sustainIsActive) {
    releaseGlobalSustain();
  } else {
    pressGlobalSustain();
  }
}

void pressGlobalSustain() {
  if (sustainIsActive) return;

  sustainIsActive = true;
  sustainChannelCount = 0;

  for (byte sensor = 0; sensor < N_ULTRASONIC_SENSORS; sensor++) {
    const byte channel = sensorMidiChannel[sensor];
    bool alreadyStored = false;

    for (byte i = 0; i < sustainChannelCount; i++) {
      if (sustainChannels[i] == channel) {
        alreadyStored = true;
        break;
      }
    }

    if (!alreadyStored) {
      sustainChannels[sustainChannelCount++] = channel;
      controlChange(channel, SUSTAIN_CC, 127);
    }
  }

  MidiUSB.flush();
}

void releaseGlobalSustain() {
  for (byte i = 0; i < sustainChannelCount; i++) {
    controlChange(sustainChannels[i], SUSTAIN_CC, 0);
  }
  MidiUSB.flush();
  sustainChannelCount = 0;
  sustainIsActive = false;
}

void syncGlobalSustainChannels() {
  if (!sustainIsActive) return;

  byte nextChannels[N_ULTRASONIC_SENSORS] = {0, 0};
  byte nextCount = 0;

  for (byte sensor = 0; sensor < N_ULTRASONIC_SENSORS; sensor++) {
    const byte channel = sensorMidiChannel[sensor];
    bool alreadyStored = false;
    for (byte i = 0; i < nextCount; i++) {
      if (nextChannels[i] == channel) {
        alreadyStored = true;
        break;
      }
    }
    if (!alreadyStored) nextChannels[nextCount++] = channel;
  }

  // Release channels no longer assigned to either sensor.
  for (byte i = 0; i < sustainChannelCount; i++) {
    bool stillUsed = false;
    for (byte j = 0; j < nextCount; j++) {
      if (sustainChannels[i] == nextChannels[j]) {
        stillUsed = true;
        break;
      }
    }
    if (!stillUsed) controlChange(sustainChannels[i], SUSTAIN_CC, 0);
  }

  // Apply Sustain to newly assigned channels.
  for (byte i = 0; i < nextCount; i++) {
    bool alreadyActive = false;
    for (byte j = 0; j < sustainChannelCount; j++) {
      if (nextChannels[i] == sustainChannels[j]) {
        alreadyActive = true;
        break;
      }
    }
    if (!alreadyActive) controlChange(nextChannels[i], SUSTAIN_CC, 127);
  }

  sustainChannelCount = nextCount;
  for (byte i = 0; i < nextCount; i++) sustainChannels[i] = nextChannels[i];
  MidiUSB.flush();
}

void adjustSelectedSensorMaximumDistance(float amountCm) {
  const byte sensor = selectedSensor;
  float nextMaximum = ultrasonicMaxDistanceCm[sensor] + amountCm;
  const float minimumAllowed =
    ultrasonicMinDistanceCm[sensor] + ULTRASONIC_MIN_PLAYING_SPAN_CM;

  if (nextMaximum < minimumAllowed) nextMaximum = minimumAllowed;
  if (nextMaximum > ULTRASONIC_HARD_MAX_DISTANCE_CM) {
    nextMaximum = ULTRASONIC_HARD_MAX_DISTANCE_CM;
  }

  if (nextMaximum == ultrasonicMaxDistanceCm[sensor]) return;

  ultrasonicMaxDistanceCm[sensor] = nextMaximum;
  currentScalePosition[sensor] = -1;

#ifdef DEBUG
  Serial.print(F("Sensor "));
  Serial.print(sensor + 1);
  Serial.print(F(" max distance = "));
  Serial.print(ultrasonicMaxDistanceCm[sensor], 1);
  Serial.println(F(" cm"));
#endif
}

void cycleSelectedSensorMode() {
  const byte oldMode = sensorMode[selectedSensor];
  const byte newMode = (oldMode + 1) % SENSOR_MODE_COUNT;

  if (oldMode == SENSOR_MODE_NOTES) {
    stopNoteForSensor(selectedSensor);
  } else if (oldMode == SENSOR_MODE_CC) {
    clearSensorCC(selectedSensor);
  } else if (oldMode == SENSOR_MODE_PITCH_BEND) {
    stopPitchBendForSensor(selectedSensor);
  }

  currentScalePosition[selectedSensor] = -1;
  sensorMode[selectedSensor] = newMode;

  if (newMode == SENSOR_MODE_NOTES && ultrasonicSensorActive[selectedSensor]) {
    processNoteSensor(selectedSensor);
  } else if (newMode == SENSOR_MODE_CC) {
    processCCSensor(selectedSensor);
  } else if (newMode == SENSOR_MODE_PITCH_BEND && ultrasonicSensorActive[selectedSensor]) {
    processPitchBendSensor(selectedSensor);
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

void changeSelectedOctave(int amount) {
  int nextOctave = (int)sensorOctaveIndex[selectedSensor] + amount;
  nextOctave = constrain(nextOctave, 0, 4);
  if (nextOctave == sensorOctaveIndex[selectedSensor]) return;

  sensorOctaveIndex[selectedSensor] = (byte)nextOctave;
  currentScalePosition[selectedSensor] = -1;
  refreshSensorMusicalOutput(selectedSensor);
}

void changeSelectedMidiChannel(int amount) {
  int nextChannel = (int)sensorMidiChannel[selectedSensor] + amount;
  nextChannel = constrain(nextChannel, 0, 15);
  if (nextChannel == sensorMidiChannel[selectedSensor]) return;

  // Clear a CC using its previous channel before storing the new channel.
  clearSensorCC(selectedSensor);
  sensorMidiChannel[selectedSensor] = (byte)nextChannel;
  syncGlobalSustainChannels();
  refreshSensorMusicalOutput(selectedSensor);

  if (sensorMode[selectedSensor] == SENSOR_MODE_CC) {
    processCCSensor(selectedSensor);
  }
}

void refreshSensorMusicalOutput(byte sensorIndex) {
  if (sensorMode[sensorIndex] == SENSOR_MODE_NOTES) {
    processNoteSensor(sensorIndex);
  } else if (sensorMode[sensorIndex] == SENSOR_MODE_PITCH_BEND) {
    processPitchBendSensor(sensorIndex);
  }
}

void refreshActiveNotes() {
  for (byte i = 0; i < N_ULTRASONIC_SENSORS; i++) {
    refreshSensorMusicalOutput(i);
  }
}
