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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <event.h>

#include "macro.h"
#include "config.h"
#include "alert.h"
#include "sensor.h"
#include "rpc.h"
#include "ids.h"

static void
terminate(int fd, short event, void *args) {
	struct ids *ids = args;

	printf("catch signal\n");
        /* 終了処理 */
	if (event != EV_SIGNAL) {
		ABORT();	
		/* NOT REACHED */
	}
     	signal_del(&ids->hup_event);
     	signal_del(&ids->term_event);
     	signal_del(&ids->int_event);
	alert_cancel(ids->alert);
	sensor_finish(ids->sensor);
	rpc_finish(ids->rpc);
}

static int 
make_pidfile(const char *path) {
        FILE *fp;

        /* 二重起動防止 */
	if (access(path, R_OK|W_OK) == 0) {
		fprintf(stderr, "already exist process id file.\n");
		return 1;
	}
        /* ファイル生成 */
        fp = fopen(path, "w+");
	if (fp == NULL) {	
		fprintf(stderr, "failed in open process id file.\n");
		return 1;
        }
        fprintf(fp, "%d\n", getpid());
        fclose(fp);

        return 0;
}

static void
usage(char *cmd)
{
	printf("%s [-c <config>]\n", cmd);
}

static int
get_args(struct ids *ids, int argc, char **argv)
{
	int opt;

	ids->config_path = DEFAULT_CONFIG_FILE_PATH;
	while ((opt = getopt(argc, argv, "c:")) != -1) {
		switch (opt) {
		case 'c':
			ids->config_path = optarg;
			break;
		default:
			return 1;
		}
	}
	return 0;
}

int 
main(int argc, char **argv)
{
	int error = 0;
	struct ids ids;
	struct config *config = NULL;
	struct sensor *sensor = NULL;
	struct alert *alert = NULL;
	struct rpc *rpc = NULL;
	struct event_base *event_base;

	memset(&ids, 0, sizeof(ids));
        /* 引数チェック */
	if (get_args(&ids, argc, argv)) {
		usage(argv[0]);
		return 1;
	}
        /* config作成＆初期化 */
	if (config_create(
	    &config,
	    DEFAULT_FIRST_ALERT_SCRIPT,
	    DEFAULT_SECOND_ALERT_SCRIPT,
	    DEFAULT_CANCEL_WAIT_TIME,
	    DEFAULT_POLL_INTERVAL,
	    DEFAULT_ALERT_THRESHOLD,
	    DEFAULT_RPC_PORT,
	    DEFAULT_RPC_TIMEOUT,
	    DEFAULT_PID_FILE_PATH)) {
		fprintf(stderr, "failed in create cofig instance.\n");
		return 1;
	}
        /* 設定読み込み */
	printf("config path = %s\n", ids.config_path);
	if (config_load(config, ids.config_path)) {
		fprintf(stderr, "failed in load config.\n");
		return 1;
	}
	config_print(config);
	/* デーモン化 */
	if (daemon(1, 1)) {
		fprintf(stderr, "failed in daemon.\n");
		return 1;
	}
        /*
         * pidファイル作る
         * 二重起動防止のチェックもする
         */
	if (make_pidfile(config->pid_file_path)) {
		fprintf(stderr, "failed in make process id file (%s).\n", config->pid_file_path);
		return 1;
	}
        /* スレッド使わないけど、今後変えるかも的な */
        event_base = event_init();
	ids.event_base = event_base;
        /* アラート生成 */
	if (alert_create(
	    &alert,
	    config->first_alert_script,
	    config->second_alert_script,
	    config->cancel_wait_time,
	    event_base)) {
		fprintf(stderr, "failed in create alert instance.\n");
		error = 1;
		goto finish;
	}
	ids.alert = alert;
        /* センサー生成 */
	if (sensor_create(&sensor,
	    alert,
	    config->poll_interval,
	    config->alert_threshold,
	    event_base)) {
		fprintf(stderr, "failed in create sensor instance.\n");
		error = 1;
		goto finish;
	}
	ids.sensor = sensor;
        /* rpc生成 */
	if (rpc_create(&rpc,
	     config->rpc_port,
	     config->rpc_timeout,
	     alert,
	     sensor,
	     event_base)) {
		fprintf(stderr, "failed in create rpc instance.\n");
		error = 1;
		goto finish;
	}
	ids.rpc = rpc;
	/*
         * hup, int, termのシグナルがきたら終了する
         * reloadは実装する気ない
	 */
	signal_set(&ids.hup_event, SIGHUP, terminate, &ids);
	event_base_set(event_base, &ids.hup_event);
     	signal_add(&ids.hup_event, NULL);
	signal_set(&ids.term_event, SIGTERM, terminate, &ids);
	event_base_set(event_base, &ids.term_event);
     	signal_add(&ids.term_event, NULL);
	signal_set(&ids.int_event, SIGINT, terminate, &ids);
	event_base_set(event_base, &ids.int_event);
     	signal_add(&ids.int_event, NULL);
	/* SIGPIPEは無視 */
	signal( SIGPIPE , SIG_IGN ); 

        /* センサーポーリング開始 */
	if (sensor_start(sensor)) {
		fprintf(stderr, "failed in start up  sensor.\n");
		error = 1;
		goto finish;
	}
	/* 
         * RPC開始 
         * RPC内のイベントループに入る。
         */
	if (rpc_start(rpc)) {
		fprintf(stderr, "failed in start up  sensor.\n");
		error = 1;
		goto finish;
	}
	printf("program ending.\n");

finish:
        /* RPC削除 */
	rpc_destroy(rpc);
        /* センサー削除 */
	sensor_destroy(sensor);
        /* アラート削除 */
	alert_destroy(alert);
        /* config削除 */
	config_destroy(config);
        /* pidファイル削除 */
	unlink(DEFAULT_PID_FILE_PATH);

	return error;
}
