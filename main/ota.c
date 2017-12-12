#include "hdr.h"
#ifdef SET_OTA

#include "mqtt.h"

//------------------------------------------------------------------------------------------------------------

const char *TAGO = "OTA";
#ifdef OTA_WR_ENABLE
    static char *ota_write_data = NULL;	//[BUFFSIZE + 1] = {0};// ota data write buffer ready to write to the flash
#endif
static char *text = NULL;		//[BUFFSIZE + 1] = {0};//recv. buffer
static int binary_file_length = 0;	//current writen length
static int total_binary_file_length = 0;//file length (from http header)
static int total_recv_length = 0;	//current recv. bytes
static int socket_id = -1, blk_num = 0;

//------------------------------------------------------------------------------------------------------------
int read_until(char *buffer, char delim, int len)
{
int i = 0;

    while (buffer[i] != delim && i < len) ++i;

    return i + 1;
}
//------------------------------------------------------------------------------------------------------------
// resolve a packet from http socket
// return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
// otherwise return false
bool read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle)
{
// i - current position
int i = 0, i_read_len = 0, prn = 1;
char *uks=&text[0], *uke = NULL;

    while (text[i] != 0 && i < total_len) {
        i_read_len = read_until(&text[i], '\n', total_len);
        if (prn) {
	    uke = strstr(text, "\r\n\r\n");
	    if (uke) {
		ESP_LOGI(TAGO, "Header:\n%.*s", uke - uks + 4, uks);
		prn = 0;
		uks = strstr(text, "Content-Length:");
		if (uks) {
			uks += 15;
			if (*uks == ' ') uks++;
			uke = strstr(uks, "\r\n");
			if (uke) {
				int dl = uke - uks;
				if (dl > 0) {
					char *tl = (char *)calloc(1, dl + 1);
					if (tl) {
						memcpy(tl , uks, dl);
						total_binary_file_length = atoi(tl);
						//ESP_LOGW(TAGO, "Downloading file %s len=%d ...", ota_filename, total_binary_file_length);
						ets_printf("%s[%s] Downloading file %s len=%d ...%s\n", GREEN_COLOR, TAGO, ota_filename, total_binary_file_length, STOP_COLOR);
						free(tl);
					}
				}
			}
		}
	    }
        }
        if (total_binary_file_length <= 0) {
            ets_printf("%s[%s] Invalid header from server : absent 'Content-Length:'%s\n", GREEN_COLOR, TAGO, STOP_COLOR);
            return false;
        } else if (total_binary_file_length > OTA_PART_SIZE) {
            ets_printf("%s[%s] Too big file_length %d. Max ota_partition_size %d%s\n", GREEN_COLOR, TAGO, total_binary_file_length, OTA_PART_SIZE, STOP_COLOR);
            return false;
        }
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2) {
            int i_write_len = total_len - (i + 2);
            total_recv_length = i_write_len;
            // copy first http packet body to write buffer
#ifdef OTA_WR_ENABLE
            memset(ota_write_data, 0, BUFFSIZE);
            memcpy(ota_write_data, &(text[i + 2]), i_write_len);
            esp_err_t err = esp_ota_write(update_handle, (const void *)ota_write_data, i_write_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAGO, "Error: esp_ota_write failed! err=0x%x", err);
                return false;
            } else {
#endif
                binary_file_length += i_write_len;
                blk_num++;
                ets_printf("[%s] Write binary data length %d from %d bytes (last_blk: num=%d size=%d)\n",
				TAGO, binary_file_length, total_binary_file_length, blk_num, i_write_len);
#ifdef OTA_WR_ENABLE
            }
#endif
            return true;
        }
        i += i_read_len;
    }
    return false;
}
//------------------------------------------------------------------------------------------------------------
bool connect_to_http_server()
{

//    ets_printf("[%s] %s() start\n", TAGO, __func__);

    socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_LOGE(TAGO, "Create socket failed!");
        return false;
    }

    struct sockaddr_in sock_info;
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;
    sock_info.sin_addr.s_addr = inet_addr(ota_srv);	//(EXAMPLE_SERVER_IP);
    sock_info.sin_port = htons(ota_srv_port);		//(EXAMPLE_SERVER_PORT);

    if (connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info))  == -1) {
        ESP_LOGE(TAGO, "Connect to server %s:%d failed! errno=%d", ota_srv, ota_srv_port, errno);
        close(socket_id);
        socket_id = -1;
        return false;
    } else {
        fcntl(socket_id, F_SETFL, (fcntl(socket_id, F_GETFL, 0)) | O_NONBLOCK);//set nonblock mode
        return true;
    }

    return false;
}
//------------------------------------------------------------------------------------------------------------
void ota_task(void *arg)
{
ota_begin = 1;
total_task++;
ota_ok = 0;
int buff_len = 0;
total_recv_length = binary_file_length = blk_num = 0;
TickType_t task_start_time = xTaskGetTickCount();
#ifdef OTA_WR_ENABLE
    char *err_txt = NULL;
#endif

    ets_printf("%s[%s] Start Firmware OTA update task | FreeMem %u%s\n", START_COLOR, TAGO, xPortGetFreeHeapSize(), STOP_COLOR);


    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

#ifdef OTA_WR_ENABLE
    ota_write_data = (char *)calloc(1, BUFFSIZE + 1);
    if (!ota_write_data) goto quit;
#endif
    text = (char *)calloc(1, BUFFSIZE + 1);
    if (!text) goto quit;

    if (connect_to_http_server() == false) goto quit;

    esp_ota_handle_t update_handle = 0;// update handle : set by esp_ota_begin(), must be freed via esp_ota_end()

/*
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
	ESP_LOGE(TAGO, "No running partition found. Bye..");
	goto quit;
    }
    ets_printf("[%s] Running    partition label=%s type=%d subtype=%d addr=0x%08x\n",
		TAGO, running->label, running->type, running->subtype, running->address);

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    if (!configured) {
	ESP_LOGE(TAGO, "No OTA configured partition found. Bye..");
	goto quit;
    }
    ets_printf("[%s] Configured partition label=%s type=%d subtype=%d addr=0x%08x\n",
		TAGO, configured->label, configured->type, configured->subtype, configured->address);
    assert(configured == running);// fresh from reset, should be running from configured boot partition
*/

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAGO, "No OTA partition found. Bye..");
        goto quit;
    }
    //assert(update_partition != NULL);
    ets_printf("[%s] Found next partition label=%s type=%d subtype=%d addr=0x%x\n",
		TAGO , update_partition->label, update_partition->type, update_partition->subtype, update_partition->address);

    char http_request[256] = {0};
    sprintf(http_request, "GET %s HTTP/1.1\r\nHost: %s:%d\r\nKeep-Alive: timeout=60, max=60 \r\n\r\n", ota_filename, ota_srv, ota_srv_port);//EXAMPLE_FILENAME, EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
    ets_printf("[%s] Send request to server %s:%d:\n%s", TAGO, ota_srv, ota_srv_port, http_request);
    int res = send(socket_id, http_request, strlen(http_request), 0);
    if (res < 0) {
        ESP_LOGE(TAGO, "Send GET request to server failed");
        goto done;
    }

#ifdef OTA_WR_ENABLE
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAGO, "esp_ota_begin failed, error=%d", err);
        goto done;
    }

    err_txt = (char *)calloc(1, 128);
    if (!err_txt) {
        ESP_LOGE(TAGO, "Error calloc for error string");
    }
#else
    vTaskDelay(200 / portTICK_RATE_MS);
#endif


    bool resp_body_start = false, flag = true;

    int tmp_recv = 0, ready = 0;
    memset(text, 0, BUFFSIZE + 1);
    TickType_t tmr = get_tmr(OTA_DATA_WAIT);


    while (flag) {

        buff_len = recv(socket_id, &text[tmp_recv], BUFFSIZE - tmp_recv, MSG_DONTWAIT);

        if (buff_len == 0) {// packet over
            flag = false;
            ets_printf("[%s] Server closed connection.\n", TAGO);
            if (tmp_recv > 0) {
		ets_printf("[%s] In text_buffer present %d bytes\n", TAGO, tmp_recv);
            }
        } else if (buff_len > 0) {// receive error
	    tmr = get_tmr(OTA_DATA_WAIT);
	    if (!resp_body_start) {// deal with response header
		resp_body_start = read_past_http_header(text, buff_len, update_handle);
		if (!resp_body_start) {
			ESP_LOGE(TAGO, "Unworking stage !!! buff_len=%d file_len=%d update_handle=%d",
					buff_len,total_binary_file_length,update_handle);
			flag = false;
		} else {
			tmp_recv = ready = 0;
			memset(text, 0, BUFFSIZE + 1);
		}
	    } else {
		total_recv_length += buff_len;
		tmp_recv += buff_len;
		if (tmp_recv == BUFFSIZE) {
			buff_len = tmp_recv;
			ready=1;
		} else if (total_recv_length == total_binary_file_length) ready = 1;

		if (ready) {
#ifdef OTA_WR_ENABLE
			memcpy(ota_write_data, text, buff_len);
			err = esp_ota_write(update_handle, (const void *)ota_write_data, buff_len);
			if (err != ESP_OK) {
				if (err_txt) {
					memset(err_txt, 0, 128);
					switch (err) {
						case ESP_ERR_INVALID_ARG:
							strcpy(err_txt,"handle is invalid");
						break;
						case ESP_ERR_OTA_VALIDATE_FAILED:
							strcpy(err_txt,"First byte of image contains invalid app image magic byte");
						break;
						case ESP_ERR_FLASH_OP_TIMEOUT:
						case ESP_ERR_FLASH_OP_FAIL:
							strcpy(err_txt,"Flash write failed");
						break;
						case ESP_ERR_OTA_SELECT_INFO_INVALID:
							strcpy(err_txt,"OTA data partition has invalid contents");
						break;
							default : sprintf(err_txt,"Unknown error (%d)", err);
					}
					ESP_LOGE(TAGO, "Error : %s", err_txt);
				}
				break;
			} else {
				binary_file_length += buff_len;
			}
#else
			binary_file_length += buff_len;
#endif
			blk_num++;
			tmp_recv = ready = 0;
			memset(text, 0, BUFFSIZE+1);
			vTaskDelay(10 / portTICK_RATE_MS);
		}
		if (binary_file_length == total_binary_file_length) {
			ota_ok = 1;
			flag = false;
		}
	    }
        }
        if (check_tmr(tmr)) {
		flag = false;
		ets_printf("[%s] Timeout (%d ms)... No data from server %s:%d\n", TAGO, OTA_DATA_WAIT, ota_srv, ota_srv_port);
        }
    }

done:

    if (socket_id>0) close(socket_id);

    ets_printf("[%s] Write binary data length %d from %d bytes (last_blk: num=%d size=%d)\n",
		TAGO, binary_file_length, total_binary_file_length, blk_num, buff_len);

#ifdef OTA_WR_ENABLE
    if (err_txt) free(err_txt);

    if (esp_ota_end(update_handle) == ESP_OK) {
	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK) {
		ota_ok = 0;
		ESP_LOGE(TAGO, "esp_ota_set_boot_partition failed! err=0x%x", err);
	}
    } else {
	ota_ok = 0;
	ESP_LOGE(TAGO, "esp_ota_end failed!");
    }
#endif


quit:

#ifdef OTA_WR_ENABLE
    if (ota_write_data) free(ota_write_data);
#endif
    if (text) free(text);

    uint32_t wtp = xTaskGetTickCount() - task_start_time;
    if (ota_ok) {
	ets_printf("%s[%s] OTA task ended OK. Writen new image from '%s:%d%s' (%d bytes) (work_time=%u ms) | FreeMem %u%s\n",
			START_COLOR, TAGO, ota_srv, ota_srv_port, ota_filename, binary_file_length, wtp, xPortGetFreeHeapSize(), STOP_COLOR);
	restart_flag = 1;
    } else {
	ESP_LOGE(TAGO, "OTA task ended failed (work_time=%u ms) | FreeMem %u", wtp, xPortGetFreeHeapSize());
    }

    ota_begin = 0;
    if (total_task) total_task--;

    vTaskDelete(NULL);
}
//------------------------------------------------------------------------------------------------------------
#endif
