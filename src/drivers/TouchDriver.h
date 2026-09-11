#pragma once
#include <stdio.h>
#include "pico/stdlib.h"
#include "config/AppConfig.h"

class TouchDriver {
private:
    const uint32_t pins[5] = {
        Config::Pins::TOUCH_PAD_1, // GP9
        Config::Pins::TOUCH_PAD_2, // GP12
        Config::Pins::TOUCH_PAD_3, // GP13
        Config::Pins::TOUCH_PAD_4, // GP14
        Config::Pins::TOUCH_PAD_5  // GP15
    };
    
    uint32_t lastTriggerTime[5] = {0};
    bool lastPinState[5] = {false};

public:
    void init() {
        printf("[TOUCH DRIVER] Initializing 5 Touch Pads...\n");
        for (int i = 0; i < 5; i++) {
            gpio_init(pins[i]);
            gpio_set_dir(pins[i], GPIO_IN);
            
            // Enable internal pull-down to prevent high-impedance floating pins
            gpio_pull_down(pins[i]); 
            
            lastPinState[i] = gpio_get(pins[i]);
            printf("[TOUCH DRIVER] Pad %d (GP%u) configured | Initial Raw State: %s\n", 
                   i + 1, pins[i], lastPinState[i] ? "HIGH" : "LOW");
        }
        printf("[TOUCH DRIVER] Initialization Complete.\n");
    }

    void update() {
        // Log raw state transitions to catch floating or sticking pins in real time
        for (int i = 0; i < 5; i++) {
            bool currentState = gpio_get(pins[i]);
            if (currentState != lastPinState[i]) {
                printf("[TOUCH RAW] Pad %d (GP%u) Raw Transition: %s -> %s\n", 
                       i + 1, pins[i], 
                       lastPinState[i] ? "HIGH" : "LOW", 
                       currentState ? "HIGH" : "LOW");
                lastPinState[i] = currentState;
            }
        }
    }

    bool isPadPressed(int padIndex) {
        int idx = padIndex - 1;
        if (idx < 0 || idx >= 5) {
            printf("[TOUCH ERROR] Invalid Pad Index requested: %d\n", padIndex);
            return false;
        }

        uint32_t now = to_ms_since_boot(get_absolute_time());

        // Active-HIGH capacitive touch check
        if (gpio_get(pins[idx])) {
            if (now - lastTriggerTime[idx] > 250) {
                lastTriggerTime[idx] = now;
                printf("[TOUCH TRIGGER] Pad %d (GP%u) Triggered!\n", padIndex, pins[idx]);
                return true;
            }
        }
        return false;
    }

    bool wasPad1Pressed() { return isPadPressed(1); }
    bool wasPad2Pressed() { return isPadPressed(2); }
    bool wasPad3Pressed() { return isPadPressed(3); }
    bool wasPad4Pressed() { return isPadPressed(4); }
    bool wasPad5Pressed() { return isPadPressed(5); }
};