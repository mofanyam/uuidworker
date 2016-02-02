//
// Created by mofan on 2/1/16.
//

#include <sys/time.h>
#include <unistd.h>
#include "uuid.h"


static struct uuid_s uuid_last = {0};

int64_t create_uuid() {
    struct uuid_s uuid;
    uuid.gpid = getpid()&((1<<10)-1); // max = (1<<10)-1 = 1023，最好在配置文件中做配置一个id
    uuid.count = 1;

    struct timeval tv;
    gettimeofday(&tv, 0);
    uuid.ms = tv.tv_sec*1000 + tv.tv_usec/1000;

    if (uuid.ms == uuid_last.ms) {
        uuid.count = uuid_last.count + 1; // max = (1<<14)-1 = 16383
    }

    uuid_last = uuid;

    int64_t r = uuid.count;
    r |= (uuid.ms<<13);
    r |= ((int64_t)uuid.gpid<<54);

    return r;
}