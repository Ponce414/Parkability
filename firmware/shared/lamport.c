#include "lamport.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static uint32_t g_clock = 0;
static SemaphoreHandle_t g_mutex = NULL;

void lamport_init(void)
{
    if (g_mutex == NULL) {
        g_mutex = xSemaphoreCreateMutex();
    }
    g_clock = 0;
}

uint32_t lamport_tick(void)
{
    uint32_t v;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_clock += 1;
    v = g_clock;
    xSemaphoreGive(g_mutex);
    return v;
}

uint32_t lamport_update(uint32_t received_ts)
{
    uint32_t v;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    if (received_ts > g_clock) {
        g_clock = received_ts;
    }
    g_clock += 1;
    v = g_clock;
    xSemaphoreGive(g_mutex);
    return v;
}

uint32_t lamport_now(void)
{
    uint32_t v;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    v = g_clock;
    xSemaphoreGive(g_mutex);
    return v;
}
