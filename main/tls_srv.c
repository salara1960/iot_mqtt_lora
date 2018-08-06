#include "hdr.h"

#ifdef SET_TLS_SRV

#include "mqtt.h"

//******************************************************************************************

const char *instr = "alarm";
const char *TAGTLS = "TLS";
uint8_t tls_start = 0;
uint8_t tls_hangup = 0;

//******************************************************************************************
time_t mk_hash(char *out, const char *part)
{
#ifdef SET_MD5
    #define hash_len 16
    const char *mark = "MD5";
#else
    #if defined(SET_SHA2_256)
        #define hash_len 32
        const char *mark = "SHA2_256";
        esp_sha_type sha_type = SHA2_256;
    #elif defined(SET_SHA2_384)
        #define hash_len 48
        const char *mark = "SHA2_384";
        esp_sha_type sha_type = SHA2_384;
    #elif defined(SET_SHA2_512)
        #define hash_len 64
        const char *mark = "SHA2_512";
        esp_sha_type sha_type = SHA2_512;
    #else
        #define hash_len 20
        const char *mark = "SHA1";
        esp_sha_type sha_type = SHA1;
    #endif
#endif
unsigned char hash[hash_len] = {0};
time_t ret = time(NULL);

    char *ts = (char *)calloc(1, (strlen(part)<<1) + 32);
    if (ts) {
	memset(out, 0, strlen(out));
	sprintf(ts,"%s_%u_%s", part, (uint32_t)ret, part);
#ifdef SET_MD5
	mbedtls_md5((unsigned char *)ts, strlen(ts), hash);
#else
	esp_sha(sha_type, (const unsigned char *)ts, strlen(ts), hash);
#endif
	free(ts);
	for (uint8_t i=0; i<hash_len; i++) sprintf(out+strlen(out),"%02X",hash[i]);
	ets_printf("%s[%s] %s hash=%s%s\n", GREEN_COLOR, TAGTLS, mark, out, STOP_COLOR);
    } else ret = 0;

    return ret;
}
//******************************************************************************************
//                    TLS server (support one client ONLY)
//                    Valid commands -> CmdName[]
//******************************************************************************************
void tls_task(void *arg)
{
tls_start = 1;
total_task++;

mbedtls_entropy_context  entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_context      ssl;
mbedtls_x509_crt         srvcert;
mbedtls_pk_context       pkey;
mbedtls_ssl_config       conf;
mbedtls_net_context      server_ctx, client_ctx;
s_led_cmd data;
int ret = 0, len = 0, err = 0, rts, mult;
uint8_t auth = 0, eot = 0;
uint32_t timeout = timeout_auth;
time_t cur_time = 0, wait_time = 0;
char *buf = NULL, *stx = NULL, *uk = NULL;
char ts[64] = {0}, hash_str[256] = {0}, str_tls_port[8] = {0};

s_tls_flags flags = {
    .first = 1,
    .first_send = 0,
    .none = 0,
};


    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    sprintf(str_tls_port,"%u", *(uint16_t *)arg);

    ets_printf("%s[%s] TLS server task starting...(port=%s) | FreeMem=%u%s\n", START_COLOR, TAGTLS, str_tls_port, xPortGetFreeHeapSize(), STOP_COLOR);

    buf = (char *)calloc(1, BUF_SIZE);
    stx = (char *)calloc(1, BUF_SIZE);
    if ((!buf) || (!stx)) goto quit1;

    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&srvcert);
    mbedtls_pk_init(&pkey);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_ssl_config_init(&conf);

    mbedtls_entropy_init(&entropy);
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)TAGTLS, strlen(TAGTLS))) != 0) {
        ESP_LOGE(TAGTLS," failed  ! mbedtls_ctr_drbg_seed returned %d", ret);
        goto quit1;
    }

    ret = mbedtls_x509_crt_parse(&srvcert, (uint8_t *)&server_cert[0], (unsigned int)(server_cert_end - server_cert));
//    ret = mbedtls_x509_crt_parse(&srvcert, (uint8_t *)&tls_cert_start[0], (unsigned int)(tls_cert_end - tls_cert_start));
    if (ret < 0) {
        ESP_LOGE(TAGTLS," failed  !  mbedtls_x509_crt_parse returned -0x%x", -ret);
        goto quit1;
    }

    ret = mbedtls_pk_parse_key(&pkey, (uint8_t *)&server_key[0], (unsigned int)(server_key_end - server_key), NULL, 0);
//    ret = mbedtls_pk_parse_key(&pkey, (uint8_t *)&tls_key_start[0], (unsigned int)(tls_key_end - tls_key_start), NULL, 0);
    if (ret) {
        ESP_LOGE(TAGTLS," failed ! mbedtls_pk_parse_key returned - 0x%x", -ret);
        goto quit1;
    }

    if ((ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        ESP_LOGE(TAGTLS," failed  ! mbedtls_ssl_config_defaults returned %d", ret);
        err = ret;
        //goto exit;
        goto quit1;
    }

/*
// MBEDTLS_SSL_VERIFY_OPTIONAL is bad for security, in this example it will print
//   a warning if CA verification fails but it will continue to connect.
//   You should consider using MBEDTLS_SSL_VERIFY_REQUIRED in your own code.
mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
mbedtls_ssl_conf_ca_chain(&conf, &srvcert, NULL);
mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
mbedtls_esp_enable_debug_log(&conf, 4);
#endif
*/

    mbedtls_ssl_conf_ca_chain(&conf, srvcert.next, NULL);
    if ( ( ret = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey) ) != 0 ) {
        ESP_LOGE(TAGTLS," failed  ! mbedtls_ssl_conf_own_cert returned %d", ret );
        //abort();
        goto quit1;
    }

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);


/*
#ifdef MBEDTLS_DEBUG_C
    mbedtls_debug_set_threshold(DEBUG_LEVEL);
    mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);
#endif
*/
    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        ESP_LOGE(TAGTLS," failed ! mbedtls_ssl_setup returned %d", ret);
        //mbedtls_ssl_free(&ssl);
        err=ret;
        goto quit1;
    }

    //---------         SET TIMEOUT FOR READ        -----------------------
    timeout = timeout_auth;//25000 msec
    mbedtls_ssl_conf_read_timeout(&conf, timeout);
    //---------------------------------------------------------------------

    tls_client_ip = 0;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_in);

    while (!restart_flag) {

        mbedtls_net_init(&server_ctx);
        mbedtls_net_init(&client_ctx);
        ets_printf("[%s] Wait new connection...\n", TAGTLS);

        // Bind
        ret = mbedtls_net_bind(&server_ctx, NULL, str_tls_port, MBEDTLS_NET_PROTO_TCP);
        if (ret) {
            ESP_LOGE(TAGTLS," failed ! mbedtls_net_bind returned %d", ret);
            err=ret;
            goto exit;
        }
	ret = mbedtls_net_set_nonblock(&server_ctx);
	if (ret) {
	    ESP_LOGE(TAGTLS,"mbedtls_net_set_nonblock for server returned %d", ret);
	}

	// Accept
	ret = MBEDTLS_ERR_SSL_WANT_READ;
	while (ret == MBEDTLS_ERR_SSL_WANT_READ) {
	    ret = mbedtls_net_accept(&server_ctx, &client_ctx, NULL, 0, NULL);
	    if (restart_flag) {
		eot = 1; err = 0;
		goto exit1;
	    } else vTaskDelay(250 / portTICK_RATE_MS);//500//1000
	}
	if (ret) {
	    ESP_LOGE(TAGTLS," Failed to accept connection. Restarting.");
	    mbedtls_net_free(&client_ctx);
	    mbedtls_net_free(&server_ctx);
	    continue;
	}
	getpeername(client_ctx.fd, (struct sockaddr *)&peer_addr, &peer_addr_len);
	ets_printf("[%s] New client %s:%u online (sock=%d)\n", TAGTLS, (char *)inet_ntoa(peer_addr.sin_addr), htons(peer_addr.sin_port) , client_ctx.fd);

//ret = mbedtls_net_set_nonblock(&client_ctx);
//ESP_LOGW(TAGTLS,"mbedtls_net_set_nonblock for client returned %d", ret);

        mbedtls_ssl_set_bio(&ssl, &client_ctx, mbedtls_net_send, NULL, mbedtls_net_recv_timeout);//<- blocking I/O, f_recv == NULL, f_recv_timout != NULL
        //mbedtls_ssl_set_bio(&ssl, &client_ctx, mbedtls_net_send, mbedtls_net_recv, NULL); //<- non-blocking I/O, f_recv != NULL, f_recv_timeout == NULL

        // Handshake
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                ESP_LOGE(TAGTLS," failed ! mbedtls_ssl_handshake returned -0x%x", -ret);
                err = ret;
                goto exit;
            }
        }
/*
        ESP_LOGW(TAGTLS, "Handshake OK. Ciphersuite %s", mbedtls_ssl_get_ciphersuite(&ssl));
	// Verify the server certificate
	ESP_LOGW(TAGTLS, "Verifying peer X.509 certificate..." );
	// In real life, we probably want to bail out when ret != 0
	ret = mbedtls_ssl_get_verify_result(&ssl);
	if (ret != 0) {
	    memset(stx, 0, BUF_SIZE);
	    ESP_LOGE(TAGTLS, " failed !!! mbedtls_ssl_get_verify_result()=%d", ret);
	    mbedtls_x509_crt_verify_info(stx, BUF_SIZE, " ! ", ret);
	    ESP_LOGE(TAGTLS, "%s", stx);
	} else {
	    ESP_LOGW(TAGTLS, " OK");
	}
	ESP_LOGW(TAGTLS, "Peer certificate information    ...");
	memset(buf, 0, BUF_SIZE);
	mbedtls_x509_crt_info(buf, BUF_SIZE - 1, "+++++", mbedtls_ssl_get_peer_cert(&ssl));
	ESP_LOGW(TAGTLS, "%s\n", buf);
*/

	memset((uint8_t *)&data, 0, size_led_cmd);
	tls_hangup = 0;
	flags.first = 1;
	flags.first_send = 0;
	wait_time = time(NULL);
	// Read loop
	while (!tls_hangup && !restart_flag) {
	    //
	    if ((!auth) && (flags.first)) {
		cur_time = mk_hash(hash_str, instr);
		len = sprintf(ts,"{\"ts\":%u}\r\n", (uint32_t)cur_time);
		while ((ret = mbedtls_ssl_write(&ssl, (unsigned char *)ts, len)) <= 0) {
		    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			ESP_LOGE(TAGTLS," failed ! mbedtls_ssl_write returned %d", ret);
			err = ret;
			break;
		    }
		}
		print_msg(TAGTLS, NULL, ts, 1);
		flags.first = 0;
	    }
	    //
	    memset(stx, 0, BUF_SIZE);
	    memset(buf, 0, BUF_SIZE); len = BUF_SIZE-1;
	    int rtr = mbedtls_ssl_read(&ssl, (unsigned char *)buf, len);
	    if (rtr > 0) {
		wait_time = time(NULL);
		uk = strstr(buf + 2, "\r\n"); if (uk) *uk = '\0';
		sprintf(stx,"Recv. data (%d bytes) from client:%s\n", rtr, buf); print_msg(TAGTLS, NULL, stx, 1);
		eot = 0;
		mult = parser_json_str(buf, (void *)&data, &auth, hash_str, &eot, NULL); rts = mult;
		if (mult != -1) rts &= 0xffff;
		if (auth) {
		    //*********************************************************************************************************************
		    switch (rts) {
			case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:// case 9://leds command
			case 28://{"rainbow":"on"},{"rainbow":"off"}
			case 29://{"line":"up"},{"line":"down"},{"line":"off"}
			case 30://{"white":"up"},{"white":"down"},{"white":"off"}
			    if (xQueueSend(led_cmd_queue, (void *)&data, (TickType_t)0) != pdPASS) {
				ESP_LOGE(TAGTLS, "Error while sending to led_cmd_queue.");
			    }
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
			    if (!flags.first_send) {
				if (xQueueSend(mqtt_evt_queue, (void *)&mult, (TickType_t)0) != pdPASS) {
				    ESP_LOGE(TAGTLS, "Error while sending to mqtt_evt_queue.");
				}
				flags.first_send = 1;
			    }
#endif
			    if (rts == 16) wait_time = time(NULL);//set new time (cmd=time)
			break;
			/*case 15://restart
			case 19://ssid
			case 20://key
			case 21://set new broker
			case 22://{"wifi_mode":"STA"} , {"wifi_mode":"AP"} , {"wifi_mode":"APSTA"}
			case 23://{"ota":"on"}
			case 24://{"ota_url":"10.100.0.103:9980/esp32/mqtt.bin"}
			case 26://{"mqtt_user":"login:password"}
			break;*/
		    }//switch(rts)
		    //*********************************************************************************************************************
		    timeout = timeout_def;//60000 msec
		    mbedtls_ssl_conf_read_timeout(&conf, timeout);
		}
		err = 0;
	    } else if (!rtr) {
		ets_printf("[%s] Client closed connection (%d)\n", TAGTLS, rtr);
		err = 0;
		break;
	    } else {// rtr < 0  -  no data from client
		err = rtr;
		if (rtr == MBEDTLS_ERR_SSL_TIMEOUT) {// -0x6800 The operation timed out
		    if (auth) {
			if ( ((uint32_t)time(NULL) - (uint32_t)wait_time) >= def_idle_count ) {
			    sprintf(stx,"Timeout...(no data from client %u sec). Server closed connection.\n", def_idle_count);
			    print_msg(TAGTLS, NULL, stx, 1);
			    break;
			} else continue;
		    } else break;
		} else break;
	    }

	    // Write
	    memset(buf, 0, BUF_SIZE);
	    if (auth) {
		tls_client_ip = (uint32_t)peer_addr.sin_addr.s_addr;
		vTaskDelay(200 / portTICK_RATE_MS);
		len = make_msg(&mult, buf, tls_client_ip, NULL, cli_id, NULL);
	    } else len = sprintf(buf, "{\"status\":\"You are NOT auth. client, bye\"}\r\n");

	    while ((ret = mbedtls_ssl_write(&ssl, (unsigned char *)buf, len)) <= 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
		    ESP_LOGE(TAGTLS," failed ! mbedtls_ssl_write returned %d", ret);
		    err = ret;
		    break;
		}
	    }
	    print_msg(TAGTLS, NULL, buf, 1);
	    if (!auth) break;
	    else {
		if (eot) break;
	    }
	    vTaskDelay(20 / portTICK_RATE_MS);
	}//while (!tls_hangup...)

#ifdef SET_MQTT
	if (auth) {
	    mult = 10;//get status
	    if (xQueueSend(mqtt_evt_queue, (void *)&mult, (TickType_t)0) != pdPASS) {
		ESP_LOGE(TAGTLS, "Error while sending to mqtt_evt_queue.");
	    }
	}
#endif

exit1:
        auth = 0;
        tls_client_ip = 0;
        mbedtls_ssl_close_notify(&ssl);

exit:
        mbedtls_ssl_session_reset(&ssl);
        mbedtls_net_free(&client_ctx);
        mbedtls_net_free(&server_ctx);

        if (err) {
            memset(stx, 0, BUF_SIZE);
            mbedtls_strerror(err, stx, BUF_SIZE-1);
            ESP_LOGE(TAGTLS,"Last error %d  (%s)", err, stx);
            err = 0;
        }
        if (eot) break;
        timeout = timeout_auth;//25000 msec
        mbedtls_ssl_conf_read_timeout(&conf, timeout);

    }

quit1:

    if (stx) free(stx);
    if (buf) free(buf);

    ets_printf("%s[%s] TLS server task stop | FreeMem=%u%s\n", START_COLOR, TAGTLS, xPortGetFreeHeapSize(), STOP_COLOR);
    if (total_task) total_task--;
    tls_start = 0;
    vTaskDelete(NULL);

}
//******************************************************************************************

#endif
