#include <LiquidCrystal.h>
#include <MIDI.h>
#include <midi_DEFS.h>
#include <EEPROM.h>
#include "AnalogDebounce.h"
#include "Rotary.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, midi1);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, midi2);

Rotary rotary = Rotary(23, 22);

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

void processRotaryEncoder()
{
  byte direction = rotary.process();
  if (direction)
  {
    if (direction == DIR_CW)
    {
      handleButtonUpPressed();
    }
    else
    {
      handleButtonDownPressed();
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
    lcdPrintMidiMonitor(midi1.getChannel(), midi1.getType(), midi1.getData1(), midi1.getData2());
  }
}

void MIDISendTo(
    midi::MidiInterface<HardwareSerial> from,
    midi::MidiInterface<HardwareSerial> to
) {
    // duplicate message on <from> input to <to> output
    to.send(
        from.getType(),
        from.getData1(),
        from.getData2(),
        from.getChannel()
    );
}

void MIDISendToAll(
    midi::MidiInterface<HardwareSerial> from
) {
    // duplicate message (except ActiveSensing) on <from> to all MIDI outputs
    midi::MidiType type = from.getType();
    if (type != midi::ActiveSensing) {
        MIDISendTo(from, midi1);
        MIDISendTo(from, midi2);
    }
}

void performMidiMapping()
{
  // Midi 1 is the midi mapper
  if (midi1.read())
  {
    int incomingMidiChannel = midi1.getChannel();
    MidiMapItem midiMapItem = midiMap[incomingMidiChannel];

    // The outgoing midi channel is overriden if a mapping exists, otherwise it is unchanged
    int outgoingMidiChannel = (midiMapItem.mapsTo > 0) ? midiMapItem.mapsTo : incomingMidiChannel;

    midi1.send(midi1.getType(),
               midi1.getData1(),
               midi1.getData2(),
               outgoingMidiChannel);

    doMidiMonitor();
  }

  // Midi 2 Merger
  // - Midi2 input is sent to Midi2 output
  // - Midi2 input is merged to Mid1 output
  if (midi2.read())
  {
    MIDISendToAll(midi2);
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

  rotary.begin();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextColor(WHITE); // Draw white text
  display.setTextSize(2);
  display.setCursor(0, 0);     // Start at top-left corner
  display.println("Midi 1");
  display.setTextSize(1);
  display.println("Midi mapper");
  display.println("");
  display.setTextSize(2);
  display.println("Midi 2");
  display.setTextSize(1);
  display.println("In2 => Out1 & Out2");
  display.display();

  // Initialize default midi mapping. i.e. Each channel maps to itself
  initializeDefaultMidiMap();

  // Listen to all MIDI channels on both inputs
  midi1.begin(MIDI_CHANNEL_OMNI);
  midi2.begin(MIDI_CHANNEL_OMNI);

  // disable automatic THRU handling
  midi1.turnThruOff();
  midi2.turnThruOff(); 

  lcd.begin(16, 2); // start the library
  //Print some initial text to the LCD.
  lcd.setCursor(0, 0); //top left

  lcd.print("MIDI Mapper!");

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
  processRotaryEncoder();
  AnalogKeypadButtons.loopCheck();

  performMidiMapping();
}