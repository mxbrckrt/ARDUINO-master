/*
  Based on Sketch built by Gustavo Silveira (aka Music Nerd)
  Modified by Dolce Wang
------------
  This code is only for Arduinos that use ATmega32u4 (like Micro, Pro Micro, Leonardo...)
  Remember to also assign the correct board in the IDE (like Tools / Boards / Sparkfun AVR / Pro Micro...)
----------------
  NOTE BY MAX BRUCKERT (Translated & Updated): 
  1/ Removed all digital switch/button logic.
  2/ Configured for exactly ONE analog input (A0) to send Control Change (CC) messages.
  3/ Parasitic noise from unused pins is eliminated by reducing the input count to 1.
*/

#include "MIDIUSB.h"  

// --- POTENTIOMETERS ---
const int N_POTS = 1;                      // Set to 1 to read only A0
const int POT_PIN[N_POTS] = {A0};          // Analog Pin connected to the pot/sensor
int potCState[N_POTS] = {0};               
int potPState[N_POTS] = {0};               
int potVar = 0;                            

int midiCState[N_POTS] = {0};              
int midiPState[N_POTS] = {0};              

const int TIMEOUT = 300;                   // Time window to consider the pot "moving"
const int VAR_THRESHOLD = 10;              // Signal jitter threshold
boolean potMoving = true;                  
unsigned long PTime[N_POTS] = {0};         
unsigned long timer[N_POTS] = {0};         

// --- MIDI ASSIGNMENTS --- 
byte midiCh = 0;        // MIDI channel (0 = Ch 1)
byte potCCBase = 1;     // MIDI CC number to send (CC1)


void setup() {
  // No digital setup required as buttons have been removed
}

void loop() {
  handlePotentiometers();
}

// --- POTENTIOMETERS TO CC ---
void handlePotentiometers() {
  for (int i = 0; i < N_POTS; i++) {
    potCState[i] = analogRead(POT_PIN[i]); 
    midiCState[i] = map(potCState[i], 0, 1023, 0, 127); 

    potVar = abs(potCState[i] - potPState[i]); 

    if (potVar > VAR_THRESHOLD) {
      PTime[i] = millis(); 
    }

    timer[i] = millis() - PTime[i]; 

    if (timer[i] < TIMEOUT) {
      potMoving = true;
    }
    else {
      potMoving = false;
    }

    if (potMoving == true) {
      if (midiPState[i] != midiCState[i]) {
        // Send MIDI CC from Analog Pin
        controlChange(midiCh, potCCBase + i, midiCState[i]); 
        MidiUSB.flush();

        potPState[i] = potCState[i]; 
        midiPState[i] = midiCState[i];
      }
    }
  }
}

// --- MIDI USB FUNCTIONS ---
void controlChange(byte channel, byte control, byte value) {
  // 0x0B = Control Change message
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}