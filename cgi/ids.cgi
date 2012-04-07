#!/usr/bin/python
# -*- coding: utf-8 -*-
# Copyright (c) 2010 Hiroyuki Kakine
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import os
import sys
import re
import cgi
import socket

#
# need json
#
import json

class Rpc:
    def __init__(self):
        # idsデーモンの接続先にあわせる
        self.address = "127.0.0.1"
        self.port = 18000
    def send(self, command):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((self.address, self.port))
        sock.send(command + "\r\n")
        res = sock.recv(1024)
        sock.close()
        return res

class Json:
    def __init__(self):
        pass
    def data_to_json(self, data):
        return json.dumps(data)
    def json_to_data(self, json_str):
        return json.loads(json_str)

class Output:
    def __init__(self):
        self.data = {}
        self.js = Json()
    def content_type(self):
        print "Content-Type: text/html\n\n";
    def message(self, msg):
        self.data["Result"] = msg 
        self.content_type()
        print self.js.data_to_json(self.data)

class Request:
    def __init__(self):
        self.fs = cgi.FieldStorage()
        self.js = Json()
    def get_command(self):
        return self.fs.getfirst("Command", "")

rpc = Rpc()
output = Output()
req = Request()
cmd = req.get_command()

if cmd == "STOP_MONITOR" or \
   cmd == "START_MONITOR" or \
   cmd == "CANCEL_ALERT" or \
   cmd == "CLEAR_ALERT_STATUS":
    res = rpc.send(cmd)
    if res == "OK\r\n":
        output.message("OK")
    else:
        output.message("RPC ERROR")
elif cmd == "GET_ALERT_STATUS":
    res = rpc.send(cmd)
    if res == "GOOD\r\n":
        output.message("GOOD")
    elif res == "FIRST ALERT\r\n":
        output.message("FIRST ALERT")
    elif res == "SECOND ALERT\r\n":
        output.message("SECOND ALERT")
    else:
        output.message("RPC ERROR")
elif cmd == "GET_MONITOR_STATUS":
    res = rpc.send(cmd)
    if res == "RUNNING\r\n":
        output.message("RUNNING")
    elif res == "STOPPING\r\n":
        output.message("STOPPING")
    else:
        output.message("RPC ERROR")
else:
    output.message("UNKOWN COMMAND")
