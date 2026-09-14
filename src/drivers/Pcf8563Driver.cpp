#include "Pcf8563Driver.h"
#include <cstdlib>
#include <cstring>

Pcf8563Driver::Pcf8563Driver(i2c_inst_t* i2c_port, uint8_t address) 
    : i2c_(i2c_port), addr_(address) {}

uint8_t Pcf8563Driver::decToBcd(int val) {
    return static_cast<uint8_t>((val / 10 * 16) + (val % 10));
}

int Pcf8563Driver::bcdToDec(uint8_t val) {
    return (val / 16 * 10) + (val % 16);
}

void Pcf8563Driver::parseBuildDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) {
    const char* date_str = __DATE__; // e.g., "Sep 14 2026"
    const char* time_str = __TIME__; // e.g., "11:03:46"

    const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char m_str[4] = {date_str[0], date_str[1], date_str[2], '\0'};
    month = (strstr(months, m_str) - months) / 3 + 1;
    day = atoi(date_str + 4);
    year = atoi(date_str + 9) - 2000;

    hour = atoi(time_str);
    minute = atoi(time_str + 3);
    second = atoi(time_str + 6);
}

bool Pcf8563Driver::initializeAndSync() {
#ifdef PICO_BUILD
    uint8_t reg_addr = 0x02; // Start from seconds register
    uint8_t time_data[7];
    
    // Read current RTC time
    i2c_write_blocking(i2c_, addr_, &reg_addr, 1, true);
    int result = i2c_read_blocking(i2c_, addr_, time_data, 7, false);

    // If read fails or chip is uninitialized, sync with MacBook compile time
    int yr, mo, dy, hr, mn, sc;
    parseBuildDateTime(yr, mo, dy, hr, mn, sc);

    uint8_t buf[8];
    buf[0] = 0x02; 
    buf[1] = decToBcd(sc & 0x7F);
    buf[2] = decToBcd(mn & 0x7F);
    buf[3] = decToBcd(hr & 0x3F);
    buf[4] = decToBcd(dy & 0x3F);
    buf[5] = decToBcd(0);        
    buf[6] = decToBcd(mo & 0x1F);
    buf[7] = decToBcd(yr & 0xFF);

    i2c_write_blocking(i2c_, addr_, buf, 8, false);
    return true;
#else
    return false;
#endif
}

bool Pcf8563Driver::getDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) {
#ifdef PICO_BUILD
    uint8_t reg_addr = 0x02;
    uint8_t time_data[7];
    
    i2c_write_blocking(i2c_, addr_, &reg_addr, 1, true);
    if (i2c_read_blocking(i2c_, addr_, time_data, 7, false) < 0) {
        return false;
    }

    second = bcdToDec(time_data[0] & 0x7F);
    minute = bcdToDec(time_data[1] & 0x7F);
    hour   = bcdToDec(time_data[2] & 0x3F);
    day    = bcdToDec(time_data[3] & 0x3F);
    month  = bcdToDec(time_data[5] & 0x1F);
    year   = bcdToDec(time_data[6]) + 2000;
    return true;
#else
    year = 2026; month = 9; day = 14; hour = 12; minute = 0; second = 0;
    return false;
#endif
}