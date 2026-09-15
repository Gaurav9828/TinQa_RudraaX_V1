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

    // Tracking for Pad 1 (GP9) - AUTO / HYPERLAPSE
    uint32_t pad1PressStartTime = 0;
    bool pad1WasPressed = false;
    bool pad1LongPressTriggered = false;
    bool m_pad1SingleClickReady = false;
    bool m_pad1LongPressReady = false;

    // Tracking for Pad 2 (GP12) - SUNRISE / SUNSET
    uint32_t pad2PressStartTime = 0;
    bool pad2WasPressed = false;
    bool pad2LongPressTriggered = false;
    bool m_pad2SingleClickReady = false;
    bool m_pad2LongPressReady = false;

    // Global 1-second touch debounce lock
    uint32_t lastGlobalTouchTime = 0;

    static constexpr uint32_t LONG_PRESS_DURATION_MS = 2000; // 2 Seconds
    static constexpr uint32_t GLOBAL_DEBOUNCE_MS     = 1000; // 1-Second cooldown post-trigger

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

    // --- Pad 1 (GP9) Press Duration Handler (AUTO / HYPERLAPSE) ---
        bool pad1Current = gpio_get(pins[0]);
        if (pad1Current && !pad1WasPressed) {
            pad1PressStartTime = now;
            pad1WasPressed = true;
            pad1LongPressTriggered = false;
        } 
        else if (pad1Current && pad1WasPressed) {
            if (!pad1LongPressTriggered && (now - pad1PressStartTime >= LONG_PRESS_DURATION_MS)) {
                if (now - lastGlobalTouchTime >= GLOBAL_DEBOUNCE_MS) {
                    pad1LongPressTriggered = true;
                    m_pad1LongPressReady = true;
                    lastGlobalTouchTime = now;
                    printf("[TOUCH TRIGGER] Pad 1 Long-Press Detected (2s)! (HYPERLAPSE)\n");
                }
            }
        } 
        else if (!pad1Current && pad1WasPressed) {
            uint32_t duration = now - pad1PressStartTime;
            pad1WasPressed = false;

            // Only fire short click if long press was NOT triggered during this press session
            if (!pad1LongPressTriggered && duration > Config::Touch::DEBOUNCE_MS && duration < LONG_PRESS_DURATION_MS) {
                if (now - lastGlobalTouchTime >= GLOBAL_DEBOUNCE_MS) {
                    m_pad1SingleClickReady = true;
                    lastGlobalTouchTime = now;
                    printf("[TOUCH TRIGGER] Pad 1 Short-Tap Confirmed! (AUTO MODE)\n");
                }
            }
        }

        // --- Pad 2 (GP12) Press Duration Handler (SUNRISE / SUNSET) ---
        bool pad2Current = gpio_get(pins[1]);
        if (pad2Current && !pad2WasPressed) {
            pad2PressStartTime = now;
            pad2WasPressed = true;
            pad2LongPressTriggered = false;
        } 
        else if (pad2Current && pad2WasPressed) {
            if (!pad2LongPressTriggered && (now - pad2PressStartTime >= LONG_PRESS_DURATION_MS)) {
                if (now - lastGlobalTouchTime >= GLOBAL_DEBOUNCE_MS) {
                    pad2LongPressTriggered = true;
                    m_pad2LongPressReady = true;
                    lastGlobalTouchTime = now;
                    printf("[TOUCH TRIGGER] Pad 2 Long-Press Detected (2s)! (SUNSET)\n");
                }
            }
        } 
        else if (!pad2Current && pad2WasPressed) {
            uint32_t duration = now - pad2PressStartTime;
            pad2WasPressed = false;

            if (!pad2LongPressTriggered && duration > Config::Touch::DEBOUNCE_MS && duration < LONG_PRESS_DURATION_MS) {
                if (now - lastGlobalTouchTime >= GLOBAL_DEBOUNCE_MS) {
                    m_pad2SingleClickReady = true;
                    lastGlobalTouchTime = now;
                    printf("[TOUCH TRIGGER] Pad 2 Short-Tap Confirmed! (SUNRISE)\n");
                }
            }
        }
    }

    // Pad 1 API
    bool wasPad1SingleClicked() {
        if (m_pad1SingleClickReady) {
            m_pad1SingleClickReady = false;
            return true;
        }
        return false;
    }

    bool wasPad1LongPressed() {
        if (m_pad1LongPressReady) {
            m_pad1LongPressReady = false;
            return true;
        }
        return false;
    }

    // Pad 2 API
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

    // Generic Pad Trigger with 1-second global debounce guard
    bool isPadPressed(int padIndex) {
        int idx = padIndex - 1;
        if (idx < 0 || idx >= 5) return false;

        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (gpio_get(pins[idx])) {
            if (now - lastGlobalTouchTime >= GLOBAL_DEBOUNCE_MS) {
                lastGlobalTouchTime = now;
                printf("[TOUCH TRIGGER] Pad %d (GP%u) Triggered!\n", padIndex, pins[idx]);
                return true;
            }
        }
        return false;
    }

    bool wasPad3Pressed() { return isPadPressed(3); }
    bool wasPad4Pressed() { return isPadPressed(4); }
    bool wasPad5Pressed() { return isPadPressed(5); }
};