#pragma once

#include <cstdio>
#include "pico/stdlib.h"
#include "../config/AppConfig.h"
#include "drivers/TouchDriver.h"
#include "../drivers/FrameStreamer.h"
#include "network/WifiWsServer.h"

class TouchTest {
public:
    static void run(TouchDriver& touch, WifiWsServer& wsServer) {
        printf("\n===================================================\n");
        printf("   SUMMIT-X FIRMWARE: TOUCH & STREAM TEST RUNNER   \n");
        printf("===================================================\n");
        printf("[INIT] Target FPS: %u (Interval: %u ms)\n", Config::TARGET_FPS, Config::FRAME_INTERVAL_MS);
        printf("[INIT] Touch Sensors: GP%u (Pad 1) | GP%u (Pad 2)\n", Config::Pins::TOUCH_PAD_1, Config::Pins::TOUCH_PAD_2);
        printf("[INIT] Resolution: %ux%u | Payload Size: %zu bytes\n", 
               Config::MATRIX_WIDTH, Config::MATRIX_HEIGHT, 
               (size_t)(4 + (Config::MATRIX_WIDTH * Config::MATRIX_HEIGHT * 3)));
        printf("---------------------------------------------------\n\n");

        FrameStreamer streamer(Config::MATRIX_WIDTH, Config::MATRIX_HEIGHT);

        bool prevPad1 = false;
        bool prevPad2 = false;
        uint32_t counterPad1 = 0;
        uint32_t counterPad2 = 0;

        uint32_t totalFramesSent = 0;
        uint32_t lastFrameTime = to_ms_since_boot(get_absolute_time());
        uint32_t lastLogTime = to_ms_since_boot(get_absolute_time());

        printf("[LOG] Entering continuous 30 FPS stream loop...\n");

        while (true) {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            bool currentPad1 = touch.isPad1Touched();
            bool currentPad2 = touch.isPad2Touched();

            // 1. Log Touch Pad 1 State Transition
            if (currentPad1 != prevPad1) {
                if (currentPad1) {
                    counterPad1++;
                    printf("[%08lu ms] [TOUCH] Pad 1 PRESS (#%u) -> HIGH (GP%u)\n", 
                           now, counterPad1, Config::Pins::TOUCH_PAD_1);
                } else {
                    printf("[%08lu ms] [TOUCH] Pad 1 RELEASE -> LOW (GP%u)\n", 
                           now, Config::Pins::TOUCH_PAD_1);
                }
                prevPad1 = currentPad1;
            }

            // 2. Log Touch Pad 2 State Transition
            if (currentPad2 != prevPad2) {
                if (currentPad2) {
                    counterPad2++;
                    printf("[%08lu ms] [TOUCH] Pad 2 PRESS (#%u) -> HIGH (GP%u)\n", 
                           now, counterPad2, Config::Pins::TOUCH_PAD_2);
                } else {
                    printf("[%08lu ms] [TOUCH] Pad 2 RELEASE -> LOW (GP%u)\n", 
                           now, Config::Pins::TOUCH_PAD_2);
                }
                prevPad2 = currentPad2;
            }

            // 3. Render Matrix Frame based on Touch Input
            if (currentPad1 && currentPad2) {
                streamer.clear(255, 0, 255); // Magenta (Dual Press)
            } else if (currentPad1) {
                streamer.clear(0, 0, 255);   // Blue (Pad 1)
            } else if (currentPad2) {
                streamer.clear(255, 0, 0);   // Red (Pad 2)
            } else {
                streamer.clear(0, 0, 0);     // Off (Idle)
            }

            // 4. Stream Frame at 30 FPS (~33ms)
            if (now - lastFrameTime >= Config::FRAME_INTERVAL_MS) {
                wsServer.sendFrame(streamer.getBufferData(), streamer.getBufferSize());
                totalFramesSent++;
                lastFrameTime = now;
            }

            // 5. Periodic Diagnostics Logging (Every 3 seconds)
            if (now - lastLogTime >= 3000) {
                printf("[%08lu ms] [SYS DIAG] Active Client: %s | Total Frames Streamed: %u | Active State: %s\n",
                       now, 
                       wsServer.hasActiveClient() ? "CONNECTED" : "WAITING", 
                       totalFramesSent,
                       (currentPad1 && currentPad2) ? "MAGENTA (DUAL)" : 
                       (currentPad1) ? "BLUE (PAD 1)" : 
                       (currentPad2) ? "RED (PAD 2)" : "IDLE");
                lastLogTime = now;
            }

            sleep_ms(5); // Fast 200 Hz GPIO sampling rate
        }
    }
};