// HC-SR04 acquisition and mode-dependent MIDI output.

float readUltrasonicDistanceCm(byte sensorIndex) {
  const byte trigPin = ULTRASONIC_TRIG_PIN[sensorIndex];
  const byte echoPin = ULTRASONIC_ECHO_PIN[sensorIndex];

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  const unsigned long echoDuration = pulseIn(
    echoPin,
    HIGH,
    ULTRASONIC_ECHO_TIMEOUT_US
  );

  if (echoDuration == 0) return 0.0f;
  return (float)echoDuration / 58.0f;
}

void updateUltrasonicSensorState(byte sensorIndex, float newDistanceCm) {
  const bool inPlayingRange =
    newDistanceCm >= ULTRASONIC_MIN_DISTANCE_CM &&
    newDistanceCm <= ULTRASONIC_MAX_DISTANCE_CM;

  if (!inPlayingRange) {
    if (ultrasonicInvalidReadings[sensorIndex] < 255) {
      ultrasonicInvalidReadings[sensorIndex]++;
    }

    if (ultrasonicInvalidReadings[sensorIndex] >= ULTRASONIC_INVALID_READINGS_TO_RELEASE) {
      ultrasonicSensorActive[sensorIndex] = false;
      ultrasonicFilterInitialized[sensorIndex] = false;
    }

#ifdef DEBUG
    Serial.print(F("Sensor "));
    Serial.print(sensorIndex + 1);
    Serial.print(F(" fora da faixa; leitura: "));
    Serial.println(newDistanceCm, 1);
#endif
    return;
  }

  ultrasonicInvalidReadings[sensorIndex] = 0;

  if (!ultrasonicFilterInitialized[sensorIndex]) {
    ultrasonicDistanceCm[sensorIndex] = newDistanceCm;
    ultrasonicFilterInitialized[sensorIndex] = true;
  } else {
    ultrasonicDistanceCm[sensorIndex] +=
      ULTRASONIC_FILTER_ALPHA *
      (newDistanceCm - ultrasonicDistanceCm[sensorIndex]);
  }

  ultrasonicSensorActive[sensorIndex] = true;

#ifdef DEBUG
  Serial.print(F("Sensor "));
  Serial.print(sensorIndex + 1);
  Serial.print(F(" distancia: "));
  Serial.print(newDistanceCm, 1);
  Serial.print(F(" cm | filtrada: "));
  Serial.println(ultrasonicDistanceCm[sensorIndex], 1);
#endif
}

int distanceToScalePosition(float distanceCm) {
  int position = (int)round(fscale(
    ULTRASONIC_MIN_DISTANCE_CM,
    ULTRASONIC_MAX_DISTANCE_CM,
    SCALE_NOTE_COUNT - 1,
    0,
    distanceCm,
    IR_curve
  ));
  return constrain(position, 0, SCALE_NOTE_COUNT - 1);
}

byte distanceToMidiValue(float distanceCm, byte minimumValue) {
  int value = (int)round(fscale(
    ULTRASONIC_MIN_DISTANCE_CM,
    ULTRASONIC_MAX_DISTANCE_CM,
    127,
    minimumValue,
    distanceCm,
    IR_curve
  ));
  value = constrain(value, minimumValue, 127);
  return (byte)value;
}

int calculateNoteForSensor(byte sensorIndex) {
  const int scalePosition = distanceToScalePosition(ultrasonicDistanceCm[sensorIndex]);
  const int rootMidiNote = 48 + rootPitchClass + octaveOffsets[octaveIndex];
  const int note = rootMidiNote + scaleIntervals[scaleModeIndex][scalePosition];
  return constrain(note, 0, 127);
}

bool otherSensorHoldsSameNote(byte sensorIndex, int noteNumber, byte channel) {
  const byte otherSensor = (sensorIndex == SENSOR_1) ? SENSOR_2 : SENSOR_1;
  return activeNote[otherSensor] == noteNumber &&
         activeNoteChannel[otherSensor] == channel;
}

void stopNoteForSensor(byte sensorIndex) {
  if (activeNote[sensorIndex] >= 0) {
    // If both sensors hold the same pitch on the same channel, send Note Off
    // only when the second sensor releases it as well.
    if (!otherSensorHoldsSameNote(
          sensorIndex,
          activeNote[sensorIndex],
          activeNoteChannel[sensorIndex]
        )) {
      noteOff(activeNoteChannel[sensorIndex], (byte)activeNote[sensorIndex], 0);
      MidiUSB.flush();
    }

    lastPlayedNote[sensorIndex] = activeNote[sensorIndex];
    activeNote[sensorIndex] = -1;
    noteStoppedAt[sensorIndex] = millis();
    lastNoteScreenUntil = millis() + DISPLAY_NOTE_RELEASE_DELAY_MS;
  }
}

void processNoteSensor(byte sensorIndex) {
  if (!ultrasonicSensorActive[sensorIndex]) {
    stopNoteForSensor(sensorIndex);
    return;
  }

  const int nextNote = calculateNoteForSensor(sensorIndex);
  if (activeNote[sensorIndex] != nextNote || activeNoteChannel[sensorIndex] != midiChannel) {
    stopNoteForSensor(sensorIndex);

    if (!otherSensorHoldsSameNote(sensorIndex, nextNote, midiChannel)) {
      noteOn(midiChannel, (byte)nextNote, globalVelocity);
      MidiUSB.flush();
    }

    activeNote[sensorIndex] = nextNote;
    activeNoteChannel[sensorIndex] = midiChannel;
    lastPlayedNote[sensorIndex] = nextNote;
    lastNoteSensor = sensorIndex;
    noteStoppedAt[sensorIndex] = 0;
    lastNoteScreenUntil = 0;

#ifdef DEBUG
    Serial.print(F("Sensor "));
    Serial.print(sensorIndex + 1);
    Serial.print(F(" Note On: "));
    Serial.print(noteNames[nextNote % 12]);
    Serial.print((nextNote / 12) - 1);
    Serial.print(F(" velocity="));
    Serial.println(globalVelocity);
#endif
  }
}

void clearSensorCC(byte sensorIndex) {
  if (lastSentCCValue[sensorIndex] >= 0 &&
      lastSentCCNumber[sensorIndex] >= 0 &&
      lastSentCCChannel[sensorIndex] >= 0) {
    controlChange(
      (byte)lastSentCCChannel[sensorIndex],
      (byte)lastSentCCNumber[sensorIndex],
      0
    );
    MidiUSB.flush();
  }

  lastSentCCValue[sensorIndex] = -1;
  lastSentCCNumber[sensorIndex] = -1;
  lastSentCCChannel[sensorIndex] = -1;
}

void sendSensorCC(byte sensorIndex, byte value) {
  sensorCCValue[sensorIndex] = value;

  if (lastSentCCValue[sensorIndex] != value ||
      lastSentCCNumber[sensorIndex] != sensorCCNumber[sensorIndex] ||
      lastSentCCChannel[sensorIndex] != midiChannel) {
    controlChange(midiChannel, sensorCCNumber[sensorIndex], value);
    MidiUSB.flush();

    lastSentCCValue[sensorIndex] = value;
    lastSentCCNumber[sensorIndex] = sensorCCNumber[sensorIndex];
    lastSentCCChannel[sensorIndex] = midiChannel;

#ifdef DEBUG
    Serial.print(F("Sensor "));
    Serial.print(sensorIndex + 1);
    Serial.print(F(" CC"));
    Serial.print(sensorCCNumber[sensorIndex]);
    Serial.print(F(" = "));
    Serial.println(value);
#endif
  }
}

void processCCSensor(byte sensorIndex) {
  byte value = 0;
  if (ultrasonicSensorActive[sensorIndex]) {
    value = distanceToMidiValue(ultrasonicDistanceCm[sensorIndex], 0);
  }
  sendSensorCC(sensorIndex, value);
}

void processVelocitySensor(byte sensorIndex) {
  // Intentionally do nothing when the hand leaves the zone: the last valid
  // global velocity is retained until a new valid distance is measured.
  if (!ultrasonicSensorActive[sensorIndex]) return;

  globalVelocity = distanceToMidiValue(ultrasonicDistanceCm[sensorIndex], 1);

#ifdef DEBUG
  Serial.print(F("Global velocity = "));
  Serial.println(globalVelocity);
#endif
}

void processSensorByMode(byte sensorIndex) {
  switch (sensorMode[sensorIndex]) {
    case SENSOR_MODE_NOTES:
      processNoteSensor(sensorIndex);
      break;

    case SENSOR_MODE_VELOCITY:
      stopNoteForSensor(sensorIndex);
      processVelocitySensor(sensorIndex);
      break;

    case SENSOR_MODE_CC:
      stopNoteForSensor(sensorIndex);
      processCCSensor(sensorIndex);
      break;
  }
}

// Alternates sensors and keeps at least 65ms between trigger pulses.
// Each sensor is therefore sampled approximately every 130ms.
void serviceUltrasonicSensors() {
  static byte nextSensor = SENSOR_1;
  static bool firstMeasurement = true;
  static unsigned long lastMeasurementAt = 0;

  const unsigned long now = millis();
  if (!firstMeasurement &&
      (unsigned long)(now - lastMeasurementAt) < ULTRASONIC_TRIGGER_INTERVAL_MS) {
    return;
  }

  firstMeasurement = false;
  const float distanceCm = readUltrasonicDistanceCm(nextSensor);
  lastMeasurementAt = millis();

  updateUltrasonicSensorState(nextSensor, distanceCm);
  processSensorByMode(nextSensor);

  nextSensor = (nextSensor == SENSOR_1) ? SENSOR_2 : SENSOR_1;
}
