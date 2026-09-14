#pragma once

#include <cstdint>
#include <cstring>

#ifdef PICO_BUILD
#include "hardware/flash.h"
#include "hardware/sync.h"
// Use the last 4KB sector of a 2MB flash layout (adjust if your flash size differs)
#define FLASH_TARGET_OFFSET (2 * 1024 * 1024 - 4096)
#endif

struct SavedTimeState {
    uint32_t magic_marker; // Verifies valid stored data
    uint32_t day_of_year;
    double simulated_seconds;
};

class TimePersistence {
public:
    static constexpr uint32_t MAGIC_VAL = 0x54494E51; // "TINQ"

    static bool loadState(uint16_t& out_day, double& out_seconds) {
#ifdef PICO_BUILD
        const uint8_t* flash_target_contents = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
        SavedTimeState state;
        std::memcpy(&state, flash_target_contents, sizeof(SavedTimeState));

        if (state.magic_marker == MAGIC_VAL && state.day_of_year > 0 && state.day_of_year <= 366) {
            out_day = static_cast<uint16_t>(state.day_of_year);
            out_seconds = state.simulated_seconds;
            return true;
        }
#endif
        // Fallback: Parse compile-time macro (__DATE__ and __TIME__)
        out_day = parseCompileDay(__DATE__);
        out_seconds = parseCompileSeconds(__TIME__);
        return false;
    }

    static void saveState(uint16_t day, double seconds) {
#ifdef PICO_BUILD
        SavedTimeState state{MAGIC_VAL, day, seconds};
        uint8_t sector_buffer[FLASH_SECTOR_SIZE];
        
        // Read existing sector contents to preserve other data if needed
        const uint8_t* flash_target_contents = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
        std::memcpy(sector_buffer, flash_target_contents, FLASH_SECTOR_SIZE);
        std::memcpy(sector_buffer, &state, sizeof(SavedTimeState));

        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_TARGET_OFFSET, sector_buffer, FLASH_PAGE_SIZE);
        restore_interrupts(ints);
#else
        (void)day;
        (void)seconds;
#endif
    }

private:
    static uint16_t parseCompileDay(const char* date_str) {
        // Simple month/day extraction from __DATE__ (e.g., "Sep 13 2026")
        const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
        char m_str[4] = {date_str[0], date_str[1], date_str[2], '\0'};
        int month = (strstr(months, m_str) - months) / 3 + 1;
        int day = atoi(date_str + 4);
        
        // Approximate day of year calculation
        int days_in_months[] = {0,31,59,90,120,151,181,212,243,273,304,334};
        return static_cast<uint16_t>(days_in_months[month - 1] + day);
    }

    static double parseCompileSeconds(const char* time_str) {
        // Parse __TIME__ (e.g., "22:41:09")
        int hour = atoi(time_str);
        int minute = atoi(time_str + 3);
        int second = atoi(time_str + 6);
        return static_cast<double>((hour * 3600) + (minute * 60) + second);
    }
};