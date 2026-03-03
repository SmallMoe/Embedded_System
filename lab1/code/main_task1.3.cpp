#include "mbed.h"

DigitalOut leds[] = {DigitalOut(LED1), DigitalOut(LED2), DigitalOut(LED3)};
// Arrary to work together with for loop

// Function to handle the 200ms on/off blink
void blink_led(int LED_no) {
    leds[LED_no] = 1;                  // Turn LED on
    ThisThread::sleep_for(200ms);      // Wait 200ms
    leds[LED_no] = 0;                  // Turn LED off
    ThisThread::sleep_for(200ms);      // Wait 200ms before next in sequence
}

int main() {
    while (true) {
        // Forward: LED1, then LED2
        for (int i = 0; i < 2; i++) {
            blink_led(i);
        }
        
        // Backward: LED3, then LED2
        for (int i = 2; i > 0; i--) {
            blink_led(i);
        }
    }
}