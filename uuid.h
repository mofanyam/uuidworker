//
// Created by renwuxun on 2/1/16.
//


#ifndef UUIDWORKER_UUID_H
#define UUIDWORKER_UUID_H



#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include "uuid.h"
#include "lib/conf.h"


struct uuid_s {
    uint64_t ms;//:41;     // (1<<42)-1 = 4398046511 103
    uint64_t count;//:13;  // 分布式进程id (1<<14)-1 = 16383
    uint64_t gpid;//:9;    // 分布式进程id (1<<10)-1 = 1023
};

int uuid_init(int worker_id, int worker_count);

int uuid_get_gpid();

uint64_t uuid_create();



#endif //UUIDWORKER_UUID_H
