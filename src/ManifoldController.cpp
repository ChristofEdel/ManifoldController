#include <ArduinoOTA.h>
#include <cppQueue.h>
#include "Esp32Controller.h"
#include "ValveManager.h"
#include "EspTools.h"
#include "OneWireManager.h"
#include "NeohubZoneManager.h"
#include "ManifoldConnections.h"
#include "MyWifi.h"
#include "sensorLog.h"

// Pin Assignments - digital pins --------------------------------
//
// A0, A1  - OpenTherm module
// A2, A3  - used by I2C bus
// A4 - A5 - available
// A6 - A7 - reserved for "230V present" inputs
// D0, D1  - I2C pins for the 0-10V DAC
// D2 - D4 - reserverd for relays
// D5      - OneWire sensor interface line
// D6      - unallocated
// D7      - unallocated, causes problems with SPI if used for OneWire,
//           best avoided!
// D8, D9  - reserverd for UART
// D10     - SD Card chip select
// D11-D13 - SPI MOSI, MISO, SCK for SD card communication
// D14-D16 - Built-in RGB LED (R/B/G)

const uint8_t openThermInPin = A0;   // Repurposed analog pins for OpenTherm module I/O
const uint8_t openThermOutPin = A1;  //
const uint8_t i2cSdaPin = D0;        // I2C pins for the 0-10V DAC
const uint8_t i2cSclPin = D1;        //
const uint8_t oneWirePin = D5;       // OnwWire sensor interface line
const uint8_t sdCardCsPin = D10;     // SD card chip select

Esp32Controller ManifoldController(sdCardCsPin, oneWirePin);

void startValveControlTask();
void triggerValveControls(bool);

void setup()
{
    ManifoldController.setup();
    
    // Initialise the valve manager from the configuration
    ValveManager.setup();
    ValveManager.setRooomSetpoint(Config.getRoomSetpoint());

    // Sustain the integral (cumulative error) for the controller stages from last start
    MyRtcData *rtcData = getMyRtcData();
    if (rtcData->getLastKnownValveControllerIntegralSet()) {
        MyLog.printf("Initialising valve integral to %.0f%\n", rtcData->getLastKnownValveControllerIntegral());
        ValveManager.setValveIntegralTerm(rtcData->getLastKnownValveControllerIntegral());
    }
    if (rtcData->getLastKnownFlowControllerIntegralSet()) {
        MyLog.printf("Initialising flow integral to %.1f\n", rtcData->getLastKnownFlowControllerIntegral());
        ValveManager.setFlowIntegralTerm(rtcData->getLastKnownFlowControllerIntegral());
    }

    // Launch the backgroud task that performs the valve control loop
    startValveControlTask();

    ManifoldController.start();

}

void loop()
{
    // How often we do what
    const unsigned long controlLoopInterval = 1000;         // Read sensors and set control vale poistion
    const unsigned long logFileInterval = 5000;             // log sensor and control values

    // When we last did that
    static unsigned long lastControlLoop = 0;
    static unsigned long lastLogFile = 0;
    static bool first = true;

    // Do stuff
    unsigned long timeNow = millis();

    if (first || timeNow - lastControlLoop >= controlLoopInterval) {
        lastControlLoop = timeNow;
        if (first || timeNow - lastLogFile >= logFileInterval) {
            lastLogFile = timeNow;
            triggerValveControls(true);  // emit a log line
        }
        else {
            triggerValveControls(false);  // no log line
        }
        if (dayChanged()) {
            logSensorIssues();
        }
    }
    first = false;

    // Trigger common loop functions
    ManifoldController.loop(timeNow);
    delay(10);
}

void readSensors()
{
    UBaseType_t prio = uxTaskPriorityGet(NULL);
    vTaskPrioritySet(NULL, 3);
    OneWireManager.readAllSensors();
    vTaskPrioritySet(NULL, prio);
}

TaskHandle_t valveControlTaskHandle = NULL;  // Task for boiler control
QueueHandle_t valveControlQueue = NULL;

void manageValveControls()
{
    // First, calculate the average room temperature for all zones which are configured
    // Update the timestamp when we collected the lastest room temperature if we can obtain at least one
    int tempCount = 0;
    double temperatureTotal = 0;
    for (NeohubZone z : NeohubZoneManager.getActiveZones()) {
        NeohubZoneData* d = NeohubZoneManager.getZoneData(z.id);
        if (d && d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
            if (d->lastUpdate > ValveManager.timestamps.roomDataLoadTime) ValveManager.timestamps.roomDataLoadTime = d->lastUpdate;
            temperatureTotal += d->roomTemperature;
            tempCount++;
        }
    }
    float roomTemperature = NeohubZoneData::NO_TEMPERATURE;
    if (tempCount > 0) roomTemperature = temperatureTotal / tempCount;

    // Then, get the temperatures from the manifold
    // the flowTemperature is the one that matters, the other ones are currently FYI
    // Update the timestamp when we last collected a valid flowTemperature
    float inputTemperature = OneWireManager.getCalibratedTemperature(Config.getInputSensorId().c_str());
    float flowTemperature = OneWireManager.getCalibratedTemperature(Config.getFlowSensorId().c_str());
    float returnTemperature = OneWireManager.getCalibratedTemperature(Config.getReturnSensorId().c_str());

    if (flowTemperature > -50) ValveManager.timestamps.flowDataLoadTime = time(nullptr);

    // run the control loop
    ValveManager.setInputs(roomTemperature, flowTemperature, inputTemperature, returnTemperature);
    ValveManager.calculateValvePosition();
    ValveManager.sendCurrentValvePosition();

    // Serial.printf(
    //   "In: %0.1lf, Setpoint: %0.1lf, Valve: %0.1lf, Flow: %0.1lf, Return: %0.1lf\n",
    //   inputTemperature,
    //   ValveManager.getSetpoint(),
    //   ValveManager.outputs.targetValvePosition,
    //   flowTemperature,
    //   returnTemperature,
    //   flowTemperature
    // );
}

void triggerValveControls(bool writeLogLine)
{
    xQueueSend(valveControlQueue, &writeLogLine, 0);
}

// Task function that runs the boiler control in the background
void valveControlTask(void* parameter)
{
    bool writeLogLine = false;
    readSensors();
    for (;;) {
        // Wait for notification from main loop
        if (xQueueReceive(valveControlQueue, &writeLogLine, portMAX_DELAY) == pdTRUE) {
            // Drain any additional messages — only the last one is kept
            bool tmp;
            while (xQueueReceive(valveControlQueue, &tmp, 0) == pdTRUE) {
                writeLogLine = tmp;
            }

            // Control loop first
            manageValveControls();

            // remember the integral values for use at next reboot
            MyRtcData *rtcData = getMyRtcData();
            rtcData->setLastKnownFlowControllerIntegral (ValveManager.getRoomIntegralTerm());
            rtcData->setLastKnownValveControllerIntegral(ValveManager.getFlowIntegralTerm());

            // Then log if requested
            if (writeLogLine) {
                logSensors();
                // Then send our stats to the central heating controller (if configured)
                const String &host = Config.getHeatingControllerAddress();
                if (host != "" && host != "null") {
                    ManifoldData data;
                    time_t now = time(nullptr);
                    String hostname = Config.getHostname();
                    if (hostname != "" && hostname.indexOf('.') == -1) hostname = hostname + ".local";
                    data.name = Config.getName() == "" ? hostname : Config.getName();
                    data.hostname = Config.getHostname() + ".local";
                    data.ipAddress = MyWiFi.getIpAddress();
                    data.roomSetpoint = ValveManager.getRoomSetpoint();
                    data.roomTemperature = ValveManager.inputs.roomTemperature;
                    data.roomDeltaT = data.roomTemperature - data.roomSetpoint;
                    data.flowSetpoint = ValveManager.getFlowSetpoint();
                    data.flowTemperature = ValveManager.inputs.flowTemperature;
                    data.flowDeltaT = data.flowTemperature - data.flowSetpoint;
                    data.valvePosition = ValveManager.getValvePosition();
                    data.flowDemand = data.flowSetpoint;
                    data.roomTemperatureAged = ValveManager.timestamps.isAged(now, ValveManager.timestamps.roomDataLoadTime);
                    data.roomTemperatureDead = ValveManager.timestamps.isDead(now, ValveManager.timestamps.roomDataLoadTime);
                    data.flowTemperatureAged = ValveManager.timestamps.isAged(now, ValveManager.timestamps.flowDataLoadTime);
                    data.flowTemperatureDead = ValveManager.timestamps.isDead(now, ValveManager.timestamps.flowDataLoadTime);
                    data.uptimeSeconds = uptime();

                    ManifoldDataPostJob::post(data, host);
                }
            }

            // Finally, read the sensors for the next iteration
            // This is done last because reading takes around 600-800 ms so this prepares
            // the next iteration
            readSensors();

            // For a loop rate slower than 1/s this should be changed!
        }
    }
}

void startValveControlTask()
{
    // Create queue for boiler control task
    valveControlQueue = xQueueCreate(1, sizeof(bool));

    // Create boiler control task
    xTaskCreate(
        valveControlTask,        // Task function
        "ValveControl",          // Task name
        4096,                    // Stack size (bytes)
        NULL,                    // Parameter to pass
        1,                       // Task priority
        &valveControlTaskHandle  // Task handle
    );
}
