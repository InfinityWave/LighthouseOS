
#include <Arduino.h>
#include <SPI.h>
#include <StateMachine.h>
#include <TimeLib.h>      //https://github.com/PaulStoffregen/Time.git
#include <DS3232RTC.h>    //https://github.com/H4K5M6/DS3232RTC.git
#include <DCF77.h>        //https://github.com/thijse/Arduino-DCF77.git
#include <Adafruit_FRAM_I2C.h> //from lib manager v2.0
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>// Controller specific library
#include <CheapStepper.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include "Adresses.h"

//DS3231 pins
#define DS3231_INT 3      // interrupt

//DCF77 pins
#define DCF_PIN 2         // Connection pin to DCF 77 device, interrupt
#define DCF_EN_PIN 4      // Enable DCF77 module active low

//TFT pins
//use Hardware SPI NANO: SCK=13, MISO=12, MOSI=11 
#define TFT_DC 9
#define TFT_CS 8
#define TFT_LITE 10 //PWM pin
//#define TFT_RESET 

//Button pins
#define BTN_C 5           //interrupt
#define BTN_L 7           //interrupt
#define BTN_R 6           //interrupt

//LED pins
#define LED_BTN_C 
#define LED_BTN_R
#define LED_BTN_L
#define LED_MAIN          //PWM pin

// stepper pins
#define STEP_IN1
#define STEP_IN2
#define STEP_IN3
#define STEP_IN4

//Parameters
#define BKGND ILI9341_BLACK // background color
#define TXT ILI9341_WHITE // text color
#define MMEL 9    // number of menu entries

#define LITE 200  // standard brightness

#define DCF_INT 6     // interval of dcf sync in hours
#define DCF_HOUR 5    // hour to start dcf sync
#define DCF_MIN 0     // minute to start dcf sync
#define DCF_LEN 30    // length in minutes of dcf sync attempt


// initializations
StateMachine machine = StateMachine();

State* S0 = machine.addState(&stateClockDisplay);
State* S1 = machine.addState(&stateStandby);
State* S2 = machine.addState(&stateMainMenu);
State* S3 = machine.addState(&stateWeddingMode);
State* S4 = machine.addState(&stateClockMenu);
State* S5 = machine.addState(&stateAlarm1Menu);
State* S6 = machine.addState(&stateAlarm2Menu);
State* S7 = machine.addState(&stateSoundMenu);
State* S8 = machine.addState(&stateLighthouseMenu);
State* S9 = machine.addState(&stateBrightnessMenu);
State* S10 = machine.addState(&stateHomingMenu);
State* S11 = machine.addState(&stateCreditsMenu);

DS3232RTC myRTC(false);

DCF77 DCF = DCF77(DCF_PIN, digitalPinToInterrupt(DCF_PIN), LOW);

Adafruit_FRAM_I2C fram = Adafruit_FRAM_I2C();

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC/*, TFT_RESET*/);
//GFXcanvas1 canvas(280, 54); // 128x32 pixel canvas
GFXcanvas1 canvasClock(76, 54); // Canvas for Clock Numbers
GFXcanvas1 canvasDate(232, 38); // Canvas for Date
GFXcanvas1 canvasDay(242, 50); // Canvas for Date

int16_t  x, y;
uint16_t w, h;

volatile time_t isrTime;
volatile bool isrTimeChange;
bool isrTimeUpdate;
char timeString[3];
char dateString[11];
bool dcfSync = false;
bool dcfSyncSucc = false;

uint8_t clockHour, clockMinute, clockSecond, clockDay;

// button flags
volatile bool isrbuttonC = false;
volatile bool isrButtonL = false;
volatile bool isrButtonR = false;

// menu entries
const char *mainMenuEntries[MMEL] = {"Clock", "Alarm1", "Alarm2", "Sound", "Light", "Brightness", "Homing", "Back", "Credits"};

//menu variables
int8_t selectedMMItem = 0;
int8_t selectedSubMItem = -1;
int8_t currentMenu = -1;
uint32_t sleepDelay = 60000;
uint32_t sleepTimer = 0;
bool updateScreen = false;
bool changeValue = false;

//display variable
uint8_t brightness = LITE;

//alarms
struct alarms {
    uint8_t hh;
    uint8_t mm;
    uint8_t dd;
    bool act;
    bool light;
    uint8_t soundfile;
} alm1, alm2;

bool weddingModeFinished = false;

void setup(void) {
    Serial.begin(9600);
    
    // setup TFT Backlight as off
    pinMode(TFT_LITE, OUTPUT);
    analogWrite(TFT_LITE, 0);

    // DCF setup
    digitalWrite(DCF_EN_PIN, HIGH);
    pinMode(DCF_EN_PIN, OUTPUT);

    // setup state machine
    // from clockDisplay
    S0->addTransition(&toSleep,S1);         // to standby
    S0->addTransition(&openMainMenu,S2);    // to main menu
    S0->addTransition(&changeToWeddingMode,S3); //start wedding mode
    S0->addTransition(&attachUnhandledInterrupts,S0);   //Must be last item
    // from standby
    S1->addTransition(&wakeup,S0);          // to clock
    // from main menu
    S2->addTransition(&closeMainMenu,S0);   // to clock
    S2->addTransition(&toSleep,S1);         // to standby
    S2->addTransition(&changeMenuSelection,S2); // stay and change selection
    S2->addTransition(&openClockMenu,S4);
    S2->addTransition(&openAlarm1Menu,S5);
    S2->addTransition(&openAlarm2Menu,S6);
    S2->addTransition(&openSoundMenu,S7);
    S2->addTransition(&openLighthouseMenu,S8);
    S2->addTransition(&openBrigthnessMenu,S9);
    S2->addTransition(&openHomingMenu,S10);
    S2->addTransition(&openCreditsMenu,S11);
    //S2->addTransition(&attachUnhandledInterrupts,S2);   //Must be last item
    // from wedding mode
    S3->addTransition(&exitWeddingMode,S0);
    //Return from all SubMenus
    S4->addTransition(&returnToMainMenu,S2);
    S5->addTransition(&returnToMainMenu,S2);
    S6->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&returnToMainMenu,S2);
    S8->addTransition(&returnToMainMenu,S2);
    S9->addTransition(&returnToMainMenu,S2);
    S10->addTransition(&returnToMainMenu,S2);
    S11->addTransition(&returnToMainMenu,S2);
    S11->addTransition(&attachUnhandledInterrupts,S11);
    // TFT setup
    delay(500);
    tft.begin();
    tft.setTextColor(TXT);
    tft.setTextSize(2);
    tft.setRotation(3);
    tft.fillScreen(BKGND);
    tft.setFont(&FreeSans18pt7b);
    tft.setCursor(105,88);
    tft.print(":");
//    canvas.setFont(&FreeSans18pt7b);
//    canvas.setTextSize(2);
    canvasClock.setFont(&FreeSans18pt7b);
    canvasClock.setTextSize(2);
    canvasDate.setFont(&FreeSans12pt7b);
    canvasDate.setTextSize(2);

    brightness = LITE; //set brightness
    
    // RTC startup
    pinMode(DS3231_INT, INPUT);
    myRTC.begin();
    myRTC.squareWave(SQWAVE_NONE);
    myRTC.setAlarm(ALM2_EVERY_MINUTE,0,0,0,0);
    myRTC.alarmInterrupt(ALARM_1, false);
    myRTC.alarmInterrupt(ALARM_2, true);
    attachInterrupt(digitalPinToInterrupt(DS3231_INT), isrChangeTime, FALLING);
    isrTimeChange = true;
    if(isrTime > 0)
        Serial.println("RTC has set the system time");
    else
        Serial.println("Unable to sync with the RTC");
        
    prepareClock();

    // initialize FRAM
    if (fram.begin())// you can stick the new i2c addr in here, e.g. begin(0x51);
        Serial.println("Found I2C FRAM");
    else
        Serial.println("I2C FRAM not identified");

    // space for variable inits
    readAlarms(ALARM1, alm1);
    readAlarms(ALARM2, alm2);
    
    /*tft.setFont(&FreeSans12pt7b);
    /*for (int i = 0; i < 100; i = i+11){
        sprintf(timeString, "%02d", i);
        tft.getTextBounds("Sonntag", 20, 140, &x, &y, &w, &h);
        Serial.print("Sonntag");
        Serial.print(" ");
        Serial.print(x);
        Serial.print(" ");
        Serial.print(y);
        Serial.print(" ");
        Serial.print(w);
        Serial.print(" ");
        Serial.println(h);
    /*}*/

   /* tft.drawBitmap(121, 138, canvasClock.getBuffer(), 76, 54, ILI9341_WHITE, ILI9341_RED);
    tft.drawBitmap(211, 138, canvasClock.getBuffer(), 76, 54, ILI9341_WHITE, ILI9341_RED);
    /*tft.fillRect(107,138,1,40,ILI9341_WHITE);
    tft.fillRect(108,138,1,40,ILI9341_RED);
    tft.fillRect(109,138,1,40,ILI9341_WHITE);
    tft.fillRect(110,138,1,40,ILI9341_RED);
    tft.fillRect(111,138,1,40,ILI9341_WHITE);
    tft.fillRect(116,138,1,40,ILI9341_WHITE);
    tft.fillRect(117,138,1,40,ILI9341_RED);
    tft.fillRect(118,138,1,40,ILI9341_WHITE);
    tft.fillRect(119,138,1,40,ILI9341_RED);
    tft.fillRect(120,138,1,40,ILI9341_BLACK);*/
    // Buttons interrupt setup
    pinMode(BTN_C, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
    pinMode(BTN_R, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
    pinMode(BTN_L, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
}

void loop()
{
    if (isrTimeChange) {
        updateClock();
    }
    if (checkAlarms()) {
        // refine this
        Serial.println("ALAAAARM!!!");
    }
    if (!dcfSync && hour(isrTime)%DCF_INT == DCF_HOUR && minute(isrTime) == DCF_MIN) {
        DCF.Start();
        digitalWrite(DCF_EN_PIN, LOW);
    } else if (dcfSync && hour(isrTime)%DCF_INT == DCF_HOUR && minute(isrTime) == DCF_MIN+DCF_LEN) {
        DCF.Stop();
        digitalWrite(DCF_EN_PIN, HIGH);
        dcfSyncSucc = false;
    } else if (dcfSync && DCF.getTime() != 0) {
        DCF.Stop();
        myRTC.set(DCF.getTime());
        digitalWrite(DCF_EN_PIN, HIGH);
        dcfSyncSucc = true;
    }
    machine.run();
}

// main clock display
void stateClockDisplay()
{
    if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay();
        analogWrite(TFT_LITE, brightness);
    }
}

// standby state
void stateStandby()
{
    //nothing yet maybe send arduino to sleep
    if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay();
    }
}

// main menu
void stateMainMenu()
{
    if (updateScreen) {
        updateScreen = false;
    tft.setCursor(100,40);
    tft.setFont(&FreeSans12pt7b);
    tft.println("Menu");
    tft.drawFastHLine(0,50,320,TXT);
    tft.fillCircle(15,145,7,TXT);
    tft.setCursor(40,100);
    tft.println(mainMenuEntries[(selectedMMItem+MMEL-1)%MMEL]);
    tft.setCursor(40,160);
    tft.println(mainMenuEntries[selectedMMItem]);
    tft.setCursor(40,220);
    tft.println(mainMenuEntries[(selectedMMItem+1)%MMEL]);
    analogWrite(TFT_LITE, brightness);
    }
}

//S3
void stateWeddingMode()
{
    //Switch Music On
    //Switch Motor On
    //Switch Main Light On
    //Blink auxiliary lights
    //Display something
    weddingModeFinished = true;
}

//S4
void stateClockMenu()
{
    //Manually set time
    //Set Light Interval
    //Set Motor Interval
}

//S5
void stateAlarm1Menu()
{
    //Set Alarm1
    //Select Alarm Sound
    //Select if lights should be on
}

//S6
void stateAlarm2Menu()
{
    //Set Alarm2
    //Select Alarm Sound
    //Select if lights should be on
}

//S7
void stateSoundMenu()
{
    //Set Volume

}

//S8
void stateLighthouseMenu()
{
    //Lighthouse Settings for normal clock mode
    //Motor Settings
    //Main Light Settings
}

//S9
void stateBrightnessMenu()
{
    //Brightness Setting for Display
    //MainLED?
}

//S10
void stateHomingMenu()
{
    //Motor Homing Procedure
}

//S11
void stateCreditsMenu()
{
    //Display Credits on Lighthouse OS
    //Turn on Main Lights and Motor
}

bool openMainMenu()
{
    if (isrbuttonC) {
      isrbuttonC = false;
      sleepTimer = millis();
      analogWrite(TFT_LITE, 0);
      tft.fillScreen(BKGND);
      currentMenu = MMEL;
      selectedMMItem = 0;
      updateScreen = true;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      return true;
    }
    return false;
}

bool toSleep()
{
    if (millis() > sleepTimer + sleepDelay) {
        analogWrite(TFT_LITE, 0);
        currentMenu = -1;
        return true;
    }
    return false;
}

bool wakeup()
{
    if (isrbuttonC||isrButtonR||isrButtonL) {
      isrbuttonC = false;
      isrButtonR = false;
      isrButtonL = false;
      sleepTimer = millis();
      currentMenu = -1;
      isrTimeUpdate = true;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
      return true;
    }
    return false;
}

bool changeMenuSelection()
{
    int noItems = 0;
    switch (currentMenu) {
      case MMEL: noItems = MMEL;
    }
    if(isrButtonR){
        isrButtonR = false;
        analogWrite(TFT_LITE, 0);
        tft.fillScreen(BKGND);
        selectedMMItem = (selectedMMItem+1)%noItems;
        updateScreen = true;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        return true;
    }
    if(isrButtonL){
        isrButtonL = false;
        analogWrite(TFT_LITE, 0);
        tft.fillScreen(BKGND);
        selectedMMItem = (selectedMMItem+noItems-1)%noItems;
        updateScreen = true;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        return true;
    }
    return false;
}

bool returnToMainMenu()
{
    if (isrbuttonC  && (selectedSubMItem == 0)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = MMEL;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openClockMenu()
{
    if (isrbuttonC  && (selectedMMItem == 0)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openAlarm1Menu()
{
    if (isrbuttonC  && (selectedMMItem == 1)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openAlarm2Menu()
{
    if (isrbuttonC  && (selectedMMItem == 2)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openSoundMenu()
{
    if (isrbuttonC  && (selectedMMItem == 3)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openLighthouseMenu()
{
    if (isrbuttonC  && (selectedMMItem == 4)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openBrigthnessMenu()
{
    if (isrbuttonC  && (selectedMMItem == 5)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool openHomingMenu()
{
    if (isrbuttonC  && (selectedMMItem == 6)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool closeMainMenu()
{
    if (isrbuttonC) {
      if (selectedMMItem == 7) {
          sleepTimer = millis();
          isrbuttonC = false;
          analogWrite(TFT_LITE, 0);
          tft.fillScreen(BKGND);
          prepareClock();
          currentMenu = -1;
          isrTimeChange = true; // to enable clock display
          delay(50);
          attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
          return true;
          }
    }
    return false;
}

bool openCreditsMenu()
{
    if (isrbuttonC  && (selectedMMItem == 8)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool changeToWeddingMode()
{
    return false;
}

bool exitWeddingMode()
{
    if(weddingModeFinished){
        weddingModeFinished = false;
        return true;
    }
    return false;
}

bool attachUnhandledInterrupts()
{
    if(isrButtonR){
        isrButtonR = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
    }
    if(isrButtonL){
        isrButtonL = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
    }
    if(isrButtonR){
        isrButtonR = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
    }
    return true;
}

// reset internal clock variables
void prepareClock()
{
    clockHour = 255;
    clockMinute = 255;
    clockSecond = 255;
    clockDay = 255;
    tft.setFont(&FreeSans18pt7b);
    tft.setCursor(105,88);
    tft.print(":");
    /*tft.setCursor(195,88);
    tft.print(":");*/
}

void updateClock()
{
    isrTimeChange = false;
    isrTimeUpdate = true;
    isrTime = myRTC.get();
    myRTC.alarm(ALARM_2);
}

void clockDisplay()
{
    
        if (clockHour != hour(isrTime)){
            clockHour = hour(isrTime);
            sprintf(timeString,"%02d",clockHour);
            canvasClock.fillScreen(BKGND);
            canvasClock.setCursor(0, 50);
            canvasClock.println(timeString);
            tft.drawBitmap(31, 38, canvasClock.getBuffer(), 76, 54, TXT, BKGND);
        }
        if (clockMinute != minute(isrTime)){
            clockMinute = minute(isrTime);
            sprintf(timeString,"%02d",clockMinute);
            canvasClock.fillScreen(BKGND);
            canvasClock.setCursor(0, 50);
            canvasClock.println(timeString);
            tft.drawBitmap(121, 38, canvasClock.getBuffer(), 76, 54, TXT, BKGND);
        }
        /*if (clockSecond != second(isrTime)){
            clockSecond = second(isrTime);
            sprintf(timeString,"%02d",clockSecond);
            canvasClock.fillScreen(ILI9341_BLACK);
            canvasClock.setCursor(0, 50);
            canvasClock.println(timeString);
            tft.drawBitmap(211, 38, canvasClock.getBuffer(), 76, 54, ILI9341_WHITE, ILI9341_RED);
        }*/
        if (clockDay != day(isrTime)){
            clockDay = day(isrTime);
            sprintf(dateString,"%02d.%02d.%04d", clockDay, month(isrTime), year(isrTime));
            canvasDate.fillScreen(BKGND);
            canvasDate.setCursor(0, 34);
            canvasDate.println(dateString);
            tft.drawBitmap(44, 138, canvasDate.getBuffer(), 232, 38, TXT, BKGND);
        }
}

// interrupt functions

void isrChangeTime()
{
    isrTimeChange = true;
}

// button interrupt flags

void buttonC()
{
    detachInterrupt(digitalPinToInterrupt(BTN_C));
    isrbuttonC = true;
}

void buttonR()
{
    detachInterrupt(digitalPinToInterrupt(BTN_R));
    isrButtonR = true;
}

void buttonL()
{
    detachInterrupt(digitalPinToInterrupt(BTN_L));
    isrButtonL = true;
}

// alarm functions

void setAlarmMenu (uint16_t address)
{
    // make alarm menu screen
    // hh:dd
    // Mo Di Mi Do Fr Sa So
    // w/ some kind of selection for weekday
    // Light y/n
    // Sound
}

/*bool setAlarms ()
{
    
    if (isrbuttonC) {
      isrbuttonC = false;
      sleepTimer = millis();
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      return true;
    }
    if (changeValue) {
        switch case
    } else {
        
    }
    return false;
}*/

bool checkAlarms () 
{
    if (alm1.act && alm1.hh == hour(isrTime) && alm1.mm == minute(isrTime)) {
        if (alm1.dd & 0b00000001 << (weekday(isrTime)-1)) {
            return true;
        }
        if (alm1.dd & 0b10000000) {
            alm1.dd = 0;
            writeAlarms (ALARM1, alm1);
            return true; //??
        }
    }
    if (alm2.act && alm2.hh == hour(isrTime) && alm2.mm == minute(isrTime)) {
        if (alm2.dd & 0b00000001 << (weekday(isrTime)-1)) {
            return true;
        }
    }
    return false;
}

void writeAlarms (uint16_t address, struct alarms alm)
{
    fram.write(address, alm.hh);
    fram.write(address+1, alm.mm);
    fram.write(address+2, alm.dd);
    fram.write(address+3, alm.act);
    fram.write(address+4, alm.light);
    fram.write(address+5, alm.soundfile);
}

void readAlarms (uint16_t address, struct alarms &alm)
{
    alm.hh = fram.read(address);
    alm.mm = fram.read(address+1);
    alm.dd = fram.read(address+2);
    alm.act = fram.read(address+3);
    alm.light = fram.read(address+4);
    alm.soundfile = fram.read(address+5);
}
