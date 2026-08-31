#pragma once

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "../config/AppConfig.h"

class TouchDriver {
public:
    enum class TouchState {
        RELEASED,
        PRESSED,
        HELD
    };

    void init() {
        // Configure GPIO pins as digital inputs with pull-down resistors
        gpio_init(Config::Pins::TOUCH_PAD_1);
        gpio_set_dir(Config::Pins::TOUCH_PAD_1, GPIO_IN);
        gpio_pull_down(Config::Pins::TOUCH_PAD_1);

        gpio_init(Config::Pins::TOUCH_PAD_2);
        gpio_set_dir(Config::Pins::TOUCH_PAD_2, GPIO_IN);
        gpio_pull_down(Config::Pins::TOUCH_PAD_2);

        gpio_init(Config::Pins::TOUCH_PAD_3);
        gpio_set_dir(Config::Pins::TOUCH_PAD_3, GPIO_IN);
        gpio_pull_down(Config::Pins::TOUCH_PAD_3);

        gpio_init(Config::Pins::TOUCH_PAD_4);
        gpio_set_dir(Config::Pins::TOUCH_PAD_4, GPIO_IN);
        gpio_pull_down(Config::Pins::TOUCH_PAD_4);

        gpio_init(Config::Pins::TOUCH_PAD_5);
        gpio_set_dir(Config::Pins::TOUCH_PAD_5, GPIO_IN);
        gpio_pull_down(Config::Pins::TOUCH_PAD_5);

        pad1LastState = false;
        pad2LastState = false;
        pad3LastState = false;
        pad4LastState = false;
        pad5LastState = false;
    }

    // Call this once per main loop iteration to update edge detection
    void update() {
        bool pad1Current = gpio_get(Config::Pins::TOUCH_PAD_1);
        bool pad2Current = gpio_get(Config::Pins::TOUCH_PAD_2);
        bool pad3Current = gpio_get(Config::Pins::TOUCH_PAD_3);
        bool pad4Current = gpio_get(Config::Pins::TOUCH_PAD_4);
        bool pad5Current = gpio_get(Config::Pins::TOUCH_PAD_5);

        // Falling edge -> Rising edge trigger for Pad 1
        pad1JustPressed = pad1Current && !pad1LastState;
        pad1LastState = pad1Current;

        // Falling edge -> Rising edge trigger for Pad 2
        pad2JustPressed = pad2Current && !pad2LastState;
        pad2LastState = pad2Current;

        // Falling edge -> Rising edge trigger for Pad 3
        pad3JustPressed = pad3Current && !pad3LastState;
        pad3LastState = pad3Current;

        // Falling edge -> Rising edge trigger for Pad 4
        pad4JustPressed = pad4Current && !pad4LastState;
        pad4LastState = pad4Current;

        // Falling edge -> Rising edge trigger for Pad 5
        pad5JustPressed = pad5Current && !pad5LastState;
        pad5LastState = pad5Current;

    }

    // Raw instantaneous state queries
    bool isPad1Touched() const {
        return gpio_get(Config::Pins::TOUCH_PAD_1);
    }

    bool isPad2Touched() const {
        return gpio_get(Config::Pins::TOUCH_PAD_2);
    }

    bool isPad3Touched() const {
        return gpio_get(Config::Pins::TOUCH_PAD_3);
    }

    bool isPad4Touched() const {
        return gpio_get(Config::Pins::TOUCH_PAD_4);
    }

    bool isPad5Touched() const {
        return gpio_get(Config::Pins::TOUCH_PAD_5);
    }

    // Edge-triggered queries (true for exactly ONE frame when pressed)
    bool wasPad1Pressed() const {
        return pad1JustPressed;
    }

    bool wasPad2Pressed() const {
        return pad2JustPressed;
    }

    bool wasPad3Pressed() const {
        return pad3JustPressed;
    }

    bool wasPad4Pressed() const {
        return pad4JustPressed;
    }

    bool wasPad5Pressed() const {
        return pad5JustPressed;
    }

private:
    bool pad1LastState = false;
    bool pad2LastState = false;
    bool pad3LastState = false;
    bool pad4LastState = false;
    bool pad5LastState = false;

    bool pad1JustPressed = false;
    bool pad2JustPressed = false;
    bool pad3JustPressed = false;
    bool pad4JustPressed = false;
    bool pad5JustPressed = false;
};