//
// Created by mofan on 4/30/16.
//

#ifndef WXWORKER_SHM_H
#define WXWORKER_SHM_H


#include <sys/shm.h>
#include <stdio.h>
#include <string.h>
#include "defs.h"


int wx_shm_alloc(key_t key, size_t size);

void* wx_shm_attach(int shmid);

int wx_shm_detach(void* addr);

void wx_shm_free(int shmid); // lazy free, real free after all children exit


#endif //WXWORKER_SHM_H
