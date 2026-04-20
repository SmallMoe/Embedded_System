#include "mbed.h"
#include "arm_book_lib.h"
#include "display.h"
#include "matrix_keypad.h"

//Defines
#define TIME_INCREMENT_MS      10
#define NUMBER_OF_AVG_SAMPLES  100
#define CODE_LENGTH            5

//Pins
AnalogIn lm35(A1);
DigitalIn mq2(PE_12);
DigitalOut buzzer(PE_10);

//Global Variables
float lm35TempC = 0.0;
float lm35ReadingsArray[NUMBER_OF_AVG_SAMPLES];
int lm35SampleIndex = 0;

bool warningActive  = false;
bool alarmActive    = false;   // true  = buzzer on, awaiting code
bool alarmWasActive = false;   // used to detect rising edge of alarm

int alarmDisplayCounter = 0;

// Code entry
char enteredCode[CODE_LENGTH + 1] = "";
int  codeIndex = 0;
const char correctCode[] = "12345";

//Function Declarations
void updateDisplayOutput(char key);
void displayWarning();
void sensorsUpdate();
void handleCodeEntry(char key);
void showCodePrompt();
void activateAlarm();
void deactivateAlarm();
float analogReadingScaledWithTheLM35Formula(float analogReading);

//=====[Main]==================================================================
int main()
{
    displayInit(DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER);
    matrixKeypadInit(TIME_INCREMENT_MS);

    buzzer = ON;   // active-low: starts silent

    // Initialize temperature buffer
    float initVal = lm35.read();
    for (int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsArray[i] = initVal;
    }

    displayCharPositionWrite(0, 0); 
    displayStringWrite("System Ready      ");

    while (true) {

        sensorsUpdate();

        // ---- Alarm triggering logic ----
        bool gasDetected = !mq2.read();
        bool overTemp    = (lm35TempC > 28.0); //Threshold is 28 degree
        bool shouldAlarm = gasDetected || overTemp;

        if (shouldAlarm && !alarmActive) {

            activateAlarm();
        }

        // ---- Warning row (row 3) ----
        displayWarning();

        // ---- Keypad ----
        char keyReleased = matrixKeypadUpdate();

        if (keyReleased != '\0') {
            if (alarmActive) {
                handleCodeEntry(keyReleased);   // password mode
            } else {
                updateDisplayOutput(keyReleased); // normal sensor display
            }
        }

        // ---- regular alarm state
        alarmDisplayCounter += TIME_INCREMENT_MS;
        if (alarmDisplayCounter >= 5000) {

            // Only overwrite row 0 when alarm is NOT active
            // (during alarm, row 0 shows the code prompt)
            if (!alarmActive) {
                char buffer[21];
                if (gasDetected && overTemp) {
                    sprintf(buffer, "ALARM: GAS+TEMP   ");
                } else if (gasDetected) {
                    sprintf(buffer, "ALARM: GAS        ");
                } else if (overTemp) {
                    sprintf(buffer, "ALARM: TEMP       ");
                } else {
                    sprintf(buffer, "ALARM: NORMAL     ");
                }
                displayCharPositionWrite(0, 0);
                displayStringWrite(buffer);
            }

            alarmDisplayCounter = 0;
        }

        delay(TIME_INCREMENT_MS);
    }
}

//Alarm Activate / Deactivate

void activateAlarm()
{
    alarmActive = true;
    buzzer = OFF;            // active-low: sound the buzzer

    // Reset any previous code attempt
    codeIndex = 0;
    memset(enteredCode, 0, sizeof(enteredCode));

    // Show prompt on rows 0-2; row 3 is kept for warnings
    showCodePrompt();
    displayCharPositionWrite(0, 1);
    displayStringWrite("Enter Code to     ");
    displayCharPositionWrite(0, 2);
    displayStringWrite("Deactivate Alarm  ");
}

void deactivateAlarm()
{
    alarmActive = false;
    buzzer = ON;           

    displayCharPositionWrite(0, 0);
    displayStringWrite("Access Granted!   ");
    displayCharPositionWrite(0, 1);
    displayStringWrite("Alarm OFF         ");
    displayCharPositionWrite(0, 2);
    displayStringWrite("                  ");
}

//=====[Code Entry]============================================================

void showCodePrompt()
{
    char displayBuf[21];

    sprintf(displayBuf, "Code: ");
    for (int i = 0; i < CODE_LENGTH; i++) {
        displayBuf[6 + i] = (i < codeIndex) ? enteredCode[i] : '_'; // syntax note to self condition ? value_if_true : value_if_false;
    }
    displayBuf[11] = ' ';
    displayBuf[12] = ' ';
    displayBuf[13] = '\0';

    displayCharPositionWrite(0, 0);
    displayStringWrite(displayBuf);
}

void handleCodeEntry(char key)
{
    if (key == '#') {
        // ENTER — check code
        enteredCode[codeIndex] = '\0';

        if (codeIndex == CODE_LENGTH && strcmp(enteredCode, correctCode) == 0) {
            deactivateAlarm();
        } else {
            // Wrong — show message, reset, re-prompt
            displayCharPositionWrite(0, 0);
            displayStringWrite("Wrong Code!       ");
            displayCharPositionWrite(0, 1);
            displayStringWrite("Try Again         ");
            displayCharPositionWrite(0, 2);
            displayStringWrite("                  ");

            delay(1500);

            codeIndex = 0;
            memset(enteredCode, 0, sizeof(enteredCode));

            showCodePrompt();
            displayCharPositionWrite(0, 1);
            displayStringWrite("Enter Code to     ");
            displayCharPositionWrite(0, 2);
            displayStringWrite("Deactivate Alarm  ");
        }

    } else if (key == '*') {
        // Backspace / clear
        codeIndex = 0;
        memset(enteredCode, 0, sizeof(enteredCode));
        showCodePrompt();

    } else if (codeIndex < CODE_LENGTH) {
        // Digit — append and refresh display
        enteredCode[codeIndex] = key;
        codeIndex++;
        showCodePrompt();
    }
    // Extra digits beyond CODE_LENGTH are silently ignored
}

//Sensor Update

void sensorsUpdate()
{
    lm35ReadingsArray[lm35SampleIndex] = lm35.read();
    lm35SampleIndex = (lm35SampleIndex + 1) % NUMBER_OF_AVG_SAMPLES;

    float sum = 0.0;
    for (int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        sum += lm35ReadingsArray[i];
    }
    lm35TempC = analogReadingScaledWithTheLM35Formula(sum / NUMBER_OF_AVG_SAMPLES);
}

//Display Logic — normal mode, when alarm is not active

void updateDisplayOutput(char key)
{
    char buffer[21];

    if (key == '4') {
        if (lm35TempC > 30.0) {
            sprintf(buffer, "Temp: HIGH %.1fC   ", lm35TempC);
        } else {
            sprintf(buffer, "Temp: NORMAL %.1fC ", lm35TempC);
        }
        displayCharPositionWrite(0, 0);
        displayStringWrite(buffer);
    }
    else if (key == '5') {
        bool gasDetected = !mq2.read();
        sprintf(buffer, gasDetected ? "Gas: DANGER       " : "Gas: NORMAL       ");
        displayCharPositionWrite(0, 0);
        displayStringWrite(buffer);
    }
}

//Warning Display — row 3

void displayWarning()
{
    char buffer[21];
    bool gasDetected = !mq2.read();
    bool overTemp    = (lm35TempC > 28.0);

    if (gasDetected || overTemp) {
        warningActive = true;
        if (gasDetected && overTemp) {
            sprintf(buffer, "WARNING: GAS+TEMP ");
        } else if (gasDetected) {
            sprintf(buffer, "WARNING: GAS      ");
        } else {
            sprintf(buffer, "WARNING: TEMP     ");
        }
        displayCharPositionWrite(0, 3);
        displayStringWrite(buffer);
    } else {
        if (warningActive) {
            displayCharPositionWrite(0, 3);
            displayStringWrite("                    ");
        }
        warningActive = false;
    }
}

//LM35 Conversion

float analogReadingScaledWithTheLM35Formula(float analogReading)
{
    return analogReading * 330.0;
}
