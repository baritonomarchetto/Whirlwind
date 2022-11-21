/* 
 PC-to-Jamma Interface based on Raspberry Pi Pico - shield
 Controls section (JOYSTICK) and sync frequency check and block (>15KHz by default, but 
 customizable for higher frequencies (i.e. 25KHz or 31KHz).
 
 Tested with Earle F. Philhower board package and Benjamin Aigner joystick library.

 by Barito 2022 (last update sept 2022)
*/

#include <Joystick.h>

#define INPUTS 26

//comment the following line to disable the sync frequency check and block (necessary for debugging other functions!!)
#define SYNC_MONITOR_ACTIVE

const int HSyncPin = 0;
const int disablePin = 1;
const int ledPin = 25;

int fq;
const int delayTime = 20;
boolean startBlock = 0;
boolean enableState;
boolean prevEnState;

//31KHz  -> 32 us
//25KHz -> 40 us
//15KHz -> 66 us
int periodSum;
int periodIst;
int periodAWG;
int sCounter;
const int samples = 100;

struct digitalInput {const byte pin; boolean state; unsigned long dbTime; const byte btn; const byte btn_shift;} 
digitalInput[INPUTS] = {
{6, HIGH, 0, 1, 1},  //P1 START - SHIFT BUTTON
{7, HIGH, 0, 2, 27}, //P2 START
{8, HIGH, 0, 3, 28}, //P1 UP
{10, HIGH, 0, 4, 29}, //P1 DWN
{16, HIGH, 0, 5, 30}, //P1 LEFT
{14, HIGH, 0, 6, 31}, //P1 RIGHT
{12, HIGH, 0, 7, 32},  //P1 B1
{20, HIGH, 0, 8, 8},  //P1 B2
{18, HIGH, 0, 9, 9},  //P1 B3
{22, HIGH, 0, 10, 10},  //P1 B4
{24, HIGH, 0, 11, 101},//P1 B5
{27, HIGH, 0, 12, 12},//P1 B6
{9, HIGH, 0, 13, 13},//P2 UP
{11, HIGH, 0, 14, 14},//P2 DWN
{17, HIGH, 0, 15, 15},//P2 LEFT
{15, HIGH, 0, 16, 16},//P2 RIGHT
{13, HIGH, 0, 17, 17},//P2 B1
{21, HIGH, 0, 18, 18},//P2 B2
{19, HIGH, 0, 19, 19},//P2 B3
{23, HIGH, 0, 20, 20},//P2 B4
{26, HIGH, 0, 21, 21},//P2 B5
{28, HIGH, 0, 22, 22}, //P2 B6
{4, HIGH, 0, 23, 23},//P1 COIN
{5, HIGH, 0, 24, 24},//P2 COIN
{3, HIGH, 0, 25, 25},//SERVICE SW
{2, HIGH, 0, 26, 26},//TEST SW
};

void setup(){  
for (int j = 0; j < INPUTS; j++){
  pinMode(digitalInput[j].pin, INPUT_PULLUP);
  digitalInput[j].state = digitalRead(digitalInput[j].pin);
  digitalInput[j].dbTime = millis();
}
pinMode(HSyncPin, INPUT_PULLUP);
pinMode(ledPin, OUTPUT);
pinMode(disablePin, OUTPUT);
#ifdef SYNC_MONITOR_ACTIVE
  digitalWrite(disablePin, HIGH); //DISABLE
  digitalWrite(ledPin, LOW);
#else
  digitalWrite(disablePin, LOW); //ENABLE
  digitalWrite(ledPin, HIGH);
#endif
//fq = 28; //25KHZ
//fq = 11; //31KHZ
fq = 55; //15KHZ - default
Joystick.begin();
//Serial.begin(9600);//DEBUG
} // setup close

void loop(){
  shiftInputs();
  generalInputs();
  #ifdef SYNC_MONITOR_ACTIVE
    freqBlock();
  #endif
}

void generalInputs(){
//general input handling
//ON STATE CHANGE
  for (int j = 1; j < INPUTS; j++){
   if (millis()-digitalInput[j].dbTime > delayTime && digitalRead(digitalInput[j].pin) !=  digitalInput[j].state){
      digitalInput[j].dbTime = millis();
      digitalInput[j].state = digitalRead(digitalInput[j].pin);
      if(digitalInput[0].state == HIGH){ //shift button not pressed
         Joystick.button(digitalInput[j].btn, !digitalInput[j].state);
         delay(8);//(as per library example) we need a short delay here, sending packets with less than 1ms (8ms I would say...) leads to packet loss!
      }
      else{
         startBlock = 1;
         Joystick.button(digitalInput[j].btn_shift, !digitalInput[j].state);
         delay(8);//(as per library example) we need a short delay here, sending packets with less than 1ms leads to packet loss!
      }
    }
  }
}

void shiftInputs(){
//reversed input handling (P1 START) - shift button
//ON STATE CHANGE
  if (millis()-digitalInput[0].dbTime > delayTime && digitalRead(digitalInput[0].pin) !=  digitalInput[0].state){
    digitalInput[0].dbTime = millis();
    digitalInput[0].state = digitalRead(digitalInput[0].pin);
    if (digitalInput[0].state == HIGH){
      if(startBlock == 0){
        Joystick.button(digitalInput[0].btn, true);
        delay(100);
        Joystick.button(digitalInput[0].btn, false);
        delay(8);//(as per library example) we need a short delay here, sending packets with less than 1ms leads to packet loss!
      }
      else{
        startBlock = 0;
      }
    }
  }
}

//31KHz  -> 32 us
//25KHz -> 40 us
//15KHz -> 66 us
void freqBlock(){
  periodIst = pulseIn(HSyncPin, HIGH);//time (in us) between a high and low pulse (Hsync: negtive sync, 5% duty cicle). pulseIn holds the code, so this function will alter the whole sketch behaviour if no pulse incomes or a timeout is defined. Timeout brakes the function (don't ask...) so it's not defined. Keep this in mind while debuggin!!
  periodSum = periodSum + periodIst;
  sCounter++;
  if(sCounter > samples){
    periodAWG = (periodSum/sCounter);
    //Serial.println(periodAWG);//DEBUG
    periodSum = 0;
    sCounter = 0;
    if(periodAWG > fq){
      enableState = 1;
    }
    else {
      enableState = 0;
    }
    if (enableState != prevEnState){//take action on state change only
      prevEnState = enableState;
      digitalWrite(disablePin, !enableState);
      digitalWrite(ledPin, enableState);
    }
  }
}
