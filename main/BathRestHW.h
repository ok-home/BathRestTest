/*
* Описание портов  и разъемов
* вид со стороны компонентов
*/
/*
Реле 220В 2А
-----------------------------------------
X SW1.1\
 -      -- Вентиляция в Туалате -- GPIO14
X SW1.2/
 -
X SW2.1\
 -      -- Вентиляция в Ванной -- GPIO14
X SW2.2/

X SW2.1\
 -      -- Свет в Туалаете -- GPIO13
X SW3.2/

X SW3.1\
 -      -- Свет в Ванной -- GPIO12
X SW3.2/
-----------------------------------------
Вход питания контроллера
X VIN+ +7.5/+12  вольт
 -
X VIN- Земля
-----------------------------------------

Входы датчиков
-----------------------------------------
P1 D1.1 - +5/+12В
P2 D1.2 - NC
P3 D1.3 - IR in
P4 D1.4 - Земля

P1 D2.1 - +5/+12В
P2 D2.2 - NC
P3 D2.3 - MV in
P4 D2.4 - Земля

P1 D3.1 - +3.3В
P2 D3.2 - HUM_SDA
P3 D3.3 - HUM_SCL
P4 D3.4 - Земля

P1 D4.1 - +5В
P2 D4.2 - DIST_TRIGGER
P3 D4.3 - DIST_ECHO
P4 D4.4 - Земля

*/

/*
* connected GPIO
* NOT TESTED
*/

#define GPIO_BATH_LIGHT_OUT    12 // Bath Light
#define GPIO_REST_LIGHT_OUT    13 // Rest Light
#define GPIO_BATH_VENT_OUT    14 // Bath Vent
#define GPIO_REST_VENT_OUT    15 // Rest Vent
#define GPIO_OUTPUT_SW_PIN_SEL  ((1ULL<<GPIO_BATH_LIGHT_OUT) | (1ULL<<GPIO_REST_LIGHT_OUT) | (1ULL<<GPIO_BATH_VENT_OUT) | (1ULL<<GPIO_REST_VENT_OUT))

#define GPIO_IR_IN 19 // IR Input
#define GPIO_MV_IN 18 // MV Input
#define GPIO_IR_MV_PIN_SEL ((1ULL << GPIO_IR_IN) | (1ULL << GPIO_MV_IN))
#define ESP_INTR_FLAG_DEFAULT 0

#define GPIO_DIST_TRIGGER_OUT 23  // Dist Remote Trigg
#define GPIO_DIST_ECHO_IN 22     // Dist Remote Echo

#define GPIO_HUM_SDA_INOUT 16 // sda pin Hum
#define GPIO_HUM_SCL_OUT 17 // scl pin Hum