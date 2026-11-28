/////////////////////////////////////////////
// if using with ATmega32U4 (micro, pro micro, leonardo...)
#ifdef ATMEGA32U4

// Arduino (pro)micro MIDI functions using the MIDIUSB library.
void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t event = {0x09, (byte)(0x90 | channel), pitch, velocity};
  MidiUSB.sendMIDI(event);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t event = {0x08, (byte)(0x80 | channel), pitch, velocity};
  MidiUSB.sendMIDI(event);
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, (byte)(0xB0 | channel), control, value};
  MidiUSB.sendMIDI(event);
}

void pitchBend(byte channel, int value) {
  value = constrain(value, 0, 16383);
  const byte lowValue = (byte)(value & 0x7F);
  const byte highValue = (byte)((value >> 7) & 0x7F);
  midiEventPacket_t event = {0x0E, (byte)(0xE0 | channel), lowValue, highValue};
  MidiUSB.sendMIDI(event);
}

// RPN 0,0 = Pitch Bend Sensitivity. The configured semitone range is applied
// in both directions from the base note. Instruments that ignore RPN may need
// the same range configured manually in the DAW/plugin.
void configurePitchBendRange(byte channel) {
  controlChange(channel, 101, 0); // RPN MSB
  controlChange(channel, 100, 0); // RPN LSB
  controlChange(channel, 6, PITCH_BEND_RANGE_SEMITONES); // Data Entry MSB
  controlChange(channel, 38, 0); // Data Entry LSB (0 cents)
  controlChange(channel, 101, 127); // Deselect RPN
  controlChange(channel, 100, 127);
  MidiUSB.flush();
}

#endif
