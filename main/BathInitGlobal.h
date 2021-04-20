
/*
* Основная таблица параметров  - Параметры управляются  и отправляются на HTTP
*/
httpNameParm_t DataParmTable[MAX_IDX_PARM_TABLE] = {

    {"BathLightStatus", 0, 0, Q_NO_IDX},
    {"RestLightStatus", 0, 0, Q_NO_IDX},
    {"BathVentStatus", 0, 0, Q_NO_IDX},
    {"RestVentStatus", 0, 0, Q_NO_IDX},

    {"TempVol", 56, 1, Q_NO_IDX},
    {"HumVol", 68, 1, Q_NO_IDX},
    {"DistVol", 230, 1, Q_NO_IDX},
    {"IrVol", 1, 0, Q_NO_IDX},
    {"MvVol", 1, 0, Q_NO_IDX},

    {"BathLightAutoEnable", 1, 0, Q_BATHLIGHT_IDX},
    {"RestLightAutoEnable", 1, 0, Q_RESTLIGHT_IDX},
    {"BathVentAutoEnable", 1, 0, Q_BATHVENT_IDX},
    {"RestVentAutoEnable", 1, 0, Q_RESTVENT_IDX},

    {"BathLightOnOff", 0, 0, Q_BATHLIGHT_IDX},
    {"RestLightOnOff", 0, 0, Q_RESTLIGHT_IDX},
    {"BathVentOnOff", 0, 0, Q_BATHVENT_IDX},
    {"RestVentOnOff", 0, 0, Q_RESTVENT_IDX},

    {"BathVentUseHum", 1, 0, Q_BATHVENT_IDX},
    {"BathVentUseMove", 1, 0, Q_BATHVENT_IDX},
    {"BathLightIrUse", 1, 0, Q_BATHLIGHT_IDX},
    {"BathLightMvUse", 1, 0, Q_BATHLIGHT_IDX},

    {"BathHumOnParm", 60, 1, Q_BATHVENT_IDX},
    {"BathHumOffParm", 61, 1, Q_BATHVENT_IDX},
    {"BathVentOnDelay", 62, 1, Q_BATHVENT_IDX},
    {"BathVentOffDelay", 63, 1, Q_BATHVENT_IDX},
    {"BathLightOnDelay", 0, 1, Q_BATHLIGHT_IDX},
    {"BathLightOffDelay", 2, 1, Q_BATHLIGHT_IDX},
    {"RestLightOnDist", 120, 1, Q_RESTLIGHT_IDX},
    {"RestLightOffDist", 120, 1, Q_RESTLIGHT_IDX},
    {"RestVentOnDelay", 68, 1, Q_RESTVENT_IDX},
    {"RestVentOffDelay", 69, 1, Q_RESTVENT_IDX}};

/*
*  таблица открытых ws сокетов
*/
struct async_resp_arg SocketArgDb[CONFIG_LWIP_MAX_SOCKETS] = {{NULL}, {0}};
/*
*Таблица переадресации Очередей 
*/
QueueHandle_t *CtrlQueueTab[Q_MAX_TABLE_IDX];

uint8_t IrMvAnyAll = 0 ; // 0 - Any, 1 - All - пока нет в таблице параметров
int RestLightDelay = 2 ;// нет в таблице параметров исрользуем данные для Ванной