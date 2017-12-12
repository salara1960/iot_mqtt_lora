#include "hdr.h"
#ifdef SET_SNTP

#include "mqtt.h"
//#include "sntp.h"
//----------------------------------------------------------------------------------------
const char *TAGS = "NTP";
uint8_t sntp_start=0;
//----------------------------------------------------------------------------------------
void sntp_task(void *arg)
{
sntp_start = 1;
total_task++;

    ets_printf("%s[%s] Start sntp_task | FreeMem %u%s\n", START_COLOR, TAGS, xPortGetFreeHeapSize(), STOP_COLOR);

    char *sntp_server = (char *)arg;

    if (sntp_server) {

	time_t now = 0;
	struct tm timeinfo = {0};
	uint8_t retry = 0, retry_max = 20;

	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

	ets_printf("[%s] Getting time from SNTP server '%s'.\n", TAGS, sntp_server);

	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, sntp_server);
	sntp_init();

	do {
		retry++;
		time(&now);
		localtime_r(&now, &timeinfo);
		if (restart_flag) break; else vTaskDelay(500 / portTICK_PERIOD_MS);
	} while ( (timeinfo.tm_year < (2017 - 1900)) && (retry < retry_max) );

	if (retry < retry_max) {
		time(&now);// update 'now' variable with current time
		setenv("TZ", time_zone, 1);
		tzset();
		localtime_r(&now, &timeinfo);
		char strftime_buf[64] = {0};
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		ets_printf("%s[%s] The current date/time is: %s %s%s\n", GREEN_COLOR, TAGS, strftime_buf, time_zone, STOP_COLOR);
	} else {
		ESP_LOGE(TAGS, "Error getting date/time from srv '%s %s'", sntp_server, time_zone);
	}

	sntp_stop();

    }

    if (total_task) total_task--;
    sntp_start = 0;
    ets_printf("%s[%s] Stop sntp_task | FreeMem %u%s\n", START_COLOR, TAGS, xPortGetFreeHeapSize(), STOP_COLOR);

    vTaskDelete(NULL);
}

#endif
