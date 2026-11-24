void setup() {
#ifdef DEBUG
  Serial.begin(115200); // Leonardo Serial uses native USB CDC; D0/D1 remain available for digital I/O unless Serial1 is used.
  delay(500);
  Serial.println(F("Theremidi HC-SR04 - setup"));
#endif

  initializeButtons();

  for (byte i = 0; i < N_ULTRASONIC_SENSORS; i++) {
    pinMode(ULTRASONIC_TRIG_PIN[i], OUTPUT);
    pinMode(ULTRASONIC_ECHO_PIN[i], INPUT);
    digitalWrite(ULTRASONIC_TRIG_PIN[i], LOW);
  }

#ifdef USING_DISPLAY
  delay(100);
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (displayReady) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setRotation(0);
    display.setTextWrap(false);
    display.setTextSize(1);
    display.display();
  }
#endif

  requestConfigDisplay();
  updateOLEDDisplay();
}
