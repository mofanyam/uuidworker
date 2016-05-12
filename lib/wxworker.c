//
// Created by mofan on 1/22/16.
//

#include "wxworker.h"


static inline ssize_t wx_send_buf(int fd, struct wx_buf_s* obuf) {
    ssize_t nsent = send(fd, obuf->base, obuf->size, 0);
    if (nsent > 0) {
        obuf->size -= nsent;
        obuf->base += nsent;
    }
    return nsent;
}
int wx_send_buf_chain(struct wx_conn_s* wx_conn, struct ev_io* wwatcher) {
    ssize_t nsent;
    int fd = wwatcher->fd;
    struct wx_buf_chain_s* tmp;
    for (;wx_conn->obufchain && ev_is_active(wwatcher);) {
        nsent = wx_send_buf(fd, &wx_conn->obufchain->buf);
        if (nsent < 0) {
            if (errno == EAGAIN) {
                errno = 0;
            } else {
                tmp = wx_conn->obufchain;
                wx_conn->obufchain = wx_conn->obufchain->next;
                if (tmp->cleanup) {
                    tmp->next = NULL; // dont hurt the othor one!
                    tmp->cleanup(wx_conn, tmp, (int)nsent);
                }
                return -1;
            }
            break;
        }
        if (wx_conn->obufchain->buf.size == 0) {
            tmp = wx_conn->obufchain;
            wx_conn->obufchain = wx_conn->obufchain->next;
            if (tmp->cleanup) {
                tmp->next = NULL; // dont hurt the othor one!
                tmp->cleanup(wx_conn, tmp, 0);
            }
        }
    }
    return 0;
}
static inline void wx_do_write(EV_P_ struct ev_io* ww, int revents) {
    struct wx_conn_s* wx_conn = container_of(ww, struct wx_conn_s, wwatcher);
    if (ev_is_active(&wx_conn->wwatcher)) {
        wx_send_buf_chain(wx_conn, &wx_conn->wwatcher);
    }
}
void wx_conn_write_start(struct wx_conn_s* wx_conn, int fd, struct wx_buf_chain_s* obufchain) {
    if (NULL == wx_conn->obufchain) {
        wx_conn->obufchain = obufchain;
    } else {
        struct wx_buf_chain_s* last, *pos = wx_conn->obufchain;
        for (last = pos; pos; pos = pos->next) {
            last = pos;
        }
        last->next = obufchain;
    }

    if (0 == wx_send_buf_chain(wx_conn, &wx_conn->wwatcher) && wx_conn->obufchain != NULL && !ev_is_active(&wx_conn->wwatcher)) {
        ev_io_set(&wx_conn->wwatcher, fd, EV_WRITE);
        ev_io_start(wx_conn->worker->loop, &wx_conn->wwatcher);
    }
}
void wx_conn_write_stop(struct wx_conn_s* wx_conn) {
    if (ev_is_active(&wx_conn->wwatcher)) {
        ev_io_stop(wx_conn->worker->loop, &wx_conn->wwatcher);
    }
}


static inline ssize_t wx_recv_buf(int fd, struct wx_buf_s* ibuf) {
    ssize_t nread = recv(fd, ibuf->base, ibuf->size, 0);
    if (nread > 0) {
        ibuf->base += nread;
        ibuf->size -= nread;
    }
    return nread;
}
static inline void wx_do_read(EV_P_ struct ev_io* rw, int revents) {
    struct wx_conn_s* wx_conn = container_of(rw, struct wx_conn_s, rwatcher);

    struct wx_worker_s* worker = wx_conn->worker;
    struct wx_buf_s* buf;
    ssize_t n;
    for (;ev_is_active(&wx_conn->rwatcher);) {
        buf = worker->alloc_cb(wx_conn, 0);
        if (!buf) {
            break;
        }

        n = wx_recv_buf(rw->fd, buf);

        worker->read_cb(wx_conn, buf, buf->base-n, n);

        if (n <= 0) {
            break;
        }
    }
}
void wx_conn_read_start(struct wx_conn_s* wx_conn, int fd) {
    if (!ev_is_active(&wx_conn->rwatcher)) {
        ev_io_set(&wx_conn->rwatcher, fd, EV_READ);
        ev_io_start(wx_conn->worker->loop, &wx_conn->rwatcher);
    }
}
void wx_conn_read_stop(struct wx_conn_s* wx_conn) {
    if (ev_is_active(&wx_conn->rwatcher)) {
        ev_io_stop(wx_conn->worker->loop, &wx_conn->rwatcher);
    }
}


void wx_do_close(struct ev_loop* loop, struct ev_timer* closetimer, int revents) {
    struct wx_conn_s* wx_conn = container_of(closetimer, struct wx_conn_s, closetimer);
    if (ev_is_active(&wx_conn->closetimer)) {
        wx_conn->worker->closetimer_cb(wx_conn);
    }
}
void wx_conn_closetimer_start(struct wx_conn_s* wx_conn, size_t timeout_ms) {
    if (ev_is_active(&wx_conn->closetimer)) {
        ev_timer_stop(wx_conn->worker->loop, &wx_conn->closetimer);
    }
    ev_timer_set(&wx_conn->closetimer, timeout_ms/1000.0, 0);
    ev_timer_start(wx_conn->worker->loop, &wx_conn->closetimer);
}
void wx_conn_closetimer_stop(struct wx_conn_s* wx_conn) {
    if (ev_is_active(&wx_conn->closetimer)) {
        ev_timer_stop(wx_conn->worker->loop, &wx_conn->closetimer);
    }
}
int wx_conn_closetimer_is_active(struct wx_conn_s* wx_conn) {
    return ev_is_active(&wx_conn->closetimer);
}


static inline void wx_do_accept(EV_P_ struct ev_io* aw, int revents) {
    struct wx_worker_s* wk = container_of(aw, struct wx_worker_s, accept_watcher);
    if (ev_is_active(&wk->accept_watcher)) {
        wk->accept_cb(wk, revents);
    }
}
void wx_accept_start(struct wx_worker_s* wk) {
    if (!ev_is_active(&wk->accept_watcher)) {
        ev_io_set(&wk->accept_watcher, wk->listen_fd, EV_READ);
        ev_io_start(wk->loop, &wk->accept_watcher);
    }
}
void wx_accept_stop(struct wx_worker_s* wk) {
    if (ev_is_active(&wk->accept_watcher)) {
        ev_io_stop(wk->loop, &wk->accept_watcher);
    }
}


void wx_conn_init(struct wx_worker_s* wk, struct wx_conn_s* wx_conn) {
    ev_init(&wx_conn->rwatcher, wx_do_read);
    ev_init(&wx_conn->wwatcher, wx_do_write);
    ev_init(&wx_conn->closetimer, wx_do_close);
    wx_conn->worker = wk;
}
void wx_worker_init(
        int listen_fd
        , struct wx_worker_s* wk
        , wx_accept_cb accept_cb
        , wx_alloc_cb alloc_cb
        , wx_read_cb read_cb
        , wx_closetimer_cb closetimer_cb
) {
    assert(listen_fd > 0);
    ev_init(&wk->accept_watcher, wx_do_accept);
    wk->loop = ev_loop_new(EVBACKEND_EPOLL);
    wk->listen_fd = listen_fd;
    wk->accept_cb = accept_cb;
    wk->alloc_cb = alloc_cb;
    wk->read_cb = read_cb;
    wk->closetimer_cb = closetimer_cb;
}


static inline void wx_break_loop(struct ev_loop* loop, struct ev_signal* quit_watcher, int revents) {
    struct wx_worker_s* wk = container_of(quit_watcher, struct wx_worker_s, quit_watcher);
    ev_signal_stop(loop, quit_watcher);
    wx_accept_stop(wk); // ev_break(loop, EVBREAK_ONE);
}


int wx_worker_run(struct wx_worker_s* wk) {
    ev_signal_init(&wk->quit_watcher, wx_break_loop, SIGQUIT);
    ev_signal_start(wk->loop, &wk->quit_watcher);

    wx_accept_start(wk);

    int r = ev_run(wk->loop, 0);

    ev_loop_destroy(wk->loop);

    return r;
}


static inline void wx_do_timeout(EV_P_ struct ev_timer* timer, int revents) {
    struct wx_timer_s* wx_timer = (struct wx_timer_s*)timer;
    if (wx_timer->timer_cb) {
        wx_timer->timer_cb(wx_timer);
    }
}
void wx_timer_init(struct wx_worker_s* wk, struct wx_timer_s* timer) {
    assert(wk->loop != NULL);
    timer->loop = wk->loop;
    ev_init(&timer->ev_timer, wx_do_timeout);
}
void wx_timer_start(struct wx_timer_s* wx_timer, size_t timeout_ms, void (*timer_cb)(struct wx_timer_s* wx_timer)) {
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


char* wx_buf_strstr(const struct wx_buf_s* buf1, const struct wx_buf_s* buf2) {
    const char* p = buf1->base;
    const size_t len = buf2->size;
    for (; (p = strchr(p, *buf2->base)) != 0 && p<=buf1->base+buf1->size; p++) {
        if (strncmp(p, buf2->base, len) == 0)
            return (char *)p;
    }
    return (0);
}