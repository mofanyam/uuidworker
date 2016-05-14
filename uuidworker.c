#include <netinet/tcp.h>
//#include <gperftools/profiler.h>
#include "conn.h"
#include "lib/env.h"
#include "lib/dummyfd.h"
#include "uuid.h"



struct wx_buf_s* alloc_cb(struct wx_conn_s* wx_conn) {
    struct conn_s* conn = container_of(wx_conn, struct conn_s, wx_conn);
    return conn->buf;
}

void conn_close(struct conn_s* conn) {
    wx_timer_stop(&conn->closetimer);
    wx_conn_close(&conn->wx_conn);
    conn_put(conn);
}

void closetimer_cb(struct wx_timer_s* closetimer) {
    struct conn_s* conn = container_of(closetimer, struct conn_s, closetimer);
    conn_close(conn);
}

void cleanup_cb(struct wx_buf_chain_s* bufchain, ssize_t n, void* arg) {
    struct conn_s* conn = (struct conn_s*)arg;

    conn->buf = (struct wx_buf_s*)conn->data;
    conn->buf->base = conn->buf->data;
    conn->buf->size = sizeof(conn->data) - sizeof(struct wx_buf_s);

    if (conn->keepalivems == 0) {
        conn_close(conn);
    } else {
        wx_timer_start(&conn->closetimer, (size_t)conn->keepalivems, closetimer_cb);
    }
}

int read_cb(struct wx_conn_s* wx_conn, struct wx_buf_s* buf, ssize_t n) {
    struct conn_s* conn = container_of(wx_conn, struct conn_s, wx_conn);

    if (n == 0 && buf->size!=0) {
        conn_close(conn);
        return 0;
    }

    wx_timer_stop(&conn->closetimer);

    if (strstr(buf->data, "\r\n\r\n")) {
        if (strstr(buf->data, "HTTP/1.1\r\n")) {
            conn->keepalivems = 15000;
        }

        conn->buf = NULL;

        struct wx_buf_chain_s* bc = (struct wx_buf_chain_s*)conn->data;
        bc->base = bc->data;
        bc->size = sizeof(conn->data) - sizeof(bc);
        bc->cleanup = cleanup_cb;
        bc->arg = conn;
        bc->next = NULL;

        uint64_t uuid = uuid_create();

        bc->size = (size_t)sprintf(bc->base, "HTTP/1.1 200 OK\r\nContent-Length: 20\r\n\r\n%llu\n", uuid);

        wx_conn_write_start(wx_conn, wx_conn->rwatcher.fd, bc);
    }

    if (buf->size == 0) {
        conn->buf = NULL;
        wx_err("header buffer overflow");
        conn_close(conn);
        return EXIT_FAILURE;
    }

    return 0;
}

void accept_cb(struct wx_worker_s* wk) {
    struct conn_s* conn = conn_get();
    if (conn ==  NULL) {
        wx_err("no more free connction");
        return;
    }

    int cfd = accept(wk->listen_fd, NULL, 0);
    if (cfd < 0) {
        if (errno == EMFILE && wx_dummyfd_get() != -1 && 0 == wx_dummyfd_close()) {
            cfd = accept(wk->listen_fd, NULL, 0);
            if (cfd < 0) {
                wx_dummyfd_open();
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    errno = 0; //reset it
                } else {
                    wx_err("accept");
                }
                conn_put(conn);
                return;
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                errno = 0; //reset it
            } else {
                wx_err("accept");
            }
            conn_put(conn);
            return;
        }
    }

    int p = fcntl(cfd, F_GETFL);
    if (-1 == p || -1 == fcntl(cfd, F_SETFL, p|O_NONBLOCK)) {
        wx_err("fcntl");
        conn_put(conn);
        return;
    }

    int one = 1;
    setsockopt(cfd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));

    conn->buf = (struct wx_buf_s*)conn->data;
    conn->buf->base = conn->buf->data;
    conn->buf->size = sizeof(conn->data) - sizeof(struct wx_buf_s);
    wx_conn_read_start(&conn->wx_conn, cfd);
}

int main(int argc, char** argv) {

    int listen_fd = wx_env_get_listen_fd();
    if (listen_fd < 0) {
        wx_err("listen_fd < 0");
        return EXIT_FAILURE;
    }

    int worker_id = wx_env_get_worker_id();
    if (worker_id < 0) {
        wx_err("worker_id < 0");
        return EXIT_FAILURE;
    }

//    char _b[64]={0};
//    sprintf(_b, "./profiler-%d.pprof", worker_id);
//    ProfilerStart(_b);

    int worker_count = wx_env_get_worker_count();
    if (worker_count < 0) {
        wx_err("worker_count < 0");
        return EXIT_FAILURE;
    }

    if (0 != uuid_init(worker_id, worker_count)) {
        return EXIT_FAILURE;
    }

    char buf32[32]={0};
    size_t connections = 1024;
    if (0 == wx_conf_get("connection", buf32, sizeof(buf32))) {
        connections = (size_t)atoi(buf32);
    }
    if (0 != conns_alloc(connections)) {
        wx_err("conns_alloc");
        return EXIT_FAILURE;
    }

    wx_dummyfd_open();

    wx_worker_init(listen_fd, accept_cb, alloc_cb, read_cb);
    int r = wx_worker_run();

    if (-1 != wx_dummyfd_get()) {
        wx_dummyfd_close();
    }

    conns_free();

//    ProfilerStop();

    wx_err("worker stop");

    return r;
}