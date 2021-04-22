/*
* другая версия
* https://github.com/petemadsen/esp32/blob/master/hc-sr04/main/hcsr04.c
*/

#include "driver/rmt.h"

//#include "soc/rmt_reg.h"

#include "Bath.h"

#define RMT_TX_CHANNEL 1                /* RMT channel for transmitter */
#define RMT_TX_GPIO_NUM GPIO_DIST_TRIGGER_OUT /* GPIO number for transmitter signal */
#define RMT_RX_CHANNEL 0                /* RMT channel for receiver */
#define RMT_RX_GPIO_NUM GPIO_DIST_ECHO_IN   /* GPIO number for receiver */
#define RMT_CLK_DIV 100                 /* RMT counter clock divider */  //1.25 // ставим 118 - 1 тик =  1 см - точность 0,3%, 
#define RMT_TX_CARRIER_EN 0             /* Disable carrier */
#define rmt_item32_tIMEOUT_US 9500      /*!< RMT receiver timeout value(us) */ // 8075 см - ставим 8000 тик= 8000 см

#define RMT_TICK_10_US (80000000 / RMT_CLK_DIV / 100000) /* RMT counter value for 10 us.(Source clock is APB clock) 8 тиков в 10 мкс 1 тик 1,25 мкс */
#define ITEM_DURATION(d) ((d & 0x7fff) * 10 / RMT_TICK_10_US) // тики * 1,25 - длительность в мкс

static void HCSR04_tx_init()
{
    rmt_config_t rmt_tx;
    rmt_tx.channel = RMT_TX_CHANNEL;
    rmt_tx.gpio_num = RMT_TX_GPIO_NUM;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_en = RMT_TX_CARRIER_EN;
    rmt_tx.tx_config.idle_level = 0;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = 0;
    rmt_config(&rmt_tx);
    rmt_driver_install(rmt_tx.channel, 0, 0);
}

static void HCSR04_rx_init()
{
    rmt_config_t rmt_rx;
    rmt_rx.channel = RMT_RX_CHANNEL;
    rmt_rx.gpio_num = RMT_RX_GPIO_NUM;
    rmt_rx.clk_div = RMT_CLK_DIV;
    rmt_rx.mem_block_num = 1;
    rmt_rx.rmt_mode = RMT_MODE_RX;
    rmt_rx.rx_config.filter_en = true;
    rmt_rx.rx_config.filter_ticks_thresh = 100;
    rmt_rx.rx_config.idle_threshold = rmt_item32_tIMEOUT_US / 10 * (RMT_TICK_10_US);
    rmt_config(&rmt_rx);
    rmt_driver_install(rmt_rx.channel, 1000, 0);
}

void DistIsrSetup()
{
    HCSR04_tx_init();
    HCSR04_rx_init();

    rmt_item32_t tx_item;
    tx_item.level0 = 1;
    tx_item.duration0 = RMT_TICK_10_US; // 8 - 10 мкс // ставим 7  = 10,3 мкс или 8 = 11,8 мкс
    tx_item.level1 = 0;
    tx_item.duration1 = RMT_TICK_10_US; // for one pulse this doesn't matter // 7-8

    size_t rx_size = 0;
    RingbufHandle_t rb = NULL;
    rmt_get_ringbuf_handle(RMT_RX_CHANNEL, &rb);
    rmt_rx_start(RMT_RX_CHANNEL, 1);

    float distance = 0;

    for (;;)
    {
        rmt_write_items(RMT_TX_CHANNEL, &tx_item, 1, true);
        rmt_wait_tx_done(RMT_TX_CHANNEL, portMAX_DELAY);

        rmt_item32_t *rx_item = (rmt_item32_t *)xRingbufferReceive(rb, &rx_size, 1000);
        distance = 340.29 * ITEM_DURATION(item->duration0) / ( 1000 * 2); // distance in centimeters 
        //int int_distance = rx_item->duration0*10; // distance in centimeters
        printf("Distance is %f cm\n", distance);                         // distance in centimeters

        vRingbufferReturnItem(rb, (void *)rx_item);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}
/*
best
div - 147 = 1,8375 mks
10 mks - 6 = 11 mks
dur_cm = tick*8 = dur<<3 
timeout - 6000 = 11 мс = 7,5 м
*/