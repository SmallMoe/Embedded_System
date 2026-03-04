#include "mbed.h"
#include "arm_book_lib.h"

// Tactile Switch (Button) setup
DigitalIn b0(D2, PullDown); DigitalIn b1(D3, PullDown);
DigitalIn b2(D4, PullDown); DigitalIn b3(D5, PullDown);
DigitalIn b4(D6, PullDown); DigitalIn b5(D7, PullDown);

DigitalOut led_warn(LED1);     // Blinks for warning and first minute of lockdown
DigitalOut led_lock(LED2);     // Solid ON during lockdown
BusOut event_log(D8, D9, D10); // Event log counter

int main() {
    int digit1, digit2, digit3, digit4; //four digit slot
    int count = 0; 
    int failed_attempts = 0;
    int lockdowns_triggered = 0;
    bool in_lockdown = false;
    int lockdown_timer = 0; 

    printf("System Active. Enter Password:\r\n");

//Assigning each button to specific digit 0 to 5 (rule i achieved)
    while (1) {
        int pressed = -1;
        if (b0 == 1) 
            pressed = 0; 
        if (b1 == 1) 
            pressed = 1;
        if (b2 == 1) 
            pressed = 2; 
        if (b3 == 1) 
            pressed = 3;
        if (b4 == 1) 
            pressed = 4; 
        if (b5 == 1) 
            pressed = 5;

//sliding user input into each digit slot
        if (pressed != -1) {
            count++;
            if (count == 1) digit1 = pressed;
            if (count == 2) digit2 = pressed;
            if (count == 3) digit3 = pressed;
            if (count == 4) digit4 = pressed;
            printf("Digit %d: %d\r\n", count, pressed);
            thread_sleep_for(300); 
        }
//After getting 4 digits, code checking the password 
        if (count == 4) {
            bool is_admin = (digit1==5 && digit2==5 && digit3==5 && digit4==5); //Admin code to override during lockdown
            bool is_user  = (digit1==1 && digit2==2 && digit3==3 && digit4==4); //User code

            if (in_lockdown) {
                //Only admin code can reset (Rule iv achieved)
                if (is_admin) {
                    printf("Admin Override Success!\r\n");
                    in_lockdown = false; //Escape out of lockdown
                    led_lock = 0; //Stops the LED lock
                    led_warn = 0; //Stops the warning LED
                    failed_attempts = 0; // Reset fails back to zero
                    lockdown_timer = 0;
                } else {
                    printf("Locked. Admin code required.\r\n");
                }
            } else {
                if (is_user) { 
                    printf("Access Granted.\r\n");
                    failed_attempts = 0;
                } else {
                    failed_attempts ++;
                    printf("Wrong Password! (Fail %d)\r\n", failed_attempts);

                    // 3 consecutive fails = 30s Warning (Rule ii achieved)
                    if (failed_attempts == 3) {
                        printf("WARNING: 30s Block Active.\r\n");
                        for (int i = 0; i < 10; i++) {
                            led_warn = !led_warn;
                            thread_sleep_for(500); // 60 * 500ms = 30s
                        }
                        led_warn = 0;
                        printf("Warning over. Next fail triggers Lockdown.\r\n");
                    } 
                    // 4th fail = Lockdown (Rule iii achieved)
                    else if (failed_attempts >= 4) {
                        printf("LOCKDOWN INITIATED.\r\n");
                        in_lockdown = true;
                        led_lock = ON; // Solid LED stays on
                        lockdowns_triggered++; // Increment log (Rule v achieved)
                        event_log = lockdowns_triggered; 
                        lockdown_timer = 0; 
                    }
                }
            }
            count = 0; 
        }

//1 minute LED blinking logic (Adjusted to 5 seconds for testing)
//Coded seperately for simplicity
        if (in_lockdown) {
            if (lockdown_timer < 500) { // 6000 loops * 10ms = 60s
                lockdown_timer++;
                if (lockdown_timer % 50 == 0) { // Toggle every 500ms
                    led_warn = !led_warn;
                }
            } else {
                led_warn = 0; // Stop blinking after (1 minute)
            }
        }
        thread_sleep_for(10); 
    }
}