
/*
* Основная таблица параметров  - Параметры управляются  и отправляются на HTTP
*/
httpNameParm_t DataParmTable[MAX_IDX_PARM_TABLE] = {

    {"B_L_Status", 0, 0, Q_NO_IDX},
    {"R_L_Status", 0, 0, Q_NO_IDX},
    {"B_V_Status", 0, 0, Q_NO_IDX},
    {"R_V_Status", 0, 0, Q_NO_IDX},

    {"TempVol", 56, 1, Q_NO_IDX},
    {"HumVol", 68, 1, Q_NO_IDX},
    {"DistVol", 230, 1, Q_NO_IDX},
    {"IrVol", 1, 0, Q_NO_IDX},
    {"MvVol", 1, 0, Q_NO_IDX},

    {"B_L_AutoEnable", 1, 0, Q_BATHLIGHT_IDX},
    {"R_L_AutoEnable", 1, 0, Q_RESTLIGHT_IDX},
    {"B_V_AutoEnable", 1, 0, Q_BATHVENT_IDX},
    {"R_V_AutoEnable", 1, 0, Q_RESTVENT_IDX},

    {"B_L_OnOff", 0, 0, Q_BATHLIGHT_IDX},
    {"R_L_OnOff", 0, 0, Q_RESTLIGHT_IDX},
    {"B_V_OnOff", 0, 0, Q_BATHVENT_IDX},
    {"R_V_OnOff", 0, 0, Q_RESTVENT_IDX},

    {"B_V_UseHum", 1, 0, Q_BATHVENT_IDX},
    {"B_V_UseMove", 1, 0, Q_BATHVENT_IDX},
    {"B_L_IrUse", 1, 0, Q_BATHLIGHT_IDX},
    {"B_L_MvUse", 1, 0, Q_BATHLIGHT_IDX},

    {"B_HumOnParm", 50, 1, Q_BATHVENT_IDX},
    {"B_HumOffParm", 50, 1, Q_BATHVENT_IDX},
    {"B_V_OnDelay", 1, 1, Q_BATHVENT_IDX},
    {"B_V_OffDelay", 10, 1, Q_BATHVENT_IDX},
    {"B_L_IrMvAnyAll", 0, 0, Q_BATHLIGHT_IDX}, // изменено
    {"B_L_OffDelay", 10, 1, Q_BATHLIGHT_IDX},
    {"R_L_OnDist", 120, 1, Q_RESTLIGHT_IDX},
    {"R_L_OffDelay", 2, 1, Q_RESTLIGHT_IDX}, //изменено на задержку выключения света в туалете
    {"R_V_OnDelay", 1, 1, Q_RESTVENT_IDX},
    {"R_V_OffDelay", 10, 1, Q_RESTVENT_IDX}};

/*
*  таблица открытых ws сокетов
*/
struct async_resp_arg SocketArgDb[CONFIG_LWIP_MAX_SOCKETS] = {{NULL}, {0}};
/*
*Таблица переадресации Очередей 
*/
QueueHandle_t CtrlQueueTab[Q_MAX_TABLE_IDX];

//uint8_t IrMvAnyAll = 0 ; // 0 - Any, 1 - All - пока нет в таблице параметров
//int RestLightDelay = 2 ;// нет в таблице параметров - задержка выключения света по датчику расстояния

/*
* таблица параметров wifi
*/

wifiNameParm_t wifiDataParm[WIFI_TAB_RESTART + 1] = {
    {"wifi_ssid", "ok-home-Keenetic"},
    {"wifi_pass", "RicohPriport"},
    {"wifi_sta_ap", "wifi_sta"},
    {"Wifi_Restart", "false"}};
