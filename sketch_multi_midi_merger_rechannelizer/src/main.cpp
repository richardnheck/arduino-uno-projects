#include <LiquidCrystal.h>
#include <MIDI.h>
#include <midi_DEFS.h>
#include <EEPROM.h>
#include "AnalogDebounce.h"
#include <ArduinoJson.h>

//#include <SoftwareSerial.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midiA);

const byte rxPin = 3;
const byte txPin = 2;

//SoftwareSerial mySerial(rxPin, txPin);

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define BUTTON_ADC_PIN A0 // A0 is the button ADC input for the Keypad

// Buttons are based on an analog keypad
//     1
// 4 3   0
//     2
#define BUTTON_NONE 255 //
#define BUTTON_RIGHT 0  //
#define BUTTON_UP 1     //
#define BUTTON_DOWN 2   //
#define BUTTON_LEFT 3   //
#define BUTTON_SELECT 4 //

// Forward declaration for keypad button push handler
void handleKeypadButtonPush(byte button);

// Specify the anlog pin and handler for debouncing
AnalogDebounce AnalogKeypadButtons(A0, handleKeypadButtonPush); // Analog Input 0,

const byte MaxChannel = 16;

/*
   --------------------------------------------------------------------------------------
   EXTENDED EEPROM READ AND WRITE FUNCTIONS 
   --------------------------------------------------------------------------------------
*/
// Absolute min and max eeprom addresses. Actual values are hardware-dependent.
// These values can be changed e.g. to protect eeprom cells outside this range.
const int EEPROM_MIN_ADDR = 0;
const int EEPROM_MAX_ADDR = 1023; // Arduino UNO => 1024B

// Returns true if the address is between the
// minimum and maximum allowed values, false otherwise.
//
// This function is used by the other, higher-level functions
// to prevent bugs and runtime errors due to invalid addresses.
boolean eeprom_is_addr_ok(int addr)
{
  return ((addr >= EEPROM_MIN_ADDR) && (addr <= EEPROM_MAX_ADDR));
}

/**
   Writes a sequence of bytes to eeprom starting at the specified address.
   Returns true if the whole array is successfully written.
   Returns false if the start or end addresses aren't between the minimum and maximum allowed values.
   When returning false, nothing gets written to eeprom.
*/
boolean eeprom_write_bytes(int startAddr, const byte *array, int numBytes)
{
  int i;
  // both first byte and last byte addresses must fall within the allowed range
  if (!eeprom_is_addr_ok(startAddr) || !eeprom_is_addr_ok(startAddr + numBytes))
  {
    return false;
  }
  for (i = 0; i < numBytes; i++)
  {
    EEPROM.write(startAddr + i, array[i]);
  }
  return true;
}

/**
   Writes a string starting at the specified address.
   Returns true if the whole string is successfully written.
   Returns false if the address of one or more bytes fall outside the allowed range.
   If false is returned, nothing gets written to the eeprom.
*/
boolean eeprom_write_string(int addr, const char *string)
{
  int numBytes;                  // actual number of bytes to be written
  numBytes = strlen(string) + 1; //write the string contents plus the string terminator byte (0x00)
  return eeprom_write_bytes(addr, (const byte *)string, numBytes);
}

/**
   Reads a string starting from the specified address.
   Returns true if at least one byte (even only the string terminator one) is read.
   Returns false if the start address falls outside the allowed range or declare buffer size is zero.

   The reading might stop for several reasons:
   - no more space in the provided buffer
   - last eeprom address reached
   - string terminator byte (0x00) encountered.
*/
boolean eeprom_read_string(int addr, char *buffer, int bufSize)
{
  byte ch;       // byte read from eeprom
  int bytesRead; // number of bytes read so far
  if (!eeprom_is_addr_ok(addr))
  { // check start address
    return false;
  }

  if (bufSize == 0)
  { // how can we store bytes in an empty buffer ?
    return false;
  }
  // is there is room for the string terminator only, no reason to go further
  if (bufSize == 1)
  {
    buffer[0] = 0;
    return true;
  }
  bytesRead = 0;                      // initialize byte counter
  ch = EEPROM.read(addr + bytesRead); // read next byte from eeprom
  buffer[bytesRead] = ch;             // store it into the user buffer
  bytesRead++;                        // increment byte counter
  // stop conditions:
  // - the character just read is the string terminator one (0x00)
  // - we have filled the user buffer
  // - we have reached the last eeprom address
  while ((ch != 0x00) && (bytesRead < bufSize) && ((addr + bytesRead) <= EEPROM_MAX_ADDR))
  {
    // if no stop condition is met, read the next byte from eeprom
    ch = EEPROM.read(addr + bytesRead);
    buffer[bytesRead] = ch; // store it into the user buffer
    bytesRead++;            // increment byte counter
  }
  // make sure the user buffer has a string terminator, (0x00) as its last byte
  if ((ch != 0x00) && (bytesRead >= 1))
  {
    buffer[bytesRead - 1] = 0;
  }
  return true;
}

/*
   --------------------------------------------------------------------------------------
   MENU
   --------------------------------------------------------------------------------------
*/
byte curMenuIndex = 0; // The currently selected menu page index
const byte NUM_MENU_PAGES = 6;

String menu[] = {
    "LOAD PATCH",
    "MIDIMAP",
    "SAVE PATCH",
    "CLEAR PATCH",
    "RESET MIDIMAP",
    "MIDI MONITOR"};

// These constants must be in the order of the above menu
const byte MENU_LOAD_PATCH = 0;
const byte MENU_MIDIMAP = 1;
const byte MENU_SAVE_PATCH = 2;
const byte MENU_CLEAR_PATCH = 3;
const byte MENU_RESET_MIDIMAP = 4;
const byte DEBUG_MENU_MONITOR = 5;

/*
   --------------------------------------------------------------------------------------
   MIDIMAP
   --------------------------------------------------------------------------------------
*/
class MidiMapItem
{
public:
  byte mapsTo;
  void incrementMapsTo();
  void decrementMapsTo();

private:
};

void MidiMapItem::incrementMapsTo()
{
  mapsTo = (mapsTo < MaxChannel) ? mapsTo + 1 : 1;
}

void MidiMapItem::decrementMapsTo()
{
  mapsTo = (mapsTo > 1) ? mapsTo - 1 : MaxChannel;
}

MidiMapItem midiMap[MaxChannel + 1]; // midiMap[0] is not used because we are not using zero index to make it easier to understand

/*
   --------------------------------------------------------------------------------------
   PATCH MANAGER
   --------------------------------------------------------------------------------------
*/
class PatchManager
{
public:
  static const byte MAX_PATCHES = 25; // The maximum number of patches
  // A patch contains a single midimap which contains 16 bytes (one byte for each midi channel)
  // It can be larger, depending on the EEPROM size.
  // Arduino UNO has 1KB of EEPROM so 1024/16 = 64 so UNO could have 64 patches
  byte patchNumber;
  void incrementPatchNumber();
  void decrementPatchNumber();
  void saveMidiMap();
  void loadMidiMap();
  bool patchExists();
  void clearPatch();

  // New functionality saving patches as json
  // Json to create is shown in example below
  // {
  //   "0": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15],
  //   "1": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15],
  //   "2": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15],
  //   "3": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15],
  //   "4": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15],
  //   "5": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15],
  //   "6": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15]
  // }

  // Based on ArduinoJson assistant the capacity required is:
  // 7*JSON_ARRAY_SIZE(16) + JSON_OBJECT_SIZE(7) + 14 bytes for strings duplication
  // https://arduinojson.org/v6/assistant/

  // ??? This means that the most basic of json is too much for a Arduino UNO
  static const int PatchCapacity = 7*JSON_ARRAY_SIZE(16) + JSON_OBJECT_SIZE(7) + 14;  // AVR 8-bit	952+14 = 966


  StaticJsonDocument<PatchManager::PatchCapacity> jsonDoc;

  void saveJsonPatch();
  void loadJsonPatch();
  void clearJsonPatch();
};

void PatchManager::incrementPatchNumber()
{
  patchNumber = (patchNumber < PatchManager::MAX_PATCHES - 1) ? patchNumber + 1 : 0;
}

void PatchManager::decrementPatchNumber()
{
  patchNumber = (patchNumber > 0) ? patchNumber - 1 : MAX_PATCHES - 1;
}

void PatchManager::saveMidiMap()
{
  int addr = patchNumber * MaxChannel; // the current address in the EEPROM (i.e. which byte we're going to write to next)
  for (byte i = 1; i <= MaxChannel; i++)
  {
    EEPROM.write(addr, midiMap[i].mapsTo);
    addr++;
  }
}

void PatchManager::loadMidiMap()
{
  int addr = patchNumber * MaxChannel; // the current address in the EEPROM (i.e. which byte we're going to write to next)
  for (byte i = 1; i <= MaxChannel; i++)
  {
    byte val = EEPROM.read(addr);
    if (val != 255)
    { // uninitialized EEPROM locations read 255
      midiMap[i].mapsTo = val;
    }
    addr++;
  }
}

bool PatchManager::patchExists()
{
  int addr = patchNumber * MaxChannel;
  byte val = EEPROM.read(addr);
  return val != 255; // A patch has been saved to the current patch number if the first byte is not 255 (when written it will be between 1-16
}

void PatchManager::clearPatch()
{
  int addr = patchNumber * MaxChannel; // the current address in the EEPROM (i.e. which byte we're going to write to next)
  for (byte i = 1; i <= MaxChannel; i++)
  {
    EEPROM.write(addr, 255); // clear the patch by writing 255
    addr++;
  }
}

void PatchManager::saveJsonPatch()
{
  // Write the midi map to json
  for (byte i = 1; i <= MaxChannel; i++)
  {
    jsonDoc["midiMap"][i-1] = midiMap[i].mapsTo;
  }

  // Declare a buffer to hold the serialized json string
  const int size = measureJson(jsonDoc) + 1;    // NB: measureJson() count doesn’t count the null-terminator so we need to add one to account for it in the string 
  char buffer[size];
  serializeJson(jsonDoc, buffer, size);         // Produce a minified JSON document

  Serial.println("Saving json string to address 0");
  eeprom_write_string(0, buffer);
}

void PatchManager::loadJsonPatch()
{
  // Load the patch from the EEPROM
  // TODO

  // Write the json midi map of the patch to the MidiMap
  // TODO
}

void PatchManager::clearJsonPatch()
{
}

/*
  --------------------------------------------------------------------------------------
  Variables
  --------------------------------------------------------------------------------------
*/
byte midiChannel = 1;

PatchManager patchManager;

bool enableThru = false; // Do not enable auto thru for the midi library

void initializeDefaultMidiMap()
{
  for (int i = 1; i <= MaxChannel; i++)
  {
    midiMap[i].mapsTo = i;
  }
}

/*
   --------------------------------------------------------------------------------------
   LCD FUNCTIONS
   --------------------------------------------------------------------------------------
*/

void lcdPrintMidiChannelMap()
{
  char buffer[6];
  sprintf(buffer, "%02d->%02d", midiChannel, midiMap[midiChannel].mapsTo);
  lcd.setCursor(0, 1);
  lcd.print(buffer);
}

void lcdPrintPatchNumber()
{
  char buffer[2];
  sprintf(buffer, "%02d", patchManager.patchNumber);
  lcd.setCursor(0, 1);
  lcd.print(buffer);
  if (patchManager.patchExists())
  {
    lcd.print("*");
  }
  else
  {
    lcd.print(" ");
  }
}

String previousTypeDescription = "";

/**
 * TODO: THIS STILL CRASHES WHEN MIDI MONITOR IS NOT THE FIRST THE MENU ITEM
 */
void lcdPrintMidiMonitor(byte channel, midi::MidiType type, midi::DataByte dataByte1, midi::DataByte dataByte2)
{
  if (type == midi::MidiType::Clock || type == midi::MidiType::ActiveSensing)
  {
    // ignore particular messages
    return;
  }
  String typeDescription = "";

  switch (type)
  {
  case midi::MidiType::NoteOn:
    typeDescription = "NoteOn";
    break;
  case midi::MidiType::NoteOff:
    typeDescription = "NoteOff";
    break;
  case midi::MidiType::AfterTouchPoly:
    typeDescription = "AfterTouchPoly";
    break;
  case midi::MidiType::AfterTouchChannel:
    typeDescription = "AfterTouchChan";
    break;
  case midi::MidiType::ControlChange:
    typeDescription = "ControlChange";
    break;
  case midi::MidiType::ProgramChange:
    typeDescription = "ProgramChange";
    break;
  case midi::MidiType::PitchBend:
    typeDescription = "PitchBend";
    break;
  case midi::MidiType::SystemExclusive:
    typeDescription = "SystemExclusive";
    break;
  case midi::MidiType::SongPosition:
    typeDescription = "SongPosition";
    break;
  case midi::MidiType::SongSelect:
    typeDescription = "SongSelect";
    break;
  case midi::MidiType::TuneRequest:
    typeDescription = "TuneRequest";
    break;
  case midi::MidiType::Start:
    typeDescription = "Start";
    break;
  case midi::MidiType::Stop:
    typeDescription = "Stop";
    break;
  case midi::MidiType::Continue:
    typeDescription = "Continue";
    break;
  case midi::MidiType::SystemReset:
    typeDescription = "SystemReset";
    break;
  default:
    typeDescription = "Unknown";
    break;
  }

  if (typeDescription != previousTypeDescription)
  {
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.print(typeDescription.c_str());
  }
  previousTypeDescription = typeDescription;

  lcd.setCursor(0, 1);
  char buffer[16];

  if (dataByte2 != 0)
  {
    snprintf(buffer, 16, "%02d %02X %02X %02X", channel, type, dataByte1, dataByte2);
  }
  else
  {
    snprintf(buffer, 16, "%02d %02X %02X", channel, type, dataByte1);
  }
  snprintf(buffer, 16, "%02d", 1);
  lcd.print(buffer);
}

void lcdPrintMenuPage()
{
  lcd.clear();
  lcd.print(menu[curMenuIndex]);
  if (curMenuIndex == MENU_MIDIMAP)
  {
    lcdPrintMidiChannelMap();
  }
  else if (curMenuIndex == MENU_LOAD_PATCH)
  {
    lcdPrintPatchNumber();
  }
  else if (curMenuIndex == MENU_SAVE_PATCH)
  {
    lcdPrintPatchNumber();
  }
  else if (curMenuIndex == MENU_CLEAR_PATCH)
  {
    lcdPrintPatchNumber();
  }
  else if (curMenuIndex == MENU_RESET_MIDIMAP)
  {
  }
  else if (curMenuIndex == DEBUG_MENU_MONITOR)
  {
  }
}

/*
   -------------------------------------------------------------------------------------------
   MIDIMAP PAGE LOGIC
   -------------------------------------------------------------------------------------------
*/
void midimap_incrementMidiChannel()
{
  midiChannel = (midiChannel < 16) ? midiChannel + 1 : 1;
  lcdPrintMidiChannelMap();
}

void midimap_decrementMidiChannel()
{
  midiChannel = (midiChannel > 1) ? midiChannel - 1 : 16;
  lcdPrintMidiChannelMap();
}

void midimap_incrementMapsToChannel()
{
  midiMap[midiChannel].incrementMapsTo();
  lcdPrintMidiChannelMap();
}

void midimap_decrementMapsToChannel()
{
  midiMap[midiChannel].decrementMapsTo();
  lcdPrintMidiChannelMap();
}

void resetMidiMap()
{
  initializeDefaultMidiMap();
  lcd.setCursor(0, 1);
  lcd.print("reset!");
  delay(250);
  curMenuIndex = MENU_MIDIMAP;
  lcdPrintMenuPage();
}

/*
   -------------------------------------------------------------------------------------------
   PATCH PAGES LOGIC
   -------------------------------------------------------------------------------------------
*/
void loadSelectedPatch()
{
  patchManager.loadMidiMap();
  lcd.setCursor(0, 1);
  lcd.print("loaded!");
  delay(250);
  curMenuIndex = MENU_MIDIMAP;
  lcdPrintMenuPage();
}

void saveMidiMapToSelectedPatch()
{
  patchManager.saveMidiMap();
  lcd.setCursor(0, 1);
  lcd.print("saved!");
  delay(250);
  curMenuIndex = MENU_MIDIMAP;
  lcdPrintMenuPage();
}

void clearSelectedPatch()
{
  patchManager.clearPatch();
  lcd.setCursor(0, 1);
  lcd.print("cleared!");
  delay(250);
  lcdPrintMenuPage();
}

void incrementPatchNumber()
{
  patchManager.incrementPatchNumber();
  lcdPrintPatchNumber();
}

void decrementPatchNumber()
{
  patchManager.decrementPatchNumber();
  lcdPrintPatchNumber();
}

/*
   -------------------------------------------------------------------------------------------
   MENU LOGIC
   -------------------------------------------------------------------------------------------
*/
void changeMenu()
{
  curMenuIndex = (curMenuIndex < (NUM_MENU_PAGES - 1)) ? curMenuIndex + 1 : 0; // change the menu page
  lcdPrintMenuPage();
}

/*
   -------------------------------------------------------------------------------------------
   HANDLE BUTTON LOGIC
   -------------------------------------------------------------------------------------------
*/

void handleButtonRightPressed()
{
  switch (curMenuIndex)
  {
  case MENU_MIDIMAP:
    midimap_incrementMidiChannel();
    break;
  case MENU_RESET_MIDIMAP:
    resetMidiMap();
    break;
  case MENU_LOAD_PATCH:
    loadSelectedPatch();
    break;
  case MENU_SAVE_PATCH:
    saveMidiMapToSelectedPatch();
    break;
  case MENU_CLEAR_PATCH:
    clearSelectedPatch();
    break;
  }
}

void handleButtonLeftPressed()
{
  switch (curMenuIndex)
  {
  case MENU_MIDIMAP:
    midimap_decrementMidiChannel();
    break;
  }
}

void handleButtonUpPressed()
{
  switch (curMenuIndex)
  {
  case MENU_MIDIMAP:
    midimap_incrementMapsToChannel();
    break;
  case MENU_LOAD_PATCH:
  case MENU_SAVE_PATCH:
  case MENU_CLEAR_PATCH:
    incrementPatchNumber();
    break;
  }
}

void handleButtonDownPressed()
{
  switch (curMenuIndex)
  {
  case MENU_MIDIMAP:
    midimap_decrementMapsToChannel();
    break;
  case MENU_LOAD_PATCH:
  case MENU_SAVE_PATCH:
  case MENU_CLEAR_PATCH:
    decrementPatchNumber();
    break;
  }
}

void handleButtonSelectPressed()
{
  changeMenu();
}

/**
 * Handle a Keypad button push
 * This is the callback handler when using the AnalogDebounce library
 */
void handleKeypadButtonPush(byte button)
{
  if (button != BUTTON_NONE)
  {
    switch (button)
    {
    case BUTTON_LEFT:
      handleButtonLeftPressed();
      break;
    case BUTTON_RIGHT:
      handleButtonRightPressed();
      break;
    case BUTTON_UP:
      handleButtonUpPressed();
      break;
    case BUTTON_DOWN:
      handleButtonDownPressed();
      break;
    case BUTTON_SELECT:
      handleButtonSelectPressed();
      break;
    }
  }
}

/*
   -------------------------------------------------------------------------------------------
   MIDI STUFF
   -------------------------------------------------------------------------------------------
*/
void doMidiMonitor()
{
  if (curMenuIndex == DEBUG_MENU_MONITOR)
  {
    lcdPrintMidiMonitor(midiA.getChannel(), midiA.getType(), midiA.getData1(), midiA.getData2());
  }
}

void performMidiMapping()
{
  if (enableThru)
  {
    // Thru on A has already pushed the input message to out A.
  }
  else
  {
    // Thru is disabled in the library so do it manually
    // Perform the Thru manually
    if (midiA.read())
    {
      int incomingMidiChannel = midiA.getChannel();
      MidiMapItem midiMapItem = midiMap[incomingMidiChannel];

      // The outgoing midi channel is overriden if a mapping exists, otherwise it is unchanged
      int outgoingMidiChannel = (midiMapItem.mapsTo > 0) ? midiMapItem.mapsTo : incomingMidiChannel;

      midiA.send(midiA.getType(),
                 midiA.getData1(),
                 midiA.getData2(),
                 outgoingMidiChannel);

      doMidiMonitor();
    }
  }
}

/*
   -------------------------------------------------------------------------------------------
   SETUP
   -------------------------------------------------------------------------------------------
*/
void setup()
{
  Serial.begin(9600);
  //int tickEvent = t.every(250, onTimerTick);

  // Initialize default midi mapping. i.e. Each channel maps to itself
  initializeDefaultMidiMap();

  // Initiate MIDI communications, listen to all channels
  midiA.begin(MIDI_CHANNEL_OMNI);

  // Handle default midi thru state for the library
  if (enableThru)
  {
    if (!midiA.getThruState())
    {
      midiA.turnThruOn();
    }
  }
  else
  {
    midiA.turnThruOff();
  }

  //pinMode( rxPin, INPUT );
  //pinMode( txPin, OUTPUT);
  //mySerial.begin( 31250 );

  lcd.begin(16, 2); // start the library
  //Print some initial text to the LCD.
  lcd.setCursor(0, 0); //top left

  lcd.print("MIDIChannelizer!");

  //loadMidiMapFromEEPROM();
  delay(250);
  for (int positionCounter = 0; positionCounter < 15; positionCounter++)
  {
    // scroll one position left:
    lcd.scrollDisplayLeft();
    // wait a bit:
    delay(100);
  }

  lcdPrintMenuPage();

  //button adc input
  pinMode(BUTTON_ADC_PIN, INPUT);    //ensure A0 is an input
  digitalWrite(BUTTON_ADC_PIN, LOW); //ensure pullup is off on A0
}

/*
   -------------------------------------------------------------------------------------------
   LOOP
   -------------------------------------------------------------------------------------------
*/
void loop()
{
  AnalogKeypadButtons.loopCheck();

  performMidiMapping();
}