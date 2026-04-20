#include "mbed.h"
#include "arm_book_lib.h"
#include "display.h"
#include "matrix_keypad.h"

//Defines
#define TIME_INCREMENT_MS      10
#define NUMBER_OF_AVG_SAMPLES  100

//Pins
AnalogIn lm35(A1);
DigitalIn mq2(PE_12);

//Global Variables
float lm35TempC = 0.0;
float lm35ReadingsArray[NUMBER_OF_AVG_SAMPLES];
int lm35SampleIndex = 0;

bool warningActive = false; 

int alarmDisplayCounter = 0; //for alarm

//Function
void updateDisplayOutput(char key);
void displayWarning();
void sensorsUpdate();
float analogReadingScaledWithTheLM35Formula(float analogReading);

//=====[Main]==================================================================
int main()
{   
    
    displayInit(DISPLAY_CONNECTION_I2C_PCF8574_IO_EXPANDER); //I2C connection
    matrixKeypadInit(TIME_INCREMENT_MS);

    // Initialize temperature buffer
    float initVal = lm35.read();
    for (int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsArray[i] = initVal;
    }

    displayCharPositionWrite(0,0);
    displayStringWrite("System Ready      ");

    while (true) {

        sensorsUpdate();
        displayWarning(); //checking warning before other function

        // IMPORTANT: returns key on RELEASE
        char keyReleased = matrixKeypadUpdate();

        if (keyReleased != '\0') {
            updateDisplayOutput(keyReleased);
        }

        alarmDisplayCounter += TIME_INCREMENT_MS; //for alarm every 5 seconds for testing purpose

        if (alarmDisplayCounter >= 5000) {  //When internal clock hits 5000 ms, 5s, this contidion comes alive

            char buffer[21];

            bool gasDetected = !mq2.read();
            bool overTemp = (lm35TempC > 28.0); //for testing purpose 28 Celsius is chosen

            if (gasDetected && overTemp) {
                sprintf(buffer, "ALARM: GAS+TEMP   ");
            }
            else if (gasDetected) {
                sprintf(buffer, "ALARM: GAS        ");
            }
            else if (overTemp) {
                sprintf(buffer, "ALARM: TEMP       ");
            }
            else {
                sprintf(buffer, "ALARM: NORMAL     ");
            }

            displayCharPositionWrite(0, 0); // first row to overwrite the matrix keypad command
            displayStringWrite(buffer);

            alarmDisplayCounter = 0; // reset
        }

        delay(TIME_INCREMENT_MS);
    }
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

    float avgReading = sum / NUMBER_OF_AVG_SAMPLES;
    lm35TempC = analogReadingScaledWithTheLM35Formula(avgReading);
}

//Display Logic

void updateDisplayOutput(char key)
{
    char buffer[21];

    if (key == '4') {
        // ---- Temperature Status ----
        if (lm35TempC > 30.0) {
            sprintf(buffer, "Temp: HIGH %.1fC   ", lm35TempC);
        } else {
            sprintf(buffer, "Temp: NORMAL %.1fC ", lm35TempC);
        }

        displayCharPositionWrite(0, 0);
        displayStringWrite(buffer);
    }
    else if (key == '5') {
        // ---- Gas Status ----
        bool gasDetected = !mq2.read(); // active LOW

        if (gasDetected) {
            sprintf(buffer, "Gas: DANGER        ");
        } else {
            sprintf(buffer, "Gas: NORMAL        ");
        }

        displayCharPositionWrite(0, 0);
        displayStringWrite(buffer);
    }
}

//Warning Display

void displayWarning()
{
    char buffer[21];

    bool gasDetected = !mq2.read();
    bool overTemp = (lm35TempC > 28.0);

    if (gasDetected || overTemp) {

        warningActive = true;

        if (gasDetected && overTemp) {
            sprintf(buffer, "WARNING: GAS+TEMP ");
        }
        else if (gasDetected) {
            sprintf(buffer, "WARNING: GAS      ");
        }
        else if (overTemp) {
            sprintf(buffer, "WARNING: TEMP     ");
        }

        displayCharPositionWrite(0, 3); // 4th row to not be overwrite anything
        displayStringWrite(buffer);
    } 
    else {

        if (warningActive) {
            displayCharPositionWrite(0, 3);
            displayStringWrite("                    "); // clear line
        }

        warningActive = false;
    }
}
//LM35 Conversion
float analogReadingScaledWithTheLM35Formula(float analogReading)
{
    // LM35: 10mV per °C
    // ADC: 0–1 → scaled to 3.3V
    return analogReading * 330.0;
}