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
    newDistanceCm >= ultrasonicMinDistanceCm[sensorIndex] &&
    newDistanceCm <= ultrasonicMaxDistanceCm[sensorIndex];

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

// Approximation of the original fscale curve (pow(10, -0.1) exponent)
// using a 65-point lookup table. This avoids pulling the large AVR libm pow()
// implementation into the Leonardo firmware while retaining a close response.
#include <avr/pgmspace.h>

const uint16_t DISTANCE_CURVE_TABLE[65] PROGMEM = {
  0, 38, 65, 90, 113, 135, 156, 177,
  196, 216, 234, 253, 271, 289, 306, 323,
  340, 357, 374, 390, 406, 423, 438, 454,
  470, 485, 501, 516, 531, 546, 561, 576,
  590, 605, 620, 634, 648, 663, 677, 691,
  705, 719, 733, 747, 760, 774, 788, 801,
  815, 828, 842, 855, 868, 882, 895, 908,
  921, 934, 947, 960, 973, 986, 998, 1011,
  1024
};

// Returns the curved distance as a fixed-point value in the range 0..1024.
// 0 means the hand is nearest to the sensor; 1024 means farthest away.
uint16_t distanceToCurve1024(byte sensorIndex, float distanceCm) {
  const float minimumDistance = ultrasonicMinDistanceCm[sensorIndex];
  const float maximumDistance = ultrasonicMaxDistanceCm[sensorIndex];
  const float distanceSpan = maximumDistance - minimumDistance;

  if (distanceCm <= minimumDistance) return 0;
  if (distanceCm >= maximumDistance) return 1024;
  if (distanceSpan <= 0.0f) return 0;

  const uint16_t scaled = (uint16_t)(
    (distanceCm - minimumDistance) * (16384.0f / distanceSpan)
  );
  const byte index = (byte)(scaled >> 8);
  const byte fraction = (byte)(scaled & 0xFF);
  const uint16_t lower = pgm_read_word(&DISTANCE_CURVE_TABLE[index]);
  const uint16_t upper = pgm_read_word(&DISTANCE_CURVE_TABLE[index + 1]);
  return lower + (uint16_t)(((uint32_t)(upper - lower) * fraction + 128) >> 8);
}

// Schmitt-style note selection prevents repeated Note On/Off events when a
// hand hovers around the boundary between adjacent scale positions.
byte getActiveScaleNoteCount(byte scaleIndex) {
  const int maximumSemitone = (int)NOTE_RANGE_OCTAVES * 12;
  byte count = 0;
  while (count < scaleMaxNoteCounts[scaleIndex] &&
         scaleIntervals[scaleIndex][count] <= maximumSemitone) {
    count++;
  }
  return count;
}

int distanceToScalePosition(byte sensorIndex) {
  const byte noteCount = getActiveScaleNoteCount(scaleModeIndex);
  const int lastPosition = (int)noteCount - 1;
  const uint16_t curve = distanceToCurve1024(sensorIndex, ultrasonicDistanceCm[sensorIndex]);

  // Q8 fixed point: near = last position, far = position zero.
  const uint32_t positionQ8 =
    ((uint32_t)(1024 - curve) * lastPosition * 256UL + 512UL) / 1024UL;
  int stablePosition = currentScalePosition[sensorIndex];

  // 0.65 of a scale step (0.5 rounding boundary + 0.15 hysteresis).
  const int hysteresisQ8 = NOTE_HYSTERESIS_Q8;
  if (stablePosition < 0 || stablePosition > lastPosition) {
    stablePosition = (int)((positionQ8 + 128UL) >> 8);
  } else if (positionQ8 > (uint32_t)(stablePosition * 256 + hysteresisQ8) ||
             (int32_t)positionQ8 < (int32_t)(stablePosition * 256 - hysteresisQ8)) {
    stablePosition = (int)((positionQ8 + 128UL) >> 8);
  }

  if (stablePosition < 0) stablePosition = 0;
  if (stablePosition > lastPosition) stablePosition = lastPosition;
  currentScalePosition[sensorIndex] = (int8_t)stablePosition;
  return stablePosition;
}

byte distanceToMidiValue(byte sensorIndex, float distanceCm, byte minimumValue) {
  const uint16_t curve = distanceToCurve1024(sensorIndex, distanceCm);
  const uint16_t span = (uint16_t)(127 - minimumValue);
  return (byte)(minimumValue +
    ((uint32_t)(1024 - curve) * span + 512UL) / 1024UL);
}

int distanceToPitchBendValue(byte sensorIndex, float distanceCm) {
  const uint16_t curve = distanceToCurve1024(sensorIndex, distanceCm);
  return (int)(((uint32_t)(1024 - curve) * 16383UL + 512UL) / 1024UL);
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

// Estimate the nearest equal-tempered MIDI note represented by the last bend.
// MIDI itself sends Note On + Pitch Bend, not a measured acoustic frequency.
int calculatePitchBendDisplayNote(byte sensorIndex) {
  const int baseNote = activeNote[sensorIndex] >= 0
    ? activeNote[sensorIndex]
    : calculatePitchBendBaseNote(sensorIndex);
  const int bendValue = lastPitchBendValue[sensorIndex] < 0
    ? PITCH_BEND_CENTER
    : lastPitchBendValue[sensorIndex];
  const int bendDelta = bendValue - PITCH_BEND_CENTER;

  int semitoneOffset = 0;
  if (bendDelta >= 0) {
    const long numerator = (long)bendDelta * PITCH_BEND_RANGE_SEMITONES;
    semitoneOffset = (int)((numerator + 4095L) / 8191L);
  } else {
    const long numerator = (long)(-bendDelta) * PITCH_BEND_RANGE_SEMITONES;
    semitoneOffset = -(int)((numerator + 4096L) / 8192L);
  }

  return constrain(baseNote + semitoneOffset, 0, 127);
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

    // Configure the receiver for the selected +/-semitone bend range, then
    // establish the bend before Note On to avoid a brief note at the old pitch.
    configurePitchBendRange(channel);
    const int bendValue = distanceToPitchBendValue(sensorIndex, ultrasonicDistanceCm[sensorIndex]);
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
    const int bendValue = distanceToPitchBendValue(sensorIndex, ultrasonicDistanceCm[sensorIndex]);
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
    value = distanceToMidiValue(sensorIndex, ultrasonicDistanceCm[sensorIndex], 0);
  }
  sendSensorCC(sensorIndex, value);
}

void processVelocitySensor(byte sensorIndex) {
  // Intentionally retain the last valid global velocity when the hand leaves.
  if (!ultrasonicSensorActive[sensorIndex]) return;

  globalVelocity = distanceToMidiValue(sensorIndex, ultrasonicDistanceCm[sensorIndex], 1);

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
