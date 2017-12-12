#include "mqtt.h"


#define DIVIDER		4 /* Above 4, timings start to deviate*/
#define DURATION	12.5 /* minimum time of a single RMT duration in nanoseconds based on clock */

#define PULSE_T0H	(  350 / (DURATION * DIVIDER));
#define PULSE_T1H	(  900 / (DURATION * DIVIDER));
#define PULSE_T0L	(  900 / (DURATION * DIVIDER));
#define PULSE_T1L	(  350 / (DURATION * DIVIDER));
#define PULSE_TRS	(50000 / (DURATION * DIVIDER));

#define MAX_PULSES	32

#define RMTCHANNEL	0
//----------------------------------------------------------------------------
typedef union {
    struct {
	uint32_t duration0:15;
	uint32_t level0:1;
	uint32_t duration1:15;
	uint32_t level1:1;
    };
    uint32_t val;
} rmtPulsePair;

typedef union {
    struct __attribute__ ((packed)) {
    	uint8_t r, g, b;
    };
    uint32_t num;
} rgbVal;
//----------------------------------------------------------------------------
static uint8_t *wbuffer = NULL;
static unsigned int wpos, wlen, whalf;
static xSemaphoreHandle wsem = NULL;
//static intr_handle_t rmt_intr_handle = NULL;
static rmtPulsePair wbits[2];
//----------------------------------------------------------------------------
inline rgbVal makeRGBVal(uint8_t r, uint8_t g, uint8_t b)
{
rgbVal v;

    v.r = r;
    v.g = g;
    v.b = b;

    return v;
}
//----------------------------------------------------------------------------
void ws2812_initRMTChannel(int rmtChannel)
{
    RMT.apb_conf.fifo_mask = 1;  //enable memory access, instead of FIFO mode.
    RMT.apb_conf.mem_tx_wrap_en = 1; //wrap around when hitting end of buffer
    RMT.conf_ch[rmtChannel].conf0.div_cnt = DIVIDER;
    RMT.conf_ch[rmtChannel].conf0.mem_size = 1;
    RMT.conf_ch[rmtChannel].conf0.carrier_en = 0;
    RMT.conf_ch[rmtChannel].conf0.carrier_out_lv = 1;
    RMT.conf_ch[rmtChannel].conf0.mem_pd = 0;

    RMT.conf_ch[rmtChannel].conf1.rx_en = 0;
    RMT.conf_ch[rmtChannel].conf1.mem_owner = 0;
    RMT.conf_ch[rmtChannel].conf1.tx_conti_mode = 0;    //loop back mode.
    RMT.conf_ch[rmtChannel].conf1.ref_always_on = 1;    // use apb clock: 80M
    RMT.conf_ch[rmtChannel].conf1.idle_out_en = 1;
    RMT.conf_ch[rmtChannel].conf1.idle_out_lv = 0;

}
//----------------------------------------------------------------------------
void ws2812_copy()
{
unsigned int i, j, offset, len, bit;


    offset = whalf * MAX_PULSES;
    whalf = !whalf;

    len = wlen - wpos;
    if (len > (MAX_PULSES / 8)) len = (MAX_PULSES / 8);

    if (!len) {
	for (i = 0; i < MAX_PULSES; i++) RMTMEM.chan[RMTCHANNEL].data32[i + offset].val = 0;
	return;
    }

    for (i = 0; i < len; i++) {
	bit = wbuffer[i + wpos];
	for (j = 0; j < 8; j++, bit <<= 1) {
	    RMTMEM.chan[RMTCHANNEL].data32[j + i * 8 + offset].val = wbits[(bit >> 7) & 0x01].val;
	}
	if (i + wpos == wlen - 1) RMTMEM.chan[RMTCHANNEL].data32[7 + i * 8 + offset].duration1 = PULSE_TRS;
    }

    for (i *= 8; i < MAX_PULSES; i++) RMTMEM.chan[RMTCHANNEL].data32[i + offset].val = 0;

    wpos += len;

}
//----------------------------------------------------------------------------
void ws2812_handleInterrupt(void *arg)
{
portBASE_TYPE taskAwoken = 0;

    if (RMT.int_st.ch0_tx_thr_event) {
        ws2812_copy();
        RMT.int_clr.ch0_tx_thr_event = 1;
    } else if (RMT.int_st.ch0_tx_end && wsem) {
        xSemaphoreGiveFromISR(wsem, &taskAwoken);
        RMT.int_clr.ch0_tx_end = 1;
    }

}
//----------------------------------------------------------------------------
esp_err_t ws2812_init(int gpioNum)
{
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_RMT_CLK_EN);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_RMT_RST);

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpioNum], 2);
    gpio_set_direction(gpioNum, GPIO_MODE_OUTPUT);
    gpio_matrix_out(gpioNum, RMT_SIG_OUT0_IDX + RMTCHANNEL, 0, 0);

    ws2812_initRMTChannel(RMTCHANNEL);

    RMT.tx_lim_ch[RMTCHANNEL].limit = MAX_PULSES;
    RMT.int_ena.ch0_tx_thr_event = 1;
    RMT.int_ena.ch0_tx_end = 1;

    wbits[0].level0 = 1;
    wbits[0].level1 = 0;
    wbits[0].duration0 = PULSE_T0H;
    wbits[0].duration1 = PULSE_T0L;
    wbits[1].level0 = 1;
    wbits[1].level1 = 0;
    wbits[1].duration0 = PULSE_T1H;
    wbits[1].duration1 = PULSE_T1L;

    return esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, ws2812_handleInterrupt, NULL, NULL);//&rmt_intr_handle);
}
//----------------------------------------------------------------------------
void ws2812_setColors(unsigned int length, rgbVal *array)
{

    wlen = (length * 3);
    wbuffer = malloc(wlen);

    if (wbuffer) {
	for (unsigned int i = 0; i < length; i++) {
	    wbuffer[0 + i * 3] = array[i].g;
	    wbuffer[1 + i * 3] = array[i].r;
	    wbuffer[2 + i * 3] = array[i].b;
	}

	wpos = whalf = 0;

	ws2812_copy();

	if (wpos < wlen) ws2812_copy();

	RMT.conf_ch[RMTCHANNEL].conf1.mem_rd_rst = 1;
	RMT.conf_ch[RMTCHANNEL].conf1.tx_start = 1;

	wsem = xSemaphoreCreateBinary();
	xSemaphoreTake(wsem, portMAX_DELAY);
	vSemaphoreDelete(wsem);
	wsem = NULL;

	free(wbuffer);
    }

    return;
}
