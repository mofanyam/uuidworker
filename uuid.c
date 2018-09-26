//
// Created by renwuxun on 2/1/16.
//


#include "uuid.h"


static struct uuid_s uuid_last = {0};
static int uuid_gpid = 0;

int uuid_init(int worker_id, int worker_count) {
    if (0 != wx_conf_init()) {
        return -1;
    }

    char buf[512]={0};

    if (0 != wx_conf_get("gpid", buf, sizeof(buf))) {
        wx_err("gpid not found in conf");
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
        q += strlen(q)+1;
    }

    uuid_gpid = atoi(q);

    if (uuid_gpid<0 || uuid_gpid>1023) {
        wx_err("number of gpid != number of workers");
        return -1;
    }

    return 0;
}

int uuid_get_gpid() {
    return uuid_gpid;
}

uint64_t uuid_create() {
    struct uuid_s uuid;
    uuid.gpid = (uint64_t)uuid_gpid;
    uuid.count = 1;

    struct timeval tv;
    gettimeofday(&tv, 0);
    uuid.ms = (uint64_t)(tv.tv_sec*1000 + tv.tv_usec/1000) - 1491696000000;

    if (uuid.ms == uuid_last.ms) {
        uuid.count = uuid_last.count + 1;
    }

    uuid_last = uuid;

    uint64_t r = uuid.ms<<22 | uuid.gpid<<12 | uuid.count<<9;

    return r;
}