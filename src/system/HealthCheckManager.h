#pragma once

#include <cstdint>
#include "drivers/BH1750Driver.h"
#include "drivers/LedMatrixDriver.h"
#include "drivers/TouchDriver.h"
#include "config/AppConfig.h"

enum class HealthStatus {
    OK = 0,
    WARNING = 1,      // Yellow blink codes (>10% LED, sensors, etc.)
    CRITICAL = 2      // Red blink codes (>20% LED, exhaust/thermal failure)
};

struct SystemHealthReport {
    bool ambientSensorHealthy;
    bool touchPadsHealthy[5];
    bool ledMatrixHealthy;
    bool exhaustFanHealthy;
    bool thermalStatusHealthy;
    bool systemMemoryHealthy;

    uint16_t totalLedCount;
    uint16_t faultyLedCount;
    float faultyLedPercentage;
    float currentInternalTempC;

    HealthStatus overallStatus;
    int assignedErrorCode;
};

class HealthCheckManager {
public:
    HealthCheckManager();

    void begin(BH1750Driver& ambientSensor, TouchDriver& touchDriver, LedMatrixDriver& matrixDriver);
    void update(uint32_t deltaMs, BH1750Driver& ambientSensor, TouchDriver& touchDriver, LedMatrixDriver& matrixDriver);
    void runManualDiagnostics(BH1750Driver& ambientSensor, TouchDriver& touchDriver, LedMatrixDriver& matrixDriver);

    void render(uint8_t* buffer, uint16_t width, uint16_t height);
    void executeSystemResetAndCacheWipe();

    bool isCheckComplete() const { return checkComplete; }
    HealthStatus getOverallStatus() const { return healthReport.overallStatus; }
    int getAssignedErrorCode() const { return healthReport.assignedErrorCode; }
    const SystemHealthReport& getReport() const { return healthReport; }

private:
    bool checkComplete;
    uint32_t elapsedMs;
    float heartbeatPhase;

    bool activeHourlyReminder;       
    uint32_t hourlyBlinkTimerMs;     
    bool isHourlyBlinkingActive;     
    uint8_t lastCheckedDay;          

    SystemHealthReport healthReport;

    // Multi-error tracking variables
    int activeErrorCount;
    int activeErrorCodes[5];

    void runFullDiagnostics(BH1750Driver& ambientSensor, TouchDriver& touchDriver, LedMatrixDriver& matrixDriver, bool isAutoRun);
    void runLedPanelDeepScan(LedMatrixDriver& matrixDriver);
    float readRp2040InternalTemperature();
    void performCacheWipe();

    void renderBlinkCodeDots(uint8_t* buffer, uint16_t width, uint16_t height, int dotCount, uint8_t r, uint8_t g, uint8_t b);
    void renderMultiErrorDisplay(uint8_t* buffer, uint16_t width, uint16_t height);
    void renderFullPanelRedPulse(uint8_t* buffer, uint16_t width, uint16_t height);
};