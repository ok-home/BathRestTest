#include "Bath.h"
#include "driver/rmt.h"

#define RMT_TX_CHANNEL 5                      /* RMT channel for transmitter */
#define RMT_TX_GPIO_NUM GPIO_DIST_TRIGGER_OUT /* GPIO number for transmitter signal */
#define RMT_RX_CHANNEL 6                      /* RMT channel for receiver */
#define RMT_RX_GPIO_NUM GPIO_DIST_ECHO_IN     /* GPIO number for receiver */
#define RMT_CLK_DIV 147                       //1.84 us  -   0,0312 cm
#define rmt_item32_tIMEOUT 64000              // 468 cm /*!< RMT receiver timeout value(us) */ // 8075 см - ставим 8000 тик= 8000 см
#define RMT_TRIGG_TICK 6                      // trigg wilth approx 10 us

static void HCSR04_tx_init()
{
    rmt_config_t rmt_tx;

    rmt_tx.channel = RMT_TX_CHANNEL;
    rmt_tx.gpio_num = RMT_TX_GPIO_NUM;
    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;
    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_en = 0;
    rmt_tx.tx_config.idle_level = 0;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_tx.rmt_mode = RMT_MODE_TX;
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
    rmt_rx.rx_config.filter_ticks_thresh = 50;
    rmt_rx.rx_config.idle_threshold = rmt_item32_tIMEOUT;
    rmt_config(&rmt_rx);
    rmt_driver_install(rmt_rx.channel, 32, 0); // rb size = 10
}
static const char *NEC_TAG = "NEC";

void DistIsrSetup(void *p)
{
    HCSR04_tx_init();
    HCSR04_rx_init();

    rmt_item32_t item;
    item.level0 = 1;
    item.duration0 = RMT_TRIGG_TICK;
    item.level1 = 0;
    item.duration1 = 20; // for one pulse this doesn't matter

    size_t rx_size = 0;
    rmt_item32_t *rx_item = NULL;
    RingbufHandle_t rb = NULL;
    rmt_get_ringbuf_handle(RMT_RX_CHANNEL, &rb);
    rmt_rx_start(RMT_RX_CHANNEL, true);

    //double distance = 0;
    int dist = 0;
    int midl_dist = 0;
    int max_dist = 0;
    int min_dist = 0;
    //int size = 0;
    int cnt = 0;
    for (;;)
    {
        rmt_write_items(RMT_TX_CHANNEL, &item, 1, true);
        rmt_wait_tx_done(RMT_TX_CHANNEL, portMAX_DELAY);

        rx_item = (rmt_item32_t *)xRingbufferReceive(rb, &rx_size, 2000);
        if (rx_item)
        {
            dist = ((int)rx_item->duration0) >> 5;
            //size = (int)rx_size;
            vRingbufferReturnItem(rb, (void *)rx_item);
            //       ESP_LOGI(NEC_TAG, "Dist is %d tick size %d item %d n ", dist, size,cnt);
            /*            if (midl_dist < dist)
            {
                midl_dist = dist;
            }
*/

            if (cnt == 0)
            {
                min_dist = max_dist = midl_dist = dist;
            }
            else
            {
                if (dist < min_dist)
                {
                    midl_dist = min_dist;
                    min_dist = dist;
                }
                else if (dist > max_dist)
                {
                    midl_dist = max_dist;
                    max_dist = dist;
                }
                else
                {
                    midl_dist = dist;
                }
            }
            if (++cnt == 3)
            {
                xSemaphoreTake(DataParmTableMutex, portMAX_DELAY);
                DataParmTable[IDX_DISTVOL].val = midl_dist;
                xSemaphoreGive(DataParmTableMutex);
                xQueueSend(DistIsrQueue, &midl_dist, 0);
                //            ESP_LOGI(NEC_TAG, "MIDL Dist is %d\n ", midl_dist);
/*
                struct WsDataToSend req;
                req.fd = 0;
                req.hd = NULL;
                req.idx = IDX_DISTVOL;
                xQueueSend(SendWsQueue, &req, 0); // датчик расстояния

*/
                cnt = 0;
                //midl_dist = 0;
            }
        }
        else
        {
            ESP_LOGI(NEC_TAG, "Distance is not readable");
        }

        vTaskDelay(150 / portTICK_PERIOD_MS);
    }
}

/*
best
div - 147 = 1,8375 mks
10 mks - 6 = 11 mks
dur_cm = tick*8 = dur>>5 
timeout - 6000 = 11 мс = 7,5 м

snd ( cm/sek)	34000	34000
clk	80000000	80000000
div	100	147
tick	1,25E-06	1,84E-06
dist ( cm/tick)	0,0425	0,0625
dist2 ( cm/tick)	0,0213	0,0312
cntPulse	8	6
PulseWidth	1,00E-05	1,10E-05
Cnttrash	100	100
dist2 Trash cm	2,125	3,12375
max Tick	32768	32768
Dist max cm	696,32	1023,5904
cnt err	9500	15000
dist err	201,875	468,5625
Period ( frames )	15	12
Period time	5,73E-01	6,62E-01
		
Test dist cm	2500	2500
Test time	1,47E-01	1,47E-01
meashured tick	117647	80032
meashured cm	2500	2501,0004


*/