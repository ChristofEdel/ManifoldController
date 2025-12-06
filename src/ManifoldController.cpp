#include <ArduinoOTA.h>
#include <SdFat.h>  // SD Card with FAT filesystem
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
#include "sensorLog.h"

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

// Singletons for thermistors, onewire sensors and their map -------
const int maxSensorCount = 20;
OneWireManager oneWireSensors;
SensorMap sensorMap(maxSensorCount);
ValveManager valveManager;

// SD Card access
SdFs sd;
MyMutex sdCardMutex("::sdCardMutex");
#define SD_CONFIG SdSpiConfig(sdCardCsPin, SD_SCK_MHZ(50))

// The valve position (preserved across resets)
RTC_NOINIT_ATTR double lastKownValvePosition;
RTC_NOINIT_ATTR double lastKnownFlowSetpoint;

void readSensors();
bool dayChanged();
void startValveControlTask();
void triggerValveControls(bool);

void setup()
{
    esp_log_level_set("*", ESP_LOG_ERROR);

    // Get rid of all watchdogs - they cause problems since OTA was added.
    disableCore0WDT();
    disableCore1WDT();
    disableLoopWDT();
    esp_task_wdt_delete(NULL);
    esp_task_wdt_deinit();

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

    // Initialise logging to serial
    MyLog.enableSerialLog();
    MyLog.logTimestamps(MyRtc);

    MyWebLog.enableSerialLog();
    MyWebLog.logTimestamps(MyRtc);

    MyCrashLog.logTimestamps(MyRtc);

    MyLog.println("-------------------------------------------------------------------------------------------");

    // Initialise SD card access and start logging to SD card as well
    MyLog.print("Initializing SD card...");
    if (!sd.begin(SD_CONFIG)) {
        sd.initErrorHalt(&Serial);
    }
    MyLog.println("done.");
    MyLog.enableSdCardLog("log.txt", &sd, &sdCardMutex);
    MyWebLog.enableSdCardLog("weblog.txt", &sd, &sdCardMutex);
    MyCrashLog.enableSdCardLog("crashlog.txt", &sd, &sdCardMutex);

    MyLog.println("-------------------------------------------------------------------------------------------");
    Config.loadFromSdCard(sd, sdCardMutex, "config.json", sensorMap, &oneWireSensors, NeohubManager);
    sensorMap.clearChanged();

    MyLog.printlnSdOnly("-------------------------------------------------------------------------------------------");

    // Connect to WiFi
    MyWiFi.connect();
    if (Config.getHostname() != "") {
        MyWiFi.setHostname(Config.getHostname());
    }

    MyWiFi.updateRtcFromTimeServer(&MyRtc);
    String resetReason = getResetReason();
    clearSoftwareResetReason();
    MyLog.print("Last reset reason: ");
    MyLog.println(resetReason);
    MyCrashLog.print("RESTART - Last reset reason: ");
    MyCrashLog.println(resetReason);

    // Initialisations for several modules
    setupOta();
    oneWireSensors.setup(oneWirePin);
    for (int i = 0; i < sensorMap.getCount(); i++) {
        oneWireSensors.addKnownSensor(sensorMap[i]->id.c_str());
    }

    valveManager.setup();
    valveManager.setRooomSetpoint(Config.getRoomSetpoint());
    if (mustClearRtcMemory()) {
        lastKnownFlowSetpoint = 0;
        lastKownValvePosition = 0;
    }
    if (lastKnownFlowSetpoint > 1) {  // sometimes after reset this value is -0.0
        MyLog.printf("Initialising flow setpoint to %.1f degrees\n", lastKnownFlowSetpoint);
        valveManager.setFlowSetpoint(lastKownValvePosition);
    }
    if (lastKownValvePosition > 1) {  // sometimes after reset this value is -0.0
        MyLog.printf("Initialising valve position to %.0f%%\n", lastKownValvePosition);
        valveManager.setValvePosition(lastKownValvePosition);
    }
    MyWebServer.setup(&sd, &sdCardMutex, &sensorMap, &valveManager, &oneWireSensors);
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

    Watchdog::printRegistrations();

    // Kick the watchdog so the hardware watchdog knows the main loop is alive.
    // kickWatchdog();

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
    oneWireSensors.readAllSensors();
    vTaskPrioritySet(NULL, prio);
}

TaskHandle_t valveControlTaskHandle = NULL;  // Task for boiler control
QueueHandle_t valveControlQueue = NULL;

void manageValveControls()
{
    int tempCount = 0;
    double temperatureTotal = 0;
    for (NeohubZone z : NeohubManager.getActiveZones()) {
        NeohubZoneData* d = NeohubManager.getZoneData(z.id);
        if (d && d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
            temperatureTotal += d->roomTemperature;
            tempCount++;
        }
    }
    float roomTemperature = NeohubZoneData::NO_TEMPERATURE;
    if (tempCount > 0) roomTemperature = temperatureTotal / tempCount;

    float inputTemperature = oneWireSensors.getCalibratedTemperature(Config.getInputSensorId().c_str());
    float flowTemperature = oneWireSensors.getCalibratedTemperature(Config.getFlowSensorId().c_str());
    float returnTemperature = oneWireSensors.getCalibratedTemperature(Config.getReturnSensorId().c_str());

    valveManager.setInputs(roomTemperature, flowTemperature, inputTemperature, returnTemperature);
    valveManager.calculateValvePosition();
    valveManager.sendCurrentValvePosition();

    lastKnownFlowSetpoint = valveManager.outputs.targetFlowTemperature;
    lastKownValvePosition = valveManager.outputs.targetValvePosition;

    // Serial.printf(
    //   "In: %0.1lf, Setpoint: %0.1lf, Valve: %0.1lf, Flow: %0.1lf, Return: %0.1lf\n",
    //   inputTemperature,
    //   valveManager.getSetpoint(),
    //   valveManager.outputs.targetValvePosition,
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

            // Then log if requested
            if (writeLogLine) logSensors();

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
//     valveManager.outputs.targetValvePosition = 50;  // half open
//     first = false;
//   }

//   double returnTemperature;
//   heatingLoop.pull(&returnTemperature);
//   returnTemperature -= deltaT;
//   double measuredFlowTemperature;
//   measurementDelay.pull(&measuredFlowTemperature);

//   double flowTemperature = returnTemperature + (inputTemperature - returnTemperature) * valveManager.outputs.targetValvePosition / 100.0;
//   heatingLoop.push(&flowTemperature);
//   measurementDelay.push(&flowTemperature);

//   // valveManager.setInputs(...);
//   // float inputTemp = oneWireSensors.getCalibratedTemperature(Config.getInputSensorId().c_str());
//   // float flowTemp = oneWireSensors.getCalibratedTemperature(Config.getFlowSensorId().c_str());
//   // float returnTemp = oneWireSensors.getCalibratedTemperature(Config.getReturnSensorId().c_str());
//   valveManager.setInputs(inputTemperature, measuredFlowTemperature, returnTemperature);
//   valveManager.calculateValvePosition();
//   valveManager.sendCurrentOutputs();
//   Serial.printf(
//     "In: %0.1lf, Setpoint: %0.1lf, Valve: %0.1lf, Flow: %0.1lf, Return: %0.1lf (calculated flow: %0.1lf)\n",
//     inputTemperature,
//     valveManager.getSetpoint(),
//     valveManager.outputs.targetValvePosition,
//     measuredFlowTemperature,
//     returnTemperature,
//     flowTemperature
//   );
// }
