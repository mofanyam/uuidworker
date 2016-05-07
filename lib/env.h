//
// Created by mofan on 4/27/16.
//

#ifndef WXWORKER_ENV_H
#define WXWORKER_ENV_H


#include <stdlib.h>


int wx_env_get_listen_fd();

int wx_env_get_worker_id();

int wx_env_get_worker_count();

int wx_env_get_shm_id();

int wx_env_get_shm_size();


#endif //WXWORKER_ENV_H
