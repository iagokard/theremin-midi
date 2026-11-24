/*
  Theremidi HC-SR04 adaptation for Arduino Leonardo (ATmega32U4).

  Two ultrasonic sensors can be configured independently. Each sensor can
  operate in one of three modes: NOTES, VELOCITY or MIDI CC.

  Default assignment:
    Sensor 1: NOTES       TRIG A0 / ECHO A1
    Sensor 2: MIDI CC 11  TRIG A2 / ECHO A3

  Main button wiring (each button connects its pin to GND when pressed):
    D4  Sensor select (Sensor 1 / Sensor 2)
    D5  Cycle selected sensor mode (NOTES / VELOCITY / CC)
    D6  Selected sensor CC number -1
    D7  Selected sensor CC number +1
    D8  Octave -1
    D9  Octave +1
    D10 MIDI channel -1
    D11 MIDI channel +1
    D12 Root note -1 (C, C#, D ... B)
    D13 Root note +1
    A4  Cycle musical scale mode
    A5  Sustain (hold to sustain)

  Optional CC step buttons (disabled by default):
    D0  Selected sensor CC number -10
    D1  Selected sensor CC number +10

  D0/D1 are reserved for the optional buttons. Enable them by changing
  ENABLE_CC_STEP10_BUTTONS to 1. Do not use Serial1 while they are enabled.

  OLED SSD1306 I2C on Leonardo: SDA=D2, SCL=D3.

  Note: the original two-button Pitch Bend toggle is replaced by the requested
  per-sensor NOTES / VELOCITY / CC mode selector. Pitch Bend is not a separate
  user mode in this version.
*/

/////////////////////////////////////////////
// BOARD AND FEATURES
#define ATMEGA32U4 1
#define USING_DISPLAY 1
#define ENABLE_CC_STEP10_BUTTONS 0  // Set to 1 to enable optional D0/D1 buttons.
// #define DEBUG 1

/////////////////////////////////////////////
// MIDI AND DISPLAY LIBRARIES
#ifdef ATMEGA32U4
#include "MIDIUSB.h"
#endif

#ifdef USING_DISPLAY
#include <Wire.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(128, 64);
bool displayReady = false;
#endif

/////////////////////////////////////////////
// SENSOR CONFIGURATION
const byte N_ULTRASONIC_SENSORS = 2;
const byte SENSOR_1 = 0;
const byte SENSOR_2 = 1;

const byte ULTRASONIC_TRIG_PIN[N_ULTRASONIC_SENSORS] = {A0, A2};
const byte ULTRASONIC_ECHO_PIN[N_ULTRASONIC_SENSORS] = {A1, A3};

// Initial playing zone; calibrate these values after the first physical tests.
const float ULTRASONIC_MIN_DISTANCE_CM = 8.0f;
const float ULTRASONIC_MAX_DISTANCE_CM = 60.0f;
const unsigned long ULTRASONIC_ECHO_TIMEOUT_US = 6000UL;
const unsigned long ULTRASONIC_TRIGGER_INTERVAL_MS = 65UL;
const float ULTRASONIC_FILTER_ALPHA = 0.35f;
const byte ULTRASONIC_INVALID_READINGS_TO_RELEASE = 2;

float ultrasonicDistanceCm[N_ULTRASONIC_SENSORS] = {0.0f, 0.0f};
bool ultrasonicFilterInitialized[N_ULTRASONIC_SENSORS] = {false, false};
bool ultrasonicSensorActive[N_ULTRASONIC_SENSORS] = {false, false};
byte ultrasonicInvalidReadings[N_ULTRASONIC_SENSORS] = {0, 0};

/////////////////////////////////////////////
// BUTTON CONFIGURATION
// Every button uses INPUT_PULLUP and is wired between its pin and GND.
enum : byte {
  BTN_SENSOR_SELECT = 0,
  BTN_SENSOR_MODE,
  BTN_CC_MINUS,
  BTN_CC_PLUS,
  BTN_OCTAVE_MINUS,
  BTN_OCTAVE_PLUS,
  BTN_CHANNEL_MINUS,
  BTN_CHANNEL_PLUS,
  BTN_ROOT_MINUS,
  BTN_ROOT_PLUS,
  BTN_SCALE_MODE,
  BTN_SUSTAIN,
  BTN_CC_MINUS_10,
  BTN_CC_PLUS_10
};

const byte N_MAIN_BUTTONS = 12;
const byte MAIN_BUTTON_PINS[N_MAIN_BUTTONS] = {
  4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A4, A5
};

const byte OPTIONAL_CC_MINUS_10_PIN = 0; // D0 / RX1
const byte OPTIONAL_CC_PLUS_10_PIN = 1;  // D1 / TX1

struct ButtonDebounceState {
  byte rawState;
  byte stableState;
  unsigned long lastChangeAt;
};

ButtonDebounceState buttonStates[14] = {};
const unsigned long BUTTON_DEBOUNCE_MS = 25UL;

/////////////////////////////////////////////
// PER-SENSOR ACTION MODES
// Each sensor's mode is stored independently.
enum : byte {
  SENSOR_MODE_NOTES = 0,
  SENSOR_MODE_VELOCITY = 1,
  SENSOR_MODE_CC = 2
};

const char* sensorModeNames[3] = {"NOTES", "VELOCITY", "CC"};
byte sensorMode[N_ULTRASONIC_SENSORS] = {
  SENSOR_MODE_NOTES,
  SENSOR_MODE_CC
};
byte selectedSensor = SENSOR_1;

/////////////////////////////////////////////
// MUSICAL CONFIGURATION (shared by sensors in NOTES mode)
byte midiChannel = 0;       // MIDI channel is zero-based internally (0..15).
byte rootPitchClass = 0;    // C=0, C#=1 ... B=11.
byte scaleModeIndex = 0;    // Major by default.
byte octaveIndex = 2;       // Center position = no octave transposition.
const int8_t octaveOffsets[5] = {-24, -12, 0, 12, 24};

const byte SCALE_COUNT = 6;
const byte SCALE_NOTE_COUNT = 14;
const char* scaleNames[SCALE_COUNT] = {
  "Major", "Minor", "HarmMin", "MinPent", "MajPent", "Whole"
};

const char* noteNames[12] = {
  "C", "C#", "D", "D#", "E", "F",
  "F#", "G", "G#", "A", "A#", "B"
};

// Scale intervals over two octaves. These include the scales active in the
// original project plus Aeolian (natural minor) and major pentatonic, which
// were present as commented-out scale definitions in the original source.
const int8_t scaleIntervals[SCALE_COUNT][SCALE_NOTE_COUNT] = {
  // Major / Ionian
  {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23},
  // Natural minor / Aeolian
  {0, 2, 3, 5, 7, 8, 10, 12, 14, 15, 17, 19, 20, 22},
  // Harmonic minor
  {0, 2, 3, 5, 7, 8, 11, 12, 14, 15, 17, 19, 20, 23},
  // Minor pentatonic
  {0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24, 27, 29, 31},
  // Major pentatonic
  {0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24, 26, 28, 31},
  // Whole tone
  {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26}
};

/////////////////////////////////////////////
// VELOCITY AND MIDI CC CONFIGURATION
// Velocity is global and applies to every new Note On, regardless of sensor.
byte globalVelocity = 100;

// Each sensor stores its own CC controller number. Both start at CC11.
byte sensorCCNumber[N_ULTRASONIC_SENSORS] = {11, 11};
byte sensorCCValue[N_ULTRASONIC_SENSORS] = {0, 0};
int lastSentCCValue[N_ULTRASONIC_SENSORS] = {-1, -1};
int lastSentCCNumber[N_ULTRASONIC_SENSORS] = {-1, -1};
int lastSentCCChannel[N_ULTRASONIC_SENSORS] = {-1, -1};
const byte SUSTAIN_CC = 64;

/////////////////////////////////////////////
// ACTIVE MIDI NOTE STATE (one note per sensor)
int activeNote[N_ULTRASONIC_SENSORS] = {-1, -1};
byte activeNoteChannel[N_ULTRASONIC_SENSORS] = {0, 0};
int lastPlayedNote[N_ULTRASONIC_SENSORS] = {-1, -1};
unsigned long noteStoppedAt[N_ULTRASONIC_SENSORS] = {0, 0};

/////////////////////////////////////////////
// DISPLAY TIMING
const unsigned long DISPLAY_CONFIG_FLASH_MS = 800UL;
const unsigned long DISPLAY_NOTE_RELEASE_DELAY_MS = 250UL;
unsigned long configFlashUntil = 0;
unsigned long lastNoteScreenUntil = 0;
byte lastNoteSensor = SENSOR_1;
unsigned long lastDisplayRefreshAt = 0;
bool sustainIsPressed = false;
byte sustainChannel = 0;

/////////////////////////////////////////////
// GENERAL SCALING
float IR_curve = 1.0f; // Kept as the original project's curve parameter.
