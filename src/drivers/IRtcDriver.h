#pragma once
#include <cstdint>

class IRtcDriver {
public:
    virtual ~IRtcDriver() = default;
    virtual bool initializeAndSync() = 0;
    virtual bool getDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) = 0;
};