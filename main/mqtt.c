#include "hdr.h"
#include "mqtt.h"
//*******************************************************************

#include "ws2812.c"
#include "tcp_srv.c"

//*******************************************************************

#define GPIO_WIFI_PIN    23
#define GPIO_WMODE_PIN   22  //1-STA  0-AP
#ifdef SET_WPS
    #define GPIO_WPS_PIN     21
    #define WPS_TEST_MODE    WPS_TYPE_PBC	//WPS_TYPE_PIN
#endif
#define GPIO_RGB_WR_PIN  9 //sd2		//old pin17
#define GPIO_DEL_LOG_PIN 10 //sd3	//old pin16
//#define GPIO_U2TXD 17 //pin17 - U2TXD
//#define GPIO_U2RXD 16 //pin16 - U2RXD

#define ADC1_TEST_CHANNEL (6) //6 channel connect to pin34
#define ADC1_TEST_PIN    34 //pin34


#define STACK_SIZE_1K    1024
#define STACK_SIZE_1K5   1536
#define STACK_SIZE_2K    2048
#define STACK_SIZE_2K5   2560
#define STACK_SIZE_3K    3072
#define STACK_SIZE_4K    4096


#define STORAGE_NAMESPACE	"nvs"
#define PARAM_RGB_NAME		"rgb"
#define PARAM_CLIID_NAME	"cliid"
#define PARAM_SNTP_NAME		"sntp"
#define PARAM_TZONE_NAME	"tzone"
#define PARAM_SSID_NAME		"ssid"
#define PARAM_KEY_NAME		"key"
#define PARAM_WMODE_NAME	"wmode"
#define PARAM_MQTT_USER_NAME	"muser"
#define PARAM_MQTT_USER_PASS	"mpass"
#define PARAM_TLS_PORT		"tport"
#define PARAM_MSMODE_NAME	"msm"

#define PARAM_HDR_LOG		"hlog"

#define mqtt_user_name_len	16
#define broker_name_len 	32
#define PARAM_BROKERIP_NAME	"bip"
#define PARAM_BROKERPORT_NAME	"bprt"
#define VFS_MOUNTPOINT		"/disk"
#define max_file_buf_len	512
#define LOG_PAGE_SIZE		256
#define EXAMPLE_WIFI_SSID 	CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS 	CONFIG_WIFI_PASSWORD
#define max_inbuf 		CONFIG_AWS_IOT_MQTT_TX_BUF_LEN//CONFIG_AWS_IOT_MQTT_RX_BUF_LEN//512//256
//#define wifi_param_len 		32

//#define sntp_srv_len 		32
#define sntp_server_def		"2.ru.pool.ntp.org"
#define sntp_tzone_def		"EET-2"


#include "../version"

//*******************************************************************

const int size_rgb = sizeof(ss_rgb);
const int size_led_cmd = sizeof(s_led_cmd);

uint8_t led_flag = 0;
uint8_t restart_flag = 0;

uint8_t total_task = 0;
uint8_t last_cmd = 255;

static const char *TAG = "MQTT";
static const char *TAGN = "NVS";
static const char *TAGT = "VFS";
static const char *TAGL = "LED";

static const char *wmode_name[] = {"NULL","STA","AP","APSTA","MAX"};
static uint8_t wmode;
static unsigned char wifi_param_ready = 0;
char work_ssid[wifi_param_len] = {0};
static char work_pass[wifi_param_len] = {0};
static bool ssid_saved = false;
static char work_sntp[sntp_srv_len] = {0};
char time_zone[sntp_srv_len] = {0};
EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

char broker_host[broker_name_len] = AWS_IOT_MQTT_HOST;
uint32_t broker_port = AWS_IOT_MQTT_PORT;
broker_t brk;

char mqtt_user_name[mqtt_user_name_len] = CONFIG_AWS_EXAMPLE_CLIENT_ID;
char mqtt_user_pass[mqtt_user_name_len] = CONFIG_AWS_EXAMPLE_CLIENT_ID;

uint8_t *macs = NULL;

static char sta_mac[18] = {0};
uint8_t ms_mode = 0;
uint32_t cli_id = 0;
char localip[32] = {0};
ip4_addr_t ip4;
ip_addr_t bca;

static const char *TAGFAT = "FAT";
static wl_handle_t s_wl_handle = -1;//WL_INVALID_HANDLE;
static size_t fatfs_size = 0;
const char *base_path = "/spiflash";
const char *base_part = "disk";
esp_err_t fs_ok = ESP_FAIL;;
const char *radio_fname = "rlist.bin";//"RLIST.BIN";
const char *log_fname = "log.txt";//"LOG.TXT";
FILE *flog = NULL;
s_log_t hdr_log;
uint32_t max_log_file_size = MAX_LOG_FILE_SIZE;

#include "radio.h"

const int size_radio_one = sizeof(s_radio_one);
const int size_radio_cmd = sizeof(s_radio_cmd);
const int size_radio_stat = sizeof(s_radio_stat);

#ifdef SET_WPS
    static const char *TAGW = "WPS";
    static bool set_wps_mode = false;
#endif

xSemaphoreHandle status_mutex;
QueueHandle_t led_cmd_queue = NULL;
s_led_cmd led_cmd;

uint32_t tls_client_ip = 0;

#ifdef SET_MQTT
    static const char *TAG_GET = "GET";
    static const char *TAG_PUT = "PUT";
    QueueHandle_t mqtt_evt_queue = NULL;
    QueueHandle_t mqtt_net_queue = NULL;
    uint8_t mqtt_client_connected = 0;
#endif

QueueHandle_t radio_evt_queue = NULL;
QueueHandle_t radio_cmd_queue = NULL;

const char *l_on = "on";
const char *l_off = "off";
const char *l_up = "up";
const char *l_down = "down";
const char *devID = "DevID";

rgb_mode_t rgb_mode = RGB_NONE;


#define sub_cmd_name_all 13
const char *SubCmdName[] = {"status","group","sntp_srv","broker","ota_url","wifi_mode","mqtt_user","tls_port","time_zone","version","web_port","ws_port","ipaddr"};

#define cmd_name_all 31
const char *CmdName[] = {
    "leds","red","green","blue","yellow","magenta","cyan","rgb","total","auth",
    "get","udp","sntp","sntp_srv","time_zone","restart","time","i2c_prn","tls_hangup","ssid",
    "key","broker","wifi_mode","dopler_enable","ota","ota_url","mqtt_user","tls_port","rainbow","line","white"};
//{"leds":"on"} , {"leds":"off"} , {"leds":val} - val = 0..255
//{"red":"on"} , {"red":"off"} , {"red":val} - val = 0..255
//{"green":"on"} , {"green":"off"} , {"green":val} - val = 0..255
//{"blue":"on"} , {"blue":"off"} , {"blue":val} - val = 0..255
//{"yellow":"on"} , {"yellow":"off"} , {"yellow":val} - val = 0..255 | X,X,0
//{"magenta":"on"} , {"magenta":"off"} , {"magenta":val} - val = 0..255 | X,0,X
//{"cyan":"on"} , {"cyan":"off"} , {"cyan":val} - val = 0..255 | 0,X,X
//{"rgb":[r,g,b]} - r,g,b = 0..255
//{"total":val} - val = 1..8
//{"auth":"hash"} - hash md5_from_key_word
//{"get":"status"},{"get":"group"},{"get":"sntp_srv"},{"get":"broker"},{"get":"ota_url"},{"get":"wifi_mode"},{"get":"mqtt_user"},{"get":"tls_port"},{"get":"version"},{"get":"web_port"},{"get":"ws_port"},{"get":"ipaddr"}
//{"udp":"on"} , {"udp":"off"}
//{"sntp":"on"}
//{"sntp_srv":"ip_ntp_server"}
//{"time_zone":"UTC+02:00"}
//{"restart":"on"}
//{"time":"1493714647"} , {"time":1493714647}
//{"i2c_prn":"on"} , {"i2c_prn":"off"}
//{"tls_hangup":"on"}
//{"ssid":"ssid"}
//{"key":"key"}
//{"broker":"ip:port"}
//{"wifi_mode":"STA"} , {"wifi_mode":"AP"} , {"wifi_mode":"APSTA"}
//{"dopler_enable":"on"} , {"dopler_enable":"off"}
//{"ota":"on"}
//{"ota_url":"10.100.0.103:9980/esp32/mqtt.bin"}
//{"mqtt_user":"esp32:esp32"}
//{"tls_port":4545}
//{"rainbow":"on"} , {"rainbow":"off"}
//{"line":"up"} , {"line":"down"} , {"line":"off"}
//{"white":"up"} , {"white":"down"} , {"white":"off"}

#ifdef UDP_SEND_BCAST
    static const char *TAGU = "UDP";
    static uint8_t udp_start = 0;
    static int8_t udp_flag = 0;
#endif

#ifdef SET_TOUCH_PAD
    #define GPIO_INPUT_PIN_16     16
    #define GPIO_INPUT_PIN_SEL_16 (1<<GPIO_INPUT_PIN_16)
    #define ESP_INTR_FLAG_DEFAULT 0
    static const char* TAGK = "KEY";
    static xQueueHandle mov_evt_queue = NULL;
    static uint8_t dopler_flag = 1;
    static uint32_t intr_count = 0;

    static void IRAM_ATTR gpio_isr_hdl(void *arg)
    {
	intr_count++;
	if (dopler_flag) {
	    uint8_t byte = 1;
	    xQueueSendFromISR(mov_evt_queue, &byte, NULL);
	}
    }
#endif

#ifdef SET_SNTP
    uint8_t sntp_go = 0;
#endif

    uint16_t tls_port = 0;

    uint16_t web_port = 0;

    uint16_t ws_port = 0;

#ifdef SET_OTA
    #define PARAM_OTA_IP_NAME "otai"
    #define PARAM_OTA_PORT_NAME "otap"
    #define PARAM_OTA_FILE_NAME "otaf"
    uint8_t ota_flag = 0;
    uint8_t ota_begin = 0;
    uint8_t ota_ok = 0;
    char ota_srv[ota_param_len] = OTA_SERVER_IP;
    uint32_t ota_srv_port = OTA_SERVER_PORT;
    char ota_filename[ota_param_len] = OTA_FILENAME;
#endif

//******************************************************************************************************************

esp_err_t read_param(char *param_name, void *param_data, size_t len);
esp_err_t save_param(char *param_name, void *param_data, size_t len);

int save_rec_log(char *, int);
int read_rec_log(char *, uint32_t *);

//******************************************************************************************************************

void print_msg(const char *tag, char *tpc, char *buf, uint8_t with)
{
    if (!tag || !buf) return;
    int len = strlen(tag) + strlen(buf) + 48;
    if (tpc != NULL) len += strlen(tpc);
    char *st = (char *)calloc(1, len);
    if (st) {
	if (with) {
		struct tm *ctimka;
		int i_day, i_mon, i_hour, i_min, i_sec;//, i_year;
		time_t it_ct = time(NULL);
		ctimka = localtime(&it_ct);
		i_day = ctimka->tm_mday;	i_mon = ctimka->tm_mon+1;	//i_year = ctimka->tm_year+1900;
		i_hour = ctimka->tm_hour;	i_min = ctimka->tm_min;	i_sec = ctimka->tm_sec;
		sprintf(st,"%02d.%02d %02d:%02d:%02d | ",i_day, i_mon, i_hour, i_min, i_sec);
	}
	sprintf(st+strlen(st),"%s", tag);
	if (tpc != NULL) sprintf(st+strlen(st)," topic '%s'", tpc);
	sprintf(st+strlen(st)," : %s", buf);
	if (st[strlen(st)-1] != '\n') sprintf(st+strlen(st),"\n");
	printf(st);
	free(st);
    }
}
//------------------------------------------------------------------------------------------------------------
const char *wifi_auth_type(wifi_auth_mode_t m)
{

    switch (m) {
	case WIFI_AUTH_OPEN:// = 0
	    return "AUTH_OPEN";
	case WIFI_AUTH_WEP:
	    return "AUTH_WEP";
	case WIFI_AUTH_WPA_PSK:
	    return "AUTH_WPA_PSK";
	case WIFI_AUTH_WPA2_PSK:
	    return "AUTH_WPA2_PSK";
	case WIFI_AUTH_WPA_WPA2_PSK:
	    return "AUTH_WPA_WPA2_PSK";
	case WIFI_AUTH_WPA2_ENTERPRISE:
	    return "AUTH_WPA2_ENTERPRISE";
	default:
	    return "AUTH_UNKNOWN";
    }

}
//------------------------------------------------------------------------------------------------------------
esp_err_t event_handler(void *ctx, system_event_t *event)
{
system_event_sta_got_ip_t *got_ip = NULL;
wifi_mode_t mode;

    switch (event->event_id) {
		case SYSTEM_EVENT_STA_START:
			esp_wifi_connect();
		break;
		case SYSTEM_EVENT_STA_GOT_IP:
		{
			memset(localip, 0, 32);
			got_ip = &event->event_info.got_ip;
			if (got_ip) {
				ip4 = got_ip->ip_info.ip;
				sprintf(localip, IPSTR, IP2STR(&ip4));
				bca.u_addr.ip4.addr = ip4.addr | 0xff000000;
			}
			char *stx = (char *)calloc(1, 96);
			if (stx) {
				sprintf(stx,"Local ip = %s bca = ", localip);
				sprintf(stx+strlen(stx), IPSTR, IP2STR(&bca.u_addr.ip4));
				ets_printf("%s[%s] %s%s\n", GREEN_COLOR, TAG, stx, STOP_COLOR);
				free(stx);
			}

			xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		}
		break;
		case SYSTEM_EVENT_STA_CONNECTED:
		{
#ifdef SET_WPS

			if (set_wps_mode) {
				ets_printf("[%s] SYSTEM_EVENT_STA_CONNECT to %.*s\n", TAGW, event->event_info.connected.ssid_len, event->event_info.connected.ssid);
				//   SSID
				if (strncmp((const char *)work_ssid, (const char *)event->event_info.connected.ssid, event->event_info.connected.ssid_len)) {
					memset(work_ssid, 0, wifi_param_len);
					memcpy(work_ssid, event->event_info.connected.ssid, event->event_info.connected.ssid_len);
					//save_param(PARAM_SSID_NAME, (void *)work_ssid, wifi_param_len);
				}
			}
#endif
			wifi_ap_record_t wd;
			if (!esp_wifi_sta_get_ap_info(&wd)) {
			    ets_printf("%s[%s] Connected to AP '%s' auth(%u):'%s' chan:%u rssi:%d%s\n", GREEN_COLOR, TAG,
					(char *)wd.ssid,
					(uint8_t)wd.authmode, wifi_auth_type(wd.authmode),
					wd.primary, wd.rssi, STOP_COLOR);
			}
		}
		break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			esp_wifi_connect();
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		break;
		case SYSTEM_EVENT_AP_START:
			esp_wifi_get_mode(&mode);
		break;
//-----------------------------------------------------------------------
		case SYSTEM_EVENT_AP_STACONNECTED:
			ets_printf("[%s] station:"MACSTR" join, AID=%d\n", TAG,
					MAC2STR(event->event_info.sta_connected.mac),
					event->event_info.sta_connected.aid);
			xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		break;
		case SYSTEM_EVENT_AP_STADISCONNECTED:
			ets_printf("[%s] station:"MACSTR" leave, AID=%d\n", TAG,
					MAC2STR(event->event_info.sta_disconnected.mac),
					event->event_info.sta_disconnected.aid);
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		break;
//-----------------------------------------------------------------------
#ifdef SET_WPS
		case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
			ets_printf("[%s] SYSTEM_EVENT_STA_WPS_ER_SUCCESS\n", TAGW);
			esp_wifi_wps_disable();
			esp_wifi_connect();
		break;
		case SYSTEM_EVENT_STA_WPS_ER_FAILED:
			ets_printf("[%s] SYSTEM_EVENT_STA_WPS_ER_FAILED\n", TAGW);
			esp_wifi_wps_disable();
			esp_wifi_wps_enable(WPS_TEST_MODE);
			esp_wifi_wps_start(0);
		break;
		case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
			ets_printf("[%s] SYSTEM_EVENT_STA_WPS_ER_TIMEOUT\n", TAGW);
			esp_wifi_wps_disable();
			esp_wifi_wps_enable(WPS_TEST_MODE);
			esp_wifi_wps_start(0);
		break;
		case SYSTEM_EVENT_STA_WPS_ER_PIN:
			ets_printf("[%s] SYSTEM_EVENT_STA_WPS_ER_PIN\n", TAGW);
			ets_printf("[%s] WPS_PIN = "PINSTR"\n", TAGW, PIN2STR(event->event_info.sta_er_pin.pin_code));
		break;
#endif
		default : break;
    }

    return ESP_OK;
}
//------------------------------------------------------------------------------------------------------------
char *get_json_str(cJSON *tp)
{
    if (tp->type != cJSON_String) return NULL;

    char *st = cJSON_Print(tp);
    if (st) {
	if (*st == '"') {
		int len = strlen(st);
		memcpy(st, st + 1, len - 1);
		*(st + len - 2) = '\0';
	}
    }

    return st;
}
//------------------------------------------------------------------------------------------------------------
s_radio *check_dev(cJSON *ob, uint8_t *a)
{
s_radio *ret=NULL;

    if (a == NULL) {//for broker
	cJSON *tmp = cJSON_GetObjectItem(ob, devID);
	if (tmp) {
		if (tmp->type == cJSON_String) {
		    char *vals = get_json_str(tmp);
		    if (vals) {
			ret = find_radio(strtoul(vals, NULL, 16));
			free(vals);
		    }
		}
	}
    } else ret = find_radio(cli_id);//for tls_client

    return ret;
}
//------------------------------------------------------------------------------------------------------------
int parser_json_str(const char *st, void *lcd, uint8_t *au, char *str_md5, uint8_t *rst, void *adr_dev)
{
int yes = -1, subc = 0;
uint8_t done = 0, val_arr = 0, ind_c = 255, prm = 0;
int k, i, val_bin = -1;
s_radio *dev = NULL;//NULL - unknown device, else addr device in list
s_led_cmd *lcmd = (s_led_cmd *)lcd;


	//char stx[128]; sprintf(stx," st='%s' md5='%s'\n", st, str_md5); print_msg(__func__, NULL, stx, 1);
	//ESP_LOGI(__func__, " st='%s' md5='%s'\n", st, str_md5);

	cJSON *obj = cJSON_Parse(st);
	if (obj) {
	
	  cJSON *tmp = NULL;
	  char *val = NULL;
	  int dev_type = -1;//unknown device

	  dev = check_dev(obj, au);
	  if (dev) {
	    dev_type = isMaster(dev);//1-master, 0-slave
	    for (i = 0; i < cmd_name_all; i++) {
		tmp = cJSON_GetObjectItem(obj, CmdName[i]);
		if (tmp) {
		    ind_c = i;
		    if ( (ind_c <= 8) ||
			    ((ind_c >= 28) && (ind_c <= 30)) ) {//"leds","red","green","blue","yellow","magenta","cyan","rgb","total",/*"auth",*/  "rainbow","line","white"
			memset((uint8_t *)lcmd, 0, size_led_cmd);
			if (xSemaphoreTake(status_mutex, portMAX_DELAY) == pdTRUE) {
				memcpy((uint8_t *)lcmd, (uint8_t *)&led_cmd, size_led_cmd);
				xSemaphoreGive(status_mutex);
			} else lcmd->size = PIXELS;
			rgb_mode = RGB_NONE;
			lcmd->mode = rgb_mode;
		    }
		    switch (tmp->type) {
			case cJSON_False:
			case cJSON_True:
			case cJSON_NULL:
			break;
			case cJSON_Number:
			    val_bin = tmp->valueint;
			break;
			case cJSON_String:
			    if (val) { free(val); val=NULL; }
			    val = get_json_str(tmp);
			break;
			case cJSON_Array:
			{
			    int dl = cJSON_GetArraySize(tmp);
			    uint8_t j = 0;
			    cJSON *it = NULL;
			    if (dl > 0) val_arr=1; else val_arr=0;
			    while (j < dl) {
				it = cJSON_GetArrayItem(tmp, (int)j);
				if (it) {
					switch (it->type) {
					case cJSON_NULL:
					case cJSON_String:
					break;
					case cJSON_False:
					case cJSON_True:
					case cJSON_Number:
						k = it->valueint;//get_json_int(it);
						if (k>255) k=255; else if (k<0) k=0;
						if (ind_c == 7) {//"rgb"
							switch (j) {
							case 0: lcmd->rgb.r = k; break;
							case 1: lcmd->rgb.g = k; break;
							case 2: lcmd->rgb.b = k; break;
							}
						}
					break;
					}
				}
				j++;
			    }
			}
			break;
			case cJSON_Object:
			break;
		    }
		    done=0;
		    if ((val) || (val_bin != -1) || (val_arr)) {
			switch (ind_c) {
			    case 0://leds
				if (val) {
				    if (!strcmp(val, l_on)) {
					done = 1;
					memset((uint8_t *)&(lcmd->rgb), 255, size_rgb);
				    } else if (!strcmp(val, l_off)) {
					done = 1;
					memset((uint8_t *)&(lcmd->rgb), 0, size_rgb);
				    }
				} else if (val_bin != -1) {
				    done = 1; val_bin &= 0xff;
				    memset((uint8_t *)&(lcmd->rgb), (uint8_t)val_bin, size_rgb);
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 1://red
				if (val) {
				    if (!strcmp(val, l_on)) {//"red":"on"
				    	done = 1;
				    	lcmd->rgb.r = 255;
				    	lcmd->rgb.g = lcmd->rgb.b = 0;
				    } else if (!strcmp(val, l_off)) {//"red":"off"
				    	done = 1;
				    	lcmd->rgb.r = 0;
				    }
				} else if (val_bin != -1) {//"red":0..255
				    done = 1;
				    lcmd->rgb.r = val_bin;
				    lcmd->rgb.g = lcmd->rgb.b = 0;
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 2://green
				if (val) {
				    if (!strcmp(val, l_on)) {//"green":"on"
					done = 1;
					lcmd->rgb.r = lcmd->rgb.b = 0;
					lcmd->rgb.g = 255;
				    } else if (!strcmp(val, l_off)) {//"green":"off"
					done = 1;
					lcmd->rgb.g = 0;
				    }
				} else if (val_bin != -1) {//"green":0..255
				    done = 1;
				    lcmd->rgb.r = lcmd->rgb.b = 0;
				    lcmd->rgb.g = val_bin;
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 3://blue
				if (val) {
				    if (!strcmp(val, l_on)) {//"blue":"on"
				    	done = 1;
				    	lcmd->rgb.r = lcmd->rgb.g = 0;
				    	lcmd->rgb.b = 255;
				    } else if (!strcmp(val, l_off)) {//"blue":"off"
				    	done = 1;
				    	lcmd->rgb.b = 0;
				    }
				} else if (val_bin != -1) {//"blue":0..255
				    done = 1;
				    lcmd->rgb.r = lcmd->rgb.g = 0;
				    lcmd->rgb.b = val_bin;
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 4://yellow
				if (val) {
				    if (!strcmp(val, l_on)) {//"yellow":"on"
				    	done = 1;
				    	lcmd->rgb.r = lcmd->rgb.g = 255;
				    	lcmd->rgb.b = 0;
				    } else if (!strcmp(val, l_off)) {//"yellow":"off"
				    	done = 1;
				    	lcmd->rgb.r = lcmd->rgb.g = 0;
				    }
				} else if (val_bin != -1) {//"yellow":0..255
				    done = 1;
				    lcmd->rgb.r = lcmd->rgb.g = val_bin;
				    lcmd->rgb.b = 0;
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 5://magenta
				if (val) {
				    if (!strcmp(val, l_on)) {//"magenta":"on"
				    	done = 1;
				    	lcmd->rgb.r = lcmd->rgb.b = 255;
				    	lcmd->rgb.g = 0;
				    } else if (!strcmp(val, l_off)) {//"magenta":"off"
				    	done = 1;
				    	lcmd->rgb.r = lcmd->rgb.b = 0;
				    }
				} else if (val_bin != -1) {//"magenta":0..255
				    done = 1;
				    lcmd->rgb.r = lcmd->rgb.b = val_bin;
				    lcmd->rgb.g = 0;
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 6://cyan
				if (val) {
				    if (!strcmp(val, l_on)) {//"cyan":"on"
				    	done = 1;
				    	lcmd->rgb.g = lcmd->rgb.b = 255;
				    	lcmd->rgb.r = 0;
				    } else if (!strcmp(val, l_off)) {//"cyan":"off"
				    	done = 1;
				    	lcmd->rgb.g = lcmd->rgb.b = 0;
				    }
				} else if (val_bin != -1) {//"cyan":0..255
				    done = 1;
				    lcmd->rgb.g = lcmd->rgb.b = val_bin;
				    lcmd->rgb.r = 0;
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 7://rgb
				if (val_arr) {
				    done = 1;
				    yes = 0;
				}
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 8://total
				if (val_bin != -1) {
				    lcmd->size = val_bin;
				    done = 1;
				    yes = 0;
				}
				if (au) if (*au == 0) yes=-1;
			    break;
			    case 9://auth
//				sprintf(stx,"auth: val='%s' md5='%s'\n", val, str_md5); print_msg(__func__, NULL, stx, 1);
				if ((au) && (str_md5) && (val)) {
				    if (!strcmp(val, str_md5)) {
					done = 1; yes = 9;
					*au = done;
					lcmd->mode = (uint8_t)rgb_mode;
				    }
				}
			    break;
			    case 10://get -> const char *SubCmdName[] = {"status","group","sntp_srv","broker","ota_url","wifi_mode","mqtt_user","tls_port","time_zone","version","web_port","ws_port","ipaddr"};
				if (val) {
				    for (k = 0; k < sub_cmd_name_all; k++) {
					if (!strcmp(val, SubCmdName[k])) {
						subc = k;
						yes = 0;
						break;
					}
				    }
				    done = 1;
				    if ((!dev_type) && (subc != 0)) yes = -1;//for slave device
				}
				if (au) if (*au == 0) yes = -1;
			    break;
#ifdef UDP_SEND_BCAST
			    case 11://udp
				if (val) {
				    if (au) {//from tls_client
					if (*au) {
						if (!strcmp(val, l_on)) {
							if (dev_type > 0) {//for master device only
								udp_flag = 1;
								yes = 0;
							}
						} else if (!strcmp(val, l_off)) {
							if (dev_type > 0) {//for master device only
								udp_flag = 0;
								yes = 0;
							}
						}
					}
				    } else {//from broker
					if (!strcmp(val, l_on)) {
						if (dev_type > 0) {//for master device only
							udp_flag = 1;
							yes = 0;
						}
					} else if (!strcmp(val, l_off)) {
						if (dev_type > 0) {//for master device only
							udp_flag = 0;
							yes = 0;
						}
					}
				    }
				    done=1;
				}
			    break;
#endif
#ifdef SET_SNTP
			    case 12://sntp
				if (val) {
				    if (au) {//from tls_client
				    	if (*au) {
				    		if (!strcmp(val, l_on)) {
				    			if (dev_type > 0) {//for master device only
				    				sntp_go = 1;
				    				yes = 0;
				    			}
				    		}
				    	}// else yes=-1;
				    } else {//from broker
				    	if (!strcmp(val, l_on)) {
				    		if (dev_type > 0) {//for master device only
				    			sntp_go = 1;
				    			yes = 0;
				    		}
				    	}
				    }
				    done = 1;
				}
			    break;
			    case 13://sntp_srv
				if (val) {
				    prm = 0;
				    if (au) {//from tls_client
					if (*au) {
						if (dev_type > 0) prm = 1;//for master device only
					}
				    } else {//from broker
					if (dev_type > 0) prm = 1;//for master device only
				    }
				    if (prm) {
					k = strlen(val); if (k > sntp_srv_len - 1) k = sntp_srv_len - 1;
					memset(work_sntp,0,sntp_srv_len);
					memcpy(work_sntp, val, k);
					save_param(PARAM_SNTP_NAME, (void *)work_sntp, sntp_srv_len);
					sntp_go = 1;
					yes = 0;
				    }
				    done = 1;
				}
			    break;
			    case 14://time_zone
				if (val) {
				    prm = 0;
				    if (au) {//from tls_client
					if (*au) {
						if (dev_type > 0) prm = 1;//for master device only
					}
				    } else {//from broker
					if (dev_type > 0) prm = 1;//for master device only
				    }
				    if (prm) {
					if (strcmp(val, time_zone)) {
					    k = strlen(val);
					    if ((k < sntp_srv_len) && (k > 0)) {
						memset(time_zone, 0, sntp_srv_len);
						memcpy(time_zone, val, k);
						save_param(PARAM_TZONE_NAME, (void *)time_zone, sntp_srv_len);
						sntp_go = 1;
					    }
					}
					yes = 0;
				    }
				    done = 1;
				}
			    break;
#endif
			    case 15://restart
#ifdef SET_OTA
				if (val && !ota_begin) {
#else
				if (val) {
#endif
				    if (au) {//from tls_client
					if (*au) {
						if (!strcmp(val, l_on)) {
							if (dev_type > 0) {//for master device only
								restart_flag = 1; yes = 0;
							}
						}
					}
				    } else {//from broker
					if (!strcmp(val, l_on)) {
						if (dev_type > 0) {//for master device only
							restart_flag = 1; yes = 0;
						}
					}
				    }
				    done = 1;
				    if (rst) *rst = restart_flag;
				}
			    break;
			    case 16://time
				k = -1;
				if (val) k = atoi(val);
				else if (val_bin != -1) k = val_bin;
				if ((k > 0) && (dev_type > 0)) {//for master device only
				    if (!au) {//from broker
					SNTP_SET_SYSTEM_TIME_US( (time_t)k, 0 );
					yes = 0;
				    } else {//from tls_client
					if (*au) {
						SNTP_SET_SYSTEM_TIME_US( (time_t)k, 0 );
						yes = 0;
					}
				    }
				}
				done = 1;
			    break;
#ifdef SET_I2C
			    case 17://i2c_prn
				if ((val) && (dev_type > 0)) {//for master device only
				    if (!au) {//from broker
					if (!strcmp(val, l_on)) { i2c_prn = 1; yes = 0; }
					else if (!strcmp(val, l_off)) { i2c_prn = 0; yes = 0; }
				    } else {//from tls_client
					if (*au) {
						if (!strcmp(val, l_on)) { i2c_prn = 1; yes = 0; }
						else if (!strcmp(val, l_off)) { i2c_prn = 0; yes = 0; }
					}
				    }
				}
				done = 1;
			    break;
#endif
#ifdef SET_TLS_SRV
			    case 18://tls_hangup
				if ((val) && (dev_type > 0)) {//for master device only
				    if (!au) {//from broker
					if (!strcmp(val, l_on)) { tls_hangup = 1; yes = 0; }
				    } else {//from tls_client
					if (*au) if (!strcmp(val, l_on)) { tls_hangup = 1; yes = 0; }
				    }
				    done = 1;
				}
			    break;
#endif
			    case 19://ssid
				if ((val) && (dev_type > 0)) {//for master device only
				    prm = 0;
				    if (!au) {//from broker
					ssid_saved = false;
					k = strlen(val);
					if ((k > 0) && (k < wifi_param_len)) prm = 1;
				    } else {//from tls_client
					if (*au) {
						ssid_saved = false;
						k = strlen(val);
						if ((k > 0) && (k < wifi_param_len)) prm = 1;
					}
				    }
				    if (prm) {
					memset(work_ssid, 0, wifi_param_len);
					memcpy(work_ssid, val, k);
					if (save_param(PARAM_SSID_NAME, (void *)work_ssid, wifi_param_len) == ESP_OK) ssid_saved = true;
					yes = 0;
				    }
				}
			    break;
			    case 20://key
				if ((val) && (dev_type>0)) {//for master device only
				    k = strlen(val);
				    if (!au) {//from broker
				    	if ((k > 0) && (k < wifi_param_len)) {
				    		memset(work_pass,0,wifi_param_len);
				    		memcpy(work_pass, val, k);
				    		if (save_param(PARAM_KEY_NAME, (void *)work_pass, wifi_param_len) == ESP_OK) {
				    			if (ssid_saved) {
				    				ssid_saved = false;
				    				wmode = WIFI_MODE_STA;
				    				save_param(PARAM_WMODE_NAME, (void *)&wmode, sizeof(uint8_t));
				    				yes = 0;
#ifdef SET_OTA
				    				if (!ota_begin) {
				    					restart_flag = 1;
				    					if (rst) *rst = restart_flag;
				    				}
#else
				    				restart_flag = 1;
				    				if (rst) *rst = restart_flag;
#endif
				    			}
				    		}
				    	}
				    } else {//from tls_client
				    	if ((*au) && (k > 0) && (k < wifi_param_len)) {
				    		memset(work_pass,0,wifi_param_len);
				    		memcpy(work_pass, val, k);
				    		if (save_param(PARAM_KEY_NAME, (void *)work_pass, wifi_param_len) == ESP_OK) {
				    			if (ssid_saved) {
				    				ssid_saved = false;
				    				wmode = WIFI_MODE_STA;
				    				save_param(PARAM_WMODE_NAME, (void *)&wmode, sizeof(uint8_t));
				    				yes = 0;
#ifdef SET_OTA
				    				if (!ota_begin) {
				    					restart_flag = 1;
				    					if (rst) *rst = restart_flag;
				    				}
#else
				    				restart_flag = 1;
				    				if (rst) *rst = restart_flag;
#endif
				    			}
				    		}
				    	}
				    }
				    done = 1;
				}
			    break;
			    case 21://set new broker
				if ((val) && (dev_type > 0)) {//for master device only
				    char *uks = strchr(val, ':');
				    if (uks) {
				    	uint32_t prt = atoi(uks + 1);
				    	if (prt > 0) {
				    		*uks = '\0';
				    		k = 0; prm = 0;
				    		if (prt != broker_port) {//check broker port
				    			if (!au) {
				    				prm = 1;
				    			} else {
				    				if (*au) {
				    					prm = 1;
				    					k++;
				    				}
				    			}
				    			if (prm) {
				    				broker_port = prt;
				    				save_param(PARAM_BROKERPORT_NAME, (void *)&broker_port, sizeof(uint32_t));
				    			}
				    		}
				    		if (strcmp(val, broker_host)) {//check broker ip/name
				    			prm = 0;
				    			if (!au) {
				    				prm = 1;
				    				k++;
				    			} else {
				    				if (*au) {
				    					prm = 1;
				    					k++;
				    				}
				    			}
				    			if (prm) {
				    				memset(broker_host, 0, broker_name_len);
				    				strcpy(broker_host, val);
				    				save_param(PARAM_BROKERIP_NAME, (void *)broker_host, broker_name_len);
				    			}
				    		}
				    		if (k) {
				    			yes = 0;
				    			ets_printf("[%s] +++ NEW MQTT_BROKER: '%s:%u' +++\n", TAGT, broker_host, broker_port);
#ifdef SET_OTA
				    			if (!ota_begin) {
				    				restart_flag = 1;
				    				if (rst) *rst = restart_flag;
				    			}
#else
				    			restart_flag = 1;
				    			if (rst) *rst = restart_flag;
#endif
				    		}
				    	}
				    }
				    done = 1;
				}
			    break;
			    case 22://{"wifi_mode":"STA"} , {"wifi_mode":"AP"} , {"wifi_mode":"APSTA"}
				if (dev_type > 0) {//master device
#ifdef SET_OTA
				    if (val && !ota_begin) {
#else
				    if (val) {
#endif
				    	wifi_mode_t tmp_mode = WIFI_MODE_NULL;
				    	if (!strcmp(val,"AP")) tmp_mode = WIFI_MODE_AP;
				    	else if (!strcmp(val,"STA")) tmp_mode = WIFI_MODE_STA;
				    	else if (!strcmp(val,"APSTA")) tmp_mode = WIFI_MODE_APSTA;
				    	if ((tmp_mode != WIFI_MODE_NULL) && (tmp_mode != wmode)) {
				    		yes = 0;
				    		prm = 0;
				    		wmode = tmp_mode;
				    		if (!au) {//from broker
				    			prm = 1;
				    		} else {//from tls_client
				    			if (*au) {
				    				prm = 1;
				    			} else yes = -1;
				    		}
				    		if (prm) {
				    			save_param(PARAM_WMODE_NAME, (void *)&wmode, sizeof(uint8_t));
				    			restart_flag = 1;
				    		}
				    		if (rst) *rst = restart_flag;
				    	}
				    	done = 1;
				    }
				}
			    break;
#ifdef SET_TOUCH_PAD
			    case 23://{"dopler_enable":"on"} , {"dopler_enable":"off"}
				if (val) {
				    if (!au) {//from broker
				    	if (!strcmp(val, l_on)) { dopler_flag = 1; yes = 0; }
				    	else if (!strcmp(val, l_off)) { dopler_flag = 0; yes = 0; }
				    } else {//from tls_client
				    	if (*au) {
				    		if (!strcmp(val, l_on)) { dopler_flag = 1; yes = 0; }
				    		else if (!strcmp(val, l_off)) { dopler_flag = 0; yes = 0; }
				    	}
				    }
				    done = 1;
				}
			    break;
#endif
#ifdef SET_OTA
			    case 24://{"ota":"on"}
				if ((val) && (dev_type > 0)) {//master device
				    if (!au) {//from broker
				    	if (!strcmp(val, l_on)) { ota_flag = 1; yes = 0; }
				    } else {//from tls_client
				    	if (*au) {
				    		if (!strcmp(val, l_on)) { ota_flag = 1; yes = 0; }
				    	}
				    }
				    done = 1;
				}
			    break;
			    case 25://{"ota_url":"10.100.0.103:9980/esp32/mqtt.bin"}
				if ((val) && (dev_type > 0)) {//master device
				    k = strlen(val);
				    if (k > 0) {
				    	char *uki = strchr(val, '/');
				    	if (uki) {
				    		memset(ota_filename, 0, ota_param_len);
				    		strcpy(ota_filename, uki);
				    		*uki = '\0';
				    		uki = strchr(val, ':');
				    		if (uki) {
				    			ota_srv_port = atoi(uki + 1);
				    			*uki = '\0';
				    		} else ota_srv_port = 80;
				    		memset(ota_srv, 0, ota_param_len);
				    		strcpy(ota_srv, val);
				    		save_param(PARAM_OTA_IP_NAME, (void *)ota_srv, ota_param_len);
				    		save_param(PARAM_OTA_PORT_NAME, (void *)&ota_srv_port, sizeof(uint32_t));
				    		save_param(PARAM_OTA_FILE_NAME, (void *)ota_filename, ota_param_len);
				    		yes = 0;
				    	}
				    }
				}
			    break;
#endif
			    case 26://{"mqtt_user":"login:password"}
				if ((val) && (dev_type > 0)) {//master device
				    char *uki = strchr(val, ':');
				    if (uki) {
				    	k = strlen(val);
				    	if ((k > 0) && (k < (mqtt_user_name_len * 2))) {
				    		memset(mqtt_user_pass, 0, mqtt_user_name_len);
				    		strcpy(mqtt_user_pass, uki+1);
				    		*uki = '\0';
				    		memset(mqtt_user_name, 0, mqtt_user_name_len);
				    		strcpy(mqtt_user_name, val);
				    		if (save_param(PARAM_MQTT_USER_NAME, (void *)mqtt_user_name, mqtt_user_name_len) == ESP_OK) {
				    			if (save_param(PARAM_MQTT_USER_PASS, (void *)mqtt_user_pass, mqtt_user_name_len) == ESP_OK) yes=0;
				    		}
				    		if (!yes) {
#ifdef SET_OTA
				    			if (!ota_begin) {
				    				restart_flag = 1;
				    				if (rst) *rst = restart_flag;
				    			}
#else
				    			restart_flag = 1;
				    			if (rst) *rst = restart_flag;
#endif
				    		}
				    		done = 1;
				    	}
				    }
				}
			    break;
			    case 27:
#ifdef SET_TLS_SRV
			    if ((val_bin > 0) && (val_bin < 65535) && (dev_type > 0)) {// {"tls_port":4545} //for master device only
			    	prm = 0;
			    	done = 1;
			    	if (!au) prm = 1;//from broker
			    	else {//from tls_client
			    		if (*au) prm = 1;
			    	}
			    	if (prm) {
			    	    if (tls_port != val_bin) {
			    		tls_port = val_bin;
			    		if (save_param(PARAM_TLS_PORT, (void *)&tls_port, sizeof(uint16_t)) == ESP_OK) yes = 0;
			    		if (!yes) {
#ifdef SET_OTA
			    			if (!ota_begin) {
			    				restart_flag = 1;
			    				if (rst) *rst = restart_flag;
			    			}
#else
			    			restart_flag = 1;
			    			if (rst) *rst = restart_flag;
#endif
			    		}
			    	    } else yes = 0;
			    	}
			    }
#endif
			    break;
			    case 28://{"rainbow":"on"} , {"rainbow":"off"}
				if (val) {
				    if (!strcmp(val, l_on)) {
				    	done = 1;
				    	lcmd->mode = 1;
				    	rgb_mode = RGB_RAINBOW;
				    } else if (!strcmp(val, l_off)) {
				    	done = 1;
				    	lcmd->mode = 0;
				    	rgb_mode = RGB_NONE;
				    }
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 29://{"line":"up"} , {"line":"down"} , {"line":"off"}
				if (val) {
				    if (!strcmp(val, l_off)) {
				    	done = 1;
				    	lcmd->mode = 0;
				    	rgb_mode = RGB_NONE;
				    } else if (!strcmp(val, l_up)) {
				    	done = 1;
				    	lcmd->mode = 3;
				    	rgb_mode = RGB_LINEUP;
				    } else if (!strcmp(val, l_down)) {
				    	done = 1;
				    	lcmd->mode = 4;
				    	rgb_mode = RGB_LINEDOWN;
				    }
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;
			    case 30://{"white":"up"} , {"white":"down"} , {"white":"off"}
				if (val) {
				    if (!strcmp(val, l_off)) {
				    	done = 1;
				    	lcmd->mode = 0;
				    	rgb_mode = RGB_NONE;
				    } else if (!strcmp(val, l_up)) {
				    	done = 1;
				    	lcmd->mode = 5;
				    	rgb_mode = WHITE_UP;
				    } else if (!strcmp(val, l_down)) {
				    	done = 1;
				    	lcmd->mode = 6;
				    	rgb_mode = WHITE_DOWN;
				    }
				}
				if (done) yes = 0;
				if (au) if (*au == 0) yes = -1;
			    break;

			}//switch (ind_c)
		    }
		    //---------------------
		    if (val) free(val); val = NULL;
		    val_bin = -1; val_arr = 0;
		    if (done) break;
		}
	    }
	  } else {
//	    ESP_LOGI(__func__, "s_radio *dev == NULL");
	  }
	  cJSON_Delete(obj);
	} else {
//	    ESP_LOGI(__func__, "obj == NULL");
	}

	if (yes != -1) { yes = ind_c; yes |= (subc << 16); }
	last_cmd = ind_c;
	if (adr_dev) *(uint32_t *)adr_dev = (uint32_t)dev;

    //ESP_LOGI(TAG, "parser_json_str return 0x%x", yes);

    return yes;
}
//--------------------------------------------------------------------------------------------------
inline void get_tsensor(t_sens_t *t_s)
{
    t_s->vcc = (uint32_t)(adc1_get_raw(ADC1_TEST_CHANNEL) * 0.8);
    t_s->faren = temprature_sens_read() - 40;
    t_s->cels = (t_s->faren - 32) * 5/9;
}
//--------------------------------------------------------------------------------------------------
char *mk_net_list(uint32_t *pk)
{
s_radio *rec = NULL, *next = NULL;
char line[32];
int all = total_radio();
int i = 32*all + 128;
char *out = NULL;
uint32_t pn;


    if (all > 0) {

	out = (char *)calloc(1, i);
	if (out) {
		sprintf(out,"{\"DevID\":\"%08X\",\"Time\":%u,\"Group\":[", cli_id, (uint32_t)time(NULL));
		i = 0;
		while ( (rec = get_radio(next)) != NULL ) {
			memset(line,0,sizeof(line));
			if (!i) sprintf(line, "\""); else sprintf(line, ",\"");
			sprintf(line+strlen(line), "%08X", (uint32_t)rec->one.dev_id);
			sprintf(line+strlen(line), "\"");
			i++;
			next = rec; rec = NULL;
			sprintf(out+strlen(out),"%s",line);
		}
		sprintf(out+strlen(out),"]");
		if (pk) {
			pn = *pk; pn++; *pk = pn;
			sprintf(out+strlen(out),",\"Pack\":%u}", pn);
		} else sprintf(out+strlen(out),"}\r\n");
	} else {
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAG, "%s: Error get memory %d bytes", __func__, i);
	    #endif
	}

    }

    return out;
}
//--------------------------------------------------------------------------------------------------
inline int make_msg(int *jret, char *buf, uint32_t cli, uint32_t *ind, uint32_t did, void *rad)
{
    if ((!buf) || (!jret)) return 0;

int rt = *jret, subc = 0;
char *tmp = NULL;
s_radio_stat *zap = (s_radio_stat *)rad;
uint8_t sta = 0;
uint32_t fmem;

    memset(buf, 0, strlen(buf));


    if (rt < 0) {//parser_json_str return error

	sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u", did, (uint32_t)time(NULL));
	sprintf(buf+strlen(buf),",\"CmdStatus\":\"Error\",\"Cmd\":\"");
	if (last_cmd < cmd_name_all) sprintf(buf+strlen(buf),"%s\"", CmdName[last_cmd]);
				else sprintf(buf+strlen(buf),"Unknown_%u\"", last_cmd);

    } else {

	subc = rt >> 16;
	rt &= 0xffff;

	switch (rt) {
		case 10://"get"
			fmem = xPortGetFreeHeapSize();
			switch (subc) {
				case 1:// "get":"group"
					tmp = mk_net_list(ind);
					if (tmp) {
						memcpy(buf, tmp, strlen(tmp));
						free(tmp);
					}
					return (strlen(buf));
				case 2:// "get":"sntp_srv"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"sntp_srv\":\"%s\"", did, (uint32_t)time(NULL), fmem, work_sntp);
				break;
				case 3:// "get":"broker"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"broker\":\"%s:%u\"", did, (uint32_t)time(NULL), fmem, broker_host, broker_port);
				break;
#ifdef SET_OTA
				case 4:// "get":"ota_url"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"ota_url\":\"%s:%u%s\"", did, (uint32_t)time(NULL), fmem, ota_srv, ota_srv_port, ota_filename);
				break;
#endif
				case 5:// "get":"wifi_mode"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"wifi_mode\":\"", did, (uint32_t)time(NULL), fmem);
					switch (wmode) {
						case WIFI_MODE_AP:
							sprintf(buf+strlen(buf),"AP\",\"SSID\":\"%s\"", sta_mac);
						break;
						case WIFI_MODE_STA:
							sprintf(buf+strlen(buf),"STA\",\"SSID\":\"%s\"", work_ssid);
						break;
						case WIFI_MODE_APSTA:
							sprintf(buf+strlen(buf),"APSTA\",\"AP\":{\"SSID\":\"%s\"},\"STA\":{\"SSID\":\"%s\"}", sta_mac, work_ssid);
						break;
							default : sprintf(buf+strlen(buf),"NONE\"");
					}
				break;
				case 6:// "get":"mqtt_user" //mqtt_user_name:mqtt_user_pass
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"mqtt_user\":\"%s:%s\"", did, (uint32_t)time(NULL), fmem, mqtt_user_name, mqtt_user_pass);
				break;
				case 7:// "get":"tls_port"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"tls_port\":%u", did, (uint32_t)time(NULL), fmem, tls_port);
				break;
				case 8:// "get":"time_zone"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"time_zone\":\"%s\"", did, (uint32_t)time(NULL), fmem, time_zone);
				break;
				case 9:// "get":"version"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"version\":\"%s\"", did, (uint32_t)time(NULL), fmem, Version);
				break;
				case 10:// "get":"web_port"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"web_port\":%u", did, (uint32_t)time(NULL), fmem, web_port);
				break;
				case 11:// "get":"ws_port"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"ws_port\":%u", did, (uint32_t)time(NULL), fmem, ws_port);
				break;
				case 12:// "get":"ipaddr"
					sprintf(buf,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"ipaddr\":\"%s\"", did, (uint32_t)time(NULL), fmem, localip);
				break;
					default : sta=1;
			}
		break;
			default : sta = 1;
	}
	if (sta) {
		sprintf(buf,"{\"DevID\":\"%08X\"", did);
		s_led_cmd data = {PIXELS, {0, 0, 0}, 0};
		uint32_t curtime;
		uint8_t rgbm = rgb_mode;
		if (!zap) {//master device
			if (xSemaphoreTake(status_mutex, portMAX_DELAY) == pdTRUE) {
				memcpy((uint8_t *)&data, (uint8_t *)&led_cmd, size_led_cmd);
				xSemaphoreGive(status_mutex);
			}
#ifdef UDP_SEND_BCAST
			if (udp_start) sprintf(buf+strlen(buf),",\"udp\":\"on\"");
#endif
			fmem = xPortGetFreeHeapSize();
			curtime = (uint32_t)time(NULL);
		} else {//slave device
			memcpy((uint8_t *)&data, (uint8_t *)&zap->cmd.one.color, size_led_cmd);//sizeof(s_led_cmd));
			fmem = zap->mem;
			curtime = (uint32_t)zap->time;
		}
		sprintf(buf+strlen(buf),",\"Time\":%u,\"FreeMem\":%u", curtime, fmem);
	    switch (rgbm) {
		case RGB_RAINBOW:
			sprintf(buf+strlen(buf),",\"rgb\":\"rainbow\"");
		break;
		case RGB_LINEUP:
			sprintf(buf+strlen(buf),",\"rgb\":\"line_up\"");
		break;
		case RGB_LINEDOWN:
			sprintf(buf+strlen(buf),",\"rgb\":\"line_down\"");
		break;
		case WHITE_UP:
			sprintf(buf+strlen(buf),",\"rgb\":\"white_up\"");
		break;
		case WHITE_DOWN:
			sprintf(buf+strlen(buf),",\"rgb\":\"white_down\"");
		break;
			default : sprintf(buf+strlen(buf),",\"rgb\":[%u,%u,%u]", data.rgb.r, data.rgb.g, data.rgb.b);
	    }
	    if (!zap) {//master device
		if (cli) sprintf(buf+strlen(buf),",\"cli\":\""IPSTR"\"", IP2STR((ip4_addr_t *)&cli));
#ifdef SET_OTA
		if (ota_begin) sprintf(buf+strlen(buf),",\"ota\":\"begin\"");
#endif

#ifdef SET_I2C
    #if defined(SET_BMP) || defined(SET_BH1750) || defined(SET_SI7021)
		result_t fl;
		memset(&fl, 0, sizeof(result_t));
		if (xSemaphoreTake(sensors_mutex, portMAX_DELAY) == pdTRUE) {
		    memcpy((uint8_t *)&fl, (uint8_t *)&sensors, sizeof(result_t));
		    xSemaphoreGive(sensors_mutex);
		}
	#ifdef SET_BH1750
		sprintf(buf+strlen(buf),",\"Lux\":%.2f", fl.lux);
	#endif
	#ifdef SET_BMP
		sprintf(buf+strlen(buf),",\"Temp\":%.2f,\"Press\":%.2f", fl.temp, fl.pres);
		if (fl.humi > 0.0) sprintf(buf+strlen(buf),",\"Humi\":%.2f",fl.humi);
	#endif
	#ifdef SET_SI7021
		sprintf(buf+strlen(buf),",\"Temp2\":%.2f,\"Humi2\":%.2f", fl.si_temp, fl.si_humi);
	#endif
    #endif
#endif
		t_sens_t t_s;
		if (!zap) get_tsensor(&t_s);//master device
			else {//slave device
			    t_s.cels = zap->temp;
			    t_s.vcc = zap->vcc;
			}
		sprintf(buf+strlen(buf),",\"TempChip\":%.2f,\"Pwr\":%u", t_s.cels, t_s.vcc);
	    }
	}

    }

    if (ind) {//for broker
	uint32_t i = *ind; i++; *ind = i;
	sprintf(buf+strlen(buf),",\"Pack\":%u}", i);
    } else sprintf(buf+strlen(buf),"}\r\n");//for tls_client

    return (strlen(buf));
}
#ifdef SET_MQTT
//--------------------------------------------------------------------------------------------------
void to_broker_from_slave(s_radio_cmd *rec, int cmd, s_radio *zap, s_led_cmd *cmdl)
{
    s_radio_one *czap = get_radio_from_list((void *)zap);
    if (czap) {
	memset((uint8_t *)rec, 0, size_radio_cmd);
	rec->flag = cmd;
	if (cmdl) memcpy((uint8_t *)&czap->color, (uint8_t *)cmdl, size_led_cmd);
	memcpy((uint8_t *)&rec->one, (uint8_t *)czap, size_radio_one);
	if (cmdl) update_radio_color(zap, cmdl);
	if (xQueueSend(radio_cmd_queue, (void *)rec, (TickType_t)0) != pdPASS) {
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAG_GET, "%s: Error while sending to radio_cmd_queue", __func__);
	    #endif
	}
	free(czap);
    } else {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAG_GET, "%s: get_radio_from_list: Error !", __func__);
	#endif
    }
}
//--------------------------------------------------------------------------------------------------
void iot_subscribe_callback_handler(AWS_IoT_Client *pClient,
					char *topicName,
					uint16_t topicNameLen,
					IoT_Publish_Message_Params *params,
					void *pData)
{
s_led_cmd cmdl;
int ret = 0, rt, mult;
uint32_t adr;
s_radio *zap = NULL;
s_radio_cmd rec;

    //ESP_LOGW(TAG_GET, "[%.*s] %.*s", topicNameLen, topicName, params->payloadLen, (char *)params->payload);

    char *ukd = (char *)calloc(1, params->payloadLen + 1);
    char *ukt = (char *)calloc(1, topicNameLen + 1);
    if ((ukd) && (ukt)) {
	memcpy(ukt, topicName, topicNameLen);
	memcpy(ukd, (char *)params->payload, params->payloadLen);
	print_msg(TAG_GET, ukt, ukd, 1);
    }
    if (ukt) free(ukt); if (ukd) free(ukd);

    ret = mult = parser_json_str((char *)params->payload, (void *)&cmdl, NULL, NULL, NULL, (void *)&adr);

    if (mult != -1) ret &= 0xffff;
    zap = (s_radio *)adr;
    if (zap) {
	rt = isMaster(zap);
	switch (ret) {//ret = cmd number
		case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:// case 9://leds command
		case 28://{"rainbow":"on"},{"rainbow":"off"}
		case 29://{"line":"up"},{"line":"down"},{"line":"off"}
		case 30://{"white":"up"},{"white":"down"},{"white":"off"}
			switch (rt) {
				case 1://cmd for master device
					if (xQueueSend(led_cmd_queue, (void *)&cmdl, (TickType_t)0) != pdPASS) {
					    #ifdef SET_ERROR_PRINT
						ESP_LOGE(TAG_GET, "Error while sending to led_cmd_queue.");
					    #endif
					}
				break;
				case 0://cmd for slave device
					to_broker_from_slave(&rec, ret, zap, &cmdl);
				break;
			}
		break;
		case 10://get:status,group,sntp_srv,broker,ota_url,wifi_mode,tls_port,time_zone,version,web_port,ws_port,ipaddr
		case 11://udp
		case 12://sntp
		case 13://sntp_srv
		case 14://time_zone
		case 16://time
		case 17://i2c_prn
		case 18://tls_hangup
		case 23://{"dopler_enable":"on"} , {"dopler_enable":"off"}
		case 27://{"tls_port":4545}
		case -1: case -2:
			switch (rt) {
				case 1://cmd for master device
					if (xQueueSend(mqtt_evt_queue, (void *)&mult, (TickType_t)0) != pdPASS) {
					    #ifdef SET_ERROR_PRINT
						ESP_LOGE(TAG_GET, "Error while sending to mqtt_evt_queue.");
					    #endif
					}
				break;
				case 0://cmd for slave device
					to_broker_from_slave(&rec, mult, zap, NULL);
				break;
			}
		break;
		/*case 15://restart
		case 19://ssid
		case 20://key
		case 21://set new broker
		case 22://{"wifi_mode":"STA"} , {"wifi_mode":"AP"} , {"wifi_mode":"APSTA"}
		case 23://{"ota":"on"}
		case 24://{"ota_url":"10.100.0.103:9980/esp32/mqtt.bin"}
		case 26://{"mqtt_user":"login:password"}
		//case 27://{"tls_port":4545}
		break;*/
	}//switch(ret)
    }//if (zap)

}
//------------------------------------------------------------------------------------------------------------
void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data)
{
    if (pClient) {
        ets_printf("%s[%s] MQTT Disconnect%s\n", BROWN_COLOR, TAG, STOP_COLOR);
        mqtt_client_connected = 0;
    }
}
//--------------------------------------------------------------------------------------------------
const char *cli_state(ClientState cs)
{
    switch (cs) {
	case CLIENT_STATE_INVALID:// = 0,
		return "INVALID";
	case CLIENT_STATE_INITIALIZED:// = 1,
		return "INITIALIZED";
	case CLIENT_STATE_CONNECTING:// = 2,
		return "CONNECTING";
	case CLIENT_STATE_CONNECTED_IDLE:// = 3,
		return "CONNECTED_IDLE";
	case CLIENT_STATE_CONNECTED_YIELD_IN_PROGRESS:// = 4,
		return "CONNECTED_YIELD_IN_PROGRESS";
	case CLIENT_STATE_CONNECTED_PUBLISH_IN_PROGRESS:// = 5,
		return "CONNECTED_PUBLISH_IN_PROGRESS";
	case CLIENT_STATE_CONNECTED_SUBSCRIBE_IN_PROGRESS:// = 6,
		return "CONNECTED_SUBSCRIBE_IN_PROGRESS";
	case CLIENT_STATE_CONNECTED_UNSUBSCRIBE_IN_PROGRESS:// = 7,
		return "CONNECTED_UNSUBSCRIBE_IN_PROGRESS";
	case CLIENT_STATE_CONNECTED_RESUBSCRIBE_IN_PROGRESS:// = 8,
		return "CONNECTED_RESUBSCRIBE_IN_PROGRESS";
	case CLIENT_STATE_CONNECTED_WAIT_FOR_CB_RETURN:// = 9,
		return "CONNECTED_WAIT_FOR_CB_RETURN";
	case CLIENT_STATE_DISCONNECTING:// = 10,
		return "DISCONNECTING";
	case CLIENT_STATE_DISCONNECTED_ERROR:// = 11,
		return "DISCONNECTED_ERROR";
	case CLIENT_STATE_DISCONNECTED_MANUALLY:// = 12,
		return "DISCONNECTED_MANUALLY";
	case CLIENT_STATE_PENDING_RECONNECT:// = 13
		return "PENDING_RECONNECT";
	default: return "UNKNOWN";
    }
}
//--------------------------------------------------------------------------------------------------
void mqtt_task(void *arg)
{
char cPayload[max_inbuf];
uint32_t ipk = 0, epk = 0, net_count = 0, tls_cli;
uint32_t *ukpk = NULL;
const uint8_t topic_name_len = 64;
int evt, nevt;
uint8_t faza = 0, last_faza = 0, from_faza = 0, sign = 0;
broker_t br;
char *net_str = NULL, *uk_topic = NULL;
char TOPIC_DAT[topic_name_len];
char TOPIC_ACK[topic_name_len];
char TOPIC_NET[topic_name_len];
char TOPIC_CMD[topic_name_len];
TickType_t tmr_rec, tmr_wcon, con_try = get_tmr(10000);
bool save_rec = false;
s_radio_stat r_stat;
IoT_Error_t rc = FAILURE;
AWS_IoT_Client client;
IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;
IoT_Publish_Message_Params paramsQOS1;

    total_task++;
    mqtt_client_connected = 0;
    memcpy((uint8_t *)&br, (uint8_t *)arg, sizeof(broker_t));

    paramsQOS1.qos = QOS1;
    paramsQOS1.payload = (void *)cPayload;
    paramsQOS1.isRetained = 1;//0;
    sprintf(TOPIC_DAT, "data/%08X",cli_id);
    sprintf(TOPIC_ACK, "ack/%08X", cli_id);
    sprintf(TOPIC_NET, "net/%08X", cli_id);
    int TOPIC_CMD_LEN = sprintf(TOPIC_CMD, "cmd/%08X", cli_id);

    ets_printf("%s[%s] MQTT client task starting...(broker=%s:%u) | FreeMem %u%s\n", START_COLOR, TAG, br.ip, br.port, xPortGetFreeHeapSize(), STOP_COLOR);

while (!restart_flag) {//main loop


    switch (faza) {

	case 0://wait wmode == WIFI_MODE_STA || wmode == WIFI_MODE_APSTA
	    if (xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, 1000 / portTICK_PERIOD_MS) & CONNECTED_BIT) {
		if (wmode&1) faza = 1;// == WIFI_MODE_STA) || (wmode != WIFI_MODE_APSTA))
	    } else faza = 4;
	    from_faza = 0;
	break;
	case 1://init client
	    mqttInitParams.enableAutoReconnect = false;
	    mqttInitParams.pHostURL = br.ip;//broker_host;//HostAddress;
	    mqttInitParams.port = br.port;//broker_port;//port
	    mqttInitParams.pRootCALocation = ca_crt;
	    mqttInitParams.pDeviceCertLocation = server_cert;//certificate_pem_crt_start;
	    mqttInitParams.pDevicePrivateKeyLocation = server_key;//private_pem_key_start;
	    mqttInitParams.mqttCommandTimeout_ms = 9000;//12000;
	    mqttInitParams.tlsHandshakeTimeout_ms = 7000;//6000;
	    mqttInitParams.isSSLHostnameVerify = false;
	    mqttInitParams.disconnectHandler = disconnectCallbackHandler;
	    mqttInitParams.disconnectHandlerData = NULL;
	    mqttInitParams.isBlockOnThreadLockEnabled = true;
	    rc = aws_iot_mqtt_init(&client, &mqttInitParams);
	    if (rc != SUCCESS) {
		ESP_LOGE(TAG, "aws_iot_mqtt_init returned error : %d ", rc);
		vTaskDelay(2000 / portTICK_RATE_MS);
		faza = 0;
	    } else {//goto start connect to broker
		connectParams.keepAliveIntervalInSec = 30;//10;//30//8;//15; - ping intreval
		connectParams.isCleanSession = true;
		connectParams.MQTTVersion = MQTT_3_1_1;
		connectParams.pClientID = sta_mac;//cli_id;//'esp32-mac_adress'
		connectParams.clientIDLen = (uint16_t)strlen(sta_mac);//(uint16_t)strlen(cli_id);
		connectParams.isWillMsgPresent = false;
		connectParams.pUsername = mqtt_user_name;//CONFIG_AWS_EXAMPLE_CLIENT_ID;
		connectParams.usernameLen = (uint16_t)strlen(mqtt_user_name);
		connectParams.pPassword = mqtt_user_pass;//CONFIG_AWS_EXAMPLE_CLIENT_ID;
		connectParams.passwordLen = (uint16_t)strlen(mqtt_user_pass);
		ets_printf("[%s] Connecting to MQTT server %s:%d ...\n", TAG, mqttInitParams.pHostURL, mqttInitParams.port);
		faza = 2;
		tmr_wcon = get_tmr(0);
	    }
	    from_faza = 1;
	break;
	case 2://start connect to broker
	    if (check_tmr(tmr_wcon)) {
		tmr_wcon = get_tmr(2000);
		rc = aws_iot_mqtt_connect(&client, &connectParams);
		if (rc != SUCCESS) {
			ESP_LOGE(TAG, "Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
			faza = 4;
		} else {
			mqtt_client_connected = 1;
			ets_printf("[%s] Connection state: '%s'. Subscribing to topic '%s' ...\n", TAG,
						cli_state(aws_iot_mqtt_get_client_state(&client)), TOPIC_CMD);
			faza = 3;
		}
	    } else faza = 4;
	    from_faza = 2;
	break;
	case 3://start Subscribing
	    rc = aws_iot_mqtt_subscribe(&client, TOPIC_CMD, TOPIC_CMD_LEN, QOS1, iot_subscribe_callback_handler, NULL);//inbuf);
	    if (rc != SUCCESS) {
		sign = 0;
		ESP_LOGE(TAG, "Error subscribing to topic '%.*s' : %d | FreeMem %u", TOPIC_CMD_LEN, TOPIC_CMD, rc, xPortGetFreeHeapSize());
		faza = 4;
	    } else {//goto publish
		sign = 1;
		tmr_rec = get_tmr(100);
		ets_printf("[%s] Subscribe OK. LOG: read_seek=%u write_seek=%u total=%u\n", TAG, (int)hdr_log.rseek, (int)hdr_log.wseek, hdr_log.total);
		faza = 5;
	    }
	    from_faza = 3;
	break;
	case 4:
	    if (!mqtt_client_connected) {
		if (xQueueReceive(mqtt_evt_queue, &evt, 10/portTICK_RATE_MS) == pdTRUE) {//check data_topic from master
			if (evt >= 0) {//for TOPIC_DAT only
				tls_cli = tls_client_ip;
				memset(cPayload, 0, sizeof(cPayload));
				paramsQOS1.payload = (void *)cPayload;
				paramsQOS1.payloadLen = make_msg(&evt, cPayload, tls_cli, &ipk, cli_id, NULL);
				save_rec_log(cPayload, strlen(cPayload));
				if (ipk > 0) ipk--;
			}
			evt = 0;
		}
		if (xQueueReceive(radio_evt_queue, &r_stat, 10/portTICK_RATE_MS) == pdTRUE) {//check data_topic from slave
			if (r_stat.cmd.flag >= 0) {//for TOPIC_DAT only
				tls_cli = 0;
				memset(cPayload, 0, sizeof(cPayload));
				paramsQOS1.payload = (void *)cPayload;
				paramsQOS1.payloadLen = make_msg(&r_stat.cmd.flag, cPayload, tls_cli, &ipk, r_stat.cmd.one.dev_id, (void *)&r_stat);
				save_rec_log(cPayload, strlen(cPayload));
				if (ipk > 0) ipk--;
			}
		}
	    }
	    if (sign) faza = 5; else faza = from_faza;
	    from_faza = 4;
	break;
	case 5:
	    from_faza = 5;
	    rc = aws_iot_mqtt_yield(&client, 250);//Max time the yield function will wait for read messages
	    if (rc == NETWORK_ATTEMPTING_RECONNECT) {
		mqtt_client_connected = 0;
	    } else {
		if ((rc == SUCCESS) ||
			(rc == NETWORK_RECONNECTED) ||
				(rc == MQTT_NOTHING_TO_READ) ||
					(rc == MQTT_CONNACK_CONNECTION_ACCEPTED)) mqtt_client_connected = 1;
	    }

	    if (mqtt_client_connected) {

		if (xQueueReceive(mqtt_net_queue, &nevt, 50/portTICK_RATE_MS) == pdTRUE) {
			net_str = mk_net_list(&net_count);
			if (net_str) {
				paramsQOS1.payload = (void *)net_str;
				paramsQOS1.payloadLen = strlen(net_str);
				rc = aws_iot_mqtt_publish(&client, TOPIC_NET, strlen(TOPIC_NET), &paramsQOS1);
				print_msg(TAG_PUT, TOPIC_NET, net_str, 1);
				free(net_str);
				if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
					ets_printf("%s[%s] QOS1 publish ack not received (%s)%s\n", BROWN_COLOR, TAG_PUT, TOPIC_NET, STOP_COLOR);
					if (net_count > 0) net_count--;
					if (xQueueSend(mqtt_net_queue, (void *)&nevt, (TickType_t)0) != pdPASS) {
						ESP_LOGE(TAG_PUT, "Error while sending to mqtt_net_queue");
					}
				}
			} else {
				ESP_LOGE(TAG_PUT, "Error: mk_net_list() -> list Empty");
			}
			nevt = 0;
		}
		// read saved records for sending to broker
		if (check_tmr(tmr_rec)) {
			if (!read_rec_log(cPayload, &ipk)) {
				uk_topic = TOPIC_DAT;
				paramsQOS1.payload = (void *)cPayload;
				paramsQOS1.payloadLen = strlen((char *)cPayload);
				rc = aws_iot_mqtt_publish(&client, uk_topic, strlen(uk_topic), &paramsQOS1);
				print_msg(TAG_PUT, uk_topic, cPayload, 1);
				if (rc == MQTT_REQUEST_TIMEOUT_ERROR)
					ets_printf("%s[%s] QOS1 publish ack not received (%s)%s\n", BROWN_COLOR, TAG_PUT, uk_topic, STOP_COLOR);
				break;
			} else tmr_rec = get_tmr(500);
		}

		if (xQueueReceive(mqtt_evt_queue, &evt, 25/portTICK_RATE_MS) == pdTRUE) {
			tls_cli = tls_client_ip;
			if (evt >= 0) { uk_topic = TOPIC_DAT; ukpk = &ipk; }
				 else { uk_topic = TOPIC_ACK; ukpk = &epk; }
			memset(cPayload, 0, sizeof(cPayload));
			paramsQOS1.payload = (void *)cPayload;
			paramsQOS1.payloadLen = make_msg(&evt, cPayload, tls_cli, ukpk, cli_id, NULL);
			save_rec = false;
			rc = aws_iot_mqtt_publish(&client, uk_topic, strlen(uk_topic), &paramsQOS1);
			print_msg(TAG_PUT, uk_topic, cPayload, 1);
			if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
				ets_printf("%s[%s] QOS1 publish ack not received (%s)%s\n", BROWN_COLOR, TAG_PUT, uk_topic, STOP_COLOR);
				if (*ukpk > 0) *ukpk -= 1;
				save_rec = true;
			}
			if (save_rec) {
				if (uk_topic == TOPIC_DAT) save_rec_log(cPayload, strlen(cPayload));
				if (*ukpk > 0) *ukpk -= 1;
			}
			evt = 0;
		}

		if (xQueueReceive(radio_evt_queue, &r_stat, 25/portTICK_RATE_MS) == pdTRUE) {
			tls_cli = 0;
			if (r_stat.cmd.flag >= 0) { uk_topic = TOPIC_DAT; ukpk = &ipk; }
					     else { uk_topic = TOPIC_ACK; ukpk = &epk; }
			memset(cPayload, 0, sizeof(cPayload));
			paramsQOS1.payload = (void *)cPayload;
			paramsQOS1.payloadLen = make_msg(&r_stat.cmd.flag, cPayload, tls_cli, ukpk, r_stat.cmd.one.dev_id, (void *)&r_stat);
			save_rec = false;
			rc = aws_iot_mqtt_publish(&client, uk_topic, strlen(uk_topic), &paramsQOS1);
			print_msg(TAG_PUT, uk_topic, cPayload, 1);
			if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
				ets_printf("%s[%s] QOS1 publish ack not received (%s)%s\n", BROWN_COLOR, TAG_PUT, uk_topic, STOP_COLOR);
				if (*ukpk > 0) *ukpk -= 1;
				save_rec = true;
			}
			if (save_rec) {
				if (uk_topic == TOPIC_DAT) save_rec_log(cPayload, strlen(cPayload));
				if (*ukpk > 0) *ukpk -= 1;
			}
		}

	    } else {
		if (check_tmr(con_try)) {
			rc = aws_iot_mqtt_attempt_reconnect(&client);
			if (rc == NETWORK_RECONNECTED) mqtt_client_connected = 1;
			con_try = get_tmr(10000);
		}
		faza = 4;
	    }
	break;
	    default:
	    {
		ESP_LOGE(TAG, "MQTT_TASK: ERROR -> LAST_STAGE=%u ERROR_STAGE=%u FROM_STAGE=%u", last_faza, faza, from_faza);
		faza = 255;
	    }
    }//switch (faza)

    if (last_faza != faza) last_faza = faza;

    if (faza == 255) break;

}//main loop


if (mqtt_client_connected) aws_iot_mqtt_disconnect(&client);


mqtt_client_connected = 0;
ets_printf("%s[%s] MQTT client task stop | FreeMem %u%s\n", START_COLOR, TAG, xPortGetFreeHeapSize(), STOP_COLOR);
if (total_task) total_task--;

vTaskDelete(NULL);

}

#endif
//------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------
bool check_pin(uint8_t pin_num)
{
    gpio_pad_select_gpio(pin_num);
    gpio_pad_pullup(pin_num);
    if (gpio_set_direction(pin_num, GPIO_MODE_INPUT) != ESP_OK) return true;

    return (bool)(gpio_get_level(pin_num));
}
//------------------------------------------------------------------------------------------------------------
#ifdef UDP_SEND_BCAST
#define max_line 256
void udp_task(void *par)
{
udp_start = 1;
err_t err = ERR_OK;
u16_t len, port = 8004;
struct netconn *conn = NULL;
struct netbuf *buf = NULL;
void *data = NULL;
uint32_t cnt = 1;
TickType_t tmr_sec;
char line[max_line] = {0};
bool loop = true;

    total_task++;

    ets_printf("%s[%s] BROADCAST task started | FreeMem %u%s\n", START_COLOR, TAGU, xPortGetFreeHeapSize(), STOP_COLOR);

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    while (loop && !restart_flag) {

	if (!udp_flag) { loop = false; break; }

	conn = netconn_new(NETCONN_UDP);
	if (!conn) {
	    vTaskDelay(1000 / portTICK_PERIOD_MS);
	    loop = false;
	}

	tmr_sec = get_tmr(100);//0 msec

	while (loop) {
		if (!udp_flag) { loop = false; break; }
		if (check_tmr(tmr_sec)) {
			buf = netbuf_new();
			if (buf) {
				memset(line,0,max_line);
				len = sprintf(line,"{\"DevID\":\"%08X\",\"Time\":%u,\"FreeMem\":%u,\"Ip\":\"%s\",\"To\":\"%s\",\"Pack\":%u}\r\n",
							cli_id,
							(uint32_t)time(NULL),
							xPortGetFreeHeapSize(),
							localip, ip4addr_ntoa(&bca.u_addr.ip4), cnt);
				data = netbuf_alloc(buf, len);
				if (data) {
					memset((char *)data, 0, len);
					memcpy((char *)data, line, len);
					err = netconn_sendto(conn, buf, (const ip_addr_t *)&bca, port);
					if (err != ERR_OK) {
						ESP_LOGE(TAGU,"Sending '%.*s' error=%d '%s'", len, (char *)data, (int)err, lwip_strerr(err));
					} else {
						print_msg(TAGU, NULL, line, 1);
						cnt++;
					}
				}
				netbuf_delete(buf); buf = NULL;
			}
			tmr_sec = get_tmr(10000);//10 sec
		} else vTaskDelay(50 / portTICK_PERIOD_MS);
		if (restart_flag) { loop = false; break; }
	}

	if (conn) { netconn_delete(conn); conn = NULL; }

    }

    udp_start = 0;
    if (total_task) total_task--;
    ets_printf("%s[%s] BROADCAST task stoped | FreeMem %u%s\n", START_COLOR, TAGU, xPortGetFreeHeapSize(), STOP_COLOR);

    vTaskDelete(NULL);
}
#endif
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void initialise_wifi(wifi_mode_t w_mode)
{

    tcpip_adapter_init();

    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "esp32.dev");

    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(w_mode));

    switch ((uint8_t)w_mode) {
		case WIFI_MODE_STA :
		{
			wifi_config_t wifi_config; memset((uint8_t *)&wifi_config, 0, sizeof(wifi_config_t));
#ifdef SET_WPS
			set_wps_mode = !check_pin(GPIO_WPS_PIN);//pin21 ???
			if (!set_wps_mode) {
#endif
				if (wifi_param_ready) {
					memcpy(wifi_config.sta.ssid, work_ssid, strlen(work_ssid));
					memcpy(wifi_config.sta.password, work_pass, strlen(work_pass));
					ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
					ets_printf("[%s] WIFI_MODE - STA: '%s':'%s'\n", TAGN, wifi_config.sta.ssid, wifi_config.sta.password);
				}
				ESP_ERROR_CHECK(esp_wifi_start());
#ifdef SET_WPS
			} else {
				ets_printf("%s[%s] === CHECK_WPS_PIN %d LEVEL IS %d ===%s\n", GREEN_COLOR, TAGTN, GPIO_WPS_PIN, !set_wps_mode, STOP_COLOR);
				ESP_ERROR_CHECK(esp_wifi_start());
				ESP_ERROR_CHECK(esp_wifi_wps_enable(WPS_TEST_MODE));
				ESP_ERROR_CHECK(esp_wifi_wps_start(0));
			}
#endif
		}
		break;
		case WIFI_MODE_AP:
		{
			wifi_config_t wifi_configap; memset((uint8_t *)&wifi_configap, 0, sizeof(wifi_config_t));
			wifi_configap.ap.ssid_len = strlen(sta_mac);
			memcpy(wifi_configap.ap.ssid, sta_mac, wifi_configap.ap.ssid_len);
			memcpy(wifi_configap.ap.password, work_pass, strlen(work_pass));
			wifi_configap.ap.channel = 6;
			wifi_configap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
			wifi_configap.ap.ssid_hidden = 0;
			wifi_configap.ap.max_connection = 4;
			wifi_configap.ap.beacon_interval = 100;
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_configap));
			ESP_ERROR_CHECK(esp_wifi_start());
			ets_printf("[%s] WIFI_MODE - AP: '%s':'%s'\n", TAGN, wifi_configap.ap.ssid, work_pass);
		}
		break;
		case WIFI_MODE_APSTA:
		{
			wifi_config_t wifi_config;   memset((uint8_t *)&wifi_config, 0, sizeof(wifi_config_t));
			memcpy(wifi_config.sta.ssid, work_ssid, strlen(work_ssid));
			memcpy(wifi_config.sta.password, work_pass, strlen(work_pass));
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

			wifi_config_t wifi_configap; memset((uint8_t *)&wifi_configap, 0, sizeof(wifi_config_t));
			wifi_configap.ap.ssid_len = strlen(sta_mac);
			memcpy(wifi_configap.ap.ssid, sta_mac, wifi_configap.ap.ssid_len);
			memcpy(wifi_configap.ap.password, work_pass, strlen(work_pass));
			wifi_configap.ap.channel = 6;
			wifi_configap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
			wifi_configap.ap.ssid_hidden = 0;
			wifi_configap.ap.max_connection = 4;
			wifi_configap.ap.beacon_interval = 100;
			ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_configap));

			ESP_ERROR_CHECK(esp_wifi_start());
			ets_printf("[%s] WIFI_MODE - AP: '%s':'%s' , STA: '%s':'%s'\n", TAGN,
						wifi_configap.ap.ssid, wifi_configap.ap.password,
						wifi_config.sta.ssid, wifi_config.sta.password);
		}
		break;
    }
}
//********************************************************************************************************************
esp_err_t read_param(char *param_name, void *param_data, size_t len)//, int8_t type)
{
esp_err_t err;
nvs_handle mhd;
size_t size = len;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &mhd);
//    err = nvs_open_from_partition(STORAGE_NAMESPACE, (const char *)param_name, NVS_READONLY, &mhd);
    if (err != ESP_OK) {
#ifdef SET_ERROR_PRINT
	ESP_LOGE(TAGN, "%s(%s): Error open '%s'", __func__, param_name, STORAGE_NAMESPACE);
#endif
    } else {//OK
	err = nvs_get_blob(mhd, param_name, param_data, &size);
	if (err != ESP_OK) {
#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGN, "%s: Error read '%s' from '%s'", __func__, param_name, STORAGE_NAMESPACE);
#endif
	}
	nvs_close(mhd);
    }

    return err;
}
//--------------------------------------------------------------------------------------------------
esp_err_t save_param(char *param_name, void *param_data, size_t len)
{
esp_err_t err;
size_t size = len;
nvs_handle mhd;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &mhd);
//    err = nvs_open_from_partition(STORAGE_NAMESPACE, (const char *)param_name, NVS_READWRITE, &mhd);
    if (err != ESP_OK) {
#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGN, "%s(%s): Error open '%s'", __func__, param_name, STORAGE_NAMESPACE);
#endif
    } else {
	err = nvs_set_blob(mhd, param_name, (uint8_t *)param_data, size);
	if (err != ESP_OK) {
#ifdef SET_ERROR_PRINT
		ESP_LOGE(TAGN, "%s: Error save '%s' with len %u to '%s'", __func__, param_name, size, STORAGE_NAMESPACE);
#endif
	} else {
		err = nvs_commit(mhd);
	}
	nvs_close(mhd);
    }

    return err;
}
//********************************************************************************************************************
esp_err_t mount_disk(void)
{
    const esp_vfs_fat_mount_config_t mount_conf = {
	.max_files = 4,
        .format_if_mount_failed = true
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, base_part, &mount_conf, &s_wl_handle);
    if (err != ESP_OK) {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGFAT, "%s: Failed to mount FATFS partition '%s' on device '%s' (0x%x)", __func__, base_part, base_path, err);
	#endif
    } else {
	fatfs_size = wl_size(s_wl_handle);
	ets_printf("[%s] Mount FATFS partition '%s' on device '%s' done (size=%u)\n", TAGFAT, base_part, base_path, fatfs_size);
    }

    return err;
}
//--------------------------------------------------------------------------------------------------
void umount_disk(esp_err_t ok)
{
    if (ok) {
	ets_printf("[%s] Unmounting FAT filesystem partition '%s'\n", TAGFAT, base_part);
	ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle));
    }
}
//------------------------------------------------------------------------------------------------------------
FILE *open_file(const char *filename, const char *open_mode)
{
    if (fs_ok == ESP_OK) {
	char fname[127]={0}; sprintf(fname, "%s/%s", base_path, filename);
	return (fopen(fname, open_mode));
    } else return NULL;
}
//------------------------------------------------------------------------------------------------------------
void close_file(FILE *f)
{
    if ((fs_ok == ESP_OK) && (f)) fclose(f);
}
//------------------------------------------------------------------------------------------------------------
int delete_file(const char *filename)
{
    if (fs_ok == ESP_OK) {
	char fname[127] = {0}; sprintf(fname, "%s/%s", base_path, filename);
	return (unlink(fname));
    } else return -1;
}
//------------------------------------------------------------------------------------------------------------
int put_to_file(FILE *f, char *buf, uint8_t prn)
{
int ret = -1;

    if (fs_ok == ESP_OK) {
	if (f) {
		ret = fputs(buf, f);
		if (prn) {
		    ets_printf("[%s] Put to file %d bytes\n", TAGFAT, ret);
		}
	} else {
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAGFAT, "%s: Error file pointer", __func__);
	    #endif
	}
    } else {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGFAT, "%s: FATFS not mounted", __func__);
	#endif
    }

    return ret;
}
//------------------------------------------------------------------------------------------------------------
int get_from_file(FILE *f, char *buf, uint8_t prn)
{
int ret = -1;

    if (fs_ok == ESP_OK) {
	if (f) {
		if (fgets(buf, strlen(buf)-1, f)) ret=strlen(buf);
		if (prn) {
			ets_printf("[%s] Get from file %d bytes\n", TAGFAT, ret);
			if (ret > 0) ets_printf("%.*s\n", ret, buf);
		}
	} else {
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAGFAT, "%s: Error file pointer", __func__);
	    #endif
	}
    } else {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGFAT, "%s: FATFS not mounted", __func__);
	#endif
    }

    return ret;
}
//------------------------------------------------------------------------------------------------------------
int get_file_size(const char *filename)
{
int ret = -1;

    if (fs_ok == ESP_OK) {
	char *nm = (char *)calloc(1, strlen(base_path) + strlen(filename) + 3);
	if (nm) {
		sprintf(nm, "%s/%s", base_path, filename);
		struct stat sta;
		stat(nm, &sta);
		ret = sta.st_size;
		free(nm);
	}
    } else {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGFAT, "%s: FATFS not mounted", __func__);
	#endif
    }

    return ret;
}
//------------------------------------------------------------------------------------------------------------
int bin_file_read(FILE *f, uint8_t *buf, int len)
{
int ret = -1;

    if ((fs_ok == ESP_OK) && (f)) ret = fread(buf, 1, len, f);

    return ret;
}
//------------------------------------------------------------------------------------------------------------
int bin_file_write(FILE *f, uint8_t *buf, int len)
{
int ret = -1;

    if ((fs_ok == ESP_OK) && (f)) ret = fwrite(buf, 1, len, f);

    return ret;
}
//********************************************************************************************************************
int create_radio_file(const char *filename, uint8_t *buf, int len, uint8_t prn)
{
int ret = -1;

    if (fs_ok == ESP_OK) {
	char fname[127] = {0};
	sprintf(fname, "%s/%s", base_path, filename);
	FILE *f = fopen(fname, "wb");
	if (f) {
		ret = fwrite(buf, 1, len, f);
		fclose(f);
		if (prn) {
		    ets_printf("[%s] Write file '%s' (write %d bytes)\n", TAGFAT, fname, ret);
		}
	} else {
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAGFAT, "%s: Failed to open file '%s' for writing", __func__, fname);
	    #endif
	}
    }

    return ret;
}
//--------------------------------------------------------------------------------------------------
int make_radio_file(int kols)
{
const int dl = size_led_cmd;
int lenf = 0, j, ret = 0;
s_led_cmd *cd = (s_led_cmd *)calloc(1, dl);

    if (!cd) return 0;

    if (xSemaphoreTake(status_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy((uint8_t *)cd, (uint8_t *)&led_cmd, dl);
        xSemaphoreGive(status_mutex);
    }

    uint32_t idd = cli_id;

    lenf = size_radio_one * kols;
    s_radio_one rec[kols];
    for (j = 0; j < kols; j++) {
	rec[j].dev_id = idd;
	memcpy( (uint8_t *)&rec[j].color, (uint8_t *)cd, dl);
	if (j) memset((uint8_t *)&rec[j].color.rgb, j, 3);
	idd++;
    }
    free(cd);
    if (create_radio_file(radio_fname, (uint8_t *)&rec[0], lenf, 1) > 0) ret = kols;

    return ret;
}
//--------------------------------------------------------------------------------------------------
void print_radio_file()
{
uint32_t kol = 0;
s_radio_one zap;
char *stx = NULL;

    stx = (char *)calloc(1, 256);
    if (stx) {
	FILE *fradio = open_file(radio_fname, RDBYTE);
	if (fradio) {
		while (1) {
			if (bin_file_read(fradio, (uint8_t *)&zap, size_radio_one) != size_radio_one) break;
			kol++;
			memset(stx, 0, 256);
			sprintf(stx,"[%02d]: id=", kol);
			sprintf(stx+strlen(stx), "%08X", (uint32_t)zap.dev_id);
			sprintf(stx+strlen(stx)," rgb[%u.%u]:[%u,%u,%u]",
						zap.color.mode, zap.color.size,
						zap.color.rgb.r, zap.color.rgb.g, zap.color.rgb.b);
			print_msg(TAGFAT, NULL, stx, 0);
		}
		close_file(fradio);
	} else {
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAGFAT,"%s: open_file(%s,%s) Error (fs_ok=%d)", __func__, radio_fname, RDBYTE, fs_ok);
	    #endif
	}
	free(stx);
    }

}
//--------------------------------------------------------------------------------------------------
int update_radio_file()
{
int kol = 0, ret = 0;
s_radio *cur = NULL, *next = NULL;
FILE *fradio = NULL;
bool err = true;

    if (fs_ok != ESP_OK) return ret;

    if (xSemaphoreTake(radio_mutex, 50/portTICK_RATE_MS) == pdTRUE) {
	if ((radio_hdr.counter > 0) && (radio_hdr.first)) {
		fradio = open_file(radio_fname, WRPLUS);
		if (fradio) {
			err = false;
			cur = radio_hdr.first;
			while (cur) {
				if (bin_file_write(fradio, (uint8_t *)cur->one.dev_id, size_radio_one) == -1) {
					err = true;
					break;
				} else {
					kol++;
					next = cur->next;
					cur = next;
				}
			}
			close_file(fradio);
		}
	}
	xSemaphoreGive(radio_mutex);
    }

    if (!err) {
	ret = kol;
	ets_printf("[%s] %s: %d records update -> OK\n", TAGFAT, __func__, ret);
    } else {
	ESP_LOGE(TAGFAT, "%s: %d records update -> Error", __func__, ret);
    }

    return ret;
}
//--------------------------------------------------------------------------------------------------
void print_log_file()
{
    if (flog) { close_file(flog); flog = NULL; }
    if (get_file_size(log_fname) > 0) {
	uint32_t cnt = 0;
	flog = open_file(log_fname, RDONLY);
	if (flog) {
		char *stx = (char *)calloc(1, max_inbuf);
		if (stx) {
			while (fgets(stx, max_inbuf-1, flog)) {
				cnt++;
				if (cnt <= 16) printf(stx);
				memset(stx, 0, max_inbuf);
			}
			free(stx);
			ets_printf("[%s] %s: total_records=%d file_size=%d\n", TAGFAT, __func__, cnt, (int)ftell(flog));
		}
	}
    }
    if (flog) { close_file(flog); flog = NULL; }
}
//--------------------------------------------------------------------------------------------------
int read_rec_log(char *uk, uint32_t *pnum)
{
int ret = -1;

    if (hdr_log.rseek == hdr_log.wseek) return ret;

    if (!flog) flog = open_file(log_fname, RDONLY);
    if (!flog) {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGFAT,"%s: Error open %s file", __func__, log_fname);
	#endif
	return ret;
    }

    char *buf = (char *)calloc(1, max_inbuf);
    if (buf) {
	char *uki = NULL;
	if (read_param(PARAM_HDR_LOG, (void *)&hdr_log, sizeof(s_log_t)) == ESP_OK) {
		if (!fsetpos(flog, &hdr_log.rseek)) {
			if (fgets(buf, max_inbuf - 1, flog)) {
				hdr_log.rseek += strlen(buf);
				if (hdr_log.total > 0) hdr_log.total--;
				if (max_inbuf > (strlen(buf) + 16)) {
					uki = strstr(buf,"\"Pack\":");
					if (uki) {
						*(uki + 7) = '\0';
						uint32_t pn = *pnum; pn++; *pnum = pn;
						sprintf(buf+strlen(buf),"%u}", pn);
					}
				}
				memcpy(uk, buf, max_inbuf);
				ret = 0;
			}
		} else {
		    #ifdef SET_ERROR_PRINT
			ESP_LOGE(TAGFAT,"%s: Error while set pos to %u in %s file", __func__, (int)hdr_log.rseek, log_fname);
		    #endif
		}
		save_param(PARAM_HDR_LOG, (void *)&hdr_log, sizeof(s_log_t));
	}
	free(buf);
    }
    if (flog) { close_file(flog); flog = NULL; }

    #ifdef SET_ERROR_PRINT
	ets_printf("%s[%s] %s: total=%u rseek=%u wseek=%u%s\n", GREEN_COLOR, TAGFAT, __func__, hdr_log.total, (int)hdr_log.rseek, (int)hdr_log.wseek, STOP_COLOR);
    #endif

    return ret;
}
//--------------------------------------------------------------------------------------------------
int save_rec_log(char *uk, int len)
{
int ret = 0;

    if (!len) return ret;

    if (!flog) flog = open_file(log_fname, WRPLUS);//WRTAIL);
    if (!flog) {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGFAT,"%s: Error open %s file", __func__, log_fname);
	#endif
	return ret;
    }

    char *buf = (char *)calloc(1, len + 3);
    if (buf) {
	memcpy(buf, uk, len);
	if (!strstr(buf, "\r\n")) strcat(buf,"\r\n");
	if (read_param(PARAM_HDR_LOG, (void *)&hdr_log, sizeof(s_log_t)) == ESP_OK) {
		if (hdr_log.wseek + strlen(buf) >= max_log_file_size) {
			hdr_log.wseek = 0;
			hdr_log.total = 0;
		}
		if (!fsetpos(flog, &hdr_log.wseek)) {
			ret = fputs(buf, flog);
			hdr_log.wseek = ftell(flog);
			hdr_log.total++;
			fflush(flog);
			#ifdef SET_ERROR_PRINT
			    ets_printf("%s[%s] %s: total=%u rseek=%u wseek=%u%s\n", GREEN_COLOR, TAGFAT, __func__, hdr_log.total, (int)hdr_log.rseek, (int)hdr_log.wseek, STOP_COLOR);
			#endif
		}
		save_param(PARAM_HDR_LOG, (void *)&hdr_log, sizeof(s_log_t));
	}
	free(buf);
    }

    if (flog) { close_file(flog); flog = NULL; }

    return ret;
}
//--------------------------------------------------------------------------------------------------
bool set_leds(s_led_cmd *val)
{

    rgbVal *pix = malloc(sizeof(rgbVal) * val->size);

    if (!pix) return false;

    rgbVal color = makeRGBVal(0, 0, 0);
    for (uint8_t i = 0; i < val->size; i++) {
        color.r = val->rgb.r;
        color.g = val->rgb.g;
        color.b = val->rgb.b;
        pix[i] = color;
    }

    ws2812_setColors(val->size, pix);

    free(pix);

    return true;
}
//--------------------------------------------------------------------------------------------------
void leds_task(void *arg)
{

total_task++;

s_led_cmd cdata;
bool rt, need_save = false;
uint8_t algo = 0, snd = 0;

const uint8_t anim_step = 10;
const uint8_t anim_max = 150;
uint8_t size = *(uint8_t *)arg;
rgbVal color, color2;
uint8_t step = 0, step2 = 0, shift = 0, c_max = anim_max, c_min = 0;
rgbVal *pixels = NULL;


    ets_printf("%s[%s] Start leds_task (total leds %u) | FreeMem %u%s\n", START_COLOR, TAGL, size, xPortGetFreeHeapSize(), STOP_COLOR);

    while (!restart_flag) {

	if (xQueueReceive(led_cmd_queue, &cdata, (TickType_t)0) == pdTRUE) {
		algo = cdata.mode;
		size = cdata.size;
		switch (algo) {
			case 1://rainbow on
				color = makeRGBVal(anim_max, 0, 0);
				color2 = makeRGBVal(anim_max, 0, 0);
				step = step2 = 0;
				if (!pixels) pixels = malloc(sizeof(rgbVal) * size);
				snd = 1;
			break;
/*
				case 2://rainbow, line, white - off
				if (pixels) { free(pixels); pixels=NULL; }
				snd=1;
				break;
*/
			case 3://line up
			case 4://line down
			case 5://white up
			case 6://white down
				if (algo == 3) {
					shift = 0;
					c_min = 0; c_max = anim_max;
				} else if (algo == 4) {
					shift = size - 1;
					c_min = 0; c_max = anim_max;
				} else if (algo == 5) {
					shift = 0;
					c_min = c_max = anim_max;
				} else if (algo == 6) {
					shift = size - 1;
					c_min = c_max = anim_max;
				}
				step = 0;
				color = makeRGBVal(0, 0, 0);//r,g,b
				color2 = makeRGBVal(0, 0, 0);
				if (!pixels) pixels = malloc(sizeof(rgbVal) * size);
				snd = 1;
			break;
			default :
			{
				if (algo == 0) {
					if (pixels) { free(pixels); pixels=NULL; }
				}
				need_save = false;
				if (xSemaphoreTake(status_mutex, /*50/portTICK_RATE_MS*/portMAX_DELAY) == pdTRUE) {
					if (led_cmd.mode != cdata.mode)  need_save = true;
					else {
						if (cdata.mode == RGB_NONE) {
							if (led_cmd.rgb.r != cdata.rgb.r) need_save = true;
							else if (led_cmd.rgb.g != cdata.rgb.g) need_save = true;
							else if (led_cmd.rgb.b != cdata.rgb.b) need_save = true;
							//else if (led_cmd.size  != cdata.size)  need_save = true;
						}
					}
					if (need_save) memcpy((uint8_t *)&led_cmd, (uint8_t *)&cdata, size_led_cmd);
					xSemaphoreGive(status_mutex);
				}
				if (set_leds(&cdata)) {
					if (need_save) {
						rt = check_pin(GPIO_RGB_WR_PIN);//pin9=sd2 //#define GPIO_RGB_WR_PIN  9
						if (!rt) {
							ets_printf("%s[%s] === CHECK_RGB_WRITE_PIN %d LEVEL IS %d ===s\n", GREEN_COLOR, TAGL, GPIO_RGB_WR_PIN, rt, STOP_COLOR);
							if (save_param(PARAM_RGB_NAME, (void *)&cdata, size_led_cmd) != ESP_OK) {
								#ifdef SET_ERROR_PRINT
									ESP_LOGE(TAGN, "save_rgb: error ! | FreeMem %u", xPortGetFreeHeapSize());
								#endif
							}
						}
					}
					snd = 1;
				} else {
					ESP_LOGE(TAGL, "set_leds: calloc error | FreeMem %u", xPortGetFreeHeapSize());
				}
			}
		}//switch(algo
		if (snd) {
#ifdef SET_MQTT
			if (xQueueSend(mqtt_evt_queue, (void *)&snd, (TickType_t)0) != pdTRUE) {
				#ifdef SET_ERROR_PRINT
					ESP_LOGE(TAGL, "Error while sending to mqtt_evt_queue. | FreeMem %u", xPortGetFreeHeapSize());
				#endif
			}
#endif
			snd = 0;
		}
	}//if (xQueueReceive(led_cmd_queue, &cdata, (TickType_t)0) == pdTRUE)

	switch (algo) {//rainbow on
		case 1:
			if (pixels) {
				color = color2;
				step = step2;
				for (uint8_t i = 0; i < size; i++) {
					pixels[i] = color;
					if (i == 1) {
						color2 = color;
						step2 = step;
					}
					switch (step) {
						case 0:
							color.g += anim_step;
							if (color.g >= anim_max) step++;
						break;
						case 1:
							color.r -= anim_step;
							if (color.r == 0) step++;
						break;
						case 2:
							color.b += anim_step;
							if (color.b >= anim_max) step++;
						break;
						case 3:
							color.g -= anim_step;
							if (color.g == 0) step++;
						break;
						case 4:
							color.r += anim_step;
							if (color.r >= anim_max) step++;
						break;
						case 5:
							color.b -= anim_step;
							if (color.b == 0) step = 0;
						break;
					}
				}
				ws2812_setColors(size, pixels);
				vTaskDelay(25 / portTICK_RATE_MS);
			}
		break;
/*
		case 2://rainbow, line, white - off
			algo=0;//none
			memset((uint8_t *)&cdata, 0, size_led_cmd);
			cdata.size = size; //cdata->mode = 0;
			if (xQueueSend(led_cmd_queue, (void *)&cdata, (TickType_t)0) != pdTRUE) {
			    #ifdef SET_ERROR_PRINT
				ESP_LOGE(TAGL, "Error while sending to led_cmd_queue.");
			    #endif
			}
			//vTaskDelay(25 / portTICK_RATE_MS);
		break;
*/
		case 3: case 4: case 5: case 6://line up, line down, white up, white down
			if (pixels) {
				for (uint8_t i = 0; i < size; i++) {
					switch (step) {
						case 0:
							color.r = c_max;
							color.g = color.b = c_min;
							step++;
						break;
						case 1:
							color.g = c_max;
							color.r = color.b = c_min;
							step++;
						break;
						case 2:
							color.b = c_max;
							color.r = color.g = c_min;
							step++;
						break;
						case 3:
							color.r = color.g = c_max;
							color.b = c_min;
							step++;
						break;
						case 4:
							color.r = color.b = c_max;
							color.g = c_min;
							step++;
						break;
						case 5:
							color.g = color.b =  c_max;
							color.r = c_min;
							step=0;
						break;
					}
					if (i == shift) pixels[i] = color; else pixels[i] = color2;
				}
				ws2812_setColors(size, pixels);
				if (algo&1) {//3, 5
					shift++; if (shift >= size) shift = 0;
				} else {
					if (shift > 0) shift--; else shift = size - 1;
				}
				vTaskDelay(25 / portTICK_RATE_MS);
			}
		break;
	}//switch(algo)


    }


    if (total_task) total_task--;
    ets_printf("%s[%s] Stop leds_task | FreeMem %u%s\n", START_COLOR, TAGL, xPortGetFreeHeapSize(), STOP_COLOR);

    vTaskDelete(NULL);
}
//--------------------------------------------------------------------------------------------------
void app_main()
{

    total_task = 0;
    esp_err_t err;
    bool rt, rt0;

    vTaskDelay(2000 / portTICK_RATE_MS);

    ets_printf("\nAppication version %s | SDK Version %s | FreeMem %u\n", Version, esp_get_idf_version(), xPortGetFreeHeapSize());

//    err = nvs_flash_init_partition(STORAGE_NAMESPACE);
    err = nvs_flash_init();
    if (err != ESP_OK) {
	ESP_LOGE(TAGN, "1: nvs_flash_init() ERROR (0x%x) !!!", err);
	nvs_flash_erase();
//	nvs_flash_erase_partition(STORAGE_NAMESPACE);
	err = nvs_flash_init();
//	err = nvs_flash_init_partition(STORAGE_NAMESPACE);
	if (err != ESP_OK) {
	    ESP_LOGE(TAGN, "2: nvs_flash_init() ERROR (0x%x) !!!", err);
	    while (1);
	}
    }

    vTaskDelay(1000 / portTICK_RATE_MS);

    macs = (uint8_t *)calloc(1, 6);
    if (macs) {
        esp_efuse_mac_get_default(macs);
        sprintf(sta_mac, MACSTR, MAC2STR(macs));
        memcpy(&cli_id, &macs[2], 4);
        cli_id = ntohl(cli_id);
    }


    //--------------------------------------------------------

    esp_log_level_set("wifi", ESP_LOG_WARN);

    //--------------------------------------------------------
    memset((uint8_t *)&led_cmd, 0, size_led_cmd); led_cmd.size = PIXELS;

    rt = check_pin(GPIO_WIFI_PIN);//pin23
    if (!rt)
        ets_printf("%s[%s] === CHECK_WIFI_REWRITE_PIN %d LEVEL IS %d ===%s\n", GREEN_COLOR, TAGT, GPIO_WIFI_PIN, rt, STOP_COLOR);


    err = read_param(PARAM_RGB_NAME, (void *)&led_cmd, size_led_cmd);
    if ((err != ESP_OK) || (!rt)) {
        memset((uint8_t *)&led_cmd, 0, size_led_cmd); led_cmd.size = PIXELS;
        if (save_param(PARAM_RGB_NAME, (void *)&led_cmd, size_led_cmd) == ESP_OK) {
                read_param(PARAM_RGB_NAME, (void *)&led_cmd, size_led_cmd);
        }
    }
    uint8_t l_size = led_cmd.size;
    status_mutex = xSemaphoreCreateMutex();
    led_cmd_queue = xQueueCreate(10, size_led_cmd);


    if (ws2812_init(LED_PIN) != ESP_OK) {
        ESP_LOGE(TAGL, "+++   RMT_INIT FAILURE   +++");
    } else {
        if (!set_leds(&led_cmd)) {
                ESP_LOGE(TAGL, "+++   SET_LEDS FALSE  +++");
        }
    }

    //--------------------------------------------------------
    //    log_mutex = xSemaphoreCreateMutex();
    //--------------------------------------------------------


    //CLI_ID
    err = read_param(PARAM_CLIID_NAME, (void *)&cli_id, sizeof(uint32_t));
    if (err != ESP_OK) save_param(PARAM_CLIID_NAME, (void *)&cli_id, sizeof(uint32_t));
    //MS_MODE
    err = read_param(PARAM_MSMODE_NAME, (void *)&ms_mode, sizeof(uint8_t));
    if ((err != ESP_OK) || (!rt)) {
	ms_mode = DEV_MASTER;//DEVICE MASTER MODE
	//    else ms_mode = 1;//DEVICE SLAVE MODE
	save_param(PARAM_MSMODE_NAME, (void *)&ms_mode, sizeof(uint8_t));
    }
    if (ms_mode == DEV_MASTER)
	ets_printf("%s[%s] DEVICE_ID='%08X' DEVICE_MODE(%u)='MASTER'%s\n", GREEN_COLOR, TAGT, cli_id, ms_mode, WHITE_COLOR);
    else
	ets_printf("%s[%s] DEVICE_ID='%08X' DEVICE_MODE(%u)='SLAVE'%s\n", GREEN_COLOR, TAGT, cli_id, ms_mode, WHITE_COLOR);

    //SNTP + TIME_ZONE
    memset(work_sntp, 0, sntp_srv_len);
    err = read_param(PARAM_SNTP_NAME, (void *)work_sntp, sntp_srv_len);
    if (err != ESP_OK) {
        memset(work_sntp, 0, sntp_srv_len);
        memcpy(work_sntp, sntp_server_def, strlen((char *)sntp_server_def));
        save_param(PARAM_SNTP_NAME, (void *)work_sntp, sntp_srv_len);
    }
    memset(time_zone, 0, sntp_srv_len);
    err = read_param(PARAM_TZONE_NAME, (void *)time_zone, sntp_srv_len);
    if (err != ESP_OK) {
        memset(time_zone, 0, sntp_srv_len);
        memcpy(time_zone, sntp_tzone_def, strlen((char *)sntp_tzone_def));
        save_param(PARAM_TZONE_NAME, (void *)time_zone, sntp_srv_len);
    }
    ets_printf("[%s] SNTP_SERVER '%s' TIME_ZONE '%s'\n", TAGT, work_sntp, time_zone);

    //WIFI
    //   MODE
    rt0 = check_pin(GPIO_WMODE_PIN);//pin22
    if (!rt0) {
        ets_printf("%s[%s] === CHECK_WIFI_MODE_PIN %d LEVEL IS %d ===%s\n", GREEN_COLOR, TAGT, GPIO_WMODE_PIN, rt0, STOP_COLOR);
    }
    uint8_t wtmp;
    err = read_param(PARAM_WMODE_NAME, (void *)&wtmp, sizeof(uint8_t));
    if (rt0) {//set wifi_mode from flash
        if (err == ESP_OK) {
                wmode = wtmp;
        } else {
                wmode = WIFI_MODE_AP;
                save_param(PARAM_WMODE_NAME, (void *)&wmode, sizeof(uint8_t));
        }
    } else {//set AP wifi_mode
        wmode = WIFI_MODE_AP;
        if (err == ESP_OK) {
                if (wtmp != wmode) save_param(PARAM_WMODE_NAME, (void *)&wmode, sizeof(uint8_t));
        } else {
                save_param(PARAM_WMODE_NAME, (void *)&wmode, sizeof(uint8_t));
        }
    }
/* Set STA mode !!! */
wmode = WIFI_MODE_STA; save_param(PARAM_WMODE_NAME, (void *)&wmode, sizeof(uint8_t));
/**/
    ets_printf("[%s] WIFI_MODE (%d): %s\n", TAGT, wmode, wmode_name[wmode]);


#ifdef UDP_SEND_BCAST
    if (wmode == WIFI_MODE_STA) udp_flag = 1;
#endif

    //   SSID
    memset(work_ssid,0,wifi_param_len);
    err = read_param(PARAM_SSID_NAME, (void *)work_ssid, wifi_param_len);
    if ((err != ESP_OK) || (!rt)) {
        memset(work_ssid,0,wifi_param_len);
        memcpy(work_ssid, EXAMPLE_WIFI_SSID, strlen(EXAMPLE_WIFI_SSID));
        save_param(PARAM_SSID_NAME, (void *)work_ssid, wifi_param_len);
    }
    //   KEY
    memset(work_pass,0,wifi_param_len);
    err = read_param(PARAM_KEY_NAME, (void *)work_pass, wifi_param_len);
    if ((err != ESP_OK) || (!rt)) {
        memset(work_pass,0,wifi_param_len);
        memcpy(work_pass, EXAMPLE_WIFI_PASS, strlen(EXAMPLE_WIFI_PASS));
        save_param(PARAM_KEY_NAME, (void *)work_pass, wifi_param_len);
    }
    ets_printf("[%s] WIFI_STA_PARAM: '%s:%s'\n", TAGT, work_ssid, work_pass);

    wifi_param_ready = 1;

#ifdef SET_MQTT
    //MQTT
    memset(broker_host, 0, broker_name_len);
    err = read_param(PARAM_BROKERIP_NAME, (void *)broker_host, broker_name_len);
    if ((err != ESP_OK) || (!rt)) {
        memset(broker_host, 0, broker_name_len);
        strcpy(broker_host, AWS_IOT_MQTT_HOST);
        save_param(PARAM_BROKERIP_NAME, (void *)broker_host, broker_name_len);
    }
    err = read_param(PARAM_BROKERPORT_NAME, (void *)&broker_port, sizeof(uint32_t));
    if ((err != ESP_OK) || (!rt)) {
        broker_port = AWS_IOT_MQTT_PORT;
        save_param(PARAM_BROKERPORT_NAME, (void *)&broker_port, sizeof(uint32_t));
    }
    ets_printf("[%s] MQTT_BROKER: '%s:%u'\n", TAGT, broker_host, broker_port);

    //MQTT_USER_NAME
    memset(mqtt_user_name, 0, mqtt_user_name_len);
    err = read_param(PARAM_MQTT_USER_NAME, (void *)mqtt_user_name, mqtt_user_name_len);
    if ((err != ESP_OK) || (!rt)) {
        memset(mqtt_user_name, 0, mqtt_user_name_len);
        strcpy(mqtt_user_name, CONFIG_AWS_EXAMPLE_CLIENT_ID);
        save_param(PARAM_MQTT_USER_NAME, (void *)mqtt_user_name, mqtt_user_name_len);
    }
    //MQTT_USER_PASS
    memset(mqtt_user_pass, 0, mqtt_user_name_len);
    err = read_param(PARAM_MQTT_USER_PASS, (void *)mqtt_user_pass, mqtt_user_name_len);
    if ((err != ESP_OK) || (!rt)) {
        memset(mqtt_user_pass, 0, mqtt_user_name_len);
        strcpy(mqtt_user_pass, CONFIG_AWS_EXAMPLE_CLIENT_ID);
        save_param(PARAM_MQTT_USER_PASS, (void *)mqtt_user_pass, mqtt_user_name_len);
    }
    ets_printf("[%s] MQTT_USER: '%s:%s'\n", TAGT, mqtt_user_name, mqtt_user_pass);
#endif

#ifdef SET_OTA
    memset(ota_srv, 0, ota_param_len);
    err = read_param(PARAM_OTA_IP_NAME, (void *)ota_srv, ota_param_len);
    if ((err != ESP_OK) || (!rt)) {
        memset(ota_srv, 0, ota_param_len);
        strcpy(ota_srv, OTA_SERVER_IP);
        save_param(PARAM_OTA_IP_NAME, (void *)ota_srv, ota_param_len);
    }
    err = read_param(PARAM_OTA_PORT_NAME, (void *)&ota_srv_port, sizeof(uint32_t));
    if ((err != ESP_OK) || (!rt)) {
        ota_srv_port = OTA_SERVER_PORT;
        save_param(PARAM_OTA_PORT_NAME, (void *)&ota_srv_port, sizeof(uint32_t));
    }
    memset(ota_filename, 0, ota_param_len);
    err = read_param(PARAM_OTA_FILE_NAME, (void *)ota_filename, ota_param_len);
    if ((err != ESP_OK) || (!rt)) {
        memset(ota_filename, 0, ota_param_len);
        strcpy(ota_filename, OTA_FILENAME);
        save_param(PARAM_OTA_FILE_NAME, (void *)ota_filename, ota_param_len);
    }
    ets_printf("[%s] OTA_URL: '%s:%u%s'\n", TAGT, ota_srv, ota_srv_port, ota_filename);
#endif

#ifdef SET_TLS_SRV
    err = read_param(PARAM_TLS_PORT, (void *)&tls_port, sizeof(uint16_t));
    if ((err != ESP_OK) || (!rt)) {
        tls_port = TLS_PORT_DEF;
        save_param(PARAM_TLS_PORT, (void *)&tls_port, sizeof(uint16_t));
    }
    ets_printf("[%s] TLS_PORT: %u\n", TAGT, tls_port);
#endif


    radio_mutex = xSemaphoreCreateMutex();

    init_radio();

    fs_ok = mount_disk();

    if (fs_ok == ESP_OK) {

	int kols = 2;


	//***************   make radio file   ******************************************
	//                  kols = make_radio_file(kols);
	//******************************************************************************


	char *line = (char *)calloc(1, 128);

	int rdl = get_file_size(radio_fname);
	if (rdl > 0) {

		FILE *fradio = open_file(radio_fname, RDBYTE);
		if (fradio != NULL) {
			//int sz = sizeof(s_radio_one);
			s_radio_one nrec;
			s_radio *ukradio=NULL;//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			kols = rdl / size_radio_one;
			ets_printf("[%s] File %s present, file_size=%d total_radio=%d\n", TAGFAT, radio_fname, rdl, kols);
			for (int j = 0; j < kols; j++) {
				memset((uint8_t *)&nrec, 0, size_radio_one);
				rdl = bin_file_read(fradio, (uint8_t *)&nrec, size_radio_one);
				if (rdl != size_radio_one) break;
				else {
					ukradio = add_radio((void *)&nrec);
/**/
					if (ukradio) {
						memset(line, 0, 128);
						sprintf(line,"[%p] DevID=%08X", ukradio, nrec.dev_id);
						sprintf(line+strlen(line),"; color[%u]:%u,%u,%u (%u)",
									nrec.color.size,
									nrec.color.rgb.r,
									nrec.color.rgb.g,
									nrec.color.rgb.b,
									nrec.color.mode);
						printf("[%u] %s\n", j, line);
					}
/**///!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				}
			}
			close_file(fradio);
		}

	}

	//******************************************************************************
	//
	//	flog = open_file(log_fname, RDBYTE);
	//
	//******************************************************************************

	memset((uint8_t *)&hdr_log, 0, sizeof(s_log_t));
	if (!check_pin(GPIO_DEL_LOG_PIN)) {//pin10=sd3
		ets_printf("%s[%s] === CHECK_DEL_LOG_PIN %d LEVEL IS Low ===%s\n", GREEN_COLOR, TAGT, GPIO_DEL_LOG_PIN, STOP_COLOR);
		if (flog) { close_file(flog); flog=NULL; }
		rt = delete_file(log_fname);
		if (rt) {
			ets_printf("%s[%s] Error delete %s file (%d)%s\n", GREEN_COLOR, TAGT, log_fname, rt, STOP_COLOR);
		} else {
			save_param(PARAM_HDR_LOG, (void *)&hdr_log, sizeof(s_log_t));
		}
	} else {
		err = read_param(PARAM_HDR_LOG, (void *)&hdr_log, sizeof(s_log_t));
		if (err != ESP_OK) {
			memset((uint8_t *)&hdr_log, 0, sizeof(s_log_t));
			save_param(PARAM_HDR_LOG, (void *)&hdr_log, sizeof(s_log_t));
		}

		if (hdr_log.rseek == hdr_log.wseek) {
			if ((get_file_size(log_fname) + 512) > MAX_LOG_FILE_SIZE) {
				memset((uint8_t *)&hdr_log, 0, sizeof(s_log_t));
				if (save_param(PARAM_HDR_LOG, (void *)&hdr_log, sizeof(s_log_t)) == ESP_OK) {
					if (flog) { close_file(flog); flog = NULL; }
					delete_file(log_fname);
				}
			}
		}
	}
	ets_printf("[%s] HDR_LOG: [%u] %d:%d\n", TAGT, hdr_log.total, (int)hdr_log.rseek, (int)hdr_log.wseek);

	int lenf = get_file_size(radio_fname);
	memset(line, 0, 128);
	sprintf(line, "%s/", base_path);
	ets_printf("[%s] Open DIR '%s':\n", TAGFAT, line);
	struct dirent *de = NULL;
	DIR *dir = opendir(line);
	if (dir) {
		//rewinddir(dir);
		uint32_t kol = 0;
		while ( (de = readdir(dir)) != NULL ) {
			lenf = get_file_size(de->d_name);
			printf("\tfile: type=%d name='%s' size=%d\n", de->d_type, de->d_name, lenf);
			seekdir(dir, ++kol);
			if ((!strcasecmp(de->d_name, log_fname)) && (lenf>0)) print_log_file();
		}
	} else {
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAGFAT, "Open DIR '%s' Error | FreeMem %u", line, xPortGetFreeHeapSize());
	    #endif
	}

	if (line) free(line);

	//	umount_disk(fs_ok);

    }

    //    print_radio_file();// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


#ifdef SET_OTA
    const esp_partition_t *run = esp_ota_get_running_partition();
    if (run) {
        ets_printf("[%s] RUN from PARTITION: label=%s type=%d subtype=%d addr=0x%08x\n" , TAGT, run->label, run->type, run->subtype, run->address);
    }
#endif


//ADC_ATTEN_0db   = 0,  /*!<The input voltage of ADC will be reduced to about 1/1 */
//ADC_ATTEN_2_5db = 1,  /*!<The input voltage of ADC will be reduced to about 1/1.34 */
//ADC_ATTEN_6db   = 2,  /*!<The input voltage of ADC will be reduced to about 1/2 */
//ADC_ATTEN_11db  = 3,  /*!<The input voltage of ADC will be reduced to about 1/3.6*/
    //adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_TEST_CHANNEL, ADC_ATTEN_11db);



    if (ms_mode == DEV_MASTER) {

	initialise_wifi(wmode);


#ifdef SET_MQTT
	mqtt_evt_queue = xQueueCreate(10, sizeof(int));
	if (!mqtt_evt_queue) {
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAG, "mqtt_evt_queue: QueueCreate Error | FreeMem %u", xPortGetFreeHeapSize());
	    #endif
	}
	mqtt_net_queue = xQueueCreate(8, sizeof(int));
	if (!mqtt_net_queue) {
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAG, "mqtt_net_queue: QueueCreate Error | FreeMem %u", xPortGetFreeHeapSize());
	    #endif
	}
#endif

    }

    radio_evt_queue = xQueueCreate(10, size_radio_stat);
    if (!radio_evt_queue) {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGR, "radio_evt_queue: QueueCreate Error | FreeMem %u", xPortGetFreeHeapSize());
	#endif
    }
    radio_cmd_queue = xQueueCreate(10, size_radio_cmd);
    if (!radio_cmd_queue) {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGR, "radio_cmd_queue: QueueCreate Error | FreeMem %u", xPortGetFreeHeapSize());
	#endif
    }




//******************************************************************************************************
//************************            Start leds control thread             ****************************
//************************                                                  **************************** ( 5 | portPRIVILEGE_BIT )

    if (xTaskCreatePinnedToCore(&leds_task, "leds_task", STACK_SIZE_2K, &l_size, 6, NULL, 1) != pdPASS) {//5//6//5//4 //5
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGL, "Create leds_task failed | FreeMem %u", xPortGetFreeHeapSize());
	#endif
    }
    vTaskDelay(1000 / portTICK_RATE_MS);

//******************************************************************************************************
//******************************************************************************************************




#ifdef SET_SNTP
    if (ms_mode == DEV_MASTER) {
	if (wmode&1) {// WIFI_MODE_STA) || WIFI_MODE_APSTA
	    if (xTaskCreatePinnedToCore(&sntp_task, "sntp_task", STACK_SIZE_2K, work_sntp, 5, NULL, 0) != pdPASS) {//7,NULL,1
		#ifdef SET_ERROR_PRINT
		    ESP_LOGE(TAGS, "Create sntp_task failed | FreeMem %u", xPortGetFreeHeapSize());
		#endif
	    }
	    vTaskDelay(1000 / portTICK_RATE_MS);
	}
    }
#endif



    if (xTaskCreatePinnedToCore(&radio_task, "radio_task", STACK_SIZE_2K5, NULL, 9, NULL, 1) != pdPASS) {//10//6//10//16 //7  core=0
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGR, "Create radio_task failed | FreeMem %u", xPortGetFreeHeapSize());
	#endif
    }
    vTaskDelay(1000 / portTICK_RATE_MS);



#ifdef SET_I2C
    uint32_t tst = STACK_SIZE_2K;
    #if defined(SET_BMP) || defined(SET_BH1750) || defined(SET_SI7021)
	sensors_mutex = xSemaphoreCreateMutex();
    #endif

    #ifdef SET_BMP
	memset(&calib,0,sizeof(bmx280_calib_t));
    #endif

    #ifdef SET_APDS
	gpio_config_t itr_conf = {
		.intr_type    = GPIO_PIN_INTR_NEGEDGE,//GPIO_PIN_INTR_ANYEGDE,//GPIO_PIN_INTR_LOLEVEL,////interrupt by back front
		.pull_up_en   = 1,
		.pull_down_en = 0,
		.pin_bit_mask = GPIO_INPUT_PIN_SEL_I2C,//bit mask of the pins
		.mode         = GPIO_MODE_INPUT,
	};
	gpio_config(&itr_conf);
	gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);//DEFAULT);//install gpio isr service
	gpio_isr_handler_add(GPIO_INPUT_I2C, apds_isr_handler, NULL);//hook isr handler for specific gpio pin
	tst += STACK_SIZE_2K;
    #endif

    #if defined(SET_BMP) || defined(SET_BH1750) || defined(SET_SI7021) || defined(SET_APDS)
	i2c_master_init();
	if (xTaskCreatePinnedToCore(&i2c_task, "i2c_task", tst, NULL, 6, NULL, 0) != pdPASS) {//8//6//7//8
	    #ifdef SET_ERROR_PRINT
			ESP_LOGE(TAGI, "Create i2c_task failed | FreeMem %u", xPortGetFreeHeapSize());
	    #endif
	}
	vTaskDelay(1000 / portTICK_RATE_MS);
    #endif
#endif

    if (ms_mode == DEV_MASTER) {

#ifdef SET_TLS_SRV
	if (xTaskCreatePinnedToCore(&tls_task, "tls_task", 8*STACK_SIZE_1K, &tls_port, 8, NULL, 0) != pdPASS) {//6,NULL,1)
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAGTLS, "Create tls_task failed | FreeMem %u", xPortGetFreeHeapSize());
	    #endif
	}
	vTaskDelay(1000 / portTICK_RATE_MS);
#endif

#ifdef SET_MQTT
	brk.ip = &broker_host[0];
	brk.port = broker_port;
	if (xTaskCreatePinnedToCore(&mqtt_task, "mqtt_task", 8*STACK_SIZE_1K, &brk, 7, NULL, 1) != pdPASS) {//7,NULL,0
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAG, "Create mqtt_task failed | FreeMem %u", xPortGetFreeHeapSize());
	    #endif
	}
	vTaskDelay(1000 / portTICK_RATE_MS);
#endif

    }

#ifdef SET_TOUCH_PAD
    gpio_config_t io_conf = {
	.intr_type    = GPIO_PIN_INTR_POSEDGE,//interrupt of rising edge
	.pull_up_en   = 0,
	.pull_down_en = 1,
	.pin_bit_mask = GPIO_INPUT_PIN_SEL_16,//bit mask of the pins
	.mode         = GPIO_MODE_INPUT,
    };
    gpio_config(&io_conf);

    uint8_t mov_evt = 0, need_off = false;
    mov_evt_queue = xQueueCreate(8, sizeof(uint8_t));//create a queue to handle gpio event from isr
    #ifndef SET_APDS
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);//install gpio isr service
    #endif
    gpio_isr_handler_add(GPIO_INPUT_PIN_16, gpio_isr_hdl, NULL);//(void *)GPIO_INPUT_PIN_16);//hook isr handler for specific gpio pin

    const uint32_t wait_leds_tick = 5000;//30000
    uint32_t tw = get_tmr(wait_leds_tick);
    bool need = false;
    s_led_cmd mcdata;
    if (xSemaphoreTake(status_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy((uint8_t *)&mcdata, (uint8_t *)&led_cmd, size_led_cmd);
        xSemaphoreGive(status_mutex);
    }
#endif


#ifdef SET_SD_CARD
    if (xTaskCreatePinnedToCore(&sdcard_task, "sdcard_task", STACK_SIZE_2K, NULL, 5, NULL, 1) != pdPASS) {
	#ifdef SET_ERROR_PRINT
	    ESP_LOGE(TAGTLS, "Create sdcard_task failed | FreeMem %u", xPortGetFreeHeapSize());
	#endif
    }
    vTaskDelay(1000 / portTICK_RATE_MS);
#endif



    if (ms_mode == DEV_MASTER) {

#ifdef SET_WEB
	web_port = WEB_PORT;
	if (xTaskCreatePinnedToCore(&web_task, "web_task", STACK_SIZE_2K, &web_port, 6, NULL, 1) != pdPASS) {//10//7
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAGWEB, "Create web_task failed | FreeMem %u", xPortGetFreeHeapSize());
	    #endif
	}
	vTaskDelay(1000 / portTICK_RATE_MS);
#endif

#ifdef SET_WS
	ws_port = WS_PORT;
	if (xTaskCreatePinnedToCore(&ws_task, "ws_task", STACK_SIZE_2K5, &ws_port, 6, NULL, 1) != pdPASS) {//8//10//7
	    #ifdef SET_ERROR_PRINT
		ESP_LOGE(TAGWS, "Create ws_task failed | FreeMem %u", xPortGetFreeHeapSize());
	    #endif
	}
	vTaskDelay(1000 / portTICK_RATE_MS);
#endif

    }

#ifdef SET_SERIAL
    serial_init();
    vTaskDelay(500 / portTICK_RATE_MS);
    if (xTaskCreatePinnedToCore(&serial_task, "serial_task", STACK_SIZE_2K, NULL, 7, NULL, 0) != pdPASS) {//7
	#ifdef SET_ERROR_PRINT
		ESP_LOGE(TAG_UART, "Create serial_task failed | FreeMem %u", xPortGetFreeHeapSize());
	#endif
    }
    vTaskDelay(500 / portTICK_RATE_MS);
#endif


#ifdef SET_SSD1306
    i2c_ssd1306_init();

    ssd1306_on(false);
    vTaskDelay(500 / portTICK_RATE_MS);

    esp_err_t ssd_ok = ssd1306_init();
    if (ssd_ok == ESP_OK) ssd1306_pattern();

    t_sens_t tc;
    result_t sfl;
    char stk[128] = {0};
    struct tm *dtimka;
    int tu, tn, di_hour, di_min, di_sec, di_day, di_mon;
    time_t dit_ct;
    uint8_t inv_cnt = 30, clr = 0;
    uint32_t adc_tw = get_tmr(1000);
#endif




  while (!restart_flag) {

#ifdef SET_SSD1306
    if (check_tmr(adc_tw)) {
	if (ssd_ok == ESP_OK) {
	    inv_cnt--;
	    if (!inv_cnt) {
		inv_cnt = 30;
		ssd1306_invert();
	    }
	    if (!clr) {
		ssd1306_clear();
		clr = 1;
	    }
	    dit_ct = time(NULL);
	    dtimka = localtime(&dit_ct);
	    di_hour = dtimka->tm_hour;	di_min = dtimka->tm_min;	di_sec = dtimka->tm_sec;
	    di_day = dtimka->tm_mday;	di_mon = dtimka->tm_mon + 1;
	    memset(stk, 0, 128);
	    sprintf(stk+strlen(stk),"%02d.%02d %02d:%02d:%02d\n", di_day, di_mon, di_hour, di_min, di_sec);
	    tu = strlen(localip);
	    if ((tu > 0) && (tu <= 16)) {
		tn = (16 - tu ) / 2;
		if ((tn > 0) && (tn < 8)) sprintf(stk+strlen(stk),"%*.s",tn," ");
		sprintf(stk+strlen(stk),"%s", localip);
	    }
	    sprintf(stk+strlen(stk),"\n");
	    memset(&sfl, 0, sizeof(result_t));
	    if (xSemaphoreTake(sensors_mutex, 50/portTICK_RATE_MS) == pdTRUE) {
		memcpy((uint8_t *)&sfl, (uint8_t *)&sensors, sizeof(result_t));
		xSemaphoreGive(sensors_mutex);
	    }
	    get_tsensor(&tc);
	    sprintf(stk+strlen(stk),"Chip : %.1fv %d%cC", (double)tc.vcc/1000, (int)round(tc.cels), 0x1F);
	    #ifdef SET_I2C
		#ifdef SET_BH1750
		    sprintf(stk+strlen(stk),"\nLux  : %d lx", (int)round(sfl.lux));
		#endif
		#ifdef SET_BMP
		    sprintf(stk+strlen(stk),"\nTemp : %.2f%cC\nPress: %d mmHg", sfl.temp, 0x1F, (int)round(sfl.pres));
		#endif
		#ifdef SET_SI7021
		    sprintf(stk+strlen(stk),"\nTemp2: %.2f%cC\nHumi2: %.2f%c", sfl.si_temp, 0x1F, sfl.si_humi, 0x25);
		#endif
	    #endif
	    ssd1306_text_xy(stk, 2, 1);
	//    if (!shift) {
	//	shift = ~shift;
	//	ssd1306_shift(true, 2);
	//    }

	    //ssd1306_contrast(contrast);
	    //contrast = ~contrast;
	    //ssd1306_scroll(scroll);
	    //scroll = ~scroll;
//	    ssd1306_text(stk);
//	    ssd1306_text_xy("ESP32", 4, 2);
	}
	adc_tw = get_tmr(1000);
    }
#endif

    if (ms_mode == DEV_MASTER) {

#ifdef SET_SNTP
	if (wmode&1) {// WIFI_MODE_STA || WIFI_MODE_APSTA
	    if (sntp_go) {
		if (!sntp_start) {
		    sntp_go = 0;
		    if (xTaskCreatePinnedToCore(&sntp_task, "sntp_task", STACK_SIZE_2K, work_sntp, 5, NULL, 0)  != pdPASS) {//5//7 core=1
			#ifdef SET_ERROR_PRINT
			    ESP_LOGE(TAGS, "Create sntp_task failed | FreeMem %u", xPortGetFreeHeapSize());
			#endif
		    }
		    vTaskDelay(500 / portTICK_RATE_MS);
		} else vTaskDelay(50 / portTICK_RATE_MS);
	    }
	}
#endif

    }

#ifdef SET_TOUCH_PAD
	if (dopler_flag) {
	    if (xQueueReceive(mov_evt_queue, &mov_evt, 25/portTICK_RATE_MS) == pdTRUE) {
		memset((uint8_t *)&mcdata.rgb, 255, 3); mcdata.mode = RGB_NONE;
		need = false;
		if (xSemaphoreTake(status_mutex, portMAX_DELAY) == pdTRUE) {
			if (led_cmd.mode != mcdata.mode) need = true;
			else if (led_cmd.rgb.r != mcdata.rgb.r) need = true;
			else if (led_cmd.rgb.g != mcdata.rgb.g) need = true;
			else if (led_cmd.rgb.b != mcdata.rgb.b) need = true;
			if (need) memcpy((uint8_t *)&led_cmd, (uint8_t *)&mcdata, size_led_cmd);
			xSemaphoreGive(status_mutex);
		}
		if (need) {
			ets_printf("[%s] GPIO(%d) Set leds ON : intr %u\n", TAGK, GPIO_INPUT_PIN_16, intr_count);
			if (xQueueSend(led_cmd_queue, (void *)&mcdata, (TickType_t)0) != pdPASS) {
				#ifdef SET_ERROR_PRINT
					ESP_LOGE(TAGK, "Error while sending to led_cmd_queue. | FreeMem %u", xPortGetFreeHeapSize());
				#endif
			}
			need_off = true;
		} else {
			ets_printf("[%s] GPIO(%d) Leds allready ON : intr %u\n", TAGK, GPIO_INPUT_PIN_16, intr_count);
		}
		tw = get_tmr(wait_leds_tick);
	    } else {
		if ( (check_tmr(tw)) && need_off ) {
			need_off = false;
			memset((uint8_t *)&mcdata.rgb, 0, 3); mcdata.mode = RGB_NONE;
			need = false;
			if (xSemaphoreTake(status_mutex, portMAX_DELAY) == pdTRUE) {
				if (led_cmd.mode != mcdata.mode) need = true;
				else if (led_cmd.rgb.r != mcdata.rgb.r) need = true;
				else if (led_cmd.rgb.g != mcdata.rgb.g) need = true;
				else if (led_cmd.rgb.b != mcdata.rgb.b) need = true;
				if (need) memcpy((uint8_t *)&led_cmd, (uint8_t *)&mcdata, size_led_cmd);
				xSemaphoreGive(status_mutex);
			}
			if (need) {
				ets_printf("[%s] GPIO(%d) Set leds OFF : timeout %u sec.\n", TAGK, GPIO_INPUT_PIN_16, wait_leds_tick/1000);
				if (xQueueSend(led_cmd_queue, (void *)&mcdata, (TickType_t)0) != pdPASS) {
					#ifdef SET_ERROR_PRINT
						ESP_LOGE(TAGK, "Error while sending to led_cmd_queue. | FreeMem %u", xPortGetFreeHeapSize());
					#endif
				}
			}
			tw = get_tmr(wait_leds_tick);
		}
	    }
	    vTaskDelay(50 / portTICK_RATE_MS);
	}
#endif

    if (ms_mode == DEV_MASTER) {

#ifdef UDP_SEND_BCAST
	if (wmode == WIFI_MODE_STA) {
	    if ((!udp_start) && (udp_flag == 1)) {
		if (xTaskCreatePinnedToCore(&udp_task, "disk_task", STACK_SIZE_1K5, NULL, 5, NULL, 0) != pdPASS) {//5,NULL,1)
			#ifdef SET_ERROR_PRINT
				ESP_LOGE(TAGU, "Create udp_task failed | FreeMem %u", xPortGetFreeHeapSize());
			#endif
		}
		vTaskDelay(500 / portTICK_RATE_MS);
	    }
	}
#endif

#ifdef SET_OTA
	if (ota_flag && !ota_begin) {
	    ota_flag = 0;
	    if (wmode&1) {// WIFI_MODE_STA || WIFI_MODE_APSTA
		if (xTaskCreatePinnedToCore(&ota_task, "ota_task", STACK_SIZE_4K, NULL, 10, NULL, 1) != pdPASS) {//8,NULL,1)
			#ifdef SET_ERROR_PRINT
				ESP_LOGE(TAGO, "Create ota_task failed | FreeMem %u", xPortGetFreeHeapSize());
			#endif
		}
	    }
	}
#endif

    }


  }

  vTaskDelay(1000 / portTICK_RATE_MS);


  uint8_t cnt = 20;
  ets_printf("%s[%s] Waiting for all task closed...%d sec.%s\n", GREEN_COLOR, TAG, cnt, STOP_COLOR);
  while (total_task) {
        cnt--; if (!cnt) break;
        vTaskDelay(1000 / portTICK_RATE_MS);
  }
  ets_printf("%s[%s] (%02d) DONE. Total unclosed task %d%s\n", GREEN_COLOR, TAG, cnt, total_task, STOP_COLOR);

  if (flog) { close_file(flog); flog = NULL; }

  remove_radio();

  if (fs_ok) { umount_disk(fs_ok); fs_ok = ESP_FAIL; }

  if (macs) free(macs);

  ets_printf("%s[%s] Waiting wifi stop...%s\n", GREEN_COLOR, TAG, STOP_COLOR);

  ESP_ERROR_CHECK(esp_wifi_stop());
  ESP_ERROR_CHECK(esp_wifi_deinit());

  //save_param(PARAM_WMODE_NAME, (void *)&wmode, sizeof(uint8_t));


  esp_restart();

}
