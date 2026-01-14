/*
  Based on Sketch built by Gustavo Silveira (aka Music Nerd)
  Modified by Dolce Wang
----------------
  This code is only for Arduinos that use ATmega32u4 (like Micro, Pro Micro, Leonardo...)
  Remember to also assign the correct board in the IDE (like Tools / Boards / Sparkfun AVR / Pro Micro...)
----------------
  NOTE FROM MAX BRUCKERT: 
  1/ Code modified so that analog inputs send midi CCs, not notes.
     Analog inputs A0 to A5 == CC1 to 6 / Digital inputs 2 to 7 == CC7 to 12.
  2/ Remember to change the number of analog inputs. Otherwise, we get terrible parasites that modulate all inputs.
     Edit the potentiometer section according to the number of connected sensors.
-----------------
  Advanced MIDI Controller with Configuration and EEPROM Support
  
  Features:
  - Configurable MIDI settings
  - Debug mode
  - EEPROM configuration storage
  - Enhanced flexibility
*/
// Change any fields with //**

// LIBRARY
#include "MIDIUSB.h"
#include <EEPROM.h>

// CONFIGURATION MACROS
#define DEBUG_MODE true           // Enable/disable debug output
#define SERIAL_BAUD_RATE 115200   // Serial debug baud rate

// MIDI Configuration Macros
#define DEFAULT_MIDI_CHANNEL 0    // Default MIDI channel (0 = Channel 1)
#define DEFAULT_NOTE_BASE 7       // Base CC number for digital inputs
#define DEFAULT_CC_BASE 1         // Base CC number for analog inputs

// Hardware Configuration Macros
#define DEBOUNCE_DELAY 50         // Button debounce time (ms)
#define POT_THRESHOLD 10          // Potentiometer change threshold
#define POT_TIMEOUT 300           // Potentiometer movement timeout

// EEPROM Memory Addresses
#define EEPROM_MAGIC_ADDR 0       // Magic number to validate EEPROM data
#define EEPROM_MIDI_CH_ADDR 1     // MIDI Channel storage address
#define EEPROM_NOTE_BASE_ADDR 2   // Note base storage address
#define EEPROM_CC_BASE_ADDR 3     // CC base storage address

// Magic number to validate EEPROM configuration
const uint16_t EEPROM_MAGIC_NUMBER = 0xABCD;

// CONFIGURATION STRUCTURE
struct MIDIControllerConfig {
  byte midiChannel;
  byte noteBase;
  byte ccBase;
};

// Global Configuration Object
MIDIControllerConfig currentConfig = {
  DEFAULT_MIDI_CHANNEL,
  DEFAULT_NOTE_BASE,
  DEFAULT_CC_BASE
};

// Debug Macro
#define DEBUG_PRINT(x) do { if (DEBUG_MODE) Serial.print(x); } while (0)
#define DEBUG_PRINTLN(x) do { if (DEBUG_MODE) Serial.println(x); } while (0)

// EEPROM Management Functions
void initializeEEPROM() {
  // Check if EEPROM has valid configuration
  uint16_t storedMagic;
  EEPROM.get(EEPROM_MAGIC_ADDR, storedMagic);
  
  if (storedMagic != EEPROM_MAGIC_NUMBER) {
    // First-time setup or reset
    DEBUG_PRINTLN(F("Initializing EEPROM with default configuration"));
    
    // Store default configuration
    EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_NUMBER);
    EEPROM.put(EEPROM_MIDI_CH_ADDR, currentConfig.midiChannel);
    EEPROM.put(EEPROM_NOTE_BASE_ADDR, currentConfig.noteBase);
    EEPROM.put(EEPROM_CC_BASE_ADDR, currentConfig.ccBase);
  } else {
    // Load stored configuration
    DEBUG_PRINTLN(F("Loading configuration from EEPROM"));
    EEPROM.get(EEPROM_MIDI_CH_ADDR, currentConfig.midiChannel);
    EEPROM.get(EEPROM_NOTE_BASE_ADDR, currentConfig.noteBase);
    EEPROM.get(EEPROM_CC_BASE_ADDR, currentConfig.ccBase);
  }
}

// Advanced Configuration Update Function
bool updateMIDIConfiguration(byte newChannel, byte newNoteBase, byte newCcBase) {
  // Validate input ranges
  if (newChannel > 15 || newNoteBase > 127 || newCcBase > 119) {
    DEBUG_PRINTLN(F("Invalid MIDI configuration parameters"));
    return false;
  }

  // Update configuration
  currentConfig.midiChannel = newChannel;

