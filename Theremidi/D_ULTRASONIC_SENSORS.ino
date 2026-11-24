/////////////////////////////////////////////
// HC-SR04 ultrasonic distance acquisition and MIDI processing.
//
// Wiring in this version:
//   Sensor 1 (notes / Pitch Bend): TRIG=A0, ECHO=A1
//   Sensor 2 (MIDI CC):            TRIG=A2, ECHO=A3
//
// The sensors are triggered alternately. This avoids firing both modules
// together and leaves >60ms between consecutive trigger pulses.

namespace {
  int activeNote = -1;
  byte activeNoteChannel = 0;

  int activePitchBendNote = -1;
  byte activePitchBendChannel = 0;
  int lastSentPitchBend = -1;

  int lastSentCC = -1;
  byte lastSentCCChannel = 0;

  const byte CC_SEND_DELTA = 1;
}

// Send a short trigger pulse and measure the ECHO high-pulse duration.
// Returns 0 when no echo is received before the timeout.
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

  if (echoDuration == 0) {
    return 0.0f;
  }

  // HC-SR04 approximation: distance in cm = echo duration in microseconds / 58.
  return (float)echoDuration / 58.0f;
}

// Update the filtered distance and determine whether a hand is inside
// the configured playing window.
void updateUltrasonicSensorState(byte sensorIndex, float newDistanceCm) {
  const bool inPlayingRange =
    (newDistanceCm >= ULTRASONIC_MIN_DISTANCE_CM) &&
    (newDistanceCm <= ULTRASONIC_MAX_DISTANCE_CM);

  if (!inPlayingRange) {
    if (ultrasonicInvalidReadings[sensorIndex] < 255) {
      ultrasonicInvalidReadings[sensorIndex]++;
    }

    if (ultrasonicInvalidReadings[sensorIndex] >= ULTRASONIC_INVALID_READINGS_TO_RELEASE) {
      ultrasonicSensorActive[sensorIndex] = false;
      ultrasonicFilterInitialized[sensorIndex] = false;
    }

#ifdef DEBUG
    Serial.print("Sensor ");
    Serial.print(sensorIndex + 1);
    Serial.print(" - sem leitura valida / fora da faixa: ");
    Serial.println(newDistanceCm);
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
  Serial.print("Sensor ");
  Serial.print(sensorIndex + 1);
  Serial.print(" - distancia: ");
  Serial.print(newDistanceCm, 1);
  Serial.print(" cm | filtrada: ");
  Serial.println(ultrasonicDistanceCm[sensorIndex], 1);
#endif
}

void stopCurrentNote() {
  if (activeNote >= 0) {
    noteOff(activeNoteChannel, (byte)activeNote, 0);
    MidiUSB.flush();
    activeNote = -1;
  }

  note_is_playing = false;
}

void stopPitchBendNote() {
  if (activePitchBendNote >= 0) {
    // Return the pitch wheel to its neutral centre before releasing the note.
    pitchBend(activePitchBendChannel, 8192);
    noteOff(activePitchBendChannel, (byte)activePitchBendNote, 0);
    MidiUSB.flush();
    activePitchBendNote = -1;
  }

  pitchBendIsPlaying = false;
  lastSentPitchBend = -1;
  pitchBendVal = 8192;
  pitchBendPVal = -1;
}

void processNoteSensor() {
  if (!ultrasonicSensorActive[SENSOR_NOTE]) {
    stopCurrentNote();
    stopPitchBendNote();
    return;
  }

  const float distanceCm = ultrasonicDistanceCm[SENSOR_NOTE];

  if (pitchBendIsOn) {
    stopCurrentNote();

    const int bendNote = NOTE + octave[octaveIndex];

    if (activePitchBendNote != bendNote || activePitchBendChannel != MIDI_CH) {
      stopPitchBendNote();

      noteOn(MIDI_CH, (byte)bendNote, 127);
      MidiUSB.flush();

      activePitchBendNote = bendNote;
      activePitchBendChannel = MIDI_CH;
      pitchBendIsPlaying = true;
      lastSentPitchBend = -1;
    }

    // Nearer hand = higher pitch-bend value, matching the original sensor direction.
    pitchBendVal = (int)round(fscale(
      ULTRASONIC_MIN_DISTANCE_CM,
      ULTRASONIC_MAX_DISTANCE_CM,
      16383,
      0,
      distanceCm,
      IR_curve
    ));
    pitchBendVal = constrain(pitchBendVal, 0, 16383);

    if (lastSentPitchBend < 0 || abs(pitchBendVal - lastSentPitchBend) >= 8) {
      pitchBend(MIDI_CH, pitchBendVal);
      MidiUSB.flush();
      lastSentPitchBend = pitchBendVal;
      pitchBendPVal = pitchBendVal;
    }

    return;
  }

  // Switching back from Pitch Bend mode releases its sustained reference note.
  stopPitchBendNote();

  // Original Theremidi uses 14 scale positions (indexes 0..13) for this sensor.
  // Nearer hand = larger scale index / higher note.
  int scalePosition = (int)round(fscale(
    ULTRASONIC_MIN_DISTANCE_CM,
    ULTRASONIC_MAX_DISTANCE_CM,
    13,
    0,
    distanceCm,
    IR_curve
  ));
  scalePosition = constrain(scalePosition, 0, 13);

  int nextNote = NOTE + scaleNotes[scaleIndex][scalePosition] + octave[octaveIndex];
  nextNote = constrain(nextNote, 0, 127);

  if (activeNote != nextNote || activeNoteChannel != MIDI_CH) {
    stopCurrentNote();

    noteOn(MIDI_CH, (byte)nextNote, 127); // Velocity remains fixed at 127, as requested.
    MidiUSB.flush();

    activeNote = nextNote;
    activeNoteChannel = MIDI_CH;
    note_is_playing = true;
  }

#ifdef DEBUG
  Serial.print("Sensor 1 - indice da escala: ");
  Serial.print(scalePosition);
  Serial.print(" | nota MIDI: ");
  Serial.println(nextNote);
#endif
}

void processCCSensor() {
  int nextCCValue = 0;

  if (ultrasonicSensorActive[SENSOR_CC]) {
    // Nearer hand = higher CC value. By default CC7 is MIDI Volume.
    nextCCValue = (int)round(fscale(
      ULTRASONIC_MIN_DISTANCE_CM,
      ULTRASONIC_MAX_DISTANCE_CM,
      127,
      0,
      ultrasonicDistanceCm[SENSOR_CC],
      IR_curve
    ));
    nextCCValue = constrain(nextCCValue, 0, 127);
  }

  if (lastSentCC < 0 || abs(nextCCValue - lastSentCC) >= CC_SEND_DELTA || lastSentCCChannel != MIDI_CH) {
    controlChange(MIDI_CH, CC, (byte)nextCCValue);
    MidiUSB.flush();

    lastSentCC = nextCCValue;
    lastSentCCChannel = MIDI_CH;

#ifdef DEBUG
    Serial.print("Sensor 2 - CC");
    Serial.print(CC);
    Serial.print(": ");
    Serial.println(nextCCValue);
#endif
  }
}

// Call continuously from loop(). One sensor is measured every 65ms; because
// the two sensors alternate, each individual module is measured every ~130ms.
void serviceUltrasonicSensors() {
  static byte nextSensor = SENSOR_NOTE;
  static bool firstMeasurement = true;
  static unsigned long lastMeasurementAt = 0;

  const unsigned long now = millis();
  if (!firstMeasurement && (unsigned long)(now - lastMeasurementAt) < ULTRASONIC_TRIGGER_INTERVAL_MS) {
    return;
  }

  firstMeasurement = false;

  const float distanceCm = readUltrasonicDistanceCm(nextSensor);
  lastMeasurementAt = millis();

  updateUltrasonicSensorState(nextSensor, distanceCm);

  if (nextSensor == SENSOR_NOTE) {
    processNoteSensor();
    nextSensor = SENSOR_CC;
  } else {
    processCCSensor();
    nextSensor = SENSOR_NOTE;
  }
}
