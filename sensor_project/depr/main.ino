#include <stdbool.h>
#include "Ultrasonic.h"
#include <Arduino_FreeRTOS.h>
#include <Arduino.h>
#define MIN_TAR_DISTANCE 150
#define MAX_TAR_DISTANCE 210

TaskHandle_t T_SensorRead;
TaskHandle_t T_ImageCapture;

struct SensorReading {
    long targetDistance;
    bool heatSensorReading;
} SensorReading_struct;

Ultrasonic ultrasonicSensor(12, 13);
volatile long currRegisteredTargetDistance;
volatile bool heatSensorActive;
int pirPin = 8;

void TaskReadSensors(void *pvParameters);

void setup() {
    Serial.begin(19200);
    xTaskCreate(TaskReadSensors, "TaskReadSensor", 2048, NULL, 1, &T_SensorRead);
    xTaskCreate(TaskImageCapture, "TaskImageCapture", 2048, NULL, 1, &T_ImageCapture);
}

QueueHandle_t queue1 = xQueueCreate(1, sizeof(SensorReading_struct));

void TaskReadSensors(void *pvParameters) {
    for(;;) {
        long tarDistance = ultrasonicSensor.Ranging(CM);
        bool currPirReading = digitalRead(pirPin)
        SensorReading data;
        data.targetDistance = tarDistance;
        data.heatSensorReading = currPirReading;

        if (currRegisteredTargetDistance > MIN_TAR_DISTANCE && currRegisteredTargetDistance < MAX_TAR_DISTANCE) {
            if(heatSensorActive) {
                // send the data
                if(xQueueSend(queue1, &data, pdMS_TO_TICKS(100) == pdPASS) {
                    // successful sensor read
                    Serial.println("Reading data");
                    vTaskResume(T_ImageCapture);
                    vTaskSuspend(T_SensorRead);
                }
            }
        }
    }
}

void TaskCaptureImage(void *pvParameters) {

}
