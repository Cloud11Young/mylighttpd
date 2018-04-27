#include "minet_ntop_cache.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const char* inet_ntop_cache_get_ip(server* srv, sock_addr* addr){
	UNUSED(srv);
	//inet_ntop(AF_INET,addr->ipv4.sin_addr,)
	return inet_ntoa(addr->ipv4.sin_addr);
}