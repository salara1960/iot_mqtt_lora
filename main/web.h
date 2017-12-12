#ifndef __WEB_H__
#define __WEB_H__

#include "hdr.h"

#ifdef SET_WEB

    #include "mqtt.h"

    #undef SET_WEB_PRN

    #define TWAIT 10000
    #define WEB_PORT 9988

    extern const char *TAGWEB;
    extern uint8_t web_start;
    extern void web_task(void *arg);
#endif

#endif
