//
// Created by renwuxun on 3/8/16.
//
// http://linux.die.net/man/3/ev

#include "dummyfd.h"


static int wx_dummyfd = -1;

int wx_dummyfd_open() {
    wx_dummyfd = open("/dev/null", O_WRONLY);
    return wx_dummyfd;
}

int wx_dummyfd_get() {
    return wx_dummyfd;
}

int wx_dummyfd_close() {
    int r = close(wx_dummyfd);
    if (0 == r) {
        wx_dummyfd = -1;
    }
    return r;
}