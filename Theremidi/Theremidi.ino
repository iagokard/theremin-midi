/*
  Made by Gustavo Silveira, 2019.
  - This adapted sketch reads two HC-SR04 ultrasonic sensors and sends MIDI notes, Pitch Bend and MIDI CC over USB
  - Sensor 1 controls notes / Pitch Bend; sensor 2 controls MIDI CC

  http://www.musiconerd.com
  http://www.youtube.com/musiconerd
  http://facebook.com/musiconerdmusiconerd
  http://instagram.com/musiconerd/
  http://www.gustavosilveira.net
  gustavosilveira@musiconerd.com

  If you are using for anything that's not for personal use don't forget to give credit.

  PS: Just change the value that has a comment like " //* "

*/

/////////////////////////////////////////////
// Choosing your board
// Define your board, choose:
// "ATMEGA328" if using ATmega328 - Uno, Mega, Nano...
// "ATMEGA32U4" if using with ATmega32U4 - Micro, Pro Micro, Leonardo...
// "TEENSY" if using a Teensy board
// "DEBUG" if you just want to debug the code in the serial monitor
// you don't need to comment or uncomment any MIDI library below after you define your board

#define ATMEGA32U4 1//* put here the uC you are using, like in the lines above followed by "1", like "ATMEGA328 1", "DEBUG 1", etc.


/////////////////////////////////////////////
// Are you using an I2C Oled Display?
#define USING_DISPLAY 1 //* comment if not using an I2C Oled Display.

/////////////////////////////////////////////
// LIBRARIES
// -- Defines the MIDI library -- //


// if using with ATmega328 - Uno, Mega, Nano...
#ifdef ATMEGA328
#include <MIDI.h>
//MIDI_CREATE_DEFAULT_INSTANCE();

// if using with ATmega32U4 - Micro, Pro Micro, Leonardo...
#elif ATMEGA32U4
#include "MIDIUSB.h"

#endif
// ---- //

//////////////////////
// Oled Display I2C
#ifdef USING_DISPLAY
//#include <Adafruit_GFX.h>  // Include core graphics library for the display
#include <Adafruit_SSD1306.h>  // Include Adafruit_SSD1306 library to drive the display
//#include <Fonts/FreeMonoBold12pt7b.h>  // Add a custom font
//#include <Fonts/FreeMono9pt7b.h>  // Add a custom font
Adafruit_SSD1306 display(128, 64);  // Create display - size of the display in pixels
#endif

//////////////////////
// scaleNotes
//#include "scaleNames.h"
//#include "scales.h"
//#include "notes.h"

/////////////////////////////////////////////
// BUTTONS
const int N_BUTTONS = 7; //*  total numbers of buttons. Number of buttons in the Arduino + number of buttons on multiplexer 1 + number of buttons on multiplexer 2...
const int N_BUTTONS_ARDUINO = 7; //* number of buttons connected straight to the Arduino (in order)
const int BUTTON_ARDUINO_PIN[N_BUTTONS] = {4, 5, 6, 7, 8, 9, 10}; //* pins of each button connected straight to the Arduino

int buttonCState[N_BUTTONS] = {0};        // stores the button current value
int buttonPState[N_BUTTONS] = {0};        // stores the button previous value

//#define pin13 1 // uncomment if you are using pin 13 (pin with led), or comment the line if it is not
byte pin13index = 12; //* put the index of the pin 13 of the buttonPin[] array if you are using, if not, comment

// debounce
unsigned long lastDebounceTime[N_BUTTONS] = {0};  // the last time the output pin was toggled
unsigned long debounceDelay = 20;    //* the debounce time; increase if the output flickers


/////////////////////////////////////////////
// HC-SR04 ULTRASONIC SENSORS
// Sensor 1 controls notes / Pitch Bend.
// Sensor 2 controls MIDI CC (CC7 by default).
// On an Arduino Leonardo, A0-A3 can also be used as digital I/O (D14-D17).
const byte SENSOR_NOTE = 0;
const byte SENSOR_CC = 1;
const byte N_ULTRASONIC_SENSORS = 2;

const byte ULTRASONIC_TRIG_PIN[N_ULTRASONIC_SENSORS] = {A0, A2};
const byte ULTRASONIC_ECHO_PIN[N_ULTRASONIC_SENSORS] = {A1, A3};

// Practical playing window. Values outside this range are treated as "no hand".
// HC-SR04's advertised range is wider, but a smaller zone is easier to play.
const float ULTRASONIC_MIN_DISTANCE_CM = 8.0f;
const float ULTRASONIC_MAX_DISTANCE_CM = 60.0f;

// HC-SR04 needs a >=10us TRIG pulse. The echo timeout below corresponds to
// roughly 103cm, which is enough for the 8-60cm playing window.
const unsigned long ULTRASONIC_ECHO_TIMEOUT_US = 6000UL;

// Keep >60ms between any two triggers. Since the two sensors alternate,
// each individual sensor is measured about every 130ms.
const unsigned long ULTRASONIC_TRIGGER_INTERVAL_MS = 65UL;

// One-pole low-pass filter. Higher values respond faster; lower values smooth more.
const float ULTRASONIC_FILTER_ALPHA = 0.35f;

// Ignore one isolated failed/out-of-range echo; release after two consecutive ones.
const byte ULTRASONIC_INVALID_READINGS_TO_RELEASE = 2;

float ultrasonicDistanceCm[N_ULTRASONIC_SENSORS] = {0.0f, 0.0f};
boolean ultrasonicFilterInitialized[N_ULTRASONIC_SENSORS] = {false, false};
boolean ultrasonicSensorActive[N_ULTRASONIC_SENSORS] = {false, false};
byte ultrasonicInvalidReadings[N_ULTRASONIC_SENSORS] = {0, 0};

/////////////////////////////////////////////
// DISPLAY
byte display_pos_x = 0;
byte display_pos_y = 0;
byte display_text_size = 2;

/////////////////////////////////////////////
// SENSOR / MIDI STATE
float IR_curve = 1.0f; // Retained from the original sketch's scaling function.
boolean note_is_playing = false;

/////////////////////////////////////////////
// MIDI STUFF
int MIDI_CH = 0; //* MIDI channel to be used
byte NOTE = 48; //* Lowest NOTE to be used - The root
byte CC = 7; //* MIDI CC sent by ultrasonic sensor 2 (default: CC7 / Volume)
byte sustainCC = 64; //* cc for the sustain pedal
boolean rootMenuIsOn = false;

boolean pitchBendIsOn = false;
boolean pitchBendMenuIsOn = false;
boolean pitchBendIsPlaying = false;

int pitchBendVal = 8192;
int pitchBendPVal = -1;

/////////////////////////////////////////////
// scaleNotes
byte scaleIndex = 0;
int octaveIndex = 2;
int octave[5] = { -24, -12, 0, 12, 24};

const byte SCALE_NUM = 4; // number of scales
const byte NOTE_NUM = 16; // number of notes in the scales


int scaleNotes[SCALE_NUM][NOTE_NUM] {

  { -1, 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24}, // 0: ionian
  { -1, 0, 2, 3, 5, 7, 8, 11, 12, 14, 15, 17, 19, 20, 23, 24}, // 4: min harm
  { -2, 0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24, 27, 29, 31, 34}, // 5: minor penta
  { -2, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28}, // 7: whltone
  //  { -2, 0, 2, 3, 5, 7, 8, 10, 12, 14, 15, 17, 19, 20, 22, 24}, // 4: aeolian
  //  { -4, 0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24, 26, 28, 31, 33}, // 5: major pentatonic
  //  { -2, 0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24, 27, 29, 31, 34}, // 6: minor pentatonic
  //  { -2, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28}, // 7: wholetone

};

char* scaleNames[] = {"Jonian", "MinHar", "MinPen", "WhlTon"};

char* noteNames[] = {
  "C", "C#", "D", "D#", "E", "F", "F#",  "G", "G#", "A", "A#", "B"
};

char* pitchBendNames[] = {"Scl", "Pb"};

byte noteOut = 0;
