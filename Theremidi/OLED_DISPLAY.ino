#ifdef USING_DISPLAY

// Draw a note name and octave centered without snprintf()/stdio, which are
// unnecessarily expensive in flash on the ATmega32U4.
void drawCenteredNote(int noteNumber, byte y) {
  const byte pitchClass = (byte)(noteNumber % 12);
  const char* noteName = noteNames[pitchClass];
  const bool isSharp = (pitchClass == 1 || pitchClass == 3 ||
                        pitchClass == 6 || pitchClass == 8 || pitchClass == 10);
  const byte characterCount = isSharp ? 3 : 2;
  const int textWidth = characterCount * 18;
  display.setTextSize(3);
  display.setCursor((128 - textWidth) / 2, y);
  display.print(noteName);
  display.print((noteNumber / 12) - 1);
}

void drawCenteredValue(byte value, byte y) {
  const byte characterCount = value >= 100 ? 3 : (value >= 10 ? 2 : 1);
  const int textWidth = characterCount * 18;
  display.setTextSize(3);
  display.setCursor((128 - textWidth) / 2, y);
  display.print(value);
}

void drawSustainIndicator() {
  display.setTextSize(1);
  display.setCursor(86, 0);
  display.print(F("SUS:"));
  display.print(sustainIsActive ? F("ON") : F("OFF"));
}

void drawSensorHeader(byte sensorIndex, const __FlashStringHelper* title) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("S"));
  display.print(sensorIndex + 1);
  display.print(F(" "));
  display.print(title);
  drawSustainIndicator();
}

void printNoteAndOctave(int noteNumber) {
  display.print(noteNames[noteNumber % 12]);
  display.print((noteNumber / 12) - 1);
}

void printSensorDistanceRange(byte sensorIndex) {
  display.print((int)(ultrasonicMinDistanceCm[sensorIndex] + 0.5f));
  display.print(F("-"));
  display.print((int)(ultrasonicMaxDistanceCm[sensorIndex] + 0.5f));
  display.print(F("cm"));
}

void requestConfigDisplay() {
  configFlashUntil = millis() + DISPLAY_CONFIG_FLASH_MS;
}

// If both sensors are playing, the configured/selected sensor takes priority.
// If only one is active, show that sensor even when the other is selected.
int getCurrentlyPlayingSensor() {
  const bool sensor1Playing = activeNote[SENSOR_1] >= 0;
  const bool sensor2Playing = activeNote[SENSOR_2] >= 0;

  if (sensor1Playing && sensor2Playing) return selectedSensor;
  if (sensor1Playing) return SENSOR_1;
  if (sensor2Playing) return SENSOR_2;
  return -1;
}

void drawConfigScreen(byte sensorIndex) {
  drawSensorHeader(sensorIndex, F("CONFIG"));

  display.setCursor(0, 10);
  display.print(F("Mode: "));
  display.print(sensorModeNames[sensorMode[sensorIndex]]);

  if (sensorMode[sensorIndex] == SENSOR_MODE_NOTES) {
    display.setCursor(0, 20);
    display.print(F("Scale: "));
    display.print(noteNames[rootPitchClass]);
    display.print(F(" "));
    display.print(scaleNames[scaleModeIndex]);

    display.setCursor(0, 30);
    display.print(F("Oct: "));
    display.print(3 + octaveOffsets[sensorOctaveIndex[sensorIndex]] / 12);
    display.print(F(" Ch: "));
    display.print(sensorMidiChannel[sensorIndex] + 1);

    display.setCursor(0, 40);
    display.print(F("Near: "));
    display.print((int)(ultrasonicMinDistanceCm[sensorIndex] + 0.5f));
    display.print(F(" cm"));

    display.setCursor(0, 48);
    display.print(F("Far:  "));
    display.print((int)(ultrasonicMaxDistanceCm[sensorIndex] + 0.5f));
    display.print(F(" cm"));

    display.setCursor(0, 56);
    display.print(F("Span: "));
    display.print(NOTE_RANGE_OCTAVES);
    display.print(F(" octave(s)"));
  } else if (sensorMode[sensorIndex] == SENSOR_MODE_PITCH_BEND) {
    display.setCursor(0, 20);
    display.print(F("Base: "));
    printNoteAndOctave(calculatePitchBendBaseNote(sensorIndex));

    display.setCursor(0, 30);
    display.print(F("Oct: "));
    display.print(3 + octaveOffsets[sensorOctaveIndex[sensorIndex]] / 12);
    display.print(F(" Ch: "));
    display.print(sensorMidiChannel[sensorIndex] + 1);

    display.setCursor(0, 40);
    display.print(F("Bend: +/-"));
    display.print(PITCH_BEND_RANGE_SEMITONES);
    display.print(F(" st"));

    display.setCursor(0, 48);
    display.print(F("Range: "));
    printSensorDistanceRange(sensorIndex);
  } else if (sensorMode[sensorIndex] == SENSOR_MODE_VELOCITY) {
    display.setCursor(0, 20);
    display.print(F("Velocity: "));
    display.print(globalVelocity);
    display.setCursor(0, 30);
    display.print(F("Global / next Note On"));
    display.setCursor(0, 42);
    display.print(F("Range: "));
    printSensorDistanceRange(sensorIndex);
  } else {
    display.setCursor(0, 20);
    display.print(F("CC: "));
    display.print(sensorCCNumber[sensorIndex]);
    display.print(F(" Value: "));
    display.print(sensorCCValue[sensorIndex]);
    display.setCursor(0, 30);
    display.print(F("Channel: "));
    display.print(sensorMidiChannel[sensorIndex] + 1);
    display.setCursor(0, 42);
    display.print(F("Range: "));
    printSensorDistanceRange(sensorIndex);
  }
}

void drawNoteScreen(byte sensorIndex, int noteNumber) {
  drawSensorHeader(sensorIndex, F("NOTE"));
  drawCenteredNote(noteNumber, 17);

  display.setTextSize(1);
  display.setCursor(0, 46);
  display.print(noteNames[rootPitchClass]);
  display.print(F(" "));
  display.print(scaleNames[scaleModeIndex]);
  display.setCursor(0, 56);
  display.print(F("Oct: "));
  display.print(3 + octaveOffsets[sensorOctaveIndex[sensorIndex]] / 12);
  display.print(F(" Ch: "));
  display.print(activeNoteChannel[sensorIndex] + 1);
}

void drawPitchBendScreen(byte sensorIndex, int estimatedNoteNumber) {
  drawSensorHeader(sensorIndex, F("PITCH"));
  drawCenteredNote(estimatedNoteNumber, 17);

  const int baseNote = activeNote[sensorIndex] >= 0
    ? activeNote[sensorIndex]
    : calculatePitchBendBaseNote(sensorIndex);
  const int semitoneOffset = estimatedNoteNumber - baseNote;

  display.setTextSize(1);
  display.setCursor(0, 46);
  display.print(F("Base: "));
  printNoteAndOctave(baseNote);
  display.print(F(" Ch:"));
  display.print(activeNoteChannel[sensorIndex] + 1);

  display.setCursor(0, 56);
  display.print(F("Bend: "));
  if (semitoneOffset > 0) display.print(F("+"));
  display.print(semitoneOffset);
  display.print(F(" st / +/-"));
  display.print(PITCH_BEND_RANGE_SEMITONES);
}

void drawVelocityScreen(byte sensorIndex) {
  drawSensorHeader(sensorIndex, F("VELOCITY"));
  drawCenteredValue(globalVelocity, 20);
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("Global / next Note On"));
}

void drawCCScreen(byte sensorIndex) {
  drawSensorHeader(sensorIndex, F("CC"));
  drawCenteredValue(sensorCCValue[sensorIndex], 18);

  display.setTextSize(1);
  display.setCursor(0, 53);
  display.print(F("CC: "));
  display.print(sensorCCNumber[sensorIndex]);
  display.print(F(" Ch: "));
  display.print(sensorMidiChannel[sensorIndex] + 1);
}

void updateOLEDDisplay() {
  if (!displayReady) return;

  const unsigned long now = millis();
  if ((unsigned long)(now - lastDisplayRefreshAt) < 80UL) return;
  lastDisplayRefreshAt = now;

  display.clearDisplay();

  // A configuration/calibration button temporarily overrides the live screen.
  if ((long)(configFlashUntil - now) > 0) {
    drawConfigScreen(selectedSensor);
  } else {
    const int playingSensor = getCurrentlyPlayingSensor();

    if (playingSensor >= 0) {
      if (sensorMode[playingSensor] == SENSOR_MODE_PITCH_BEND) {
        drawPitchBendScreen(
          (byte)playingSensor,
          calculatePitchBendDisplayNote((byte)playingSensor)
        );
      } else {
        drawNoteScreen((byte)playingSensor, activeNote[playingSensor]);
      }
    } else if ((long)(lastNoteScreenUntil - now) > 0 && lastPlayedNote[lastNoteSensor] >= 0) {
      drawNoteScreen(lastNoteSensor, lastPlayedNote[lastNoteSensor]);
    } else if (sensorMode[selectedSensor] == SENSOR_MODE_VELOCITY) {
      drawVelocityScreen(selectedSensor);
    } else if (sensorMode[selectedSensor] == SENSOR_MODE_CC) {
      drawCCScreen(selectedSensor);
    } else {
      drawConfigScreen(selectedSensor);
    }
  }

  display.display();
}

#else

void requestConfigDisplay() {}
void updateOLEDDisplay() {}

#endif
