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
#ifndef RPC_H
#define RPC_H

#define DEFAULT_RPC_TIMEOUT  60
#define DEFAULT_RPC_PORT     "18000"

struct rpc {
	struct event_base *event_base;
	struct tcp_server *tcpserver;  /* tcpサーバーのインスタンス */
	char *bind_port;               /* バインドするポート */
        int rpc_timeout;               /* RPCのタイムアウト */
	struct alert *alert;            /* alertのインスタンス */
	struct sensor *sensor;          /* sensorのインスタンス */
};

/* rpcのインスタンス生成 */
int rpc_create(
    struct rpc **rpc,
    const char *bind_port,
    int rpc_timeout,
    struct alert *alert,
    struct sensor *sensor,
    struct event_base *event_base);
/* rpcの開始 */
int rpc_start(
    struct rpc *rpc);
/* rpcの終了 */
void rpc_finish(
    struct rpc *rpc);
/* rpcのインスタンス削除 */
void rpc_destroy(
    struct rpc *rpc);

#endif
