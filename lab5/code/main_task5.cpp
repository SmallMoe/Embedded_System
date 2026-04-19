//Libraries

#include "mbed.h"

#include "arm_book_lib.h"

#include <cstring>

#include <cstdlib>

#include <cstdio>

#include <cmath>

//Defines

#define KEYPAD_NUMBER_OF_ROWS    4

#define KEYPAD_NUMBER_OF_COLS    4

#define DEBOUNCE_KEY_TIME_MS     40

#define TIME_INCREMENT_MS        10

#define NUMBER_OF_KEYS           4

#define EVENT_MAX_STORAGE        5

#define EVENT_NAME_MAX_LENGTH    14

//Declaration of public data types

typedef enum {

    MATRIX_KEYPAD_SCANNING,

    MATRIX_KEYPAD_DEBOUNCE,

    MATRIX_KEYPAD_KEY_HOLD_PRESSED

} matrixKeypadState_t;

typedef struct {

    time_t seconds;

    char typeOfEvent[EVENT_NAME_MAX_LENGTH];

} systemEvent_t;

systemEvent_t arrayOfStoredEvents[EVENT_MAX_STORAGE];

int eventsIndex = 0;

//Declaration and initialization of public global objects

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

DigitalOut keypadRowPins[KEYPAD_NUMBER_OF_ROWS] = {PB_3, PB_5, PC_7, PA_15};

DigitalIn keypadColPins[KEYPAD_NUMBER_OF_COLS]  = {PB_12, PB_13, PB_15, PC_6};

//Declaration and initialization of public global variables

int accumulatedDebounceMatrixKeypadTime = 0;

char matrixKeypadLastKeyPressed = '\0';

float lm35TempC = 0.0f;

float celsiusToFahrenheit( float tempInCelsiusDegrees );

float tempThreshold = 30.0f;

float lastThreshold = 0.0f;

bool alarmState = false;

char codeSequence[NUMBER_OF_KEYS] = {'1', '8', '0', '5'};

char enteredCode[NUMBER_OF_KEYS];

int keyCount = 0;

bool alarmLastState = false;

bool gasLastState = false;

bool tempLastState = false;

bool ICLastState = false;

bool SBLastState = false;

bool overTempDetector = false;

bool incorrectCode = false;

char matrixKeypadIndexToCharArray[] = {

    '1', '2', '3', 'A',

    '4', '5', '6', 'B',

    '7', '8', '9', 'C',

    '*', '0', '#', 'D',

};

matrixKeypadState_t matrixKeypadState;

//Declarations (prototypes) of public functions

void matrixKeypadInit();

void uartTask();

char matrixKeypadScan();

char matrixKeypadUpdate();

void updateTemperature();

void updateThreshold();

void updateAlarm();

void handleKeypadAlarm(char key);

void eventLogUpdate();

void systemElementStateUpdate( bool lastState,

                               bool currentState,

                               const char* elementName );



// Pins

AnalogIn lm35(A1);

AnalogIn pot(A0);

DigitalOut buzzer(PE_10);

DigitalIn mq2(PE_12);

DigitalOut alarmLed(LED1);

DigitalOut incorrectCodeLed(LED2);

DigitalOut systemBlockedLed(LED3);





//Main function



int main()

{

    matrixKeypadInit();

    char keyReleased;

    while (true) {

        uartTask();

        updateTemperature();

        updateThreshold();

        updateAlarm();

        eventLogUpdate();

        keyReleased = matrixKeypadUpdate();

        handleKeypadAlarm(keyReleased);

        delay(TIME_INCREMENT_MS);

    }

}

//Implementations of public functions

void handleKeypadAlarm(char key)

{

    // Only process keypad if the alarm is actually active

    if (!alarmState) {

        return;

    }

    if (key != '\0') {

        // 1. Handle Digit Input (0-9)

        if (key >= '0' && key <= '9') {

            if (keyCount < NUMBER_OF_KEYS) {

                enteredCode[keyCount] = key;

                keyCount++;

                // Visual feedback on Serial Monitor

                char str[20];

                sprintf(str, "Key pressed: %c\r\n", key);

                uartUsb.write(str, strlen(str));

            }

        }

        // 2. Handle Enter (#)

        else if (key == '#') {

            bool isCorrect = true;

            // Check if 4 digits were entered

            if (keyCount == NUMBER_OF_KEYS) {

                for (int i = 0; i < NUMBER_OF_KEYS; i++) {

                    if (enteredCode[i] != codeSequence[i]) {

                        isCorrect = false;

                    }

                }

                if (isCorrect) {

                    uartUsb.write("Code Correct. Alarm Deactivated.\r\n", 34);

                    alarmState = false;

                } else {

                    uartUsb.write("Incorrect Code. Try again.\r\n", 28);

                }

            } else {

                uartUsb.write("Please enter 4 digits first.\r\n", 30);

            }

            // Reset buffer for next attempt

            keyCount = 0;

        }

    }

}

void updateAlarm()

{

    bool gasDetected = (mq2 == 0);

    // Check if current temperature exceeds the pot threshold or the gas is detected

    if ( (lm35TempC > tempThreshold) || gasDetected ) {

        if (!alarmState) {

            uartUsb.write("!!! ALARM TRIGGERED !!!\r\n", 26);

            if (gasDetected) uartUsb.write("Reason: Gas/Smoke detected\r\n", 28);

            if (lm35TempC > tempThreshold) uartUsb.write("Reason: Temperature limit exceeded\r\n", 36);

        }

        alarmState = true;

    }

    // Drive the hardware based on the state

    if ( alarmState ) {

        buzzer = OFF;

        alarmLed = ON;

    } else {

        buzzer = ON;

        alarmLed = OFF;

    }

}

void updateThreshold()

{

    char str[50];

    // 1. Read and Calculate

    float potValue = pot.read();

    tempThreshold = 25.0f + floor(potValue * 17.0f);

    // 2. Only print if the value has changed by more than 0.1 degrees

    // This prevents "flickering" text on your screen

    if ( fabs(tempThreshold - lastThreshold) > 0.1f ) {

        sprintf(str, "New Threshold: %.1f C\r\n", tempThreshold);

        uartUsb.write(str, strlen(str));

        lastThreshold = tempThreshold; // Update the record

    }

}



void matrixKeypadInit()

{

    matrixKeypadState = MATRIX_KEYPAD_SCANNING;

    int pinIndex = 0;

    for( pinIndex=0; pinIndex<KEYPAD_NUMBER_OF_COLS; pinIndex++ ) {

        (keypadColPins[pinIndex]).mode(PullUp);

    }

}



void uartTask()

{

    char receivedChar = '\0';

    char str[100];

    int stringLength;

    if( uartUsb.readable() ) {

        uartUsb.read( &receivedChar, 1 );

        switch (receivedChar) {

        // --- Celsius Case ---

        case 'c':

        case 'C':

            // lm35TempC should be updated by your sensor reading function

            sprintf ( str, "Temperature: %.2f C\r\n", lm35TempC );

            stringLength = strlen(str);

            uartUsb.write( str, stringLength );

            break;

        // --- Fahrenheit Case ---

        case 'f':

        case 'F':

            // Uses the helper function to convert the current Celsius reading

            sprintf ( str, "Temperature: %.2f F\r\n",

                      celsiusToFahrenheit( lm35TempC ) );

            stringLength = strlen(str);

            uartUsb.write( str, stringLength );

            break;

        case 's':

        case 'S': {

            struct tm rtcTime;

            int strIndex;

            uartUsb.write( "\r\nType four digits for the current year (YYYY): ", 48 );

            for( strIndex=0; strIndex<4; strIndex++ ) {

                uartUsb.read( &str[strIndex] , 1 );

                uartUsb.write( &str[strIndex] ,1 );

            }

            str[4] = '\0';

            rtcTime.tm_year = atoi(str) - 1900;

            uartUsb.write( "\r\n", 2 );

            uartUsb.write( "Type two digits for the current month (01-12): ", 47 );

            for( strIndex=0; strIndex<2; strIndex++ ) {

                uartUsb.read( &str[strIndex] , 1 );

                uartUsb.write( &str[strIndex] ,1 );

            }

            str[2] = '\0';

            rtcTime.tm_mon  = atoi(str) - 1;

            uartUsb.write( "\r\n", 2 );

            uartUsb.write( "Type two digits for the current day (01-31): ", 45 );

            for( strIndex=0; strIndex<2; strIndex++ ) {

                uartUsb.read( &str[strIndex] , 1 );

                uartUsb.write( &str[strIndex] ,1 );

            }

            str[2] = '\0';

            rtcTime.tm_mday = atoi(str);

            uartUsb.write( "\r\n", 2 );

            uartUsb.write( "Type two digits for the current hour (00-23): ", 46 );

            for( strIndex=0; strIndex<2; strIndex++ ) {

                uartUsb.read( &str[strIndex] , 1 );

                uartUsb.write( &str[strIndex] ,1 );

            }

            str[2] = '\0';

            rtcTime.tm_hour = atoi(str);

            uartUsb.write( "\r\n", 2 );

            uartUsb.write( "Type two digits for the current minutes (00-59): ", 49 );

            for( strIndex=0; strIndex<2; strIndex++ ) {

                uartUsb.read( &str[strIndex] , 1 );

                uartUsb.write( &str[strIndex] ,1 );

            }

            str[2] = '\0';

            rtcTime.tm_min  = atoi(str);

            uartUsb.write( "\r\n", 2 );

            uartUsb.write( "Type two digits for the current seconds (00-59): ", 49 );

            for( strIndex=0; strIndex<2; strIndex++ ) {

                uartUsb.read( &str[strIndex] , 1 );

                uartUsb.write( &str[strIndex] ,1 );

            }

            str[2] = '\0';

            rtcTime.tm_sec  = atoi(str);

            uartUsb.write( "\r\n", 2 );



            rtcTime.tm_isdst = -1;

            set_time( mktime( &rtcTime ) );

            uartUsb.write( "Date and time has been set\r\n", 28 );

            break;

        }

        case 't':

        case 'T': {

            time_t seconds = time(NULL);

            char str[50];

            sprintf(str, "Current Time: %s", ctime(&seconds));

            uartUsb.write(str, strlen(str));

            break;

        }



        case 'e':

        case 'E': {

            uartUsb.write("\r\n--- RECENT EVENTS ---\r\n\r\n", 28);

            for (int i = 0; i < EVENT_MAX_STORAGE; i++) {

                if (arrayOfStoredEvents[i].seconds != 0) { // Only show initialized slots

                    char str[100];

                    sprintf(str, "Event: %s | Time: %s",

                            arrayOfStoredEvents[i].typeOfEvent,

                            ctime(&arrayOfStoredEvents[i].seconds));

                    uartUsb.write(str, strlen(str));

                }

            }

            break;

        }



        // --- Help/Default Menu ---

        default:

            uartUsb.write( "Available commands:\r\n", 21 );

            uartUsb.write( "Press 'c' or 'C' for Celsius\r\n", 30 );

            uartUsb.write( "Press 'f' or 'F' for Fahrenheit\r\n", 33 );

            uartUsb.write( "Press 's' or 'S' to set the date and time\r\n", 43 );

            uartUsb.write( "Press 't' or 'T' to get the date and time\r\n", 43 );

            uartUsb.write( "Press 'e' or 'E' to get the stored events\r\n\r\n", 45 );

            break;

        }

    }

}



// Helper function for the conversion

float celsiusToFahrenheit( float tempInCelsiusDegrees )

{

    return ( tempInCelsiusDegrees * 9.0 / 5.0 + 32.0 );

}



char matrixKeypadScan()

{

    int row = 0;

    int col = 0;

    int i = 0;



    for( row=0; row<KEYPAD_NUMBER_OF_ROWS; row++ ) {



        for( i=0; i<KEYPAD_NUMBER_OF_ROWS; i++ ) {

            keypadRowPins[i] = ON;

        }



        keypadRowPins[row] = OFF;



        for( col=0; col<KEYPAD_NUMBER_OF_COLS; col++ ) {

            if( keypadColPins[col] == OFF ) {

                return matrixKeypadIndexToCharArray[row * KEYPAD_NUMBER_OF_COLS + col];

            }

        }

    }

    return '\0';

}

char matrixKeypadUpdate()
{
    char keyDetected = '\0';

    char keyReleased = '\0';


    switch (matrixKeypadState) {

        case MATRIX_KEYPAD_SCANNING:

            keyDetected = matrixKeypadScan();

            if (keyDetected != '\0') {

                matrixKeypadLastKeyPressed = keyDetected;

                accumulatedDebounceMatrixKeypadTime = 0;

                matrixKeypadState = MATRIX_KEYPAD_DEBOUNCE;

            }

            break;

        case MATRIX_KEYPAD_DEBOUNCE:

            accumulatedDebounceMatrixKeypadTime += TIME_INCREMENT_MS;

            if (accumulatedDebounceMatrixKeypadTime >= DEBOUNCE_KEY_TIME_MS) {

                keyDetected = matrixKeypadScan();

                if (keyDetected == matrixKeypadLastKeyPressed) {

                    matrixKeypadState = MATRIX_KEYPAD_KEY_HOLD_PRESSED;

                } else {

                    matrixKeypadState = MATRIX_KEYPAD_SCANNING;

                }

            }

            break;

        case MATRIX_KEYPAD_KEY_HOLD_PRESSED:

            keyDetected = matrixKeypadScan();

            if (keyDetected == '\0') {

                keyReleased = matrixKeypadLastKeyPressed;

                matrixKeypadState = MATRIX_KEYPAD_SCANNING;

            }

            break;

    }

    return keyReleased;

}



void updateTemperature()

{

    // Read A1 and convert to Celsius

    // (Reading * 3.3V) / 0.01V per degree

    lm35TempC = (lm35.read() * 3.3f) / 0.01f;

}



void eventLogUpdate()

{

    systemElementStateUpdate( alarmLastState, alarmState, "ALARM" );

    alarmLastState = alarmState;

    systemElementStateUpdate( gasLastState, !mq2, "GAS_DET" );

    gasLastState = !mq2;

    systemElementStateUpdate( tempLastState, overTempDetector, "OVER_TEMP" );

    tempLastState = overTempDetector;

    systemElementStateUpdate( ICLastState, incorrectCodeLed, "LED_IC" );

    ICLastState = incorrectCodeLed;

    systemElementStateUpdate( SBLastState, systemBlockedLed, "LED_SB" );

    SBLastState = systemBlockedLed;

}



void systemElementStateUpdate( bool lastState,

                               bool currentState,

                               const char* elementName )

{

    char eventAndStateStr[EVENT_NAME_MAX_LENGTH] = "";



    if ( lastState != currentState ) {



        strcat( eventAndStateStr, elementName );

        if ( currentState ) {

            strcat( eventAndStateStr, "_ON" );

        } else {

            strcat( eventAndStateStr, "_OFF" );

        }



        arrayOfStoredEvents[eventsIndex].seconds = time(NULL);

        strcpy( arrayOfStoredEvents[eventsIndex].typeOfEvent,eventAndStateStr );

        if ( eventsIndex < EVENT_MAX_STORAGE - 1 ) {

            eventsIndex++;

        } else {

            eventsIndex = 0;

        }

        uartUsb.write( eventAndStateStr , strlen(eventAndStateStr) );

        uartUsb.write( "\r\n", 2 );

    }

}
