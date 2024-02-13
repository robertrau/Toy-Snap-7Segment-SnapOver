/*!
    @file    7SegFirmware.cpp
    @brief   Application for an Arduino mini that plugs into a SnapCircuit7SegDecodeArduino Board, and that board plugs on a Snap Circuit 7 segment display.
             Using the mode switch, the user can select between:
             W) 1-n digit counter with carry, latch, and lamp test functions.
             X) A 2 digit voltmeter
             Y) Roll the Dice
             Z) Random subtraction problems
             
    @version 0.04
    @date    2/12/2024
    @author  Bob Rau
    @bug     No know bugs......yet. But there were plenty! Most fixed.
*/


/*!
   @mainpage Firmware for my Snap Circuits 7 segment Snap-Over board
  7SegFirmware - Firmware for my Snap Circuits 7 segment Snap-Over board
*/

//  Written: 1/14/2024
//     Rev.: 0.00
//       By: Robert S. Rau
//  Changes: Started
//
//  Written: 1/17/2024
//     Rev.: 0.01
//       By: Robert S. Rau
//  Changes: added Carry
//
//  Written: 1/19/2024
//     Rev.: 0.02
//       By: Robert S. Rau
//  Changes: added volt meter and mode switch dispatch
//
//  Written: 2/10/2024
//     Rev.: 0.03
//       By: Robert S. Rau
//  Changes: Dice: made carry an input.
//
//  Written: 2/12/2024
//     Rev.: 0.04
//       By: Robert S. Rau
//  Changes: Dice: made carry an input.


//  ************
//  Board Setup
//  ************

// Inputs   Inputs   Inputs   Inputs

// manual enum of input functions for function Get0To1
int CountInput = 0;
int ClearInput = 1;
int TestInput = 2;
int LatchInput = 3;   //

// **  Input pins  **
#define CountPin 2
#define ClearPin 8
#define TestPin 12
#define LatchPin A0  // Mode W: A0 snap, setting high will hold the segments at the state they were at but the counter continues to operate normally. Can be used to make a frequency counter.
#define VinPin A0    // Mode X: analog input for voltmeter
static unsigned int LastInput[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int InputPins[4] = {CountPin, ClearPin, TestPin, LatchPin};

// Debounce times in milliseconds
#define DebounceTime 100

// Mode Switch
#define ModeSwitch A2   //  analog
//  A-D read    Function
//   3493-4095	Fault
//   3125-3492	W
//   2545-3124	X
//   2059-2544	Y
//   1000-2058	Z
//   0-999	Between positions

#define MODE_W 0   // BCD counter with latch and lamp test
#define MODE_X 1   // 2 digit volt meter
#define MODE_Y 2
#define MODE_Z 3
#define MODE_NONE 100
int CurrentModeOfOperation;
int LastModeOfOperation;

// **  Output pins  **
#define CarryPin 11

// Diagnostics
#define DebugLED 13


//    7 Segment Display pin assignments
#define SegmentA 6
#define SegmentB 5
#define SegmentC 3
#define SegmentD 10
#define SegmentE 9
#define SegmentF 7
#define SegmentG 4
#define SegmentDP A3

const int AllSegments[7] = {SegmentA, SegmentB, SegmentC, SegmentD, SegmentE, SegmentF, SegmentG};
// NumbersAsSegments defines all 10 digits with the MSB as segment A and the LSB unused.
const int NumbersAsSegments[10] = {0b11111100, 0b01100000, 0b11011010, 0b11110010, 0b01100110, 0b10110110, 0b10111110, 0b11100000, 0b11111110, 0b11110110};
//                                   blank      degrees     equal      3 x equal     y            o            "           A       minus
const int SymbolsAsSegments[9] = {0b00000000, 0b11000110, 0b00010010, 0b10010010, 0b01110110, 0b00111010, 0b01000100, 0b11101110, 0b00000010};

int BCDCounterValue = 0;   // counter for mode W
int VoltmeterStartupDone = 0;  // mode X
int VoltmeterIAmMSD = 0;     //  mode X
#define AnalogFullScale = 5  // full scale volts

int FirstValue;
int SecondValue;
int Answer;
int LatchedBCDCounterValue;
unsigned long i;


// Debug
#define BAUD_RATE 230400   //  debug serial baud rate



//******************************************************************************************************************
//******************************************************************************************************************
//
//   FUNCTIONS        FUNCTIONS        FUNCTIONS        FUNCTIONS        FUNCTIONS        FUNCTIONS        FUNCTIONS
//
//******************************************************************************************************************
//******************************************************************************************************************

/*!  SegmentWrite
   @brief This takes the information about a segment and the segment data and sets a pin, emulating open drain, high or low.
*/
void SegmentWrite(byte Segment, byte SegmentMask, byte SegmentData)
{
  pinMode(Segment, (SegmentMask & SegmentData) ? OUTPUT : INPUT);
}


/*!  BCDTo7Segment
   @brief This takes a binary code decimal (BCD) value and writes out the proper segments.
*/
int BCDTo7Segment(byte BCDValue)
{
  int SegmentData = NumbersAsSegments[BCDValue];
  int SegmentMask = 0b10000000;
  int Segment;
  for (Segment = 0; Segment < 7; Segment++)
  {
    SegmentWrite(AllSegments[Segment], SegmentMask, SegmentData);
    SegmentMask = SegmentMask >> 1;
  }
}


/*!  SymbolTo7Segment
   @brief This takes 
*/
int SymbolTo7Segment(byte SymbolIndex)
{
  int SegmentData = SymbolsAsSegments[SymbolIndex];
  int SegmentMask = 0b10000000;
  int Segment;
  for (Segment = 0; Segment < 7; Segment++)
  {
    SegmentWrite(AllSegments[Segment], SegmentMask, SegmentData);
    SegmentMask = SegmentMask >> 1;
  }
}

int Get0To1(unsigned int Input)
{
  unsigned int PinHL;
  unsigned int PinValue;
  unsigned int PinOldNew;
  unsigned int IncrementValue;

  PinHL = digitalRead(Input);
  PinValue = (PinHL == HIGH) ? 1U : 0U;
  PinOldNew = LastInput[Input] | PinValue;
  //Serial.println(PinOldNew, BIN);    //    debug  
  //Serial.print(PinOldNew);    //    debug   
  if (LastInput[Input] != (PinValue << 1))
  {
    delay(30); 
  }
  LastInput[Input] = PinValue << 1U;      // update last state
  IncrementValue = (PinOldNew == 0b01) ? 1 : 0;    //  0b01 is old state 0 and new state 1, so a 0 to 1 transition.
   return IncrementValue;
}


void RandomDice()
{
  static int LastTime = 0;
  int DiceValue;
  DiceValue = LastTime;
  while (DiceValue == LastTime)
  {
    DiceValue = random(6);  // value from 0-5
  }
  LastTime = DiceValue;
  BCDTo7Segment(DiceValue + 1);

}

int ReadModeSwitch()
{
  int ModeVoltage = analogRead(ModeSwitch);
  //Serial.println(ModeVoltage);    //    debug
  if ((ModeVoltage >= 100) && (ModeVoltage <= 417))
  {
    return MODE_Z;
  }
  if ((ModeVoltage >= 418) && (ModeVoltage <= 609))
  {
    return MODE_Y;
  }
  if ((ModeVoltage >= 610) && (ModeVoltage <= 778))
  {
    return MODE_X;
  }
  if ((ModeVoltage >= 779) && (ModeVoltage <= 900))
  {
    return MODE_W;
  }
  return MODE_NONE;
}

int DelayAndCheckStillInMode(int Milliseconds, int ThisMode)
{
  long DoneMilliseconds;
  DoneMilliseconds = millis() + Milliseconds;
  while((ReadModeSwitch() == ThisMode) && (millis() < DoneMilliseconds))
  {
    // just wait
  }
  return (ReadModeSwitch() == ThisMode);
}


//******************************************************************************************************************
//******************************************************************************************************************
//
//   SETUP     SETUP     SETUP     SETUP     SETUP     SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
//
//******************************************************************************************************************
//******************************************************************************************************************

void setup()
{
  // put your setup code here, to run once:

  // Mode setup
  CurrentModeOfOperation = ReadModeSwitch();
  LastModeOfOperation = 900;  // this will force an init of any mode

  //    7 Segment Outputs
  // Digital pins configured as inputs without a pull up, when configured as outputs will be low.
  // so setting the pin as input and output is like simulating an open drain output.

  // Initialize all segments to a zero, start by setting all pull up resistors off
  pinMode(SegmentA, INPUT); // Segment A off
  pinMode(SegmentB, INPUT); // Segment B off
  pinMode(SegmentC, INPUT); // Segment C off
  pinMode(SegmentD, INPUT); // Segment D off
  pinMode(SegmentE, INPUT); // Segment E off
  pinMode(SegmentF, INPUT); // Segment F off
  pinMode(SegmentG, INPUT); // Segment G off
  pinMode(SegmentDP, INPUT); // Segment G off

  // Now make the zero
  pinMode(SegmentA, OUTPUT); // Segment A on
  pinMode(SegmentB, OUTPUT); // Segment B on
  pinMode(SegmentC, OUTPUT); // Segment C on
  pinMode(SegmentD, OUTPUT); // Segment D on
  pinMode(SegmentE, OUTPUT); // Segment E on
  pinMode(SegmentF, OUTPUT); // Segment F on

  // Carry setup
  pinMode(CarryPin, OUTPUT);  // totempole output
  digitalWrite(CarryPin, LOW);

  // test pin
  pinMode(TestPin, INPUT); 

  VoltmeterStartupDone = 0;  // mode X
  VoltmeterIAmMSD = 0;


  // setup for debug

  Serial.begin(BAUD_RATE, SERIAL_8N1); // for debugging

  Serial.println("");            // debug
  Serial.println("RESET");            // debug
  Serial.println("");            // debug



}


//******************************************************************************************************************
//******************************************************************************************************************
//
//   LOOP      LOOP      LOOP      LOOP      LOOP      LOOP      LOOP      LOOP      LOOP      LOOP      LOOP
//
//******************************************************************************************************************
//******************************************************************************************************************

void loop()
{
  int BCDCounterIncrement;
  
  int LatchTriggered;
  int MyCount;
   //Serial.println(MyCount++);    //    debug  

  // Check mode switch
  CurrentModeOfOperation = ReadModeSwitch();
  //Serial.println(CurrentModeOfOperation);    //    debug  
  if (CurrentModeOfOperation != LastModeOfOperation)
  {
    // clean up between modes
    BCDCounterValue = 0;   // counter for mode W
    VoltmeterStartupDone = 0;
    pinMode(TestPin, INPUT); 
  }
  LastModeOfOperation = CurrentModeOfOperation;


  switch (CurrentModeOfOperation)
  {
    case MODE_W:
      // Count function
      BCDCounterIncrement = Get0To1(CountPin);
      // Serial.println(9, BCDCounterIncrement);    //    debug
      BCDCounterValue = BCDCounterValue + BCDCounterIncrement;
      if (BCDCounterValue > 9)
      {
        digitalWrite(CarryPin, HIGH);
        BCDCounterValue = 0;
        delay(100);
      }
      else
      {
        digitalWrite(CarryPin, LOW);
      }

      // Clear function
      if (digitalRead(ClearPin) == HIGH)
      {
        BCDCounterValue = 0;
      }

      // Capture counter value when entering latch state
      LatchTriggered = Get0To1(LatchPin);
      if (LatchTriggered == 1)
      {
        LatchedBCDCounterValue = BCDCounterValue;
      }

      // Test (lamp Test)   doesn't interrupt counting
      if (digitalRead(TestPin) == HIGH)
      {
        // Light up every segment and decimal point for lamp test
        BCDTo7Segment(8);   // For lamp test, display an 8 (all segments on...)
        pinMode(SegmentDP, OUTPUT);   //   ... and the decimal point
      }
      else
      {
        // ... and if not lamp test: Release control of decimal point...
        pinMode(SegmentDP, INPUT);

        // ...and restore either counting or latched state
        if (digitalRead(LatchPin) == LOW)
        {
          BCDTo7Segment(BCDCounterValue);
        }
        else
        {
          // restore latched data (if lamp test was done)
          BCDTo7Segment(LatchedBCDCounterValue);
        }
      }

      // debounce (only if critical IO changed state)
      //if ((BCDCounterIncrement != 0) || (LatchTriggered != 0))
      //{
        //delay(DebounceTime);
      //}
      break;


    case MODE_X:
      // 2 digit Volt meter mode (both boards must be set to same mode) AND both boards need to be powered up at the same time!
      // We hijack the test pin for our output latch function.
      // Count and Carry are used for LSD MSD detection diring configuration, then to increment the 10s digit.
      // reads from 0.0 to 5.0 volts.
      int CountInput;
      pinMode(CountPin, INPUT);  // 
      if (VoltmeterStartupDone != 1)
      {
        //  First figure out if I am the LSD or the MSD
        digitalWrite(CarryPin, HIGH);
        delay(200);
        // Now, if we are the LSD, there is no connection to our Count input, so it should be low
        //  ... and if we are the MSD, the LSD Carry is making our Count input High.
        CountInput = digitalRead(CountPin);
        if (CountInput == HIGH)
        {
          VoltmeterIAmMSD = 1;
        }

        VoltmeterStartupDone = 1;  // mode X
      }

      // So now we know if the are the LSD or the MSD.
      // The LSD will read the A/D, clear the MSD, and pass the MSD value on the count line.
      //  then unlatch and latch the display.
      if (VoltmeterIAmMSD == 0)
      {
        // LSD
        unsigned long RawAnalog = analogRead(VinPin);
        unsigned long VinCounts = 100U * RawAnalog;  //  10 bit value scaled up so we don't loose accuracy in our math
        unsigned long NumConstant = 102400;
        unsigned long Display = VinCounts / (NumConstant / 50);   // ... and the 1024 scaled up to keep things in scale
        //Serial.print(VinCounts);    //    debug  
        unsigned int MSDValue = Display / 10U;
        unsigned int LSDValue = Display - (MSDValue * 10U);
        
        //Serial.print("__");    //    debug  
        //Serial.println(Display);    //    debug 
        
        // setup latch
        pinMode(ClearPin, OUTPUT); 
        digitalWrite(ClearPin, HIGH);

        // Now send MSD
        //digitalWrite(ClearPin, HIGH);
        //delay(50);
        //digitalWrite(ClearPin, LOW);
        //delay(250);   // wait for MSD to get through its Arduino loop
        int i;
        for (i = 0; i < MSDValue; i++)
        {
          delay(70);
          digitalWrite(CarryPin, HIGH);
          delay(70);
          digitalWrite(CarryPin, LOW);
          
        }
        delay(20);
        // and clear the latch
        digitalWrite(ClearPin, LOW);
        BCDTo7Segment(LSDValue);
        delay(80);

      }
      else
      {
        // MSD
        static unsigned int MSD;
        static unsigned int ClearStart;
        
        pinMode(ClearPin, INPUT);
        pinMode(CountPin, INPUT);  //
        pinMode(SegmentDP, OUTPUT);   //   set the decimal point

        ClearStart = Get0To1(ClearPin);
        if (ClearStart == 1)
        {
          BCDTo7Segment(MSD);
          MSD = 0;
          Serial.println("C");    //    debug   
        }
        MSD = MSD + Get0To1(CountPin);
      }

      break;


    case MODE_Y:
      //  Press the count button to roll the dice
      // Count & Carry setup
      pinMode(CountPin, INPUT);  // 
      pinMode(CarryPin, INPUT);  // totempole INPUT, this lets us switch from 2 digit coiunter to dice without removing the carry to count connection.
     

      static unsigned long RollDelay;
      int DiceValue;
      //SymbolTo7Segment(4);  //  y
      if (digitalRead(CountPin) == HIGH)  // wait for the count button to be pressed
      {
        pinMode(SegmentDP, INPUT);
        for(i=0;i<111222;i++){}  // let analog input a6 float a bit before reading it.
        randomSeed(analogRead(A6) * millis());  // A6 is not hooked to anything so its readings vary a lot. millis is milliseconds since the Arduino powered up.
        RollDelay = random(40);
        //Serial.println(-RollDelay);    //    debug
        //DiceValue = random(6);  // value from 0-5
        //BCDTo7Segment(DiceValue + 1);
        RandomDice();
        delay(20);
      }
      else
      {
        if (RollDelay < 800)
        {
        delay(RollDelay);
        //DiceValue = random(6);  // value from 0-5
        //BCDTo7Segment(DiceValue + 1);
        RandomDice();
        if (RollDelay < 500)
        {
          RollDelay = RollDelay * 5;
        }
        else
        {
          RollDelay = RollDelay * 9;
        }
        RollDelay = RollDelay >> 2;
        }
        else
        {
          pinMode(SegmentDP, OUTPUT);
        }
      }
      break;


    case MODE_Z:  //  Subtraction problems
    for(i=0;i<51222;i++){}  // let analog input a6 float a bit before reading it.
    analogRead(A6);
    for(i=0;i<51222;i++){}  // let analog input a6 float a bit before reading it.
    analogRead(A6);
     for(i=0;i<51222;i++){}  // let analog input a6 float a bit before reading it.
    analogRead(A6);
    randomSeed(analogRead(A6) + millis());  // A6 is not hooked to anything so its readings vary a lot. millis is milliseconds since the Arduino powered up.
    pinMode(SegmentDP, INPUT);   //   clear the decimal point
      FirstValue = 5 + int(random(5));  // value from 5 to 9
      SecondValue = random(6);  // value from 0-5
      Answer = FirstValue - SecondValue;
      BCDTo7Segment(FirstValue);
      DelayAndCheckStillInMode(1200, MODE_Z);
      SymbolTo7Segment(0);  //  blank
      DelayAndCheckStillInMode(600, MODE_Z);
      SymbolTo7Segment(8);  //  minus
      DelayAndCheckStillInMode(600, MODE_Z);
      SymbolTo7Segment(0);  //  blank
      DelayAndCheckStillInMode(600, MODE_Z);
      BCDTo7Segment(SecondValue);
      DelayAndCheckStillInMode(1200, MODE_Z);
      SymbolTo7Segment(0);  //  blank
      DelayAndCheckStillInMode(600, MODE_Z);
      SymbolTo7Segment(2);  //  equal
      DelayAndCheckStillInMode(600, MODE_Z);
      SymbolTo7Segment(0);  //  blank
      DelayAndCheckStillInMode(600, MODE_Z);
      BCDTo7Segment(Answer);
      DelayAndCheckStillInMode(1600, MODE_Z);
      SymbolTo7Segment(0);  //  blank
      DelayAndCheckStillInMode(1600, MODE_Z);
      break;


    default:
      // panic
      SymbolTo7Segment(3);  //  3 x equal
  }
}
