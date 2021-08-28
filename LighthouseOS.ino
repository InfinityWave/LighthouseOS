
#include <Arduino.h>
#include <SPI.h>
#include <StateMachine.h>
#include <TimeLib.h>      //https://github.com/PaulStoffregen/Time.git
#include <DS3232RTC.h>    //https://github.com/H4K5M6/DS3232RTC.git
#include <DCF77.h>        //https://github.com/thijse/Arduino-DCF77.git
#include <Adafruit_FRAM_I2C.h> //from lib manager v2.0
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>// Controller specific library
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>

//DS3231 pins
#define DS3231_INT 3

//DCF77 pins
#define DCF_PIN 2         // Connection pin to DCF 77 device
#define DCF_EN_PIN 4      // Enable DCF77 module active low

//TFT pins
//use Hardware SPI NANO: SCK=13, MISO=12, MOSI=11 
#define TFT_DC 9
#define TFT_CS 10
#define TFT_LITE 5 //PWM pin
//#define TFT_RESET 

//Button pins
#define BTN_MID 7
#define BTN_L A6
#define BTN_R 4

//Parameters
#define BKGND ILI9341_BLACK
#define TXT ILI9341_WHITE 
#define MMEL 5    // number of menu entries

#define LITE 200  // standard brightness



// initializations
StateMachine machine = StateMachine();

State* S0 = machine.addState(&stateClockDisplay);
State* S1 = machine.addState(&stateStandby);
State* S2 = machine.addState(&stateMainMenu);

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
char timeString[3];
char dateString[11];

uint8_t clockHour, clockMinute, clockSecond, clockDay;

// button flags
volatile bool isrButtonMID = false;
volatile bool isrButtonL = false;
volatile bool isrButtonR = false;

// menu entries
const char *mainMenuEntries[MMEL] = {"Volume", "Alarm", "Brightness", "Homing", "Back"};

//menu variables
int8_t selectedItem = 0;
int8_t currentMenu = -1;
uint16_t sleepDelay = 65000;
unsigned long sleepTimer = 0;

//display variable
uint8_t brightness = 0;

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
    // from standby
    S1->addTransition(&wakeup,S0);          // to clock
    // from main menu
    S2->addTransition(&closeMainMenu,S0);   // to clock
    S2->addTransition(&toSleep,S1);         // to standby
    S2->addTransition(&changeSelection,S2); // stay and change selection
    
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
    pinMode(BTN_MID, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_MID), buttonMID, FALLING);
    pinMode(BTN_R, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
    pinMode(BTN_L, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
}

void loop()
{
    machine.run();
}

// main clock display
void stateClockDisplay()
{
    if (isrTimeChange) {
      isrTimeChange = false;
      isrTime = myRTC.get();
      myRTC.alarm(ALARM_2);
      Serial.println("Ja");
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
        analogWrite(TFT_LITE, brightness);
    }
}

// standby state
void stateStandby()
{
    //nothing yet maybe send arduino to sleep
}

// main menu
void stateMainMenu()
{
    tft.setCursor(100,40);
    tft.setFont(&FreeSans12pt7b);
    tft.println("Menu");
    tft.drawFastHLine(0,50,320,TXT);
    tft.fillCircle(15,145,7,TXT);
    tft.setCursor(40,100);
    tft.println(mainMenuEntries[(selectedItem+MMEL-1)%MMEL]);
    tft.setCursor(40,160);
    tft.println(mainMenuEntries[selectedItem]);
    tft.setCursor(40,220);
    tft.println(mainMenuEntries[(selectedItem+1)%MMEL]);
    analogWrite(TFT_LITE, brightness);
}

bool openMainMenu()
{
    if (isrButtonMID) {
      isrButtonMID = false;
      sleepTimer = millis();
      analogWrite(TFT_LITE, 0);
      tft.fillScreen(BKGND);
      currentMenu = 0;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_MID), buttonMID, FALLING);
      return true;
    }
    return false;
}

bool toSleep()
{
    if (millis() > sleepTimer + sleepDelay) {
        analogWrite(TFT_LITE, 0);
        return true;
    }
    return false;
}

bool closeMainMenu()
{
    if (isrButtonMID) {
      isrButtonMID = false;
      sleepTimer = millis();
      if (selectedItem == MMEL-1) {
          analogWrite(TFT_LITE, 0);
          tft.fillScreen(BKGND);
          prepareClock();
          currentMenu = -1;
          isrTimeChange = true; // to enable clock display
          delay(50);
          attachInterrupt(digitalPinToInterrupt(BTN_MID), buttonMID, FALLING);
          return true;
          }
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_MID), buttonMID, FALLING);
      return false;
    }
    return false;
}

bool wakeup()
{
    if (isrButtonMID||isrButtonR||isrButtonL) {
      isrButtonMID = false;
      isrButtonR = false;
      isrButtonL = false;
      sleepTimer = millis();
      analogWrite(TFT_LITE, 0);
      tft.fillScreen(BKGND);
      prepareClock();
      currentMenu = -1;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_MID), buttonMID, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
      isrTimeChange = true; // to enable clock display
      return true;
    }
    return false;
}

void changeSelection()
{
    int noItems = 0;
    switch (currentMenu) {
      case 0: noItems = MMEL;
    }
    if(isrButtonR){
        isrButtonR = false;
        tft.fillScreen(BKGND);
        selectedItem = (selectedItem+1)%noItems;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        return true;
    }
    if(isrButtonL){
        isrButtonL = false;
        tft.fillScreen(BKGND);
        selectedItem = (selectedItem+noItems-1)%noItems;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        return true;
    }
    return false;
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

// interrupt functions

void isrChangeTime()
{
    isrTimeChange = true;
}

// button interrupt flags

void buttonMID()
{
    detachInterrupt(digitalPinToInterrupt(BTN_MID));
    isrButtonMID = true;
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
