#include "Bath.h"

uint16_t IrTimerEmu[4] = {2000, 2000, 2000, 5000};                             // тайминг ик датчика - эмулятор
uint16_t MvTimerEmu[4] = {3000, 3000, 3000, 6000};                             // тайминг мв датчика - эмулятор
uint16_t DistDataEmu[9] = {200, 150, 100, 50, 20, 50, 100, 150, 200}; // изменение расстояния с датчика
uint16_t DistTimerEmu = 1000;                                                  // снимаем данные каждую секунду
uint16_t HumDataEmu[9] = {20,40,60,80,90,80,60,40,20}; // изменение расстояния с датчика влажности
uint16_t HumTimerEmu = 2000;                                                  // снимаем данные каждые 2 секунды


void TestChangeStat(void *p);
void DistIsrTimerF(void *p);
void IrIsrTimerF(void *p);
void MvIsrTimerF(void *p);
void HumIsrTimerF(void *p);

void StartEmu();

void testunion()
{
 StartEmu();
}

// старт эмулятора - запускаем после инциализации всех очередей и обработчиков
void StartEmu()
{
    xTaskCreate(IrIsrTimerF, "timer IR", 2000, NULL, 1, NULL);
    xTaskCreate(MvIsrTimerF, "timer Mv", 2000, NULL, 1, NULL);
    xTaskCreate(DistIsrTimerF, "timer Mv", 2000, NULL, 1, NULL);
    xTaskCreate(HumIsrTimerF, "timer Hum", 2000, NULL, 1, NULL);
    
    //xTaskCreate(TestChangeStat, "Test", 4000, NULL, 1, NULL);
}
// эмулятор прерываний от датчика расстояния
void DistIsrTimerF(void *p)
{
    uint16_t dist;
    for (;;)
    {
        for (uint16_t i = 0; i < 9; i++)
        {
            dist = DistDataEmu[i];
            xQueueSend(DistIsrQueue, &dist, 0);
            vTaskDelay(DistTimerEmu / portTICK_RATE_MS);
        }
    }
}

// эмулятор прерываний от датчика влажности
void HumIsrTimerF(void *p)
{
    uint16_t hum;
    for (;;)
    {
        for (uint16_t i = 0; i < 9; i++)
        {
            hum = HumDataEmu[i];
            xQueueSend(HumIsrQueue, &hum, 0);
            vTaskDelay(HumTimerEmu / portTICK_RATE_MS);
        }
    }
}

// Эмулятор прерываний от ИК датчика движения
void IrIsrTimerF(void *p)
{
    uint8_t pp;
    for (;;)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            vTaskDelay(IrTimerEmu[i] / portTICK_RATE_MS);
            xQueueSend(IrIsrQueue, &pp, 0);
        }
    }
}
// Эмулятор прерываний от МВ датчика движения
void MvIsrTimerF(void *p)
{
    uint8_t pp;
    for (;;)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            vTaskDelay(MvTimerEmu[i] / portTICK_RATE_MS);
            xQueueSend(MvIsrQueue, &pp, 0);
        }
    }
}
// отладка - блокировку не настраиваем
void TestChangeStat(void *p)
{
    for (;;)
    {
        for (int i = 0; i <= IDX_MVVOL; i++)
        {
            if (DataParmTable[i].type == 0)
            { //bool
                if (DataParmTable[i].val == 0)
                    DataParmTable[i].val = 1;
                else
                    DataParmTable[i].val = 0;
            }
            else
            { //digit
                if (DataParmTable[i].val > 200)
                    DataParmTable[i].val = i;
                else
                    DataParmTable[i].val += i;
            }
        }
        vTaskDelay(6000 / portTICK_PERIOD_MS);
    }
}
