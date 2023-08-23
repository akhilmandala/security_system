#include <stdbool.h>
#include "Ultrasonic.h"

Ultrasonic ultrasonicSensor(12, 13);
volatile long currRegisteredTargetDistance;
volatile bool heatSensorActive;

typedef enum {
    READY,   // The task is ready to be run
    RUNNING, // The task is currently running
    DEAD     // The task has terminated and is not ready to be run
} TaskStatus;

struct TCB {
  // Define variables according to your requirement
  void *function;
  int uniqueID;           // Unique ID for the task
  char taskName[21];      // Task name, up to 20 characters
  int timesRestarted;     // Total number of times the task has been started/restarted
  TaskStatus status;      // Current status of the task
} TCB_struct; // name given to the TCB struct

TCB taskPriorityList[10];
TCB* activeTaskList[10];
TCB* deadTaskList[10]; 

/**
 * This helper function acts as a TCB constructor.
*/
TCB createTCBTask(int *function(), int uniqueID, char taskName, int timesRestarted, \
TaskStatus status) {
  TCB tmp;
  tmp.function = function;
  tmp.uniqueID = uniqueID;
  strncpy(tmp.taskName, taskName, 21);
  tmp.timesRestarted = timesRestarted;
  tmp.status = status;
  return tmp;
}

void setup() {
    TCB readUltraSonicSensorTask = createTCBTask(readUltraSonicSensor, 1, "a", 0, READY);
    TCB readPIRSensorTask = createTCBTask(readPIRSensor, 2, "b", 0, READY);
    TCB captureImageTask = createTCBTask(captureImage, 3, "c", DEAD);
    currRegisteredTargetDistance = -1;
    heatSensorActive = false;
}

void loop() {
    currRegisteredTargetDistance = ultrasonicSensor.Ranging(CM);
    
}

void readUltraSonicSensor() {

}