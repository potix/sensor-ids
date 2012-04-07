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
#include <limits.h>
#include <errno.h>

#include "macro.h"
#include "string_util.h"
#include "config.h"

/*
 * 更新関数雛型
 * 細かいvalidationはやる気ない
 */
#define CONFIG_UPDATE_STRING(name)					\
static int								\
config_update_##name(							\
    struct config *config,						\
    struct string_kv *kv) {						\
	char *tmp = strdup(kv->value);					\
	if (tmp == NULL) {						\
		return 1;						\
	}								\
	free(config->name);						\
	config->name = tmp;						\
									\
	return 0;							\
}
#define CONFIG_UPDATE_INT(name, min, max)				\
static int								\
config_update_##name(							\
    struct config *config,						\
    struct string_kv *kv) {						\
        char *endptr;							\
        long val;							\
									\
        val = strtol(kv->value, &endptr, 10);				\
        if (*endptr != '\0') {						\
                return 1;						\
        }								\
        if ((val == LONG_MIN || val == LONG_MAX) &&			\
	    errno == ERANGE) {						\
                return 1;						\
        }								\
        if (val == 0 && errno == EINVAL) {				\
                return 1;						\
        }								\
	if (val < (min) || val > (max)) {				\
		return 1;						\
	}								\
	config->name = (int)val;					\
									\
	return 0;							\
}

/* 更新関数定義 */
CONFIG_UPDATE_STRING(first_alert_script)
CONFIG_UPDATE_STRING(second_alert_script)
CONFIG_UPDATE_STRING(rpc_port)
CONFIG_UPDATE_STRING(pid_file_path)
CONFIG_UPDATE_INT(cancel_wait_time, 5, 86400)
CONFIG_UPDATE_INT(poll_interval, 1000, 255000)
CONFIG_UPDATE_INT(alert_threshold, 1, 65535)
CONFIG_UPDATE_INT(rpc_timeout, 5, 3600)

/* keyと処理関数の定義 */
struct config_key_map {
	const char *key;
	int (*update_func)(struct config *config, struct string_kv *kv);
};
struct config_key_map config_key_map[] = {
	{ "first_alert_script", config_update_first_alert_script },
	{ "second_alert_script", config_update_second_alert_script },
	{ "rpc_port", config_update_rpc_port },
	{ "pid_file_path", config_update_pid_file_path },
	{ "cancel_wait_time", config_update_cancel_wait_time },
	{ "poll_interval", config_update_poll_interval },
	{ "alert_threshold", config_update_alert_threshold },
	{ "rpc_timeout", config_update_rpc_timeout },
	{ NULL, NULL},
};

/* コンフィグ更新処理 */
static int
config_update(struct config *config, struct string_kv *kv) {
	int i;

	for (i = 0; config_key_map[i].key != NULL; i++) {
		if (strncasecmp(
		    config_key_map[i].key,
		    kv->key,
		    strlen(config_key_map[i].key)) == 0) {
			config_key_map[i].update_func(config, kv);
		}
	}

	return 0;
}

/* コンフィグのパース */
static int
config_parse(struct config *config, FILE *fp) {
	char line[256];
	struct string_kv kv;
	int linecnt = 0;

	while (fgets(line, sizeof(line), fp) != NULL) {
		linecnt++;
		if (string_rstrip(line, "\r\n")) {
			fprintf(stderr, "reverse strip error (%d: %s)\n", linecnt, line);
			return 1;
		}
		if (line[0] == '\0' || line[0] == '#') {
			continue;
		}
		if (string_kv_split(&kv, line, "=")) {
			fprintf(stderr, "sprint error (%d: %s)\n", linecnt, line);
			return 1;
		}
		if (config_update(config, &kv)) {
			fprintf(stderr, "update error (%d: %s)\n", linecnt, line);
			return 1;
		}
	}
	return 0;
}

int
config_create(
    struct config **config,
    const char *first_alert_script,
    const char *second_alert_script,
    int cancel_wait_time,
    int poll_interval,
    int alert_threshold,
    const char *rpc_port,
    int rpc_timeout,
    const char *pid_file_path)
{
	struct config *inst = NULL;
	char *fascript = NULL;
	char *sascript = NULL;
	char *rport = NULL;
	char *pfpath = NULL;

	inst = malloc(sizeof(struct config));
	memset(inst, 0, sizeof(struct config));
	fascript = strdup(first_alert_script);
	if (fascript == NULL) {
		goto fail;
	}
	sascript = strdup(second_alert_script);
	if (sascript == NULL) {
		goto fail;
	}
	rport = strdup(rpc_port);
	if (rport == NULL) {
		goto fail;
	}
	pfpath = strdup(pid_file_path);
	if (pfpath== NULL) {
		goto fail;
	}
	inst->first_alert_script = fascript;
	inst->second_alert_script = sascript;
	inst->rpc_port = rport;
	inst->pid_file_path = pfpath;
	inst->cancel_wait_time = cancel_wait_time;
	inst->poll_interval = poll_interval;
	inst->alert_threshold = alert_threshold;
	inst->rpc_timeout = rpc_timeout;
	*config = inst;

	return 0;

fail:
	free(fascript);
	free(sascript);
	free(rport);
	free(pfpath);
	free(inst);

	return 1;
}

int
config_load(struct config *config, const char *config_path) {
	FILE *fp;
	fp = fopen(config_path, "r");
	if (fp == NULL) {
		return 1;
	}
	if (config_parse(config, fp)) {
		fclose(fp);
		return 1;
	}
	fclose(fp);

	return 0;
}

void
config_print(struct config *config) {
	printf("first_alert_script = %s\n", config->first_alert_script);
	printf("second_alert_script = %s\n", config->second_alert_script);
	printf("rpc_port = %s\n", config->rpc_port);
	printf("pid_file_path = %s\n", config->pid_file_path);
	printf("cancel_wait_time = %d\n", config->cancel_wait_time);
	printf("poll_interval = %d\n", config->poll_interval);
	printf("alert_threshold = %d\n", config->alert_threshold);
	printf("rpc_timeout = %d\n", config->rpc_timeout);
}

void
config_destroy(struct config *config) {
	free(config->first_alert_script);
	free(config->second_alert_script);
	free(config->rpc_port);
	free(config->pid_file_path);
	free(config);
}
