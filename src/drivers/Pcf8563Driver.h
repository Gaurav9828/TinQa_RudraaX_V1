#pragma once

#include "IRtcDriver.h"
#include <hardware/i2c.h>

class Pcf8563Driver : public IRtcDriver {
public:
    explicit Pcf8563Driver(i2c_inst_t* i2c_port, uint8_t address = 0x51);
    
    bool initializeAndSync() override;
    bool getDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) override;

private:
    i2c_inst_t* i2c_;
    uint8_t addr_;

    static uint8_t decToBcd(int val);
    static int bcdToDec(uint8_t val);
    static void parseBuildDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second);
};