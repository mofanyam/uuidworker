//
// Created by renwuxun on 5/13/16.
//

#ifndef WXWORKER_WXWORKER_H
#define WXWORKER_WXWORKER_H


#include <ev.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <errno.h>
#include "defs.h"
#include "dummyfd.h"



struct wx_buf_s {
    char* base;
    size_t size;
    char data[0];
};


struct wx_conn_s {
    struct ev_io rwatcher;
    struct ev_io wwatcher;
    struct wx_buf_chain_s* sendbuf;
};

struct wx_buf_chain_s {
    char* base;
    size_t size;
    struct wx_buf_chain_s* next;
    void (*cleanup)(struct wx_buf_chain_s* bufchain, ssize_t n, void* arg);
    void* arg;
    char data[0];
};

typedef struct wx_worker_s wx_worker_t;

typedef void (*wx_accept_cb)(struct wx_worker_s* wx_worker);
typedef struct wx_buf_s* (*wx_alloc_cb)(struct wx_conn_s* wx_conn);
// return non-zere to break socket read
typedef int (*wx_read_cb)(struct wx_conn_s* wx_conn, struct wx_buf_s* buf, ssize_t n);

struct wx_worker_s {
    struct ev_io accept_watcher;
    struct ev_signal quit_watcher;
    struct ev_loop* loop;
    wx_accept_cb accept_cb;
    wx_alloc_cb alloc_cb;
    wx_read_cb read_cb;
    int listen_fd;
};


struct wx_timer_s {
    struct ev_timer ev_timer;
    void (*timer_cb)(struct wx_timer_s* wx_timer);
};

void wx_timer_init(struct wx_timer_s* timer);
void wx_timer_start(struct wx_timer_s* wx_timer, size_t timeout_ms, void (*timer_cb)(struct wx_timer_s* wx_timer));
void wx_timer_stop(struct wx_timer_s* wx_timer);
int wx_timer_is_active(struct wx_timer_s* wx_timer);


void wx_conn_write_start(struct wx_conn_s* wx_conn, int fd, struct wx_buf_chain_s* bufchain);

void wx_conn_write_stop(struct wx_conn_s* wx_conn);



void wx_conn_read_start(struct wx_conn_s* wx_conn, int fd);

void wx_conn_read_stop(struct wx_conn_s* wx_conn);


void wx_conn_init(struct wx_conn_s* wx_conn);

void wx_conn_close(struct wx_conn_s* wx_conn);

void wx_worker_init(int fd, wx_accept_cb accept_cb, wx_alloc_cb alloc_cb, wx_read_cb read_cb);

int wx_accept(int listen_fd, __SOCKADDR_ARG __addr, socklen_t* __restrict __addr_len);

int wx_worker_run();



#endif //WXWORKER_WXWORKER_H
