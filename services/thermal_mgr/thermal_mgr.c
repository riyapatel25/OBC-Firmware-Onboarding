#include "thermal_mgr.h"
#include "errors.h"
#include "lm75bd.h"
#include "console.h"
#include "logging.h"
#include <FreeRTOS.h>
#include <os_task.h>
#include <os_queue.h>

#include <string.h>

#define THERMAL_MGR_STACK_SIZE 256U

static TaskHandle_t thermalMgrTaskHandle;
static StaticTask_t thermalMgrTaskBuffer;
static StackType_t thermalMgrTaskStack[THERMAL_MGR_STACK_SIZE];

#define THERMAL_MGR_QUEUE_LENGTH 10U
#define THERMAL_MGR_QUEUE_ITEM_SIZE sizeof(thermal_mgr_event_t)

static QueueHandle_t thermalMgrQueueHandle;
static StaticQueue_t thermalMgrQueueBuffer;
static uint8_t thermalMgrQueueStorageArea[THERMAL_MGR_QUEUE_LENGTH * THERMAL_MGR_QUEUE_ITEM_SIZE];

static void thermalMgr(void *pvParameters);

void initThermalSystemManager(lm75bd_config_t *config) {
  memset(&thermalMgrTaskBuffer, 0, sizeof(thermalMgrTaskBuffer));
  memset(thermalMgrTaskStack, 0, sizeof(thermalMgrTaskStack));
  
  thermalMgrTaskHandle = xTaskCreateStatic(
    thermalMgr, "thermalMgr", THERMAL_MGR_STACK_SIZE,
    config, 1, thermalMgrTaskStack, &thermalMgrTaskBuffer);

  memset(&thermalMgrQueueBuffer, 0, sizeof(thermalMgrQueueBuffer));
  memset(thermalMgrQueueStorageArea, 0, sizeof(thermalMgrQueueStorageArea));

  thermalMgrQueueHandle = xQueueCreateStatic(
    THERMAL_MGR_QUEUE_LENGTH, THERMAL_MGR_QUEUE_ITEM_SIZE,
    thermalMgrQueueStorageArea, &thermalMgrQueueBuffer);

}

error_code_t thermalMgrSendEvent(thermal_mgr_event_t *event) {
  if (thermalMgrQueueHandle == NULL)
    return ERR_CODE_INVALID_STATE;

  if (event == NULL)
    return ERR_CODE_NULL_ERR;

  if (xQueueSend(thermalMgrQueueHandle, event, 0) != pdTRUE)
    return ERR_CODE_QUEUE_FULL;

  return ERR_CODE_SUCCESS;
}

void osHandlerLM75BD(void) {
  /* Implement this function */
  thermal_mgr_event_t event;
  event.type = THERMAL_MGR_EVENT_INTERRUPT;
  thermalMgrSendEvent(&event);
}

static void thermalMgr(void *pvParameters) {

  // Get config data from pvParameters
  lm75bd_config_t config_data = *(lm75bd_config_t *) pvParameters;

  while (1) {
      thermal_mgr_event_t eventFromQueue;
      error_code_t errorCode;
  
      // You can receive the next item in the queue using xQueueReceive
      // if receive from queue is successful
    	if (xQueueReceive(thermalMgrQueueHandle, &eventFromQueue, 0) == pdTRUE) {
        float temperature;
		  // Handle event received from queue
        if( eventFromQueue.type == THERMAL_MGR_EVENT_MEASURE_TEMP_CMD){

           errorCode = readTempLM75BD(config_data.devAddr, &temperature);
           if (errorCode != ERR_CODE_SUCCESS){
            LOG_ERROR_CODE(errorCode);
          }
          else{
            addTemperatureTelemetry(temperature);
          }       
       
           
              
        }

        else if( eventFromQueue.type == THERMAL_MGR_EVENT_INTERRUPT){
            errorCode = readTempLM75BD(config_data.devAddr, &temperature);
            if (errorCode != ERR_CODE_SUCCESS){
            LOG_ERROR_CODE(errorCode);
          }
          else{
            if(temperature > config_data.overTempThresholdCelsius){
              overTemperatureDetected();
          } 
            else {
              safeOperatingConditions();
          }

          }

            

             
        }
		}
    
  }
}

void addTemperatureTelemetry(float tempC) {
  printConsole("Temperature telemetry: %f deg C\n", tempC);
}

void overTemperatureDetected(void) {
  printConsole("Over temperature detected!\n");
}

void safeOperatingConditions(void) { 
  printConsole("Returned to safe operating conditions!\n");
}
