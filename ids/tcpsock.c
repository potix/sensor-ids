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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <event.h>
#include <semaphore.h>
#include <stdint.h>
#include <time.h>

#include "macro.h"
#include "tcpsock.h"

static void
tcp_server_accept(int listen_sd, short event, void *args){
	tcp_accept_t *tcpaccept;
	tcp_server_t *tcpserver;
	int i;

	tcpaccept = args;
	tcpserver = tcpaccept->tcpserver;

	if (event != EV_READ) {
		fprintf(stderr, "not event read (accept).\n");
		return;
	}

	for (i = 0; i < ACCEPT_LIMIT; i++) {
		if (tcpaccept->tcpacceptinfo[i].accept_sd == -1)
			break;
	}
	if (i == ACCEPT_LIMIT) {
		fprintf(stderr, "server connection is full.\n");
		return;
	}
	tcpaccept->tcpacceptinfo[i].sa_st_len = sizeof(struct sockaddr_storage);
	tcpaccept->tcpacceptinfo[i].accept_sd = accept(listen_sd,
	    (struct sockaddr *)&(tcpaccept->tcpacceptinfo[i].sa_st), &(tcpaccept->tcpacceptinfo[i].sa_st_len));
	if (tcpaccept->tcpacceptinfo[i].accept_sd < 0) {
		fprintf(stderr, "failed in accept.\n");
		return;
	}
	tcpaccept->tcpacceptinfo[i].accept_sp = fdopen(tcpaccept->tcpacceptinfo[i].accept_sd, "r+");
	if (tcpaccept->tcpacceptinfo[i].accept_sp == NULL) {
		close(tcpaccept->tcpacceptinfo[i].accept_sd);
		tcpaccept->tcpacceptinfo[i].accept_sd = -1;
		fprintf(stderr, "failed in fdopen.\n");
		return;
	}
	if (tcpaccept->init_accept_cb) {
		if (tcpaccept->init_accept_cb(
		    tcpaccept->tcpacceptinfo[i].accept_sd,
		    &tcpaccept->tcpacceptinfo[i])) {
			fprintf(stderr, "failed in initialize of accept.\n");
			goto fail;
		}
	}
	event_set(&tcpaccept->tcpacceptinfo[i].accept_event, tcpaccept->tcpacceptinfo[i].accept_sd,
	    tcpserver->event, tcpaccept->main_accept_cb, &tcpaccept->tcpacceptinfo[i]);
	if (event_base_set(tcpserver->event_base, &tcpaccept->tcpacceptinfo[i].accept_event)) {
		fprintf(stderr, "failed in set event of accept.\n");
		goto fail;
	}
	if (event_add(&tcpaccept->tcpacceptinfo[i].accept_event, tcpserver->timeout) < 0 ) {
		fprintf(stderr, "failed in add evet of accept.\n");
		goto fail;
	}

	return;

fail:
	fclose(tcpaccept->tcpacceptinfo[i].accept_sp);
	tcpaccept->tcpacceptinfo[i].accept_sd = -1;
	return;
}

static int
tcp_server_accept_stop(tcp_accept_t *tcpaccept) {
	int i;

	/*登録していたlistenのイベントを削除*/
	for (i = 0; i < ACCEPT_LIMIT; i++) {
		if (tcpaccept->tcpacceptinfo[i].accept_sd == -1) {
			continue;
		}
		tcpaccept->finish_accept_cb(
		    tcpaccept->tcpacceptinfo[i].accept_sd,
		    &tcpaccept->tcpacceptinfo[i]);
	}
	for (i = 0; i < ACCEPT_LIMIT; i++) {
		if (tcpaccept->tcpacceptinfo[i].accept_sd == -1) {
			continue;
		}
		if (event_del(&tcpaccept->tcpacceptinfo[i].accept_event)) {
			fprintf(stderr, "failed in delete evet of accept.\n");
		}
		fclose(tcpaccept->tcpacceptinfo[i].accept_sp);
		tcpaccept->tcpacceptinfo[i].accept_sd = -1;
	}

	return 0;
}


static void
tcp_server_listen_clear(struct addrinfo *addr_info_res0)
{
	freeaddrinfo(addr_info_res0);
}

void
tcp_server_accept_clear(tcp_accept_info_t *tcpacceptinfo) {

	if (event_del(&tcpacceptinfo->accept_event)) {
		fprintf(stderr, "failed in delete of accept event.\n");
	}
	close(tcpacceptinfo->accept_sd);
	tcpacceptinfo->accept_sd = -1;
}

int
tcp_server_create(
    tcp_server_t **tcpserver,
    const char *address,
    const char *port,
    int recvbuf,
    short event,
    struct timeval *timeout,
    int (*init_accept_cb)(int sd, void *args),
    void (*main_accept_cb)(int sd, short event, void *args),
    int (*finish_accept_cb)(int sd, void *args),
    int (*init_listen_cb)(int sd, void *args),
    int (*finish_listen_cb)(int sd, void *args),
    void *args,
    struct event_base *event_base)
{
	int i, j;
	char *dup_addr = NULL;
	char *dup_port = NULL;
	tcp_server_t *inst = NULL;
	int pfd[2] = { -1, -1 };

	ASSERT(tcpserver != NULL);
	ASSERT(port != NULL);
	ASSERT(main_accept_cb != NULL);

	*tcpserver = NULL;
	dup_port = strdup(port);
	if (dup_port == NULL) {
		fprintf(stderr, "failed in copy port.\n");
		goto fail;
	}
	if (address && address[0] != '\0') {
		dup_addr = strdup(address);
		if (dup_addr == NULL) {
			fprintf(stderr, "failed in copy address.\n");
			goto fail;
		}
	}
	if (pipe(pfd) == -1) {
		fprintf(stderr, "failed in make pipe.\n");
		goto fail;
	}
	inst = (tcp_server_t *)malloc(sizeof(tcp_server_t));
	if (inst == NULL) {
		fprintf(stderr, "failed in allocate memory of tcp server context.\n");
		return 1;
	}
	memset(inst, 0, sizeof(tcp_server_t));
        for (i = 0; i < LISTEN_LIMIT; i++) {
                inst->listen_sd[i] = -1;
		inst->tcpaccept[i].accept_idx = i;
		for (j = 0; j < ACCEPT_LIMIT; j++) {
			inst->tcpaccept[i].tcpacceptinfo[j].accept_sd = -1;
			inst->tcpaccept[i].tcpacceptinfo[j].args = args;
			inst->tcpaccept[i].tcpacceptinfo[j].tcpaccept
			    = &inst->tcpaccept[i];
		}
		inst->tcpaccept[i].init_accept_cb = init_accept_cb;
		inst->tcpaccept[i].main_accept_cb = main_accept_cb;
		inst->tcpaccept[i].finish_accept_cb = finish_accept_cb;
		inst->tcpaccept[i].tcpserver = inst;
        }
	inst->listen_sd_array_max = 0;
        inst->address = dup_addr;
        inst->port = dup_port;
        inst->recvbuf = recvbuf;
        inst->args = args;
        inst->event = event;
        inst->timeout = timeout;
        inst->init_listen_cb = init_listen_cb;
        inst->finish_listen_cb = finish_listen_cb;
	inst->event_base = event_base;
	*tcpserver  = inst;

	return 0;
fail:
	if (pfd[0] != -1) {
		close(pfd[0]);
	}
	if (pfd[1] != -1) {
		close(pfd[1]);
	}
	free(dup_addr);
	free(dup_port);
	free(inst);
	return 1;
}

void
tcp_server_destroy(tcp_server_t *tcpserver)
{
	free(tcpserver->address);
	free(tcpserver->port);
	free(tcpserver);
}

int
tcp_server_start(tcp_server_t *tcpserver)
{
	struct addrinfo addr_info_hints, *addr_info_res, *addr_info_res0;
	int sd[LISTEN_LIMIT];
        int sock_max;
        int sarray_max;
	int i = 0, j;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	int error;

#ifdef  IPV6_V6ONLY
	int sopt;
        const int v6only = 1;
#endif
#ifdef REUSE_ADDR
        const int reuse_addr = 1;
#endif
#ifdef REUSE_PORT
        const int reuse_port = 1;
#endif
	ASSERT(tcpserver != NULL);

	if (tcpserver->port == NULL ||  *(tcpserver->port) == '\0') {
		fprintf(stderr, "invalid port.\n");
		return 1;
	}
	memset(&addr_info_hints, 0, sizeof(addr_info_hints));
	addr_info_hints.ai_socktype = SOCK_STREAM;
	addr_info_hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(tcpserver->address, tcpserver->port, &addr_info_hints, &addr_info_res0);
	if (error) {
		fprintf(stderr,
		    "invalid address or port address = (%s), port = (%s): (%s).\n",
		    tcpserver->address, tcpserver->port, gai_strerror(error));
		return 1;
	}
        sock_max = -1;
        sarray_max = 0;
	for (addr_info_res = addr_info_res0;
	     addr_info_res && sarray_max < LISTEN_LIMIT;
	     addr_info_res = addr_info_res->ai_next) {
		sd[sarray_max] = socket(addr_info_res->ai_family,
		    addr_info_res->ai_socktype, addr_info_res->ai_protocol);

		if (sd[sarray_max] < 0) {
			fprintf(stderr, "failed in create socket.\n");
			continue;
		}
#ifdef IPV6_V6ONLY
		sopt = setsockopt(sd[sarray_max], 
		    IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
        	if (addr_info_res->ai_family == AF_INET6 && sopt < 0) {
			fprintf(stderr, "failed in setsockopt (V6ONLY).\n");
			close(sd[sarray_max]);
			sd[sarray_max] = -1;
			continue;
		}
#endif
#ifdef REUSE_ADDR
		if (setsockopt(sd[sarray_max],
		    SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr))) {
			fprintf(stderr, "failed in setsockopt (SO_REUSEADDR).\n");
			close(sd[sarray_max]);
			sd[sarray_max] = -1;
			continue;
		}
#endif
                if (setsockopt(sd[sarray_max],
		    SOL_SOCKET, SO_RCVBUF, &tcpserver->recvbuf, sizeof(tcpserver->recvbuf)) < 0) {
			fprintf(stderr, "failed in setsockopt (SO_RCVBUF).\n");
			close(sd[sarray_max]);
			sd[sarray_max] = -1;
			continue;
                }
		if (bind(sd[sarray_max], addr_info_res->ai_addr, addr_info_res->ai_addrlen) < 0) {
			fprintf(stderr, "failed in bind addr_info_res.\n");
			close(sd[sarray_max]);
			sd[sarray_max] = -1;
			continue;
		}

		/* backlogはACCEPT_LIMITと同じ値 */
		if (listen(sd[sarray_max], ACCEPT_LIMIT) < 0) {
			fprintf(stderr, "failed in listen.\n");
			close(sd[sarray_max]);
			sd[sarray_max] = -1;
			continue;
		}

		error = getnameinfo(addr_info_res->ai_addr, addr_info_res->ai_addrlen,
		    hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), 0);
		if (error)  {
			fprintf(stderr,
			    "failed in getnameinfo address = %s, port = %s (%s).\n",
			    tcpserver->address, tcpserver->port, gai_strerror(error));
		} else {
			printf("tcp listen: address = %s, port = %s.\n", hbuf, sbuf);
		}

		if (sd[sarray_max] > sock_max)
			sock_max = sd[sarray_max];

		sarray_max++;
	}

	tcpserver->listen_sd_array_max = sarray_max;

	if (sock_max == -1) {
		fprintf(stderr, "not there avilable socket of listen.\n");
		freeaddrinfo(addr_info_res0);
		return 1;
	}

	/* 来たコネクションをacceptするためのイベント登録 */
	for (i = 0; i < sarray_max; i++) {
		tcpserver->listen_sd[i] = sd[i];
		if (tcpserver->init_listen_cb) {
			if (tcpserver->init_listen_cb(sd[i], &tcpserver->args)) {
				fprintf(stderr, "failed in initialized of listen.\n");
				goto fail;
			}
		}
		event_set(&tcpserver->listen_events[i], sd[i],
		    EV_READ | EV_PERSIST, tcp_server_accept, &tcpserver->tcpaccept[i]);
		if (event_base_set(tcpserver->event_base, &tcpserver->listen_events[i])) {
			fprintf(stderr, "failed in set event of listen.\n");
			goto fail;
		}
		if (event_add(&tcpserver->listen_events[i], NULL) < 0) {
			fprintf(stderr, "failed in add event of listen.\n");
			goto fail;
		}
	}
	tcpserver->tcp_listen_run = 1;
	/* dispatch */
	if (event_base_dispatch(tcpserver->event_base) < 0) {
		fprintf(stderr, "failed in dispatch event.\n");
		goto fail;
	}
        tcp_server_listen_clear(addr_info_res0);
	return 0;

fail:
	for (j = 0; j < i; j++) {
		if (event_del(&tcpserver->listen_events[j])) {
			fprintf(stderr, "failed in delete event of listen.\n");
		}
		close(tcpserver->listen_sd[j]);
	}
        tcp_server_listen_clear(addr_info_res0);
	return 1;
}

int
tcp_server_stop(tcp_server_t *tcpserver) {
	int i;
	tcp_accept_t *tcpaccept;

	/* 登録していたlistenのイベントを削除 */
	for (i = 0; i < tcpserver->listen_sd_array_max; i++) {
		tcpaccept = &tcpserver->tcpaccept[i];
		tcp_server_accept_stop(tcpaccept);
		tcpserver->finish_listen_cb(tcpserver->listen_sd[i], tcpserver->args);
	}
	tcpserver->tcp_listen_run = 0;
	for (i = 0; i < tcpserver->listen_sd_array_max; i++) {
		if (event_del(&tcpserver->listen_events[i])) {
			fprintf(stderr, "failed in delete event of listen.\n");
		}
		close(tcpserver->listen_sd[i]);
		tcpserver->listen_sd[i] = -1;
	}

	return 0;
}
