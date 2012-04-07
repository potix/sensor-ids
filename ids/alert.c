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
#include "tcpsock.h"
#include "alert.h"

static int
alert_execute(char *cmd) {
	int st;
	switch (fork()) {
	case -1:
		fprintf(stderr, "failed in fork.");
                break;
        case 0:
                /* 子側 */
		/* 子プロセスの管理はinitへ作戦 */
		switch (fork()) {
		case -1:
			fprintf(stderr, "failed in fork.");
			break;
		case 0:
			/* child process */
			{
				char *shell = getenv("SHELL");
	                        execl(shell, shell, "-c", cmd, (char *)0);
				fprintf(stderr, "execl: couldn't exec (%s)", shell);
				exit(1);
			}
			break;
		default:
			/* parent process */
			break;
		}
                /* child process */
                exit(0);
                break;
        default:
                /* 親側 */
                /* 速やかにwait */
		wait(&st);
                break;
        }
	return 0;
}

static void
alert_start_second(int fd, short event, void *args) {
	struct alert *alert = args;

	if (event != EV_TIMEOUT) {
		ABORT();
		/* NOT REACHED */
	}
	/* 2次警報処理の開始 */
	alert->alert_status = ALERT_STATUS_SECOND_ALERT;
	alert_execute(alert->second_alert_script);
	alert->alert_processing = 0;
}

int
alert_create(
    struct alert **alert,
    const char *first_alert_script,
    const char *second_alert_script,
    int cancel_wait_time,
    struct event_base *event_base)
{
	struct alert *inst = NULL;
	char *nscript = NULL;
	char *ascript = NULL;

	*alert = NULL;
	inst = malloc(sizeof(struct alert));
	if (inst == NULL) {
		goto fail;
	}
	memset(inst, 0, sizeof(struct alert));
	if (first_alert_script) {
		nscript = strdup(first_alert_script);
		if (nscript == NULL) {
			goto fail;
		}
	}
	if (second_alert_script) {
		ascript = strdup(second_alert_script);
		if (ascript == NULL) {
			goto fail;
		}
	}
	inst->first_alert_script= nscript;
	inst->second_alert_script = ascript;
	inst->cancel_wait_time = cancel_wait_time;
	inst->event_base = event_base;
	*alert = inst;

	return 0;

fail:
	free(inst);
	free(nscript);
	free(ascript);

	return 1;
}

/* 1次警報処理の開始 */
int
alert_start_first(struct alert *alert) {
	struct timeval wait_time;
	
	alert->alert_status = ALERT_STATUS_FIRST_ALERT;
	if (alert->alert_processing) {
		return 0;
	}
	alert->alert_processing = 1;

	/* 1次警報処理スクリプトの実行 */
	alert_execute(alert->first_alert_script);

	/* 2時警報処理イベントを登録 */
	wait_time.tv_sec = alert->cancel_wait_time;
	wait_time.tv_usec = 0;
     	evtimer_set(&alert->second_alert_event, alert_start_second, alert);
     	event_base_set(alert->event_base, &alert->second_alert_event);
     	evtimer_add(&alert->second_alert_event, &wait_time);

	return 0;
}


void
alert_cancel(struct alert *alert) {
        /* 登録してあるイベントを削除 */
	evtimer_del(&alert->second_alert_event);
	alert->alert_processing = 0;
	alert->alert_status = ALERT_STATUS_NO_ALERT;
}

void
alert_destroy(struct alert *alert) {
	if (alert) {
		free(alert->first_alert_script);
		free(alert->second_alert_script);
		free(alert);
	}
}

void
alert_clear_status(struct alert *alert) {
	alert->alert_status = ALERT_STATUS_NO_ALERT;
}

int
alert_get_status(struct alert *alert) {
	return alert->alert_status;
}
