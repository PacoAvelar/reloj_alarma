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

typedef enum {
    seconds_type, minutes_type, hours_type
} time_types_t;

typedef struct {
    time_types_t time_type;
    uint8_t value;
} time_msg_t;

#define TOP_SECONDS 60
#define TOP_MINUTES 60
#define TOP_HOURS 24

#define HOURS_ALARM 22
#define MINUTES_ALARM 1
#define SECONDS_ALARM 2

#define HOURS_INIT 22
#define MINUTES_INIT 1
#define SECONDS_INIT 58


#define HOURS_EVENT_BIT (1 << 0)
#define MINUTES_EVENT_BIT (1 << 1)
#define SECONDS_EVENT_BIT (1 << 2)

#define DEBUG 1

SemaphoreHandle_t minutes_semaphore;
SemaphoreHandle_t hours_semaphore;
SemaphoreHandle_t mutex_uart;

static QueueHandle_t time_Queue;
void alarm_task(void * args) {

    /*
    *It waits until the hours, minutes & seconds events happen
    *then it takes the UART with a mutex to prevent collision with other tasks 
    * prints "ALARM!" and release the mutex of the UART
    */
    
    for (;;)
    {
        xEventGroupWaitBits(g_time_events,
        SECONDS_EVENT_BIT | MINUTES_EVENT_BIT | HOURS_EVENT_BIT,
                            pdTRUE,
                            pdTRUE,
                            portMAX_DELAY);
        xSemaphoreTake(mutex_uart,portMAX_DELAY);

        PRINTF("\033[2J");
        PRINTF("ALARM! \033[5;10H");
        xSemaphoreGive(mutex_uart);
    }

}


void print_task(void * args) {
    /*
    *The value of each unit it's updated 
    *depending on the time_type received by the Queue.
    */
    static time_msg_t *message;
    static uint8_t sec = SECONDS_INIT;
    static uint8_t min = MINUTES_INIT;
    static uint8_t hr = HOURS_INIT;

    PRINTF("\033[2J");
    for (;;)
    {
#if DEBUG
        xQueueReceive(time_Queue, &message, portMAX_DELAY);
#endif
        switch (message->time_type)
        {
            case seconds_type:
                sec = message->value;
                break;
            case minutes_type:
                min = message->value;
                break;
            case hours_type:
                hr = message->value;
                break;
            default:
            break;
        }
        /*To prevent errors between task
        * a mutex is used for the use of the UART
        */
        xSemaphoreTake(mutex_uart,portMAX_DELAY);

        PRINTF("%d : %d : %d hrs \033[3;10H", hr,min, sec );
        xSemaphoreGive(mutex_uart);
        /*Free memory to prevent overflow
        */
        vPortFree(message);
    }

}

void seconds_task(void *args) {
    static time_msg_t *message;
    static TickType_t LastTimeAwake;
    static uint8_t seconds = SECONDS_INIT;
    /*
    * If the alarm is equal to the minutes and hours of
    *initialization, then the seconds of initialization are checked.
    */
    if ((MINUTES_EVENT_BIT | HOURS_EVENT_BIT)
            == xEventGroupGetBits(g_time_events))
    {
        if (SECONDS_ALARM == seconds)
        {

            xEventGroupSetBits(g_time_events, SECONDS_EVENT_BIT);
        }
    }
#if 0
    QueueHandle_t * seconds_handler = (QueueHandle_t*)(args);
#endif
    /*
    * This task wakes up every second.
    * it increments the value of seconds and if
    * it gets to 0 again. 
    * Sets up an event to increase the value of minutes.
    * Reserves memory for the message and sends it.
    * 
    */
    for (;;)
    {
        LastTimeAwake = xTaskGetTickCount();
        if (seconds < TOP_SECONDS - 1)
        {
            seconds++;

        } else
        {
            seconds = 0;
            xSemaphoreGive(minutes_semaphore);
        }

        if ((MINUTES_EVENT_BIT | HOURS_EVENT_BIT)
                == xEventGroupGetBits(g_time_events))
        {
            if (SECONDS_ALARM == seconds)
            {
                xEventGroupSetBits(g_time_events, SECONDS_EVENT_BIT);
            }
        }

        message = pvPortMalloc(sizeof(time_msg_t));
        message->time_type = seconds_type;
        message->value = seconds;
#if DEBUG
        xQueueSend(time_Queue,&message, 0);
#endif
        vTaskDelayUntil(&LastTimeAwake, pdMS_TO_TICKS(1000));

    }

}

void minutes_task(void *arg) {
    static time_msg_t *message;
    static uint8_t minutes = MINUTES_INIT;
    /*
    * If the alarm is equal to the hours of
    *initialization, then the minutes of initialization are checked.
    */
    if (HOURS_EVENT_BIT == xEventGroupGetBits(g_time_events))
    {
        if (MINUTES_ALARM == minutes)
        {
            xEventGroupSetBits(g_time_events, MINUTES_EVENT_BIT);
        }
    }
    /*
    * This task wakes up every minute wainting for the 60 seconds event
    * it increments the value of minutes and if
    * it gets to 0 again. 
    * Sets up an event to increase the value of hours.
    * Reserves memory for the message and sends it.
    * 
    */
    for (;;)
    {
        xSemaphoreTake(minutes_semaphore, portMAX_DELAY);
        if (minutes < TOP_MINUTES - 1)
        {
            minutes++;
        } else
        {
            minutes = 0;
            xSemaphoreGive(hours_semaphore);
        }
        if (HOURS_EVENT_BIT == xEventGroupGetBits(g_time_events))
        {
            if (MINUTES_ALARM == minutes)
            {
                xEventGroupSetBits(g_time_events, MINUTES_EVENT_BIT);
            }
        }

        message = pvPortMalloc(sizeof(time_msg_t));
        message->time_type = minutes_type;
        message->value = minutes;
#if DEBUG
        xQueueSend(time_Queue, &message, 0);

#endif
    }
}

void hours_task(void * args) {
    static time_msg_t *message;

    static uint8_t hours = HOURS_INIT;
    if (HOURS_ALARM == hours)
    {
        xEventGroupSetBits(g_time_events, HOURS_EVENT_BIT);

    }
    for (;;)
    {
        xSemaphoreTake(hours_semaphore, portMAX_DELAY);
        if (hours < TOP_HOURS - 1)
        {
            hours++;
        } else
        {
            hours = 0;
        }
        if (HOURS_ALARM == hours)
        {
            xEventGroupSetBits(g_time_events, HOURS_EVENT_BIT);

        }

        message = pvPortMalloc(sizeof(time_msg_t));
        message->time_type = hours_type;
        message->value = hours;
#if DEBUG
        xQueueSend(time_Queue, &message, 0);
#endif
    }

}

int main(void) {

    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();

    PRINTF("\033[2J");

    time_Queue = xQueueCreate(1, sizeof(void*));

    minutes_semaphore = xSemaphoreCreateBinary();
    hours_semaphore = xSemaphoreCreateBinary();
    mutex_uart = xSemaphoreCreateMutex();

    xTaskCreate(seconds_task, "Seconds", configMINIMAL_STACK_SIZE+200,
                (void*) (time_Queue), configMAX_PRIORITIES - 1, NULL);

    xTaskCreate(minutes_task, "Minutes", configMINIMAL_STACK_SIZE +200,
                (void*) (time_Queue), configMAX_PRIORITIES - 3, NULL);

    xTaskCreate(hours_task, "Hours", configMINIMAL_STACK_SIZE +200, NULL,
    configMAX_PRIORITIES - 4,NULL);

    xTaskCreate(alarm_task, "Alarm", configMINIMAL_STACK_SIZE +200, NULL,
    configMAX_PRIORITIES - 5,
                NULL);

    xTaskCreate(print_task, "Print", configMINIMAL_STACK_SIZE + 200, NULL,
    configMAX_PRIORITIES - 6,
                NULL);

    g_time_events = xEventGroupCreate();

    vTaskStartScheduler();

    while (1)
    {
    }
    return 0;
}
