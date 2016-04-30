//
// Created by mofan on 1/15/16.
//

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "conf.h"
#include "defs.h"



int wx_getconf(const char* confstr, const char* key, char* buf, size_t bufsize) {
    size_t l = strlen(key);
    size_t ll = l+15;// =%ns\n
    char fmt[ll];
    sprintf(fmt, "%s=%%%us\n", key, bufsize);
    return sscanf(confstr, fmt, buf);
}


ssize_t wx_getselfpath(char* buf, size_t bufsize) {
    return readlink("/proc/self/exe", buf, bufsize);
}

int wx_readconf(const char* conffile, char* buf, size_t bufsize) {
    int fd = open(conffile, O_RDONLY);
    if (-1 == fd) {
        return -1;
    }

    size_t left=bufsize;
    ssize_t n=0;
    while (1) {
        n = read(fd, buf, left);
        if (n <= 0) {
            break;
        }
        left-=n;
    }

    return close(fd);
}


static char wx_confstr[WX_MAX_CONF_FILE_SIZE] = {0};
static char wx_selfpath[WX_MAX_SELF_PATH_SIZE] = {0};
static char wx_conffile[WX_MAX_SELF_PATH_SIZE + 6] = {0};

int wx_conf_init() {
    if (wx_selfpath[0] == 0) {
        if (-1 == wx_getselfpath(wx_selfpath, sizeof(wx_selfpath))) {
            wx_err("wx_getselfpath");
            return -1;
        }
        sprintf(wx_conffile, "%s.conf", wx_selfpath);
        if (-1 == wx_readconf(wx_conffile, wx_confstr, sizeof(wx_confstr))) {
            wx_err("wx_readconf[%s]", wx_conffile);
            return -1;
        }
    }
    return 0;
}

int wx_conf_get(const char* key, char* buf, size_t bufsize) {
    if (wx_confstr[0] == 0) {
        wx_err("you must call wx_conf_init() before wx_getconf()", key, wx_conffile);
        return -1;
    }
    if (1 != wx_getconf(wx_confstr, key, buf, bufsize)) {
        return -1;
    }
    return 0;
}

char* wx_conf_exe_name() {
    return wx_selfpath;
}