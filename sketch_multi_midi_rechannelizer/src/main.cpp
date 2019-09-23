//Sample using LiquidCrystal library
#include <LiquidCrystal.h>
#include <MIDI.h>
#include <midi_DEFS.h>
#include <EEPROM.h>
//#include "Timer.h"
/*******************************************************
  hisT program will test the LCD panel and the buttons
  https://www.auselectronicsdirect.com.au/assets/files/TA0055%20LCD%20Controller.pdf
  Mark Bramwell, July 2010
********************************************************/

//#include <SoftwareSerial.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midiA);

const byte rxPin = 3;
const byte txPin = 2;

//SoftwareSerial mySerial(rxPin, txPin);

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
// define some values used by the panel and buttons
int lcd_key = 0;
int adc_key_in = 0;
#define btnRIGHT 0
#define btnUP 1
#define btnDOWN 2
#define btnLEFT 3
#define btnSELECT 4
#define btnNONE 5

#define BUTTON_ADC_PIN A0 // A0 is the button ADC input

//return values for ReadButtons()
#define BUTTON_NONE 0   //
#define BUTTON_RIGHT 1  //
#define BUTTON_UP 2     //
#define BUTTON_DOWN 3   //
#define BUTTON_LEFT 4   //
#define BUTTON_SELECT 5 //

const byte MaxChannel = 16;

//Timer t;

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
static byte buttonJustPressed = false;  //this will be true after a ReadButtons() call if triggered
static byte buttonJustReleased = false; //this will be true after a ReadButtons() call if triggered
byte buttonWas = BUTTON_NONE;           //used by ReadButtons() for detection of button events

byte midiChannel = 1;

PatchManager patchManager;

bool enableThru = false;

void onTimerTick()
{
}

/*--------------------------------------------------------------------------------------
  ReadButtons()
  Detect the button pressed and return the value
  Uses global values buttonWas, buttonJustPressed, buttonJustReleased.
  --------------------------------------------------------------------------------------*/
byte ReadButtons()
{
  unsigned int buttonVoltage;
  byte button = BUTTON_NONE; // return no button pressed if the below checks don't write to btn

  //read the button ADC pin voltage
  buttonVoltage = analogRead(BUTTON_ADC_PIN);

  //sense if the voltage falls within valid voltage windows
  if (buttonVoltage < 50)
  {
    button = BUTTON_RIGHT;
  }
  else if (buttonVoltage < 195)
  {
    button = BUTTON_UP;
  }
  else if (buttonVoltage < 380)
  {
    button = BUTTON_DOWN;
  }
  else if (buttonVoltage < 555)
  {
    button = BUTTON_LEFT;
  }
  else if (buttonVoltage < 790)
  {
    button = BUTTON_SELECT;
  }

  //handle button flags for just pressed and just released events
  if ((buttonWas == BUTTON_NONE) && (button != BUTTON_NONE))
  {
    //the button was just pressed, set buttonJustPressed, this can optionally be used to trigger a once-off action for a button press event
    //it's the duty of the receiver to clear these flags if it wants to detect a new button change event
    buttonJustPressed = true;
    buttonJustReleased = false;
    //Serial.print("press");
  }
  if ((buttonWas != BUTTON_NONE) && (button == BUTTON_NONE))
  {
    buttonJustPressed = false;
    buttonJustReleased = true;
    //Serial.print("release");
  }

  //save the latest button value, for change event detection next time round
  buttonWas = button;

  return (button);
}

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
char buffer[16];

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
  if (dataByte2 != NULL)
  {
    sprintf(buffer, "%02d %02X %02X %02X", channel, type, dataByte1, dataByte2);
  }
  else
  {
    sprintf(buffer, "%02d %02X %02X", channel, type, dataByte1);
  }
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
byte button;
void loop()
{
  performMidiMapping();

  //get the latest button pressed, also the buttonJustPressed, buttonJustReleased flags
  button = ReadButtons();

  //show text label for the button pressed
  switch (button)
  {
  case BUTTON_NONE:
  {
    break;
  }
  case BUTTON_LEFT:
  {
    delay(250);
    handleButtonLeftPressed();
    break;
  }
  case BUTTON_RIGHT:
  {
    delay(250);
    handleButtonRightPressed();
    break;
  }
  case BUTTON_UP:
  {
    delay(250);
    handleButtonUpPressed();
    break;
  }
  case BUTTON_DOWN:
  {
    delay(250);
    handleButtonDownPressed();
    break;
  }
  case BUTTON_SELECT:
  {
    delay(250);
    handleButtonSelectPressed();
    break;
  }
  default:
  {
    break;
  }
  }

  //clear the buttonJustPressed or buttonJustReleased flags, they've already done their job now.
  if (buttonJustPressed)
    buttonJustPressed = false;
  if (buttonJustReleased)
    buttonJustReleased = false;
}