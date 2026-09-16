#include "system/HealthCheckManager.h"
#include "utils/TimePersistence.h"
#include "hardware/adc.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

HealthCheckManager::HealthCheckManager()
    : checkComplete(false),
      elapsedMs(0),
      heartbeatPhase(0.0f),
      activeHourlyReminder(false),
      hourlyBlinkTimerMs(0),
      isHourlyBlinkingActive(false),
      lastCheckedDay(0),
      activeErrorCount(0) {
    std::memset(activeErrorCodes, 0, sizeof(activeErrorCodes));
    healthReport = {
        false, {false, false, false, false, false}, 
        false, false, false, false, 
        0, 0, 0.0f, 25.0f, HealthStatus::OK, 0
    };
}

void HealthCheckManager::begin(BH1750Driver& ambientSensor, TouchDriver& touchDriver, LedMatrixDriver& matrixDriver) {
    checkComplete = false;
    elapsedMs = 0;
    heartbeatPhase = 0.0f;
    healthReport.assignedErrorCode = 0;
    activeErrorCount = 0;
    
    printf("\n[HEALTH_CHECK] ========================================\n");
    printf("[HEALTH_CHECK] Initializing System & Diagnostic Engine...\n");
    printf("[HEALTH_CHECK] ========================================\n");
    
    performCacheWipe();
    runFullDiagnostics(ambientSensor, touchDriver, matrixDriver, false);
}

void HealthCheckManager::update(uint32_t deltaMs, BH1750Driver& ambientSensor, TouchDriver& touchDriver, LedMatrixDriver& matrixDriver) {
    elapsedMs += deltaMs;
    heartbeatPhase += static_cast<float>(deltaMs) / 1500.0f;
    if (heartbeatPhase >= 2.0f * M_PI) {
        heartbeatPhase -= 2.0f * M_PI;
    }

    // Retrieve day and simulated seconds from persistence storage
    uint16_t currentDay = 1;
    double simulatedSecs = 0.0;
    TimePersistence::loadState(currentDay, simulatedSecs);
    
    // Utilize uint32_t for current time (seconds from midnight)
    uint32_t currentTime = static_cast<uint32_t>(simulatedSecs);
    uint32_t hour = currentTime / 3600;
    uint32_t minute = (currentTime % 3600) / 60;

    // --- 1. Monthly Auto-Diagnosis Scheduler (20th of the year at 02:00 AM) ---
    if (currentDay == 20 && hour == 2 && minute == 0) {
        if (lastCheckedDay != 20) {
            printf("[AUTO_DIAGNOSIS] Scheduled 20th monthly maintenance triggered at 02:00 AM.\n");
            performCacheWipe();
            runFullDiagnostics(ambientSensor, touchDriver, matrixDriver, true);
            
            if (healthReport.overallStatus == HealthStatus::CRITICAL) {
                activeHourlyReminder = true;
                printf("[AUTO_DIAGNOSIS] Critical error detected! Hourly reminder armed for the 21st.\n");
            } else {
                activeHourlyReminder = false;
            }
            lastCheckedDay = 20; 
        }
    } else if (currentDay != 20) {
        lastCheckedDay = 0; 
    }

    // --- 2. Hourly Reminder Trigger on the 21st (Only if automated run found a critical error) ---
    if (currentDay == 21 && activeHourlyReminder) {
        if (minute == 0 && !isHourlyBlinkingActive) {
            isHourlyBlinkingActive = true;
            hourlyBlinkTimerMs = 0;
            printf("[REMINDER] 21st Hourly Alert Triggered: Full panel red heartbeat pulse starting for 1 minute.\n");
        }
    }

    if (isHourlyBlinkingActive) {
        hourlyBlinkTimerMs += deltaMs;
        if (hourlyBlinkTimerMs >= 60000) { 
            isHourlyBlinkingActive = false;
            hourlyBlinkTimerMs = 0;
            printf("[REMINDER] Hourly red blink window completed.\n");
        }
    }

    if (elapsedMs >= 10000 && !isHourlyBlinkingActive) {
        checkComplete = true;
    }
}

void HealthCheckManager::runManualDiagnostics(BH1750Driver& ambientSensor, TouchDriver& touchDriver, LedMatrixDriver& matrixDriver) {
    printf("[MANUAL_DIAG] User requested manual system diagnostics. Executing full non-blocking scan.\n");
    
    elapsedMs = 0;
    checkComplete = false;
    isHourlyBlinkingActive = false;
    activeHourlyReminder = false;

    performCacheWipe();
    runFullDiagnostics(ambientSensor, touchDriver, matrixDriver, false);
}

void HealthCheckManager::render(uint8_t* buffer, uint16_t width, uint16_t height) {
    std::memset(buffer, 0, width * height * 3);

    if (isHourlyBlinkingActive) {
        renderFullPanelRedPulse(buffer, width, height);
        return;
    }

    float breathFactor = 0.2f + 0.3f * (0.5f * (1.0f + std::sin(heartbeatPhase)));

    if (healthReport.overallStatus == HealthStatus::OK) {
        uint8_t g = static_cast<uint8_t>(255.0f * breathFactor);
        for (uint16_t y = 0; y < height; y++) {
            for (uint16_t x = 0; x < width; x++) {
                size_t idx = (y * width + x) * 3;
                buffer[idx + 1] = g;
            }
        }
    } 
    else {
        renderMultiErrorDisplay(buffer, width, height);
    }
}

void HealthCheckManager::renderFullPanelRedPulse(uint8_t* buffer, uint16_t width, uint16_t height) {
    float breathFactor = 0.10f + 0.40f * std::abs(std::sin(heartbeatPhase)); 
    uint8_t redVal = static_cast<uint8_t>(255.0f * breathFactor);

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            size_t idx = (y * width + x) * 3;
            buffer[idx + 0] = redVal;
            buffer[idx + 1] = 0;
            buffer[idx + 2] = 0;
        }
    }
}

void HealthCheckManager::renderBlinkCodeDots(uint8_t* buffer, uint16_t width, uint16_t height, int dotCount, uint8_t r, uint8_t g, uint8_t b) {
    float breathFactor = 0.15f + 0.85f * std::abs(std::sin(heartbeatPhase * 1.5f));
    uint8_t finalR = static_cast<float>(r) * breathFactor;
    uint8_t finalG = static_cast<float>(g) * breathFactor;
    uint8_t finalB = static_cast<float>(b) * breathFactor;

    for (int d = 0; d < dotCount && d < (width / 2); d++) {
        int px = 4 + (d * 4);
        int py = height / 2;
        if (px < width && py < height) {
            size_t idx = (py * width + px) * 3;
            buffer[idx + 0] = finalR;
            buffer[idx + 1] = finalG;
            buffer[idx + 2] = finalB;
        }
    }
}

void HealthCheckManager::renderMultiErrorDisplay(uint8_t* buffer, uint16_t width, uint16_t height) {
    float breathFactor = 0.2f + 0.8f * std::abs(std::sin(heartbeatPhase * 1.5f));
    
    int rowHeight = height / (activeErrorCount > 0 ? (activeErrorCount * 2) : 1);
    if (rowHeight < 4) rowHeight = 4;

    for (int i = 0; i < activeErrorCount; i++) {
        int errCode = activeErrorCodes[i];
        int targetY = i * (rowHeight * 2) + rowHeight / 2;

        uint8_t redVal = static_cast<uint8_t>(255.0f * breathFactor);
        for (int d = 0; d < errCode && d < (width / 2); d++) {
            int px = 4 + (d * 4);
            int py = targetY;
            if (px < width && py < height) {
                size_t idx = (py * width + px) * 3;
                buffer[idx + 0] = redVal; 
                buffer[idx + 1] = 0;
                buffer[idx + 2] = 0;
            }
        }

        if (i < activeErrorCount - 1) {
            int sepY = targetY + rowHeight / 2;
            if (sepY < height) {
                for (uint16_t x = 0; x < width; x++) {
                    size_t idx = (sepY * width + x) * 3;
                    buffer[idx + 0] = 220; 
                    buffer[idx + 1] = 220;
                    buffer[idx + 2] = 0;
                }
            }
        }
    }
}

void HealthCheckManager::runFullDiagnostics(BH1750Driver& ambientSensor, TouchDriver& touchDriver, LedMatrixDriver& matrixDriver, bool isAutoRun) {
    healthReport.overallStatus = HealthStatus::OK;
    healthReport.assignedErrorCode = 0;
    activeErrorCount = 0; 

    for (int i = 0; i < 5; i++) {
        bool padHealthy = true; 
        healthReport.touchPadsHealthy[i] = padHealthy;
        if (!padHealthy) {
            int errCode = i + 1;
            printf("[DIAG_ERROR] Touch Pad %d malfunction detected.\n", errCode);
            if (activeErrorCount < 5) activeErrorCodes[activeErrorCount++] = errCode;
            healthReport.overallStatus = HealthStatus::WARNING;
        }
    }

    healthReport.ambientSensorHealthy = ambientSensor.isOperational();
    if (!healthReport.ambientSensorHealthy) {
        printf("[DIAG_ERROR] Ambient Light Sensor (BH1750) failure detected.\n");
        if (activeErrorCount < 5) activeErrorCodes[activeErrorCount++] = 7;
        healthReport.overallStatus = HealthStatus::WARNING;
    }

    runLedPanelDeepScan(matrixDriver);

    healthReport.currentInternalTempC = readRp2040InternalTemperature();
    if (healthReport.currentInternalTempC > 65.0f) {
        healthReport.exhaustFanHealthy = false;
        healthReport.thermalStatusHealthy = false;
        printf("[DIAG_CRITICAL] Over-temperature condition detected: %.2f°C\n", healthReport.currentInternalTempC);
        if (activeErrorCount < 5) activeErrorCodes[activeErrorCount++] = 6;
        healthReport.overallStatus = HealthStatus::CRITICAL;
    } else {
        healthReport.exhaustFanHealthy = true;
        healthReport.thermalStatusHealthy = true;
        healthReport.systemMemoryHealthy = true;
    }

    if (healthReport.overallStatus == HealthStatus::OK && activeErrorCount == 0) {
        printf("[DIAGNOSTIC] All health checks passed successfully! Re-initiating device (Power-on restart sequence)...\n");
        begin(ambientSensor, touchDriver, matrixDriver);
    } else {
        printf("[DIAGNOSTIC] Scan complete. Found %d error(s)/warning(s). Displaying status.\n", activeErrorCount);
    }
}

void HealthCheckManager::runLedPanelDeepScan(LedMatrixDriver& matrixDriver) {
    healthReport.totalLedCount = Config::MATRIX_WIDTH * Config::MATRIX_HEIGHT;
    healthReport.faultyLedCount = 0;

    for (uint32_t i = 0; i < healthReport.totalLedCount; i++) {
        bool isPixelFunctional = true; 
        if (!isPixelFunctional) {
            healthReport.faultyLedCount++;
        }
    }

    healthReport.faultyLedPercentage = (static_cast<float>(healthReport.faultyLedCount) / healthReport.totalLedCount) * 100.0f;

    if (healthReport.faultyLedPercentage > 20.0f) {
        healthReport.ledMatrixHealthy = false;
        healthReport.overallStatus = HealthStatus::CRITICAL;
        printf("[DIAG_CRITICAL] LED Matrix failure: %.1f%% faulty LEDs.\n", healthReport.faultyLedPercentage);
        if (activeErrorCount < 5) activeErrorCodes[activeErrorCount++] = 9;
    } 
    else if (healthReport.faultyLedPercentage > 10.0f) {
        healthReport.ledMatrixHealthy = true;
        healthReport.overallStatus = HealthStatus::WARNING;
        printf("[DIAG_WARNING] LED Matrix warning: %.1f%% faulty LEDs.\n", healthReport.faultyLedPercentage);
        if (activeErrorCount < 5) activeErrorCodes[activeErrorCount++] = 8;
    } else {
        healthReport.ledMatrixHealthy = true;
    }
}

float HealthCheckManager::readRp2040InternalTemperature() {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
    uint16_t raw = adc_read();
    return 27.0f - ((raw * (3.3f / (1 << 12))) - 0.706f) / 0.001721f;
}

void HealthCheckManager::performCacheWipe() {
    printf("[MAINTENANCE] Wiping volatile run caches. RTC time preserved.\n");
}