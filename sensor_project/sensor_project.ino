/**
 * @file sensor_project.ino
 * @author Akhil Mandala & Gen Sakura
 * @date July 25, 2023
 * @brief The sketch file to be loaded onto an Arduino MEGA board for a security system.
 *
 * This half of our system handles processing data from two sensors (ultrasonic and PIR) and activating/deactivating the overall security system accordingly. The system reads data from a secondary Arduino NANO board responsible for image processing. Depending on that output, this system displays predicted information about whether someone is in the system's presence on a LCD display.
 */

#include <stdbool.h>
#include "Ultrasonic.h"
#include <Arduino_FreeRTOS.h>
#include <LiquidCrystal.h>
#include <Arduino.h>
#include <time.h>
#include <task.h>
#include <queue.h>
#include "memorysaver.h"

/** @brief LED Pin, required by spec */
#define LED_PIN 46

/** @brief Sensor variables */
int pirPin = 8;
Ultrasonic ultrasonicSensor(9, 10);
volatile long currRegisteredTargetDistance;
volatile bool heatSensorActive;

/** @brief Defines for activating system, used by ultrasonic sensor; defined in cm */
#define MIN_TAR_DISTANCE 150
#define MAX_TAR_DISTANCE 210

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

/** @brief 5 tasks used by system

/**
 * @brief Blink function
 *
 * This function drives an output to an LED connected to pin 47, with a 0.3 s period.
 */
void TaskBlink(void *pvParameters);

/**
 * @brief Read sensor function
 *
 * This function reads the ultrasonic and PIR sensor, and sends an active/inactive signal to the NANO board according to the sensor data. If the PIR sensor is active and the ultrasonic sensor data is within the predefined range [150, 210], then the function sends an active signal to the NANO board.
 */
void TaskReadSensors(void *pvParameters);
TaskHandle_t T_SensorRead;

/**
 * @brief Read NANO board function
 *
 * This function monitors the serial connection for output from the NANO board. This output would contain the preduction scores, and must be read in a synchronous way. When the data packet is fully received, the function calls the TaskProcessIncomingScore function to store the data and update displays.
 * @see TaskProcessIncomingScore
 * @see TaskDisplayOutput
 */
void TaskReadNanoBoard(void *pvParameters);
TaskHandle_t T_ReadNanoBoard;
bool readyForNewData = false;

/**
 * @brief Process incoming data function
 *
 * This function stores incoming data and takes a running average of the last 5 predicted scores.
 * @see TaskReadNanoBoard
 * @see TaskDisplayOutput
 */
void TaskProcessIncomingScore(void *pvParameters);
TaskHandle_t T_ProcessIncomingScore;
float recentPersonScores[5];
float recentNoPersonScores[5];
int numEntries = 0;

/**
 * @brief Display output function
 *
 * This function drives the LED display, changing what is shown depending on the average of the latest predicted scores.
 * @see TaskProcessIncomingScore
 * @see TaskReadNanoBoard
 */
void TaskDisplayOutput(void *pvParameters);
TaskHandle_t T_DisplayOutput;

float lastReadScore[2];
QueueHandle_t queue1 = xQueueCreate(1, sizeof(lastReadScore));

void setup()
{
  Serial.begin(115200);
  xTaskCreate(TaskBlink, "Task Blink Offboard LED", 128, NULL, 1, NULL);
  xTaskCreate(TaskReadSensors, "TaskReadSensor", 2048, NULL, 1, &T_SensorRead);
  xTaskCreate(TaskReadNanoBoard, "TaskReadNanoBoard", 2048, NULL, 1, &T_ReadNanoBoard);
  xTaskCreate(TaskProcessIncomingScore, "TaskProcessIncomingScore", 2048, NULL, 1, &T_ProcessIncomingScore);
  xTaskCreate(TaskDisplayOutput, "TaskDisplayOutput", 1024, NULL, 1, &T_DisplayOutput);

  vTaskStartScheduler();
}

void loop() {}

void TaskBlink(void *pvParameters)
{ // This is a task.
  // initialize digital LED_BUILTIN on pin 13 as an output.
  pinMode(LED_PIN, OUTPUT);

  for (;;)
  {                                       // A Task shall never return or exit.
    digitalWrite(LED_PIN, HIGH);          // turn the LED on (HIGH is the voltage level)
    vTaskDelay(100 / portTICK_PERIOD_MS); // wait for 0.1 second
    digitalWrite(LED_PIN, LOW);           // turn the LED off by making the voltage LOW
    vTaskDelay(200 / portTICK_PERIOD_MS); // wait for 0.2 second
  }
}

void TaskReadSensors(void *pvParameters)
{
  for (;;)
  {
    long tarDistance = ultrasonicSensor.Ranging(CM);
    bool currPirReading = digitalRead(pirPin);
    if (currPirReading)
    {
      Serial.println(tarDistance);
    }

    if ((tarDistance < MAX_TAR_DISTANCE && tarDistance > MIN_TAR_DISTANCE && tarDistance < 500) && currPirReading)
    {
      // capture and send images
      Serial.write("1");
      lcd.clear();
      lcd.print(" Movement ");
      lcd.setCursor(0, 1);
      lcd.print(" Detected! ");
    }
    else
    {
      // capture and send images
      Serial.write("0");
      lcd.clear();
      lcd.print(" Standby ");
    }
  }
}

void TaskReadNanoBoard(void *pvParameters)
{
  char START_CHAR = "<";
  char END_CHAR = ">";
  static bool transferInProgress = false;
  static int8_t score;
  static int idx = 0;
  char rc;

  for (;;)
  {
    while (Serial.available() > 0 && ~readyForNewData)
    {
      lcd.clear();
      lcd.print(" Calculating... ");
      rc = Serial.read();
      if (transferInProgress)
      {
        if (rc != END_CHAR)
        {
          score = (int8_t) rc;
        }
        if (rc == END_CHAR)
        {
          idx = 0;
          transferInProgress = false;
          readyForNewData = true;
          if (xQueueSend(queue1, &score, pdMS_TO_TICKS(100)) == pdPASS)
          {
            vTaskResume(T_ProcessIncomingScore);
            vTaskSuspend(T_ReadNanoBoard);
          }
        }
      }
      else if (rc == START_CHAR)
      {
        transferInProgress = true;
      }
    }
  }
}

void TaskProcessIncomingScore(void *pvParameters)
{
  float inputBuffer[2];
  TickType_t startTime, endTime, runTime;

  if (xQueueReceive(queue1, &inputBuffer, portMAX_DELAY) == pdTRUE)
  {
    if (numEntries == 5)
    {
      for (int i = 0; i < 4; i++)
      {
        recentNoPersonScores[i] = recentNoPersonScores[i + 1];
        recentPersonScores[i] = recentPersonScores[i + 1];
      }
      recentPersonScores[4] = inputBuffer[0];
      recentNoPersonScores[4] = inputBuffer[1];
    }
    else
    {
      recentPersonScores[numEntries] = inputBuffer[0];
      recentNoPersonScores[numEntries] = inputBuffer[1];
      numEntries += 1;
    }
  }
}

void TaskDisplayOutput(void *pvParameters)
{
  for (;;)
  {
    float avgPersonScore = 0.0;
    float avgNoPersonScore = 0.0;

    for (int i = 0; i < numEntries; i++)
    {
      avgPersonScore += (float)recentPersonScores[i] / numEntries;
      avgNoPersonScore += (float)recentNoPersonScores[i] / numEntries;
    }

    if (avgPersonScore > 0.8 && avgNoPersonScore < 0.2)
    {
      // High likelihood
      lcd.clear();
      lcd.print("High Likelihood");
      lcd.setCursor(0, 1);
      lcd.print("Intruder Present");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else if (avgPersonScore > 0.7 && avgNoPersonScore < 0.5)
    {
      // Somewhat likeley
      lcd.clear();
      lcd.print("Somewhat Likely");
      lcd.setCursor(0, 1);
      lcd.print("Intruder Present");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else if (avgPersonScore > 0.5 && avgNoPersonScore > 0.5)
    {
      // Unknown / cannot determine
      lcd.clear();
      lcd.print("Cannot Determine");
      lcd.setCursor(0, 1);
      lcd.print("Intruder Present");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    else if (avgPersonScore < 0.5 && avgNoPersonScore > 0.5)
    {
      // Unlikely
      lcd.clear();
      lcd.print("Unlikely");
      lcd.setCursor(0, 1);
      lcd.print("Intruder Present");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}