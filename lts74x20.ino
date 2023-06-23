/*
 Logic tester for 74XX 14,16,20 pin ICs
 */

 #define VERSION " V. 0.2"

// Use debug when testing 14 or 16 pin chps. Uart pins (0,1) are
// used to test 20 pin chips - therefore disable debug
// when full 20 pin testing functionality is required.

#define SHOW_DEBUG 0

// Enable TEST_KEYS to check key/button values as read by ADC during start-up
#define TEST_KEYS 0

// Enable TEST_IO_EXPANDER to test IO expander pins P1 and P2 (P0 is not used)
#define TEST_IO_EXPANDER 0

// OLED DISPLAYS with silkscreened address 0x3C (0b111100) work with OLED_ADDRESS value set to 0x78 (0b1111000)
#define OLED_ADDRESS 0x78

// IO Expander I2C Address (PCA9557D), PINS A0,A1,A2 are all tied to GND
#define IO_ADDR 0x18


#include <EEPROM.h>

//index                           0  1  2  3  4  5  6    7   8          9  10  11  12  13  14  15  16  17
//IC pins                         1  2  3  4  5  6  7    8   9  GND-10 11  12  13  14  15  16  17  18  19  VCC-20
static const uint8_t pins[18]  = {4, 5, 6, 7, 8, 9, 10, 11, 13,         0,  1,  2,  3, A0, A1, A2, A3, 12};

// "E: test string is null!"
#define ERR_TEST_STRING_NULL 1

// "E: setPins: malformed config string!"
#define ERR_TEST_FORMAT 2

// "E: GND definition is incorrect!"
#define ERR_TEST_GND 3

// "E: VCC definition is incorrect!"
#define ERR_TEST_VCC 4

// "Test not found"
#define ERR_TEST_NOT_FOUND 5

uint8_t errors[16] = {0}; //index 0 holds number of errors
    
// the test data database
#include "ic_dbase.h"

// OLED screen drawing
#include "oled_ssd1306.h"

#define STATE_MENU 0
#define STATE_TEST 1
#define STATE_RESULT 2
#define STATE_PINOUT 3
#define STATE_SHOW_ERRORS 4
#define STATE_SCREEN_SAVER 5

#define PIN_KB     A6

#define PIN_VCC_DETECT A7

#define ADC_LEFT  874
#define ADC_UP    354
#define ADC_DOWN  532
#define ADC_RIGHT 710

#define KEY_BACK  1
#define KEY_UP    2
#define KEY_DOWN  3
#define KEY_OK    4

#define MENU_ITEMS 7

#define FILL_FLAG 0x80

// About 5 minutes (value 6000)
#define SCREEN_SAVER_TIMEOUT 6000


uint8_t state;
uint8_t key;

uint8_t icIndex = 0; // currently selected IC index in the list 
uint8_t topIndex = 0; // index of the first item displayed in the IC list

int8_t scrSaverX = -7;
int8_t scrSaverY = 8;
int8_t scrSaverIncX = 1;
int8_t scrSaverIncY = 1;

static uint8_t prevHighlight = 0;

static void handleTestResults(char* pinout, char* result, uint8_t errors);

// Load variables from EEPROM
static void loadVariables(void) {
    if (EEPROM.read(0) != 0x74 || EEPROM.read(1) != 0x1E) {
        topIndex = 0;
        icIndex = 0;
        return;
    }
    topIndex = (int8_t) EEPROM.read(2);
    icIndex = (int8_t) EEPROM.read(3);
}

// Save Variables to EEPROM
static void saveVariables(void) {
    EEPROM.update(0, 0x74);
    EEPROM.update(1, 0x1E);
    EEPROM.update(2, topIndex);
    EEPROM.update(3, icIndex);
}

// Simple error loggging to be displayed on the OLED screen/
static void addError(uint8_t e) {
    uint8_t errCount = errors[0];
    if (errCount < 14) {
      errCount++;
      errors[0] = errCount;
      errors[errCount] = e;
    }
}

// set IO Expaner P1 and P2
// 0: P1 and P2 to 0 - no GND on ZIF7 and ZIF8 pins
// 1: ZIF7 GND 
// 2: ZIF8 GND
static void setGnd(uint8_t gndPins) {
    uint8_t val = 0;
    if (gndPins) {
        val = (gndPins == 1) ? 0b100 : 0b10;
    }
    
    Wire.beginTransmission(IO_ADDR);
    // command 1: set output port 
    Wire.write(1);
    Wire.write(val); // P1 and P2 are outputs
    Wire.endTransmission();
}

// Set pins based on the test configuration string.
static void setPins(char* cfg, uint8_t pinCount) {
    uint8_t i, pinIndex;
    uint8_t p;
    uint8_t clockPin = 0xFF;
    uint8_t latchPin = 0xFF; // when latch pin is detected all '0' and '1' will be flipped after the latch is set
    uint8_t flipInput = 0;

    if (cfg == NULL) {
        addError(ERR_TEST_STRING_NULL);
        return;
    }


    //handle special flags at the start
    if (cfg[0] == 'F') {
        cfg++;
        flipInput = 1;
    }
    
    //sanity check
    if (strlen(cfg) < pinCount) {
        addError(ERR_TEST_FORMAT);
        return;
    }

#if SHOW_DEBUGx
    Serial.print("Testing: ");
    Serial.println(cfg);
#endif


    pinIndex = 0;
    for (i = 0; pinIndex < 18 && cfg[i] != 0; i++) { // do not process the last pin: VCC
        p = pins[pinIndex];
        switch (cfg[i]) {
            case 'C':
                clockPin = p;
                // fall through
            case 'D': //disable pin: low (no flip)
                //fall through
            case '0':
                pinMode(p, OUTPUT);
                digitalWrite(p, 0);
                pinIndex++;
                break;
            case 'A':
                latchPin = p; // latch pin is normally high then when all the rest is set it goes low
                //fall through
            case 'E': //enable pin: high (no flip)
                //fall through
            case '1':
                pinMode(p, OUTPUT);
                digitalWrite(p, 1);
                pinIndex++;
                break;
            case 'H':
            case 'Z':
            case '?':
                pinMode(p, INPUT);
                pinIndex++;
                break;
            case 'X':
                pinMode(p, INPUT);
                pinIndex++;
                break;
            case 'L': // if expecting low state then set a pull up to ensure the pin was actively driven low
            case 'U': //new state - maybe not required?
                pinMode(p, INPUT_PULLUP);
                pinIndex++;
                break;
            case  'G':
                if (pinCount == 14 && pinIndex == 6) {
                    pinIndex = 12;
                } else
                if (pinCount == 16 && pinIndex == 7) {
                    pinIndex = 11;
                } else
                if (!(pinCount == 20 && pinIndex == 9)) {
                    addError(ERR_TEST_GND);
                }
                break;
            case  'V':
                addError(ERR_TEST_VCC);
                break;
            // any other character is ignored
        }
    }

    //handle clock pin (assume it was set as output already)
    if (clockPin != 0xFF) {
        delayMicroseconds(100);
        digitalWrite(clockPin, 1);
        delayMicroseconds(100);
        digitalWrite(clockPin, 0);
    }

    // exit if there is no latch pin
    if (latchPin == 0xFF) {
        delayMicroseconds(100);
        return;
    }


    //handle latch pin (assume it was set as output high already)
    delayMicroseconds(100);
    digitalWrite(latchPin, 0);
    delayMicroseconds(100);


    // exit if flipping the input pins is not required
    if (!flipInput) {
        return;
    }

    //flip '0's and '1's. The expected outputs should not be changed as the latch was already set
    pinIndex = 0;
    for (i = 0; pinIndex < 18 && cfg[i] != 0; i++) { // do not process the last pin: VCC
        p = pins[pinIndex];
        switch (cfg[i]) {
            case '0':
            case '1':
                digitalWrite(p, '1' - cfg[i] );  //set the inverted value
                pinIndex++;
                break;

            case 'C':
            case 'A':
            case 'H':
            case 'Z':
            case '?':
            case 'X':
            case 'L': 
            case 'U': 
            case 'E': 
            case 'D': 
                pinIndex++;
                break;
            case  'G':
                if (pinCount == 14 && pinIndex == 6) {
                    pinIndex = 12;
                } else
                if (pinCount == 16 && pinIndex == 7) {
                    pinIndex = 11;
                } else
                if (!(pinCount == 20 && pinIndex == 9)) {
                    addError(ERR_TEST_GND);
                }
                break;
            case  'V':
                addError(ERR_TEST_VCC);
                break;
            // any other character is ignored
        }
    }
    delayMicroseconds(100);
    
}

// Test the results by reading pins marked as inputs (H or L or Z)
// returns number of failed pins
static unsigned char testPins(char* cfg, char* testResult, uint8_t pinCount) {
    unsigned char res = 0;
    unsigned char i, pinIndex;
    if (cfg == NULL) {
        addError(ERR_TEST_STRING_NULL);
        return 0xFF;
    }

    //handle special flags at the start
    if (cfg[0] == 'F') {
        cfg++;
    }
    
    //sanity check
    if (strlen(cfg) < pinCount) {
        addError(ERR_TEST_FORMAT);
        return 0xFF;
    }

    pinIndex = 0;
    for (i = 0; pinIndex < 18 && cfg[i] != 0; i++) { //do not test the last pin: VCC
        uint8_t p = pins[pinIndex];
        switch (cfg[i]) {
            case '0':
            case 'C':
            case '1':          
            case 'E':
            case 'D':
                pinIndex++;
                break;
            case 'H':
                if (digitalRead(p) != 1) {
                    res++;
                    testResult[i] = 'E';
                }
                pinIndex++;
                break;
            case 'L':
                if (digitalRead(p) != 0) {
                    res++;
                    testResult[i] = 'E';
                }
                pinIndex++;
                break;
            case 'Z':
                {
                    uint8_t x = 0;
#if SHOW_DEBUG                    
                    Serial.print("Test pin: ");
                    Serial.println(pinIndex);
#endif
                    while(x++ < 4){
                        pinMode(p, OUTPUT);
                        digitalWrite(p, 0); // force the pin Low
                        delayMicroseconds(20);
                        pinMode(p, INPUT_PULLUP); //weak internal pull up should raise the pin High
                        delayMicroseconds(100);
                        if (digitalRead(p) != 1) { //test the pin is High
                            res++;
                            testResult[i] = 'E';
                        }
                        digitalWrite(p, 1); // force the pin High
                        delayMicroseconds(20);
                        pinMode(p, INPUT); //weak external pull down should set the pin Low
                        delayMicroseconds(100);
                        if (digitalRead(p) != 0) { //test the pin is Low
                            res++;
                            testResult[i] = 'E';
                        }
                    }
                    pinIndex++;
                }
                break;
            case '?':
            case 'S':
                // TODO - implement ?
                pinIndex++;
                break;
            case 'X':
            case 'A':
                //pinIndex++;
                pinIndex++;
                break;
            case  'G':
                if (pinCount == 14 && pinIndex == 6) {
                    pinIndex = 12;
                } else
                if (pinCount == 16 && pinIndex == 7) {
                    pinIndex = 11;
                } else
                if (!(pinCount == 20 && pinIndex == 9)) {
                    addError(ERR_TEST_GND);
                }
                break;
            case  'V':
                addError(ERR_TEST_VCC);
                break;
            // any other character is ignored
        }
    }
    testResult[i] = 0;

#if SHOW_DEBUG
    if (res) {
        Serial.print("Errors: ");
        Serial.println(res);
    }
#endif
    return res;    
}

#if SHOW_DEBUG
static void printTestResult(unsigned char res, char* cfg) {
    if (res) {
        Serial.println(F("Test FAILED."));       
        Serial.print(cfg);
        Serial.print(F(" ("));
        Serial.print(res);
        Serial.println(F(" errors)"));
    }
#if 1
    else {
        Serial.println(F("Test PASSED."));
    }
#endif
}
#endif

// Decompress/decode one test string stored in flash.
static void decodeProgmemStr(char* dst, const char* src, uint16_t max) {
    uint16_t i, len;
    uint16_t pos = 0;
    char c;

    len = strlen_P(src);

    for (i = 0; i < len; i++) {
        char replace4 = 0;
        c = pgm_read_byte_near(src + i);
        if (c == '-') {
            replace4 = '0';
        } else
        if (c == '+') {
            replace4 = '1';
        } else
        if (c == '*') {
            replace4 = 'H';
        } else
        if (c == '/') {
            replace4 = 'L';
        } else
        if (c == '[') {
            replace4 = 'Z';
        }

        // replace 1 character by 4 characters
        if (replace4) {
            c = 4;
            while (c--) {
                dst[pos++] = replace4;
            }
        } else
        // replace 1 character by 2 characters
        if (c >= 'a' && c <= '}') {
            uint8_t ti = c - 'a';
            ti <<= 1;
            dst[pos++] = pgm_read_byte_near(decode_tokens + ti);
            dst[pos++] = pgm_read_byte_near(decode_tokens + ti + 1);
        } else {
            //copy the character
            dst[pos++] = c;
        }
        if (max && pos >= max) {
          i = len; //exit the loop
        }
    }
    dst[pos] = 0;

}

// Copies string stored in flash to a RAM buffer.
static void copyProgmemStr(char* dst, const char* src, uint8_t max) {
    uint16_t i, len;
    uint8_t fill = max & FILL_FLAG;

    max &= 0x7F; //remove FILL flag

    len = strlen_P(src);
    if (max && max < len) {
        len = max - 1;
    }
    for (i = 0; i < len; i++) {
        dst[i] = pgm_read_byte_near(src + i);
    }
    if (fill) {
        if (len < max) {
            while (i < max) dst[i++] = ' ';
        }
        dst[max] = 0;
    } else {
        dst[i] = 0;
    }
}

// Splits the test line into individual tests.
static int16_t splitTests(char* testLine, char** tests, uint8_t maxTest) {
    uint16_t i,t;

    if (NULL == testLine) {
        return 0;
    }
    i = 0;
    t = 0;
    tests[t] = testLine;
    while (testLine[i] != 0) {
        if (testLine[i] == ' ') {
            testLine[i] = 0;
            if (t < maxTest - 1) {
                t++;
                tests[t] = testLine + i + 1;
            }
        }
        i++;
    }
    return t + 1;
}


// Main testing function. Pass an index of the device to test.
static void performIcTest(int16_t i) {
    uint16_t err = 0;
    int16_t total;
    char cfg[530]; //Serial.println runs out of stack when more than 530 bytes are allocated
    char testResult[22];
    char* tests[32];

    // Display "TESTING ..." screen
    oled_clear_display();
    {
        char* buf = testResult;
        copyProgmemStr(buf, (const char*) F(" TESTING"), 0);
        oled_set_y(24);
        oled_draw_string(buf, false);

        copyProgmemStr(buf, (const char*) F("   ..."), 0);
        oled_set_y(64);
        oled_draw_string(buf, false);
    }

    // ensure the testResult is an empty string filled with space
    for (total = 0; total < 21; total++) {
        testResult[total] = 32;
    }
    testResult[21] = 0;


#if SHOW_DEBUG
    Serial.println(F("---[Testing]----------------------"));
#endif    
    
    if (i > 0) {
        uint8_t pinCount;
        uint8_t gndPins = 0;
        decodeProgmemStr(cfg, ic_tests[i], 528);

#if SHOW_DEBUG
        Serial.println(cfg);
#endif
        total = splitTests(cfg, tests, 32);
#if SHOW_DEBUG
        Serial.print("Found ");
        Serial.print(total);
        Serial.println(" tests.");
        for (i = 0; i < total; i++) {
            Serial.println(tests[i]);
        }
#endif        
        pinCount = strlen(tests[0]);
        // special character exists ?
        if (tests[0][0] == 'F') {
            pinCount--;
        }

        if (pinCount == 14) {
            gndPins = 1;
        } else
        if (pinCount == 16) {
            gndPins = 2;
        }
        setGnd(gndPins);
#if SHOW_DEBUG
        Serial.print(F("gndPulse"));
        Serial.println(gndPulse, DEC);
#endif

        // run all tests X times
        for (i = 0; i < 100; i++) {
            unsigned char t, r;
            unsigned char testCnt = 4;
            for (t = 0; t < total; t++) {
                setPins(tests[t], pinCount);
                r = testPins(tests[t], testResult, pinCount);
                //printTestResult(r, testResult);
                err += r;
            }
            if (err) { //errors detected
                i  = 100; //break from the loop
            }
        }
        //unset all pins
        for (i = 0; i < 18; i++) {
#if SHOW_DEBUG
           if (i != 9 && i != 10)
#endif          
            pinMode(pins[i], INPUT);
            delayMicroseconds(1);
        }
        setGnd(0); //no GND pins 
    } else {
        addError(ERR_TEST_NOT_FOUND);
    }

    // Fatal error occured: display error list
    if (errors[0]) {
        state = STATE_SHOW_ERRORS;
    }
    // Display test results
    else {
        handleTestResults(tests[0], testResult, err);
    }
}

// This is a debugging / troubleshooting version of the above.
// It runs only one test and waits 3 seconds so that the problematic
// pins can be scoped or measured by multimeter. After 3 seconds
// The pin settings are reset and the test is repeated.
// Note that you have to alter the test index.
static void performIcTest2(int16_t i) {
    uint16_t err = 0;
    int16_t total;
    char cfg[530]; //Serial.println runs out of stack
    char testResult[22];
    char* tests[32];
    char* buf = testResult;

    // Display "TESTING ..." screen
    oled_clear_display();
    {
        copyProgmemStr(buf, (const char*) F(" TESTING"), 0);
        oled_set_y(24);
        oled_draw_string(buf, false);

        copyProgmemStr(buf, (const char*) F("   .o."), 0);
        oled_set_y(64);
        oled_draw_string(buf, false);
    }

    // ensure the testResult is an empty string filled with space
    for (total = 0; total < 21; total++) {
        testResult[total] = 32;
    }
    testResult[21] = 0;

  
    
    if (i > 0) {
        uint8_t j;
        uint8_t pinCount;
        uint8_t gndPins = 0;
        decodeProgmemStr(cfg, ic_tests[i], 528);

#if SHOW_DEBUG
        Serial.println(cfg);
#endif
        total = splitTests(cfg, tests, 32);
#if SHOW_DEBUG
        Serial.print("Found ");
        Serial.print(total);
        Serial.println(" tests.");
        for (i = 0; i < total; i++) {
            Serial.println(tests[i]);
        }
#endif
        pinCount = strlen(tests[0]);

        if (pinCount == 14) {
            gndPins = 1;
        } else
        if (pinCount == 16) {
            gndPins = 2;
        }
        //setGnd(gndPins); // OLED off, GNDs on

        // run all tests X times
        for (i = 0; i < 100; i++) {
            unsigned char t, r;
            unsigned char testCnt = 4;

            setGnd(gndPins);
            delayMicroseconds(100);

            setPins(tests[2], pinCount); // <<=== here change the test index
            pinMode(pins[10], INPUT);
            delayMicroseconds(100);
            r  = digitalRead(pins[1]);
            delay(3000);
            //unset all pins
            for (j = 0; j < 18; j++) {
    #if SHOW_DEBUG
              if (j != 9 && j != 10)
    #endif          
                pinMode(pins[j], INPUT);
                delayMicroseconds(1);
            }
            setGnd(0); //no GND pins

            if (r == 0) {
               copyProgmemStr(buf, (const char*) F("    0"), 0);
            } else {
               copyProgmemStr(buf, (const char*) F("    1"), 0);
            }
            oled_set_y(64);
            oled_draw_string(buf, false);
        }
    }
}


// Checks whether particular button / key was pressed by comparing 
// analogue value with preset value.
static uint8_t isKbPressed(uint16_t r, uint16_t val) {
  return  (val - 10 < r && r < val + 10);
}

// Returns a key code of a pressed key or 0 if no key is pressed.
static uint8_t checkKb() {
   uint16_t r;
   r = analogRead(PIN_KB);
   if (isKbPressed(r, ADC_UP)) {
      return KEY_UP;
   }
   if (isKbPressed(r, ADC_DOWN)) {
      return KEY_DOWN;
   }
   if (isKbPressed(r, ADC_LEFT)) {
      return KEY_BACK;
   }
   if (isKbPressed(r, ADC_RIGHT)) {
      return KEY_OK;
   }
#if SHOW_DEBUG
#if 0
   else if (r > 20) {
     Serial.println(r,DEC);
   }
#endif
#endif
   return 0;
}


// Checks whether the power switch is on by checking an aanlogue
// value on VCC_DETECT pin.
static uint8_t checkPowerOn() {
    uint16_t r;
    r = analogRead(PIN_VCC_DETECT);
    if (r > 300) {
        return 1;
    } else {
        return 0;
    }
}

// Wait till no key is pressed
static void waitForKeyRelease() {
    while (checkKb()) delay(50);
}

// Show inital information and the version string.
void showSplashScreen() {
  oled_draw_h_line(0, 63, 23, 1);
  oled_set_y(24);
  oled_draw_string(" LTS74X20", true);

  oled_set_y(40);
  oled_draw_string("74X LOGIC", false);
  oled_set_y(50);
  oled_draw_string(" TESTER", false);

  oled_set_y(80);
  oled_draw_string(VERSION, false);
}

#if TEST_KEYS
// This function checks keyboard and prints pressed key values on the OLED screen.
// Use it if the keys do not seem to work.
static void testKeys() {
  char buf[12] = { 'K', 'E', 'Y', ':'};

  oled_clear_display();
  while (true) {
    uint16_t r;
    r = analogRead(PIN_KB);

    buf[4] = r > 1000 ? '0' + (r /1000) : ' ';
    r %= 1000;
    buf[5] = r >= 100 ? '0' + (r /100) : '0';
    r %= 100;
    buf[6] = r >= 10 ? '0' + (r /10) : '0';
    r %= 10;
    buf[7] = '0' + r;
    buf[8] = 0;        
    
    oled_set_y(24);
    oled_draw_string(buf, false);
    delay(100);
  }
}
#endif /* TEST_KEYS */


// Initialises GPIO Expander - sets P1 and P2 as output pins.
static void setupIoExpander() {
    Wire.beginTransmission(IO_ADDR);
    // set configuration
    Wire.write(3);
    Wire.write(0b11111001); // P1 and P2 are outputs
    Wire.endTransmission();  
}


#if TEST_IO_EXPANDER
// Tests the GPIO expaner by setting pins P1 and P2 high and low.
// Use it to verify the expaner functionality.
static void testIoExpander() {
    char buf[12] = { 'I', 'O', ':', ' ', 0};
    uint8_t val = 0;

    oled_clear_display();
    while (1) {
        Wire.beginTransmission(IO_ADDR);
        // set output port 
        Wire.write(1);
        Wire.write(val << 1); // P1 and P2 are outputs
        Wire.endTransmission();
        buf[3] = '0' + val;
        oled_set_y(24);
        oled_draw_string(buf, false);
        val++;
        val &= 0b11;
        delay(5000);
    }
}
#endif /* TEST_IO_EXPANDER */

// Initial Arduino setup.
void setup() {
    uint8_t i;

    analogReference(EXTERNAL);

    // Normally the Uart pins are used as GPIO pins
#if SHOW_DEBUG
    // initialize serial:
    Serial.begin(115200);
#endif

    // Initialise the OLED screen
    if (!oled_begin(OLED_ADDRESS)) {
#if SHOW_DEBUG
        Serial.println(F("OLED I2C failed"));
#endif
        while (1) delay(3000); //infinite loop: nothing to do if OLED is not working!
    }

    //unset all pins
    for (i = 0; i < 18; i++) {
#if SHOW_DEBUG
      if (i != 9 && i != 10)
#endif
        pinMode(pins[i], INPUT);
    }

    // prevent loading variables if BACK key is pressed during start
    if (KEY_BACK == checkKb()) {
        //initialise EEPROM contents - failsafe in case of EEPROM corruption
        saveVariables();
    } else {
        loadVariables();
    }

#if SHOW_DEBUG
    Serial.print(F("Power "));
    Serial.println(checkPowerOn(), DEC);
#endif


    showSplashScreen();
    delay(2000);
    state = STATE_MENU;
    key = 0;

    waitForKeyRelease();

#if TEST_KEYS
    testKeys();
#endif

    setupIoExpander();
    setGnd(0); // ensure GND transistors are disabled after reset
#if TEST_IO_EXPANDER
    testIoExpander();
#endif
}


#define FLAG_FULL_REDRAW 0b1
#define FLAG_ARROW_UP    0b10
#define FLAG_ARROW_DOWN  0b100


static void showIcList(uint8_t topIndex, uint8_t icIndex, uint8_t flags) {
    uint8_t i;
    char name[16] = {32, 32, 32, 32};
    uint8_t posY = 32;
#if SHOW_DEBUG
#if 0
    Serial.print("redraw ");
    Serial.print(topIndex);
    Serial.print(" icIndex ");
    Serial.println(icIndex);
#endif
#endif

    if (flags & FLAG_FULL_REDRAW) {
        oled_draw_h_line(0, 63, 7, 1);
        oled_set_y(8);
        oled_draw_string(" IC LIST ", true);
    }

    // draw UP / DOWN indicators

    name[5] = 0; //terminator
    if (flags & FLAG_ARROW_UP) {
        name[4] = '`';
    }
    oled_set_y(20);
    oled_draw_string(name, false); //draw arrow UP indicator

    if (flags & FLAG_ARROW_DOWN) {
        name[4] = 'a';
    } else {
        name[4] = ' ';
    }
    oled_set_y(116);
    oled_draw_string(name, false); //draw arrow Down indicator

    name[0] = 32;
    for (i = 0; i < MENU_ITEMS; i++) {
        uint16_t index = (topIndex + i);
        index <<= 1; //the IC name is every other text string
        //get the name from the flash memory
        copyProgmemStr(name + 1, ic_tests[index], FILL_FLAG | 8); //max 8 characters

        if ((topIndex +i) == icIndex) {
           //clear and redraw the higlighted line
          if (prevHighlight && prevHighlight != posY - 1) {
              oled_draw_h_line(0, 63, prevHighlight, 0);
              oled_draw_h_line(0, 63, prevHighlight + 8, 0);
              prevHighlight = posY - 1;
              oled_draw_h_line(0, 63, prevHighlight, 1);
          } else if (!prevHighlight){
              prevHighlight = posY - 1;
              oled_draw_h_line(0, 63, prevHighlight, 1);
          }       
          oled_set_y(posY);
          oled_draw_string(name, true);
        } else {
          oled_set_y(posY);
          oled_draw_string(name, false);
        }
        posY += 12;
    }

}

static void handleMenu() {
    uint8_t keyPressed = 0;
    uint8_t flags = FLAG_FULL_REDRAW;
    uint8_t maxIc = (sizeof(ic_tests) / sizeof(char*)) >> 1;
    uint16_t scrSaverCnt = 0;
    uint8_t cnt = 0;

    oled_clear_display();

    if (topIndex > 0) {
      flags |= FLAG_ARROW_UP;
    }
    if (topIndex + MENU_ITEMS < maxIc) {
      flags |= FLAG_ARROW_DOWN;
    }

    prevHighlight = 0;
    while (1) {
        showIcList(topIndex, icIndex, flags);
        keyPressed = 0;

        while (keyPressed == 0) {
            flags = 0;
            keyPressed = checkKb();
            
            if (keyPressed == KEY_UP) {
                if (icIndex > 0) {
                    icIndex --;
                    if (icIndex < topIndex) {
                        topIndex = icIndex;
                    }
                } else {
                  keyPressed = 0; // prevent redrawing
                }
                scrSaverCnt = 0;
            }
            if (keyPressed == KEY_DOWN) {
                if (icIndex < maxIc - 1) {
                    icIndex++;
                    if (icIndex >= topIndex + MENU_ITEMS) {
                      topIndex++;
                    }
                } else {
                  keyPressed = 0; // prevent redrawing
                }
                scrSaverCnt = 0;
            }
            // display pinout of the ic
            if (keyPressed == KEY_BACK) {
                saveVariables();
                waitForKeyRelease();
                state = STATE_PINOUT;
                return;
            } else
            if (keyPressed == KEY_OK) {
                saveVariables();
                waitForKeyRelease();
                state = STATE_TEST;
                return;
            }

            if (topIndex > 0) {
              flags |= FLAG_ARROW_UP;
            }
            if (topIndex + MENU_ITEMS < maxIc) {
              flags |= FLAG_ARROW_DOWN;
            }
            //Serial.println(keyPressed, DEC);
            // if no key is *currently* pressed: wait only a short time to quickly react to a new key press
            if (keyPressed == 0) {
                delay(10);
                // update screen saver counting
                cnt++;
                // every 50 ms
                if (cnt > 5) {
                    cnt = 0;
                    scrSaverCnt++;
                    if (scrSaverCnt > SCREEN_SAVER_TIMEOUT) {
                        state = STATE_SCREEN_SAVER;
                        return;
                    }
                }
            } else {
                // if there was no key *previously* pressed: wait a bit longer to navigate 1 item at a time
                delay(key == 0 ? 300 : 60);
            }
            key = keyPressed;
        }
    }
}

static char getPinLabel(char c) {
    if (c == '0' || c == '1') {
        return 'I'; //input
    }
    if (c == 'H' || c == 'L' || c == 'X') {
        return 'O'; // output
    }
    if (c == 'A') {
        return 'C'; // display lAtch pin as a Clock pin
    }
    return c;
}

#define COMP_FRAME 0b1
#define COMP_ERRORS 0b10

static void showPinout(char* pinout, char* testResult, uint8_t components) {
    char buf[10] = {0};
    int w = 40;
    int h;
    int y = 24;
    int x = 11;
    int i;
    uint8_t drawErrors = (testResult != NULL) && (components & COMP_ERRORS);
    uint8_t pinCnt;
    

    //handle special flags at the start of the pinout
    if (pinout[0] == 'F') {
        pinout++;
    }

    pinCnt = strlen(pinout) >> 1;

    if (pinCnt == 7) {
        y += 8;
    } else 
    if (pinCnt == 8) {
        y += 4;
    }
    h = (pinCnt + 1) * 9;

    if (components & COMP_FRAME) {
        // top notch (pun not intended :)
        buf[0] = buf[1] = buf[2] = buf[3] = 32;
        buf[4] = 'x'; // notch glyph
        buf[5] = 0;
        oled_set_y(y - 4);
        oled_draw_string(buf, false);

        //left and right vertical lines
        oled_draw_v_line(y, y + h, x);
        oled_draw_v_line(y, y + h, x + w);
    }

    buf[1] = 'b'; // left pin
    buf[7] = 'c'; // right pin
    buf[9] = 0;   // line terminator
    buf[3] = 32;
    buf[4] = 32;
    buf[5] = 32;

    for (i = 0; i < pinCnt; i++) {
        uint8_t drawLine = 1;
        buf[0] = 'd' + i; // squashed pin number - left
        buf[2] = getPinLabel(pinout[i]);
        buf[6] = getPinLabel(pinout[(pinCnt << 1) - i - 1]);
        buf[8] = 'd' + (pinCnt << 1) - i - 1; // squashed pin number - right

        if (drawErrors) {
            buf[3] = (testResult[i] == 'E') ? 'y' : 32;
            buf[5] = (testResult[(pinCnt << 1) - i - 1] == 'E') ? 'y' : 32;
            // draw error lines only
            if (buf[5] == 32 && buf[3] == 32) {
                drawLine = 0;
            }
        }

        if (drawLine) {
            oled_set_y(y + 6 + i * 9 );
            oled_draw_string(buf, false);
        }
    }

    if (components & COMP_FRAME) {
        //top horizontal lines
        oled_draw_h_line(x, x + 18, y, 1);
        oled_draw_h_line(x+22, x + w + 1, y, 1);
        //bottom horizontal line
        oled_draw_h_line(x, x + w + 1, y + h, 1);
    }
}

static void waitForKeyExitToMenu() {
    uint8_t keyPressed = 0;
    uint16_t cnt = 0;

    waitForKeyRelease();

    while (1) {
      keyPressed = checkKb();
            
      if (keyPressed) {
          waitForKeyRelease();
          // then exit to IC list
          state = STATE_MENU;
          return;
      }
      delay(50);
      cnt++;
      if (cnt > SCREEN_SAVER_TIMEOUT) {
          state = STATE_SCREEN_SAVER;
          return;
      }
    }
}

// Display a pinout screen and wait for a key press.
static void handlePinout() {
    char name[28] = {32, 32, 32, 32};
    char* tests[4] = {0, 0};
    uint16_t index = icIndex;
    uint8_t total;
    index <<= 1; //the IC name is every other text string

    oled_clear_display();

    //draw the IC name on the top
    //get the name from the flash memory
    copyProgmemStr(name + 1, ic_tests[index], FILL_FLAG | 8); //max 8 characters

    oled_draw_h_line(0, 63, 0, 1);
    oled_set_y(1);
    oled_draw_string(name, true);

    //extract the first test
    decodeProgmemStr(name, ic_tests[index + 1], 24); //up to 24 characters
#if SHOW_DEBUG
    Serial.print('"');
    Serial.print(name);
    Serial.println('"');
#endif  
    total = splitTests(name, tests, 4);
#if SHOW_DEBUG
    Serial.print("Found ");
    Serial.print(total);
    Serial.println(" tests.");
    Serial.println(tests[0]);
#endif
    showPinout(tests[0], NULL, COMP_FRAME);
    waitForKeyExitToMenu();
}

// Display a test result screen and then wait for
// a key press.
static void handleTestResults(char* pinout, char* result, uint8_t errors) {
    char text[10] = {32};
    uint16_t index = icIndex;
    uint16_t cnt = 0;

    index <<= 1; //the IC name is every other text string

    //draw the IC name on the top
    //get the name from the flash memory
    copyProgmemStr(text + 1, ic_tests[index], FILL_FLAG | 8); //max 8 characters

    oled_clear_display();

    oled_draw_h_line(0, 63, 0, 1);
    oled_set_y(1);
    oled_draw_string(text, true);  //inverse text

    if (errors) {
        uint8_t i;
        uint8_t show = 1;
        uint8_t components = COMP_FRAME;

        copyProgmemStr(text + 1, (const char*) F("FAILED!"), 8); //max 8 characters
        oled_set_y(12);
        oled_draw_string(text, false);

        while (show) {
            showPinout(pinout, result, components);
            i = 10;
            // wait 500 ms while checking for key press
            while (i) {
                delay(50);
                if (checkKb()) { // key pressed -> exit
                    show = 0;
                    break;
                }
                i--;
                // update screen saver counter
                if (++cnt > SCREEN_SAVER_TIMEOUT) {
                    state = STATE_SCREEN_SAVER;
                    return;
                }                
            }
            // blink/flash the errors
            if (components == COMP_ERRORS) {
                components = 0;
            } else {
                components = COMP_ERRORS;
            }
        }
#if SHOW_DEBUG
        Serial.println(F("exit"));
#endif        
    } else {
        //No errors detected
        copyProgmemStr(text + 1, (const char*) F("PASSED!"), 8); //max 8 characters
        oled_set_y(32);
        oled_draw_string(text, false);

        copyProgmemStr(text + 1, (const char*) F("CHIP IS"), 8); //max 8 characters
        oled_set_y(52);
        oled_draw_string(text, false);

        copyProgmemStr(text + 1, (const char*) F("  OK."), 8); //max 8 characters
        oled_set_y(64);
        oled_draw_string(text, false);

        //wait for a key press
        while(!checkKb()) {
            delay(50);
            if (++cnt > SCREEN_SAVER_TIMEOUT) {
                state = STATE_SCREEN_SAVER;
                return;
            }
        }
    }
    waitForKeyRelease();
    state = STATE_MENU;
}

static void handleTest() {
    int16_t index = icIndex;
    index <<= 1; //index to ic_tests array containing the test definition

    // check the Power switch is ON
    if (!checkPowerOn()) {
        uint8_t buf[10];
        uint16_t inactiveCnt = 0;;

        oled_clear_display();

        copyProgmemStr(buf, (const char*) F("TURN THE"), 0);
        oled_set_y(24);
        oled_draw_string(buf, false);

        copyProgmemStr(buf, (const char*) F("  POWER  "), 0);
        oled_set_y(44);
        oled_draw_string(buf, false);

        copyProgmemStr(buf, (const char*) F("SWITCH ON"), 0);
        oled_set_y(64);
        oled_draw_string(buf, false);

        while(!checkPowerOn()) {
            delay(50);
            if (inactiveCnt++ > 600) { // about 30 seconds
                state = STATE_MENU; //exit back to menu
                return;
            }
        }
    }

    performIcTest(index + 1);
}

static void handleErrorScreen() {
    uint8_t cnt = errors[0];
    uint8_t i;
    uint8_t posY = 0;
    char buf[10];
    //sanity check
    if (cnt == 0) {
        waitForKeyRelease();
        state = STATE_MENU; // no errors? go back to menu
        return;
    }
    errors[0] = 0;
    //sanity check
    if (cnt > 15) {
        cnt = 15;
    }

    oled_clear_display();
    
    oled_set_y(posY);
    copyProgmemStr(buf, (const char*) F("ERRORS:"), 0);
    oled_draw_string(buf, false);
    posY += 8;

    for (i = 0; i < cnt; i++) {
        oled_set_y(posY);
        switch (errors[i + 1]) {
            case ERR_TEST_STRING_NULL:
                copyProgmemStr(buf, (const char*) F("T:STR NUL"), 0);
                break;
            case ERR_TEST_FORMAT: 
                copyProgmemStr(buf, (const char*) F("T:STR FMT"), 0);
                break;
            case ERR_TEST_GND:
                copyProgmemStr(buf, (const char*) F("T:STR GND"), 0);
                break;
            case ERR_TEST_VCC:
                copyProgmemStr(buf, (const char*) F("T:STR VCC"), 0);
                break;
            case ERR_TEST_NOT_FOUND:
                copyProgmemStr(buf, (const char*) F("NO TESTS"), 0);
                break;
            default:
                copyProgmemStr(buf, (const char*) F("???"), 0);
                break;
        }
        oled_draw_string(buf, false);
        posY += 8;
    }
    waitForKeyExitToMenu();
}

static void handleScreenSaver() {
    char buf[10];
    uint8_t i, j;
    uint8_t prevY = scrSaverY;

    if (scrSaverX < 0) {
      oled_clear_display();
      scrSaverX = 4;
    }

    scrSaverX += scrSaverIncX;
    scrSaverY += scrSaverIncY;

    // bounce X
    if (scrSaverX > 8) {
        scrSaverX = 7;
        scrSaverIncX = -1;
    } else if (scrSaverX < 0) {
        scrSaverX = 1;
        scrSaverIncX = 1;
    }

    // bounce Y
    if (scrSaverY > 15) {
        scrSaverY = 14;
        scrSaverIncY = -1;
    } else if (scrSaverY < 0) {
        scrSaverY = 1;
        scrSaverIncY = 1;
    }

    for (i = 0; i < 9; i++) {
      buf[i] = 32;
    }
    buf[9] = 0;

    // clear old line
    oled_set_y(prevY * 8);
    oled_draw_string(buf, false);

    // draw new line
    buf[scrSaverX] = '*';
    oled_set_y(scrSaverY * 8);
    oled_draw_string(buf, false);

    if (checkKb()) {
        waitForKeyRelease();
        scrSaverX = -1; // next time clear the screen
        state = STATE_MENU;
    } else {
        delay(100);
    }
}

void loop() {
    switch (state) {
      case STATE_MENU:
        handleMenu();
        break;
      case STATE_TEST:
        handleTest();
        break;
      case STATE_PINOUT:
        handlePinout();
        break;
      case STATE_SHOW_ERRORS:
        handleErrorScreen();
        break;
      case STATE_SCREEN_SAVER:
        handleScreenSaver();
        break;
    }
}
