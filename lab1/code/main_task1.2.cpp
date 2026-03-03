#include "mbed.h"

// Define the threads
Thread thread1;
Thread thread2;

void led2_thread() {
    DigitalOut led2(LED2);
    while (true) {
        led2 = !led2;
        ThisThread::sleep_for(250ms);
    }
}

void led3_thread() {
    DigitalOut led3(LED3);
    while (true) {
        led3 = !led3;
        ThisThread::sleep_for(1000ms);
    }
}

int main() {
    // Start the other threads
    thread1.start(led2_thread);
    thread2.start(led3_thread);

    // Main thread handles LED1
    DigitalOut led1(LED1);
    while (true) {
        led1 = !led1;
        ThisThread::sleep_for(500ms);
    }
}