//
// Created by renwuxun on 4/30/16.
//


#include "shm.h"


int wx_shm_alloc(key_t key, size_t size) {
    if (!key) {
        key = IPC_PRIVATE;//其他进程只能通过shmid访问
    }
    int shmid = shmget(key, size, (SHM_R|SHM_W|IPC_CREAT));
    if (shmid == -1) {
        wx_err("shmget(%d, %u, (SHM_R|SHM_W|IPC_CREAT))", key, size);
    }
    return shmid;
}

void* wx_shm_attach(int shmid) {
    void* addr = shmat(shmid, NULL, 0);
    if (addr == (void*)-1) {
        wx_err("shmat(%d, NULL, 0)", shmid);
        return NULL;
    }
    return addr;
}

int wx_shm_detach(void* addr) {
    return shmdt(addr);
}

void wx_shm_free(int shmid) {
    if (-1 == shmctl(shmid, IPC_RMID, NULL)) {
        wx_err("shmctl(%d, IPC_RMID, NULL)", shmid);
    }
}