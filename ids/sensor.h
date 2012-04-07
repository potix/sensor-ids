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
#ifndef SENSOR_H
#define SENSOR_H

#define USB_VENDOR      0x04bb /* IODATA */
#define USB_PRODUCT     0x0f04 /* SENSOR-HM/ECO */
#define DEVICE_TIMEOUT  (10 * 1000)
#define DEFAULT_POLL_INTERVAL	5000
#define DEFAULT_ALERT_THRESHOLD 12

struct sensor {
	struct event poll_event;
	struct event_base *event_base;
        struct usb_bus *bus;
        struct usb_device *dev;
        struct usb_dev_handle *dh;
	unsigned long detect_count;     /* 連続検出回数 */
	int execute_alert;              /* alertの処理を行うかどうかのフラグ */
	struct alert *alert;            /* alertのインスタンス */
        int poll_interval;              /* ポーリング間隔 */
        int alert_threshold;            /* 警報処理を開始する検出回数の境界値
                                         * 小さすぎると誤検知、大きすぎると検知しない
                                         */
};

/* sensorのインスタンスを生成 */
int sensor_create(
    struct sensor **sensor,
    struct alert *alert,
    int poll_interval,
    int alert_threshold,
    struct event_base *event_base);
/*
 * センサーの初期化をポーリングを開始
 * pollingはlibeventのタイマーで行う
 */
int sensor_start(
    struct sensor *sensor);
/*
 * タイマーイベントを削除しつつ、
 * センサーの開放処理を行う
 */
void sensor_finish(
    struct sensor *sensor);
/* インスタンス削除 */
void sensor_destroy(
    struct sensor *sensor);
/* alert処理をするようにする */
void sensor_monitor_start(
    struct sensor *sensor);
/* alert処理をしないようにする */
void sensor_monitor_stop(
    struct sensor *sensor);
/* alert処理をしているかしていないかの状態を返す */
int sensor_get_monitor_status(
    struct sensor *sensor);

#endif
