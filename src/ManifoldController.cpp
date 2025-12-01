#include <SdFat.h>                // SD Card with FAT filesystem
#include "MyWiFi.h"               // WiFi access
#include "MyLog.h"                // Logging to serial and, if available, SD card
#include "MyRtc.h"                // Real time clock
#include "OneWireSensorManager.h" // OneWire temperature sensor reading and management
#include "webserver/MyWebServer.h"// Request handing and web page generator
#include "SensorMap.h"            // Sensor name mapping
#include "MemDebug.h"
#include "MyMutex.h"
#include "LedBlink.h"
#include "MyConfig.h"
#include "Watchdog.h"
#include "EspTools.h"
#include <ArduinoOTA.h>
#include "Arduino.h"
#include "ValveManager.h"
#include "NeohubManager.h"
#include <cppQueue.h>

// Pin Assignments - digital pins --------------------------------
const uint8_t openThermInPin          = A0;         // Repurposed analog pins for OpenTherm module I/O
const uint8_t openThermOutPin         = A1;
// A2 and A3 - used by I2C bus
// A4 - A5: available
// A6 - A7 - reserved for "230V present" inputs
const uint8_t i2cSdaPin               = D0;         // I2 pins
const uint8_t i2cSclPin               = D1;
// Pin 2-4: reserved for relays
const uint8_t oneWirePin              = D5;         // OnwWire sensor interface
// Pin 6/7 - unallocated
//           pin 7 causes problems with
//           SPI if used for OneWire, 
//           best avoided!
// Pin 8/9: Reserved for UART
const uint8_t sdCardCsPin             = D10;        // SD card chip select
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
MyMutex sdCardMutex;
#define SD_CONFIG SdSpiConfig(sdCardCsPin, SD_SCK_MHZ(50))

// The valve position (preserved across resets)
RTC_DATA_ATTR double lastKownValvePosition;


void checkForNewSensors();
void readAndLogSensors();
String getSensorHeaderLine();
String getSensorLogLine();
void logSensorIssues();
bool dayChanged();
void startValveControlTask();
void triggerValveControls();

void setup() {
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

  const int serialWaitTimeoutMs = 3000; // Timeout in case we are not debugging 
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
  Config.loadFromSdCard(sd, sdCardMutex, "config.json", sensorMap , &oneWireSensors, NeohubManager);
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
  valveManager.setup();
  // valveManager.setRooomSetpoint(Config.getRoomSetpoint());
  valveManager.setFlowSetpoint(Config.getFlowSetpoint());
  valveManager.setValvePosition(lastKownValvePosition);
  MyWebServer.setup(&sd, &sdCardMutex, &sensorMap, &valveManager, &oneWireSensors);
  ledBlinkSetup();

  startValveControlTask();

  MyLog.println("-------------------------------------------------------------------------------------------");

}


void loop() {

  // How often we do what
  const unsigned long timeSyncInterval             = 60 * 60 * 1000; // Synchronise time every hour
  const unsigned long sensorCheckInterval          = 60000;          // Check for new or removed sensors
  const unsigned long valvePositionControlInterval = 1000;           // Read sensors and set control vale poistion
  const unsigned long memoryCheckInterval          = 10000;          // Check for possible memory leaks
  const unsigned long blinkInterval                = 100;            // Blink so we can see
  const unsigned long otaCheckInterval             = 1000;           // Over-The-Air updates
  
  // When we last did that
  static unsigned long lastTimeSync = 0;
  static unsigned long lastSensorCheck = 0;
  static unsigned long lastValvePositionControl = 0;
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

  if (first || timeNow - lastSensorCheck >= sensorCheckInterval) {
    lastSensorCheck = timeNow;
    checkForNewSensors();
    if (dayChanged()) {
      logSensorIssues();
    }
  }

  if (first || timeNow - lastValvePositionControl >= valvePositionControlInterval) {
    lastValvePositionControl = timeNow;
    triggerValveControls();
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

bool dayChanged() {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);

    if (previousDay == -1) {
        previousDay = t->tm_mday;
        return false;
    }

    bool changed = (t->tm_mday != previousDay);
    previousDay = t->tm_mday;
    return changed;
}

void checkForNewSensors() {
  // Add any new OneWire sensors to the map if they are not there
  oneWireSensors.scanForSensors();
  for (int i = 0; i < oneWireSensors.getCount(); i++) {
    const char *id = oneWireSensors[i].id;
    if (sensorMap.getNameForId(id).isEmpty()) {
      sensorMap.setNameForId(id, id);
    }
  }
  oneWireSensors.readAllSensors();
}

char sensorDataFileName[] = "Sensor Values yyyy-mm-dd.csv";
char * getSensorDataFileName() {
  sprintf(sensorDataFileName, "Sensor Values %s.csv", MyRtc.getTime().getDateText().c_str());
  return sensorDataFileName;
}

void readAndLogSensors() {

  UBaseType_t prio = uxTaskPriorityGet(NULL);
  vTaskPrioritySet(NULL, 3);
  oneWireSensors.readAllSensors();
  vTaskPrioritySet(NULL, prio);

  if (sdCardMutex.lock()) {
    char * dataFileName = getSensorDataFileName();

    bool printHeaderLine = !sd.exists(dataFileName);
    if (sensorMap.hasChanged()) {
      printHeaderLine = true;
      // sensorMap.dump(Serial);
      sensorMap.clearChanged();
    } 

    FsFile dataFile = sd.open(dataFileName, (O_RDWR | O_CREAT | O_AT_END));
    String logLine = getSensorLogLine();
    if (dataFile && dataFile.isOpen()) {
      if (printHeaderLine) dataFile.println(getSensorHeaderLine());
      logLine = getSensorLogLine();
      dataFile.println(logLine);
      dataFile.close();
      sdCardMutex.unlock();
    }
    else {
      sdCardMutex.unlock();
      MyLog.print("error opening ");
      MyLog.println(dataFileName);
    }
  }
}

void logSensorHeaderLine() {
  if (sdCardMutex.lock()) {
    char * dataFileName = getSensorDataFileName();
    FsFile dataFile = sd.open(dataFileName, (O_RDWR | O_CREAT | O_AT_END));
    if (dataFile && dataFile.isOpen()) {
      dataFile.println(getSensorHeaderLine());
      dataFile.close();
    }
    sdCardMutex.unlock();
  }
}

String getSensorLogLine() {
    String result;
    result = MyRtc.getTime().getTimestampText();

    result += ",";
    result += String(valveManager.getFlowSetpoint(),1);
    result += ",";
    if (valveManager.inputs.inputTemperature > -50) result += String(valveManager.inputs.inputTemperature,1);
    result += ",";
    if (valveManager.inputs.returnTemperature > -50) result += String(valveManager.inputs.returnTemperature,1);
    result += ",";
    result += String(valveManager.outputs.targetValvePosition,1);
    result += "%,";
    if (valveManager.inputs.flowTemperature > -50) result += String(valveManager.inputs.flowTemperature,1);

    String inputSensorId = Config.getInputSensorId();
    String flowSensorId = Config.getFlowSensorId();
    String returnSensorId = Config.getReturnSensorId();

    int n = sensorMap.getCount();
    for (int i = 0; i < n; i++) {
      SensorMapEntry * entry = sensorMap[i];
      if (entry->id == inputSensorId) continue;
      if (entry->id == flowSensorId) continue;
      if (entry->id == returnSensorId) continue;
      Sensor * sensor = oneWireSensors.getSensor(entry->id.c_str());
      float temperature = SensorManager::SENSOR_NOT_FOUND;
      if (sensor) {
        temperature = sensor->calibratedTemperature();
      }
      result += ",";
      if (temperature > -50) {
        result += String(temperature,1);
      }
    }
    return result;
}

String getSensorHeaderLine() {
    String result;
    result = "Time,Setpoint,Input,Return,Valve,Flow";

    String inputSensorId = Config.getInputSensorId();
    String flowSensorId = Config.getFlowSensorId();
    String returnSensorId = Config.getReturnSensorId();

    int n = sensorMap.getCount();
    for (int i = 0; i < n; i++) {
      SensorMapEntry *entry = sensorMap[i];
      if (entry->id == inputSensorId) continue;
      if (entry->id == flowSensorId) continue;
      if (entry->id == returnSensorId) continue;
      result += ",";
      result += entry->name;
    }
    
    return result;
}

void logSensorIssues() {
  int totalReadings = 0;
  int totalCrcErrors = 0;
  int totalNoResponseErrors = 0;
  int totalOtherErrors = 0;
  int totalFailures = 0;
  for (int index = 0; index < oneWireSensors.getCount(); index++) {
    OneWireSensor & sensor = oneWireSensors[index];
    totalReadings += sensor.readings;
    totalCrcErrors += sensor.crcErrors;
    totalNoResponseErrors += sensor.noResponseErrors;
    totalOtherErrors += sensor.otherErrors;
    totalFailures += sensor.failures;
    sensor.readings = 0;
    sensor.crcErrors = 0;
    sensor.noResponseErrors = 0;
    sensor.otherErrors = 0;
    sensor.failures = 0; 
  }
  if (totalReadings > 0 && (totalCrcErrors > 0 || totalNoResponseErrors > 0 || totalFailures > 0)) {
    MyLog.print("Cumulative sensor issues: ");
    if (totalCrcErrors > 0) {
      MyLog.print(totalCrcErrors);
      MyLog.print(" CRC errors (");
      MyLog.print((double)totalCrcErrors * 100.0 / (double)totalReadings, 1);
      MyLog.print("%), ");
    }
    if (totalNoResponseErrors > 0) {
      MyLog.print(totalNoResponseErrors);
      MyLog.print(" missing responses (");
      MyLog.print((double)totalNoResponseErrors * 100.0 / (double)totalReadings, 1);
      MyLog.print("%), ");
    }
    MyLog.print(totalFailures);
    MyLog.print(" failures (");
    MyLog.print((double)totalFailures * 100.0 / (double)totalReadings, 1);
    MyLog.println("%).");
  }
}


TaskHandle_t valveControlTaskHandle = NULL;          // Task for boiler control
QueueHandle_t valveControlQueue = NULL;

void manageValveControls() {
  int tempCount = 0;
  double temperatureTotal = 0;
  for (NeohubZone z: NeohubManager.getActiveZones()) {
    NeohubZoneData *d = NeohubManager.getZoneData(z.id);
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
  valveManager.sendOutputs();
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

void triggerValveControls() {
  // Send the hot water call state to the background task which manages the boiler controls
  static bool dummyParameter = false;
  xQueueSend(valveControlQueue, &dummyParameter, 0);
}

// Task function that runs the boiler control in the background
void valveControlTask(void *parameter) {
  bool dummyParameter = false;
  for(;;) {
    // Wait for notification from main loop
    if(xQueueReceive(valveControlQueue, &dummyParameter, portMAX_DELAY) == pdTRUE) {
      // Drain any additional messages — only the last one is kept
      bool tmp;
      while (xQueueReceive(valveControlQueue, &tmp, 0) == pdTRUE) {
        dummyParameter = tmp;
      }
      manageValveControls();
      lastKownValvePosition = valveManager.outputs.targetValvePosition;
      readAndLogSensors();
    }
  }
}

void startValveControlTask() {
  // Create queue for boiler control task
  valveControlQueue = xQueueCreate(1, sizeof(bool));
  
  // Create boiler control task
  xTaskCreate(
    valveControlTask,         // Task function
    "ValveControl",           // Task name
    4096,                      // Stack size (bytes)
    NULL,                      // Parameter to pass
    1,                         // Task priority
    &valveControlTaskHandle   // Task handle
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
//   valveManager.sendOutputs();
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

