//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

//=====[Defines]===============================================================

#define NUMBER_OF_KEYS                         4
#define BLINKING_TIME_GAS_ALARM               1000
#define BLINKING_TIME_OVER_TEMP_ALARM          500
#define BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM  100
#define NUMBER_OF_AVG_SAMPLES                  100
#define OVER_TEMP_LEVEL                        50
#define TIME_INCREMENT_MS                      10
#define REPORT_INTERVAL_MS                     2000

//=====[Declaration and initialization of public global objects]===============

DigitalIn enterButton(BUTTON1);
DigitalIn alarmTestButton(D2);
DigitalIn aButton(D4);
DigitalIn bButton(D5);
DigitalIn cButton(D6);
DigitalIn dButton(D7);
DigitalIn mq2(PE_12);

DigitalOut alarmLed(LED1);
DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

DigitalInOut sirenPin(PE_10);

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn potentiometer(A0);
AnalogIn lm35(A1);

//=====[Declaration and initialization of public global variables]=============

bool alarmState      = OFF;
bool incorrectCode   = false;
bool overTempDetector = OFF;

int numberOfIncorrectCodes = 0;
int buttonBeingCompared    = 0;
int codeSequence[NUMBER_OF_KEYS]   = { 1, 1, 0, 0 };
int buttonsPressed[NUMBER_OF_KEYS] = { 0, 0, 0, 0 };
int accumulatedTimeAlarm = 0;
int reportingTimeAccumulator = 0;

bool gasDetectorState          = OFF;
bool overTempDetectorState     = OFF;

float potentiometerReading = 0.0;
float lm35ReadingsAverage  = 0.0;
float lm35ReadingsSum      = 0.0;
float lm35ReadingsArray[NUMBER_OF_AVG_SAMPLES];
float lm35TempC            = 0.0;

float tempThreshold = 20.0f;
float lastThreshold = 0.0f;
float sensitivity   = 0.0f;

//=====[Declarations (prototypes) of public functions]=========================

void inputsInit();
void outputsInit();
void alarmActivationUpdate();
void updateThreshold();
void uartTask();
void availableCommands();
bool areEqual();
float celsiusToFahrenheit( float tempInCelsiusDegrees );
float analogReadingScaledWithTheLM35Formula( float analogReading );
void continuousReportUpdate();

//=====[Main function]=========================================================

int main()
{
    inputsInit();
    outputsInit();
    
    // Initialize the temp array with current reading to avoid 0.0 average at start
    float initialRead = lm35.read();
    for(int i=0; i<NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsArray[i] = initialRead;
    }

    while (true) {
        alarmActivationUpdate();
        uartTask();
        continuousReportUpdate();
        updateThreshold();
        
        delay(TIME_INCREMENT_MS);
    }
}

//=====[Implementations of public functions]===================================

void inputsInit()
{
    alarmTestButton.mode(PullDown);
    aButton.mode(PullDown);
    bButton.mode(PullDown);
    cButton.mode(PullDown);
    dButton.mode(PullDown);
    sirenPin.mode(OpenDrain);
    sirenPin.input();
}

void outputsInit()
{
    alarmLed = OFF;
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
}

void continuousReportUpdate()
{
    reportingTimeAccumulator += TIME_INCREMENT_MS;

    if (reportingTimeAccumulator >= REPORT_INTERVAL_MS) {
        reportingTimeAccumulator = 0;
        
        char reportStr[150];
        float potVal = potentiometer.read() * 100;
        bool gasVal = mq2.read(); // 0 = Gas Detected, 1 = Normal
        
        sprintf(reportStr, "\r\n--- SYSTEM STATUS REPORT ---\r\n"
                           "Temp: %.2f C | Pot: %.2f | Gas: %s\r\n"
                           "Alarm State: %s\r\n"
                           "----------------------------\r\n",
                           lm35TempC, 
                           potVal, 
                           (!gasVal ? "!!! DETECTED !!!" : "Normal"),
                           (alarmState ? "ACTIVE" : "OFF"));
                           
        uartUsb.write(reportStr, strlen(reportStr));
    }
}

void alarmActivationUpdate()
{
    static int lm35SampleIndex = 0;

    // Moving average
    lm35ReadingsArray[lm35SampleIndex] = lm35.read();
    lm35SampleIndex++;
    if (lm35SampleIndex >= NUMBER_OF_AVG_SAMPLES) {
        lm35SampleIndex = 0;
    }

    lm35ReadingsSum = 0.0;
    for (int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        lm35ReadingsSum += lm35ReadingsArray[i];
    }
    lm35ReadingsAverage = lm35ReadingsSum / NUMBER_OF_AVG_SAMPLES;
    lm35TempC = analogReadingScaledWithTheLM35Formula(lm35ReadingsAverage);

    // Threshold check
    overTempDetector = (lm35TempC > tempThreshold) ? ON : OFF;

    // Trigger logic
    gasDetectorState      = !mq2;
    overTempDetectorState = overTempDetector;

    if (alarmTestButton) {
        gasDetectorState      = ON;
        overTempDetectorState = ON;
    }

    alarmState = (gasDetectorState || overTempDetectorState) ? ON : OFF;

    // Hardware output logic
    if (alarmState) {
        accumulatedTimeAlarm += TIME_INCREMENT_MS;
        sirenPin.output();
        sirenPin = LOW;

        if (gasDetectorState && overTempDetectorState) {
            if (accumulatedTimeAlarm >= BLINKING_TIME_GAS_AND_OVER_TEMP_ALARM) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if (gasDetectorState) {
            if (accumulatedTimeAlarm >= BLINKING_TIME_GAS_ALARM) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        } else if (overTempDetectorState) {
            if (accumulatedTimeAlarm >= BLINKING_TIME_OVER_TEMP_ALARM) {
                accumulatedTimeAlarm = 0;
                alarmLed = !alarmLed;
            }
        }
    } else {
        accumulatedTimeAlarm = 0;
        alarmLed = OFF;
        sirenPin.input();
    }
}

void uartTask()
{
    char receivedChar = '\0';
    char str[100];

    // Warnings based on sensor state
    if (overTempDetectorState) {
        uartUsb.write("Temperature Warning\r\n", 23);
    }
    if (gasDetectorState) {
        uartUsb.write("Gas Warning\r\n", 13);
    }

    if (uartUsb.readable()) {
        uartUsb.read(&receivedChar, 1);
        switch (receivedChar) {
            case '1':
                sprintf(str, "Alarm is %s\r\n", alarmState ? "ACTIVATED" : "NOT ACTIVATED");
                uartUsb.write(str, strlen(str));
                break;
            case '2':
                sprintf(str, "Gas is %s\r\n", !mq2 ? "BEING DETECTED" : "NOT DETECTED");
                uartUsb.write(str, strlen(str));
                break;
            case '3':
                sprintf(str, "Temp is %s limit\r\n", overTempDetector ? "ABOVE" : "BELOW");
                uartUsb.write(str, strlen(str));
                break;
            case 'c':
            case 'C':
                sprintf(str, "Temperature: %.2f C\r\n", lm35TempC);
                uartUsb.write(str, strlen(str));
                break;
            case 'p':
            case 'P':
                sprintf(str, "Potentiometer: %.2f\r\n", potentiometer.read());
                uartUsb.write(str, strlen(str));
                break;
            default:
                availableCommands();
                break;
        }
    }
}

void updateThreshold()
{
    // Read the raw pot value (0.0 to 1.0)
    float potReading = potentiometer.read();

    // Map the 0.0-1.0 range to 20-50 degrees
    // Offset is 20, Multiplier is 30 (the difference between 50 and 20)
    tempThreshold = 20.0f + (potReading * 30.0f);

    // To show the 0-100 "sensitivity" as well:
    float sensitivity = potReading * 100.0f;

    // Optional: Print the results
    if (fabs(tempThreshold - lastThreshold) > 0.5f) {
        char str[100];
        sprintf(str, "Sensitivity: %.0f%% | Threshold: %.1f C\r\n", sensitivity, tempThreshold);
        uartUsb.write(str, strlen(str));
        lastThreshold = tempThreshold;
    }
}

void availableCommands()
{
    uartUsb.write( "Available commands:\r\n", 21 );
    uartUsb.write( "1: Alarm State | 2: Gas State | 3: Temp State\r\n", 47 );
    uartUsb.write( "P: Pot Reading | C: Temp Reading\r\n\r\n", 35 );
}

bool areEqual()
{
    for (int i = 0; i < NUMBER_OF_KEYS; i++) {
        if (codeSequence[i] != buttonsPressed[i]) return false;
    }
    return true;
}

float analogReadingScaledWithTheLM35Formula( float analogReading )
{
    return ( analogReading * 3.3 / 0.01 );
}

float celsiusToFahrenheit( float tempInCelsiusDegrees )
{
    return ( tempInCelsiusDegrees * 9.0 / 5.0 + 32.0 );
}