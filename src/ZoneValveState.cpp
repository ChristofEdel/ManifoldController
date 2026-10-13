#include "ZoneValveState.h"

#include <Arduino.h>
#include <mutex>
#include "MyConfig.h"
#include "NeohubZoneManager.h"

namespace {
    MyMutex stateMutex("ZoneValveState");
    ZoneValveState state = ZoneValveState::STARTUP;
    unsigned long openingStartedAt = 0;

    unsigned long getOpeningDelayMs() {
        const long delaySeconds = Config.getZoneValveDelaySeconds();
        return delaySeconds > 0 ? static_cast<unsigned long>(delaySeconds) * 1000UL : 0;
    }
}

ZoneValveState updateZoneValveState() {
    static const unsigned long startupStartedAt = millis();

    // Check if there are any zones which are calling for heat.
    bool anyZoneCallingForHeat = false;
    for (NeohubZone z : NeohubZoneManager.getActiveZones()) {
        NeohubZoneData* d = NeohubZoneManager.getZoneData(z.id);
        if (d && d->demand) {
            anyZoneCallingForHeat = true;
            break;
        }
    }

    std::lock_guard<MyMutex> lock(stateMutex);
    const unsigned long openingDelayMs = getOpeningDelayMs();

    switch (state) {
        // Demand during the first ten seconds bypasses the opening delay.
        case ZoneValveState::STARTUP:
            if (millis() - startupStartedAt >= 10000UL) {
                state = ZoneValveState::CLOSED;
            }
            else if (anyZoneCallingForHeat) {
                state = ZoneValveState::OPEN;
            }
            break;

        // Stop immediately when no zones are calling for heat.
        case ZoneValveState::OPEN:
            if (anyZoneCallingForHeat) break;
            state = ZoneValveState::CLOSED;
            break;

        // A new call for heat starts the opening delay, if configured.
        case ZoneValveState::CLOSED:
            if (!anyZoneCallingForHeat) break;
            if (openingDelayMs > 0) {
                openingStartedAt = millis();
                state = ZoneValveState::OPENING;
                break;
            }
            state = ZoneValveState::OPEN;
            break;

        // Cancel the wait if demand disappears; otherwise open when it expires.
        case ZoneValveState::OPENING:
            if (!anyZoneCallingForHeat) {
                state = ZoneValveState::CLOSED;
                break;
            }
            if (millis() - openingStartedAt >= openingDelayMs) {
                openingStartedAt = 0;
                state = ZoneValveState::OPEN;
            }
            break;
    }

    return state;
}

ZoneValveState getZoneValveState() {
    std::lock_guard<MyMutex> lock(stateMutex);
    return state;
}

float getZoneValveOpeningSecondsRemaining() {
    std::lock_guard<MyMutex> lock(stateMutex);
    if (state != ZoneValveState::OPENING) return 0.0f;

    const unsigned long openingDelayMs = getOpeningDelayMs();
    const unsigned long elapsedMs = millis() - openingStartedAt;
    if (elapsedMs >= openingDelayMs) return 0.0f;

    return (openingDelayMs - elapsedMs) / 1000.0f;
}
