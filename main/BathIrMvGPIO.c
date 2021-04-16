#include "Bath.h"
#include "driver/gpio.h"

/**
 * Brief:
 * This test code shows how to configure gpio and how to use gpio interrupt.
 *
 * GPIO4:  input, pulled up, interrupt from rising edge
 * GPIO5:  input, pulled up, interrupt from rising edge.
 */
#define GPIO_INPUT_IO_0 4 // IR Input
#define GPIO_INPUT_IO_1 5 // MV Input
#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0) | (1ULL << GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0

static void IRAM_ATTR Ir_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(IrIsrQueue, &gpio_num, NULL); // send to IR Queue
}
static void IRAM_ATTR Mv_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(MvIsrQueue, &gpio_num, NULL); // send to MV Queue
    
}

void IrMvISRSetup(void)
{
    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    //io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, IR_isr_handler, (void *)GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, Mv_isr_handler, (void *)GPIO_INPUT_IO_1);

    }