#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include "lib/wxworker.h"
#include "connection.h"

struct uuid_s {
    uint64_t ms:41; // (1<<42)-1 = 4398046511 103
    uint32_t gpid:9;//分布式进程id (1<<10)-1 = 1023
    uint32_t count:13;//分布式进程id (1<<14)-1 = 16383
};

void timer_cb_close(struct wx_timer_s* close_timer) {
    struct connection_s* conn = container_of(close_timer, struct connection_s, close_timer);
    wx_timer_stop(close_timer);
    wx_read_stop(&conn->wx_conn);
    wx_write_stop(&conn->wx_conn);
    wx_fire_outbuf_chain_cleanup(&conn->wx_conn, -10);
    connection_put(conn);
}

struct wx_buf_s* alloc_cb(struct wx_conn_s* wx_conn, size_t suggested) {
    struct connection_s* conn = (struct connection_s*)wx_conn;
    struct wx_buf_s* buf = (struct wx_buf_s*)conn->recvbuf;
    buf->base = conn->recvbuf + sizeof(struct wx_buf_s) + sizeof(struct wx_buf_chain_s) + conn->recvlen;
    buf->size = sizeof(conn->recvbuf) - (sizeof(struct wx_buf_s) + sizeof(struct wx_buf_chain_s)) - conn->recvlen;
    return buf;
}

void cleanup(struct wx_conn_s* wx_conn, struct wx_buf_chain_s* out_bufc, int status) {
    wx_write_stop(wx_conn);

    struct connection_s* conn = (struct connection_s*)wx_conn;
    connection_put(conn);
}
void cleanup_free(struct wx_conn_s* wx_conn, struct wx_buf_chain_s* out_bufc, int status) {
    free(out_bufc);
}

struct uuid_s uuid_last = {0};
int64_t create_uuid() {
    struct uuid_s uuid;
    uuid.gpid = getpid()&((1<<10)-1); // max = (1<<10)-1 = 1023，最好在配置文件中做配置一个id
    uuid.count = 1;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    uuid.ms = tv.tv_sec*1000 + tv.tv_usec/1000;

    if (uuid.ms == uuid_last.ms) {
        uuid.count = uuid_last.count + 1; // max = (1<<14)-1 = 16383
    }

    uuid_last = uuid;

    int64_t r;
    r = uuid.count;
    r |= uuid.ms<<13;
    r |= (int64_t)uuid.gpid<<54;

    return r;
}

void do_request(struct connection_s* conn, const char* buf, size_t buflen) {
    struct wx_buf_chain_s* bc = NULL;
    if (NULL == (char*)conn->wx_conn.out_bufc) {
        bc = (struct wx_buf_chain_s*)conn->sendbuf;
        bc->buf->base = conn->sendbuf + sizeof(struct wx_buf_chain_s);
        bc->cleanup = NULL;
    } else {
        bc = (struct wx_buf_chain_s*)malloc(sizeof(conn->sendbuf));
        bc->buf->base = (char*)buf + sizeof(struct wx_buf_chain_s);
        bc->cleanup = cleanup_free;
    }
    bc->next = NULL;
    bc->buf->size = RESPONSE_LEN;

    int64_t uuid = create_uuid();

    sprintf(bc->buf->base, "len:%d%lld", RESPONSE_LEN, uuid);

    wx_write_start(&conn->wx_conn, conn->fd, bc);
}

void read_cb(struct wx_conn_s* wx_conn, struct wx_buf_s* buf, ssize_t nread) {
    struct connection_s* conn = (struct connection_s*)wx_conn;
    if (nread <= 0) {
        if (nread==0 || errno != EAGAIN) {
            wx_read_stop(&conn->wx_conn);
            connection_put(conn);
        }
        return;
    }

    conn->recvlen += nread;

    char* buf_base = (char*)buf + sizeof(struct wx_buf_s) + sizeof(struct wx_buf_chain_s);
    if (conn->recvlen == REQUEST_LEN) {
        conn->recvlen = 0;
        buf->base = buf_base;
        buf->size = REQUEST_LEN;
        do_request(conn, buf_base, REQUEST_LEN);
    } else {
        buf->base += nread;
        buf->size -= nread;
    }

    /*struct wx_buf_chain_s* bufc = (struct wx_buf_chain_s*)((char*)buf + sizeof(struct wx_buf_s));
    bufc->next = NULL;
    bufc->buf = buf;
    bufc->cleanup = cleanup;
    wx_write_start(wx_conn, wx_conn->rwatcher.fd, bufc);*/
}

void accept_cb(struct wx_worker_s* wk, int revents) {
    struct connection_s* conn = connection_get();
    if (!conn) {
        wx_err("no more free connections");
        return;
    }
    
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


int main(int argc, char** argv) {
    int listen_fd = -1;
    char* evnptr = getenv("LISTEN_FD");
    if (evnptr) {
        listen_fd = atoi(evnptr);
    }
    if (listen_fd < 0) {
        wx_err("listen_fd < 0");
        return EXIT_FAILURE;
    }

    struct wx_worker_s worker;
    wx_worker_init(listen_fd, NULL, &worker);

    connections_alloc(&worker, 1024);

    int r = wx_worker_run(&worker, before_loop, NULL);

    wx_err("worker stop");

    connections_free();

    return r;
}