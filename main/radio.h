#ifndef __RADIO_H__
#define __RADIO_H__

#undef PRN_SET

#pragma pack(push,1)
    typedef struct {
	uint32_t dev_id;
	s_led_cmd color;
    } s_radio_one;
#pragma pack(pop)

#pragma pack(push,1)
    typedef struct self_rec {
	s_radio_one one;
	struct self_rec *before;
	struct self_rec *next;
    } s_radio;
#pragma pack(pop)

#pragma pack(push,1)
    typedef struct {
	struct self_rec *first;
	struct self_rec *end;
	unsigned int counter;
    } s_radio_hdr;
#pragma pack(pop)

#pragma pack(push,1)
    typedef struct {
	s_radio_one one;
	int flag;
    } s_radio_cmd;
#pragma pack(pop)

#pragma pack(push,1)
    typedef struct {
	s_radio_cmd cmd;
	time_t time;
	uint32_t mem;
	float temp;
	uint32_t vcc;
    } s_radio_stat;
#pragma pack(pop)

extern const char *TAGR;

extern s_radio_hdr radio_hdr;
extern xSemaphoreHandle radio_mutex;

extern s_radio *add_radio(void *one);
extern int del_radio(s_radio *rcd, bool withlock);
extern s_radio *find_radio(uint32_t id);
extern int isMaster(s_radio *rcd);
extern void remove_radio();
extern void init_radio();
extern int total_radio();
extern s_radio *get_radio(s_radio *last);
extern s_radio_one *get_radio_from_list(void *src);
extern void update_radio_color(s_radio *adr, s_led_cmd *newc);
extern void radio_task(void *arg);

#endif
