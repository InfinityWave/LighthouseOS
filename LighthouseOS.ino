
#include <Arduino.h>
#include <SPI.h>
#include <StateMachine.h>
#include <TimeLib.h>      //https://github.com/PaulStoffregen/Time.git
#include <DS3232RTC.h>    //https://github.com/H4K5M6/DS3232RTC.git
#include <DCF77.h>        //https://github.com/thijse/Arduino-DCF77.git
#include <Adafruit_FRAM_I2C.h> //from lib manager v2.0
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>// Controller specific library
#include <CheapStepper.h> //https://github.com/H4K5M6/CheapStepper
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
#define STEP_IN1 A0
#define STEP_IN2 A1
#define STEP_IN3 A2
#define STEP_IN4 A3

//Parameters
#define COLOR_BKGND ILI9341_BLACK // background color
#define COLOR_TXT ILI9341_WHITE // text color
#define MMEL 9    // number of menu entries

// TFT/Canvas Settings
#define FONTSIZE_BIG_HEIGHT 54
#define FONTSIZE_SMALL_HEIGHT 38
#define MENULINEWIDTH 3 // LineWidth for Menu
#define CANVAS_CLOCK_WIDTH 76
#define CANVAS_CLOCK_CURSOR_POS 50
#define CANVAS_DATE_WIDTH 232
#define CANVAS_DATE_CURSOR_POS 34
// Canvas Settings for Left and Right Menu Icons
#define CANVAS_MENULR_WIDTH 45
#define CANVAS_MENULR_HEIGHT 45
#define CANVAS_MENULR_OFFSET_X 5
#define CANVAS_MENULR_OFFSET_Y 7
//Global Position for Icon Canvas
#define CANVAS_MENUL_X 10
#define CANVAS_MENUL_Y 0
#define CANVAS_MENUR_X 240
#define CANVAS_MENUR_Y 0
#define CENTER_ICON_X 160
#define CENTER_ICON_Y 15
#define CENTER_ICON_SIZE 10
#define CENTER_ICON_LW 2
//ClockDisplaySettings
#define CLOCKDISPLAY_BTLINE_Y 220
//MenuSettings
#define MENU_TOPLINE_Y 50
#define MENU_ITEM1_Y 80



#define LITE 200  // standard brightness

#define DCF_INT 6     // interval of dcf sync in hours
#define DCF_HOUR 0    // hour to start dcf sync
#define DCF_MIN 0     // minute to start dcf sync
#define DCF_LEN 3600    // length in seconds of dcf sync attempt

#define STEP_RPM 1
#define STEP_N 4096

// initializations
uint8_t canvasClockHeight = FONTSIZE_BIG_HEIGHT + MENULINEWIDTH;
uint8_t canvasDateHeight = FONTSIZE_SMALL_HEIGHT + MENULINEWIDTH ;
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC/*, TFT_RESET*/);
//GFXcanvas1 canvas(280, 54); // 128x32 pixel canvas
GFXcanvas1 canvasClock(CANVAS_CLOCK_WIDTH, canvasClockHeight); // Canvas for Clock Numbers
GFXcanvas1 canvasDate(CANVAS_DATE_WIDTH, canvasDateHeight); // Canvas for Date
GFXcanvas1 canvasMenuLR(CANVAS_MENULR_WIDTH, CANVAS_MENULR_HEIGHT);
//GFXcanvas1 canvasDay(242, 50); // Canvas for Date


// Define StateMachine
/////////////////////////////////////////////////////////////
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

State* S99 = machine.addState(&stateAlarmActive); // State for active alarm

// Define Clock-HW
////////////////////////////////////////////////////////////
DS3232RTC myRTC(false);

DCF77 DCF = DCF77(DCF_PIN, digitalPinToInterrupt(DCF_PIN), false);

Adafruit_FRAM_I2C fram = Adafruit_FRAM_I2C();

CheapStepper stepper (STEP_IN1, STEP_IN2, STEP_IN3, STEP_IN4); 


volatile time_t isrTime;
volatile bool isrTimeChange;
bool isrTimeUpdate;
char timeString[3];
char dateString[11];
bool dcfSync = false;
bool dcfSyncSucc = false;
uint32_t dcfSyncStart = 0;

uint8_t clockHour, clockMinute, clockSecond, clockDay;

// button flags
volatile bool isrbuttonC = false;
volatile bool isrButtonL = false;
volatile bool isrButtonR = false;

// menu entries
const char *mainMenuEntries[MMEL] = {"Clock", "Alarm1", "Alarm2", "Sound", "Light", "Brightness", "Homing", "Back", "Credits"};

//menu variables
int8_t selectedMMItem = 0;
int8_t currentMenu = -1;
uint32_t sleepDelay = 60000;
uint32_t sleepTimer = 0;
bool updateScreen = false;
bool changeValue = false;

// Alarm variables
uint32_t alarmLightDelay = 1000;
uint32_t alarmLightTimer = 0;
bool alarmLightOn = false;

//stepper variables
bool moveTower = true;
bool stepperActive = false;

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

// settings
uint8_t hours_per_day = 24; 

struct menuStruct {
    uint8_t selectedItem = 0;
    uint8_t currentVal = 0;
    int16_t item[8];
    
} submenu;

bool weddingModeFinished = false;

void setup(void) {
    Serial.begin(9600);
    
    // setup TFT Backlight as off
    pinMode(TFT_LITE, OUTPUT);
    analogWrite(TFT_LITE, 0);

    // setup tower LED
    pinMode(LED_MAIN, OUTPUT);
    analogWrite(LED_MAIN, 0);		

    //stepper setup
    stepper.setRpm(STEP_RPM);

    // DCF setup
    digitalWrite(DCF_EN_PIN, HIGH);
    pinMode(DCF_EN_PIN, OUTPUT);

    // setup state machine
    // from clockDisplay
    S0->addTransition(&toSleep,S1);         // to standby
    S0->addTransition(&openMainMenu,S2);    // to main menu
    S0->addTransition(&changeToWeddingMode,S3); //start wedding mode
	S0->addTransition(&startAlarm,S99); // check and start alarm
    S0->addTransition(&attachUnhandledInterrupts,S0);   //Must be last item
    // from standby
    S1->addTransition(&wakeup,S0);          // to clock
	S1->addTransition(&startAlarm,S99); // check and start alarm
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
	S2->addTransition(&startAlarm,S99); // check and start alarm
    //S2->addTransition(&attachUnhandledInterrupts,S2);   //Must be last item
    // from wedding mode
    S3->addTransition(&exitWeddingMode,S0);
    //Return from all SubMenus
    S4->addTransition(&returnToMainMenu,S2);
    S4->addTransition(&changeClockMenuSelection,S4);
	S4->addTransition(&startAlarm,S99); // check and start alarm
    S5->addTransition(&returnToMainMenu,S2);
	S5->addTransition(&startAlarm,S99); // check and start alarm
    S6->addTransition(&returnToMainMenu,S2);
	S6->addTransition(&startAlarm,S99); // check and start alarm
    S7->addTransition(&returnToMainMenu,S2);
	S7->addTransition(&startAlarm,S99); // check and start alarm
    S8->addTransition(&returnToMainMenu,S2);
	S8->addTransition(&startAlarm,S99); // check and start alarm
    S9->addTransition(&returnToMainMenu,S2);
	S9->addTransition(&startAlarm,S99); // check and start alarm
    S10->addTransition(&returnToMainMenu,S2);
	S10->addTransition(&startAlarm,S99); // check and start alarm
    S11->addTransition(&returnToMainMenu,S2);
	S11->addTransition(&startAlarm,S99); // check and start alarm
    S11->addTransition(&attachUnhandledInterrupts,S11);
	// Stop alarm and return to clock
	S99->addTransition(&stopAlarm,S2);
	
    // TFT setup
    delay(500);
    tft.begin();
    tft.setTextColor(COLOR_TXT);
    tft.setTextSize(2);
    tft.setRotation(3);
    tft.fillScreen(COLOR_BKGND);
    tft.setFont(&FreeSans18pt7b);
    tft.setCursor(105,88);
    tft.print(":");
//    canvas.setFont(&FreeSans18pt7b);
//    canvas.setTextSize(2);
    canvasClock.setFont(&FreeSans18pt7b);
    canvasClock.setTextSize(2);
    canvasDate.setFont(&FreeSans12pt7b);
    canvasDate.setTextSize(2);
    canvasMenuLR.setFont(&FreeSans18pt7b);
    canvasMenuLR.setTextSize(2);

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
    //myRTC.set(1577836800);
    updateClock();
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
    stepper.setStep(read16bit(C_STEP));
    
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
    if (!dcfSync && hour(isrTime)%DCF_INT == DCF_HOUR && minute(isrTime) == DCF_MIN) {
        DCF.Start();
        digitalWrite(DCF_EN_PIN, LOW);
        dcfSync = true;
        dcfSyncStart = isrTime;
        Serial.print("DCFSync start: ");
        Serial.print(hour(isrTime));
        Serial.print(":");
        Serial.println(minute(isrTime));
    }
    if (dcfSync && isrTime >= dcfSyncStart+DCF_LEN) {
        DCF.Stop();
        digitalWrite(DCF_EN_PIN, HIGH);
        dcfSync = false;
        dcfSyncSucc = false;
        Serial.println("Sync Failed");
    }
    if (dcfSync && DCF.getTime() != 0) {
        DCF.Stop();
        myRTC.set(DCF.getTime());
        digitalWrite(DCF_EN_PIN, HIGH);
        dcfSync = false;
        dcfSyncSucc = true;
        Serial.print("Sync Success");
        Serial.print(hour(isrTime));
        Serial.print(":");
        Serial.println(minute(isrTime));
    }
    machine.run(); // StateMachine
    stepper.run();
    if (moveTower) {
        stepper.run();
        if (stepperActive && stepper.getStepsLeft() == 0) {
            stepperActive = false;
            stepper.off();
        }
    }
}


void correct_date(int16_t *date){
    // Check the date and correct based on calendar
    // Date must be an array with 1st entry hour, than minute, day, month, year
    
    // Check if leap year
    int leapyear = 0;
    if (!(date[4] % 4)){
        leapyear = 1;
    }
    // Check month overflow
    date[3] = date[3] % 12;
    // check correct number of days for month
    // Begin with month with 31 days
    if (date[3]==0 || date[3]==2 || date[3]==4 || date[3]==6 || date[3]==7 || date[3]==9 || date[3]==11){
        while (date[2] < 0){
            date[2] = 31 + date[2]; 
        }
        date[2] = date[2] % 31; 
    }
    // Month with 30 days
    if (date[3]==3 || date[3]==5 || date[3]==8 || date[3]==10){
        while (date[2] < 0){
            date[2] = 30 + date[2]; 
        }
        date[2] = date[2] % 30;
    }
    // February
    if (date[3]==1){
        while (date[2] < 0){
            date[2] = 28 + leapyear + date[2]; 
        }
        date[2] = date[2] % (28 + leapyear);
    }
    // Correct hour over and underflow
    while (date[0] < 0){
        date[0] = date[0] + hours_per_day; 
    }
    date[0] = date[0] % hours_per_day;
    // Correct minute over and underflow
    while (date[1] < 0){
        date[1] = date[1] + 60; 
    }
    date[1] = date[1] % 60;
}

//StateMachine States
////////////////////////////////////////////////////////////
//S0 = main clock display
void stateClockDisplay()
{
    if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
        analogWrite(TFT_LITE, brightness);
    }
	if (isrButtonR) {							// Light button was pressed
      isrButtonR = false;						// Reset Btn-flag
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
	  alarmLightOn = true; 						// Turn on light flag
	  analogWrite(LED_MAIN, ALARM_LIGHT_MAX);	// Switch tower light
	  digitalWrite(LED_BTN_C, HIGH);			// Switch btn LEDs on
	  digitalWrite(LED_BTN_R, HIGH);			// Switch btn LEDs on
	  digitalWrite(LED_BTN_L, HIGH);			// Switch btn LEDs on
	  alarmLightTimer = millis();				// (Re-)Start timer
    }
	if (millis() > alarmLightTimer + alarmLightDelay){ // Time is up...
		alarmLightOn = false; 						// Turn on light flag
		analogWrite(LED_MAIN, 0);					// Switch tower light
		digitalWrite(LED_BTN_C, LOW);				// Switch btn LEDs off
		digitalWrite(LED_BTN_R, LOW);				// Switch btn LEDs off
		digitalWrite(LED_BTN_L, LOW);				// Switch btn LEDs off
	}
}

// standby state
void stateStandby()
{
    //nothing yet maybe send arduino to sleep
    if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
    }
}

// S99 = Active alarm
void stateAlarmActive()
{
	if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
        analogWrite(TFT_LITE, brightness);
	}
	if (millis() > alarmLightTimer + alarmLightDelay) {
		alarmLightTimer = millis();								// Restart timer
		alarmLightOn = not alarmLightOn;						// Switch state of light
		analogWrite(LED_MAIN, ALARM_LIGHT_MAX * alarmLightOn);	// Switch tower light
	}
		/////////////////////////////////////
		// TODO
		// Make sure MP3 is running
		/////////////////////////////////////
}

//S1
// main menu
void stateMainMenu()
{
    if (updateScreen) {
        updateScreen = false;
        // Draw Top Icons
        drawMenuSetter(true);
        tft.setCursor(100,40);
        tft.setFont(&FreeSans12pt7b);
        tft.println("Menu");
        tft.drawRect(0,55,320,MENULINEWIDTH,COLOR_TXT);
        tft.fillCircle(15,145,7,COLOR_TXT);
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
    if (updateScreen) {
        updateScreen = false;
    tft.setCursor(100,40);
    //tft.setFont(&FreeSans12pt7b);
    tft.println("Clock");
    tft.drawFastHLine(0,50,320,COLOR_TXT);
    tft.fillCircle(15,145,7,COLOR_TXT);
    tft.setCursor(40,100);
    tft.println(mainMenuEntries[(selectedMMItem+MMEL-1)%MMEL]);
    tft.setCursor(40,160);
    tft.println(mainMenuEntries[selectedMMItem]);
    tft.setCursor(40,220);
    tft.println(mainMenuEntries[(selectedMMItem+1)%MMEL]);
    analogWrite(TFT_LITE, brightness);
    }
    //Manually set time
    //Set Light Interval
    //Set Motor Interval
    //clockDisplay(isrTime);
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
    //Set Volume 0 - 30

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

//StateMachine Transitions
///////////////////////////////////////////////
bool openMainMenu()
{
    if (isrbuttonC) {
      isrbuttonC = false;
      sleepTimer = millis();
      analogWrite(TFT_LITE, 0);
      tft.fillScreen(COLOR_BKGND);
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

// Stop alarm when pressing L-Button
bool stopAlarm()
{
	if (isrButtonL) {			// Stop-Alam button was pressed
      isrButtonL = false;		// Reset Btn-flag
      currentMenu = -1;			// No menu will be active
      isrTimeUpdate = true;		// ??
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
	  analogWrite(LED_MAIN, 0); // Switch off tower light
	  digitalWrite(LED_BTN_C, LOW);			// Switch btn LEDs off
	  digitalWrite(LED_BTN_R, LOW);			// Switch btn LEDs off
	  digitalWrite(LED_BTN_L, LOW);			// Switch btn LEDs off
	  /////////////////////////////////////
	  // TODO
	  // Stop MP3-Player
	  /////////////////////////////////////
      return true;
    }
	// Do nothing when C or R is pressed (switch off flag)
	if (isrbuttonC||isrButtonR) {
      isrbuttonC = false;
      isrButtonR = false;
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
      return false;
	}
    return false;
}

// Start alamr when time has come
bool startAlarm()
{
	if (checkAlarms()) {
		digitalWrite(LED_BTN_C, HIGH);			// Switch btn LEDs on
		digitalWrite(LED_BTN_R, HIGH);			// Switch btn LEDs on
		digitalWrite(LED_BTN_L, HIGH);			// Switch btn LEDs on
		/////////////////////////////////////
		// TODO
		// Start MP3-Player
		/////////////////////////////////////
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
        tft.fillScreen(COLOR_BKGND);
        selectedMMItem = (selectedMMItem+1)%noItems;
        updateScreen = true;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        return true;
    }
    if(isrButtonL){
        isrButtonL = false;
        analogWrite(TFT_LITE, 0);
        tft.fillScreen(COLOR_BKGND);
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
    if (isrbuttonC  && (submenu.selectedItem == 0)){
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
        submenu.selectedItem = 0;
        submenu.item[0] = hour(isrTime);
        submenu.item[1] = minute(isrTime);
        submenu.item[2] = day(isrTime);
        submenu.item[3] = month(isrTime);
        submenu.item[4] = year(isrTime);
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;   
    }
    return false;
}

bool changeClockMenuSelection()
{
    if (isrbuttonC){
        isrbuttonC = false;
        submenu.selectedItem = (submenu.selectedItem + 1) % 6;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;
    }
    if (isrButtonL || isrButtonR) {
        if (isrButtonL){
            isrButtonL = false;
            submenu.item[submenu.selectedItem] --;
            delay(50);
            attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        }
        else {
            isrButtonR = false;
            submenu.item[submenu.selectedItem] ++; 
            delay(50);
            attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        }
    }
    // Correct date after input
    correct_date(submenu.item);
}

bool openAlarm1Menu()
{
    if (isrbuttonC  && (selectedMMItem == 1)){
        sleepTimer = millis();
        isrbuttonC = false;
        currentMenu = selectedMMItem;
        submenu.selectedItem = 0;
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
        submenu.selectedItem = 0;
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
        submenu.selectedItem = 0;
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
        submenu.selectedItem = 0;
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
        submenu.selectedItem = 0;
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
        submenu.selectedItem = 0;
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
          tft.fillScreen(COLOR_BKGND);
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

// Clock stuff
////////////////////////////////////////////////////////
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

void updateCanvasClock(uint8_t cDigit, bool underline)
{
    sprintf(timeString,"%02d",cDigit);
    canvasClock.fillScreen(COLOR_BKGND);
    canvasClock.setCursor(0, CANVAS_CLOCK_CURSOR_POS);
    canvasClock.println(timeString);
    if (underline)
    {
        int16_t  x, y;
        uint16_t w, h;
        canvasClock.getTextBounds(timeString, 0, CANVAS_CLOCK_CURSOR_POS, &x, &y, &w, &h);
        canvasClock.fillRect(x, y+h+1, w, MENULINEWIDTH, COLOR_TXT);
    }

}

void updateCanvasDate(uint8_t cDay, uint8_t cMonth, uint16_t cYear, int8_t underline)
{
    uint8_t cursorPos = CANVAS_DATE_CURSOR_POS;
    sprintf(dateString,"%02d.%02d.%04d", cDay, cMonth, cYear);
    canvasDate.fillScreen(COLOR_BKGND);
    canvasDate.setCursor(0, CANVAS_DATE_CURSOR_POS);
    canvasDate.println(dateString);
    if (true)
    {
        int16_t  x1, y1;
        uint16_t w1, h1;
        uint16_t x2, y2;
        uint16_t w2, h2;
        canvasDate.getTextBounds(dateString, 0, cursorPos, &x1, &y1, &w1, &h1);
        sprintf(dateString,"%04d", cYear);
        canvasDate.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        canvasDate.fillRect(x1+w1-w2, y1+h2+1, w2, MENULINEWIDTH, COLOR_TXT);
    }

}

void updateCanvasMenuSetter(int signtype)
{
    canvasMenuLR.fillScreen(COLOR_BKGND);
    canvasMenuLR.setCursor(CANVAS_MENULR_OFFSET_X, CANVAS_MENULR_HEIGHT-CANVAS_MENULR_OFFSET_Y);
    
    switch (signtype)
    {
        case 0: canvasMenuLR.println('-');
        case 1: canvasMenuLR.println('+');
    }

}

void drawMenuSetter(bool isOn)
{
    if (true){
        updateCanvasMenuSetter(0);
        tft.drawBitmap(CANVAS_MENUL_X, CANVAS_MENUL_Y, canvasMenuLR.getBuffer(), CANVAS_MENULR_WIDTH, CANVAS_MENULR_HEIGHT, COLOR_TXT, COLOR_BKGND);
        updateCanvasMenuSetter(1);
        tft.drawBitmap(CANVAS_MENUR_X, CANVAS_MENUR_Y, canvasMenuLR.getBuffer(), CANVAS_MENULR_WIDTH, CANVAS_MENULR_HEIGHT, COLOR_TXT, COLOR_BKGND);
    }
    else{

    }
    tft.fillCircle(CENTER_ICON_X, CENTER_ICON_Y, CENTER_ICON_Y, COLOR_TXT);
    tft.fillCircle(CENTER_ICON_X, CENTER_ICON_Y, CENTER_ICON_Y-CENTER_ICON_LW, COLOR_BKGND);
}

void clockDisplay(time_t t)
{
        if (clockHour != hour(t)){
            clockHour = hour(t);
            updateCanvasClock(clockHour, true);
            tft.drawBitmap(31, 38, canvasClock.getBuffer(), CANVAS_CLOCK_WIDTH, canvasClockHeight, COLOR_TXT, ILI9341_RED);
        }
        if (clockMinute != minute(t)){
            clockMinute = minute(t);
            updateCanvasClock(clockMinute, true);
            tft.drawBitmap(121, 38, canvasClock.getBuffer(), CANVAS_CLOCK_WIDTH, canvasClockHeight, COLOR_TXT, COLOR_BKGND);
            if (moveTower) {
                stepper.newMoveToDegree(true, ((uint16_t)clockMinute)*30);
                stepperActive = true;
                Serial.println(((uint16_t)clockMinute)*30);
                Serial.println(stepper.getStep());
                Serial.println(stepper.getStepsLeft());
            }
        }
        /*if (clockSecond != second(isrTime)){
            clockSecond = second(isrTime);
            sprintf(timeString,"%02d",clockSecond);
            canvasClock.fillScreen(ILI9341_BLACK);
            canvasClock.setCursor(0, 50);
            canvasClock.println(timeString);
            tft.drawBitmap(211, 38, canvasClock.getBuffer(), 76, 54, ILI9341_WHITE, ILI9341_RED);
        }*/
        if (clockDay != day(t)){
            clockDay = day(t);
            updateCanvasDate(clockDay, month(isrTime), year(isrTime), -1);
            tft.drawBitmap(44, 138, canvasDate.getBuffer(), CANVAS_DATE_WIDTH, canvasDateHeight, COLOR_TXT, ILI9341_BLUE);
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

// Check if an alarm must be triggered
bool checkAlarms () 
{
		/////////////////////////////////////
		// TODO
		// Make sure each alarm is triggered only once!
		// Idea: Set to next day or turn off
		/////////////////////////////////////
    if (alm1.act && alm1.hh >= hour(isrTime) && alm1.mm >= minute(isrTime)) {
        if (alm1.dd & 0b00000001 << (weekday(isrTime)-1)) {
            return true;
        }
        if (alm1.dd & 0b10000000) {
            alm1.dd = 0;
            writeAlarms (ALARM1, alm1);
            return true; //??
        }
    }
    if (alm2.act && alm2.hh >= hour(isrTime) && alm2.mm >= minute(isrTime)) {
        if (alm2.dd & 0b00000001 << (weekday(isrTime)-1)) {
            return true;
        }
    }
    return false;
}

//FRAM functions

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

void write16bit (uint16_t address, uint16_t data)
{
    fram.write(address, (uint8_t*)&data, 2);
}

uint16_t read16bit (uint16_t address)
{
    uint16_t data;
    fram.read(address, (uint8_t*)&data, 2);
    return data;
}


void write32bit (uint16_t address, uint32_t data)
{
    fram.write(address, (uint8_t*)&data, 4);
}

uint32_t read32bit (uint16_t address)
{
    uint32_t data;
    fram.read(address, (uint8_t*)&data, 4);
    return data;
}
