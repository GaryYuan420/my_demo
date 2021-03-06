#ifndef __MAKEU_NET_H__
#define __MAKEU_NET_H__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>			/* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>			/* superset of previous */
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/epoll.h>
#define SERV_PORT 9991
#define SERV_IP_ADDR "127.0.0.1"
#define BACKLOG 5

#define QUIT_STR "quit"
#define SERV_RESP_STR "SERVER:"
#define max(a,b) ((a > b ) ? a : b)
#define EPOLLEVENTS 20
#define FDSIZE 1024
#endif
