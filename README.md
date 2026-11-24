# Theremidi — HC-SR04 / Arduino Leonardo

This repository contains the adapted Theremidi project for an Arduino Leonardo (ATmega32U4), two HC-SR04 ultrasonic sensors, USB MIDI, and an SSD1306 I2C OLED.

**Read `README_HC-SR04_PT-BR.md` before wiring or configuring the instrument.** The original Fritzing schematic in `Schematic/` depicts the original Pro Micro and Sharp analog sensors; it is not the wiring diagram for this HC-SR04/Leonardo adaptation.

The current firmware provides four independent sensor modes (NOTES, VELOCITY, CC, PITCH), independent MIDI channel and octave settings for each sensor, global root-note and scale settings, a chromatic scale, and continuous Pitch Bend mode. The Portuguese README documents the controls, MIDI behavior, calibration, and hardware pinout. See `CHANGELOG-v3.txt` for this revision's changes.
