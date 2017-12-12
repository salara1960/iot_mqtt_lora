#include "hdr.h"
#if defined(SET_WEB) || defined(SET_WS)

#include "mqtt.h"

//------------------------------------------------------------------------------------------------------------

const char *TAGTCP = "TCP";

//------------------------------------------------------------------------------------------------------------
int get_socket_error_code(int socket)
{
int result;
socklen_t optlen = sizeof(int);

    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen) == -1) {
	ESP_LOGE(TAGTCP, "getsockopt for socket %d failed", socket);
	result = -1;
    }
    return result;
}
//------------------------------------------------------------------------------------------------------------
void show_socket_error_reason(int socket)
{
int err = get_socket_error_code(socket);

    ets_printf("%s[%s] socket %d error %d %s%s\n", BROWN_COLOR, TAGTCP, socket, err, strerror(err), STOP_COLOR);

}
//------------------------------------------------------------------------------------------------------------
inline int create_tcp_server(u16_t prt)
{
int soc = -1, err = 0;

    soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc >= 0) {
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(prt);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (!bind(soc, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
	    if (listen(soc, 1)) err = 1;//(soc , 5)
	} else err = 1;

	if (err) {
	    show_socket_error_reason(soc);
	    close(soc); soc = -1;
	}
    }
    return soc;
}
//------------------------------------------------------------------------------------------------------------
#endif
