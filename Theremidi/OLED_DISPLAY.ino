#ifdef USING_DISPLAY

#include <stdio.h>

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
    display.print(3 + octaveOffsets[octaveIndex] / 12);
    display.print(F("   Ch: "));
    display.print(midiChannel + 1);
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
    display.print(F("  Ch: "));
    display.print(midiChannel + 1);
  }
}

void drawNoteScreen(byte sensorIndex, int noteNumber) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SENSOR "));
  display.print(sensorIndex + 1);
  display.print(F("  MIDI NOTE"));

  char noteText[8];
  snprintf(noteText, sizeof(noteText), "%s%d", noteNames[noteNumber % 12], (noteNumber / 12) - 1);
  display.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(noteText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 17);
  display.print(noteText);

  display.setTextSize(1);
  display.setCursor(0, 46);
  display.print(noteNames[rootPitchClass]);
  display.print(F(" "));
  display.print(scaleNames[scaleModeIndex]);
  display.setCursor(0, 56);
  display.print(F("Oct: "));
  display.print((noteNumber / 12) - 1);
  display.print(F("   Ch: "));
  display.print(midiChannel + 1);
}

void drawVelocityScreen(byte sensorIndex) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SENSOR "));
  display.print(sensorIndex + 1);
  display.print(F("  VELOCITY"));

  char valueText[5];
  snprintf(valueText, sizeof(valueText), "%u", globalVelocity);
  display.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(valueText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 20);
  display.print(valueText);

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print(F("Used by the next Note On"));
}

void drawCCScreen(byte sensorIndex) {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SENSOR "));
  display.print(sensorIndex + 1);
  display.print(F("  MIDI CC"));

  char valueText[5];
  snprintf(valueText, sizeof(valueText), "%u", sensorCCValue[sensorIndex]);
  display.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(valueText, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 18);
  display.print(valueText);

  display.setTextSize(1);
  display.setCursor(0, 53);
  display.print(F("CC: "));
  display.print(sensorCCNumber[sensorIndex]);
  display.print(F("    Ch: "));
  display.print(midiChannel + 1);
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
      drawNoteScreen((byte)playingSensor, activeNote[playingSensor]);
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
