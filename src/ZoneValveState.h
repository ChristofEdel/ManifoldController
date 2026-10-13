#ifndef ZONE_VALVE_OPEN_H
#define ZONE_VALVE_OPEN_H

// The state of the zone valves on this manifold
enum class ZoneValveState {
    STARTUP, 
    CLOSED,     // All zone valves are closed
    OPENING,    // The first zone valve (or several zone vales are) in transition to OPEN
    OPEN        // At least one zone valve is open
};

// Determine the zone valve state from examinig the call for heat information
// received from the individual zone stats.
ZoneValveState updateZoneValveState();

// Read the current state without influencing it
ZoneValveState getZoneValveState();

// Seconds until opening completes; zero outside OPENING or once the wait has elapsed.
float getZoneValveOpeningSecondsRemaining();

#endif
