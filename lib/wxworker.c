//
// Created by mofan on 1/22/16.
//

#include "wxworker.h"


static ssize_t wx_send_buf(int fd, struct wx_buf_s* buf) {
    ssize_t nsent = send(fd, buf->base, buf->size, 0);
    if (nsent > 0) {
        buf->size -= nsent;
        buf->base += nsent;
    }
    return nsent;
}

int wx_send_buf_chian(struct wx_conn_s* wx_conn, int fd) {
    ssize_t nsent;
    struct wx_buf_chain_s* tmp;
    for (;wx_conn->out_bufc;) {
        nsent = wx_send_buf(fd, &wx_conn->out_bufc->buf);
        if (nsent < 0) {
            if (errno == EAGAIN) {
                errno = 0;
            } else {
                tmp = wx_conn->out_bufc;
                wx_conn->out_bufc = wx_conn->out_bufc->next;
                if (tmp->cleanup) {
                    tmp->next = NULL; // dont hurt the othor one!
                    tmp->cleanup(wx_conn, tmp, (int)nsent);
                }
                return -1;
            }
            break;
        }
        if (wx_conn->out_bufc->buf.size == 0) {
            tmp = wx_conn->out_bufc;
            wx_conn->out_bufc = wx_conn->out_bufc->next;
            if (tmp->cleanup) {
                tmp->next = NULL; // dont hurt the othor one!
                tmp->cleanup(wx_conn, tmp, 0);
            }
        }
    }
    return 0;
}


static void wx_do_write(EV_P_ struct ev_io* ww, int revents) {
    struct wx_conn_s* wx_conn = container_of(ww, struct wx_conn_s, wwatcher);

    if (ev_is_active(&wx_conn->wwatcher)) {
        wx_send_buf_chian(wx_conn, wx_conn->wwatcher.fd);
    }
}
void wx_write_start(struct wx_conn_s* wx_conn, int fd, struct wx_buf_chain_s* out_bufc) {
    assert(fd > 0);

    if (NULL == wx_conn->out_bufc) {
        wx_conn->out_bufc = out_bufc;
    } else {
        struct wx_buf_chain_s* last, *pos = wx_conn->out_bufc;
        for (last = pos; pos; pos = pos->next) {
            last = pos;
        }
        last->next = out_bufc;
    }

    if (0 == wx_send_buf_chian(wx_conn, fd) && wx_conn->out_bufc != NULL && !ev_is_active(&wx_conn->wwatcher)) {
        ev_io_set(&wx_conn->wwatcher, fd, EV_WRITE);
        ev_io_start(wx_conn->worker->loop, &wx_conn->wwatcher);
    }
}
void wx_write_stop(struct wx_conn_s* wx_conn) {
    if (ev_is_active(&wx_conn->wwatcher)) {
        ev_io_stop(wx_conn->worker->loop, &wx_conn->wwatcher);
    }
}


static void wx_do_read(EV_P_ struct ev_io* rw, int revents) {
    struct wx_conn_s* wx_conn = container_of(rw, struct wx_conn_s, rwatcher);

    struct wx_buf_s* buf;
    ssize_t n;
    for (;ev_is_active(&wx_conn->rwatcher);) {
        buf = wx_conn->alloc_cb(wx_conn, 0);
        if (!buf) {
            break;
        }

        n = recv(rw->fd, buf->base, buf->size, 0);

        wx_conn->read_cb(wx_conn, buf, n);

        if (n <= 0) {
            break;
        }
    }
}
void wx_read_start(
        struct wx_conn_s* wx_conn,
        int fd,
        struct wx_buf_s* (*alloc_cb)(struct wx_conn_s* wx_conn, size_t suggested_size),
        void (*read_cb)(struct wx_conn_s* wx_conn, struct wx_buf_s* buf, ssize_t nread)
) {
    assert(alloc_cb != NULL);
    assert(read_cb != NULL);
    assert(fd > 0);

    wx_conn->alloc_cb = alloc_cb;
    wx_conn->read_cb = read_cb;

    if (!ev_is_active(&wx_conn->rwatcher)) {
        ev_io_set(&wx_conn->rwatcher, fd, EV_READ);
        ev_io_start(wx_conn->worker->loop, &wx_conn->rwatcher);
    }
}
void wx_read_stop(struct wx_conn_s* wx_conn) {
    if (ev_is_active(&wx_conn->rwatcher)) {
        ev_io_stop(wx_conn->worker->loop, &wx_conn->rwatcher);
        wx_conn->alloc_cb = NULL;
        wx_conn->read_cb = NULL;
    }
}


static void wx_do_accept(EV_P_ struct ev_io* aw, int revents) {
    struct wx_worker_s* wk = container_of(aw, struct wx_worker_s, accept_watcher);
    if (wk->accept_cb) {
        wk->accept_cb(wk, revents);
    }
}
void wx_accept_start(struct wx_worker_s* wk, void (*accept_cb)(struct wx_worker_s* wk, int revents)) {
    assert(accept_cb != NULL);

    wk->accept_cb = accept_cb;

    if (!ev_is_active(&wk->accept_watcher)) {
        ev_io_set(&wk->accept_watcher, wk->listen_fd, EV_READ);
        ev_io_start(wk->loop, &wk->accept_watcher);
    }
}
void wx_accept_stop(struct wx_worker_s* wk) {
    if (ev_is_active(&wk->accept_watcher)) {
        ev_io_stop(wk->loop, &wk->accept_watcher);
        wk->accept_cb = NULL;
    }
}


void wx_conn_init(struct wx_worker_s* wk, struct wx_conn_s* wx_conn) {
    ev_init(&wx_conn->rwatcher, wx_do_read);
    ev_init(&wx_conn->wwatcher, wx_do_write);
    wx_conn->worker = wk;
}
void wx_worker_init(int listen_fd, void* data, struct wx_worker_s* wk) {
    assert(listen_fd > 0);
    ev_init(&wk->accept_watcher, wx_do_accept);
    wk->loop = ev_loop_new(EVBACKEND_EPOLL);
    wk->listen_fd = listen_fd;
    wk->data = data;
}


static struct ev_signal quit_watcher;
static void wx_break_loop(EV_P_ struct ev_signal* quit_watcher, int revents) {
    ev_signal_stop(loop, quit_watcher);
    ev_break(loop, EVBREAK_ONE);
}
static void wx_setup_quit_monitor(struct ev_loop* loop) {
    ev_signal_init(&quit_watcher, wx_break_loop, SIGQUIT);
    ev_signal_start(loop, &quit_watcher);
}


int wx_worker_run(
        struct wx_worker_s* wk,
        void(*before_loop)(struct wx_worker_s* wk),
        void(*after_loop)(struct wx_worker_s* wk)
) {
    wx_setup_quit_monitor(wk->loop);

    if (before_loop){
        before_loop(wk);
    }

    int r = ev_run(wk->loop, 0);

    if (after_loop) {
        after_loop(wk);
    }

    ev_loop_destroy(wk->loop);

    return r;
}


static void wx_comm_timer_cb(EV_P_ struct ev_timer* timer, int revents) {
    struct wx_timer_s* wx_timer = (struct wx_timer_s*)timer;
    if (wx_timer->timer_cb) {
        wx_timer->timer_cb(wx_timer);
    }
}
void wx_timer_init(struct wx_worker_s* wk, struct wx_timer_s* timer) {
    assert(wk->loop != NULL);
    timer->loop = wk->loop;
    ev_init(&timer->ev_timer, wx_comm_timer_cb);
}
void wx_timer_start(struct wx_timer_s* wx_timer, uint32_t timeout_ms, void (*timer_cb)(struct wx_timer_s* wx_timer)) {
    assert(timer_cb != NULL);
    wx_timer->timer_cb = timer_cb;
    if (!ev_is_active(&wx_timer->ev_timer)) {
        ev_timer_set(&wx_timer->ev_timer, timeout_ms/1000, 0);
        ev_timer_start(wx_timer->loop, &wx_timer->ev_timer);
    }
}
void wx_timer_stop(struct wx_timer_s* wx_timer) {
    if (ev_is_active(&wx_timer->ev_timer)) {
        ev_timer_stop(wx_timer->loop, &wx_timer->ev_timer);
        wx_timer->timer_cb = NULL;
    }
}
int wx_timer_is_active(struct wx_timer_s* wx_timer) {
    return ev_is_active(&wx_timer->ev_timer);
}