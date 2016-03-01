#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "lib/wxworker.h"
#include "connection.h"
#include "bufpool.h"
#include "uuid.h"


#define POOL_BUF_SIZE 4096



void connection_close(struct connection_s* conn, int statue) {
    if (!conn->inuse) {
        return;
    }
    wx_timer_stop(&conn->close_timer);
    wx_read_stop(&conn->wx_conn);
    wx_write_stop(&conn->wx_conn);
    wx_fire_outbuf_chain_cleanup(&conn->wx_conn, statue);
    buf_pool_put((struct connection_buf_s*)conn->recvbuf);
    connection_put(conn);
}

void timer_cb_close(struct wx_timer_s* close_timer) {
    struct connection_s* conn = container_of(close_timer, struct connection_s, close_timer);
    connection_close(conn, -10);
}

struct wx_buf_s* alloc_cb(struct wx_conn_s* wx_conn, size_t suggested) {
    struct connection_s* conn = (struct connection_s*)wx_conn;

    if (conn->recvbuf == NULL) {
        conn->recvbuf = (struct wx_buf_s*)buf_pool_get();
        if (conn->recvbuf == NULL) {
            wx_err("no more free buf in buf pool");
            connection_close(conn, -9);
            return NULL;
        }
    }

    return conn->recvbuf;
}

void cleanup_put_buf(struct wx_conn_s* wx_conn, struct wx_buf_chain_s* out_bufc, int status) {
    buf_pool_put((struct connection_buf_s*)out_bufc);
    out_bufc->cleanup = NULL;

    struct connection_s* conn = (struct connection_s*)wx_conn;
    if (conn->keepalivems == 0) {
        connection_close(conn, 0);
    } else if (conn->keepalivems > 0) {
        if (wx_timer_is_active(&conn->close_timer)) {
            wx_timer_stop(&conn->close_timer);
        }
        wx_timer_start(&conn->close_timer, (uint32_t)conn->keepalivems, timer_cb_close);
    }
}

void do_request(struct connection_s* conn) {
    struct connection_buf_s* cbuf = buf_pool_get();
    if (cbuf == NULL) {
        wx_err("no more free buf in buf pool");
        connection_close(conn, -9);
        return;
    }

    struct wx_buf_chain_s* bc = (struct wx_buf_chain_s*)cbuf;
    wx_buf_chain_init(bc, cleanup_put_buf);

    int64_t uuid = create_uuid();

    sprintf(bc->buf.base, "%lld\n", uuid);
    bc->buf.size = strlen(bc->buf.base);

    wx_write_start(&conn->wx_conn, conn->fd, bc);
}

void do_line(struct connection_s* conn, const char* bufbase, size_t buflen) {
    if (buflen > 11 && 0 == strncasecmp(bufbase, "keep-alive:", 11)) {
        conn->keepalivems = atoi(bufbase+11);
    }else{
        conn->keepalivems = 0;
    }

    if (0 == strncmp(bufbase, "\r\n", 2)) {
        do_request(conn);
    }
}

int find_first_ln(char* ptr, size_t size) {
    int i;
    for (i=0; i<size; i++) {
        if (ptr[i] == '\n') {
            return i;
        }
    }
    return -1;
}

int find_last_ln(char* ptr, size_t size) {
    for (;size--;) {
        if (ptr[size] == '\n') {
            return (int)size;
        }
    }
    return -1;
}

void read_cb(struct wx_conn_s* wx_conn, struct wx_buf_s* buf, ssize_t nread) {
    struct connection_s* conn = (struct connection_s*)wx_conn;
    if (nread < 0) {
        if (errno == EAGAIN) {
            errno = 0;
        } else {
            connection_close(conn, nread);
        }
        return;
    } else if (nread==0) {
        connection_close(conn, 0);
        return;
    }

    conn->recvbuf->size -= nread;
    conn->recvbuf->base += nread;

    char* origin_base = (char*)buf + sizeof(struct connection_buf_s);
    struct wx_buf_s databuf = {.base=origin_base, .size=POOL_BUF_SIZE-conn->recvbuf->size};

    if (conn->recvbuf->size == 0 && -1 == find_last_ln(origin_base, POOL_BUF_SIZE-conn->recvbuf->size)) {
        connection_close(conn, -12);
        return;
    }

    int i,lastlnpos;
    for(;;) {
        lastlnpos = find_first_ln(databuf.base, databuf.size);
        if (lastlnpos == -1) {
            if (origin_base != databuf.base) {
                for(i=0;i<databuf.size;i++) {
                    origin_base[i] = databuf.base[i];
                }
                conn->recvbuf->base = origin_base;
                conn->recvbuf->size = POOL_BUF_SIZE-databuf.size;
            }
            break;
        }
        lastlnpos++;
        do_line(conn, databuf.base, (size_t)lastlnpos);
        databuf.base += lastlnpos;
        databuf.size -= lastlnpos;
    }
}

void accept_cb(struct wx_worker_s* wk, int revents) {
    struct connection_s* conn = connection_get();
    if (!conn) {
        wx_err("no more free connections");
        return;
    }

    wx_timer_init(conn->wx_conn.worker, &conn->close_timer);

    int cfd = accept(wk->listen_fd, NULL, 0);
    if (cfd < 0) {
        if (errno == EAGAIN) {
            errno = 0; //reset it
        } else {
            wx_err("accept");
        }
        connection_put(conn);
        return;
    }

    conn->fd = cfd;

    int p = fcntl(cfd, F_GETFL);
    if (-1 == p || -1 == fcntl(cfd, F_SETFL, p|O_NONBLOCK)) {
        wx_err("fcntl");
        connection_put(conn);
        return;
    }

    wx_read_start(&conn->wx_conn, cfd, alloc_cb, read_cb);
}

void before_loop(struct wx_worker_s* wk) {
    wx_accept_start(wk, accept_cb);
}

int get_listen_fd() {
    int listen_fd = -1;
    char* evnptr = getenv("LISTEN_FD");
    if (evnptr) {
        listen_fd = atoi(evnptr);
    }
    return listen_fd;
}


int main(int argc, char** argv) {
    int listen_fd = get_listen_fd();
    if (listen_fd < 0) {
        wx_err("listen_fd < 0");
        return EXIT_FAILURE;
    }

    struct wx_worker_s worker;
    wx_worker_init(listen_fd, NULL, &worker);

    if (0 != connections_alloc(&worker, 1024)) {
        wx_err("connections_alloc");
        return EXIT_FAILURE;
    }
    if (0 != buf_pool_alloc(POOL_BUF_SIZE, 1024)) {
        wx_err("buf_pool_alloc");
        connections_free();
        return EXIT_FAILURE;
    }

    int r = wx_worker_run(&worker, before_loop, NULL);

    wx_err("worker stop");

    buf_pool_free();
    connections_free();

    return r;
}