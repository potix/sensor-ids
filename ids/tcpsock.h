/* Copyright (c) 2010 Hiroyuki Kakine
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef TCPSOCK_H
#define TCPSOCK_H

#define REUSE_ADDR
#define LISTEN_LIMIT	10	/* ipv4, ipv6のプロトコルファミリーでlisten, bindするアドレス数の限界値*/
#define ACCEPT_LIMIT	10	/* liten,bind毎のacceptするセッション数 */
#define TCP_LIMIT	LISTEN_LIMIT
#define RECV_BUFF       (256 * 4)

typedef struct tcp_accept_info tcp_accept_info_t;
typedef struct tcp_accept tcp_accept_t;
typedef struct tcp_server tcp_server_t;

struct tcp_accept_info {
	int accept_sd;						/* acceptしたsd */
	FILE *accept_sp;					/* sdをfdopenしたもの */
	void *args;						/* コールバックに渡す引数 */
	struct event accept_event;				/* acceptしたsd用のevent構造体 */
	socklen_t sa_st_len;					/* sockaddrの長さ */
	struct sockaddr_storage sa_st;				/* 接続を受け付けた相手のアドレス情報 */
	tcp_accept_t *tcpaccept;				/* tcpaccept へのポインタ */
};

struct tcp_accept{
	int accept_idx;						/* accept コンテキストの番号 */
	tcp_accept_info_t tcpacceptinfo[ACCEPT_LIMIT];		/* acceptした際の情報を入れる構造体
								 * init_cbとmain_cbコールバックの引数にはこれを渡す */
	tcp_server_t *tcpserver;				/* tcp_server_へのバックポインタ */
        int (*init_accept_cb)(int sd, void *);          	/* accept直後の初期化用のコールバック
								   void *引数にはtcp_accept_infoを渡す */
        void (*main_accept_cb)(int sd, short event, void *);	/* accept後の送信や受信イベントが発生した際に呼ばれる関数
								   void *引数にはtcp_accept_infoを渡す */
        int (*finish_accept_cb)(int sd, void *);        	/* accept処理の停止を行いたい場合に呼ぶ関数 */
};

struct tcp_server{
	int tcp_listen_run;				/* tcpのlistenがうごいているかどうか */
	char *address;					/* bindするアドレス */
	char *port;					/* bindするポート番号 */
	int recvbuf;					/* 受信バッファ */
	int listen_sd[LISTEN_LIMIT];			/* listen sd の配列 */
	int listen_sd_array_max;			/* listen sd 配列の最大個数 */
	struct event_base *event_base;			/* libeventのevent base */
	struct event listen_events[LISTEN_LIMIT];	/* listenしたsd用のevent構造体 */
        tcp_accept_t tcpaccept[LISTEN_LIMIT];	/* tcp accept の構造体 */
	void *args;					/* コールバックに渡す引数 */
        short event; 			                /* 監視するイベントのフラグ (libevent由来) */
        struct timeval *timeout;			/* タイムアウト */
        int (*init_listen_cb)(int sd, void *);          /* accept直後の初期化用のコールバック */
        int (*finish_listen_cb)(int sd, void *);        /* listen処理の停止を行いたい場合に呼ぶ関数 */
        struct event stop_event;			/* 終了するときにeventを抜けさせる */
        int stop_fd[2];					/* pipe */
};


/*
 * tcp serverの関数
 */
/*
 * acceptしたソケットを閉じたい場合にcallする
 * accept_init_cb, accept_main_cb内でcallする
 * これを呼んだ後は、ソケットディスクリプタは
 *  closeされるのでread/writeできなくなる
 * 通常はread()で0以下が返ってきた場合に
 * ソケットをクリアして終了する処理部分でcallする
 */
void tcp_server_accept_clear(tcp_accept_info_t *tcpacceptinfo);

/*
 * tcp serverのコンテキストを作成する
 */
int tcp_server_create(
    tcp_server_t **tcpserver,
    const char *address,
    const char *port,
    int recvbuf,
    short event,
    struct timeval *timeout,
    int (*init_accept_cb)(int sd, void *acceptinfo),
    void (*main_accept_cb)(int sd, short event, void *acceptinfo),
    int (*finish_accept_cb)(int sd, void *acceptinfo),
    int (*init_listen_cb)(int sd, void *args),
    int (*finish_listen_cb)(int sd, void *args),
    void *args,
    struct event_base *event_base);

/*
 * tcp serverのコンテキストを作成する
 * tcp serverを終了しておく必要がある
 */
void tcp_server_destroy(tcp_server_t *tcpserver);

/*
 * tcp serverを開始する
 */
int tcp_server_start(tcp_server_t *tcpserver);

/*
 * tcp serverを停止する
 */
int tcp_server_stop(tcp_server_t *tcpserver);

#endif
