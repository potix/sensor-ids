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
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <event.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include "macro.h"
#include "string_util.h"
#include "alert.h"
#include "sensor.h"
#include "tcpsock.h"
#include "rpc.h"

/* RPC コマンド */
#define COMMAND_START_MONITOR           "START_MONITOR"
#define COMMAND_STOP_MONITOR            "STOP_MONITOR"
#define COMMAND_GET_MONITOR_STATUS      "GET_MONITOR_STATUS"
#define COMMAND_CANCEL_ALERT            "CANCEL_ALERT"
#define COMMAND_GET_ALERT_STATUS        "GET_ALERT_STATUS"
#define COMMAND_CLEAR_ALERT_STATUS      "CLEAR_ALERT_STATUS"

/* RPC レスポンス */
#define RESPONSE_OK                     "OK\r\n"
#define RESPONSE_RUNNING                "RUNNING\r\n"
#define RESPONSE_STOPPING               "STOPPING\r\n"
#define RESPONSE_GOOD                   "GOOD\r\n"
#define RESPONSE_FIRST_ALERT            "FIRST ALERT\r\n"
#define RESPONSE_SECOND_ALERT           "SECOND ALERT\r\n"
#define RESPONSE_UNKNOWN_COMMAND        "UNKNOWN COMMAND\r\n"
#define RESPONSE_TIMEOUT                "TIMEOUT\r\n"
#define RESPONSE_INTERNAL_ERROR         "INTERNAL ERROR\r\n"

/* TCP ACCEPT前にしておきたい処理 */
static int 
rpc_accept_init(int sd, void *info) {
	return 0;
}

/* TCP ACCEPT後の処理 */
static void
rpc_accept_main(int sd, short event, void *info) {
	tcp_accept_info_t *acceptinfo = info;
	struct rpc *rpc = acceptinfo->args;
	FILE *sp = acceptinfo->accept_sp;
	char buffer[128] = "";

	if (event == EV_READ) {
		if (fgets(buffer, sizeof(buffer), sp) == NULL) {
			tcp_server_accept_clear(acceptinfo);
			return;
		}
		if (string_rstrip(buffer, "\r\n \t")) {
			fprintf(sp, RESPONSE_INTERNAL_ERROR);
			goto end;
		}
		if (strncmp(
		    buffer,
		    COMMAND_STOP_MONITOR,
		    sizeof(COMMAND_STOP_MONITOR) - 1) == 0 ) {
			alert_cancel(rpc->alert);
			sensor_monitor_stop(rpc->sensor);
			fprintf(sp, RESPONSE_OK);
		} else if (strncmp(
		    buffer,
		    COMMAND_START_MONITOR,
		    sizeof(COMMAND_START_MONITOR) - 1) == 0 ) {
			sensor_monitor_start(rpc->sensor);
			fprintf(sp, RESPONSE_OK);
		} else if (strncmp(
		    buffer,
		    COMMAND_GET_MONITOR_STATUS,
		    sizeof(COMMAND_GET_MONITOR_STATUS) - 1) == 0 ) {
			if (sensor_get_monitor_status(rpc->sensor)) {
				fprintf(sp, RESPONSE_RUNNING);
			} else {
				fprintf(sp, RESPONSE_STOPPING);
			}
		} else if (strncmp(
		    buffer,
		    COMMAND_CANCEL_ALERT,
		    sizeof(COMMAND_CANCEL_ALERT) - 1) == 0 ) {
			alert_cancel(rpc->alert);
			fprintf(sp, RESPONSE_OK);
		} else if (strncmp(
		    buffer,
		    COMMAND_GET_ALERT_STATUS,
		    sizeof(COMMAND_GET_ALERT_STATUS) - 1) == 0 ) {
			switch (alert_get_status(rpc->alert)) {
			case ALERT_STATUS_NO_ALERT:
				fprintf(sp, RESPONSE_GOOD);
				break;
			case ALERT_STATUS_FIRST_ALERT:
				fprintf(sp, RESPONSE_FIRST_ALERT);
				break;
			case ALERT_STATUS_SECOND_ALERT:
				fprintf(sp, RESPONSE_SECOND_ALERT);
				break;
			default:
				ABORT();
				/* NOT REACHED */
			}
		} else if (strncmp(buffer,
		     COMMAND_CLEAR_ALERT_STATUS,
		     sizeof(COMMAND_CLEAR_ALERT_STATUS) - 1) == 0 ) {
			alert_clear_status(rpc->alert);
			fprintf(sp, RESPONSE_OK);
		} else {
			fprintf(stderr, "rpc unknown command.\n");
			fprintf(sp, RESPONSE_UNKNOWN_COMMAND);
		}
	} else if (event == EV_TIMEOUT) {
		fprintf(stderr, "rpc timeout.\n");
		fprintf(sp, RESPONSE_TIMEOUT);
	} else {
		ABORT();        
                /* NOT REACHED */
	}
end:
	fflush(sp);
	tcp_server_accept_clear(acceptinfo);
	return;
}

/* accept終了時の処理 */
static int
rpc_accept_finish(int sd, void *info) {
	return 0;
}

/* listen前にしておきたい処理 */
static int
rpc_listen_init(int sd, void *args) {
	return 0;
}

/* listen終了時のの処理 */
static int
rpc_listen_finish(int sd, void *args) {
	return 0;
}

int
rpc_create(
    struct rpc **rpc,
    const char *bind_port,
    int rpc_timeout,
    struct alert *alert,
    struct sensor *sensor,
    struct event_base *event_base)
{
	struct rpc *inst = NULL;
	char *bport = NULL;

	*rpc = NULL;
	inst = malloc(sizeof(struct rpc));
	bport = strdup(bind_port);
	if (inst == NULL ||
	    bport == NULL) {
		goto fail;
	}
	memset(inst, 0, sizeof(struct rpc));
	inst->bind_port = bport;
	inst->rpc_timeout = rpc_timeout;
	inst->alert = alert;
	inst->sensor = sensor;
	inst->event_base = event_base;
	*rpc = inst;

	return 0;

fail:
	free(inst);
	free(bport);

	return 1;
}


int
rpc_start(struct rpc *rpc) {
	tcp_server_t *tcpserver = NULL;
	struct timeval timeout;

	/* タイムアウト値の設定 */
	timeout.tv_sec = rpc->rpc_timeout;
	timeout.tv_usec = 0;
	/* TCPサーバーの生成 */
	if (tcp_server_create(
	    &tcpserver,
	    NULL,
	    rpc->bind_port,
	    RECV_BUFF,
	    EV_READ,
	    &timeout,
	    rpc_accept_init,
	    rpc_accept_main,
	    rpc_accept_finish,
	    rpc_listen_init,
	    rpc_listen_finish,
	    rpc,
	    rpc->event_base)) {
		fprintf(stderr, "failed in create tcp server instance.\n");
		return 1;
	}
	rpc->tcpserver = tcpserver;
	/*
         * TCPサーバーの開始
         * イベントループで回る
         */ 
	if (tcp_server_start(rpc->tcpserver)) {
		fprintf(stderr, "failed in start up tcp server instance.\n");
		rpc->tcpserver = NULL;
	}

        /* 
         * TCPサーバーの削除
         */
	tcp_server_destroy(rpc->tcpserver);
	rpc->tcpserver = NULL;
        printf("tcp server end.\n");

	return 0;
}

void
rpc_finish(struct rpc *rpc) {
	if (rpc->tcpserver) {
		tcp_server_stop(rpc->tcpserver);
	}
}

void
rpc_destroy(struct rpc *rpc) {
	if (rpc) {
		free(rpc->bind_port);
		free(rpc);
	}
}
