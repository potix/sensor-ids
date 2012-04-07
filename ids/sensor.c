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
#include <usb.h>
#include <sys/types.h>
#include <unistd.h>
#include <event.h>

#include "macro.h"
#include "alert.h"
#include "sensor.h"

/* USBデバイスの初期化処理 */
static struct usb_bus *
usb_initialize(void)
{
	usb_init();
	usb_find_busses();
	usb_find_devices();
	return usb_get_busses();
}

/* USBデバイスを探す */
static struct usb_device *
usb_search(
    struct usb_bus *busses,
    struct usb_device *dev)
{
	struct usb_bus *bus;

	for (bus=busses; bus; bus=bus->next) {
		for (dev=bus->devices; dev; dev=dev->next) {
			if ((dev->descriptor.idVendor==USB_VENDOR) &&
			    (dev->descriptor.idProduct==USB_PRODUCT)) {
				return dev;
			}
		}
	}

	return NULL;
}

/* UBSデバイスの設定 */
static int
usb_configure(
    struct usb_device *dev,
    struct usb_dev_handle *dh)
{
	if (usb_set_configuration(dh, dev->config->bConfigurationValue) < 0) {
		if (usb_detach_kernel_driver_np(dh, dev->config->interface->altsetting->bInterfaceNumber) < 0) {
			fprintf(stderr, "failed to device configuration.\n");
			fprintf(stderr, "failed to device detach. (%s)\n", usb_strerror());
 		}
	}

	if (usb_claim_interface(dh, dev->config->interface->altsetting->bInterfaceNumber) < 0) {
		if (usb_detach_kernel_driver_np(dh, dev->config->interface->altsetting->bInterfaceNumber) < 0) {
			fprintf(stderr,"failed to claim interface.\n");
			fprintf(stderr, "failed to device detach. (%s)\n", usb_strerror());
		}
	}

	if (usb_claim_interface(dh, dev->config->interface->altsetting->bInterfaceNumber) < 0) {
		fprintf(stderr, "failed to device detach. (%s)\n", usb_strerror());
	}

	return 0;

}

/* USBデバイスのintteruptポーリング */
static void
sensor_polling(int fd, short event, void *args) {
	struct sensor *sensor = args;
	struct timeval timer;
	unsigned char wdata[0];
	unsigned char rdata[8];
	struct usb_endpoint_descriptor *endpoint;
	
        if (event != EV_TIMEOUT) {
		ABORT();
		/* NOT REACHED */
        }

	/* エンドポイントは１つだけと仮定 */
	endpoint = &sensor->dev->config->interface->altsetting->endpoint[0];

	/* intterrupt bulk通信 */
rewrite:
	if (usb_interrupt_write(
	    sensor->dh,
	    endpoint->bEndpointAddress,
	    (char *)wdata,
	    sizeof(wdata),
	    DEVICE_TIMEOUT) < 0) {
		if (errno == EAGAIN) {
			goto rewrite;
		}
		fprintf(stderr, "failed in intterupt write. (%d: %s)\n", errno, usb_strerror());
		goto next;
	}

reread:
	if (usb_interrupt_read(
	    sensor->dh,
	    endpoint->bEndpointAddress,
	    (char *)rdata,
	    sizeof(rdata),
	    DEVICE_TIMEOUT) < 0) {
		if (errno == EAGAIN) {
			goto reread;
		}
		fprintf(stderr, "failed in intterupt read. (%d: %s)\n", errno, usb_strerror());
		goto next;
	}

        /* 取得した情報をチェック */
	if (rdata[4] == 0xff) {
		/* 人がいる */
		sensor->detect_count++;
		if (sensor->detect_count > sensor->alert_threshold) {
			printf("alert!!\n");
			if (sensor->execute_alert) {
				alert_start_first(sensor->alert);
			}
			sensor->detect_count = 0;
		}
	} else {
		/* 人がいない */
		sensor->detect_count = 0;
	}
	/* 次のポーリングイベントを登録 */
next:
	timer.tv_sec = 0;
	timer.tv_usec = sensor->poll_interval;
        evtimer_set(&sensor->poll_event, sensor_polling, sensor);
        event_base_set(sensor->event_base, &sensor->poll_event);
        evtimer_add(&sensor->poll_event, &timer);
}

static void
sensor_close(struct sensor *sensor) {
	if (sensor->dh) {
		/* device release */
		printf("device release.\n");
		if (usb_release_interface(sensor->dh, 0)) {
			fprintf(stderr, "failed in release device. (%s)\n", usb_strerror());
		}
		/* device close */
		printf("device close.\n");
		if (usb_close(sensor->dh) < 0) {
			fprintf(stderr, "failed in close device.(%s)\n", usb_strerror());
		}
	}
}

int 
sensor_create(
    struct sensor **sensor,
    struct alert *alert,
    int poll_interval,
    int alert_threshold,
    struct event_base *event_base)
{
	struct sensor *inst;

	*sensor = NULL;
	inst = malloc(sizeof(struct sensor));
	if (inst == NULL) {
		return 1;
	}
	memset(inst, 0, sizeof(struct sensor));
	inst->alert = alert;
	inst->execute_alert = 1;
	inst->event_base = event_base;
	inst->poll_interval = poll_interval;
	inst->alert_threshold = alert_threshold;
	*sensor = inst;

	return 0;
}

int 
sensor_start(struct sensor *sensor)
{
	int error = 0;
	unsigned char wdata[0];
	struct timeval timer;

	if (sensor == NULL) {
		fprintf(stderr, "invalid argument\n");
		return  1;
	}
	/* USBデバイスの初期化処理 */
	printf("device Initializing\n");
	sensor->bus = usb_initialize();
	sensor->dev = usb_search(sensor->bus, sensor->dev);
	if (sensor->dev == NULL) {
		fprintf(stderr, "Device not found\n");
		error = 1;
		goto finish;
	}
        /* USBデバイスを開く */
	printf("device opening\n");
	sensor->dh = usb_open(sensor->dev);
	if (sensor->dh == NULL) {
		fprintf(stderr, "failed in open device. (%s)\n", usb_strerror());
		error = 1;
		goto finish;
	}
        /* USBデバイスの設定処理 */
        if (usb_configure(sensor->dev, sensor->dh)) {
		fprintf(stderr, "failed in configuration device.\n");
		error = 1;
		goto finish;
	}

	/* エンドポイントのチェック */
	if (sensor->dev->config->interface->altsetting->bNumEndpoints <= 0) {
		fprintf(stderr, "not found endpoints.\n");
		error = 1;
		goto finish;
	}

	/* コントロールメッセージの送信 */
	if (usb_control_msg(
	    sensor->dh,
	    0x42,
	    0x10,
	    0x18,
	    0x00,
	    (char *)wdata,
	    sizeof(wdata),
	    DEVICE_TIMEOUT) < 0) {
		fprintf(stderr, "failed in control message.\n");
		error = 1;
		goto finish;
	}

	/*
         * センサーのポーリングイベントの登録
         * 初回は、イベントループに入るまでに、
         * RPC開始などの処理があるため2秒ほど待つ
         * 2秒は適当。
         */ 
	timer.tv_sec = 2;
	timer.tv_usec = 0;
        evtimer_set(&sensor->poll_event, sensor_polling, sensor);
        event_base_set(sensor->event_base, &sensor->poll_event);
        evtimer_add(&sensor->poll_event, &timer);

	return 0;

finish:
	sensor_close(sensor);

	return error;
}

void 
sensor_finish(struct sensor *sensor) {
        /* ポーリングイベントを削除 */
	evtimer_del(&sensor->poll_event);
	sensor_close(sensor);
}

void 
sensor_destroy(struct sensor *sensor) {
	free(sensor);
}

void
sensor_monitor_start(struct sensor *sensor) {
	sensor->execute_alert = 1;
}

void
sensor_monitor_stop(struct sensor *sensor) {
	sensor->execute_alert = 0;
}

int
sensor_get_monitor_status(struct sensor *sensor) {
	return sensor->execute_alert;
}
