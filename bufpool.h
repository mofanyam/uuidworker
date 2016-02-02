//
// Created by mofan on 2/1/16.
//

#ifndef UUIDWORKER_BUFPOOL_H
#define UUIDWORKER_BUFPOOL_H


#include "lib/wxworker.h"


struct connection_buf_s {
    struct wx_buf_s buf;
    struct connection_buf_s* next;
    int8_t inuse;
};


int buf_pool_alloc(size_t buflen, size_t size);

void buf_pool_free();

struct connection_buf_s* buf_pool_get();

void buf_pool_put(struct connection_buf_s* connection_buf);


#endif //UUIDWORKER_BUFPOOL_H
