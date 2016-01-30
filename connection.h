//
// Created by mofan on 1/30/16.
//

#ifndef UUIDWORKER_CONNECTION_H
#define UUIDWORKER_CONNECTION_H


#include "lib/wxworker.h"


#define REQUEST_LEN 100
#define RESPONSE_LEN 100

struct connection_s {
    struct wx_conn_s wx_conn;
    struct wx_timer_s close_timer;
    struct connection_s* next;
    int fd;
    int inuse;
    size_t recvlen;
    char recvbuf[sizeof(struct wx_buf_s) + sizeof(struct wx_buf_chain_s) + REQUEST_LEN];//100=len:[int32_t]|[keepalive:int32_t][cmd:int32_t]
    char sendbuf[sizeof(struct wx_buf_chain_s) + RESPONSE_LEN]; //100=len:[int32_t][int64_t]
};


int connections_alloc(struct wx_worker_s* wk, uint32_t count);
void connections_free();
struct connection_s* connection_get();
void connection_put(struct connection_s* conn);


#endif //UUIDWORKER_CONNECTION_H
