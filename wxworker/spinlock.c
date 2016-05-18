//
// Created by mofan on 5/16/16.
//

#include "spinlock.h"


void wx_spinlock_init(struct wx_spinlock_s *lock) {
    lock->write = 0;
    lock->read = 0;
}

void wx_spinlock_rlock(struct wx_spinlock_s *lock) {
    for (;;) {
        while (lock->write) {
            __sync_synchronize();
        }
        __sync_add_and_fetch(&lock->read, 1);
        if (lock->write) {
            __sync_sub_and_fetch(&lock->read, 1);
        } else {
            break;
        }
    }
}

void wx_spinlock_wlock(struct wx_spinlock_s *lock) {
    while (__sync_lock_test_and_set(&lock->write, 1)) { }
    while (lock->read) {
        __sync_synchronize();
    }
}

void wx_spinlock_wunlock(struct wx_spinlock_s *lock) {
    __sync_lock_release(&lock->write);
}

void wx_spinlock_runlock(struct wx_spinlock_s *lock) {
    __sync_sub_and_fetch(&lock->read, 1);
}