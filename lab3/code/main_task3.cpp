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

// Memory Boxes (Global Variables)
bool gaslatchState = false;   // Is the alarm "toggled" on?
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
    // 1. Check if the gas button is being pushed RIGHT NOW
    if (gasDetector == ON) {
        
        // 2. ONLY do something if this is a NEW push (the gate is not locked)
        if (GasToggleLatch == true) {
            
            // 3. FLIP THE LATCH: If it was OFF, make it ON. If it was ON, make it OFF.
            gaslatchState = !gaslatchState; 

            // 4. Update the LED and send the message ONCE
            if (gaslatchState == ON) {
                GasLED = ON;
                uartUsb.write("Gas Alarm Latch: ON\r\n", 21);
            } else {
                GasLED = OFF;
                uartUsb.write("Gas Alarm Latch: OFF\r\n", 22);
            }

            // 5. LOCK THE GATE: Tell the computer the button is down
            GasToggleLatch = false;
        }
    } 
    else {
        // 6. If you LET GO of the button, UNLOCK THE GATE for the next time
        GasToggleLatch = true;
    }
}

void ToggleTemp(){
    // 1. Check if the gas button is being pushed RIGHT NOW
    if (TempDetector == ON) {
        
        // 2. ONLY do something if this is a NEW push (the gate is not locked)
        if (tempToggleLatch == true) {
            
            // 3. FLIP THE LATCH: If it was OFF, make it ON. If it was ON, make it OFF.
            templatchState = !templatchState; 

            // 4. Update the LED and send the message ONCE
            if (templatchState == ON) {
                TempLED = ON;
                uartUsb.write("Over Temperature Alarm Latch: ON\r\n", 34);
            } else {
                TempLED = OFF;
                uartUsb.write("Over Temperature Alarm Latch: OFF\r\n", 35);
            }

            // 5. LOCK THE GATE: Tell the computer the button is down
            tempToggleLatch = false;
        }
    } 
    else {
        // 6. If you LET GO of the button, UNLOCK THE GATE for the next time
        tempToggleLatch = true;
    }
}

void GasStateSerial() {
    // 1. Is the button being pressed?
    if (GasStateButton == ON) {
        
        // 2. ONLY enter if we haven't already reported this press
        if (gasStateMsgSent == false) {
            
            if (gaslatchState == true) {
                uartUsb.write("Gas Alarm Active\r\n", 18);
            } else {
                uartUsb.write("Gas Alarm Clear\r\n", 17);
            }

            // 3. LOCK THE GATE so it doesn't repeat
            gasStateMsgSent = true;
        }
    } 
    else {
        // 4. When the finger is lifted, UNLOCK the gate for the next press
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

            // 3. LOCK THE GATE so it doesn't repeat
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
        // A tiny 20ms pause helps the computer not "stutter"
        GasStateSerial();
        TempStateSerial(); 
        clearAlarm();
        continuousMonitoring();
        thread_sleep_for(20); 
    }
}