#undef SET_SD_CARD
#undef SET_WPS
#undef SET_TOUCH_PAD
#undef UDP_SEND_BCAST
#undef SET_MQTT
#define SET_I2C
#define SET_SNTP
#define SET_TLS_SRV
#define SET_OTA
#define SET_ERROR_PRINT
#undef SET_WEB
#define SET_WS
#undef SET_SSD1306
#define SET_SERIAL

#ifdef SET_I2C
    #define SET_APDS
    #define SET_BMP
    #define SET_BH1750
    #define SET_SI7021
#endif

