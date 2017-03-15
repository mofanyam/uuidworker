//
// Created by mofan on 5/16/16.
//

#ifndef WXWORKER_SPINLOCK_H
#define WXWORKER_SPINLOCK_H


struct wx_spinlock_s {
    char write;
    char read;
};


void wx_spinlock_init(struct wx_spinlock_s *lock);

void wx_spinlock_rlock(struct wx_spinlock_s *lock);

void wx_spinlock_wlock(struct wx_spinlock_s *lock);

void wx_spinlock_wunlock(struct wx_spinlock_s *lock);

void wx_spinlock_runlock(struct wx_spinlock_s *lock);


#endif //WXWORKER_SPINLOCK_H
