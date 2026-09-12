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

    // Long press and single click tracking for Pad 2 (GP12)
    uint32_t pad2PressStartTime = 0;
    bool pad2WasPressed = false;
    bool pad2LongPressTriggered = false;

    static constexpr uint32_t LONG_PRESS_DURATION_MS = 2000; // 2 seconds threshold
    static constexpr uint32_t DEBOUNCE_MS = 50;

public:
    void init() {
        printf("[TOUCH DRIVER] Initializing 5 Touch Pads...\n");
        for (int i = 0; i < 5; i++) {
            gpio_init(pins[i]);
            gpio_set_dir(pins[i], GPIO_IN);
            gpio_pull_down(pins[i]); 
            
            lastPinState[i] = gpio_get(pins[i]);
            printf("[TOUCH DRIVER] Pad %d (GP%u) configured | Initial Raw State: %s\n", 
                   i + 1, pins[i], lastPinState[i] ? "HIGH" : "LOW");
        }
        printf("[TOUCH DRIVER] Initialization Complete.\n");
    }

    void update() {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // Log raw state transitions
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

        // --- Pad 2 (GP12) Press Duration Handler ---
        bool pad2Current = gpio_get(pins[1]);

        if (pad2Current && !pad2WasPressed) {
            // Button just pressed down
            pad2PressStartTime = now;
            pad2WasPressed = true;
            pad2LongPressTriggered = false;
        } 
        else if (pad2Current && pad2WasPressed) {
            // Button is being held down
            if (!pad2LongPressTriggered && (now - pad2PressStartTime >= LONG_PRESS_DURATION_MS)) {
                pad2LongPressTriggered = true;
                m_pad2LongPressReady = true;
                printf("[TOUCH TRIGGER] Pad 2 Long-Press Detected (2s)! (SUNSET)\n");
            }
        } 
        else if (!pad2Current && pad2WasPressed) {
            // Button released
            uint32_t duration = now - pad2PressStartTime;
            pad2WasPressed = false;

            // Trigger single-click only if released before long-press threshold
            if (!pad2LongPressTriggered && duration > DEBOUNCE_MS && duration < LONG_PRESS_DURATION_MS) {
                m_pad2SingleClickReady = true;
                printf("[TOUCH TRIGGER] Pad 2 Short-Tap Confirmed! (SUNRISE)\n");
            }
        }
    }

    bool wasPad2SingleClicked() {
        if (m_pad2SingleClickReady) {
            m_pad2SingleClickReady = false;
            return true;
        }
        return false;
    }

    bool wasPad2LongPressed() {
        if (m_pad2LongPressReady) {
            m_pad2LongPressReady = false;
            return true;
        }
        return false;
    }

    bool isPadPressed(int padIndex) {
        int idx = padIndex - 1;
        if (idx < 0 || idx >= 5) return false;

        uint32_t now = to_ms_since_boot(get_absolute_time());

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
    bool wasPad3Pressed() { return isPadPressed(3); }
    bool wasPad4Pressed() { return isPadPressed(4); }
    bool wasPad5Pressed() { return isPadPressed(5); }

private:
    bool m_pad2SingleClickReady = false;
    bool m_pad2LongPressReady = false;
};