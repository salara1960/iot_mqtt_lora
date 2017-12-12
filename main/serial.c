#include "hdr.h"

#ifdef SET_SERIAL

#include "at_cmd.c"

const char *TAG_UART = "UART";
const int unum = UART_NUMBER;
const int uspeed = UART_SPEED;
const int BSIZE = 128;
s_pctrl pctrl = {0, 0, 1, 1, 0};

//******************************************************************************************

void serial_init()
{
    uart_config_t uart_conf = {
        .baud_rate = uspeed,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    uart_param_config(unum, &uart_conf);

    uart_set_pin(unum, GPIO_U2TXD, GPIO_U2RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);//Set UART2 pins(TX, RX, RTS, CTS)

    uart_driver_install(unum, BSIZE << 1, 0, 0, NULL, 0);
}
//-----------------------------------------------------------------------------------------
void lora_init()
{
    gpio_pad_select_gpio(U2_CONFIG); gpio_pad_pulldown(U2_CONFIG); gpio_set_direction(U2_CONFIG, GPIO_MODE_OUTPUT);
    gpio_set_level(U2_CONFIG, pctrl.config);//0 //set configure mode - at_command
//    gpio_pad_select_gpio(U2_SLEEP); /*gpio_pad_pulldown(U2_SLEEP);*/ gpio_set_direction(U2_SLEEP, GPIO_MODE_OUTPUT);
//    gpio_set_level(U2_SLEEP, pctrl.sleep);//0 //set no sleep
    gpio_pad_select_gpio(U2_RESET); gpio_pad_pullup(U2_RESET); gpio_set_direction(U2_RESET, GPIO_MODE_OUTPUT);
    gpio_set_level(U2_RESET, pctrl.reset);//1 //set no reset
    gpio_pad_select_gpio(U2_STATUS); gpio_pad_pullup(U2_STATUS); gpio_set_direction(U2_RESET, GPIO_MODE_INPUT);
    pctrl.status = gpio_get_level(U2_STATUS);
}
//-----------------------------------------------------------------------------------------
void lora_reset()
{
    if (!(pctrl.status = gpio_get_level(U2_STATUS))) vTaskDelay(20 / portTICK_RATE_MS);//wait status=high (20 ms)

    pctrl.reset = 0; gpio_set_level(U2_RESET, pctrl.reset);//set reset
    vTaskDelay(2 / portTICK_RATE_MS);
    pctrl.reset = 1; gpio_set_level(U2_RESET, pctrl.reset);//set no reset
}
//-----------------------------------------------------------------------------------------
void lora_data_mode(bool cnf)//true - data mode, false - at_command mode
{
    if (cnf) pctrl.config = 1; else pctrl.config = 0;
    gpio_set_level(U2_CONFIG, pctrl.config);//0 - configure mode (at_command), 1 - data transfer mode
}
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
bool lora_at_mode()//true - at_command mode, false - data mode
{
    return (pctrl.config);
}
//-----------------------------------------------------------------------------------------
bool lora_check_status()
{
    pctrl.status = gpio_get_level(U2_STATUS);

    return (bool)pctrl.status;
}
//-----------------------------------------------------------------------------------------
/*
bool lora_sleep_mode(bool slp)//true - sleep mode, false - normal mode
{
    if (slp) {
	if (pctrl.sleep) return (bool)pctrl.sleep;//already sleep mode
	pctrl.sleep = 1;
    } else {
	if (!pctrl.sleep) return (bool)pctrl.sleep;//already normal mode
	pctrl.sleep = 0;
    }
    gpio_set_level(U2_SLEEP, pctrl.sleep);

    if (!pctrl.sleep) {
	uint8_t sch = 12;
	while ( sch-- && !(pctrl.status = gpio_get_level(U2_STATUS)) ) vTaskDelay(1 / portTICK_RATE_MS);
    }

    return (bool)pctrl.sleep;
}
//-----------------------------------------------------------------------------------------
bool lora_check_sleep()
{
    return (bool)pctrl.sleep;
}
*/
//-----------------------------------------------------------------------------------------
void serial_task(void *arg)
{
total_task++;

    ets_printf("%s[%s] Start serial_task (%d) | FreeMem %u%s\n", START_COLOR, TAG_UART, TotalCmd, xPortGetFreeHeapSize(), STOP_COLOR);

    char *data = (char *)calloc(1, BSIZE + 1);
    if (data) {
	int bs2 = (BSIZE << 2) + 32;
	char cmds[bs2];
	lora_init();
	vTaskDelay(25 / portTICK_RATE_MS);
	lora_reset();
	vTaskDelay(50 / portTICK_RATE_MS);
	uint32_t len = 0, dl = 0;
	uint8_t buf = 0, allcmd = 0, rd_done = 0, mode = false;//false - at_command
	TickType_t tms = 0, tmrecv = 0;
	memset(&lora_stat, 0, sizeof(s_lora_stat));

	while (!restart_flag) {

	    while (allcmd < TotalCmd) {//at command loop

		if (!allcmd) ets_printf("%s[%s] Wait init lora module...%s\n", GREEN_COLOR, TAG_UART, STOP_COLOR);

		memset(cmds, 0, bs2);
		sprintf(cmds, "%s", at_cmd[allcmd].cmd);

		if (!strcmp(cmds, "AT+LRSF=")) {//Spreading Factor
		    lora_stat.sf = 11;//12
		    sprintf(cmds+strlen(cmds),"%X", lora_stat.sf);//7—SF=7, 8—SF=8, 9—SF=9, A—SF=10, B—SF=11, C—SF=12
		}
		else
		if (!strcmp(cmds, "AT+LRPL=")) {// PackLen
		    lora_stat.plen = 80;
		    sprintf(cmds+strlen(cmds), "%d", lora_stat.plen);//1..127
		}
		else
		if (!strcmp(cmds, "AT+CS=")) {
		    lora_stat.chan = 10;
		    sprintf(cmds+strlen(cmds), "%X", lora_stat.chan);//B//set Channel Select to 10 //0..F — 0..15 channel
		}
		else
		//if (!strcmp(cmds, "AT+LRSBW=")) sprintf(cmds+strlen(cmds),"7");//6-62.5, 7-125, 8-250, 9-500
		//else
		//if (!strcmp(cmds, "AT+SYNW=")) sprintf(cmds+strlen(cmds),"%08X", cli_id);//AT+SYNW=1234ABEF\r\n (if sync word is 0x12,0x34,0xAB,0xEF)
		//else
		//if (!strcmp(cmds, "AT+SYNL=")) sprintf(cmds+strlen(cmds),"%u", sizeof(cli_id));//set sync word len // 0..8
		//else
//		if (!strcmp(cmds, "AT+LRHF=")) {
//		    sprintf(cmds+strlen(cmds),"1");//0,1
//		    lora_stat.hfss = 1;
//		}
//		else if (!strcmp(cmds, "AT+LRHPV=")) sprintf(cmds+strlen(cmds),"10");//0..255
//		else
//		if (!strcmp(cmds, "AT+LRFSV=")) sprintf(cmds+strlen(cmds),"819");//0..65535  //819 - 50KHz (1638 - 100KHz)
//		else
//		if (!strcmp(cmds, "AT+POWER=")) {
//		    sprintf(cmds+strlen(cmds),"0");//2//set POWER to //0—20dbm 5—8dbm, 1—17dbm 6—5dbm, 2—15dbm 7—2dbm, 3—10dbm
//		    lora_stat.power = 0;
//		}
//		else 
		if (strchr(cmds,'=')) sprintf(cmds+strlen(cmds),"?");

		sprintf(cmds+strlen(cmds),"\r\n");
		dl = strlen(cmds);
#ifdef PRINT_AT
		ets_printf("%s%s%s", BROWN_COLOR, cmds, STOP_COLOR);
#endif
		uart_write_bytes(unum, cmds, dl);//send at_command
		tms = get_tmr(at_cmd[allcmd].wait);
		rd_done = 0; len = 0; memset(data, 0, BSIZE);
		while ( !rd_done && !check_tmr(tms) ) {
		    if (uart_read_bytes(unum, &buf, 1, (TickType_t)25) == 1) {
			data[len++] = buf;
			if ( (strstr(data, "\r\n")) || (len >= BSIZE - 2) ) {
			    if (strstr(data, "ERROR:"))
				ets_printf("%s%s%s", RED_COLOR, data, STOP_COLOR);
			    else {
#ifdef PRINT_AT
				ets_printf("%s", data);
#endif
				if (data[0] == '+') put_at_value(allcmd, data);
			    }
			    rd_done = 1;
			}
		    }
		}
		allcmd++;
		if (restart_flag) break;
		vTaskDelay(10 / portTICK_RATE_MS);//250
	    }//while (allcmd < TotalCmd)

	    if (!mode) {//at_command mode
		if (lora_stat.plen) ets_printf("%s[%s] Freq=%s Mode=%s Hopping=%s Power=%s Channel=%u BandW=%s SF=%u PackLen=%u%s\n",
						GREEN_COLOR, TAG_UART,
						lora_freq[lora_stat.freq],
						lora_main_mode[lora_stat.mode],
						lora_hopping[lora_stat.hfss],
						lora_power[lora_stat.power],
						lora_stat.chan,
						lora_bandw[lora_stat.bandw - 6],
						lora_stat.sf,
						lora_stat.plen,
						STOP_COLOR);
		mode = true;
		lora_data_mode(mode);//set data mode
		vTaskDelay(15 / portTICK_RATE_MS);
		ets_printf("%s[%s] Switch from at_command to data tx/rx mode, listen air...%s\n", MAGENTA_COLOR, TAG_UART, STOP_COLOR);
		len = 0; memset(data, 0, BSIZE);
	    }

	    if (uart_read_bytes(unum, &buf, 1, (TickType_t)0) == 1) {

//		ets_printf("%s[%s] 0x%02X%s\n", MAGENTA_COLOR, TAG_UART, buf, STOP_COLOR);

		data[len++] = buf;
		if (len == 1) tmrecv = get_tmr(TICK_CNT_WAIT);//5000
		if ( (strchr(data, '\n')) || (len >= BSIZE - 2) ) {
		    if (!strchr(data,'\n')) sprintf(data,"\n");
		    ets_printf("%s[%s] Recv : %s%s", MAGENTA_COLOR, TAG_UART, data, STOP_COLOR);

		    memset(cmds, 0, bs2);
		    if (strstr(data, "TS"))
			dl = sprintf(cmds,"DevID:%08X TS[%u:%s] : %s", cli_id, (uint32_t)time(NULL), time_zone, data);
		    else
			dl = sprintf(cmds,"DevID:%08X : %s", cli_id, data);
		    uart_write_bytes(unum, cmds, dl);
		    ets_printf("%s[%s] Return : %s%s", BROWN_COLOR, TAG_UART, cmds, STOP_COLOR);

		    len = 0; memset(data, 0, BSIZE);
		    tmrecv = get_tmr(TICK_CNT_WAIT);
		} else {
		    if ((len) && (check_tmr(tmrecv))) {
			memset(cmds, 0, bs2);
			sprintf(cmds, "%s[%s] Recv:", WHITE_COLOR, TAG_UART);
			for (int i=0; i<len; i++) sprintf(cmds+strlen(cmds)," %02X", data[i]);
			sprintf(cmds+strlen(cmds), "%s\n", STOP_COLOR);
			ets_printf(cmds);
			len = 0; memset(data, 0, BSIZE);
			tmrecv = get_tmr(TICK_CNT_WAIT);
		    }
		}
	    }

	}

	vTaskDelay(500 / portTICK_RATE_MS);
	free(data);

    }

    ets_printf("%s[%s] Stop serial_task | FreeMem %u%s\n", START_COLOR, TAG_UART, xPortGetFreeHeapSize(), STOP_COLOR);
    if (total_task) total_task--;

    vTaskDelete(NULL);
}

//******************************************************************************************

#endif
