# Theremidi — HC-SR04 / Arduino Leonardo

This folder contains the adapted Theremidi project for an Arduino Leonardo (ATmega32U4) with two HC-SR04 ultrasonic sensors, USB MIDI, and an optional SSD1306 I2C OLED.

**Read `README_HC-SR04_PT-BR.md` before wiring.** The original Fritzing schematic in `Schematic/` depicts the original Pro Micro and Sharp analog sensors; it is not the wiring diagram for this HC-SR04/Leonardo adaptation.

The adapted firmware gives each sensor an independent mode (NOTES, VELOCITY, CC), retains the last valid global velocity value, and supports individual CC number settings. See the Portuguese README for pin assignments and button behavior.
