#include "../MyWebServer.h"

struct KnownTask {
    const char* name;
    const char* purpose;
};

KnownTask knownTasks[] = {
    {"IDLE0",          "OS       | Idle    | Core 0 Idle Task"},
    {"IDLE1",          "OS       | Idle    | Core 1 Idle Task"},
    {"tiT",            "OS       | Idle    | Yet another idle task"},
    {"mdns",           "OS       | Network | mDNS service task"},
    {"async_tcp",      "AsyncTCP | Network | TCP event dispatcher and web server"},
    {"sys_evt",        "OS       | Network | System network event dispatcher"},
    {"wifi",           "OS       | Network | Wifi connecetion handler"},
    {"websocket_task", "OS       | Network | UDP Websocket handler"},
    {"esp_timer",      "OS       | Timers  | ESP precision timer dispatcher"},
    {"Tmr Svc",        "OS       | Timers  | System timer service"},
    {"ipc0",           "OS       |         | Inter-Processer Communication"},
    {"loopTask",       "Arduino  |         | Arduino core task for setup and loop" },
    {"ValveControl",   "APP      |         | Valve control loop"},
    {"arduino_events", "Arduino  | Network | Arduino network event dispatcher"},
    {"ipc1",           "OS       |         | Inter-Processer Communication"},
    {"BgFileWriter",   "APP      |         | Backrgound (log) file writer"},
    {"Watchdogs",      "APP      |         | Watchdogs"},
    {"NeohubProxy",    "APP      |         | Neohub proxy polling loop"},
    { nullptr,         "?        | ?       | ?" }
};

int taskCompare(const void *a, const void *b) {
    const TaskStatus_t *tsA = *(const TaskStatus_t **)a;
    const TaskStatus_t *tsB = *(const TaskStatus_t **)b;

    if (tsA->xCoreID < tsB->xCoreID) return -1;
    if (tsA->xCoreID > tsB->xCoreID) return 1;

    if (tsA->uxCurrentPriority < tsB->uxCurrentPriority) return -1;
    if (tsA->uxCurrentPriority > tsB->uxCurrentPriority) return 1;

    return strcmp(tsA->pcTaskName, tsB->pcTaskName);
}

void printTaskLine(AsyncResponseStream *response, TaskStatus_t& ts)
{
    UBaseType_t aff = ts.xCoreID;   // bitmask
    const char* affStr = aff > 1 ? "0|1" : aff == 1 ? " 1" : " 0";

    const char * stateStr;
    switch(ts.eCurrentState) {
      case eRunning: stateStr = "RUN"; break;
      case eReady: stateStr = "RDY"; break;
      case eBlocked: stateStr = "BLK"; break;
      case eSuspended: stateStr = "SUS"; break;
      case eDeleted: stateStr = "DEL"; break;
      case eInvalid: stateStr = "INV"; break;
      default: stateStr = "??"; break;
    }

    KnownTask *t = knownTasks;
    while (t->name) {
      if (strcmp(t->name, ts.pcTaskName) == 0) break;
      t++;
    }
    const char *purpose = t->purpose;

    response->printf(
      "%c %-18s|  %-3s | %4u |  %-3s  | %8u | %s\n",
      ts.eCurrentState == eRunning ? '*' : ' ',
      ts.pcTaskName, affStr,
      ts.uxCurrentPriority,
      stateStr,
      ts.usStackHighWaterMark,
      purpose
    );

}

void CMyWebServer::respondWithTaskList(AsyncWebServerRequest *request) {
  AsyncResponseStream* response = request->beginResponseStream("text/plain");
  response->println("Task List:");
  response->println();

  // Get all tasks
  UBaseType_t n = uxTaskGetNumberOfTasks();
  TaskStatus_t *ts = (TaskStatus_t*)malloc(n * sizeof(TaskStatus_t));
  uint32_t totalRunTime;
  n = uxTaskGetSystemState(ts, n, &totalRunTime);

  // Convert to pointer array and sort
  TaskStatus_t **tsp = (TaskStatus_t**)malloc(n * sizeof(TaskStatus_t*));
  for(int i = 0; i < n; i++) tsp[i] = &ts[i];
  qsort(tsp, n, sizeof(TaskStatus_t *), taskCompare);

  // Print table
  response->println("Task                | core | prio | state | stack HW | Owner    | Purpose | Description");
  response->println("--------------------+------+------+-------+----------+----------+---------+------------------------------");

  for (UBaseType_t i = 0; i < n; ++i) if (tsp[i]->xCoreID == 0) printTaskLine(response, *tsp[i]);
  response->println("--------------------+------+------+-------+----------+----------+---------+------------------------------");
  for (UBaseType_t i = 0; i < n; ++i) if (tsp[i]->xCoreID == 1) printTaskLine(response, *tsp[i]);
  response->println("--------------------+------+------+-------+----------+----------+---------+------------------------------");
  for (UBaseType_t i = 0; i < n; ++i) if (tsp[i]->xCoreID >1) printTaskLine(response, *tsp[i]);

  response->println("--------------------+------+------+-------+----------+----------+---------+------------------------------");
  free(ts);
  request->send(response);
}



