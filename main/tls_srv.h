#ifndef __TLS_SRV_H__
#define __TLS_SRV_H__

#include "hdr.h"

#ifdef SET_TLS_SRV

    #undef SET_MD5

    #ifndef SET_MD5
        #include "hwcrypto/sha.h"
        #define SET_SHA1
        //#define SET_SHA2_256
        //#define SET_SHA2_384
        //#define SET_SHA2_512
    #endif

    #include "mbedtls/config.h"
    #include "mbedtls/net.h"
    #include "mbedtls/debug.h"
    #include "mbedtls/ssl.h"
    #include "mbedtls/entropy.h"
    #include "mbedtls/ctr_drbg.h"
    #include "mbedtls/error.h"
    #include "mbedtls/certs.h"
    #include "mbedtls/md5.h"

    #define def_idle_count 60
    #define timeout_auth 30000
    #define timeout_def 10000

    #pragma pack(push,1)
    typedef struct {
        unsigned first : 1;
        unsigned first_send : 1;
        unsigned none : 6;
    } s_tls_flags;
    #pragma pack(pop)

    extern const char *TAGTLS;
    extern uint8_t tls_start;
    extern uint8_t tls_hangup;
    extern time_t mk_hash(char *out, const char *part);
    extern void tls_task(void *arg);
#endif

#endif
