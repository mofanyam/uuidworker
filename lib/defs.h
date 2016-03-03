//
// Created by mofan on 3/2/16.
//

#ifndef WXWORKER_DEFS_H
#define WXWORKER_DEFS_H


#include <errno.h>
#include <stdio.h>
#include <unistd.h>


#ifndef container_of
# define container_of(ptr, type, field) ((type*)((char*)(ptr) - ((char*)&((type*)0)->field)))
#endif

#define wx_err(fmt,...) fprintf(stderr, "[pid:%d] "__FILE__" : %d : "fmt" %s\n", getpid(), __LINE__, ##__VA_ARGS__, strerror(errno))


#endif //WXWORKER_DEFS_H
