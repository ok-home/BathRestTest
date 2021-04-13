#include <stdio.h>
#include <stdlib.h>
#include "jsmn.h"
#include <string.h>
#include <ctype.h>

/*
*  Индекс в таблице для параметров
*/
#define IDX_BATHLIGHTSTATUS 0 // состояние света в ванной
#define IDX_RESTLIGHTSTATUS 1 // состояние света в туалете
#define IDX_BATHVENTSTATUS 2 // состояние вентиляции в ванной
#define IDX_RESTVENTSTATUS 3 // сстояние вентиляции в туалете

#define IDX_TEMPVOL 4   // температура с датчика
#define IDX_HUMVOL 5    // влажность с датчика
#define IDX_DISTVOL 6   // расстояние с датчика
#define IDX_IRVOL 7     // состояние ИК датчика движения
#define IDX_MVVOL 8     // состояние МВ датчика движения


#define IDX_BATHLIGHTAUTOENABLE 9 // разрешено автоуправление светом в ванной
#define IDX_RESTLIGHTAUTOENABLE 10 // разрешено автоуправление светом в туалете
#define IDX_BATHVENTAUTOENABLE 11 // разрешено автоуправление вентиляцией в ванной
#define IDX_RESTVENTAUTOENABLE 12 // разрешено автоуправление вентиляцией в туалете

#define IDX_BATHLIGHTONOFF 13 // ручное включение света в ванной
#define IDX_RESTLIGHTONOFF 14 // ручное включение света в туалете
#define IDX_BATHVENTONOFF 15 // ручное включение вентиляции в ванной
#define IDX_RESTVENTONOFF 16 // ручное включение вентиляции в туалете

#define IDX_BATHVENTUSEHUM 17 // используем датчик влажности для автоматики вентиляции в ванной
#define IDX_BATHVENTUSEMOVE 18 // используем датчики движения для автоматики вентиляции в ванной
#define IDX_BATHLIGHTIRUSE 19 // используем  ИК датчик движения для автоматики света в ванной
#define IDX_BATHLIGHTMVUSE 20 // используем  МВ датчик движения для автоматики света в ванной

#define IDX_BATHHUMONPARM 21 // влажность для включения
#define IDX_BATHHUMOFFPARM 22 // влажность для выключения
#define IDX_BATHVENTONDELAY 23 // задержка включения вентиляции в ванной
#define IDX_BATHVENTOFFDELAY 24 // задержка выключения вентиляции в ванной
#define IDX_BATHLIGHTONDELAY 25 // задержка включения света в ванной
#define IDX_BATHLIGHTOFFDELAY 26 // задержка выключения света в ванной
#define IDX_RESTLIGHTONDIST 27 // расстояние включения света в туалете
#define IDX_RESTLIGHTOFFDIST 28 // расстояние выключения света в туалете
#define IDX_RESTVENTONDELAY 29 // задержка включения вентиляции в туалете
#define IDX_RESTVENTFFDELAY 30 // задержка выключения вентиляции в туалете

#define MAX_IDX_PARM_TABLE 31 // размер таблицы параметров


typedef struct httpNameParm
{
    char *name; // строка имя в http
    int   val;  // значение параметра
    int type;   // тип данных число = 1 false/true =0
    int  *dataHandler; // указатель на функцию обработчик
} httpNameParm_t;

/*
* таблица параметров системы
*/

httpNameParm_t DataParmTable[MAX_IDX_PARM_TABLE]={
// еще состояние света и вентиляции
   {"BathLightStatus",0,0,0},
   {"RestLightStatus",0,0,0},
   {"BathVentStatus",0,0,0},
   {"RestVentStatus",0,0,0},

   {"TempVol",56,1,0},
   {"HumVol",68,1,0},  
   {"DistVol",230,1,0},  
   {"IrVol",1,0,0},  
   {"MvVol",1,0,0},  
   
   {"BathLightAutoEnable",1,0,0},  
   {"RestLightAutoEnable",1,0,0},  
   {"BathVentAutoEnable",1,0,0},  
   {"RestVentAutoEnable",1,0,0},  

   {"BathLightOnOff",0,0,0},  
   {"RestLightOnOff",0,0,0},  
   {"BathVentOnOff",0,0,0},  
   {"RestVentOnOff",0,0,0},  
  
   {"BathVentUseHum",1,0,0},  
   {"BathVentUseMove",1,0,0},  
   {"BathLightIrUse",1,0,0},  
   {"BathLightMvUse",1,0,0},  

   {"BathHumOnParm",60,1,0},  
   {"BathHumOffParm",61,1,0},  
   {"BathVentOnDelay",62,1,0},  
   {"BathVentOffDelay",63,1,0},  
   {"BathLightOnDelay",64,1,0},  
   {"BathLightOffDelay",65,1,0},  
   {"RestLightOnDist",66,1,0},  
   {"RestLightOffDist",67,1,0},  
   {"BRestVentOnDelay",68,1,0},  
   {"RestVentOffDelay",69,1,0}
};  



char *inJsonStr[3] = {"{\"name\": \"Stringfield\" , \"val\": ext }",
                     "{\"name\": \"Digitfield\" , \"val\": 555 }",
                     "{\"name\": \"Boolfield\" , \"val\": true }"};



/*
* Преобразование строки Json 2 параметра строка имя, 
* второй параметр только цифра или false/true
* jsonStr - строка Json
* nameStr - возвращаемое имя параметра
* возвращает значение параметра цифра или 0/1 false/true
* -1 - это не строка Json
* -2 - размер nameStr меньше строки наименования параметра
* -3 - параметр не цифра или false/true
*/
int JsonDigitBool(char *jsonStr, char *nameStr, int maxNameSize){
    int r; // количество токенов
    jsmn_parser p;
    jsmntok_t t[5];// только 2 пары параметров и obj
    
    memset(nameStr,0,maxNameSize);
    jsmn_init(&p);
    r = jsmn_parse(&p, jsonStr, strlen(jsonStr), t, sizeof(t) / sizeof(t[0]));
    if(r<0) return (-1); // не строка jSON
    if(maxNameSize < t[2].end-t[2].start) return (-2); // недостаточная длина строки для имени
    strncpy(nameStr,jsonStr+t[2].start,t[2].end-t[2].start); 
   
    if((jsonStr+t[4].start)[0]=='t') return (1); // true
    if((jsonStr+t[4].start)[0]=='f') return (0); // false
    if(isdigit((jsonStr+t[4].start)[0])) return(atoi(jsonStr+t[4].start)); // число
    return(-3); // это не число или false/true
};
/*
* фрмимирует строку Json из 2-х пар имя:значение
* dest - указатель на строку получатель ( размер не контролируется, может быть превышение)
*/
void StrToJson(char *dest, char *name1, char *vol1,char *name2, char *vol2 ){
    strcpy(dest,"{ ");strcat(dest,name1);strcat(dest," :\"");strcat(dest,vol1);strcat(dest,"\" , ");
                      strcat(dest,name2);strcat(dest,": ");strcat(dest,vol2);strcat(dest," }");
    };

int main(){

char name[20];
char valstr[8];
char json[32];
char jsonteststr[32][32];
int ret;
for( int i=0;i<3;i++ )
    {
       ret =  JsonDigitBool(inJsonStr[i],name,sizeof(name));
       if (ret<0) {printf("err %d\n",ret);}
       printf("Source %s ParceName: %s ParceVal %d\n",inJsonStr[i],name,ret);

        if(strcmp("Digitfield",name)==0){
        printf("check Name: %s vol %d\n",name,ret);
        itoa(ret,valstr,10);
        StrToJson(json,"name","Digitfield","val",valstr);
        printf("jstr  %s\n",json);
        }   
        else if(strcmp("Boolfield",name)==0){
        if( ret == 0 ) strcpy(valstr,"false");
        else strcpy(valstr,"true");
        printf("bool Name: %s vol %s\n",name,valstr);
        }
    }

for( int i=0;i<MAX_IDX_PARM_TABLE;i++)
{
    if(DataParmTable[i].type==0)
        {
        if(DataParmTable[i].val) 
            strcpy(valstr,"true"); 
            else strcpy(valstr,"false");
        }
    else 
        itoa(DataParmTable[i].val,valstr,10);

    StrToJson(jsonteststr[i],"\"name\"",DataParmTable[i].name,"\"val\"",valstr);
    printf("idx %d %s\n",i,jsonteststr[i]);
    ret =  JsonDigitBool(jsonteststr[i],name,sizeof(name));
    printf("back %s %d\n",name,ret);
} 

return(0);
}
