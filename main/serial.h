#ifndef __SERIAL_H__
#define __SERIAL_H__

#include "hdr.h"

#ifdef SET_SERIAL

    #include "driver/uart.h"
    #include "soc/uart_struct.h"

    #undef PRINT_AT
    #define TICK_CNT_WAIT 10000

    #define U2_CONFIG 21 //pin21 - out, pull down
//    #define U2_SLEEP 22 //pin22 - out, pull down
    #define U2_RESET 2 //pin2 - out, pull up
    #define U2_STATUS 15 //pin17 - in
    #define GPIO_U2TXD 17 //pin17 - U2TXD
    #define GPIO_U2RXD 16 //pin16 - U2RXD
    #define UART_NUMBER UART_NUM_2
    #define UART_SPEED 115200

    #pragma pack(push,1)
    typedef struct
    {
	unsigned config:1;
	unsigned sleep:1;
	unsigned reset:1;
	unsigned status:1;
	unsigned none:4;
    } s_pctrl;
    #pragma pack(pop)

    extern const char *TAG_UART;

    extern void serial_init();
    extern void serial_task(void *arg);

#endif

#endif

