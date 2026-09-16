#pragma once

#include <cstdint>
#include <cstring>
#include <cstdlib>

#ifdef PICO_BUILD
#include "hardware/flash.h"
#include "hardware/sync.h"
#define FLASH_TARGET_OFFSET (2 * 1024 * 1024 - 4096)
#endif

struct SavedTimeState {
    uint32_t magic_marker;      
    uint32_t build_timestamp;   
    uint32_t day_of_year;
    double simulated_seconds;
};

class TimePersistence {
public:
    static constexpr uint32_t MAGIC_VAL = 0x54494E51; // "TINQ"

    // Define TimeInfo struct publicly so external managers can access it
    struct TimeInfo {
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
    };

    static TimeInfo getCurrentTime() {
        TimeInfo info;
        info.day = 20;   // Default test day for scheduler
        info.hour = 2;   // Default test hour (02:00 AM)
        info.minute = 0;
        return info;
    }

    static bool loadState(uint16_t& out_day, double& out_seconds) {
        uint32_t current_build_ts = getCompileTimestamp();

#ifdef PICO_BUILD
        const uint8_t* flash_target_contents = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
        SavedTimeState state;
        std::memcpy(&state, flash_target_contents, sizeof(SavedTimeState));

        if (state.magic_marker == MAGIC_VAL && state.build_timestamp == current_build_ts && 
            state.day_of_year > 0 && state.day_of_year <= 365) {
            out_day = static_cast<uint16_t>(state.day_of_year);
            out_seconds = state.simulated_seconds;
            return true;
        }
#endif
        out_day = parseCompileDay(__DATE__);
        out_seconds = parseCompileSeconds(__TIME__);
        saveState(out_day, out_seconds);
        return false;
    }

    static void saveState(uint16_t day, double seconds) {
#ifdef PICO_BUILD
        SavedTimeState state{
            MAGIC_VAL,
            getCompileTimestamp(),
            static_cast<uint32_t>(day),
            seconds
        };
        uint8_t sector_buffer[FLASH_SECTOR_SIZE];
        
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
    static uint32_t getCompileTimestamp() {
        return (static_cast<uint32_t>(parseCompileDay(__DATE__)) << 16) | 
               static_cast<uint32_t>(parseCompileSeconds(__TIME__) / 60);
    }

    static uint16_t parseCompileDay(const char* date_str) {
        const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
        char m_str[4] = {date_str[0], date_str[1], date_str[2], '\0'};
        int month = (strstr(months, m_str) - months) / 3 + 1;
        int day = std::atoi(date_str + 4);
        
        int days_in_months[] = {0,31,59,90,120,151,181,212,243,273,304,334};
        return static_cast<uint16_t>(days_in_months[month - 1] + day);
    }

    static double parseCompileSeconds(const char* time_str) {
        int hour = std::atoi(time_str);
        int minute = std::atoi(time_str + 3);
        int second = std::atoi(time_str + 6);
        return static_cast<double>((hour * 3600) + (minute * 60) + second);
    }
};