//
// Created by mofan on 2/1/16.
//

#include <stdlib.h>
#include <assert.h>
#include "bufpool.h"


static struct connection_buf_s* uuid_buf_chains = NULL;
static size_t uuid_buf_len = 0;


int buf_pool_alloc(size_t buflen, size_t size) {
    assert(uuid_buf_chains == NULL);
    uuid_buf_len = buflen;
    struct connection_buf_s* bs = (struct connection_buf_s*)malloc((sizeof(struct connection_buf_s) + buflen) * size);
    if (NULL == bs) {
        return -1;
    }
    uuid_buf_chains = bs;
    size_t i;
    for(i=0; i<size; i++) {
        bs[i].inuse = 0;
        bs[i].buf.base = (char*)&bs[i] + sizeof(struct connection_buf_s);
        bs[i].buf.size = buflen;
        bs[i].next = (i+1)<size ? &bs[i+1] : NULL;
    }
    return 0;
}

void buf_pool_free() {
    if (uuid_buf_chains) {
        free(uuid_buf_chains);
        uuid_buf_chains = NULL;
        uuid_buf_len = 0;
    }
}

struct connection_buf_s* buf_pool_get() {
    struct connection_buf_s* tmp = NULL;
    if (uuid_buf_chains) {
        tmp = uuid_buf_chains;
        uuid_buf_chains = uuid_buf_chains->next;
        tmp->next = NULL;
        tmp->inuse = 1;
    }
    return tmp;
}

void buf_pool_put(struct connection_buf_s* connection_buf) {
    if (connection_buf->inuse == 0) {
        return;
    }
    connection_buf->buf.size = uuid_buf_len;
    connection_buf->buf.base = (char*)connection_buf + sizeof(struct connection_buf_s);
    connection_buf->inuse = 0;
    connection_buf->next = uuid_buf_chains;
    uuid_buf_chains = connection_buf;
}