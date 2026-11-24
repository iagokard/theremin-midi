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
      currentScalePosition[sensorIndex] = -1;
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
    const float difference = newDistanceCm - ultrasonicDistanceCm[sensorIndex];
    const bool largeChange =
      difference >= ULTRASONIC_FILTER_FAST_THRESHOLD_CM ||
      difference <= -ULTRASONIC_FILTER_FAST_THRESHOLD_CM;
    const float alpha = largeChange
      ? ULTRASONIC_FILTER_ALPHA_FAST
      : ULTRASONIC_FILTER_ALPHA;
    ultrasonicDistanceCm[sensorIndex] += alpha * difference;
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

float distanceToScalePositionFloat(float distanceCm, byte noteCount) {
  const int lastPosition = (int)noteCount - 1;
  float position = fscale(
    ULTRASONIC_MIN_DISTANCE_CM,
    ULTRASONIC_MAX_DISTANCE_CM,
    lastPosition,
    0,
    distanceCm,
    IR_curve
  );
  return constrain(position, 0.0f, (float)lastPosition);
}

// Schmitt-style note selection prevents repeated Note On/Off events when a
// hand hovers around the boundary between adjacent scale positions.
int distanceToScalePosition(byte sensorIndex) {
  const byte noteCount = scaleNoteCounts[scaleModeIndex];
  const int lastPosition = (int)noteCount - 1;
  const float rawPosition = distanceToScalePositionFloat(
    ultrasonicDistanceCm[sensorIndex],
    noteCount
  );

  int stablePosition = currentScalePosition[sensorIndex];
  if (stablePosition < 0 || stablePosition > lastPosition) {
    stablePosition = (int)round(rawPosition);
  } else if (rawPosition > stablePosition + 0.5f + NOTE_HYSTERESIS_STEPS ||
             rawPosition < stablePosition - 0.5f - NOTE_HYSTERESIS_STEPS) {
    stablePosition = (int)round(rawPosition);
  }

  stablePosition = constrain(stablePosition, 0, lastPosition);
  currentScalePosition[sensorIndex] = (int8_t)stablePosition;
  return stablePosition;
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

int distanceToPitchBendValue(float distanceCm) {
  // Near the sensor = maximum positive bend; far = maximum negative bend.
  const int value = (int)round(fscale(
    ULTRASONIC_MIN_DISTANCE_CM,
    ULTRASONIC_MAX_DISTANCE_CM,
    16383,
    0,
    distanceCm,
    IR_curve
  ));
  return constrain(value, 0, 16383);
}

int calculateNoteForSensor(byte sensorIndex) {
  const int scalePosition = distanceToScalePosition(sensorIndex);
  const int rootMidiNote = 48 + rootPitchClass + octaveOffsets[sensorOctaveIndex[sensorIndex]];
  const int note = rootMidiNote + scaleIntervals[scaleModeIndex][scalePosition];
  return constrain(note, 0, 127);
}

int calculatePitchBendBaseNote(byte sensorIndex) {
  const int rootMidiNote = 48 + rootPitchClass + octaveOffsets[sensorOctaveIndex[sensorIndex]];
  return constrain(rootMidiNote, 0, 127);
}

bool otherSensorHoldsSameNote(byte sensorIndex, int noteNumber, byte channel) {
  const byte otherSensor = (sensorIndex == SENSOR_1) ? SENSOR_2 : SENSOR_1;
  return activeNote[otherSensor] == noteNumber &&
         activeNoteChannel[otherSensor] == channel;
}

bool otherSensorUsesPitchBendChannel(byte sensorIndex, byte channel) {
  const byte otherSensor = (sensorIndex == SENSOR_1) ? SENSOR_2 : SENSOR_1;
  return sensorMode[otherSensor] == SENSOR_MODE_PITCH_BEND &&
         activeNote[otherSensor] >= 0 &&
         activeNoteChannel[otherSensor] == channel &&
         lastPitchBendValue[otherSensor] >= 0;
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
    lastNoteSensor = sensorIndex;
    lastNoteScreenUntil = millis() + DISPLAY_NOTE_RELEASE_DELAY_MS;
  }
}

void stopPitchBendForSensor(byte sensorIndex) {
  const byte previousChannel = activeNoteChannel[sensorIndex];
  stopNoteForSensor(sensorIndex);

  // Pitch Bend is channel-wide. Do not center it while the other sensor is
  // actively using Pitch Bend on the same channel.
  if (lastPitchBendValue[sensorIndex] >= 0 &&
      !otherSensorUsesPitchBendChannel(sensorIndex, previousChannel)) {
    pitchBend(previousChannel, PITCH_BEND_CENTER);
    MidiUSB.flush();
  }

  lastPitchBendValue[sensorIndex] = -1;
}

void processNoteSensor(byte sensorIndex) {
  if (!ultrasonicSensorActive[sensorIndex]) {
    stopNoteForSensor(sensorIndex);
    return;
  }

  const int nextNote = calculateNoteForSensor(sensorIndex);
  const byte channel = sensorMidiChannel[sensorIndex];
  if (activeNote[sensorIndex] != nextNote || activeNoteChannel[sensorIndex] != channel) {
    stopNoteForSensor(sensorIndex);

    if (!otherSensorHoldsSameNote(sensorIndex, nextNote, channel)) {
      noteOn(channel, (byte)nextNote, globalVelocity);
      MidiUSB.flush();
    }

    activeNote[sensorIndex] = nextNote;
    activeNoteChannel[sensorIndex] = channel;
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
    Serial.print(F(" channel="));
    Serial.print(channel + 1);
    Serial.print(F(" velocity="));
    Serial.println(globalVelocity);
#endif
  }
}

void processPitchBendSensor(byte sensorIndex) {
  if (!ultrasonicSensorActive[sensorIndex]) {
    stopPitchBendForSensor(sensorIndex);
    return;
  }

  const int baseNote = calculatePitchBendBaseNote(sensorIndex);
  const byte channel = sensorMidiChannel[sensorIndex];

  if (activeNote[sensorIndex] != baseNote || activeNoteChannel[sensorIndex] != channel) {
    stopPitchBendForSensor(sensorIndex);

    // Configure the receiver for a +/-12-semitone bend, then establish the
    // bend before Note On to avoid a brief note at the old pitch.
    configurePitchBendRange(channel);
    const int bendValue = distanceToPitchBendValue(ultrasonicDistanceCm[sensorIndex]);
    pitchBend(channel, bendValue);
    MidiUSB.flush();

    noteOn(channel, (byte)baseNote, globalVelocity);
    MidiUSB.flush();

    activeNote[sensorIndex] = baseNote;
    activeNoteChannel[sensorIndex] = channel;
    lastPlayedNote[sensorIndex] = baseNote;
    lastNoteSensor = sensorIndex;
    noteStoppedAt[sensorIndex] = 0;
    lastNoteScreenUntil = 0;
    lastPitchBendValue[sensorIndex] = bendValue;
  } else {
    const int bendValue = distanceToPitchBendValue(ultrasonicDistanceCm[sensorIndex]);
    if (lastPitchBendValue[sensorIndex] < 0 ||
        abs(bendValue - lastPitchBendValue[sensorIndex]) >= PITCH_BEND_UPDATE_THRESHOLD) {
      pitchBend(channel, bendValue);
      MidiUSB.flush();
      lastPitchBendValue[sensorIndex] = bendValue;
    }
  }

#ifdef DEBUG
  Serial.print(F("Sensor "));
  Serial.print(sensorIndex + 1);
  Serial.print(F(" Pitch Bend="));
  Serial.print(lastPitchBendValue[sensorIndex]);
  Serial.print(F(" channel="));
  Serial.println(channel + 1);
#endif
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
  const byte channel = sensorMidiChannel[sensorIndex];

  if (lastSentCCValue[sensorIndex] != value ||
      lastSentCCNumber[sensorIndex] != sensorCCNumber[sensorIndex] ||
      lastSentCCChannel[sensorIndex] != channel) {
    controlChange(channel, sensorCCNumber[sensorIndex], value);
    MidiUSB.flush();

    lastSentCCValue[sensorIndex] = value;
    lastSentCCNumber[sensorIndex] = sensorCCNumber[sensorIndex];
    lastSentCCChannel[sensorIndex] = channel;

#ifdef DEBUG
    Serial.print(F("Sensor "));
    Serial.print(sensorIndex + 1);
    Serial.print(F(" CC"));
    Serial.print(sensorCCNumber[sensorIndex]);
    Serial.print(F(" = "));
    Serial.print(value);
    Serial.print(F(" channel="));
    Serial.println(channel + 1);
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
  // Intentionally retain the last valid global velocity when the hand leaves.
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

    case SENSOR_MODE_PITCH_BEND:
      processPitchBendSensor(sensorIndex);
      break;
  }
}

// Alternates sensors and keeps at least 60ms between trigger pulses to reduce
// acoustic cross-talk. Each sensor is therefore sampled approximately every 120ms.
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
