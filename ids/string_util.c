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
#include <string.h>

#include "string_util.h"

int
string_strip(
    char **new_str,
    char *str,
    const char *strip_str) {
        int len, last;
        char *find;

	if (new_str == NULL ||
	    str == NULL ||
	    strip_str == NULL) {
		return 1;
	}
        last = strlen(str);
	len = 0;
        while(len < last && str[len] != '\0') {
                find = strchr(strip_str, str[len]);
                if (find) {
                        str[len] = '\0';
                } else {
                        break;
                }
                len++;
        }
        *new_str = &str[len];

	return 0;
}

int
string_rstrip(
    char *str,
    const char *strip_str) {
        int len;
        char *find;

	if (str == NULL || strip_str == NULL) {
		return 1;
	}
        len = strlen(str);
        while(len > 0  && str[len - 1] != '\0') {
                find = strchr(strip_str, str[len - 1]);
                if (find) {
                        str[len - 1] = '\0';
                } else {
                        break;
                }
                len--;
        }

	return 0;
}

int
string_kv_split(
    struct string_kv *kv,
    char *str,
    const char *delim_str) {
	char *key;
	char *value;

	if ((key = strsep(&str, "=")) == NULL) {
		return 1;
	}
	if (string_rstrip(key, " \t")) {
		return 1;
	}
	if (string_strip(&value, str, " \t")) {
		return 1;
	}
	if (string_rstrip(value, " \t")) {
		return 1;
	}
	kv->key = key;
	kv->value = value;

	return 0;
}
