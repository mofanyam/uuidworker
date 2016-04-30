//
// Created by mofan on 1/22/16.
//

#ifndef WORKER_WXWORKER_H
#define WORKER_WXWORKER_H


#include <ev.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <assert.h>
#include <errno.h>
#include "defs.h"



struct wx_buf_s {
    char* base;
    size_t size;
};

struct wx_conn_s {
    struct ev_io rwatcher;
    struct wx_buf_s* (*alloc_cb)(struct wx_conn_s* wx_conn, size_t suggested_size);
    void (*read_cb)(struct wx_conn_s* wx_conn, struct wx_buf_s* buf, char* lastbase, ssize_t nread);

    struct ev_io wwatcher;
    struct wx_buf_chain_s* out_bufc;

    struct wx_worker_s* worker;
};

struct wx_buf_chain_s {
    struct wx_buf_s buf;
    struct wx_buf_chain_s* next;
    void (*cleanup)(struct wx_conn_s* wx_conn, struct wx_buf_chain_s* out_bufc, int status);
};

struct wx_worker_s {
    struct ev_io accept_watcher;
    struct ev_loop* loop;
    int listen_fd;
    void (*accept_cb)(struct wx_worker_s* wk, int revents);
    void* data;
};

struct wx_timer_s {
    struct ev_timer ev_timer;
    struct ev_loop* loop;
    void (*timer_cb)(struct wx_timer_s* wx_timer);
};


void wx_conn_init(struct wx_worker_s* wk, struct wx_conn_s* wx_conn);

void wx_write_start(struct wx_conn_s* wx_conn, int fd, struct wx_buf_chain_s* out_bufc);
void wx_write_stop(struct wx_conn_s* wx_conn);

void wx_read_start(
        struct wx_conn_s* wx_conn,
        int fd,
        struct wx_buf_s* (*alloc_cb)(struct wx_conn_s* wx_conn, size_t suggested_size),
        void (*read_cb)(struct wx_conn_s* wx_conn, struct wx_buf_s* buf, char* lastbase, ssize_t nread)
);
void wx_read_stop(struct wx_conn_s* wx_conn);

void wx_accept_start(struct wx_worker_s* wk, void (*accept_cb)(struct wx_worker_s* wk, int revents));
void wx_accept_stop(struct wx_worker_s* wk);

void wx_worker_init(int listen_fd, void* data, struct wx_worker_s* wk);

int wx_worker_run(
        struct wx_worker_s* wk,
        void(*before_loop)(struct wx_worker_s* wk),
        void(*after_loop)(struct wx_worker_s* wk)
);

void wx_timer_init(struct wx_worker_s* wk, struct wx_timer_s* timer);
void wx_timer_start(struct wx_timer_s* wx_timer, uint32_t timeout_ms, void (*timer_cb)(struct wx_timer_s* wx_timer));
void wx_timer_stop(struct wx_timer_s* wx_timer);
int wx_timer_is_active(struct wx_timer_s* wx_timer);


#define wx_buf_chain_init(bc, cleanup_cb) do{                   \
(bc)->cleanup = (cleanup_cb);                                   \
(bc)->next = NULL;                                              \
(bc)->buf.base = (char*)(bc) + sizeof(struct wx_buf_chain_s);   \
}while(0)

char* wx_buf_strstr(const struct wx_buf_s* buf1, const struct wx_buf_s* buf2);


#endif //WORKER_WXWORKER_H
