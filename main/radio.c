#include "hdr.h"

#include "mqtt.h"
#include "radio.h"

//--------------------------------------------------------------------------------------------------

const char *TAGR = "AIR";
uint8_t radio_start = 0;
s_radio_hdr radio_hdr = {NULL, NULL, 0};
xSemaphoreHandle radio_mutex;

//--------------------------------------------------------------------------------------------------
s_radio *add_radio(void *one)
{
s_radio *ret = NULL, *tmp = NULL;

    if (!one) return ret;

    if (xSemaphoreTake(radio_mutex, portMAX_DELAY) == pdTRUE) {
	s_radio *rec = (s_radio *)calloc(1, sizeof(s_radio));
	if (rec) {
	    rec->before = rec->next = NULL;
	    memcpy((uint8_t *)&rec->one, (uint8_t *)one, size_radio_one);
	    if (!radio_hdr.first) {//first record
		radio_hdr.first = radio_hdr.end = rec;
	    } else {//add to tail
		tmp = radio_hdr.end;
		rec->before = tmp;
		radio_hdr.end = rec;
		tmp->next = rec;
	    }
	    radio_hdr.counter++;
	    ret = rec;
	}
	xSemaphoreGive(radio_mutex);
    }

    return ret;
}
//--------------------------------------------------------------------------------------------------
int del_radio(s_radio *rcd, bool withlock)
{
int ret=-1;
s_radio *bf = NULL, *nx = NULL;

    if (!rcd) return ret;

    if (withlock) xSemaphoreTake(radio_mutex, portMAX_DELAY);

	bf = rcd->before;
	nx = rcd->next;
	if (bf) {
	    if (nx) {
		bf->next = nx;
		nx->before = bf;
	    } else {
		bf->next = NULL;
		radio_hdr.end = bf;
	    }
	} else {
	    if (nx) {
		radio_hdr.first = nx;
		nx->before = NULL;
	    } else {
		radio_hdr.first = NULL;
		radio_hdr.end = NULL;
	    }
	}
	if (radio_hdr.counter>0) radio_hdr.counter--;
	free(rcd); rcd = NULL;
	ret = 0;

    if (withlock) xSemaphoreGive(radio_mutex);

    return ret;
}
//--------------------------------------------------------------------------------------------------
s_radio *find_radio(uint32_t id)
{
s_radio *ret = NULL, *temp = NULL, *tmp = NULL;

    if (xSemaphoreTake(radio_mutex, portMAX_DELAY) == pdTRUE) {
	if (radio_hdr.first) {
		tmp = radio_hdr.first;
		while (tmp) {
			if (tmp->one.dev_id == id) {
				ret = tmp;
				break;
			} else {
				temp = tmp->next;
				tmp = temp;
			}
		}
	}
	xSemaphoreGive(radio_mutex);
    }

    return ret;
}
//--------------------------------------------------------------------------------------------------
int isMaster(s_radio *rcd)
{
int ret = -1;

    if (!rcd) return ret;

    if (xSemaphoreTake(radio_mutex, portMAX_DELAY) == pdTRUE) {
        if (radio_hdr.first) {
                if (rcd == radio_hdr.first) ret = 1; else ret = 0;
        }
        xSemaphoreGive(radio_mutex);
    }

    return ret;
}
//------------------------------------------------------------------------
void remove_radio()
{
    if (xSemaphoreTake(radio_mutex, portMAX_DELAY) == pdTRUE) {
        while (radio_hdr.first) del_radio(radio_hdr.first, 0);
        xSemaphoreGive(radio_mutex);
    }
}
//------------------------------------------------------------------------
void init_radio()
{
    if (xSemaphoreTake(radio_mutex, portMAX_DELAY) == pdTRUE) {
        radio_hdr.first = radio_hdr.end = NULL;
        radio_hdr.counter = 0;
        xSemaphoreGive(radio_mutex);
    }
}
//------------------------------------------------------------------------
int total_radio()
{
int ret = 0;

    if (xSemaphoreTake(radio_mutex, portMAX_DELAY) == pdTRUE) {
        ret = radio_hdr.counter;
        xSemaphoreGive(radio_mutex);
    }

    return ret;
}
//--------------------------------------------------------------------------------------------------
s_radio *get_radio(s_radio *last)
{
s_radio *ret = NULL;

    if (xSemaphoreTake(radio_mutex, portMAX_DELAY) == pdTRUE) {
        if (!last) ret = radio_hdr.first;
              else ret = last->next;
        xSemaphoreGive(radio_mutex);
    }

    return ret;
}
//--------------------------------------------------------------------------------------------------
s_radio_one *get_radio_from_list(void *src)//if (ret!=NULL) you must free(ret)
{
s_radio_one *ret = NULL;

    if (src) {
        ret = (s_radio_one *)calloc(1, size_radio_one);
        if (ret) {
            if (xSemaphoreTake(radio_mutex, portMAX_DELAY) == pdTRUE) {
                memcpy((uint8_t *)ret, (uint8_t *)src, size_radio_one);
                xSemaphoreGive(radio_mutex);
            }
        }
    }

    return ret;
}
//--------------------------------------------------------------------------------------------------
#ifdef PRN_SET
void print_radio_stat(s_radio_stat *rstat)
{

    if (rstat)
	ets_printf("%s[%s] s_radio_stat(%d):\n\ts_radio_cmd(%d):\n\ts_radio_one(%d):\n\t0x%08X\n\ts_led_cmd(%d):\n\trgb[%u.%u]:[%u,%u,%u]\n\tflag:%d\n\ttime:%u, freemem:%u, temp=%.2f pwr=%u %s\n", GREEN_COLOR, TAR,
		size_radio_stat,
		size_radio_cmd,
		size_radio_one,
		(uint32_t)rstat->cmd.one.dev_id,
		size_led_cmd,
		rstat->cmd.one.color.mode, rstat->cmd.one.color.size, rstat->cmd.one.color.rgb.r, rstat->cmd.one.color.rgb.g, rstat->cmd.one.color.rgb.b,
		rstat->cmd.flag,
		(uint32_t)rstat->time,
		rstat->mem,
		rstat->temp,
		rstat->vcc,
		STOP_COLOR);

}
#endif
//--------------------------------------------------------------------------------------------------
void update_radio_color(s_radio *adr, s_led_cmd *newc)
{
    if ((adr) && (newc)) {
        if (xSemaphoreTake(radio_mutex, portMAX_DELAY) == pdTRUE) {
            memcpy((uint8_t *)&adr->one.color, (uint8_t *)newc, size_led_cmd);
            xSemaphoreGive(radio_mutex);
        }
    }
}
//--------------------------------------------------------------------------------------------------
bool send_to_slave(s_radio_cmd *cmd)
{
bool ret = false;

    //-------------------------------
    ret = true;
    //-------------------------------

    return ret;
}
//--------------------------------------------------------------------------------------------------
bool recv_from_slave(s_radio_stat *answer)
{
bool ret = false;

    //-------------------------------
    ret = true;
    //-------------------------------

    return ret;
}
//--------------------------------------------------------------------------------------------------
void do_slave(s_radio_cmd *cmd, s_radio_stat *answer)
{

    if (!send_to_slave(cmd)) cmd->flag = -1;
    if (!recv_from_slave(answer)) cmd->flag = -1;

    memcpy((uint8_t *)answer, (uint8_t *)cmd, size_radio_cmd);
    answer->time = time(NULL);
    answer->temp = 40.3;
    answer->vcc = 3300;
    answer->mem = xPortGetFreeHeapSize();

}
//--------------------------------------------------------------------------------------------------
void radio_task(void *arg)
{
    radio_start=1;
    total_task++;

#ifdef PRN_SET
    s_radio *rec = NULL, *next = NULL;
    char line[128];
#endif

#if defined(SET_MQTT) || defined(PRN_SET)
    int i = 0;
#endif

    int all;

    if (ms_mode != DEV_MASTER) all = 0; else all = total_radio();

    ets_printf("%s[%s] Start radio_task (total network devices %d) | FreeMem %u%s\n", START_COLOR, TAGR, all, xPortGetFreeHeapSize(), STOP_COLOR);


    if (ms_mode == DEV_MASTER) {

#ifdef PRN_SET
	while ( (rec = get_radio(next)) != NULL ) {
		memset(line,0,sizeof(line));
		sprintf(line,"DevID=%08X", rec->one.dev_id);
		sprintf(line+strlen(line),"; color[%u.%u]:%u,%u,%u",
				rec->one.color.size,
				rec->one.color.mode,
				rec->one.color.rgb.r,
				rec->one.color.rgb.g,
				rec->one.color.rgb.b);
		ets_printf("[%s] (%u) %s\n", TAGR, i, line);
		i++;
		next = rec; rec = NULL;
	}
#endif

#ifdef SET_MQTT
	if (all > 0) {
	    i = 1;
	    if (xQueueSend(mqtt_net_queue, (void *)&i, (TickType_t)0) != pdPASS) {
		ESP_LOGE(TAGR, "Error while sending to mqtt_net_queue. | FreeMem %u", xPortGetFreeHeapSize());
	    }
	}
#endif

    }

    //all = update_radio_file();
    //print_radio_file();
    //if (all<=0) all = total_radio();

    s_radio_cmd rc;//cmd to slave
    s_radio_stat rs;//answer from slave
    memset((uint8_t *)&rs, 0, size_radio_stat);

    while (!restart_flag) {

	if (xQueueReceive(radio_cmd_queue, (void *)&rc, (TickType_t)0) == pdTRUE) {
#ifdef PRN_SET
		memset(line,0,sizeof(line));
		sprintf(line,"CMD for DevID=0x%08X", rc.one.dev_id);
		sprintf(line+strlen(line),"; color[%u.%u]:%u,%u,%u flag=%d",
				rc.one.color.size,
				rc.one.color.mode,
				rc.one.color.rgb.r,
				rc.one.color.rgb.g,
				rc.one.color.rgb.b,
				rc.flag);
		ets_printf("[%s] %s\n", TAGR, line);
#endif

		memset((uint8_t *)&rs, 0, size_radio_stat);
		do_slave(&rc, &rs);

#ifdef PRN_SET
		print_radio_stat(&rs);
#endif

	    if (xQueueSend(radio_evt_queue, (void *)&rs, (TickType_t)0) != pdPASS) {
		ESP_LOGE(TAGR, "Error while sending to radio_evt_queue. | FreeMem %u", xPortGetFreeHeapSize());
	    }

	}
	vTaskDelay(500 / portTICK_PERIOD_MS);

    }


    if (total_task) total_task--;
    radio_start = 0;
    ets_printf("%s[%s] Stop radio_task | FreeMem %u%s\n", START_COLOR, TAGR, xPortGetFreeHeapSize(), STOP_COLOR);

    vTaskDelete(NULL);
}
