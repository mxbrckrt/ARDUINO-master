/*
  =============================================================================
  MIDI Controller for ATmega32u4 (Micro, Leonardo, Pro Micro)
  Author: Max Bruckert
  Based on Sketch by Gustavo Silveira & Dolce Wang
  =============================================================================
  
  WIRING EXPLANATION:
  This script uses the Internal Pull-up resistor, which means you do NOT need 
  external resistors. 
  
  1. Take your button/switch.
  2. Connect one side to a Digital Pin (e.g., D2).
  3. Connect the other side to a GND (Ground) pin on the Arduino.
  
  The signal is "Active Low": when the button is open, the pin is 5V (HIGH).
  When you press the button, it connects to GND (LOW), triggering the MIDI note.

  USER SETTINGS:
  - toggleMode: Switch between Momentary (piano) and Toggle (on/off).
  - velocity: Set the "force" of the note (0 to 127).
  - debugMode: View activity in the Arduino Serial Monitor.
  =============================================================================
*/

#include "MIDIUSB.h"

// --- USER EDITABLE SETTINGS ---
bool toggleMode = false;       // true = Toggle (On/Off) | false = Momentary
int velocity = 127;           // General Velocity (0 = silent, 127 = max)
bool debugMode = false;        // true = Serial Monitor feedback | false = Silent
byte midiCh = 0;              // MIDI Channel (0 = Channel 1)
byte noteBase = 36;           // Starting MIDI note (36 = C2 / Kick)

const int N_BUTTONS = 1;                // Number of active buttons
const int BUTTON_PINS[N_BUTTONS] = {2}; // The Digital Pins used

// --- INTERNAL STATE TRACKING ---
int buttonPState[N_BUTTONS];      
bool noteActive[N_BUTTONS];       
unsigned long lastDebounceTime[N_BUTTONS];
unsigned long debounceDelay = 50; 

void setup() {
  if (debugMode) {
    Serial.begin(31250); 
  }

  for (int i = 0; i < N_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    buttonPState[i] = HIGH;
    noteActive[i] = false;
    lastDebounceTime[i] = 0;
  }
}

void loop() {
  handleButtons();
}

void handleButtons() {
  for (int i = 0; i < N_BUTTONS; i++) {
    int currentButtonState = digitalRead(BUTTON_PINS[i]);
    byte noteToSend = noteBase + i;

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (currentButtonState != buttonPState[i]) {
        lastDebounceTime[i] = millis();

        if (toggleMode) {
          if (currentButtonState == LOW) {
            noteActive[i] = !noteActive[i];
            if (noteActive[i]) {
              sendNote(midiCh, noteToSend, velocity, "Toggle ON");
            } else {
              sendNote(midiCh, noteToSend, 0, "Toggle OFF");
            }
          }
        } 
        else {
          if (currentButtonState == LOW) {
            sendNote(midiCh, noteToSend, velocity, "Note ON");
          } else {
            sendNote(midiCh, noteToSend, 0, "Note OFF");
          }
        }
        buttonPState[i] = currentButtonState;
      }
    }
  }
}

// --- HELPER FUNCTION: SEND MIDI & DEBUG ---
void sendNote(byte channel, byte note, byte vel, String label) {
  midiEventPacket_t event = {0x09, (byte)(0x90 | channel), note, (byte)vel};
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();

  if (debugMode) {
    Serial.print(label);
    Serial.print(" | Note: ");
    Serial.print(note);
    Serial.print(" | Vel: ");
    Serial.print(vel);
    Serial.print(" | Ch: ");
    Serial.println(channel + 1);
  }
}