# Theremidi — HC-SR04 / Arduino Leonardo

This folder contains the adapted Theremidi project for an Arduino Leonardo (ATmega32U4) with two HC-SR04 ultrasonic sensors, USB MIDI, and an optional SSD1306 I2C OLED.

**Read `README_HC-SR04_PT-BR.md` before wiring.** The original Fritzing schematic in `Schematic/` depicts the original Pro Micro and Sharp analog sensors; it is not the wiring diagram for this HC-SR04/Leonardo adaptation.

The adapted firmware gives each sensor an independent mode (NOTES, VELOCITY, CC, PITCH), MIDI channel, octave, and CC number. Root note and scale remain global. It includes a chromatic scale, continuous Pitch Bend, and independent two-sensor note output. See `README_HC-SR04_PT-BR.md` for pin assignments and button behavior.

For the ATmega32U4 flash-optimized build, use the bundled `Libs/Adafruit_SSD1306` copy: it omits the unused Adafruit startup splash. The firmware also uses a small PROGMEM lookup table instead of `pow()` and avoids `snprintf()` in OLED rendering. These changes retain all four sensor modes.

See `GUIA_CALIBRACAO_PT-BR.md` for the exact code locations and adjustment procedure for sensor cutoffs, scale span, filtering/hysteresis, OLED priority, Sustain toggle, and Pitch Bend range.
