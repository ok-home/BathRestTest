#include "Bath.h"

static void IRAM_ATTR Ir_isr_handler(void *arg)
{
    //uint32_t gpio_num = (uint32_t)arg;
    uint32_t on_off = gpio_get_level(GPIO_IR_IN);
    xQueueSendFromISR(IrIsrQueue, &on_off, NULL); // send to IR Queue
}
static void IRAM_ATTR Mv_isr_handler(void *arg)
{
    //uint32_t gpio_num = (uint32_t)arg;
    uint32_t on_off = gpio_get_level(GPIO_MV_IN);
    xQueueSendFromISR(MvIsrQueue, &on_off, NULL); // send to MV Queue
    
}

void IrMvISRSetup()
{
    gpio_config_t io_conf;
    //interrupt of ANY edge
    io_conf.intr_type = GPIO_PIN_INTR_ANYEDGE;//GPIO_PIN_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_IR_MV_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_IR_IN, Ir_isr_handler, (void *)GPIO_IR_IN);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_MV_IN, Mv_isr_handler, (void *)GPIO_MV_IN);

    }