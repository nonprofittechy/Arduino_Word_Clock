#include <DS1307RTC.h>
#include <Time.h>
#include <Wire.h>
#include "LedControl.h"

const int birthDay = 15;
const int birthMonth = 6;

const int minUpPin = 3;
const int minDownPin = 5;

int minUpState;
int minDownState;

int lastMinUpState = HIGH;  // we are using the pull-up resistor, so we check for pin state == LOW to find a button press
int lastMinDownState = HIGH;

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastUpDebounceTime = 0;  // the last time the output pin was toggled
long lastDownDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 50;    // the debounce time; increase if the output flickers


// an ledWord can only span one row but may span multiple MAXIM7221s
struct ledWord {
  int row;
  int colsValue; // the byte value we want set for this row
};

/*
 * Here we define an array of ledWord objects that contain the pin address for each word we will want to separately control on the display
 * see key below to figure out mapping of second value to a column in the array.
 */
ledWord ITIS[] =   { { 0,128+64+16+8},
                  { 0,0}};
//ledWord IS[] =  { {0,16+8},
//                  {0,0}};
ledWord TEN[]= { {0,4+2+1},
                  {0,0}};
ledWord HALF[]= { {0,0},
                  {0,8+4+2+1}};
ledWord QUARTER[] =   { {1, 64+32+16+8+4+2+1},
                        {0,0} };
ledWord TWENTY[]  =   { {0,0},
                        {1,32+16+8+4+2+1}};
ledWord FIVE[]    =   { {2,128+64+32+16},
                        {0,0}};
ledWord HAPPY[]   =   { {2,8+4+2+1},
                        {2,128}};
ledWord PAST[]    =   { {0,0},
                        {2,8+4+2+1}};
ledWord BIRTHDAY[]=   { {3,64+32+16+8+4+2+1},
                        {3,128}};
ledWord TO[]      =   { {0,0},
                        {3,32+16}};
ledWord ONEOCLOCK[] = { {0,0},
                        {3,4+2+1}};
ledWord TWOOCLOCK[] = { {4,128+64+32},
                        {0,0}};
ledWord THREEOCLOCK[] = { {4,8+4+3+2+1},
                          {0,0}};
ledWord FOUROCLOCK[]  = { {0,0},
                          {4,128+64+32+16}};
ledWord FIVEOCLOCK[]  = { {0,0},
                          {4,8+4+2+1}};
ledWord SIXOCLOCK[]   = { {5,128+64+32},
                          {0,0}};
ledWord SEVENOCLOCK[] = { {5,2+1},
                          {5,128+64+32}};
ledWord EIGHTOCLOCK[] = { {0,0},
                          {5,16+8+4+2+1}};
ledWord NINEOCLOCK[]  = { {6,128+64+32+16},
                          {0,0}};
ledWord TENOCLOCK[]   = { {6,4+2+1},
                          {0,0}};
ledWord ELEVENOCLOCK[]= { {0,0},
                          {6,32+16+8+4+2+1}};
ledWord TWELVEOCLOCK[]= { {7,128+64+32+16+8+4},
                          {0,0}};
ledWord NAME[]        = { {7,1},
                          {7,128+64+32}};
ledWord DOT1[]        = { {0,0},
                          {7,8}};
ledWord DOT2[]        = { {0,0},
                          {7,8+4}}; // value includes previous dots
ledWord DOT3[]        = { {0,0},
                          {7,8+4+2}};; // value includes previous dots
ledWord DOT4[]        = { {0,0},
                          {7,8+4+2+1}};; // value includes previous dots

/* 
 * Below shows the layout of LEDs in my array:
 *      
 * DIG0 0 I T   I S T E N  0         H A L F 
 * DIG1 1   Q U A R T E R  1     T W E N T Y  
 * DIG2 2 F I V E H A P P  2 Y       P A S T  
 * DIG3 3   B I R T H D A  3 Y   T O   O N E
 * DIG4 4 T W O T H R E E  4 F O U R F I V E  
 * DIG5 5 S I X       S E  5 V E N E I G H T  
 * DIG6 6 N I N E   T E N  6     E L E V E N
 * DIG7 7 T W E L V E   N  7 I C O   * * * *  
 *        0 1 2 3 4 5 6 7    0 1 2 3 4 5 6 7
 *        P A B C D E F G    P A B C D E F G 
 */

 /*
  * Cheatsheet for values if each column is "ON" in a row:
  *  0   1   2   3   4   5   6   7
  *  128 64  32  16  8   4   2   1
  */

int lc0RVals[8]; // will hold row setting variables for ledControl 0
int lc1RVals[8]; // will hold row setting variables for ledControl 1
          
/* 
 * Create a new LedControl object named wordMatrix
 * We use pins 12,11 and 10 for the SPI interface
 * With our hardware we have connected pin 12 to the DATA IN-pin (1) of the first MAX7221
 * pin 11 is connected to the CLK-pin(13) of the first and second MAX7221
 * pin 10 is connected to the LOAD-pin(12) of the first and second MAX7221    
 * We will have two MAX7221s attached to the arduino 
 */
LedControl wordMatrix=LedControl(12,11,10,2); 

/*
 * Setup initial state for our Arduino
 */
void setup() {
  // turn on the displays
  wordMatrix.shutdown(0,false);
  wordMatrix.shutdown(1,false);

  // set both of the LED matrices to moderate brightness (intensity = 0..15)
  wordMatrix.setIntensity(0,8);
  wordMatrix.setIntensity(1,8);
  
  wordMatrix.clearDisplay(0);
  wordMatrix.clearDisplay(1);

  


  pinMode(minUpPin, INPUT);
  pinMode(minDownPin, INPUT);

  // activate internal pull-up resistors
  digitalWrite(minUpPin, HIGH);
  digitalWrite(minDownPin, HIGH);
}


// each cycle we will go through 16 rows and set the LEDs to the proper value
// using LEDControl library, this is done by setting LED Row to the proper value (adding powers of 2 to end up with a single byte value)
// to reduce flicker and stray lit LEDs, we will set the row values all at once at the end of our script
void loop() {
  
  // create a variable to hold the current time
  tmElements_t tm;
  int hr;
  int mn;

  ////////////////////////////////////////////
  // read the current time from our DS3231 Real Time Clock
  RTC.read(tm);

  mn = tm.Minute;
  hr = (tm.Hour >= 12) ?  tm.Hour - 12 :  tm.Hour; // convert hours to 12 hour time, zero-based

  int upButtonReading = digitalRead(minUpPin);
  int downButtonReading = digitalRead(minDownPin);  

  ////////////////////////////////////////////
  // Check the button states and increase/decrease times
  // first check the minute UP button
  if(upButtonReading != lastMinUpState) {
    lastUpDebounceTime = millis();
  }
  if (millis() - lastUpDebounceTime > debounceDelay) {
    if (upButtonReading != lastMinUpState) {
      lastMinUpState = upButtonReading;
      if (lastMinUpState == LOW) {
        minInc(tm); 
      }
    } 
  }
  if(downButtonReading != lastMinDownState) {
    lastDownDebounceTime = millis();
  }
  // next check the minute DOWN button
  if (millis() - lastDownDebounceTime > debounceDelay) {
    if (downButtonReading != lastMinDownState) {
      lastMinDownState = downButtonReading;
      if (lastMinDownState == LOW) {
        minDec(tm); 
      }
    } 
  }
 
  // zero out the display array variables so we have no stray LEDs lit
  for (int i=0;i<8;i++) {
    lc0RVals[i]=0;
    lc1RVals[i]=0;
  }

  // Light up IT IS
  lc0RVals[ITIS[0].row] += ITIS[0].colsValue;
  lc1RVals[ITIS[1].row] += ITIS[1].colsValue;

  // Light up NAME
  lc0RVals[NAME[0].row] += NAME[0].colsValue;
  lc1RVals[NAME[1].row] += NAME[1].colsValue;

  // Set the status of the 4 extra "dots"
  lightMinuteDots(mn % 5);

  // Set the remaining minute LEDs
  lightMinuteWords(mn);

  // Set the Hour LEDs
  lightHourWords(hr);
  
  // set the birthday LEDs
  if (isBirthday) {
    lc0RVals[HAPPY[0].row] += HAPPY[0].colsValue;
    lc1RVals[HAPPY[1].row] += HAPPY[1].colsValue;

    lc0RVals[BIRTHDAY[0].row] += BIRTHDAY[0].colsValue;
    lc1RVals[BIRTHDAY[1].row] += BIRTHDAY[1].colsValue;
  }

  // copy the row values from our lcRVals arrays to the LedControls
  for (int j=0;j<8;j++) {
    wordMatrix.setRow(0,j,lc0RVals[j]);
    wordMatrix.setRow(1,j,lc1RVals[j]);
  }

}

bool isBirthday(tmElements_t tm) {
  return (tm.Month == birthMonth) && (tm.Day == birthDay);
}

/*
 * light up 0-4 dots, depending on how many minutes we are "over" the 5-minute accuracy we get using just the words LEDs
 * remainder must be a number < 5,  > 0
 */
void lightMinuteDots(int remainder) {
  if (remainder >= 4) {
    lc0RVals[DOT4[0].row]+=DOT4[0].colsValue;
    lc1RVals[DOT4[1].row]+=DOT4[1].colsValue;
  } else if (remainder == 3) {
    lc0RVals[DOT3[0].row]+=DOT3[0].colsValue;
    lc1RVals[DOT3[1].row]+=DOT3[1].colsValue;
  } else if (remainder == 2) {
    lc0RVals[DOT2[0].row]+=DOT2[0].colsValue;
    lc1RVals[DOT2[1].row]+=DOT2[1].colsValue;
  } else if (remainder == 1) {
    lc0RVals[DOT1[0].row]+=DOT1[0].colsValue;
    lc1RVals[DOT1[1].row]+=DOT1[1].colsValue;
  }
}

/*
 * Light up the right words for the current minutes
 */
void lightMinuteWords(int mn) {
  if (mn < 5) {
    // nothing to do  
  } else if (mn < 10) {
    // IT IS FIVE PAST ONE
    //FIVE
    lc0RVals[FIVE[0].row]+=FIVE[0].colsValue;
    lc1RVals[FIVE[1].row]+=FIVE[1].colsValue;
    //PAST
    lc0RVals[PAST[0].row]+=PAST[0].colsValue;
    lc1RVals[PAST[1].row]+=PAST[1].colsValue;
  } else if (mn < 15) {
    // IT IS TEN PAST ONE    
    lc0RVals[TEN[0].row]+=TEN[0].colsValue;
    lc1RVals[TEN[1].row]+=TEN[1].colsValue;
    //PAST
    lc0RVals[PAST[0].row]+=PAST[0].colsValue;
    lc1RVals[PAST[1].row]+=PAST[1].colsValue;
   
  } else if (mn < 20) {
    // IT IS QUARTER PAST ONE
    lc0RVals[QUARTER[0].row]+=QUARTER[0].colsValue;
    lc1RVals[QUARTER[1].row]+=QUARTER[1].colsValue;
    //PAST
    lc0RVals[PAST[0].row]+=PAST[0].colsValue;
    lc1RVals[PAST[1].row]+=PAST[1].colsValue;    
  } else if (mn < 25) {
    // IT IS TWENTY PAST ONE
    lc0RVals[TWENTY[0].row]+=TWENTY[0].colsValue;
    lc1RVals[TWENTY[1].row]+=TWENTY[1].colsValue;
    //FIVE
    lc0RVals[FIVE[0].row]+=FIVE[0].colsValue;
    lc1RVals[FIVE[1].row]+=FIVE[1].colsValue;
    //PAST
    lc0RVals[PAST[0].row]+=PAST[0].colsValue;
    lc1RVals[PAST[1].row]+=PAST[1].colsValue;    
    
  } else if (mn < 35 ) {
    // IT IS HALF PAST ONE
    lc0RVals[HALF[0].row]+=HALF[0].colsValue;
    lc1RVals[HALF[1].row]+=HALF[1].colsValue;
    //PAST
    lc0RVals[PAST[0].row]+=PAST[0].colsValue;
    lc1RVals[PAST[1].row]+=PAST[1].colsValue;        
  } else if (mn < 40) {
    // IT IS TWENTY FIVE TO TWO
    lc0RVals[TWENTY[0].row]+=TWENTY[0].colsValue;
    lc1RVals[TWENTY[1].row]+=TWENTY[1].colsValue;
    //FIVE
    lc0RVals[FIVE[0].row]+=FIVE[0].colsValue;
    lc1RVals[FIVE[1].row]+=FIVE[1].colsValue;
    //PAST
    lc0RVals[TO[0].row]+=TO[0].colsValue;
    lc1RVals[TO[1].row]+=TO[1].colsValue;        
    
  } else if (mn < 45) {
    // IT IS TWENTY TO TWO
    lc0RVals[TWENTY[0].row]+=TWENTY[0].colsValue;
    lc1RVals[TWENTY[1].row]+=TWENTY[1].colsValue;
    //PAST
    lc0RVals[TO[0].row]+=TO[0].colsValue;
    lc1RVals[TO[1].row]+=TO[1].colsValue;        
    
  } else if (mn < 50) {
    // IT IS QUARTER TO TWO
    lc0RVals[QUARTER[0].row]+=QUARTER[0].colsValue;
    lc1RVals[QUARTER[1].row]+=QUARTER[1].colsValue;
    //PAST
    lc0RVals[TO[0].row]+=TO[0].colsValue;
    lc1RVals[TO[1].row]+=TO[1].colsValue;        
  
  } else if (mn < 55) {
    // IT IS TEN TO TWO
    lc0RVals[TEN[0].row]+=TEN[0].colsValue;
    lc1RVals[TEN[1].row]+=TEN[1].colsValue;
    //PAST
    lc0RVals[TO[0].row]+=TO[0].colsValue;
    lc1RVals[TO[1].row]+=TO[1].colsValue;        
  
  } else if (mn < 60) {
    // IT IS FIVE TO TWO
    lc0RVals[FIVE[0].row]+=FIVE[0].colsValue;
    lc1RVals[FIVE[1].row]+=FIVE[1].colsValue;
    //PAST
    lc0RVals[TO[0].row]+=TO[0].colsValue;
    lc1RVals[TO[1].row]+=TO[1].colsValue;        
  }
}

/*
 * Light up the right words for the current hour, in 12 hour format
 */
void lightHourWords(int hr) {
  switch (hr) {
    case 11: 
    lc0RVals[ELEVENOCLOCK[0].row]+=ELEVENOCLOCK[0].colsValue;
    lc1RVals[ELEVENOCLOCK[1].row]+=ELEVENOCLOCK[1].colsValue;    
    break;
    case 10: 
    lc0RVals[TENOCLOCK[0].row]+=TENOCLOCK[0].colsValue;
    lc1RVals[TENOCLOCK[1].row]+=TENOCLOCK[1].colsValue;    
    break;
    case 9: 
    lc0RVals[NINEOCLOCK[0].row]+=NINEOCLOCK[0].colsValue;
    lc1RVals[NINEOCLOCK[1].row]+=NINEOCLOCK[1].colsValue;    
    break;
    case 8: 
    lc0RVals[EIGHTOCLOCK[0].row]+=EIGHTOCLOCK[0].colsValue;
    lc1RVals[EIGHTOCLOCK[1].row]+=EIGHTOCLOCK[1].colsValue;    
    break;
    case 7: 
    lc0RVals[SEVENOCLOCK[0].row]+=SEVENOCLOCK[0].colsValue;
    lc1RVals[SEVENOCLOCK[1].row]+=SEVENOCLOCK[1].colsValue;    
    break;
    case 6: 
    lc0RVals[SIXOCLOCK[0].row]+=SIXOCLOCK[0].colsValue;
    lc1RVals[SIXOCLOCK[1].row]+=SIXOCLOCK[1].colsValue;    
    break;
    case 5: 
    lc0RVals[FIVEOCLOCK[0].row]+=FIVEOCLOCK[0].colsValue;
    lc1RVals[FIVEOCLOCK[1].row]+=FIVEOCLOCK[1].colsValue;    
    break;
    case 4: 
    lc0RVals[FOUROCLOCK[0].row]+=FOUROCLOCK[0].colsValue;
    lc1RVals[FOUROCLOCK[1].row]+=FOUROCLOCK[1].colsValue;    
    break;
    case 3: 
    lc0RVals[THREEOCLOCK[0].row]+=THREEOCLOCK[0].colsValue;
    lc1RVals[THREEOCLOCK[1].row]+=THREEOCLOCK[1].colsValue;    
    break;
    case 2: 
    lc0RVals[TWOOCLOCK[0].row]+=TWOOCLOCK[0].colsValue;
    lc1RVals[TWOOCLOCK[1].row]+=TWOOCLOCK[1].colsValue;    
    break;
    case 1: 
    lc0RVals[ONEOCLOCK[0].row]+=ONEOCLOCK[0].colsValue;
    lc1RVals[ONEOCLOCK[1].row]+=ONEOCLOCK[1].colsValue;    
    break;
    case 0:
    lc0RVals[TWELVEOCLOCK[0].row]+=TWELVEOCLOCK[0].colsValue;
    lc1RVals[TWELVEOCLOCK[1].row]+=TWELVEOCLOCK[1].colsValue;    
    break;
  }
}

/*
 * Adjust the time on the RTC up by one minute
 */
void minInc(tmElements_t tm) {
   //check for the cases when we are at 0 minutes, or 0 hours, prior to decrementing.
   // we won't handle changing the date
   if (tm.Minute == 0) {
     if (tm.Hour == 0) {
      tm.Day--;
      tm.Hour = 23;
      tm.Minute = 59;
     } else {
       tm.Hour--;
       tm.Minute = 59;
     }
   } else {
     tm.Minute--;    
   }
   
   RTC.write(tm);
}

/*
 * Adjust the time on the RTC down by one minute
 */
void minDec(tmElements_t tm) { 
  if (tm.Minute == 59) {
      if (tm.Hour == 23) {
        tm.Day++;
        tm.Hour = 0;
        tm.Minute = 0;
      } else {
        tm.Hour++;
        tm.Minute = 0;
      } 
      
  } else {
        tm.Minute++;
  }
    
    RTC.write(tm);
}
