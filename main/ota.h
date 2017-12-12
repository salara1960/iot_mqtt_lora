#ifndef __OTA_H__
#define __OTA_H__

#include "hdr.h"

#ifdef SET_OTA
    #include "mqtt.h"
    #include "esp_ota_ops.h"

    #undef OTA_WR_ENABLE

    #define ota_param_len   32
    #define OTA_SERVER_IP   CONFIG_SERVER_IP		//"10.100.0.103"
    #define OTA_SERVER_PORT CONFIG_SERVER_PORT		//9980
    #define OTA_FILENAME    CONFIG_EXAMPLE_FILENAME	//"/esp32/mqtt.bin"
    #define BUFFSIZE        1024			//CONFIG_TCPIP_TASK_STACK_SIZE //512
    #define OTA_PART_SIZE   1024 * 1024			//1M
    #define OTA_DATA_WAIT   20000			//20 sec//8 sec//5 sec

    extern const char *TAGO;
    extern void ota_task(void *arg);
#endif

#endif
