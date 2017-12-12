#ifndef __SD_CARD_H__
#define __SD_CARD_H__

#include "hdr.h"

#ifdef SET_SD_CARD
    #include "mqtt.h"

    #include "driver/sdmmc_defs.h"
    #include "driver/sdmmc_host.h"
    #include "driver/sdspi_host.h"
    #include "sdmmc_cmd.h"

    extern const char *TAGSD;
    extern void sdcard_task(void *arg);
#endif

#endif
