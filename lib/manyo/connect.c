/* Copyright (c) 2014-2015 Izumo Project. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Izumo -connect.c-
 *   Author: Hashimoto Masahiko
 *
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

static int fd;

int mny_con_create_socket(char* path)
{
	int ret = -1, len;
	struct sockaddr_un unix_addr;

	/* create socket */
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return(fd);

	/* connect server */
	unix_addr.sun_family = AF_UNIX;
	strcpy(unix_addr.sun_path, path);
	len = sizeof(unix_addr.sun_family) + strlen(unix_addr.sun_path);
	ret = connect(fd, (struct sockaddr*)&unix_addr, len);
	if (ret < 0)
		return(ret);

	return(0);
}

int mny_con_sndrcv(char *buf, int len)
{
	int ret = -1;

	/* send */
	ret = write(fd, buf, len);
	if (ret < 0)
	    return(ret);

	/* receive */
	ret = read(fd, buf, 1024);

	return(ret);
}


int mny_con_close()
{
	return close(fd);
}



