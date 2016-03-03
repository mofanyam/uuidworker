//
// Created by mofan on 2/1/16.
//

#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include "uuid.h"
#include "lib/conf.h"
#include "lib/defs.h"


static struct uuid_s uuid_last = {0};
static int uuid_gpid = 0;

int uuid_init(int worker_id, int worker_count) {
    if (0 != wx_conf_init()) {
        return -1;
    }

    char buf[512]={0};

    if (0 != wx_conf_get("gpid", buf, sizeof(buf))) {
        return -1;
    }

    int count = 0;
    char* q = buf;
    while (strsep(&q, ",")) {
        count++;
    }
    if (worker_count != count) {
        wx_err("need %d gpid but %d given", worker_count, count);
        return -1;
    }

    q = buf;
    int i;
    for (i=0; i<worker_id; i++) {
        q += strlen(q);
    }

    uuid_gpid = atoi(q);

    if (uuid_gpid<0 || uuid_gpid>1023) {
        wx_err("number of gpid != number of workers");
        return -1;
    }

    return 0;
}

int64_t uuid_create() {
    struct uuid_s uuid;
    uuid.gpid = uuid_gpid; // max = (1<<10)-1 = 1023
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