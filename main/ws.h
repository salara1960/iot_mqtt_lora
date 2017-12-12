#ifndef __WS_H__
#define __WS_H__

#include "hdr.h"

#ifdef SET_WS

    #undef WS_PRN

    #include "mqtt.h"
    #include "esp_heap_caps.h"
    #include "hwcrypto/sha.h"
    #include "wpa2/utils/base64.h"

    #define WS_MASK_L		0x4	// \brief Length of MASK field in WebSocket Header

    #define WS_PORT		8899
    #define WAIT_DATA_WS        5000
    #define WS_CLIENT_KEY_L	24	//< \brief Length of the Client Key
    #define SHA1_RES_L		20	//< \brief SHA1 result
    #define WS_STD_LEN		125	//< \brief Maximum Length of standard length frames //125
    #define WS_SPRINTF_ARG_L	4	//< \brief Length of sprintf argument for string (%.*s)
    #define WS_STD_LEN2		126
    #define MAX_FRAME_LEN	0xffff


    // Opcode according to RFC 6455
    typedef enum {
	WS_OP_CON = 0x0, // Continuation Frame
	WS_OP_TXT = 0x1, // Text Frame
	WS_OP_BIN = 0x2, // Binary Frame
	WS_OP_CLS = 0x8, // Connection Close Frame
	WS_OP_PIN = 0x9, // Ping Frame
	WS_OP_PON = 0xa  // Pong Frame
    } WS_OPCODES;

    #pragma pack(push,1)
    typedef struct {
	uint8_t opcode         : WS_MASK_L;
	uint8_t reserved       : 3;
	uint8_t FIN            : 1;
	uint8_t payload_length : 7;
	uint8_t mask           : 1;
    } WS_frame_header_t;
    #pragma pack(pop)

    extern const char *TAGWS;
    extern uint8_t ws_start;

    extern void ws_task(void *arg);
#endif

#endif

