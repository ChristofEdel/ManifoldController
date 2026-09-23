#include <ArduinoOTA.h>
#include <cppQueue.h>
#include "MqttManager.h"
#include "Esp32Controller.h"
#include "ValveManager.h"
#include "EspTools.h"
#include "OneWireManager.h"
#include "NeohubZoneManager.h"
#include "ManifoldConnections.h"
#include "MyWifi.h"
#include "SensorLog.h"
#include "version.h"
#include "WeatherDataManager.h"
#include "StringTools.h"
#include "LoopTimer.h"
#include "StringTools.h"
#include "MyWiFi.h"
#include "MemDebug.h"

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

void mqttResetCallback(void *arg, const String&topic, const String& payload)
{
    MyLog.printf("Received %s = %s\n", topic.c_str(), payload.c_str());
    softwareReset(SW_RESET_MQTT_RESET, "Reset command from MQTT");
}

void setup()
{
    ManifoldController.setup();
    if (!Config.getMqttHost().isEmpty()) {
        if (!Config.getHostname().isEmpty()) {
            MqttManager.setAvailabilityTopic(StringPrintf("manifold/%s/available", MyWiFi.getMacAddress().c_str()));
        }
        MqttManager.start(
            StringPrintf("mqtt://%s:%d", Config.getMqttHost().c_str(), Config.getMqttPort()), 
            Config.getMqttUsername(), 
            Config.getMqttPassword()
        );
        NeohubZoneManager.subscribeZoneData(Config.getMqttTopicNeohub());
        WeatherDataManager.subscribeWeatherData(Config.getMqttTopicTemperature(), Config.getMqttTopicTemperatureKeepalive());
        Config.subscribeToMqttSetTopics(MyWiFi.getMacAddress());
        MqttManager.subscribeTopic("manifold/" + MyWiFi.getMacAddress() + "/command/restart", mqttResetCallback, nullptr);
    }

    // Initialise the valve manager from the configuration
    ValveManager.setup();

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

#define HOUR_MS (60 * 60 * 1000) 
void fillManifoldData (ManifoldData &data); // forward declaration
const int configRetentionSeconds = 60 * 60; // 1 hour
bool configPublished = false;

void republishConfiguration() {
    configPublished = false;
}

void loop()
{
    // How often we do what
    const unsigned long controlLoopInterval = 1000;              // Read sensors and set control vale poistion
    const unsigned long logFileInterval = 5000;                  // log sensor and control values
    const unsigned long mqttAutodiscoverInterval = 24 * HOUR_MS; // publish HASS autodiscover messages to MQTT daily
    const unsigned long mqttConfigPublishInterval = configRetentionSeconds * 500; // publish config parameters at half the retention period

    // When we last did that
    static unsigned long lastControlLoop = 0;
    static unsigned long lastLogFile = 0;
    static unsigned long lastAutodiscover = 0;
    static unsigned long lastConfigPublish = 0;
    static bool first = true;
    static bool autodiscoverPublished = false;

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

    if (!autodiscoverPublished || timeNow - lastAutodiscover >= mqttAutodiscoverInterval) {
        lastAutodiscover = timeNow;
        if (MqttManager.isConnected()) {
            ManifoldData d;
            d.id = MyWiFi.getMacAddress();
            fillManifoldData(d);
            autodiscoverPublished = d.publishAutodiscoverTopics() && Config.publishAutodiscoverTopics(d.id);
            if (autodiscoverPublished) MyLog.println("MQTT: Autodiscovery messages published");
        }
    }

    if (!configPublished || timeNow - lastConfigPublish >= mqttConfigPublishInterval) {
        lastConfigPublish = timeNow;
        if (MqttManager.isConnected()) {
            Config.publishConfigToMqtt(MyWiFi.getMacAddress(), configRetentionSeconds);
            configPublished = true;
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
    bool allRoomsOff = true;
    for (NeohubZone z : NeohubZoneManager.getActiveZones()) {
        NeohubZoneData* d = NeohubZoneManager.getZoneData(z.id);
        if (d) {
            if (!isnan(d->roomTemperature)) {
                if (d->lastUpdate > ValveManager.timestamps.roomDataLoadTime) ValveManager.timestamps.roomDataLoadTime = d->lastUpdate;
                temperatureTotal += d->roomTemperature;
                tempCount++;
            }
            if (d->demand) allRoomsOff = false;
        }
    }
    float roomTemperature = std::numeric_limits<float>::quiet_NaN();
    if (tempCount > 0) roomTemperature = temperatureTotal / tempCount;

    // Then, get the temperatures from the manifold
    // the flowTemperature is the one that matters, the other ones are currently FYI
    // Update the timestamp when we last collected a valid flowTemperature
    float inputTemperature = OneWireManager.getCalibratedTemperature(Config.getInputSensorId().c_str());
    float flowTemperature = OneWireManager.getCalibratedTemperature(Config.getFlowSensorId().c_str());
    float returnTemperature = OneWireManager.getCalibratedTemperature(Config.getReturnSensorId().c_str());
    float outsideTemperature = WeatherDataManager.getWeatherData().outsideTemperature;

    if (!isnan(flowTemperature)) ValveManager.timestamps.flowDataLoadTime = time(nullptr);

    // run the control loop
    ValveManager.setInputs(allRoomsOff, roomTemperature, flowTemperature, inputTemperature, returnTemperature, outsideTemperature);
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

void fillManifoldData (ManifoldData &data)
{
    time_t now = time(nullptr);

    // General information ------------------------------------------------------------------------
    data.name = Config.getName() == "" ? Config.getHostname() : Config.getName();
    data.hostname = Config.getHostname();
    // ip address and version are set once only
    data.uptimeSeconds = uptime();

    // Demand for boiler --------------------------------------------------------------------------
    data.flowDemand = data.flowSetpoint + (isnan(Config.getFlowAddOn()) ? 0 : Config.getFlowAddOn());

    // Room controller ----------------------------------------------------------------------------
    data.roomSetpoint = ValveManager.getRoomSetpoint();
    data.roomTemperature = ValveManager.inputs.roomTemperature;
    data.roomTemperatureAged = ValveManager.timestamps.isAged(now, ValveManager.timestamps.roomDataLoadTime);
    data.roomTemperatureDead = ValveManager.timestamps.isDead(now, ValveManager.timestamps.roomDataLoadTime);
    data.flowPidControllerP = ValveManager.getFlowProportionalTerm();
    data.flowPidControllerI = ValveManager.getFlowIntegralTerm();
    data.flowPidControllerD = ValveManager.getFlowDerivativeTerm(); 


    // Flow controller ----------------------------------------------------------------------------
    data.flowSetpoint = ValveManager.getFlowSetpoint();
    data.flowTemperature = ValveManager.inputs.flowTemperature;
    data.flowTemperatureAged = ValveManager.timestamps.isAged(now, ValveManager.timestamps.flowDataLoadTime);
    data.flowTemperatureDead = ValveManager.timestamps.isDead(now, ValveManager.timestamps.flowDataLoadTime);
    data.valvePidControllerP = ValveManager.getValveProportionalTerm();
    data.valvePidControllerI = ValveManager.getValveIntegralTerm();
    data.valvePidControllerD = ValveManager.getValveDerivativeTerm(); 
    data.valvePosition = ValveManager.getValvePosition();
    
    // Other information --------------------------------------------------------------------------
    data.inputTemperature = ValveManager.inputs.inputTemperature;
    data.returnTemperature = ValveManager.inputs.returnTemperature;

    // Diagnostics --------------------------------------------------------------------------------
    data.freeMemoryKb = freeRam() / 1024.0;
    data.lastResetReason = getResetReasonText();
}

// Task function that runs the boiler control in the background
void valveControlTask(void* parameter)
{
    ManifoldData myManifoldData;

    myManifoldData.id = MyWiFi.getMacAddress();
    myManifoldData.ipAddress = MyWiFi.getIpAddress();
    myManifoldData.version = String(VERSION);

    bool writeLogLine = false;
    uint32_t previousLoopStartMillis = millis(); 
    readSensors();
    LoopTimer timer;

    for (;;) {
        // Wait for notification from main loop
        if (xQueueReceive(valveControlQueue, &writeLogLine, portMAX_DELAY) == pdTRUE) {
            // Drain any additional messages — only the last one is kept --------------------------
            bool tmp;
            while (xQueueReceive(valveControlQueue, &tmp, 0) == pdTRUE) {
                writeLogLine = tmp;
            }

            // Restart the timer and remember the time from the previous loop
            timer.stop();
            LoopTimer previousTimer = timer;
            timer.start();

            // Run valve controller ---------------------------------------------------------------
            manageValveControls();
            
            // remember the integral values for use at next reboot
            MyRtcData *rtcData = getMyRtcData();
            rtcData->setLastKnownFlowControllerIntegral (ValveManager.getFlowIntegralTerm());
            rtcData->setLastKnownValveControllerIntegral(ValveManager.getValveIntegralTerm());

            timer.controlComplete();

            // MQTT Publishing --------------------------------------------------------------------
            // Get and send the current manifold controller information to MQTT
            // (and via that to the boiler)
            fillManifoldData(myManifoldData);
            myManifoldData.loopTimer = previousTimer;
            myManifoldData.sendChangesToMqtt();

            timer.sendingComplete();

            // Then log if requested --------------------------------------------------------------
            if (writeLogLine) {
                logSensors();
            }
            timer.loggingComplete();

            // Finally, read the sensors for the next iteration -----------------------------------
            // This is done last because reading takes around 600-800 ms so this prepares
            // the next iteration
            readSensors();
            // For a loop rate slower than 1/s this should be changed!

            timer.measuringComplete();
            // timer will be stopped at next round trip so we get the idlet time

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
