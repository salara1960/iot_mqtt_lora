#ifndef __MQTT_H__
#define __MQTT_H__


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include <freertos/semphr.h>
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#ifdef SET_I2C
    #include "driver/i2c.h"
#endif

#include "esp_system.h"
#include "esp_adc_cal.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_wps.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include <esp_intr.h>

#include "esp_types.h"
#include "stdatomic.h"

#include "tcpip_adapter.h"

#include "crt.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#ifndef CONFIG_AWS_EXAMPLE_CLIENT_ID
    #define CONFIG_AWS_EXAMPLE_CLIENT_ID "unic"
#endif
#include "rom/ets_sys.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include <soc/rmt_struct.h>
#include <soc/dport_reg.h>
#include <soc/gpio_sig_map.h>



#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/api.h"
//#include "lwip/priv/api_msg.h"
#include "lwip/netif.h"

#include "cJSON.h"


#define DEV_MASTER ((uint8_t)0)

//#define LOG_COLOR_BLACK   "30"
//#define LOG_COLOR_RED     "31"
//#define LOG_COLOR_GREEN   "32"
//#define LOG_COLOR_BROWN   "33"
//#define LOG_COLOR_BLUE    "34"
//#define LOG_COLOR_PURPLE  "35"
//#define LOG_COLOR_CYAN    "36"
#define BLACK_COLOR  "\x1B[30m"
#define RED_COLOR  "\x1B[31m"
#define GREEN_COLOR  "\x1B[32m"
#define BROWN_COLOR  "\x1B[33m"
#define BLUE_COLOR  "\x1B[34m"
#define MAGENTA_COLOR  "\x1B[35m"
#define CYAN_COLOR  "\x1B[36m"
#define WHITE_COLOR "\x1B[0m"
#define START_COLOR CYAN_COLOR
#define STOP_COLOR  WHITE_COLOR


#define TLS_PORT_DEF 4545
#define LED_PIN GPIO_NUM_4
#define PIXELS 8

#define MAX_LOG_FILE_SIZE 1024 * 750	//512K

#define sntp_srv_len 32
#define wifi_param_len 32

#define get_tmr(tm) (xTaskGetTickCount() + tm)
#define check_tmr(tm) (xTaskGetTickCount() >= tm ? true : false)


#pragma once


typedef enum {
    RGB_NONE = 0,
    RGB_RAINBOW,
    RGB_LINEUP,
    RGB_LINEDOWN,
    WHITE_UP,
    WHITE_DOWN
} rgb_mode_t;

#pragma pack(push,1)
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ss_rgb;
typedef struct
{
    uint8_t size;
    ss_rgb rgb;
    uint8_t mode;
} s_led_cmd;
#pragma pack(pop)


#define MAX_FILE_BUF_SIZE 1024
#define RDBYTE "rb"
#define WRBYTE "wb"
#define WRPLUS "w+"
#define RDONLY "r"
#define WRTAIL "a+"

#define BUF_SIZE 1024

#pragma pack(push,1)
typedef struct {
    fpos_t rseek;
    fpos_t wseek;
    uint32_t total;
} s_log_t;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
    uint8_t faren;
    float cels;
    uint32_t vcc;
} t_sens_t;
#pragma pack(pop)
extern uint8_t temprature_sens_read();

extern uint8_t ms_mode;
extern uint32_t cli_id;

extern char work_ssid[wifi_param_len];

extern const int size_rgb;
extern const int size_led_cmd;
extern const int size_radio_one;
extern const int size_radio_cmd;
extern const int size_radio_stat;

typedef struct {
    char *ip;
    uint32_t port;
} broker_t;
#ifdef SET_MQTT
    extern uint8_t mqtt_client_connected;
    extern QueueHandle_t mqtt_evt_queue;
    extern QueueHandle_t mqtt_net_queue;
#endif
extern QueueHandle_t radio_evt_queue;
extern QueueHandle_t radio_cmd_queue;

extern rgb_mode_t rgb_mode;

extern const char server_cert[];
extern const char server_key[];

extern EventGroupHandle_t wifi_event_group;
extern const int CONNECTED_BIT;

extern uint8_t total_task;
extern uint8_t restart_flag;

extern s_led_cmd led_cmd;
extern QueueHandle_t led_cmd_queue;
extern xSemaphoreHandle status_mutex;

extern esp_err_t fs_ok;
extern const char *radio_fname;
extern FILE *flog;
//extern xSemaphoreHandle log_mutex;
//extern uint32_t log_seek;

extern uint8_t tls_hangup;
extern uint32_t tls_client_ip;



extern int make_msg(int *jret, char *buf, uint32_t cli, uint32_t *ind, uint32_t did, void *rad);

extern void print_msg(const char *tag, const char *tpc, const char *buf, uint8_t with);
extern int parser_json_str(const char *st, void *lcd, uint8_t *au, char *str_md5, uint8_t *rst, void *adr_dev);

extern void print_radio_file();
extern int update_radio_file();

extern void get_tsensor(t_sens_t *t_s);

#ifdef SET_TLS_SRV
    #include "tls_srv.h"
#endif

#ifdef SET_I2C
    #include "sensors.h"
#endif

#ifdef SET_OTA
    #include "ota.h"
    extern uint8_t ota_begin;
    extern uint8_t ota_ok;
    extern char ota_srv[ota_param_len];
    extern uint32_t ota_srv_port;
    extern char ota_filename[ota_param_len];
#endif

#ifdef SET_SNTP
    #include "sntp.h"
    extern char time_zone[sntp_srv_len];
#endif


#ifdef SET_SD_CARD
    #include "sd_card.h"
#endif

#if defined(SET_WEB) || defined(SET_WS)
extern int get_socket_error_code(int socket);
extern void show_socket_error_reason(int socket);
extern int create_tcp_server(u16_t prt);
#endif

#ifdef SET_WEB
    #include "web.h"
#endif

#ifdef SET_WS
    #include "ws.h"
#endif

#ifdef SET_SSD1306
    #include "ssd1306.h"
#endif

#ifdef SET_SERIAL
    #include "serial.h"
#endif


#endif
