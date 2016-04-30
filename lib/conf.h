//
// Created by mofan on 1/15/16.
//

#ifndef WXWORKER_CONF_H
#define WXWORKER_CONF_H


#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WX_MAX_CONF_FILE_SIZE
# define WX_MAX_CONF_FILE_SIZE 65536
#endif

#ifndef WX_MAX_SELF_PATH_SIZE
# define WX_MAX_SELF_PATH_SIZE 1024
#endif

int wx_getconf(const char* confstr, const char* key, char* buf, size_t bufsize);

ssize_t wx_getselfpath(char* buf, size_t bufsize);

int wx_readconf(const char* conffile, char* buf, size_t bufsize);

int wx_conf_init();

int wx_conf_get(const char* key, char* buf, size_t bufsize);

char* wx_conf_exe_name();


#ifdef __cplusplus
}
#endif


#endif //WXWORKER_CONF_H