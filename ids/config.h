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

#ifndef CONFIG_H
#define CONFIG_H

struct config {
	char *first_alert_script;
	char *second_alert_script;
	int cancel_wait_time;
	int poll_interval;
	int alert_threshold;
	char *rpc_port;
	int rpc_timeout;
	char *pid_file_path;
};

/* configの生成 */
int config_create(
    struct config **config,
    const char *first_alert_script,
    const char *second_alert_script,
    int cancel_wait_time,
    int poll_interval,
    int alert_threshold,
    const char *rpc_port,
    int rpc_timeout,
    const char *pid_file_path);
/* コンフィグファイルの読み込み */
int config_load(
    struct config *config,
    const char *config_path);
/* コンフィグ情報を出力する */
void config_print(
    struct config *config);
/* configの削除 */
void config_destroy(
    struct config *config);

#endif
