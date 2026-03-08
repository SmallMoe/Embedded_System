#include "mbed.h"
#include "arm_book_lib.h"

// Pins
DigitalIn gasDetector(D2);
DigitalIn TempDetector(D3);
DigitalIn GasStateButton(D4);
DigitalIn TempStateButton(D5);
DigitalIn ClearAlarmButton(D6);
DigitalIn continuousMonitoringButton(D7);
DigitalOut GasLED(LED1);
DigitalOut TempLED(LED2);
UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

bool gaslatchState = false;  
bool gasStateMsgSent = false;
bool GasToggleLatch = true;

bool templatchState = false; 
bool tempToggleLatch = true; 
bool tempStateMsgSent = false; 

bool resetMsgSent = false;

bool monitoringActive = false;  
bool monitorToggleLatch = true; 
bool monitorMsgSent = false;

int monitoringCounter; 

void ToggleGas() {
    
    if (gasDetector == ON) {
        
        
        if (GasToggleLatch == true) {
            
            gaslatchState = !gaslatchState; 

            if (gaslatchState == ON) {
                GasLED = ON;
                uartUsb.write("Gas Alarm Latch: ON\r\n", 21);
            } else {
                GasLED = OFF;
                uartUsb.write("Gas Alarm Latch: OFF\r\n", 22);
            }

            GasToggleLatch = false;
        }
    } 
    else {

        GasToggleLatch = true;
    }
}

void ToggleTemp(){
  
    if (TempDetector == ON) {
        
        if (tempToggleLatch == true) {
            
            templatchState = !templatchState; 

            if (templatchState == ON) {
                TempLED = ON;
                uartUsb.write("Over Temperature Alarm Latch: ON\r\n", 34);
            } else {
                TempLED = OFF;
                uartUsb.write("Over Temperature Alarm Latch: OFF\r\n", 35);
            }

            tempToggleLatch = false;
        }
    } 
    else {

        tempToggleLatch = true;
    }
}

void GasStateSerial() {

    if (GasStateButton == ON) {

        if (gasStateMsgSent == false) {
            
            if (gaslatchState == true) {
                uartUsb.write("Gas Alarm Active\r\n", 18);
            } else {
                uartUsb.write("Gas Alarm Clear\r\n", 17);
            }

            gasStateMsgSent = true;
        }
    } 
    else {

        gasStateMsgSent = false;
    }
}

void TempStateSerial() {
    if (TempStateButton == ON) {
        
        if (tempStateMsgSent == false) {
            
            if (templatchState == true) {
                uartUsb.write("Temp Alarm Active\r\n", 19);
            } else {
                uartUsb.write("Temp Alarm Clear\r\n", 18);
            }

            tempStateMsgSent = true;
        }
    } 
    else {
        tempStateMsgSent = false;
    }
}

void clearAlarm() {
    if (ClearAlarmButton == ON) {
        if (resetMsgSent == false) {
        
            templatchState = OFF;
            gaslatchState = OFF;
            
            GasLED = OFF;
            TempLED = OFF;

            uartUsb.write("Alarms Reset\r\n", 14);
            
            resetMsgSent = true; 
        }
    } else {
        resetMsgSent = false; 
    }
}

void continuousMonitoring() {

    if (continuousMonitoringButton == ON) {
        if (monitorToggleLatch == true) {
            monitoringActive = !monitoringActive; 
            
            
            if (monitoringActive) {
                uartUsb.write("Continuous Monitoring: START\r\n", 30);
                monitoringCounter = 0; 
            }
            
            monitorToggleLatch = false; 
        }
    } else {
        monitorToggleLatch = true; 
    }

    if (monitoringActive == true) {
        monitoringCounter++;

        if (monitoringCounter >= 100) { // 100 * 20ms = 2 seconds
            uartUsb.write("--- Periodic Status ---\r\n", 25);
            
            if (gaslatchState) uartUsb.write("Gas: ACTIVE | ", 14);
            else uartUsb.write("Gas: CLEAR | ", 13);
            
            if (templatchState) uartUsb.write("Temp: ACTIVE\r\n", 14);
            else uartUsb.write("Temp: CLEAR\r\n", 13);

            monitoringCounter = 0;
        }
        
        monitorMsgSent = false; 
    } 
    
    else {
        monitoringCounter = 0;
        
        if (monitorMsgSent == false) {
            uartUsb.write("Continuous Monitoring turned off\r\n", 34);
            monitorMsgSent = true; 
        }
    }
}

int main() {
    // Setup pins
    gasDetector.mode(PullDown);
    TempDetector.mode(PullDown);
    GasStateButton.mode(PullDown);
    TempStateButton.mode(PullDown);
    ClearAlarmButton.mode(PullDown);
    continuousMonitoringButton.mode(PullDown);
    GasLED = OFF;
    TempLED = OFF; 

    while (true) {
        ToggleGas();
        ToggleTemp(); 
        GasStateSerial();
        TempStateSerial(); 
        clearAlarm();
        continuousMonitoring();
        thread_sleep_for(20); // A tiny 20ms pause helps the computer not "stutter"
    }
}