#include <ArduinoOTA.h>
#include <cppQueue.h>

#include "Arduino.h"
#include "EspTools.h"
#include "LedBlink.h"
#include "MemDebug.h"
#include "MyConfig.h"
#include "MyLog.h"  // Logging to serial and, if available, SD card
#include "MyMutex.h"
#include "MyRtc.h"   // Real time clock
#include "MyWiFi.h"  // WiFi access
#include "NeohubManager.h"
#include "OneWireManager.h"  // OneWire temperature sensor reading and management
#include "SensorMap.h"       // Sensor name mapping
#include "ValveManager.h"
#include "Watchdog.h"
#include "webserver/MyWebServer.h"  // Request handing and web page generator
#include "ManifoldConnections.h"
#include "BackgroundFileWriter.h"
#include "sensorLog.h"
#include "Watchdog.h"
#include "Filesystem.h"

// Pin Assignments - digital pins --------------------------------
const uint8_t openThermInPin = A0;  // Repurposed analog pins for OpenTherm module I/O
const uint8_t openThermOutPin = A1;
// A2 and A3 - used by I2C bus
// A4 - A5: available
// A6 - A7 - reserved for "230V present" inputs
const uint8_t i2cSdaPin = D0;  // I2 pins
const uint8_t i2cSclPin = D1;
// Pin 2-4: reserved for relays
const uint8_t oneWirePin = D5;  // OnwWire sensor interface
// Pin 6/7 - unallocated
//           pin 7 causes problems with
//           SPI if used for OneWire,
//           best avoided!
// Pin 8/9: Reserved for UART
const uint8_t sdCardCsPin = D10;  // SD card chip select
// Pin 11: PIN_SPI_MOSI
// Pin 12: PIN_SPI_MISO
// Pin 13: PIN_SPI_SCK
// Pin 14/15/16: Built-in RGB LED (R/B/G)


// The valve position (preserved across resets)

void readSensors();
bool dayChanged();
void startValveControlTask();
void triggerValveControls(bool);

void setup()
{
    esp_log_level_set("*", ESP_LOG_ERROR);
    handleReboot();

    // Set pins immediately so we don't trigger any relays by accident
    // pinMode(hotWaterValveCallOutPin, OUTPUT);
    // digitalWrite(heatingValveCallOutPin,LOW);

    // Initialise the serial line and wait for it to be ready
    Serial.begin(9600);

    const int serialWaitTimeoutMs = 3000;  // Timeout in case we are not debugging
    unsigned long serialWaitStart = millis();
    while (!Serial && (millis() - serialWaitStart < serialWaitTimeoutMs)) {
        delay(100);
    }

    // Initialise clock and memory debugging
    MyRtc.start();
    setupMemDebug();
    WatchdogManager.setup(100); // Check our watchdogs every 100 ms
    

    // Initialise logging to serial
    MyLog.enableSerialLog();
    MyLog.logTimestamps(MyRtc);

    MyWebLog.enableSerialLog();
    MyWebLog.logTimestamps(MyRtc);

    MyBootLog.logTimestamps(MyRtc);

    MyLog.println("-------------------------------------------------------------------------------------------");

    // Initialise SD card access and start logging to SD card as well
    MyLog.print("Initializing Filesystems...");
    if (!Filesystem.setup(sdCardCsPin)) {

    }
    MyLog.println("done.");
    BackgroundFileWriter.setup();
    MyLog.enableSdCardLog("/sdcard/log.txt");
    MyWebLog.enableSdCardLog("/sdcard/weblog.txt");
    MyBootLog.enableSdCardLog("/sdcard/bootlog.txt");

    MyLog.println("-------------------------------------------------------------------------------------------");
    Config.load();
    SensorMap.clearChanged();

    MyLog.printlnSdOnly("-------------------------------------------------------------------------------------------");

    // Connect to WiFi and initialise clock
    MyWiFi.connect();
    if (Config.getHostname() != "") {
        MyWiFi.setHostname(Config.getHostname());
    }
    MyWiFi.updateRtcFromTimeServer(&MyRtc);

    // Deal with resets and crashes
    String resetReason = getResetReasonText();
    String resetMessage = getSoftwareResetMessage();

    MyLog.print("Last reset reason: ");
    MyLog.println(resetReason);
    if (resetMessage != emptyString) MyLog.println(resetMessage);

    MyBootLog.print("RESTART - Last reset reason: ");
    MyBootLog.println(resetReason);
    if (resetMessage != emptyString) MyBootLog.println(resetMessage);
    if (getResetReason() == ESP_RST_PANIC || getResetReason() == ESP_RST_INT_WDT) {
        Esp32CoreDump dump;
        if (dump.exists()) {
            dump.writeBacktrace(MyBootLog);
        }
    }

    // Initialisations for several modules
    setupOta();
    OneWireManager.setup(oneWirePin);
    for (int i = 0; i < SensorMap.getCount(); i++) {
        OneWireManager.addKnownSensor(SensorMap[i]->id.c_str());
    }

    ValveManager.setup();
    ValveManager.setRooomSetpoint(Config.getRoomSetpoint());
    MyRtcData *rtcData = getMyRtcData();
    if (rtcData->getLastKnownValveControllerIntegralSet()) {
        MyLog.printf("Initialising valve integral to %.0f%\n", rtcData->getLastKnownValveControllerIntegral());
        ValveManager.setValveIntegralTerm(rtcData->getLastKnownValveControllerIntegral());
    }
    if (rtcData->getLastKnownFlowControllerIntegralSet()) {
        MyLog.printf("Initialising flow integral to %.1f\n", rtcData->getLastKnownFlowControllerIntegral());
        ValveManager.setFlowIntegralTerm(rtcData->getLastKnownFlowControllerIntegral());
    }
    MyWebServer.setup();
    ledBlinkSetup();

    startValveControlTask();

    MyLog.println("-------------------------------------------------------------------------------------------");

}

void loop()
{
    // How often we do what
    const unsigned long timeSyncInterval = 60 * 60 * 1000;  // Synchronise time every hour
    const unsigned long controlLoopInterval = 1000;         // Read sensors and set control vale poistion
    const unsigned long logFileInterval = 5000;             // log sensor and control values
    const unsigned long memoryCheckInterval = 10000;        // Check for possible memory leaks
    const unsigned long blinkInterval = 100;                // Blink so we can see the loop is running
    const unsigned long otaCheckInterval = 1000;            // Over-The-Air updates

    // When we last did that
    static unsigned long lastTimeSync = 0;
    static unsigned long lastControlLoop = 0;
    static unsigned long lastLogFile = 0;
    static unsigned long lastMemoryCheck = 0;
    static unsigned long lastBlink = 0;
    static unsigned long lastOtaCheck = 0;
    static bool first = true;

    // Do stuff
    unsigned long timeNow = millis();

    if (first || timeNow - lastTimeSync >= timeSyncInterval) {
        lastTimeSync = timeNow;
        if (!MyWiFi.updateRtcFromTimeServer(&MyRtc)) {
            MyLog.println("Failed to synchronise real time clock with network");
        }
    }

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

    if (first || timeNow - lastMemoryCheck >= memoryCheckInterval) {
        lastMemoryCheck = timeNow;
        checkMemoryChange(4096);  // Report if we have lost more than 4kB since last check
    }

    if (first || timeNow - lastOtaCheck >= otaCheckInterval) {
        lastOtaCheck = timeNow;
        ArduinoOTA.handle();
    }

    if (first || timeNow - lastBlink >= blinkInterval) {
        lastBlink = timeNow;
        ledBlinkLoop();
    }

    first = false;
    delay(10);
}

int previousDay = -1;

bool dayChanged()
{
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    if (previousDay == -1) {
        previousDay = t->tm_mday;
        return false;
    }

    bool changed = (t->tm_mday != previousDay);
    previousDay = t->tm_mday;
    return changed;
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
    for (NeohubZone z : NeohubManager.getActiveZones()) {
        NeohubZoneData* d = NeohubManager.getZoneData(z.id);
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

// SIMULATION
//
// // Queue representing the heating loop
// const int heatingLoopDelaySeconds = 60;
// const int measurementDelaySeconds = 10;
// cppQueue	heatingLoop(sizeof(double), heatingLoopDelaySeconds+10, FIFO);
// cppQueue	measurementDelay(sizeof(double), measurementDelaySeconds+10, FIFO);
// bool first = true;

// void simulatedManageValveControls()
// {
//   double loopInitialTemperature = 35;
//   double deltaT = 5;
//   double inputTemperature = 40.0;

//   if (first) {
//     for (int i = 0; i < heatingLoopDelaySeconds; i++) {
//       heatingLoop.push(&loopInitialTemperature);
//     }
//     for (int i = 0; i < measurementDelaySeconds; i++) {
//       measurementDelay.push(&loopInitialTemperature);
//     }
//     ValveManager.outputs.targetValvePosition = 50;  // half open
//     first = false;
//   }

//   double returnTemperature;
//   heatingLoop.pull(&returnTemperature);
//   returnTemperature -= deltaT;
//   double measuredFlowTemperature;
//   measurementDelay.pull(&measuredFlowTemperature);

//   double flowTemperature = returnTemperature + (inputTemperature - returnTemperature) * ValveManager.outputs.targetValvePosition / 100.0;
//   heatingLoop.push(&flowTemperature);
//   measurementDelay.push(&flowTemperature);

//   // ValveManager.setInputs(...);
//   // float inputTemp = OneWireManager.getCalibratedTemperature(Config.getInputSensorId().c_str());
//   // float flowTemp = OneWireManager.getCalibratedTemperature(Config.getFlowSensorId().c_str());
//   // float returnTemp = OneWireManager.getCalibratedTemperature(Config.getReturnSensorId().c_str());
//   ValveManager.setInputs(inputTemperature, measuredFlowTemperature, returnTemperature);
//   ValveManager.calculateValvePosition();
//   ValveManager.sendCurrentOutputs();
//   Serial.printf(
//     "In: %0.1lf, Setpoint: %0.1lf, Valve: %0.1lf, Flow: %0.1lf, Return: %0.1lf (calculated flow: %0.1lf)\n",
//     inputTemperature,
//     ValveManager.getSetpoint(),
//     ValveManager.outputs.targetValvePosition,
//     measuredFlowTemperature,
//     returnTemperature,
//     flowTemperature
//   );
// }
