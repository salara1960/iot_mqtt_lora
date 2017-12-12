#include "hdr.h"
#ifdef SET_WS

#include "mqtt.h"

//------------------------------------------------------------------------------------------------------------

const char *TAGWS = "WS";
uint8_t ws_start = 0;

static bool WS_conn = false;

const char WS_sec_WS_keys[] = "Sec-WebSocket-Key:";
const char WS_sec_conKey[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const char WS_srv_hs[] ="HTTP/1.1 101 Switching Protocols \r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %.*s\r\n\r\n";

//------------------------------------------------------------------------------------------------------------
err_t WS_write_data(int sc, char *p_data, size_t length)
{
    if (!WS_conn) return ERR_CONN;

    if (length > MAX_FRAME_LEN) return ERR_VAL;

    size_t ofs=0, tot=0, len;
    err_t result = ERR_VAL;

    WS_frame_header_t hdr;
    hdr.opcode = WS_OP_TXT; //4bit
    hdr.reserved = 0;       //3bit
    hdr.FIN = 1;            //1bit
    hdr.mask = 0;           //1bit

    unsigned short dl = length;
    dl = htons(dl);

    if (length >= WS_STD_LEN2) {
	hdr.payload_length = WS_STD_LEN2;
	ofs = sizeof(unsigned short);
    } else hdr.payload_length = length;

    len = length + sizeof(WS_frame_header_t) + ofs;
    char *buf = (char *)calloc(1, len);
    if (buf) {
	memcpy(buf + tot, &hdr, sizeof(WS_frame_header_t)); tot += sizeof(WS_frame_header_t);
	if (ofs) { memcpy(buf + tot, &dl, ofs); tot += ofs; }
	memcpy(buf + tot, p_data, length);
	if (send(sc, buf, len, 0) == len) result = ERR_OK; else result = ERR_VAL;

/* #ifdef WS_PRN
    buf_prn_hex(buf, len);

    printf("  %s: hdr: 0x%02X 0x%02X ", __func__, *(unsigned char *)(buf), *(unsigned char *)(buf+1));
    if (ofs) printf("0x%02X 0x%02X ", *(unsigned char *)(buf+2), *(unsigned char *)(buf+3));
    printf("data: %s\n", p_data);
#endif */
	free(buf);
    }

    return result;
}
//------------------------------------------------------------------------------------------------------------
void do_ack(int skt, char *ibuf)
{
int rts, mult;
s_led_cmd data;
uint8_t au = 1;

#ifdef WS_PRN
    char *stx = (char *)calloc(1, strlen(ibuf) + 64);
    if (stx) {
	sprintf(stx,"Recv. data (%d bytes) from ws_client:%s\n", strlen(ibuf), ibuf);
	print_msg(TAGWS, NULL, stx, 1);
	free(stx);
    }
#endif

    memset((uint8_t *)&data, 0, sizeof(s_led_cmd));
    rts = mult = parser_json_str(ibuf, (void *)&data, &au, NULL, NULL, NULL);
    if (mult != -1) rts &= 0xffff;
    switch (rts) {
	case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:// case 9://leds command
	case 28://{"rainbow":"on"},{"rainbow":"off"}
	case 29://{"line":"up"},{"line":"down"},{"line":"off"}
	case 30://{"white":"up"},{"white":"down"},{"white":"off"}
	    if (xQueueSend(led_cmd_queue, (void *)&data, (TickType_t)0) != pdPASS) {
		ESP_LOGE(TAGWS, "Error while sending to led_cmd_queue.");
	    }
	    vTaskDelay(200 / portTICK_RATE_MS);
	break;
	case 9:
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
#ifdef SET_MQTT
	    if (xQueueSend(mqtt_evt_queue, (void *)&mult, (TickType_t)0) != pdPASS) {
		ESP_LOGE(TAGWS, "Error while sending to mqtt_evt_queue.");
	    }
#endif
	break;
    }

    char *obuf = (char *)calloc(1, BUF_SIZE);
    if (obuf) {
	rts = make_msg(&mult, obuf, tls_client_ip, NULL, cli_id, NULL);
	if (rts > 0) {
	    WS_write_data(skt, obuf, (size_t)rts);
//#ifdef WS_PRN
	    print_msg(TAGWS, NULL, obuf, 1);
//#endif
	}
	free(obuf);
    }
}
//------------------------------------------------------------------------------------------------------------
void buf_prn_hex(char *bf, int len, bool fl)
{
    if ((len <= 0) || !bf) return;
    char *st = (char *)calloc(1, ((len*4) + 64));
    if (st) {
	sprintf(st,"buf(%d): ", len);
	for (int i = 0; i < len; i++) sprintf(st+strlen(st),"%02X ", *(unsigned char *)(bf+i));
	sprintf(st+strlen(st),"\n");
	print_msg(TAGWS, NULL, st, 1);
	free(st); st = NULL;
    }
    if (fl) {
	if (len >= sizeof(WS_frame_header_t)) {
	    st = (char *)calloc(1, 128);
	    if (st) {
		WS_frame_header_t *hdr = (WS_frame_header_t *)bf;
		sprintf(st,"hdr(%d): FIN:1=%d reserved:3=%d opcode:%d=%d , mask:1=%d len:7=%d\n",
			    sizeof(WS_frame_header_t),
			    hdr->FIN, hdr->reserved, WS_MASK_L, hdr->opcode,
			    hdr->mask, hdr->payload_length);
		print_msg(TAGWS, NULL, st, 1);
		free(st);
	    }
	}
    }
}
//------------------------------------------------------------------------------------------------------------
static void ws_server_netconn_serv(int sc)
{
char *p_buf = NULL;
uint16_t i;
char *p_payload = NULL;
WS_frame_header_t *p_frame_hdr;
int len = 0, total = 0;
TickType_t tcikl = 0;


    char *p_SHA1_Inp    = heap_caps_malloc(WS_CLIENT_KEY_L + sizeof(WS_sec_conKey), MALLOC_CAP_8BIT);//allocate memory for SHA1 input
    char *p_SHA1_result = heap_caps_malloc(SHA1_RES_L, MALLOC_CAP_8BIT);//allocate memory for SHA1 result
    char *buf = (char *)calloc(1, BUF_SIZE);

    if (p_SHA1_Inp && p_SHA1_result && buf) {
	//receive handshake request
	tcikl = get_tmr(WAIT_DATA_WS);
	while ( total < sizeof(WS_sec_conKey) ) {
	    if (restart_flag) goto done;
	    len = recv(sc, buf + total, BUF_SIZE - total - 1, 0);
	    if (len>0) {
		if (!total) tcikl = get_tmr(WAIT_DATA_WS);
		total += len;
	    }
	    if (total)
		if (check_tmr(tcikl)) break;
	}
#ifdef WS_PRN
	print_msg(TAGWS, NULL,"Receive handshake request:\n",1);
	print_msg(TAGWS, NULL, buf, 0);
#endif
	//write static key into SHA1 Input
	for (i = 0; i < sizeof(WS_sec_conKey); i++) p_SHA1_Inp[i + WS_CLIENT_KEY_L] = WS_sec_conKey[i];
	p_buf = strstr(buf, WS_sec_WS_keys);//find Client Sec-WebSocket-Key:
	if (p_buf) {
	    for (i = 0; i < WS_CLIENT_KEY_L; i++) p_SHA1_Inp[i] = *(p_buf + sizeof(WS_sec_WS_keys) + i);//get Client Key
	    esp_sha(SHA1, (unsigned char *)p_SHA1_Inp, strlen(p_SHA1_Inp), (unsigned char *)p_SHA1_result);// calculate hash
	    p_buf = (char*)_base64_encode((unsigned char *)p_SHA1_result, SHA1_RES_L, (size_t *)&i);//hex to base64
	    p_payload = heap_caps_malloc(sizeof(WS_srv_hs) + i - WS_SPRINTF_ARG_L, MALLOC_CAP_8BIT);//allocate memory for handshake
	    if (p_payload) {
		sprintf(p_payload, WS_srv_hs, i - 1, p_buf);//prepare handshake
		send(sc, p_payload, strlen(p_payload), 0);//send handshake
#ifdef WS_PRN
		print_msg(TAGWS, NULL, "Send handshake:\n", 1);
		print_msg(TAGWS, NULL, p_payload, 0);
#endif
		heap_caps_free(p_payload); p_payload=NULL;
		WS_conn = true;
		while (!restart_flag) {
		    total = len = 0;
		    memset(buf,0,BUF_SIZE);
		    tcikl = get_tmr(WAIT_DATA_WS);
		    while (total < sizeof(WS_frame_header_t)) {
			if (restart_flag) goto done;
			len = recv(sc, buf + total, BUF_SIZE - total - 1, 0);
			if (len > 0) {
			    if (!total) tcikl = get_tmr(WAIT_DATA_WS);
			    total += len;
			}
			if (total)
			    if (check_tmr(tcikl)) break;
		    }
#ifdef WS_PRN
		    buf_prn_hex(buf, sizeof(WS_frame_header_t), true);
#endif
		    p_frame_hdr = (WS_frame_header_t *)buf;
		    if (p_frame_hdr->opcode == WS_OP_CLS) break;//check if clients wants to close the connection
		    if (p_frame_hdr->payload_length <= WS_STD_LEN) {
			p_buf = (char *)&buf[sizeof(WS_frame_header_t)];
			if (p_frame_hdr->mask) {
			    p_payload = heap_caps_malloc(p_frame_hdr->payload_length + 1, MALLOC_CAP_8BIT);
			    if (p_payload) {
				//decode playload
				for (i = 0; i < p_frame_hdr->payload_length; i++) p_payload[i] = (p_buf + WS_MASK_L)[i] ^ p_buf[i % WS_MASK_L];
				p_payload[p_frame_hdr->payload_length] = 0;
			    }
			} else p_payload = p_buf;//content is not masked
			if ((p_payload) && (p_frame_hdr->opcode == WS_OP_TXT)) do_ack(sc, p_payload);
			if ((p_payload) && (p_payload != p_buf)) {
			    heap_caps_free(p_payload);
			    p_payload = NULL;
			}
		    }
		    vTaskDelay(10 / portTICK_RATE_MS);
		}
	    }
	}
    }

done:

    WS_conn = false;
    if (p_payload) heap_caps_free(p_payload);
    if (buf) free(buf);
    if (p_SHA1_Inp) heap_caps_free(p_SHA1_Inp);
    if (p_SHA1_result) heap_caps_free(p_SHA1_result);

}
//------------------------------------------------------------------------------------------------------------
void ws_task(void *arg)
{
ws_start = 1;
total_task++;

int srv = -1, cli = -1;
struct sockaddr_in client_addr;
unsigned int socklen = sizeof(client_addr);


    uint16_t wp = *(u16_t *)arg;

    ets_printf("%s[%s] Start WebSocket task (port=%u)| FreeMem %u%s\n", START_COLOR, TAGWS, wp, xPortGetFreeHeapSize(), STOP_COLOR);

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    srv = create_tcp_server(wp);
    if (srv >= 0) {
	fcntl(srv, F_SETFL, (fcntl(srv, F_GETFL, 0)) | O_NONBLOCK);
	ets_printf("%s[%s] Wait new web_socket client... | FreeMem %u%s\n", START_COLOR, TAGWS, xPortGetFreeHeapSize(), STOP_COLOR);
	while (!restart_flag) {
	    cli = accept(srv, (struct sockaddr*)&client_addr, &socklen);
	    if (cli > 0) {
		ets_printf("%s[%s] ws_client %s:%u (soc=%u) online | FreeMem %u%s\n",
				START_COLOR,
				TAGWS,
				(char *)inet_ntoa(client_addr.sin_addr),
				htons(client_addr.sin_port),
				cli,
				xPortGetFreeHeapSize(),
				STOP_COLOR);
		fcntl(cli, F_SETFL, (fcntl(cli, F_GETFL, 0)) | O_NONBLOCK);
		ws_server_netconn_serv(cli);
		ets_printf("%s[%s] Closed connection (%s:%u soc=%u) | FreeMem %u%s\n",
				START_COLOR,
				TAGWS,
				(char *)inet_ntoa(client_addr.sin_addr),
				htons(client_addr.sin_port),
				cli,
				xPortGetFreeHeapSize(),
				STOP_COLOR);
		close(cli); cli = -1;
	    }
	}
    } else {
	ESP_LOGE(TAGWS, "ERROR create_tcp_server(%u)=%d", wp, srv);
    }


    ets_printf("%s[%s] WebSocket task stop | FreeMem %u%s\n", START_COLOR, TAGWS, xPortGetFreeHeapSize(), STOP_COLOR);

    if (total_task) total_task--;
    ws_start = 0;

    vTaskDelete(NULL);
}
//------------------------------------------------------------------------------------------------------------
#endif
