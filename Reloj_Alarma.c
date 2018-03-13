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

/**type definition for recognizing which time element is passed*/
typedef enum {
    seconds_type, minutes_type, hours_type
} time_types_t;

/**type definition for containing the information to be shared between tasks*/
typedef struct {
    time_types_t time_type;
    uint8_t value;
} time_msg_t;

#define TOP_SECONDS 60  /**the amount of seconds in 1 minute*/
#define TOP_MINUTES 60  /**the amount of minutes in 1 hour*/
#define TOP_HOURS 24    /**the amount of hours in 1 day*/

#define HOURS_ALARM 22  /**alarm hour setup*/
#define MINUTES_ALARM 1 /**alarm minutes setup*/
#define SECONDS_ALARM 2 /**alarm seconds setup*/

#define HOURS_INIT 22   /**initial clock hours*/
#define MINUTES_INIT 1  /**initial clock minutes*/
#define SECONDS_INIT 58 /**initial clock seconds*/

#define HOURS_EVENT_BIT (1 << 0)    /**event group hours bit*/
#define MINUTES_EVENT_BIT (1 << 1)  /**event group minutes bit*/
#define SECONDS_EVENT_BIT (1 << 2)  /**event group seconds bit*/

#define DEBUG 1

/**RTOS elements declaration*/
SemaphoreHandle_t minutes_semaphore;
SemaphoreHandle_t hours_semaphore;
SemaphoreHandle_t mutex_uart;
static QueueHandle_t time_Queue;

void alarm_task(void * args)
{

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
        xSemaphoreTake(mutex_uart, portMAX_DELAY);

        PRINTF("\033[2J"); /**UART clear screen VT100 command*/
        PRINTF("ALARM! \033[5;10H");
        xSemaphoreGive(mutex_uart);
    }

}

void print_task(void * args)
{
    /*
     *The value of each unit it's updated
     *depending on the time_type received by the Queue.
     */
    static time_msg_t *message;
    static uint8_t sec = SECONDS_INIT;
    static uint8_t min = MINUTES_INIT;
    static uint8_t hr = HOURS_INIT;

    PRINTF("\033[2J"); /**UART clear screen VT100 command*/
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
        /**To prevent errors between task
         * a mutex is used for the use of the UART
         */
        xSemaphoreTake(mutex_uart, portMAX_DELAY);
        PRINTF("%d : %d : %d hrs \033[3;10H", hr, min, sec);
        xSemaphoreGive(mutex_uart);
        /**Free memory to prevent overflow
         */
        vPortFree(message);
    }

}

void seconds_task(void *args)
{
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
            /**if the seconds correspond with that of the alarm, the seconds bit is set*/
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
        xQueueSend(time_Queue, &message, 0);    /**the message is sent to the shared queue*/
#endif
        vTaskDelayUntil(&LastTimeAwake, pdMS_TO_TICKS(1000));

    }

}

void minutes_task(void *arg)
{
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
     * This task wakes up every minute waiting for the 60 seconds event
     * it increments the value of minutes and if
     * it gets to 0 again.
     * Sets up an event to increase the value of hours.
     * Reserves memory for the message and sends it.
     *
     */
    for (;;)
    {
        xSemaphoreTake(minutes_semaphore, portMAX_DELAY);   /**if the minutes semaphore is free to be taken,*/
        /**the following instructions will be performed*/

        if (minutes < TOP_MINUTES - 1)  /**if current minutes variable doesn't exceed the minutes limit,*/
        {
            minutes++;  /**then current minutes variable is increased*/
        } else  /**if current minutes variable has exceeded the minutes limit,*/
        {
            minutes = 0;    /**as an hour has passed, the minutes variable is reset*/
            xSemaphoreGive(hours_semaphore);    /**if an hour has passed, then the hour semaphore is released*/
        }
        if (HOURS_EVENT_BIT == xEventGroupGetBits(g_time_events))  /**if previously the hours event bit had been set,*/
        {
            if (MINUTES_ALARM == minutes)   /**if the current minute coincides with the alarm minutes,*/
            {
                xEventGroupSetBits(g_time_events, MINUTES_EVENT_BIT);   /**then the corresponding minutes bit is set*/
            }
        }

        message = pvPortMalloc(sizeof(time_msg_t)); /**memory is reserved for the message to be sent*/
        message->time_type = minutes_type;  /**in the message, the time type is established as minutes*/
        message->value = minutes;   /**in the message, the value to be passed is the current minutes variable*/
#if DEBUG
        xQueueSend(time_Queue, &message, 0);    /**the message is sent to the shared queue*/

#endif
    }
}

void hours_task(void * args)
{
    /*
     * This task wakes up every hour waiting for the 60 minutes event
     * it increments the value of hours and if
     * it gets to 0 again.
     * Sets the hour event bit
     * Reserves memory for the message and sends it.
     *
     */
    static time_msg_t *message;

    static uint8_t hours = HOURS_INIT;
    if (HOURS_ALARM == hours)
    {
        xEventGroupSetBits(g_time_events, HOURS_EVENT_BIT);

    }
    for (;;)
    {
        xSemaphoreTake(hours_semaphore, portMAX_DELAY);
        /**if the hours semaphore is could be taken, i.e. it was
         * free or released, then the following
         * operations will be performed*/

        if (hours < TOP_HOURS - 1) /**if the current hours is still less than the hours limit,*/
        {
            hours++; /**then the hours are increased*/
        } else /**if the current hours exceed the hours limit,*/
        {
            hours = 0; /**the current hours variable is reset to 0*/
        }
        if (HOURS_ALARM == hours) /**if current hour coincides with the alarm hour,*/
        {
            xEventGroupSetBits(g_time_events, HOURS_EVENT_BIT); /**then the corresponding hour event group bit is set*/

        }

        message = pvPortMalloc(sizeof(time_msg_t)); /**memory is reserved for the message to be sent*/
        message->time_type = hours_type; /**in the message, the time type is established as hour*/
        message->value = hours; /**in the message, the value to be passed is the current hour variable*/
#if DEBUG
        xQueueSend(time_Queue, &message, 0); /**the message is sent to the shared queue*/
#endif
    }

}

int main(void)
{

    /* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
    /* Init FSL debug console. */
    BOARD_InitDebugConsole();

    PRINTF("\033[2J"); /**clear screen VT100 command*/

    /**RTOS elements creation*/
    time_Queue = xQueueCreate(1, sizeof(void*)); /**IPC queue created with the size of a void pointer*/
    minutes_semaphore = xSemaphoreCreateBinary(); /**binary semaphore created for the minutes*/
    hours_semaphore = xSemaphoreCreateBinary(); /**binary semaphore created for the hours*/
    mutex_uart = xSemaphoreCreateMutex(); /**mutex created in order to protect the uart*/

     /**seconds task created with the highest priority as it is the most frequent task*/
    xTaskCreate(seconds_task, "Seconds", configMINIMAL_STACK_SIZE + 200,
                (void*) (time_Queue), configMAX_PRIORITIES - 1, NULL);

    /**minutes task created*/
    xTaskCreate(minutes_task, "Minutes", configMINIMAL_STACK_SIZE + 200,
                (void*) (time_Queue), configMAX_PRIORITIES - 3, NULL);

    /**hours task created*/
    xTaskCreate(hours_task, "Hours", configMINIMAL_STACK_SIZE + 200, NULL,
    configMAX_PRIORITIES - 4,
                NULL);

    /**alarm task created*/
    xTaskCreate(alarm_task, "Alarm", configMINIMAL_STACK_SIZE + 200, NULL,
    configMAX_PRIORITIES - 5,
                NULL);

    /**print task created*/
    xTaskCreate(print_task, "Print", configMINIMAL_STACK_SIZE + 200, NULL,
    configMAX_PRIORITIES - 6,
                NULL);

    /**the shared bits event group is created*/
    g_time_events = xEventGroupCreate();

    /**RTOS scheduler takes tasks control from now on*/
    vTaskStartScheduler();

    while (1)
    {
    }
    return 0;
}
