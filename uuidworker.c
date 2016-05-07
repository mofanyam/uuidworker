#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/tcp.h>
#include "lib/wxworker.h"
#include "connection.h"
#include "uuid.h"
#include "lib/conf.h"
#include "lib/dummyfd.h"
#include "lib/env.h"
//#include <gperftools/profiler.h>



void connection_close(struct connection_s* conn, int status) {
    if (connection_inuse(conn)) {
        if (wx_dummyfd_get() == -1) {
            wx_dummyfd_open();
        }
        struct wx_conn_s* wx_conn = &conn->wx_conn;
        wx_conn_closetimer_stop(wx_conn);
        wx_conn_read_stop(wx_conn);
        wx_conn_write_stop(wx_conn);
        connection_put(conn);
    }
}

void closetimer_cb(struct wx_conn_s* wx_conn) {
    struct connection_s* conn = container_of(wx_conn, struct connection_s, wx_conn);
    connection_close(conn, -10);
}

struct wx_buf_s* alloc_cb(struct wx_conn_s* wx_conn, size_t suggested) {
    struct connection_s* conn = (struct connection_s*)wx_conn;
    struct wx_buf_s* buf = &conn->recvbuf;
    if (buf->size>0 && buf->base!=NULL) {
        return buf;
    }
    return NULL;
}

void cleanup_buf_chain(struct wx_conn_s* wx_conn, struct wx_buf_chain_s* obufchain, int status) {
    obufchain->cleanup = NULL;

    struct connection_s* conn = (struct connection_s*)wx_conn;

    // start recv again
    conn->recvbuf.base = conn->bufchainwithbuf+sizeof(struct wx_buf_chain_s);
    conn->recvbuf.size = sizeof(conn->bufchainwithbuf) - sizeof(struct wx_buf_chain_s);

    wx_conn_closetimer_stop(&conn->wx_conn); // 停止发送计时
    // 开始等待下次新请求计时
    if (conn->keepalivems == 0) {
        connection_close(conn, 0);
    } else if (conn->keepalivems > 0) {
        wx_conn_closetimer_start(&conn->wx_conn, (size_t)conn->keepalivems);
    }
}

static inline void do_request(struct connection_s* conn, const char* data_base, size_t data_size) {
    struct wx_buf_chain_s* bc = (struct wx_buf_chain_s*)(data_base - sizeof(struct wx_buf_chain_s));
    wx_buf_chain_init(bc, cleanup_buf_chain);

    uint64_t uuid = uuid_create();//

    bc->buf.size = (size_t)sprintf(bc->buf.base, "%llu\n", uuid);

    wx_conn_write_start(&conn->wx_conn, conn->wx_conn.rwatcher.fd, bc);
}

static inline int find_char(char* ptr, size_t size, char c) {
    int i;
    for (i=0; i<size; i++) {
        if (ptr[i] == c) {
            return i;
        }
    }
    return -1;
}

void do_line(struct connection_s* conn, const char* bufbase, size_t buflen) {
    if (buflen > 11 && 0 == strncasecmp(bufbase, "keep-alive:", 11)) {
        conn->keepalivems = atoi(bufbase+11);
    }

    if (0 == strncmp(bufbase, "\r\n", 2)) {
        char* data_base = conn->bufchainwithbuf+sizeof(struct wx_buf_chain_s);
        size_t data_size = sizeof(conn->bufchainwithbuf) - sizeof(struct wx_buf_chain_s) - conn->recvbuf.size;
        // temporary stop recv
        conn->recvbuf.base = NULL;
        conn->recvbuf.size = 0;
        wx_conn_closetimer_start(&conn->wx_conn, 10000); // 已接收到完整的请，关闭前面的接收超时器，开始发送计时，给你10秒钟时间接收数据
        do_request(conn, data_base, data_size);
    }
}

void read_cb(struct wx_conn_s* wx_conn, struct wx_buf_s* buf, char* lastbase, ssize_t nread) {
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

    wx_conn_closetimer_start(&conn->wx_conn, 10000);  // 停止上次等待新请求计时，给你10秒钟，如果还不发送完一个请求老子不伺候了

    struct wx_buf_s data;
    data.base = conn->bufchainwithbuf + sizeof(struct wx_buf_chain_s);
    data.size = sizeof(conn->bufchainwithbuf) - sizeof(struct wx_buf_chain_s) - buf->size;

    struct wx_buf_s rnrn = {.base="\r\n\r\n", .size=4};

    if (data.size > 3 && 0 < wx_buf_strstr(&data, &rnrn)) {
        int lastlnpos;
        char* data_base = data.base;
        size_t data_size = data.size;
        for (;;) {
            lastlnpos = find_char(data_base, data_size, '\n');
            if (lastlnpos == -1) {
                break;
            }
            lastlnpos++;
            do_line(conn, data_base, (size_t)lastlnpos);
            data_base += lastlnpos;
            data_size -= lastlnpos;
        }
    } else if (buf->size == 0) {
        connection_close(conn, -12); // buffer overflow
        wx_err("recv buffer overflow");
    }
}

void accept_cb(struct wx_worker_s* wk, int revents) {
    struct connection_s* conn = connection_get();
    if (!conn) {
        wx_err("no more free connections");
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
                connection_put(conn);
                return;
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                errno = 0; //reset it
            } else {
                wx_err("accept");
            }
            connection_put(conn);
            return;
        }
    }

    int p = fcntl(cfd, F_GETFL);
    if (-1 == p || -1 == fcntl(cfd, F_SETFL, p|O_NONBLOCK)) {
        wx_err("fcntl");
        connection_put(conn);
        return;
    }

    int one = 1;
    setsockopt(cfd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));

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

    char connection_buf[32]={0};
    size_t connection = 1024;
    if (0 == wx_conf_get("connection", connection_buf, sizeof(connection_buf))) {
        connection = (size_t)atoi(connection_buf);
    }

    struct wx_worker_s worker;
    wx_worker_init(listen_fd, &worker, accept_cb, alloc_cb, read_cb, closetimer_cb);

    if (0 != connections_alloc(&worker, (uint32_t)connection)) {
        wx_err("connections_alloc");
        return EXIT_FAILURE;
    }

    wx_dummyfd_open();

    int r = wx_worker_run(&worker);

    wx_err("worker stop");

    connections_free();
    if (-1 != wx_dummyfd_get()) {
        wx_dummyfd_close();
    }

//    ProfilerStop();

    return r;
}