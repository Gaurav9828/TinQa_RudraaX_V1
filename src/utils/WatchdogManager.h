#pragma once

#include <cstdint>
#include <cstdio>
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"

#define WATCHDOG_RECOVERY_MAGIC_KEY 0xDEADBEEF 

class WatchdogManager {
public:
    static WatchdogManager& getInstance() {
        static WatchdogManager instance;
        return instance;
    }

    void init(uint32_t timeout_ms = 3000) {
        m_timeout_ms = timeout_ms;
        
        if (watchdog_caused_reboot()) {
            uint32_t scratch_flag = watchdog_hw->scratch[0];
            if (scratch_flag == WATCHDOG_RECOVERY_MAGIC_KEY) {
                printf("[WATCHDOG] System recovered from freeze! Resuming continuous Auto Mode from saved timestamp.\n");
            } else {
                printf("[WATCHDOG] System rebooted by hardware watchdog timeout! Restoring Auto Mode.\n");
            }
        } else {
            printf("[WATCHDOG] Normal Cold Boot / Power-On detected. Initializing continuous cyclic Auto Mode.\n");
        }

        watchdog_enable(m_timeout_ms, 1);
        printf("[WATCHDOG] Hardware Watchdog Active (%lu ms timeout).\n", m_timeout_ms);
    }

    inline void kick() {
        watchdog_update();
    }

    void saveRecoveryState(double simulated_seconds, uint16_t day_of_year) {
        watchdog_hw->scratch[0] = WATCHDOG_RECOVERY_MAGIC_KEY;
        watchdog_hw->scratch[1] = static_cast<uint32_t>(simulated_seconds);
        watchdog_hw->scratch[2] = static_cast<uint32_t>(day_of_year);
    }

    bool getRecoveryTime(double& out_seconds, uint16_t& out_day) const {
        if (watchdog_hw->scratch[0] == WATCHDOG_RECOVERY_MAGIC_KEY) {
            out_seconds = static_cast<double>(watchdog_hw->scratch[1]);
            out_day = static_cast<uint16_t>(watchdog_hw->scratch[2]);
            return true;
        }
        return false;
    }

    void triggerAutoModeRecovery() {
        printf("[WATCHDOG] Forcing auto-recovery reboot to continuous AUTO_MODE...\n");
        watchdog_reboot(0, 0, 10);
        while (true) { tight_loop_contents(); }
    }

private:
    WatchdogManager() : m_timeout_ms(3000) {}
    uint32_t m_timeout_ms;
};