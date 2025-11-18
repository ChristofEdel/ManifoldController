#include "../MyWebServer.h"

int taskCompare(const void *a, const void *b) {
    const TaskStatus_t *tsA = *(const TaskStatus_t **)a;
    const TaskStatus_t *tsB = *(const TaskStatus_t **)b;

    if (tsA->xCoreID < tsB->xCoreID) return -1;
    if (tsA->xCoreID > tsB->xCoreID) return 1;

    if (tsA->uxCurrentPriority < tsB->uxCurrentPriority) return -1;
    if (tsA->uxCurrentPriority > tsB->uxCurrentPriority) return 1;

    return strcmp(tsA->pcTaskName, tsB->pcTaskName);
}

size_t printTaskLine(WiFiClient &client, TaskStatus_t& ts)
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

    return client.printf(
      "%c %-18s|  %-3s | %4u |  %-3s  | %u\n",
      ts.eCurrentState == eRunning ? '*' : ' ',
      ts.pcTaskName, affStr,
      ts.uxCurrentPriority,
      stateStr,
      ts.usStackHighWaterMark
    );

}

size_t CMyWebServer::respondWithTaskList(WiFiClient &client) {
  size_t totalBytesSent = 0;
  totalBytesSent += this->startHttpResponse(client, "200 OK", "text/plain");
  totalBytesSent += client.println("Task List:");
  totalBytesSent += client.println();

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
  totalBytesSent += client.println("Task                | core | prio | state | stack HW");
  totalBytesSent += client.println("--------------------+------+------+-------+---------");

  for (UBaseType_t i = 0; i < n; ++i) if (tsp[i]->xCoreID == 0) totalBytesSent += printTaskLine(client, *tsp[i]);
  totalBytesSent += client.println("--------------------+------+------+-------+---------");
  for (UBaseType_t i = 0; i < n; ++i) if (tsp[i]->xCoreID == 1) totalBytesSent += printTaskLine(client, *tsp[i]);
  totalBytesSent += client.println("--------------------+------+------+-------+---------");
  for (UBaseType_t i = 0; i < n; ++i) if (tsp[i]->xCoreID >1) totalBytesSent += printTaskLine(client, *tsp[i]);

  totalBytesSent += client.println("--------------------+------+------+-------+---------");
  free(ts);
  return totalBytesSent;
}



