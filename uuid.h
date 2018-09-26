//
// Created by renwuxun on 2/1/16.
//


#ifndef UUIDWORKER_UUID_H
#define UUIDWORKER_UUID_H


#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include "uuid.h"
#include "wxworker/conf.h"


struct uuid_s {
    uint64_t ms;    //:42;
    uint64_t gpid;  //:10;
    uint64_t count; //:12;
};

int uuid_init(int worker_id, int worker_count);

int uuid_get_gpid();

uint64_t uuid_create();


#endif //UUIDWORKER_UUID_H
