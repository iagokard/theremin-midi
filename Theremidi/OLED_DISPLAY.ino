#ifdef USING_DISPLAY

// Draw a note name and octave centered without snprintf()/stdio, which are
// unnecessarily expensive in flash on the ATmega32U4.
void drawCenteredNote(int noteNumber, byte y) {
  const byte pitchClass = (byte)(noteNumber % 12);
  const char* noteName = noteNames[pitchClass];
  const bool isSharp = (pitchClass == 1 || pitchClass == 3 ||
                        pitchClass == 6 || pitchClass == 8 || pitchClass == 10);
  const byte characterCount = isSharp ? 3 : 2; // note name + octave digit
  const int textWidth = characterCount * 18;     // 6px font * text size 3
  display.setTextSize(3);
  display.setCursor((128 - textWidth) / 2, y);
  display.print(noteName);
  display.print((noteNumber / 12) - 1);
}

void drawCenteredValue(byte value, byte y) {
  const byte characterCount = value >= 100 ? 3 : (value >= 10 ? 2 : 1);
  const int textWidth = characterCount * 18; // 6px font * text size 3
  display.setTextSize(3);
  display.setCursor((128 - textWidth) / 2, y);
  display.print(value);
}

void requestConfigDisplay() {
  configFlashUntil = millis() + DISPLAY_CONFIG_FLASH_MS;
}

int getCurrentlyPlayingSensor() {
  if (activeNote[lastNoteSensor] >= 0) return lastNoteSensor;
  for (byte i = 0; i < N_ULTRASONIC_SENSORS; i++) {
    if (activeNote[i] >= 0) return i;
  }
  return -1;
}

void drawConfigScreen(byte sensorIndex) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SENSOR "));
  display.print(sensorIndex + 1);
  display.print(F(" CONFIG"));

  display.setCursor(0, 16);
  display.print(F("Mode: "));
  display.println(sensorModeNames[sensorMode[sensorIndex]]);

  if (sensorMode[sensorIndex] == SENSOR_MODE_NOTES) {
    display.setCursor(0, 30);
    display.print(F("Scale: "));
    display.print(noteNames[rootPitchClass]);
    display.print(F(" "));
    display.println(scaleNames[scaleModeIndex]);

    display.setCursor(0, 46);
    display.print(F("Oct: "));
    display.print(3 + octaveOffsets[sensorOctaveIndex[sensorIndex]] / 12);
    display.print(F("   Ch: "));
    display.print(sensorMidiChannel[sensorIndex] + 1);
  } else if (sensorMode[sensorIndex] == SENSOR_MODE_PITCH_BEND) {
    display.setCursor(0, 30);
    display.print(F("Base: "));
    display.print(noteNames[rootPitchClass]);

    display.setCursor(0, 46);
    display.print(F("Oct: "));
    display.print(3 + octaveOffsets[sensorOctaveIndex[sensorIndex]] / 12);
    display.print(F("   Ch: "));
    display.print(sensorMidiChannel[sensorIndex] + 1);

    display.setCursor(0, 56);
    display.print(F("Bend range: +/-12 st"));
  } else if (sensorMode[sensorIndex] == SENSOR_MODE_VELOCITY) {
    display.setCursor(0, 31);
    display.print(F("Global velocity for"));
    display.setCursor(0, 43);
    display.print(F("new Note On events"));
    display.setCursor(0, 55);
    display.print(F("Value: "));
    display.print(globalVelocity);
  } else {
    display.setCursor(0, 31);
    display.print(F("CC number: "));
    display.print(sensorCCNumber[sensorIndex]);
    display.setCursor(0, 45);
    display.print(F("Value: "));
    display.print(sensorCCValue[sensorIndex]);
    display.print(F(" Ch: "));
    display.print(sensorMidiChannel[sensorIndex] + 1);
  }
}

void drawNoteScreen(byte sensorIndex, int noteNumber) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SENSOR "));
  display.print(sensorIndex + 1);
  display.print(F("  MIDI NOTE"));

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

void drawPitchBendScreen(byte sensorIndex, int noteNumber) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SENSOR "));
  display.print(sensorIndex + 1);
  display.print(F("  PITCH BEND"));

  drawCenteredNote(noteNumber, 17);

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("PB: "));
  display.print(lastPitchBendValue[sensorIndex] < 0
    ? PITCH_BEND_CENTER
    : lastPitchBendValue[sensorIndex]);
  display.print(F(" Ch: "));
  display.print(activeNoteChannel[sensorIndex] + 1);
}

void drawVelocityScreen(byte sensorIndex) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SENSOR "));
  display.print(sensorIndex + 1);
  display.print(F("  VELOCITY"));

  drawCenteredValue(globalVelocity, 20);

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("Next Note On only"));
}

void drawCCScreen(byte sensorIndex) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SENSOR "));
  display.print(sensorIndex + 1);
  display.print(F("  MIDI CC"));

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

  // A configuration button temporarily overrides the live note screen.
  if ((long)(configFlashUntil - now) > 0) {
    drawConfigScreen(selectedSensor);
  } else {
    const int playingSensor = getCurrentlyPlayingSensor();

    if (playingSensor >= 0) {
      if (sensorMode[playingSensor] == SENSOR_MODE_PITCH_BEND) {
        drawPitchBendScreen((byte)playingSensor, activeNote[playingSensor]);
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
