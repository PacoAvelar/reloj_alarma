/*
 * Copyright (c) 2017, NXP Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of NXP Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file    Reloj_Alarma.c
 * @brief   Application entry point.
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK64F12.h"
#include "FreeRTOS.h"
#include "fsl_debug_console.h"

#include "task.h"
#include "semphr.h"

#include "event_groups.h"

EventGroupHandle_t g_time_events;



/* TODO: insert other include files here. */

/* TODO: insert other definitions and declarations here. */

/*
 * @brief   Application entry point.
 *
 */

typedef enum {
	seconds_type, minutes_type, hours_type
} time_types_t;

typedef struct

{
	time_types_t time_type;

	uint8_t value;

} time_msg_t;



#define TOP_SECONDS 60
#define TOP_MINUTES 60
#define TOP_HOURS 24

#define HOURS_ALARM 23
#define MINUTES_ALARM 59
#define SECONDS_ALARM 59

#define HOURS_EVENT_BIT (1 << 0)
#define MINUTES_EVENT_BIT (1 << 1)
#define SECONDS_EVENT_BIT (1 << 2)
SemaphoreHandle_t minutes_semaphore;
SemaphoreHandle_t hours_semaphore;
SemaphoreHandle_t mutex_uart;


QueueHandle_t xQueue;



QueueHandle_t time_Queue;
void alarm_task(void * args){
	time_msg_t *message;



}

void print_task(void * args){
	time_msg_t *message;

}



void seconds_task(void *args){
	time_msg_t *message;
	const TickType_t LastTimeAwake ;
	static uint8_t seconds = 0;

#if 0
	QueueHandle_t * seconds_handler = (QueueHandle_t*)(args);
#endif
	for(;;){
		message = pvPortMalloc(sizeof(time_msg_t));
		/*
		 * Falta verificar la parte de alarma
		 */

		if(seconds < TOP_SECONDS){
			seconds ++;

		}else{
			seconds = 0;
			xSemaphoreGive(minutes_semaphore);
		}

		message->time_type = seconds_type;
		message->value =seconds;
		xQueueSend(time_Queue, &message, portMAX_DELAY);
		vTaskDelayUntil(&LastTimeAwake,pdMS_TO_TICKS(1000));

	};

}


void minutes_task(void *arg){
	time_msg_t *message;
	static uint8_t minutes = 0;

	xSemaphoreTake(minutes_semaphore, portMAX_DELAY);
	if(minutes < TOP_SECONDS){
		minutes++;
	}else{
		minutes = 0;
		xSemaphoreGive(hours_semaphore);
	}
	message->time_type = minutes_type;
	message->value = minutes;
	xQueueSend(time_Queue, &message, portMAX_DELAY);
	xSemaphoreTake(minutes_semaphore,portMAX_DELAY);

}

void hours_task(void * args){
	time_msg_t *message;

	static uint8_t hours = 0;

		xSemaphoreTake(hours_semaphore, portMAX_DELAY);
		if(hours < TOP_HOURS){
			hours++;
		}else{
			hours = 0;
		}
		xEventGroupSetBits(g_time_events, HOURS_EVENT_BIT);

		message->time_type = hours_type;
		message->value = hours;
		xQueueSend(time_Queue, &message, portMAX_DELAY);
		xSemaphoreTake(hours_semaphore,portMAX_DELAY);


}



int main(void) {

	/* Init board hardware. */
	BOARD_InitBootPins();
	BOARD_InitBootClocks();
	BOARD_InitBootPeripherals();
	/* Init FSL debug console. */
	BOARD_InitDebugConsole();



	xTaskCreate(seconds_task, "Seconds", configMINIMAL_STACK_SIZE, (void*)(xQueue), configMAX_PRIORITIES - 0, NULL);
	xTaskCreate(minutes_task, "Minutes", configMINIMAL_STACK_SIZE, (void*)(xQueue), configMAX_PRIORITIES - 1, NULL);
	xTaskCreate(hours_task, "Hours", configMINIMAL_STACK_SIZE, (void*)(xQueue), configMAX_PRIORITIES - 2, NULL);
	xTaskCreate(alarm_task, "Alarm", configMINIMAL_STACK_SIZE, (void*)(xQueue), configMAX_PRIORITIES - 3, NULL);
	xTaskCreate(print_task, "Print", configMINIMAL_STACK_SIZE, (void*)(xQueue), configMAX_PRIORITIES - 4, NULL);



	minutes_semaphore = xSemaphoreCreateBinary();
	hours_semaphore = xSemaphoreCreateBinary();
	mutex_uart = xSemaphoreCreateMutex();

	g_time_events = xEventGroupCreate();

	vTaskStartScheduler();

	while (1) {
	}
	return 0;
}
