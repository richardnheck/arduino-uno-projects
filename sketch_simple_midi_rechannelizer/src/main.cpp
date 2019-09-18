#include <MIDI.h>
#include <midi_DEFS.h>
#include "LedControl.h"
#include <Rotary.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midiA);

const byte MaxChannel = 16;

/*
  DIN connects to pin 12
  CLK connects to pin 11
  CS connects to pin 10
*/
LedControl lc = LedControl(12, 11, 10, 1);

// Rotary encoder
Rotary rotary = Rotary(2, 3);

/*
  --------------------------------------------------------------------------------------
  Variables
  --------------------------------------------------------------------------------------
*/
byte character1[8] = {
    B00000000,
    B00001000,
    B00011000,
    B00001000,
    B00001000,
    B00001000,
    B00000000,
    B11111111};

byte character2[8] = {
    B00000000,
    B00111100,
    B00000100,
    B00111100,
    B00100000,
    B00111100,
    B00000000,
    B11111111};

byte character3[8] = {
    B00000000,
    B00111100,
    B00000100,
    B00111100,
    B00000100,
    B00111100,
    B00000000,
    B11111111};

byte character4[8] = {
    B00000000,
    B00100100,
    B00100100,
    B00111100,
    B00000100,
    B00000100,
    B00000000,
    B11111111};

byte character5[8] = {
    B00000000,
    B00111100,
    B00100000,
    B00111100,
    B00000100,
    B00111100,
    B00000000,
    B11111111};

byte character6[8] = {
    B00000000,
    B00111100,
    B00100000,
    B00111100,
    B00100100,
    B00111100,
    B00000000,
    B11111111};

byte character7[8] = {
    B00000000,
    B00111100,
    B00000100,
    B00000100,
    B00000100,
    B00000100,
    B00000000,
    B11111111};

byte character8[8] = {
    B00000000,
    B00111100,
    B00100100,
    B00111100,
    B00100100,
    B00111100,
    B00000000,
    B11111111};

byte character9[8] = {
    B00000000,
    B00111100,
    B00100100,
    B00111100,
    B00000100,
    B00111100,
    B00000000,
    B11111111};

byte character10[8] = {
    B00000000,
    B00101110,
    B01101010,
    B00101010,
    B00101010,
    B00101110,
    B00000000,
    B11111111};

byte character11[8] = {
    B00000000,
    B00100100,
    B01101100,
    B00100100,
    B00100100,
    B00100100,
    B00000000,
    B11111111};

byte character12[8] = {
    B00000000,
    B00101110,
    B01100010,
    B00101110,
    B00101000,
    B00101110,
    B00000000,
    B11111111};

byte character13[8] = {
    B00000000,
    B00101110,
    B01100010,
    B00101110,
    B00100010,
    B00101110,
    B00000000,
    B11111111};

byte character14[8] = {
    B00000000,
    B00101010,
    B01101010,
    B00101110,
    B00100010,
    B00100010,
    B00000000,
    B11111111};

byte character15[8] = {
    B00000000,
    B00101110,
    B01101000,
    B00101110,
    B00100010,
    B00101110,
    B00000000,
    B11111111};

byte character16[8] = {
    B00000000,
    B00101110,
    B01101000,
    B00101110,
    B00101010,
    B00101110,
    B00000000,
    B11111111};

const byte maxNum = 16;
byte *characters[maxNum] = {
    character1, character2, character3, character4, character5, character6, character7, character8,
    character9, character10, character11, character12, character13, character14, character15, character16};

// The currently selected midi channel
byte midiChannel = 1;

// The direction of rotation of the rotary encoder
byte direction = 0;

bool enableThru = false;

/**
 * Display a character on the 8x8 led matrix
 */ 
void displayCharacter(byte character[8])
{
  for (int c = 0; c < 8; c++)
  {
    lc.setRow(0, c, character[c]);
  }

  //  This code offsets the characters down and removes the red underline
  //  lc.setRow(0, 0, B00000000);
  //  for (int c = 1; c < 7; c++)
  //  {
  //    lc.setRow(0, c, character[c]);
  //  }
  //  lc.setRow(0, 7, B00000000);
}

void changeMidiChannel(byte direction)
{
  if (direction == DIR_CW)
  {
    // increment
    midiChannel = (midiChannel < 16) ? midiChannel + 1 : 1;
  }
  else
  {
    // decrement
    midiChannel = (midiChannel > 1) ? midiChannel - 1 : 16;
  }
}

/*
   -------------------------------------------------------------------------------------------
   SETUP
   -------------------------------------------------------------------------------------------
*/
void setup()
{
  rotary.begin();

  lc.shutdown(0, false);
  // Set brightness to a medium value
  lc.setIntensity(0, 2);
  // Clear the display
  lc.clearDisplay(0);

  displayCharacter(characters[midiChannel - 1]);

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
}

/*
   -------------------------------------------------------------------------------------------
   LOOP
   -------------------------------------------------------------------------------------------
*/

void loop()
{
  direction = rotary.process();
  if (direction) {
    changeMidiChannel(direction);
    displayCharacter(characters[midiChannel - 1]);
  }
  
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
      midiA.send(midiA.getType(),
                 midiA.getData1(),
                 midiA.getData2(),
                 midiChannel);
    }
  }
}