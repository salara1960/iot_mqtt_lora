#include "hdr.h"
#ifdef SET_WEB

#include "mqtt.h"

//------------------------------------------------------------------------------------------------------------

const char *TAGWEB = "WEB";
uint8_t web_start = 0;

static const char *http_html_hdr = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";

static const char *http_index_hml = "<!DOCTYPE html>"\
"<html>\n"\
"<head>\n"\
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" charset=\"UTF-8\">\n"\
"  <style type=\"text/css\">\n"\
"    html, body, iframe { margin: 0; padding: 0; height: 100%; }\n"\
"    iframe { display: block; width: 100%; border: none; }\n"\
"  </style>\n"\
"<title>ESP32</title>\n"\
"</head>\n"\
"<body>\n"\
"<h2>Hello, i am ESP32 !</h2>\n"\
"</body>\n"\
 "</html>\n";
//------------------------------------------------------------------------------------------------------------
void http_request_parse(int sc, char *js)
{
int total = 0, len = 0, mult = 10;//"get":"status"
s_led_cmd data;
uint8_t onoff = 0, col = 0, ready = 0;
TickType_t tcikl = 0;


    memset(js,0,BUF_SIZE);

    while (!ready && !restart_flag) {
	len = recv(sc, js + total, BUF_SIZE - total - 1, 0);
	if (len>0) {
	    if (!total) tcikl = get_tmr(TWAIT);
	    total += len;
	    if ((total + 1) >= BUF_SIZE) ready = 1;
	    else
	    if (strstr(js,"\r\n\r\n")) ready = 1;
	}
	if (total)
	    if (check_tmr(tcikl)) break;
    }

    if (ready) {
#ifdef SET_WEB_PRN
	char *tmp = (char *)calloc(1, total + 64);
	if (tmp) {
	    sprintf(tmp,"[%s] request (%u):\n%.*s", TAGWEB, total, total, js);
	    ets_printf(tmp);
	    free(tmp);
	}
#endif
	if (strstr(js,"GET /")) {
	    if (xSemaphoreTake(status_mutex, portMAX_DELAY) == pdTRUE) {
		memcpy((uint8_t *)&data, (uint8_t *)&led_cmd, sizeof(s_led_cmd));
		xSemaphoreGive(status_mutex);
	    } else data.size = PIXELS;
	    data.mode = rgb_mode = RGB_NONE;
	    send(sc, http_html_hdr, strlen(http_html_hdr), 0);
	    if (strstr(js,"/off")) {
		onoff = 1; col = 0;
	    } else if(strstr(js,"/on")) {
		onoff = 1; col = 255;
	    } else if(strstr(js,"/status")) {
		memset(js, 0, BUF_SIZE);
		len = make_msg(&mult, js, tls_client_ip, NULL, cli_id, NULL);
		if (len > 0) {
		    ets_printf("[%s] %s", TAGWEB, js);
		    send(sc, js, len, 0);
		}
	    } else {
		send(sc, http_index_hml, strlen(http_index_hml), 0);
	    }

	    if (onoff) {
		data.rgb.r = data.rgb.g = data.rgb.b = col;
		//memset((uint8_t *)&(data.rgb), col, sizeof(ss_rgb));
		if (xQueueSend(led_cmd_queue, (void *)&data, (TickType_t)1000) != pdPASS) {
		    ESP_LOGE(TAGWEB, "Error while sending to led_cmd_queue.");
		}
		send(sc, http_index_hml, strlen(http_index_hml), 0);
	    }
	}
    }

}
//------------------------------------------------------------------------------------------------------------
void web_task(void *arg)
{
web_start = 1;
total_task++;

int srv = -1, cli = -1;
struct sockaddr_in client_addr;
unsigned int socklen = sizeof(client_addr);


    uint16_t wp = *(u16_t *)arg;

    ets_printf("%s[%s] Start WebServer task (port=%u)| FreeMem %u%s\n", START_COLOR, TAGWEB, wp, xPortGetFreeHeapSize(), STOP_COLOR);

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);


    char *jss = (char *)calloc(1, BUF_SIZE);
    if (jss) {
	srv = create_tcp_server(wp);
	if (srv >= 0) {
	    fcntl(srv, F_SETFL, (fcntl(srv, F_GETFL, 0)) | O_NONBLOCK);
	    ets_printf("%s[%s] Wait new http client... | FreeMem %u%s\n", START_COLOR, TAGWEB, xPortGetFreeHeapSize(), STOP_COLOR);
	    while (!restart_flag) {
		cli = accept(srv, (struct sockaddr*)&client_addr, &socklen);
		if (cli > 0) {
		    ets_printf("%s[%s] Http_client %s:%u (soc=%u) online | FreeMem %u%s\n", START_COLOR, TAGWEB, (char *)inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port), cli, xPortGetFreeHeapSize(), STOP_COLOR);
		    fcntl(cli, F_SETFL, (fcntl(cli, F_GETFL, 0)) | O_NONBLOCK);
		    http_request_parse(cli, jss);
		    ets_printf("%s[%s] Closed connection (%s:%u soc=%u) | FreeMem %u%s\n", START_COLOR, TAGWEB, (char *)inet_ntoa(client_addr.sin_addr), htons(client_addr.sin_port), cli, xPortGetFreeHeapSize(), STOP_COLOR);
		    close(cli); cli = -1;
		}
	    }
	} else {
	    ESP_LOGE(TAGWEB, "ERROR create_tcp_server(%u)=%d", wp, srv);
	}
	free(jss);
    }

    ets_printf("%s[%s] WebServer task stop | FreeMem %u%s\n", START_COLOR, TAGWEB, xPortGetFreeHeapSize(), STOP_COLOR);


    if (total_task) total_task--;
    web_start = 0;

    vTaskDelete(NULL);
}
//------------------------------------------------------------------------------------------------------------
#endif
