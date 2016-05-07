//
// Created by mofan on 1/30/16.
//

#include "connection.h"



static struct connection_s* free_conns = NULL;
static struct connection_s* _free_conns;


int connections_alloc(struct wx_worker_s* wk, uint32_t count) {
    assert(free_conns == NULL);
    assert(count > 0);
    _free_conns = free_conns = (struct connection_s*)malloc(sizeof(struct connection_s) * count);
    if (free_conns==NULL) {
        return -1;
    }

    int i;
    for (i=0; i<count; i++) {
        wx_conn_init(wk, &free_conns[i].wx_conn);
        free_conns[i].next = (i+1)<count ? &free_conns[i+1] : NULL;
    }

    return 0;
}
void connections_free() {
    if (_free_conns) {
        free(_free_conns);
        _free_conns = NULL;
    }
}
struct connection_s* connection_get() {
    struct connection_s* tmp = NULL;
    if (free_conns) {
        tmp = free_conns;
        free_conns = free_conns->next;
        tmp->next = NULL;
        tmp->inuse = 1;
        tmp->recvbuf.base = tmp->bufchainwithbuf + sizeof(struct wx_buf_chain_s);
        tmp->recvbuf.size = sizeof(tmp->bufchainwithbuf) - sizeof(struct wx_buf_chain_s);
        tmp->keepalivems = 0; //默认完成一个请求之后立即关闭链接
    }
    return tmp;
}
void connection_put(struct connection_s* conn) {
    if (connection_inuse(conn)) {
        conn->inuse = 0;
        int fd = conn->wx_conn.rwatcher.fd;
        if (fd > 0) {
            close(fd);
        }
        conn->next = free_conns;
        free_conns = conn;
    }
}
