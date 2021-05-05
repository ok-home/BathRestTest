#include "Bath.h"

#define BOUNCEDELAY 20 / portTICK_RATE_MS     // время дребезга в мс
#define MAXFIXINT 3                           // минимум еще несколько интервалов для особо плохих кнопок
#define QUICK_SW_DELAY 500 / portTICK_RATE_MS // задержка быстрого переключения переход в автомат

void BathLightSwDebounce(void *p);
void RestLightSwDebounce(void *p);
void QuickBathLightSw(void *p);
void QuickRestLightSw(void *p);

TaskHandle_t xHandleBLS;
TaskHandle_t xHandleRLS;
TaskHandle_t xHandleQBLS;
TaskHandle_t xHandleQRLS;

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
    int cntisr = 0; // прерываний в очереди
    int sw_lvl = 0; // зафиксированное состояние переключателя
    int fix = 0;    // количество циклов до срабатывания
    int curr_lvl = gpio_get_level(GPIO_BATH_LIGHT_SW_IN);

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
            if (sw_lvl == curr_lvl)                         // не было измнения выключателя, только дребезг
                continue;
            //
            // send lwl  as sw on/off
            //
            curr_lvl = sw_lvl;
            //ESP_LOGI("Bath", "SW %d", sw_lvl);
            xTaskNotifyGive(xHandleQBLS);
        }
    }
}

void RestLightSwDebounce(void *p)
{
    int delay = portMAX_DELAY;
    int cntisr = 0;
    int sw_lvl = 0;
    int fix = 0;
    int curr_lvl = gpio_get_level(GPIO_REST_LIGHT_SW_IN);
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
            if (sw_lvl == curr_lvl)                         // небыло изменения положеия выключателя, только дребезг
                continue;
            //
            // send lwl  as sw on/off
            //
            curr_lvl = sw_lvl;
            xTaskNotifyGive(xHandleQRLS);
            //ESP_LOGI("Rest", "SW %d", sw_lvl);
        }
    }
}

/*
*  ванная ручное управление светом 
* при быстром переключении кнопок - переходим в автоматический режим
*   при одиночном переключении - переходим в ручной режим в статус переключателя
*/

void QuickBathLightSw(void *p)
{
    int delay = portMAX_DELAY;
    int cnt = 0;
    int sw_lvl;
    struct WsDataToSend req; // отправить на сайт
    union QueueHwData ud;    // отправить контроллеру света
    for (;;)
    {
        if (ulTaskNotifyTake(pdFALSE, delay))
        {

            //ESP_LOGI("max","DELAY %d,cnt %d",delay,cnt);
            if (delay == portMAX_DELAY)
            {
                cnt = 0;
            }
            delay = QUICK_SW_DELAY;
            cnt++;

            continue;
        }
        else
        {
            if (cnt <= 1) // нет быстрых переключений - отключение автомата
            {
                sw_lvl = gpio_get_level(GPIO_BATH_LIGHT_SW_IN);
                delay = portMAX_DELAY;
                //ESP_LOGI("QUICK BATH SLOW", "cnt %d lvl %d", cnt, sw_lvl);
                cnt = 0;

                // в таблицу параметров
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                DataParmTable[IDX_BATHLIGHTAUTOENABLE].val = 0;
                DataParmTable[IDX_BATHLIGHTONOFF].val = sw_lvl;
                xSemaphoreGive(DataParmTableMutex);
                // в контроллер света
                ud.HttpData.sender = 0; // почти с сайта
                ud.HttpData.ParmIdx = IDX_BATHLIGHTONOFF;
                xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], &ud, 0); // на контроллер света
                // на сайт
                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_BATHLIGHTAUTOENABLE;
                xQueueSend(SendWsQueue, &req, 0); // отправить http автомат света
                req.idx = IDX_BATHLIGHTONOFF;
                xQueueSend(SendWsQueue, &req, 0); // отправить http выключатель
                continue;
            }
            else // быстрые переключения - вернуть автомат
            {
                sw_lvl = gpio_get_level(GPIO_BATH_LIGHT_SW_IN);

                //ESP_LOGI("QUICK BATH FAST", "cnt %d lvl %d", cnt, sw_lvl);
                delay = portMAX_DELAY;

                // в таблицу параметров
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                DataParmTable[IDX_BATHLIGHTAUTOENABLE].val = 1;
                DataParmTable[IDX_BATHLIGHTONOFF].val = sw_lvl;
                xSemaphoreGive(DataParmTableMutex);
                // в контроллер света
                ud.HttpData.sender = 0; // почти с сайта
                ud.HttpData.ParmIdx = IDX_BATHLIGHTONOFF;
                xQueueSend(CtrlQueueTab[Q_BATHLIGHT_IDX], &ud, 0); // на контроллер света
                // на сайт
                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_BATHLIGHTAUTOENABLE;
                xQueueSend(SendWsQueue, &req, 0); // отправить http автомат света
                req.idx = IDX_BATHLIGHTONOFF;
                xQueueSend(SendWsQueue, &req, 0); // отправить http выключатель
                continue;
            }
        }
    }
}

/*
*  туалет ручное управление светом 
*  при быстром переключении кнопок - переходим в автоматический режим
*  при одиночном переключении - переходим в ручной режим в статус переключателя
*/

void QuickRestLightSw(void *p)
{
    int delay = portMAX_DELAY;
    int cnt = 0;
    int sw_lvl;
    struct WsDataToSend req; // отправить на сайт
    union QueueHwData ud;    // отправить контроллеру света
    for (;;)
    {
        if (ulTaskNotifyTake(pdFALSE, delay))
        {

            //ESP_LOGI("max","DELAY %d,cnt %d",delay,cnt);
            if (delay == portMAX_DELAY)
            {
                cnt = 0;
            }
            delay = QUICK_SW_DELAY;
            cnt++;

            continue;
        }
        else
        {
            if (cnt <= 1) // нет быстрых переключений - отключение автомата
            {
                sw_lvl = gpio_get_level(GPIO_REST_LIGHT_SW_IN);
                delay = portMAX_DELAY;
                //ESP_LOGI("QUICK REST SLOW", "cnt %d lvl %d", cnt, sw_lvl);
                cnt = 0;

                // в таблицу параметров
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                DataParmTable[IDX_RESTLIGHTAUTOENABLE].val = 0;
                DataParmTable[IDX_RESTLIGHTONOFF].val = sw_lvl;
                xSemaphoreGive(DataParmTableMutex);
                // в контроллер света
                ud.HttpData.sender = 0; // почти с сайта
                ud.HttpData.ParmIdx = IDX_RESTLIGHTONOFF;
                xQueueSend(CtrlQueueTab[Q_RESTLIGHT_IDX], &ud, 0); // на контроллер света
                // на сайт
                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_RESTLIGHTAUTOENABLE;
                xQueueSend(SendWsQueue, &req, 0); // отправить http автомат света
                req.idx = IDX_RESTLIGHTONOFF;
                xQueueSend(SendWsQueue, &req, 0); // отправить http выключатель
                continue;
            }
            else // быстрые переключения - вернуть автомат
            {
                sw_lvl = gpio_get_level(GPIO_REST_LIGHT_SW_IN);

                //ESP_LOGI("QUICK REST FAST", "cnt %d lvl %d", cnt, sw_lvl);
                delay = portMAX_DELAY;

                // в таблицу параметров
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                DataParmTable[IDX_RESTLIGHTAUTOENABLE].val = 1;
                DataParmTable[IDX_RESTLIGHTONOFF].val = sw_lvl;
                xSemaphoreGive(DataParmTableMutex);
                // в контроллер света
                ud.HttpData.sender = 0; // почти с сайта
                ud.HttpData.ParmIdx = IDX_RESTLIGHTONOFF;
                xQueueSend(CtrlQueueTab[Q_RESTLIGHT_IDX], &ud, 0); // на контроллер света
                // на сайт
                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_RESTLIGHTAUTOENABLE;
                xQueueSend(SendWsQueue, &req, 0); // отправить http автомат света
                req.idx = IDX_RESTLIGHTONOFF;
                xQueueSend(SendWsQueue, &req, 0); // отправить http выключатель
                continue;
            }
        }
    }
}


void LightISRSetup()
{

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

    xTaskCreate(BathLightSwDebounce, "blsw", 2000, NULL, 1, &xHandleBLS);
    xTaskCreate(RestLightSwDebounce, "rlsw", 2000, NULL, 1, &xHandleRLS);
    xTaskCreate(QuickBathLightSw, "qblsw", 2000, NULL, 1, &xHandleQBLS);
    xTaskCreate(QuickRestLightSw, "qblsw", 2000, NULL, 1, &xHandleQRLS);

    //install gpio isr service // возможно уже установлен
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT); // может выйти с ошибкой если уже установлен
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_BATH_LIGHT_SW_IN, BathLightSw_isr_handler, (void *)GPIO_BATH_LIGHT_SW_IN);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_REST_LIGHT_SW_IN, RestLightSw_isr_handler, (void *)GPIO_REST_LIGHT_SW_IN);
    
}