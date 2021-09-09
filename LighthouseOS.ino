
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
#define TFT_WIDTH 320
#define TFT_HEIGHT 240
//#define TFT_RESET 

//Button pins
#define BTN_C 5           //interrupt
#define BTN_L 7           //interrupt
#define BTN_R 6           //interrupt

//LED pins
#define LED_BTN_C 0        //TODO Button LED
#define LED_BTN_R 0        //TODO Button LED
#define LED_BTN_L 0        //TODO Button LED
#define LED_MAIN 0         //TODO PWM pin
#define ALARM_LIGHT_MAX 0  //TODO Set

// stepper pins
#define STEP_IN1 A0
#define STEP_IN2 A1
#define STEP_IN3 A2
#define STEP_IN4 A3

//Parameters
#define COLOR_BKGND ILI9341_BLACK // background color
#define COLOR_TXT ILI9341_WHITE // text color
#define COLOR_BLUE 0x001F
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0


/* Pixels for FontSize
Number "88":
12pt size 1: 24x17 
12pt size 2: 48x34
18pt size 1: 36x25 
18pt size 2: 72x50
Date "11.09.2021":
12pt size 1: 112x17 
12pt size 2: 224x34
18pt size 1: 165x25 
18pt size 2: 296x50
*/
// TFT&Canvas Settings
#define FONTSIZE_BIG_HEIGHT 50
#define FONTSIZE_NORMAL_HEIGHT 34
#define FONTSIZE_SMALL_HEIGHT 25
#define FONTSIZE_TINY_HEIGHT 17
#define FONT_MARGIN 4
#define CANVAS_CLOCK_WIDTH 76
#define CANVAS_DATE_WIDTH 232
// Margins due to the surrounding wooden box. TODO Edit for final setup
#define TFT_MARGIN_LEFT 5
#define TFT_MARGIN_RIGHT 30
#define TFT_MARGIN_TOP 7 
#define TFT_MARGIN_BOT 25
// Canvas Settings for Left and Right Menu Icons
#define CANVAS_MENULR_WIDTH 45
#define CANVAS_MENULR_HEIGHT 30
#define CANVAS_MENULR_ICONOFFSET 12
//Global Position for Icon Canvas
#define CANVAS_MENUL_X 5
#define CANVAS_MENUL_Y 7
#define CANVAS_MENUR_X 235
#define CANVAS_MENUR_Y 7
#define CENTER_ICON_X 135
#define CENTER_ICON_Y 13
#define CENTER_ICON_SIZE 12
#define CENTER_ICON_LW 4
//ClockDisplaySettings
#define CLOCKDISPLAY_CLOCK_X 55
#define CLOCKDISPLAY_CLOCK_Y 55
#define CLOCKDISPLAY_DATE_X 35
#define CLOCKDISPLAY_DATE_Y 125
#define CLOCKDISPLAY_LH_X 0
#define CLOCKDISPLAY_SYNC_X 175
#define CLOCKDISPLAY_TEMP_X 245

//MenuSettings
#define MENULINEWIDTH 3
#define MENU_TOPLINE_Y 43
#define MENU_ITEMS 3
#define MENU_ITEMS_X 35
#define MENU_ITEM1_Y 80
#define MENU_SEL_X 10
#define MENU_SEL_Y 150
#define MENU_SEL_SIZE 5
//SpecialMenuSettings
#define MAINMENU_ITEMS 9    // number of menu entries
#define MAINMENU_TXT_X 120
//SpecialSubMenuSettings
#define SUBMENU_MAX_ITEMS 8
#define SOUND_ITEMS 3
#define ALARM_ITEMS 4
#define ALARMTIME_ITEMS 4
#define CLOCK_ITEMS 4

#define LITE 200  // standard brightness

#define DCF_INT 6     // interval of dcf sync in hours
#define DCF_HOUR 0    // hour to start dcf sync
#define DCF_MIN 0     // minute to start dcf sync
#define DCF_LEN 3600    // length in seconds of dcf sync attempt

#define STEP_RPM 1
#define STEP_N 4096


//


// initializations
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC/*, TFT_RESET*/);

GFXcanvas1 canvasClock(CANVAS_CLOCK_WIDTH, FONTSIZE_BIG_HEIGHT+FONT_MARGIN+MENULINEWIDTH); // Canvas for Clock Numbers
GFXcanvas1 canvasDate(CANVAS_DATE_WIDTH, FONTSIZE_NORMAL_HEIGHT+FONT_MARGIN+MENULINEWIDTH); // Canvas for Date
GFXcanvas1 canvasMenuItem(180, FONTSIZE_SMALL_HEIGHT+3*FONT_MARGIN+MENULINEWIDTH); // Bigger Canvas not supported
GFXcanvas1 canvasTinyFont(220, FONTSIZE_TINY_HEIGHT+3*FONT_MARGIN);
GFXcanvas1 canvasMenuLR(CANVAS_MENULR_WIDTH, CANVAS_MENULR_HEIGHT);

//Variables to store size of display/canvas texts
int16_t  x1, y1;
uint16_t w1, h1;
uint16_t x2, y2;
uint16_t w2, h2;
int16_t cursorX, cursorY, cursorYMenuStart;

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
char outString[100];
bool dcfSync = false;
bool dcfSyncSucc = false;
bool DCFSyncStatus = false;
bool DCFSyncChanged = false;
uint32_t dcfSyncStart = 0;

uint8_t clockHour, clockMinute, clockSecond, clockDay;
float currentTemp = 19.0;
bool tempChanged = false;


// button flags
volatile bool isrButtonC = false;
volatile bool isrButtonL = false;
volatile bool isrButtonR = false;

// menu entries
const char *mainMenuEntries[MAINMENU_ITEMS] = {"Clock", "Alarm 1", "Alarm 2", "Sound", "Light", "Brightness", "Homing", "Credits", "Back"};
const char *soundOptions[SOUND_ITEMS] = {"Horn", "Wedding", "Off"};
const char *alarmOptions[ALARM_ITEMS] = {"Light+Horn", "Light+Wedding", "Light only", "Off"};
const char *alarmTimeOptions[ALARMTIME_ITEMS] = {"Always", "Mo-Fr", "Sa+So", "Off"};
const char *clockOptions[CLOCK_ITEMS] = {"Light+Motor", "Light only", "Motor only", "Off"};

uint16_t soundOption = 0; //ToDo Read from FRAM 
uint16_t alarmOption = 0; //ToDo Read from FRAM 
uint16_t alarmTimeOption = 0; //ToDo Read from FRAM 
uint16_t clockOption = 0; //ToDo Read from FRAM 


//menu variables
int8_t selectedMMItem = 0;
uint32_t rtcSyncDelay = 10*60*1000; // 10min
uint32_t rtcSyncTimer = 0;
uint32_t sleepDelay = 60000;
uint32_t sleepTimer = 0;
bool updateScreen = true;
bool updateMenuSelection = false;
bool updateClockSign = false;
bool changeValue = false;

// Alarm variables
uint32_t alarmLightDelay = 1000;	// Time for tower light on (ms)
uint32_t alarmLightTimer = 0;		// Tower light times
bool alarmLightOn = false;			// If tower light is on
time_t isrTime_last = 0;			// Stores time to check if a new alarm check is needed
uint32_t alarmTotalTimer = 0;		// Timer to stop alarm after some time (if noone is at home to press the button)
uint32_t alarmTotalDelay = 120000;	// 2 min

// Stepper and tower variables
bool moveTower = true;
bool stepperActive = false;			// Does the stepper run? Do not command any new movement before the last one is finished
uint16_t towerPosOffset = 0; 		// 0 o'clock light position (homing)
uint16_t towerPos_last = 0; 		// To compare and check if a new position has to be commanded

//display variable
uint8_t brightness = LITE;

//alarms
struct alarms {
    uint8_t hh;
    uint8_t mm;
    uint8_t dd;			// Not used, outside of read and write FRAM
    bool act;			// Not used, outside of read and write FRAM
    bool light;			// Not used, outside of read and write FRAM
    uint8_t soundfile;	// Not used, outside of read and write FRAM
    time_t nextAlarm; // Time stamp of next alarm. Will not be stored on FRAM
	uint8_t mode; // 0: Off, 1: Once, 2: Every day, 3: Weekdays, 4: Weekend
} alm1, alm2;

// settings
uint8_t hours_per_day = 24; 

struct menuStruct {
    uint8_t num_items;
    uint8_t selectedItem = 0;
    uint8_t currentVal = 0;
    int16_t item[SUBMENU_MAX_ITEMS];
    int16_t prev_item[SUBMENU_MAX_ITEMS];
    int8_t dateinfo_starts;
    int8_t timeinfo_starts;
    uint8_t maxVal[SUBMENU_MAX_ITEMS];
    uint8_t minVal[SUBMENU_MAX_ITEMS];
} submenu;

bool weddingModeFinished = false;

void setup(void) {
    Serial.begin(9600);
    
    // setup TFT Backlight as off
    pinMode(TFT_LITE, OUTPUT);
    analogWrite(TFT_LITE, brightness);

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
	//S0->addTransition(&startAlarm,S99); // check and start alarm
    S0->addTransition(&reAttachUnhandledInterrupts,S0);   //Must be last item
    // from standby
    S1->addTransition(&wakeup,S0);          // to clock
	//S1->addTransition(&startAlarm,S99); // check and start alarm
    // from main menu
    S2->addTransition(&returnToClockDisplay,S0); //After timeout
    S2->addTransition(&closeMainMenu,S0);   // to clock
    S2->addTransition(&changeMainMenuSelection,S2); // stay and change selection
    S2->addTransition(&openClockMenu,S4);
    S2->addTransition(&openAlarm1Menu,S5);
    S2->addTransition(&openAlarm2Menu,S6);
    S2->addTransition(&openSoundMenu,S7);
    S2->addTransition(&openLighthouseMenu,S8);
    S2->addTransition(&openBrigthnessMenu,S9);
    S2->addTransition(&openHomingMenu,S10);
    S2->addTransition(&openCreditsMenu,S11);
	//S2->addTransition(&startAlarm,S99); // check and start alarm
    //S2->addTransition(&anyButtonPressed,S2);   //Must be last item
    // from wedding mode
    S3->addTransition(&exitWeddingMode,S0);
    //Returns from all SubMenus (Timeout and pressed Buttons and alarms)
    //Clock Menu
    S4->addTransition(&returnToClockDisplay,S0); //After timeout
    S4->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&saveReturnToMainMenu,S2);
    S4->addTransition(&changeSubMenuSelection,S4);
    //S4->addTransition(&startAlarm,S99); // check and start alarm
    //Alarm1 Menu
    S5->addTransition(&returnToClockDisplay,S0); //After timeout
    S5->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&saveReturnToMainMenu,S2);
	//S5->addTransition(&startAlarm,S99); // check and start alarm    
    //Alarm2 Menu
    S6->addTransition(&returnToClockDisplay,S0); //After timeout
    S6->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&saveReturnToMainMenu,S2);
	//S6->addTransition(&startAlarm,S99); // check and start alarm
    //Sound Menu
    S7->addTransition(&returnToClockDisplay,S0); //After timeout
    S7->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&saveReturnToMainMenu,S2);
	//S7->addTransition(&startAlarm,S99); // check and start alarm
    //Volumne Menu
    S8->addTransition(&returnToClockDisplay,S0); //After timeout
    S8->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&saveReturnToMainMenu,S2);
	//S8->addTransition(&startAlarm,S99); // check and start alarm
    //Brightness Menu
    S9->addTransition(&returnToClockDisplay,S0); //After timeout
    S9->addTransition(&returnToMainMenu,S2);
    S7->addTransition(&saveReturnToMainMenu,S2);
	//S9->addTransition(&startAlarm,S99); // check and start alarm
    //Homing Menu
    S10->addTransition(&returnToClockDisplay,S0); //After timeout
    S10->addTransition(&anyButtonPressed,S2);
	//S10->addTransition(&startAlarm,S99); // check and start alarm
    //Credits Menu
    S11->addTransition(&returnToClockDisplay,S0); //After timeout
	S11->addTransition(&anyButtonPressed,S2);
	//S11->addTransition(&startAlarm,S99); // check and start alarm

	// Stop alarm and return to clock
	S99->addTransition(&stopAlarm,S2);
	S99->addTransition(&reAttachUnhandledInterrupts,S99);
	
    // TFT setup
    delay(500);
    tft.begin();
    tft.setTextColor(COLOR_TXT);
    tft.setRotation(3);
    tft.fillScreen(COLOR_BKGND);
    set_default_font();
    canvasClock.setFont(&FreeSans18pt7b);
    canvasClock.setTextSize(2);
    canvasDate.setFont(&FreeSans12pt7b);
    canvasDate.setTextSize(2);
    canvasMenuLR.setFont(&FreeSans18pt7b);
    canvasMenuLR.setTextSize(2);
    canvasTinyFont.setFont(&FreeSans12pt7b);
    canvasTinyFont.setTextSize(1);
    canvasMenuItem.setFont(&FreeSans18pt7b);
    canvasMenuItem.setTextSize(1);
    

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
    //myRTC.set(1631107034);
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

	// Read stored alarms from FRAM
	///////////////////////////////////
    readAlarms(ALARM1, alm1);
    readAlarms(ALARM2, alm2);
	
	// Set time value for alarms
	////////////////////////////////////
	// Note: isrTime must be correct!
	recalcAlarm(alm1);
	recalcAlarm(alm2);
	// Init Stepper
	//////////////////////////////////
    stepper.setStep(read16bit(C_STEP));
    
    // Buttons interrupt setup
    pinMode(BTN_C, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
    pinMode(BTN_R, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
    pinMode(BTN_L, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
    update_temperature();
}

void loop()
{
	// Time from RTC
    if (isrTimeChange) {
		// Get new time from RTC, sets isrTime
        updateClock();
		// Check if tower has to be moved to new posistion and command movement
		updateTowerLight(isrTime);
    }
    // Trigger new read from RTC
    if (millis() - rtcSyncTimer > rtcSyncDelay){
        rtcSyncTimer = millis();
        isrTimeChange = true;
        update_temperature();
    }
	// Time from DCF
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
        DCFSyncStatus = false;;
        DCFSyncChanged = true;
        Serial.println("Sync Failed");
    }
    if (dcfSync && DCF.getTime() != 0) {
        DCF.Stop();
        myRTC.set(DCF.getTime());
        digitalWrite(DCF_EN_PIN, HIGH);
        dcfSync = false;
        dcfSyncSucc = true;
        DCFSyncStatus = true;
        DCFSyncChanged = true;
        Serial.print("Sync Success");
        Serial.print(hour(isrTime));
        Serial.print(":");
        Serial.println(minute(isrTime));
    }
	// Run interface
	//////////////////////////
	// StateMachine
    machine.run();
	// Stepper
    if (moveTower) {
        stepper.run();
		// Is the stepper run finished (no stepps left)?
        if (stepperActive && stepper.getStepsLeft() == 0) {
            stepperActive = false;							// Not active any more
            stepper.off();									// TODO: Check if the stepper is switched on in newMoveDegreesCCW
        }
    }
}

void set_default_font(){
    tft.setFont(&FreeSans18pt7b);
    tft.setTextSize(1);
}

//StateMachine States
////////////////////////////////////////////////////////////
//S0 = main clock display
void stateClockDisplay()
{
    if (updateScreen) {
        updateScreen = false;
        updateClockSign = true;
        drawClockDisplayInfo();
        drawMenuSetter(1+4);
        tft.fillRect(0,MENU_TOPLINE_Y+TFT_MARGIN_TOP,TFT_WIDTH,MENULINEWIDTH,COLOR_TXT);
    }
    if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
    }
    if (DCFSyncChanged || tempChanged){
        drawClockDisplayInfo();
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
	if (millis() - alarmLightTimer > alarmLightDelay){ // Time is up...
		alarmLightOn = false; 						// Turn on light flag
		analogWrite(LED_MAIN, 0);					// Switch tower light
		digitalWrite(LED_BTN_C, LOW);				// Switch btn LEDs off
		digitalWrite(LED_BTN_R, LOW);				// Switch btn LEDs off
		digitalWrite(LED_BTN_L, LOW);				// Switch btn LEDs off
	}
}

void drawClockDisplayInfo(){
    tempChanged = false;
    DCFSyncChanged = false;
    
    int16_t ypos = TFT_HEIGHT - TFT_MARGIN_BOT - canvasTinyFont.height();
     
    tft.fillRect(0, ypos-MENULINEWIDTH, TFT_WIDTH, MENULINEWIDTH, COLOR_TXT);

    canvasTinyFont.fillScreen(COLOR_BKGND);
    canvasTinyFont.setCursor(0, FONTSIZE_TINY_HEIGHT+FONT_MARGIN);
    canvasTinyFont.print("LighthouseOS");
    tft.drawBitmap(CLOCKDISPLAY_LH_X+TFT_MARGIN_LEFT, ypos, canvasTinyFont.getBuffer(), canvasTinyFont.width(), canvasTinyFont.height(), COLOR_TXT, COLOR_BKGND);

    if (DCFSyncStatus){
        canvasTinyFont.fillScreen(COLOR_BKGND);
        canvasTinyFont.setCursor(0, FONTSIZE_TINY_HEIGHT+FONT_MARGIN);
        canvasTinyFont.print("DCF");
        tft.drawBitmap(CLOCKDISPLAY_SYNC_X+TFT_MARGIN_LEFT, ypos, canvasTinyFont.getBuffer(), canvasTinyFont.width(), canvasTinyFont.height(), COLOR_TXT, COLOR_BKGND);
    }
    canvasTinyFont.fillScreen(COLOR_BKGND);
    canvasTinyFont.setCursor(0, FONTSIZE_TINY_HEIGHT+FONT_MARGIN);
    sprintf(outString, "%d", (int)currentTemp);
    canvasTinyFont.print(outString);
    //canvasTinyFont.print((char)247); //Not supported by non standard Font
    canvasTinyFont.print("C");
    tft.drawBitmap(CLOCKDISPLAY_TEMP_X+TFT_MARGIN_LEFT, ypos, canvasTinyFont.getBuffer(), canvasTinyFont.width(), canvasTinyFont.height(), COLOR_TXT, COLOR_BKGND);
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
	// Continue showing the clock
	if (isrTimeUpdate) {
        isrTimeUpdate = false;
        clockDisplay(isrTime);
        analogWrite(TFT_LITE, brightness);
	}
	// Switch on and off the tower LED
	if (millis() - alarmLightTimer > alarmLightDelay) {
		alarmLightTimer = millis();								// Restart timer
		alarmLightOn = not alarmLightOn;						// Switch state of light
		analogWrite(LED_MAIN, ALARM_LIGHT_MAX * alarmLightOn);	// Switch tower light
	}
	// Move the tower in full revs
	if (stepperActive == false){								// Only command a new movemoent if nothing is running
		stepperActive	= true;									// Set flag to make sure no other movement is commanded
		stepper.newMoveDegreesCCW(360);							// Command one full rev
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
        drawMenuTop("Main Menu");
        updateMenuSelection = true;
        tft.fillCircle(MENU_SEL_X,TFT_MARGIN_TOP+MENU_SEL_Y,MENU_SEL_SIZE,COLOR_TXT);
    }
    if (updateMenuSelection){
        cursorY = cursorYMenuStart;
        for (int i=0; i<MENU_ITEMS; i++){
            sprintf(outString, "Item %d: %d", i, ((selectedMMItem+MAINMENU_ITEMS-1+i)%MAINMENU_ITEMS));
            Serial.println(outString);
            canvasMenuItem.fillScreen(COLOR_BKGND);
            canvasMenuItem.setCursor(0,FONT_MARGIN + FONTSIZE_SMALL_HEIGHT);
            canvasMenuItem.println(mainMenuEntries[(selectedMMItem+MAINMENU_ITEMS-1+i)%MAINMENU_ITEMS]);
            tft.drawBitmap(TFT_MARGIN_LEFT+MENU_ITEMS_X, cursorY, canvasMenuItem.getBuffer(), canvasMenuItem.width(), canvasMenuItem.height(), COLOR_TXT, COLOR_BKGND);
            cursorY = cursorY + canvasMenuItem.height();
        }
    }
}

void drawMenuTop(char *menuName)
{
    // Draw Top Icons
    drawMenuSetter(2+4);
    cursorY = TFT_MARGIN_TOP+MENU_TOPLINE_Y;
    tft.fillRect(0,cursorY,TFT_WIDTH,MENULINEWIDTH,COLOR_TXT);
    cursorY = cursorY + MENULINEWIDTH+FONT_MARGIN+FONTSIZE_SMALL_HEIGHT+2;
    // Write Center Entry
    tft.getTextBounds(menuName, 0, cursorY, &x1, &y1, &w1, &h1);
    cursorX = (uint8_t) ((TFT_WIDTH - TFT_MARGIN_LEFT - TFT_MARGIN_RIGHT - w1) / 2);
    tft.setCursor(cursorX, cursorY);
    tft.println(menuName);      
    tft.fillRect(0,y1+h1+6,TFT_WIDTH,MENULINEWIDTH,COLOR_TXT); 
    cursorYMenuStart = y1+h1+6+MENULINEWIDTH+6;
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
        updateMenuSelection = true;
    }
    if (updateMenuSelection){
        updateMenuSelection = false;
        cursorY = cursorYMenuStart;
        cursorX = CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT;
        uint8_t underline = 0;
        if (updateScreen || submenu.selectedItem==0 || submenu.selectedItem==1){
            if (submenu.selectedItem==0 || submenu.selectedItem==1){underline=submenu.selectedItem+1;}
            drawClock(canvasMenuItem, submenu.item[0], submenu.item[1], cursorX, cursorY, underline);      
        }
        cursorY = cursorY + canvasMenuItem.height();
        if (updateScreen || submenu.selectedItem==2 || submenu.selectedItem==3 || submenu.selectedItem==4){
            if (submenu.selectedItem==2 || submenu.selectedItem==3 || submenu.selectedItem==4){
                underline=submenu.selectedItem-1;
                }       
            updateCanvasDate(canvasMenuItem, submenu.item[2], submenu.item[3], submenu.item[4], underline);
            tft.drawBitmap(cursorX, cursorY, canvasMenuItem.getBuffer(), canvasMenuItem.width(), canvasMenuItem.height(), COLOR_TXT, COLOR_BKGND);
         }
        cursorY = cursorY + canvasMenuItem.height();/*
        if (updateScreen || submenu.selectedItem==5){
            canvasMenuItem.fillScreen(COLOR_BKGND);
            canvasMenuItem.setCursor(0, canvasMenuItem.height()-FONT_MARGIN-MENULINEWIDTH);
            canvasMenuItem.println(clockOptions[submenu.item[5]]);
            if (submenu.selectedItem==5){
                canvasMenuItem.getTextBounds(clockOptions[submenu.item[5]], 0, canvasMenuItem.height()-FONT_MARGIN-MENULINEWIDTH, &x1, &y1, &w1, &h1);
                canvasMenuItem.fillRect(x1, y1+h1+1, w1, MENULINEWIDTH, COLOR_TXT);
            }
            tft.drawBitmap(cursorX, cursorY, canvasMenuItem.getBuffer(), canvasMenuItem.width(), canvasMenuItem.height(), COLOR_TXT, COLOR_BKGND);
            //updateCanvasText(canvasMenuItem, soundOptions[submenu.item[5]],
        }*/


        updateScreen = false;
    }
    
    //Manually set time
    //Set Light Interval
    //Set Motor Interval
    //clockDisplay(isrTime);
}


void drawAlarmMenu(){

}

//S5
void stateAlarm1Menu()
{
    if (updateScreen) {
        updateScreen = false;        
        drawMenuTop("Alarm 1 Menu");
        updateMenuSelection = true;
        tft.fillCircle(MENU_SEL_X,TFT_MARGIN_TOP+MENU_SEL_Y,MENU_SEL_SIZE,COLOR_TXT);
    }
    drawAlarmMenu();
    //Set Alarm1
    //Select Alarm Sound
    //Select if lights should be on
}

//S6
void stateAlarm2Menu()
{
    if (updateScreen) {
        updateScreen = false;        
        drawMenuTop("Alarm 2 Menu");
        updateMenuSelection = true;
        tft.fillCircle(MENU_SEL_X,TFT_MARGIN_TOP+MENU_SEL_Y,MENU_SEL_SIZE,COLOR_TXT);
    }
    drawAlarmMenu();
    //Set Alarm2
    //Select Alarm Sound
    //Select if lights should be on
}

//S7
void stateSoundMenu()
{
    if (updateScreen) {
        updateScreen = false;
        drawNotImplementedMessage();
    }
    //Set Volume 0 - 30

}

//S8
void stateLighthouseMenu()
{
    if (updateScreen) {
        updateScreen = false;
        drawNotImplementedMessage();
    }
    //Lighthouse Settings for normal clock mode
    //Motor Settings
    //Main Light Settings
}

//S9
void stateBrightnessMenu()
{
    if (updateScreen) {
        updateScreen = false;
        drawNotImplementedMessage();
    }
    //Brightness Setting for Display
    //MainLED?
}

//S10
void stateHomingMenu()
{
    if (updateScreen) {
        updateScreen = false;
        drawNotImplementedMessage();
    }
    //Motor Homing Procedure
}

//S11
void stateCreditsMenu()
{
    if (updateScreen) {
        updateScreen = false;   
        tft.setFont(&FreeSans12pt7b);
        tft.setTextSize(1);
        tft.setCursor(0, cursorYMenuStart+FONTSIZE_TINY_HEIGHT);
        tft.println("  Not All Who Wander");
        tft.println("  Are Lost!");
        //tft.setFont();
        //tft.setTextSize(2);
        //tft.println(" ");
        tft.setFont(&FreeSans12pt7b);
        tft.setTextSize(1);
        tft.println("  Frauke, Christian, Hans &");
        tft.println("  Ben");
        set_default_font();
        //ToDo Turn on Main Lights and Motor
    }
}

//StateMachine Transitions
///////////////////////////////////////////////
bool toSleep()
{
    if (millis() - sleepTimer >  + sleepDelay) {
        analogWrite(TFT_LITE, 0);
        return true;
    }
    return false;
}

bool wakeup()
{
    if (isrButtonC||isrButtonR||isrButtonL) {
      // Following section uncommented to save pressed buttons after WakeUP
      //isrButtonC = false;
      //isrButtonR = false;
      //isrButtonL = false;
      sleepTimer = millis();
      isrTimeUpdate = true;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
      attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
      analogWrite(TFT_LITE, brightness);
      return true;
    }
    return false;
}

bool openMainMenu()
{
    if (isrButtonC) {
      isrButtonC = false;
      sleepTimer = millis();
      clearTFT();
      selectedMMItem = MAINMENU_ITEMS-1;
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
      return true;
    }
    return false;
}

bool changeMainMenuSelection()
{
    if(isrButtonR){
        isrButtonR = false;
        updateMenuSelection = true;
        selectedMMItem = (selectedMMItem+1)%MAINMENU_ITEMS;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        return true;
    }
    if(isrButtonL){
        isrButtonL = false;
        updateMenuSelection = true;
        selectedMMItem = (selectedMMItem+MAINMENU_ITEMS-1)%MAINMENU_ITEMS;
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        return true;
    }
    return false;
}

bool closeMainMenu()
{
    if (isrButtonC) {
      if (selectedMMItem == (MAINMENU_ITEMS-1)) {
          sleepTimer = millis();
          isrButtonC = false;
          clearTFT();
          prepareClock();
          isrTimeChange = true; // to enable clock display
          delay(50);
          attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
          return true;
          }
    }
    return false;
}

// Stop alarm when pressing L-Button
bool stopAlarm()
{
	// Stop-Alam button was pressed OR on timeout
	if ((isrButtonL)||(mills()-alarmTotalTimer > alarmTotalDelay)) {
      isrButtonL = false;		// Reset Btn-flag
      isrTimeUpdate = true;		// ??
      delay(50);
      attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
	  analogWrite(LED_MAIN, 0); 			// Switch off tower light
	  digitalWrite(LED_BTN_C, LOW);			// Switch btn LEDs off
	  digitalWrite(LED_BTN_R, LOW);			// Switch btn LEDs off
	  digitalWrite(LED_BTN_L, LOW);			// Switch btn LEDs off
	  
	  /////////////////////////////////////
	  // TODO
	  // Stop MP3-Player
	  /////////////////////////////////////
      return true;
    }
    return false;
}

// Start alarm when time has come
bool startAlarm()
{
	if (checkAlarms()) {
		digitalWrite(LED_BTN_C, HIGH);			// Switch btn LEDs on
		digitalWrite(LED_BTN_R, HIGH);			// Switch btn LEDs on
		digitalWrite(LED_BTN_L, HIGH);			// Switch btn LEDs on
		alarmTotalTimer = mills();			// Start timer
		/////////////////////////////////////
		// TODO
		// Start MP3-Player
		/////////////////////////////////////
		return true;
    }
	return false;
}


bool returnToClockDisplay()
{
    if (millis() - sleepTimer > sleepDelay){
        sleepTimer = millis();
        clearTFT();
        prepareClock();
        isrTimeChange = true; // to enable clock display
        return true;
    }   
    return false;
}

//////////////////////
// Begin Section for SubMenus
//
// Return without saving changes
bool returnToMainMenu()
{   
    //Return to main menu without saving
    if (isrButtonC  && (submenu.selectedItem == submenu.num_items)){
        sleepTimer = millis();
        isrButtonC = false;
        clearTFT();
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        Serial.println("Return to MM");
        return true;   
    }
    return false;
}

// Save changes to FRAM and apply to system + return
bool saveReturnToMainMenu()
{   
    //Save and return to main menu
    if (isrButtonC  && (submenu.selectedItem == (submenu.num_items+1))){
        sleepTimer = millis();
        isrButtonC = false;
        // Write alarms to FRAM
		writeAlarms(alm1);
		writeAlarms(alm2);
		// Update Alarms for next call
		recalcAlarm(alm1);
		recalcAlarm(alm2);
		// Do other stuff
        clearTFT();
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        Serial.println("Save and Return to MM");
        return true;   
    }
    return false;
}

bool openClockMenu()
{
    if (isrButtonC  && (selectedMMItem == 0)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openAlarm1Menu()
{
    if (isrButtonC  && (selectedMMItem == 1)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openAlarm2Menu()
{
    if (isrButtonC  && (selectedMMItem == 2)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openSoundMenu()
{
    if (isrButtonC  && (selectedMMItem == 3)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openLighthouseMenu()
{
    if (isrButtonC  && (selectedMMItem == 4)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openBrigthnessMenu()
{
    if (isrButtonC  && (selectedMMItem == 5)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openHomingMenu()
{
    if (isrButtonC  && (selectedMMItem == 6)){
        openSuBMenu();
        return true;   
    }
    return false;
}

bool openCreditsMenu()
{
    if (isrButtonC  && (selectedMMItem == 7)){
        openSuBMenu();
        return true;   
    }
    return false;
}

void updateSubMenuCenterIcon()
{
    if (submenu.selectedItem==(submenu.num_items+1)){
        Serial.println("Draw Setter 16");
        drawMenuSetter(16);
    }
    if (submenu.selectedItem==submenu.num_items){
        drawMenuSetter(8);
        Serial.println("8");
    }
    if (submenu.selectedItem<submenu.num_items){
        drawMenuSetter(4);
        Serial.println("4");
    }
}

void openSuBMenu()
{
    sleepTimer = millis();
    isrButtonC = false;
    // Init
    
    submenu.dateinfo_starts = -1;
    submenu.timeinfo_starts = -1;
    for (int i=0; i<SUBMENU_MAX_ITEMS; i++){
        submenu.maxVal[i] = 0;
        submenu.minVal[i] = 0;
    }
    switch (selectedMMItem){
        case 0:
            //ClockMenu
            submenu.num_items = 5;
            submenu.dateinfo_starts = 0;
            submenu.item[0] = hour(isrTime);
            submenu.item[1] = minute(isrTime);
            submenu.item[2] = day(isrTime);
            submenu.item[3] = month(isrTime);
            submenu.item[4] = year(isrTime);
            submenu.item[5] = clockOption;
            break;
        case 8:
            //Credits
            submenu.num_items = 0;
    }
    // Begin with selection of last item, which should be save and exit
    submenu.selectedItem = submenu.num_items+1;
    clearTFT();
    drawMenuTop(mainMenuEntries[selectedMMItem]);
    updateSubMenuCenterIcon();
    delay(50);
    attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
}

bool changeSubMenuSelection()
{
    if (isrButtonC){
        isrButtonC = false;
        submenu.selectedItem = (submenu.selectedItem + 1) % (submenu.num_items+2);  
        updateScreen = true;
        updateSubMenuCenterIcon();
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        return true;
    }
    if (isrButtonL || isrButtonR) {
        // Check if is one of last two items, then only decrement/increment selection
        if (submenu.selectedItem >= submenu.num_items){
            if (isrButtonL){
                isrButtonL = false;
                submenu.selectedItem = (submenu.selectedItem + 1) % (submenu.num_items+2);  
                delay(50);
                attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
            }
            else {
                isrButtonR = false;
                submenu.selectedItem = (submenu.selectedItem - 1 + submenu.num_items+2) % (submenu.num_items+2);  
                delay(50);
                attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
            }
            updateScreen = true;
            updateSubMenuCenterIcon();
            return true;
        }
        for (int i=0; i<SUBMENU_MAX_ITEMS; i++){
            submenu.prev_item[i] = submenu.item[i];
        }
        // Handle normal increment of currently selected value
        updateMenuSelection = true;
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
    
        // Correct date if enabled for this submenu
        if (submenu.dateinfo_starts >=0){
            correct_date(submenu.item + submenu.dateinfo_starts);
        }
        // Correct time if enabled for this submenu
        if (submenu.timeinfo_starts >=0){
            correct_time(submenu.item + submenu.timeinfo_starts);
        }
        // Check Max Value
        if (submenu.maxVal[submenu.selectedItem]){
            if (submenu.item[submenu.selectedItem] < submenu.minVal[submenu.selectedItem]){
                submenu.item[submenu.selectedItem] = submenu.maxVal[submenu.selectedItem];
            }
            submenu.item[submenu.selectedItem] = submenu.item[submenu.selectedItem] %  (submenu.maxVal[submenu.selectedItem]+1);
        }
        // Check Min Value
        else{
            if (submenu.item[submenu.selectedItem] < submenu.minVal[submenu.selectedItem]){
                submenu.item[submenu.selectedItem] = submenu.minVal[submenu.selectedItem];
            }
        }
        return true;
    }
    return false;
}

// Helper functions and wedding mode
///////////////////////////////
void correct_date(int16_t *date)
{
    // Check the date and correct based on calendar
    // date must be an array with 1st entry hour, than minute, day, month, year
    
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
    correct_time(date);
}

void correct_time(int16_t *dtime)
{
    // Check the time for over and underflows
    // date must be an array with 1st entry hour, than minute
    
    while (dtime[0] < 0){
        dtime[0] = dtime[0] + hours_per_day; 
    }
    dtime[0] = dtime[0] % hours_per_day;
    // Correct minute over and underflow
    while (dtime[1] < 0){
        dtime[1] = dtime[1] + 60; 
    }
    dtime[1] = dtime[1] % 60;
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

bool anyButtonPressed()
{
    bool buttonPressed = false;
    if(isrButtonR){
        isrButtonR = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_R), buttonR, FALLING);
        buttonPressed = true;
    }
    if(isrButtonL){
        isrButtonL = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_L), buttonL, FALLING);
        buttonPressed = true;
    }
    if(isrButtonC){
        isrButtonC = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
        buttonPressed = true;
    }
    if (buttonPressed){
        clearTFT();
        return true;
    }
}

bool reAttachUnhandledInterrupts()
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
    if(isrButtonC){
        isrButtonC = false;    
        delay(50);
        attachInterrupt(digitalPinToInterrupt(BTN_C), buttonC, FALLING);
    }
    return true;
}

// Clock and Display stuff
////////////////////////////////////////////////////////
void clearTFT()
{
    tft.fillScreen(COLOR_BKGND);
    updateScreen = true;
}

// reset internal clock variables
void prepareClock()
{
    clockHour = 255;
    clockMinute = 255;
    clockSecond = 255;
    clockDay = 255;
}

void updateClock()
{
    isrTimeChange = false;
    isrTimeUpdate = true;
    isrTime = myRTC.get();
    myRTC.alarm(ALARM_2);
}

void updateCanvasText(GFXcanvas1& cCanvas, char *text, bool underline)
{
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH);
    cCanvas.println(text);
    if (underline)
    {
        cCanvas.getTextBounds(text, 0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH, &x1, &y1, &w1, &h1);
        cCanvas.fillRect(x1, y1+h1+1, w1, MENULINEWIDTH, COLOR_TXT);
    }
}

void updateCanvasClock(GFXcanvas1& cCanvas, uint8_t cDigit, bool underline)
{
    sprintf(timeString,"%02d",cDigit);
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH);
    cCanvas.println(timeString);
    if (underline)
    {
        cCanvas.getTextBounds(timeString, 0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH, &x1, &y1, &w1, &h1);
        cCanvas.fillRect(x1, y1+h1+1, w1, MENULINEWIDTH, COLOR_TXT);
    }
}

void updateCanvasDate(GFXcanvas1& cCanvas, uint8_t cDay, uint8_t cMonth, uint16_t cYear, uint8_t underline)
{
    uint8_t cursorPos = cCanvas.height()-FONT_MARGIN-MENULINEWIDTH;
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cursorPos);
    sprintf(dateString,"%02d.%02d.%04d", cDay, cMonth, cYear);
    cCanvas.println(dateString);
    cCanvas.getTextBounds(dateString, 0, cursorPos, &x1, &y1, &w1, &h1);   
    if (underline == 0){
        return;
    }   
    if (underline & 1){
        // Underline day
        sprintf(dateString,"%02d", cDay);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1, y1+h2+1, w2, MENULINEWIDTH, COLOR_TXT); 
    }
    if (underline & 2){
        // Underline month
        int16_t w3;
        sprintf(dateString,"%02d", cMonth);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w3, &h2);
        sprintf(dateString,"%02d.%04d", cMonth, cYear);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1+w1-w2, y1+h2+1, w3, MENULINEWIDTH, COLOR_TXT);
    }      
    if (underline & 4){
        //underline year
        sprintf(dateString,"%04d", cYear);
        cCanvas.getTextBounds(dateString, 0, cursorPos, &x2, &y2, &w2, &h2);
        cCanvas.fillRect(x1+w1-w2, y1+h2+1, w2, MENULINEWIDTH, COLOR_TXT);
    }
    if (underline & 8){
        //underline all
        cCanvas.fillRect(x1, y1+h1+1, w1, MENULINEWIDTH, COLOR_TXT);
    }
}


void drawClock(GFXcanvas1& cCanvas, uint8_t cHour, uint8_t cMinute, int16_t xPos, int16_t yPos, uint8_t underline)
{     
    //Draw Hour
    updateCanvasClock(cCanvas, cHour, (bool)(underline & 1));
    drawClockItems(cCanvas, xPos, yPos, 0);
    //Manually Draw :
    cCanvas.fillScreen(COLOR_BKGND);
    cCanvas.setCursor(0, cCanvas.height()-FONT_MARGIN-MENULINEWIDTH);
    cCanvas.println(":");
    drawClockItems(cCanvas, xPos, yPos, 1);
    //Draw Minute
    updateCanvasClock(cCanvas, cMinute, (bool)(underline & 2));
    drawClockItems(cCanvas, xPos, yPos, 2);
}

void updateCanvasMenuSetter(int signtype)
{
    canvasMenuLR.fillScreen(COLOR_BKGND);
    canvasMenuLR.setCursor(0, CANVAS_MENULR_HEIGHT);
    
    switch (signtype)
    {
        case 0: canvasMenuLR.println('-');
        case 1: canvasMenuLR.println('+');
    }
}

void drawMenuSetter(uint8_t isOn)
{
    //
    if (isOn & 1){
        // Left Right Icons for Clock Display
        //TODO Replace with Hearts
        tft.fillCircle(CANVAS_MENUL_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, COLOR_RED);
        //tft.fillCircle(CANVAS_MENUL_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE-CENTER_ICON_LW, COLOR_BKGND);
        tft.fillCircle(CANVAS_MENUR_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, COLOR_BLUE);
        //tft.fillCircle(CANVAS_MENUR_X+TFT_MARGIN_LEFT+CANVAS_MENULR_ICONOFFSET, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE-CENTER_ICON_LW, COLOR_BKGND);
    }
    
    if (isOn & 2){
        // Left Right Icons for Menus
        updateCanvasMenuSetter(0);
        tft.drawBitmap(TFT_MARGIN_LEFT+CANVAS_MENUL_X, CANVAS_MENUL_Y+TFT_MARGIN_TOP, canvasMenuLR.getBuffer(), canvasMenuLR.width(), canvasMenuLR.height(), COLOR_TXT, COLOR_BKGND);
        updateCanvasMenuSetter(1);
        tft.drawBitmap(TFT_MARGIN_LEFT+CANVAS_MENUR_X, CANVAS_MENUR_Y+TFT_MARGIN_TOP, canvasMenuLR.getBuffer(), canvasMenuLR.width(), canvasMenuLR.height(), COLOR_TXT, COLOR_BKGND);
    }
    if (isOn & 4){
        // Center Icon
        canvasMenuItem.fillScreen(COLOR_BKGND);
        cursorX = CENTER_ICON_X+TFT_MARGIN_LEFT - (uint8_t)(canvasMenuItem.width()/2);
        cursorY = CENTER_ICON_Y+TFT_MARGIN_TOP - (uint8_t)(canvasMenuItem.height()/2);
        tft.drawBitmap(cursorX, cursorY, canvasMenuItem.getBuffer(), canvasMenuItem.width(), canvasMenuItem.height(), COLOR_TXT, COLOR_BKGND);
        tft.fillCircle(CENTER_ICON_X+TFT_MARGIN_LEFT, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE, COLOR_TXT);
        tft.fillCircle(CENTER_ICON_X+TFT_MARGIN_LEFT, CENTER_ICON_Y+TFT_MARGIN_TOP, CENTER_ICON_SIZE-CENTER_ICON_LW, COLOR_BKGND);
    }
    if ((isOn & 8) || (isOn & 16)){
            uint16_t tcolor; 
        if (isOn & 8){
            // Center Icon Save
            sprintf(outString, "Save");
            tcolor = COLOR_GREEN;
        }
        else {
            // Center Icon Cancel
            sprintf(outString, "Cancel");
            tcolor = COLOR_RED;
        }
        canvasMenuItem.fillScreen(COLOR_BKGND);
        canvasMenuItem.setCursor(0, canvasMenuItem.height()-FONT_MARGIN-MENULINEWIDTH);
        canvasMenuItem.getTextBounds(outString, 0, canvasMenuItem.height()-FONT_MARGIN-MENULINEWIDTH, &x1, &y1, &w1, &h1);
        canvasMenuItem.println(outString);        
        canvasMenuItem.fillRect(x1,canvasMenuItem.height()-FONT_MARGIN,w1,MENULINEWIDTH,tcolor);
        cursorX = CENTER_ICON_X+TFT_MARGIN_LEFT - (uint8_t)((x1+w1)/2);
        cursorY = CENTER_ICON_Y+TFT_MARGIN_TOP - (uint8_t)(canvasMenuItem.height()/2);
        tft.drawBitmap(cursorX, cursorY, canvasMenuItem.getBuffer(), canvasMenuItem.width(), canvasMenuItem.height(), tcolor, COLOR_BKGND);
    }
}

void drawClockItems(GFXcanvas1& cCanvas, int16_t xPos, int16_t yPos, uint8_t clock_item)
{
    switch (clock_item){
        case 0: 
            xPos = xPos; // Hour
            break;
        case 1:
            cCanvas.getTextBounds("88", 0, cCanvas.height(), &x1, &y1, &w1, &h1);
            xPos = xPos + x1 + w1 + FONT_MARGIN; // :-Sign
            break;
        case 2: 
            cCanvas.getTextBounds("88", 0, cCanvas.height(), &x1, &y1, &w1, &h1);
            cCanvas.getTextBounds(":", 0, cCanvas.height(), &x2, &y2, &w2, &h2);
            xPos = xPos  + x1 + w1 + x2 + w2 + 3*FONT_MARGIN;    // Minute
            break;
    }
    tft.drawBitmap(xPos, yPos, cCanvas.getBuffer(), cCanvas.width(), cCanvas.height(), COLOR_TXT, COLOR_BKGND);
}

void clockDisplay(time_t t)
{

    if (updateClockSign){
        updateClockSign = false;
        canvasClock.fillScreen(COLOR_BKGND);
        canvasClock.setCursor(0, canvasClock.height()-FONT_MARGIN-MENULINEWIDTH);
        canvasClock.println(":");
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_CLOCK_Y+TFT_MARGIN_TOP, 1);
       
    }
    if (clockHour != hour(t)){
        clockHour = hour(t);
        updateCanvasClock(canvasClock, clockHour, false);
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_CLOCK_Y+TFT_MARGIN_TOP, 0);
    }
    if (clockMinute != minute(t)){
        sprintf(outString, ("Moving stepper due to Minute: %02d, %02d", clockMinute, minute(t)));
        Serial.println(outString);
        clockMinute = minute(t);
        updateCanvasClock(canvasClock, clockMinute, false);
        drawClockItems(canvasClock, CLOCKDISPLAY_CLOCK_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_CLOCK_Y+TFT_MARGIN_TOP, 2);
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
        updateCanvasDate(canvasDate, clockDay, month(isrTime), year(isrTime), 0);
        tft.drawBitmap(CLOCKDISPLAY_DATE_X+TFT_MARGIN_LEFT, CLOCKDISPLAY_DATE_Y+TFT_MARGIN_TOP, canvasDate.getBuffer(), canvasDate.width(), canvasDate.height(), COLOR_TXT, COLOR_BKGND);
    }
}

void drawNotImplementedMessage(){
    tft.setFont();
    tft.setCursor(0, cursorYMenuStart);
    tft.setTextSize(2);
    tft.println("Hey Thomas, stellt sich raus du bist nicht einzige mit einem Deadline-Problem. Tja, stellt sich raus wir koennen es nicht loesen! Dieses Problem bleibt also bei dir! :-)");
    set_default_font();
}

// read temperature from RTC:
void update_temperature(){
    // Read temperature from RTC and update if necessary
    float newtemp = myRTC.temperature()/4; 
    if (! (newtemp==currentTemp)){
        currentTemp = newtemp;
        tempChanged = false;
    }
}

// Move tower light
void updateTowerLight(time_t t){
	uint16_t towerPos;
	uint16_t towerWay;
	towerPos = towerPosOffset + ( 15 * (uint16_t)hour(t)) + ((uint16_t)minute(t) / 15) * 3; 	// 360°/24h plus 3° per 1/4 h. Will change every 15min
	// Command new position if needed ...
	if ((moveTower)&&(stepperActive==false)&&(towerPos_last!=towerPos)) {		// Set point changed AND nothing is running		
		if (towerPos > towerPos_last){											// make sure the resulting way is positive
			towerWay = towerPos-towerPos_last;
		}else{
			towerWay = towerPos+360-towerPos_last;
		}
		stepper.newMoveDegreesCCW(towerWay);									// Command movemnt (relative! not absolute)
		towerPos_last = towerPos;												// Store changed pos value
		stepperActive = true;													// Set flag to make sure no other movement is commanded
	}
	
}

// Interrupt functions
//////////////////////////////////////////////////////

void isrChangeTime()
{
    isrTimeChange = true;
}

// Button interrupt flags
///////////////////////////////////////////////////////

void buttonC()
{
    detachInterrupt(digitalPinToInterrupt(BTN_C));
    isrButtonC = true;
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

// Alarm functions
//////////////////////////////////////////////////////
// Note: Saving changes in the alarms and recalculation the next alarm is done in saveReturnToMainMenu
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
    
    if (isrButtonC) {
      isrButtonC = false;
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
bool checkAlarms () {	
	// Only check once per min
	if (minute(isrTime)!= minute(isrTime_last)){
		isrTime_last = isrTime;								// Store time to see when the next minute comes
		return (checkAlarm(alm1) || checkAlarm(alm2));		// Check both alarms
	}
	return false;
}

// General alarm check and reset for next day
bool checkAlarm (struct alarms &alm){
	// Weekday:
	// 1: Son, 2: Mon, 3: Tue, 4: Wed, 5: Thr, 6: Fr, 7: Sa
	if (alm.mode > 0 && alm.nextAlarm >= isrTime){ // Alarm is triggered
		switch (alm.mode){
			case 0: return false; break;				// Off: Not possible
			case 1: alm.mode = 0; break; 				// Once: Switch off
			case 2: alm.nextAlarm += 60*60*24; break;	// Every day: Add 24h
			case 3: if (weekday(isrTime)<5) {			// Weekdays: So..Thr: Add 24h
						alm.nextAlarm += 60*60*24;
					}else if (weekday(isrTime)==6) {		// Weekdays: Fr: Add 3 x 24h
						alm.nextAlarm += 60*60*24*3;
					}else if (weekday(isrTime)==7) {		// Weekdays: Fr: Add 2 x 24h
						alm.nextAlarm += 60*60*24*2;
					}
					break;
			case 4: if (weekday(isrTime)==0) {				// Weekend: So: Add 6 x 24h
						alm.nextAlarm += 60*60*24;
					}else if (weekday(isrTime)==7) {		// Weekend: Sa: Add 24h
						alm.nextAlarm += 60*60*24;
					}
					break;
		}
		return true;
	}
	return false;
}

// Recalculate nextAlarm, e.g. after reading from FRAM or setting a new alarm in the menu
void recalcAlarm(struct alarms &alm){
	
	// 1st: Set alarm for today
	tmElements_t myElements = {0, alm.mm, alm.hh, weekday(isrTime), day(isrTime), month(isrTime), year(isrTime)-1970 };
	time_t alm.nextAlarm = makeTime(myElements);
	// 2nd: Check Alarm is in the past or not allowed today
	if (alm.mode<=2){ 						// Not related to weekdays
		if (alm.nextAlarm < isrTime){		// If alarm is in the past...
			alm.nextAlarm += 60*60*24;		// ... add 24h
		}
	}else{									// Alarm is related to weekdays
		// Weekday:
		// 1: Son, 2: Mon, 3: Tue, 4: Wed, 5: Thr, 6: Fr, 7: Sa
		switch (weekday(isrTime)){ 			// Check today:
			case 0: 						// Sunday
				if (alm.mode==3){			// Next weekday is Monday => add 24h
					alm.nextAlarm += 60*60*24;
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // if in the past, next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 6; // Add 6 days
					}
				}
			break;
			case 1: 						// Monday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past ... 
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 5; // Add 5 days
					}
				}
			break;
			case 2: 						// Tuesday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past ... 
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 4; // Add 4 days
					}
				}
			break;
			case 3: 						// Wednesday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past ... 
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 3; // Add 3 days
					}
				}
			break;
			case 4: 						// Thurday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past ... 
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24 * 2; // Add 2 days
					}
				}
			break;
			case 5: 						// Friday
				if (alm.mode==3){			// Weekday-alarm
					if (alm.nextAlarm < isrTime){ // if in the past, next alarm is on Mo 
						alm.nextAlarm += 60*60*24 * 3; // Add 3 days
					}
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // Next alarm is on Sa...
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}
			break;
			case 6: 						// Saturday
				if (alm.mode==3){			// Next weekday is Monday => add 2 days
					alm.nextAlarm += 60*60*24 *2;
				}else if (alm.mode==4){		// Weekend-alarm...
					if (alm.nextAlarm < isrTime){ // if in the past, next alarm is on So...
						alm.nextAlarm += 60*60*24; // Add 1 days
					}
				}
			break;
		}
	}

}
//FRAM functions
//////////////////////////////////////////////////////

void writeAlarms (uint16_t address, struct alarms alm)
{
    fram.write(address, alm.hh);
    fram.write(address+1, alm.mm);
    fram.write(address+2, alm.dd);
    fram.write(address+3, alm.act);
    fram.write(address+4, alm.light);
    fram.write(address+5, alm.soundfile);
	fram.write(address+6, alm.mode);
}


void readAlarms (uint16_t address, struct alarms &alm)
{
	// Note: nextAlarm is not sored. Must be calculated after reading the alarm
    alm.hh = fram.read(address);
    alm.mm = fram.read(address+1);
    alm.dd = fram.read(address+2);
    alm.act = fram.read(address+3);
    alm.light = fram.read(address+4);
    alm.soundfile = fram.read(address+5);
	alm.mode 	= fram.read(address+6);
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
