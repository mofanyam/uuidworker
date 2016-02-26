//
// Created by mofan on 2/1/16.
//

#include <stdint.h>

#ifndef UUIDWORKER_UUID_H
#define UUIDWORKER_UUID_H



struct uuid_s {
    uint64_t ms:41;     // (1<<42)-1 = 4398046511 103
    uint64_t gpid:9;    // 分布式进程id (1<<10)-1 = 1023
    uint64_t count:13;  // 分布式进程id (1<<14)-1 = 16383
};

int64_t create_uuid();


#endif //UUIDWORKER_UUID_H
