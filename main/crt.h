#ifndef __CRT_H__
#define __CRT_H__

extern const char ca_crt[] asm("_binary_ca_crt_start");
//extern const char ca_crt_end[] asm("_binary_ca_crt_end");

extern const char server_cert[] asm("_binary_cacert_pem_start");
extern const char server_cert_end[] asm("_binary_cacert_pem_end");

extern const char server_key[] asm("_binary_cakey_pem_start");
extern const char server_key_end[] asm("_binary_cakey_pem_end");

//extern const char tls_cert_start[] asm("_binary_tlscert_pem_start");
//extern const char tls_cert_end[]   asm("_binary_tlscert_pem_end");
//extern const char tls_key_start[] asm("_binary_tlskey_pem_start");
//extern const char tls_key_end[] asm("_binary_tlskey_pem_end");

#endif
