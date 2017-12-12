
#include "at_cmd.h"

#undef PRN_DUMP

#define wait_ack_def 3000
#define max_at_len 16


typedef struct {
    char *cmd;
    TickType_t wait;
} s_at_cmd;

#pragma pack(push,1)
typedef struct {
    unsigned char hpv; //0..255
    unsigned short fsv;//0..65535
    char syncw[16];    //0xXX........

    unsigned syncl:4;  //0..8
    unsigned power:3;  //0..7 //0—20dbm,  1—17dbm, 2—15dbm, 3—10dbm, 4-?, 5—8dbm, 6—5dbm, 7—2dbm
    unsigned crc:1;    //0..1

    unsigned chan:4;   //0..F — 0..15 channel
    unsigned bandw:4;  //6—62.5KHZ, 7—125KHZ, 8—250KHZ, 9—500KHZ

    unsigned hfss:1;   //0..1
    unsigned plen:7;   //1..127

    unsigned sf:4;     //7—SF=7, 8—SF=8, 9—SF=9, A—SF=10, B—SF=11, C—SF=12
    unsigned crcode:2; //0—CR4/5, 1—CR4/6, 2—CR4/7, 3—CR4/8
    unsigned mode:2;   //0-LoRa, 1-OOK, 2-FSK, 3-GFSK

    unsigned freq:2;   //0-434MHZ Band, 1-470MHZ Band, 2-868MHZ Band, 3-915MHZ Band
    unsigned spr:4;    //0-1200bps 1-2400bps 2-4800bps 3-9600bps 4-14400bps 5-19200bps 6-38400bps 7-56000bps 8-57600bps 9-115200bps
    unsigned spc:2;    //0..2  :  0-none 1-even 2-odd
} s_lora_stat;
#pragma pack(pop)

s_lora_stat lora_stat;

const char *lora_uspeed[] = {"1200","2400","4800","9600","14400","19200","38400","56000","57600","115200"};//0..9
const char *lora_ucheck[] = {"none","even","odd"};//0,1,2
const char *lora_power[] = {"20dbm","17dbm","15dbm","10dbm","??dbm","8dbm","5dbm","2dbm"};//0..7
const char *lora_crcode[] = {"CR4/5","CR4/6","CR4/7","CR4/8"};//0..3
const char *lora_bandw[] = {"62.5KHZ","125KHZ","250KHZ","500KHZ"};//0..3
const char *lora_main_mode[] = {"LoRa","OOK","FSK","GFSK"};//0..3
const char *lora_freq[] = {"434MHZ","470MHZ","868MHZ","915MHZ"};//0..3
const char *lora_hopping[] = {"disable","enable"};//0,1

s_at_cmd at_cmd[] = {
    {//0
        .cmd = "AT",
        .wait = 5000,
    },
    {//1//set speed //0-1200bps 1-2400bps 2-4800bps 3-9600bps 4-14400bps 5-19200bps 6-38400bps 7-56000bps 8-57600bps 9-115200bps
        .cmd = "AT+SPR=",
        .wait = wait_ack_def,
    },
    {//2//set Serial Port Check // 0-none 1-even 2-odd
        .cmd = "AT+SPC=",
        .wait = wait_ack_def,
    },
    {//3//set POWER to 20dbm //0—20dbm, 1—17dbm, 2—15dbm, 3—10dbm, 5—8dbm, 6—5dbm, 7—2dbm
        .cmd = "AT+POWER=",
        .wait = wait_ack_def,
    },
    {//4//set Channel Select to 10 //0..F — 0..15 channel
        .cmd = "AT+CS=",
        .wait = wait_ack_def,
    },
    {//5//set // Less than 16 characters Used 0..9,A..F // for example: AT+SYNW=1234ABEF\r\n (if sync word is 0x12,0x34,0xAB,0xEF)
        .cmd = "AT+SYNW=",
        .wait = wait_ack_def,
    },
    {//6//set sync word len // 0..8
        .cmd = "AT+SYNL=",
        .wait = wait_ack_def,
    },
    {//7//In FSK mode. The node function can be set.//AT+NODE=n,m -> n: 0—disable, 1—enable; mode: 0—only match NID, 1-match NID and BID
        .cmd = "AT+NODE=",
        .wait = wait_ack_def,
    },
    {//8//In FSK mode. The node ID can be set 0..255
        .cmd = "AT+NID=",
        .wait = wait_ack_def,
    },
    {//9//In FSK mode, AT+BID can be set 0..255
        .cmd = "AT+BID=",
        .wait = wait_ack_def,
    },
    {//10//In LoRA mode,CRC Function enable or disable //0‐disabe CRC function, 1‐enable CRC function
        .cmd = "AT+LRCRC=",
        .wait = wait_ack_def,
    },
    {//11//In LoRa mode, According to the demand to set signal band width. //6—62.5KHZ, 7—125KHZ, 8—250KHZ, 9—500KHZ
        .cmd = "AT+LRSBW=",
        .wait = wait_ack_def,
    },
    {//12//In Lora mode set spreading factor //7—SF=7, A—SF=10, 8—SF=8, B—SF=11, 9—SF=9, C—SF=12
        .cmd = "AT+LRSF=",
        .wait = wait_ack_def,
    },
    {//13//In LoRa mode.Data transfer adopt Forward Error Correction Code, this command is choose it Coding Rate.//0—CR4/5, 1—CR4/6, 2—CR4/7, 3—CR4/8
        .cmd = "AT+LRCR=",
        .wait = wait_ack_def,
    },
    {//14//In LoRa mode, module has FHSS function.//0‐disable HFSS function, 1‐enable HFSS function,note: if SBW=500 and SF=7,HFSS function disable
        .cmd = "AT+LRHF=",
        .wait = wait_ack_def,
    },
    {//15//In LoRa mode.Data transmission in the form of subcontract, this command to set the length of data packet.//1..127 (>16)
        .cmd = "AT+LRPL=",
        .wait = wait_ack_def,
    },
    {//16//In LoRa mode, the hopping period value can be set. //0..255 (>5)
        .cmd = "AT+LRHPV=",
        .wait = wait_ack_def,
    },
    {//17//In Lora mode, Frequency Step Value can be set.//0..65535
        .cmd = "AT+LRFSV=",
        .wait = wait_ack_def,
    },
    {//18//Set mode. The modulation of HM-TRLR can be changed,//0-LoRa, 1-OOK, 2-FSK, 3-GFSK, In the OOK mode，baudrate no more than 9600 bps
        .cmd = "AT+MODE=",
        .wait = wait_ack_def,
    },
    {//19//Frequence band can be changed.//0--434MHZ Band, 1--470MHZ Band, 2--868MHZ Band, 3--915MHZ Band
        .cmd = "AT+BAND=",
        .wait = wait_ack_def,
    },
    {//20//save profile to dataflash
        .cmd = "AT&V",
        .wait = 5000,
    },
    {//21//save profile to dataflash
        .cmd = "AT&W",
        .wait = wait_ack_def,
    }
};
#define TotalCmd  ((sizeof(at_cmd) / sizeof(s_at_cmd)) - 1)


void put_at_value(uint8_t ind, char *uack)
{
    if ( (ind > TotalCmd) || (!uack) ) return;

    char cd[max_at_len] = {0};
    memcpy(cd, &at_cmd[ind].cmd[2], strlen(at_cmd[ind].cmd) - 2);
    int dl = strlen(cd); if (dl <= 0) return;

    uint8_t bt;
    bool fl;
    int val;
    char *uk = NULL;
    uk = strchr(cd, '='); if (uk) *uk = ':';

    switch (ind) {
	case 1://AT+SPR
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		if ( (bt >= 0x30) && (bt <= 0x39) ) {
		    bt -= 0x30;
		    lora_stat.spr = bt;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' uspeed=%u(%sbps)\n", ind, at_cmd[ind].cmd, lora_stat.spr, lora_uspeed[lora_stat.spr]);
#endif
		}
	    }
	break;
	case 2://AT+SPC
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		if ( (bt >= 0x30) && (bt <= 0x32) ) {
		    bt -= 0x30;
		    lora_stat.spc = bt;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' ucheck=%u(%s)\n", ind, at_cmd[ind].cmd, lora_stat.spc, lora_ucheck[lora_stat.spc]);
#endif
		}
	    }
	break;
	case 3://AT+POWER
	    uk = strstr(uack, cd);
	    if (uk) {
		val = atoi(uk + dl);
		if ( (val >= 0) && (val <= 7) ) lora_stat.power = val;
#ifdef PRN_DUMP
		printf("cmd=%u '%s' power=%u(%s)\n", ind, at_cmd[ind].cmd, lora_stat.power, lora_power[lora_stat.power]);
#endif
	    }
	break;
	case 4://AT+CS
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		fl = false;
		bt = *uk;
		if ( (bt >= 0x30) && (bt <= 0x39) ) {
		    bt -= 0x30; fl = true;
		} else if ( (bt >= 0x41) && (bt <= 0x46) ) {
		    bt -= 0x37; fl = true;
		}
		if (fl) {
		    lora_stat.chan = bt;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' chan=%u\n", ind, at_cmd[ind].cmd, lora_stat.chan);
#endif
		}
	    }
	break;
	case 5://AT+SYNW=1234ABEF\r\n (if sync word is 0x12,0x34,0xAB,0xEF)
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		memset(lora_stat.syncw, 0, 16);
		char *uke = strstr(uk,"\r\n"); if (!uke) uke = strchr(uk,'\0');
		if (uke) {
		    val = uke - uk; if (val > 16) val = 16;
		    memset(&lora_stat.syncw[0], 0, 16);
		    memcpy(&lora_stat.syncw[0], uk, val);
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' syncw=%.*s\n", ind, at_cmd[ind].cmd, 16, lora_stat.syncw);
#endif
		}
	    }
	break;
	case 6://AT+SYNL=//6//set sync word len // 0..8
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		if ( (bt >= 0x30) && (bt <= 0x38) ) {
		    lora_stat.syncl = bt - 0x30;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' syncl=%u\n", ind, at_cmd[ind].cmd, lora_stat.syncl);
#endif
		}
	    }
	break;
	case 10://AT+LRCRC= //0‐disabe CRC function, 1‐enable CRC function
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		if ( (bt >= 0x30) && (bt <= 0x31) ) {
		    lora_stat.crc = bt - 0x30;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' crc=%u\n", ind, at_cmd[ind].cmd, lora_stat.crc);
#endif
		}
	    }
	break;
	case 11://AT+LRSBW= //6—62.5KHZ, 7—125KHZ, 8—250KHZ, 9—500KHZ
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		if ( (bt >= 0x36) && (bt <= 0x39) ) {
		    lora_stat.bandw = bt - 0x30;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' bandw=%u(%s)\n", ind, at_cmd[ind].cmd, lora_stat.bandw, lora_bandw[lora_stat.bandw - 6]);
#endif
		}
	    }
	break;
	case 12://AT+LRSF= //uint8_t sf;// 7—SF=7, 8—SF=8, 9—SF=9, A—SF=10, B—SF=11, C—SF=12
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		fl = false;
		if ( (bt >= 0x37) && (bt <= 0x39) ) {
		    bt -= 0x30; fl = true;
		} else if ( (bt >= 0x41) && (bt <= 0x43) ) {
		    bt -= 0x37; fl = true;
		}
		if (fl) {
		    lora_stat.sf = bt;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' sf=%u\n", ind, at_cmd[ind].cmd, lora_stat.sf);
#endif
		}
	    }
	break;
	case 13://AT+LRCR= //0—CR4/5, 1—CR4/6, 2—CR4/7, 3—CR4/8
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		if ( (bt >= 0x30) && (bt <= 0x33) ) {
		    lora_stat.crcode = bt - 0x30;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' crcode=%u(%s)\n", ind, at_cmd[ind].cmd, lora_stat.crcode, lora_crcode[lora_stat.crcode]);
#endif
		}
	    }
	break;
	case 14://AT+LRHF= //0‐disable HFSS function, 1‐enable HFSS function
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		if ( (bt >= 0x30) && (bt <= 0x31) ) {
		    lora_stat.hfss = bt - 0x30;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' hfss=%u\n", ind, at_cmd[ind].cmd, lora_stat.hfss);
#endif
		}
	    }
	break;
	case 15://AT+LRPL= //this command to set the length of data packet.//1..127 (>16)
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		val = atoi(uk);
		lora_stat.plen = val;
#ifdef PRN_DUMP
		printf("cmd=%u '%s' plen=%u\n", ind, at_cmd[ind].cmd, lora_stat.plen);
#endif
	    }
	break;
	case 16://AT+LRHPV= //In LoRa mode, the hopping period value can be set. //0..255 (>5)
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		val = atoi(uk);
		lora_stat.hpv = val;
#ifdef PRN_DUMP
		printf("cmd=%u '%s' hpv=%u\n", ind, at_cmd[ind].cmd, lora_stat.hpv);
#endif
	    }
        break;
        case 17://AT+LRFSV= //Frequency Step Value can be set.//0..65535
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		val = atoi(uk);
		lora_stat.fsv = val;
#ifdef PRN_DUMP
		printf("cmd=%u '%s' fsv=%u\n", ind, at_cmd[ind].cmd, lora_stat.fsv);
#endif
	    }
        break;
        case 18://AT+MODE= //0-LoRa, 1-OOK, 2-FSK, 3-GFSK, In the OOK mode，baudrate no more than 9600 bps
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		if ( (bt >= 0x30) && (bt <= 0x33) ) {
		    lora_stat.mode = bt - 0x30;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' mode=%u(%s)\n", ind, at_cmd[ind].cmd, lora_stat.mode, lora_main_mode[lora_stat.mode]);
#endif
		}
	    }
        break;
        case 19://AT+BAND= //0--434MHZ Band, 1--470MHZ Band, 2--868MHZ Band, 3--915MHZ Band
	    uk = strstr(uack, cd);
	    if (uk) {
		uk += dl;
		bt = *uk;
		if ( (bt >= 0x30) && (bt <= 0x33) ) {
		    lora_stat.freq = bt - 0x30;
#ifdef PRN_DUMP
		    printf("cmd=%u '%s' freq=%u(%s)\n", ind, at_cmd[ind].cmd, lora_stat.freq, lora_freq[lora_stat.freq]);
#endif
		}
	    }
        break;
    }

}

/*
HM‐TRLR‐S‐868 Module default setting

Command		default		remark
AT+SPR=n 	n=3 		Baud rate 9600pbs
AT+SPC=n 	n=0 		None check
AT+POWER=n 	n=0 		Power 20dbm
AT+CS=n 	n=A 		868MHz
AT+SYNL=n 	n=6 		6 bytes
AT+NODE=n,mode 	n=0,mode=0 	Disable ID Node function
AT+LRCRC=n 	n=1 		Lora mode,CRC enable
AT+LRSBW=n 	n=7 		SBW = 125KHz
AT+LRSF=n 	n=9 		SF = 9
AT+LRCR=n 	n=0 		CodeRate=4/5
AT+LRHF=n 	n=0 		FHSS is disable
AT+LRPL=n 	n=32 		Package lenth 32bytes
AT+LRHPV=n 	n=10 		Hopping period
AT+LRFSV=n 	n=1638 		Frequence step 100KHz
AT+MODE=n 	n=0 		LoRa mode
AT+BAND=n 	n=2 		868MHz band
*/
