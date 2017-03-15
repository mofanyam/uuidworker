//
// Created by renwuxun on 5/13/16.
//

#include "wxworker.h"


struct wx_worker_s wx_worker = {0};


static inline ssize_t wx_conn_send(struct wx_conn_s* wx_conn, int fd) {
    assert(wx_conn->sendbuf != NULL);

    struct wx_buf_chain_s* tmp,*sendbuf = wx_conn->sendbuf;
    ssize_t n;

    do {
        n = send(fd, sendbuf->base, sendbuf->size, 0);
        if (n < 0) {
            if (errno == EAGAIN) {
                //errno = 0;
            } else {
                do {
                    tmp = sendbuf->next;
                    sendbuf = sendbuf->next;
                    if (tmp->cleanup) {
                        tmp->next = NULL;
                        tmp->cleanup(tmp, n, tmp->arg);
                    }
                } while (sendbuf);
            }
            break;
        }
        sendbuf->base += n;
        sendbuf->size -= n;
        if (sendbuf->size == 0) {
            tmp = sendbuf;
            sendbuf = sendbuf->next;
            if (tmp->cleanup) {
                tmp->next = NULL;
                tmp->cleanup(tmp, n, tmp->arg);
            }
        }
    } while (sendbuf);

    wx_conn->sendbuf = sendbuf;

    return n;
}

void wx_conn_write_start(struct wx_conn_s* wx_conn, int fd, struct wx_buf_chain_s* bufchain) {
    assert(bufchain != NULL);

    if (wx_conn->sendbuf == NULL) {
        wx_conn->sendbuf = bufchain;
    } else {
        struct wx_buf_chain_s* last = wx_conn->sendbuf;
        if (last->next) {
            do {
                last = last->next;
            } while (last->next);
        }
        last->next = bufchain;
    }

    ssize_t n = wx_conn_send(wx_conn, fd);
    if (wx_conn->sendbuf != NULL && n < 0 && errno == EAGAIN) {
        ev_io_set(&wx_conn->wwatcher, wx_conn->rwatcher.fd, EV_WRITE);
        ev_io_start(wx_worker.loop, &wx_conn->wwatcher);
    }
}
void wx_conn_write_stop(struct wx_conn_s* wx_conn) {
    ev_io_stop(wx_worker.loop, &wx_conn->wwatcher);
}

static inline void wx_ev_write_cb(struct ev_loop* loop, struct ev_io* wwatcher, int events) {
    wx_conn_send(container_of(wwatcher, struct wx_conn_s, wwatcher), wwatcher->fd);
}


static inline ssize_t wx_conn_recv(struct wx_conn_s* wx_conn, int fd) {
    struct wx_buf_s* buf;
    ssize_t n = -1;
    do {
        buf = wx_worker.alloc_cb(wx_conn);
        if (!buf) {
            break;
        }
        n = recv(fd, buf->base, buf->size, 0);
        if (n > 0) {
            buf->base += n;
            buf->size -= n;
        }
        if (0 != wx_worker.read_cb(wx_conn, buf, n)) {
            break;
        }
    } while (n > 0);
    return n;
}
void wx_conn_read_start(struct wx_conn_s* wx_conn, int fd) {
    ev_io_set(&wx_conn->rwatcher, fd, EV_READ);
    ev_io_start(wx_worker.loop, &wx_conn->rwatcher);
}
void wx_conn_read_stop(struct wx_conn_s* wx_conn) {
    ev_io_stop(wx_worker.loop, &wx_conn->rwatcher);
}
void wx_ev_read_cb(struct ev_loop* loop, struct ev_io* rwatcher, int events) {
    wx_conn_recv(container_of(rwatcher, struct wx_conn_s, rwatcher), rwatcher->fd);
}

void wx_conn_init(struct wx_conn_s* wx_conn) {
    ev_init(&wx_conn->rwatcher, wx_ev_read_cb);
    ev_init(&wx_conn->wwatcher, wx_ev_write_cb);
}
void wx_conn_close(struct wx_conn_s* wx_conn) {
    wx_conn_read_stop(wx_conn);
    wx_conn_write_stop(wx_conn);
    close(wx_conn->rwatcher.fd);
}

void wx_worker_init(int fd, wx_accept_cb accept_cb, wx_alloc_cb alloc_cb, wx_read_cb read_cb) {
    wx_worker.listen_fd = fd;
    wx_worker.accept_cb = accept_cb;
    wx_worker.alloc_cb = alloc_cb;
    wx_worker.read_cb = read_cb;
}

static inline void wx_break_loop(struct ev_loop* loop, struct ev_signal* quit_watcher, int revents) {
    ev_signal_stop(loop, quit_watcher);
    ev_io_stop(loop, &wx_worker.accept_watcher);
}

static inline void wx_do_accept(struct ev_loop* loop, struct ev_io* accept_watcher, int revents) {
    wx_worker.accept_cb(&wx_worker);
}

int wx_accept(int listen_fd, __SOCKADDR_ARG __addr, socklen_t* __restrict __addr_len) {
    int client_fd = accept(listen_fd, __addr, __addr_len);
    if (client_fd < 0) {
        if (errno == EMFILE && wx_dummyfd_get() != -1 && 0 == wx_dummyfd_close()) {
            client_fd = accept(listen_fd, __addr, __addr_len);
            if (client_fd < 0) {
                wx_dummyfd_open();
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    errno = 0; //reset it
                } else {
                    wx_err("accept");
                }
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                errno = 0; //reset it
            } else {
                wx_err("accept");
            }
        }
    }
    return client_fd;
}

int wx_worker_run() {
    wx_worker.loop = ev_loop_new(EVBACKEND_EPOLL);

    ev_signal_init(&wx_worker.quit_watcher, wx_break_loop, SIGQUIT);
    ev_signal_start(wx_worker.loop, &wx_worker.quit_watcher);

    ev_io_init(&wx_worker.accept_watcher, wx_do_accept, wx_worker.listen_fd, EV_READ);
    ev_io_start(wx_worker.loop, &wx_worker.accept_watcher);

    wx_dummyfd_open();

    int r = ev_run(wx_worker.loop, 0);
    ev_loop_destroy(wx_worker.loop);

    if (-1 != wx_dummyfd_get()) {
        wx_dummyfd_close();
    }

    return r;
}

static inline void wx_do_timeout(EV_P_ struct ev_timer* timer, int revents) {
    struct wx_timer_s* wx_timer = (struct wx_timer_s*)timer;
    if (wx_timer->timer_cb) {
        wx_timer->timer_cb(wx_timer);
    }
}

void wx_timer_init(struct wx_timer_s* timer) {
    ev_init(&timer->ev_timer, wx_do_timeout);
}
void wx_timer_start(struct wx_timer_s* wx_timer, size_t timeout_ms, void (*timer_cb)(struct wx_timer_s* wx_timer)) {
    assert(timer_cb != NULL);
    wx_timer->timer_cb = timer_cb;
    ev_timer_set(&wx_timer->ev_timer, timeout_ms/1000.0, 0);
    ev_timer_start(wx_worker.loop, &wx_timer->ev_timer);
}
void wx_timer_stop(struct wx_timer_s* wx_timer) {
    if (ev_is_active(&wx_timer->ev_timer)) {
        ev_timer_stop(wx_worker.loop, &wx_timer->ev_timer);
        wx_timer->timer_cb = NULL;
    }
}
int wx_timer_is_active(struct wx_timer_s* wx_timer) {
    return ev_is_active(&wx_timer->ev_timer);
}