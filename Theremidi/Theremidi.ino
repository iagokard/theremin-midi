/*
  Theremidi HC-SR04 adaptation for Arduino Leonardo (ATmega32U4).

  Two ultrasonic sensors can be configured independently. Each sensor can
  operate in one of four modes: NOTES, VELOCITY, MIDI CC or PITCH BEND.

  Default assignment:
    Sensor 1: NOTES       TRIG A0 / ECHO A1
    Sensor 2: MIDI CC 11  TRIG A2 / ECHO A3

  Main button wiring (each button connects its pin to GND when pressed):
    D4  Sensor select (Sensor 1 / Sensor 2)
    D5  Cycle selected sensor mode (NOTES / VELOCITY / CC / PITCH)
    D6  Selected sensor CC number -1
    D7  Selected sensor CC number +1
    D8  Selected sensor octave -1
    D9  Selected sensor octave +1
    D10 Selected sensor MIDI channel -1
    D11 Selected sensor MIDI channel +1
    D12 Global root note -1 (C, C#, D ... B)
    D13 Global root note +1
    A4  Cycle global musical scale mode
    A5  Toggle global Sustain ON/OFF (both configured sensor channels)

  Distance calibration buttons (enabled by default):
    D0  Selected sensor maximum distance -1 cm
    D1  Selected sensor maximum distance +1 cm

  D0/D1 are used as digital inputs; do not use Serial1 while enabled.

  OLED SSD1306 I2C on Leonardo: SDA=D2, SCL=D3.
*/

/////////////////////////////////////////////
// BOARD AND FEATURES
#define ATMEGA32U4 1
#define USING_DISPLAY 1
#define ENABLE_DISTANCE_CALIBRATION_BUTTONS 1  // D0/D1 adjust selected sensor far cutoff.
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

// Per-sensor playing zones. Edit the arrays to set each sensor's near/far cutoffs.
// D0/D1 adjust only the selected sensor's maximum (far) cutoff at runtime.
const float ULTRASONIC_DEFAULT_MIN_DISTANCE_CM = 8.0f;
const float ULTRASONIC_DEFAULT_MAX_DISTANCE_CM = 60.0f;
float ultrasonicMinDistanceCm[N_ULTRASONIC_SENSORS] = {
  ULTRASONIC_DEFAULT_MIN_DISTANCE_CM, ULTRASONIC_DEFAULT_MIN_DISTANCE_CM
};
float ultrasonicMaxDistanceCm[N_ULTRASONIC_SENSORS] = {
  ULTRASONIC_DEFAULT_MAX_DISTANCE_CM, ULTRASONIC_DEFAULT_MAX_DISTANCE_CM
};
const float DISTANCE_CALIBRATION_STEP_CM = 1.0f;
const float ULTRASONIC_MIN_PLAYING_SPAN_CM = 5.0f;
const float ULTRASONIC_HARD_MAX_DISTANCE_CM = 100.0f; // Keep below the 6000us echo timeout.
const unsigned long ULTRASONIC_ECHO_TIMEOUT_US = 6000UL;
// Keep at least ~60ms between HC-SR04 pings to reduce acoustic cross-talk.
const unsigned long ULTRASONIC_TRIGGER_INTERVAL_MS = 60UL;

// Adaptive EMA: smoother for small fluctuations, faster response to hand motion.
const float ULTRASONIC_FILTER_ALPHA = 0.35f;
const float ULTRASONIC_FILTER_ALPHA_FAST = 0.70f;
const float ULTRASONIC_FILTER_FAST_THRESHOLD_CM = 3.0f;
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
  BTN_DISTANCE_MAX_MINUS,
  BTN_DISTANCE_MAX_PLUS
};

const byte N_MAIN_BUTTONS = 12;
const byte MAIN_BUTTON_PINS[N_MAIN_BUTTONS] = {
  4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A4, A5
};

const byte DISTANCE_MAX_MINUS_PIN = 0; // D0 / RX1
const byte DISTANCE_MAX_PLUS_PIN = 1;  // D1 / TX1

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
  SENSOR_MODE_CC = 2,
  SENSOR_MODE_PITCH_BEND = 3
};

const byte SENSOR_MODE_COUNT = 4;
const char* sensorModeNames[SENSOR_MODE_COUNT] = {
  "NOTES", "VELOCITY", "CC", "PITCH"
};
byte sensorMode[N_ULTRASONIC_SENSORS] = {
  SENSOR_MODE_NOTES,
  SENSOR_MODE_CC
};
byte selectedSensor = SENSOR_1;

/////////////////////////////////////////////
// GLOBAL MUSICAL CONFIGURATION
// Root note and scale remain global; octave and MIDI channel are per sensor.
byte rootPitchClass = 0;    // C=0, C#=1 ... B=11.
byte scaleModeIndex = 0;    // Major by default.
byte sensorOctaveIndex[N_ULTRASONIC_SENSORS] = {2, 2};
byte sensorMidiChannel[N_ULTRASONIC_SENSORS] = {0, 0}; // MIDI channels 1 and 1.
const int8_t octaveOffsets[5] = {-24, -12, 0, 12, 24};

const char* noteNames[12] = {
  "C", "C#", "D", "D#", "E", "F",
  "F#", "G", "G#", "A", "A#", "B"
};

// Each scale table contains up to two octaves (0..24 semitones).
// Set to 1 or 2 to distribute the selected sensor's distance over that span.
const byte NOTE_RANGE_OCTAVES = 2;
const byte SCALE_COUNT = 7;
const byte SCALE_MAX_NOTE_COUNT = 25;
const byte scaleMaxNoteCounts[SCALE_COUNT] = {
  15, // Major: 7 notes per octave, plus the closing root.
  15, // Natural minor / Aeolian.
  15, // Harmonic minor.
  11, // Minor pentatonic.
  11, // Major pentatonic.
  13, // Whole tone.
  25  // Chromatic: every semitone.
};
const char* scaleNames[SCALE_COUNT] = {
  "Major", "Minor", "HarmMin", "MinPent", "MajPent", "Whole", "Chromatic"
};

const int8_t scaleIntervals[SCALE_COUNT][SCALE_MAX_NOTE_COUNT] = {
  // Major / Ionian: 2 octaves.
  {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24},
  // Natural minor / Aeolian: 2 octaves.
  {0, 2, 3, 5, 7, 8, 10, 12, 14, 15, 17, 19, 20, 22, 24},
  // Harmonic minor: 2 octaves.
  {0, 2, 3, 5, 7, 8, 11, 12, 14, 15, 17, 19, 20, 23, 24},
  // Minor pentatonic: 2 octaves.
  {0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24},
  // Major pentatonic: 2 octaves.
  {0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24},
  // Whole tone: 2 octaves.
  {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24},
  // Chromatic: every semitone across 2 octaves.
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
   13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}
};

// Q8 threshold equivalent to the original 0.5 + 0.15 scale-step boundary.
const byte NOTE_HYSTERESIS_Q8 = 166;
int8_t currentScalePosition[N_ULTRASONIC_SENSORS] = {-1, -1};

/////////////////////////////////////////////
// VELOCITY AND MIDI CC CONFIGURATION
// Velocity is global and applies to every new Note On, regardless of sensor.
byte globalVelocity = 100;

// Each sensor stores its own CC controller number. Both start at CC11.
// All 0..127 values remain available intentionally, including reserved CCs.
byte sensorCCNumber[N_ULTRASONIC_SENSORS] = {11, 11};
byte sensorCCValue[N_ULTRASONIC_SENSORS] = {0, 0};
int lastSentCCValue[N_ULTRASONIC_SENSORS] = {-1, -1};
int lastSentCCNumber[N_ULTRASONIC_SENSORS] = {-1, -1};
int lastSentCCChannel[N_ULTRASONIC_SENSORS] = {-1, -1};
const byte SUSTAIN_CC = 64;

// Global Sustain is toggled and sent to each distinct sensor channel. The
// channel list is synchronized if a sensor channel changes while Sustain is ON.
bool sustainIsActive = false;
byte sustainChannels[N_ULTRASONIC_SENSORS] = {0, 0};
byte sustainChannelCount = 0;

/////////////////////////////////////////////
// ACTIVE MIDI NOTE / PITCH BEND STATE
// One active note per sensor; note channels are remembered for proper Note Off.
int activeNote[N_ULTRASONIC_SENSORS] = {-1, -1};
byte activeNoteChannel[N_ULTRASONIC_SENSORS] = {0, 0};
int lastPlayedNote[N_ULTRASONIC_SENSORS] = {-1, -1};
unsigned long noteStoppedAt[N_ULTRASONIC_SENSORS] = {0, 0};

const int PITCH_BEND_CENTER = 8192;
const byte PITCH_BEND_RANGE_SEMITONES = 24; // +/-24 semitones (4 octaves total).
const int PITCH_BEND_UPDATE_THRESHOLD = 4;
int lastPitchBendValue[N_ULTRASONIC_SENSORS] = {-1, -1};

/////////////////////////////////////////////
// DISPLAY TIMING
const unsigned long DISPLAY_CONFIG_FLASH_MS = 800UL;
const unsigned long DISPLAY_NOTE_RELEASE_DELAY_MS = 250UL;
unsigned long configFlashUntil = 0;
unsigned long lastNoteScreenUntil = 0;
byte lastNoteSensor = SENSOR_1;
unsigned long lastDisplayRefreshAt = 0;

/////////////////////////////////////////////
// GENERAL SCALING
