#include "Bath.h"

#define BOUNCEDELAY 25 / portTICK_RATE_MS // время дребезга в мс
#define MAXFIXINT 3                       // минимум еще несколько интервалов для особо плохих кнопок

void BathLightSwDebounce(void *p);
void RestLightSwDebounce(void *p);
TaskHandle_t xHandleBLS;
TaskHandle_t xHandleRLS;

static void IRAM_ATTR BathLightSw_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = 0;
    vTaskNotifyGiveFromISR(xHandleBLS, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
        portYIELD_FROM_ISR();
}
static void IRAM_ATTR RestLightSw_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = 0;
    vTaskNotifyGiveFromISR(xHandleRLS, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
        portYIELD_FROM_ISR();
}

void BathLightSwDebounce(void *p)
{
    int delay = portMAX_DELAY;
    int cntisr = 0;
    int sw_lvl = 0;
    int fix = 0;
    for (;;)
    {
        cntisr = ulTaskNotifyTake(pdFALSE, delay); // есть некоторое количество пррываний
        if (cntisr != 0)                           // interupt from delay time
        {
            delay = BOUNCEDELAY; // проверяем дребезг за время задержки
            fix = 0;
            continue; // пока отрабатываем каждый фронт на дребезге; cntisr - должен уменьшаться на каждом вызове ulTaskNotifyTake
        }
        else // за время задержки дребезга не было прерываний
        {
            if (fix < MAXFIXINT) // не было прерываний несколько циклов
            {
                fix++;
                continue;
            }
            sw_lvl = gpio_get_level(GPIO_BATH_LIGHT_SW_IN); // достаточно - фиксируем уровень
            delay = portMAX_DELAY;                          // в следующем цикле - ожидаем новый пакет прерываний
            //
            // send lwl  as sw on/off
            //
            ESP_LOGI("Bath", "SW %d", sw_lvl);
        }
    }
}

void RestLightSwDebounce(void *p)
{
    int delay = portMAX_DELAY;
    int cntisr = 0;
    int sw_lvl = 0;
    int fix = 0;
    for (;;)
    {
        cntisr = ulTaskNotifyTake(pdFALSE, delay); // есть некоторое количество пррываний
        if (cntisr != 0)                           // interupt from delay time
        {
            delay = BOUNCEDELAY; // проверяем дребезг за время задержки
            fix = 0;
            continue; // пока отрабатываем каждый фронт на дребезге; cntisr - должен уменьшаться на каждом вызове ulTaskNotifyTake
        }
        else // за время задержки дребезга не было прерываний
        {
            if (fix < MAXFIXINT) // не было прерываний несколько циклов
            {
                fix++;
                continue;
            }
            sw_lvl = gpio_get_level(GPIO_REST_LIGHT_SW_IN); // достаточно - фиксируем уровень
            delay = portMAX_DELAY;                          // в следующем цикле - ожидаем новый пакет прерываний
            //
            // send lwl  as sw on/off
            //
            ESP_LOGI("Rest", "SW %d", sw_lvl);
        }
    }
}

void LightISRSetup()
{
    xTaskCreate(BathLightSwDebounce, "blsw", 2000, NULL, 1, &xHandleBLS);
    xTaskCreate(RestLightSwDebounce, "rlsw", 2000, NULL, 1, &xHandleRLS);

    gpio_config_t io_conf;
    //interrupt of ANY edge
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE; //GPIO_PIN_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_LIGHT_SW_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;

    gpio_config(&io_conf);

    //install gpio isr service // возможно уже установлен
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_BATH_LIGHT_SW_IN, BathLightSw_isr_handler, (void *)GPIO_BATH_LIGHT_SW_IN);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_REST_LIGHT_SW_IN, RestLightSw_isr_handler, (void *)GPIO_REST_LIGHT_SW_IN);
    for (;;)
    {
        vTaskDelay(100);
        //ESP_LOGI("sw"," delay");
    }
}