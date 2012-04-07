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
#ifndef ALERT_H
#define ALERT_H

#define DEFAULT_FIRST_ALERT_SCRIPT	"/var/ids/first_alert_script.sh"
#define DEFAULT_SECOND_ALERT_SCRIPT	"/var/ids/second_alert_script.sh"
#define DEFAULT_CANCEL_WAIT_TIME 	60

#define ALERT_STATUS_NO_ALERT     0
#define ALERT_STATUS_FIRST_ALERT  1
#define ALERT_STATUS_SECOND_ALERT 2

struct alert {
	struct event_base *event_base;
	char *first_alert_script;        /* 1次警報のスクリプトファイルパス */
	char *second_alert_script;       /* 2次警報のスクリプトファイルパス */
	struct event second_alert_event; /* ２次警報のイベント */
	int cancel_wait_time;            /* cancel待ちの猶予時間 */
	int alert_processing;            /* アラート処理中フラグ */
	int alert_status;                /* アラートの状態 */
};

/* alertのインスタンスを生成 */
int alert_create(
    struct alert **alert,
    const char *notice_script,
    const char *alert_script,
    int cancel_wait_time,
    struct event_base *event_base);
/* 1次警報処理を開始する */
int alert_start_first(
    struct alert *alert);
/* alert処理をキャンセルする*/
void alert_cancel(
    struct alert *alert);
/* alertのインスタンスを削除する */
void alert_destroy(
    struct alert *alert);
/* alertステータスをクリアする */
void alert_clear_status(
    struct alert *alert);
/* alertステータスを取得する */
int alert_get_status(
    struct alert *alert);

#endif
