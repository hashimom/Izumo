/* Copyright (c) 2015 Masahiko Hashimoto <hashimom@geeko.jp>
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
*/

#include "server.h"
#include <signal.h>
#include <fcntl.h>

static int websock_flg = 0;

enum {
	SOCK_OTHER_ERROR = -2,
	SOCK_BIND_ERROR,
	SOCK_OK, /* This is '0'. (dummy) */
};

static int open_unix_socket(struct sockaddr_un *unaddr)
{
	int oldUmask, oldflags;;
	int request = -1;
	int status = SOCK_OTHER_ERROR;
	const int sockpathmax = sizeof(unaddr->sun_path) - 3;

	unaddr->sun_family = AF_UNIX;
	oldUmask = umask (0);

	if ( mkdir( IR_UNIX_DIR, 0777 ) == -1 && errno != EEXIST ) {
		ir_debug( Dmsg(5, "Can't open %s error No. %d\n",IR_UNIX_DIR, errno));
	}

	/* make file descriptor path name */
	strncpy(unaddr->sun_path, IR_UNIX_PATH, sockpathmax);
	if (strlen(unaddr->sun_path) >= sockpathmax) {
		ir_debug( Dmsg(5, "Path to socket is too long\n"));
		goto last;
	}

	request = socket(AF_UNIX, SOCK_STREAM, 0);
	if (request < 0) {
		ir_debug( Dmsg(5, "Warning: UNIX socket for server failed.\n"));
	}
	else {
		if (bind(request, (struct sockaddr *)unaddr, sizeof(struct sockaddr_un)) < 0) {
			status = SOCK_BIND_ERROR;
			ir_debug( Dmsg(5,"Warning: Server could not bind.\n"));
			close(request);
			goto last;
		}
		ir_debug( Dmsg(5, "ファイル名:[%s]\n",unaddr->sun_path));

		if (listen(request, 5) < 0) {
			ir_debug( Dmsg(5,"Warning: Server could not listen.\n"));
			close(request);
			unlink(unaddr->sun_path);
			goto last;
		}

		oldflags = fcntl(request, F_GETFL, 0);
		if (fcntl(request, F_SETFL, oldflags | O_NONBLOCK) < 0) {
			ir_debug( Dmsg(5,"Warning: Server could not set nonblocking mode.\n"));
			close(request);
			unlink(unaddr->sun_path);
			goto last;
		}

		status = request;
	}

last:
	(void)umask( oldUmask );
	return(status);
}

static int open_inet_socket()
{
	struct sockaddr_in insock;
	struct servent *sp;
	int retry = 0, request, oldflags;
	int status = SOCK_OTHER_ERROR;

	request = socket(AF_INET, SOCK_STREAM, 0);
	if (request  < 0) {
		ir_debug( Dmsg(5,"Warning: INET socket for server failed.\n"));
	}
	else {

#ifdef SO_REUSEADDR
		int one = 1;
		setsockopt(request, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(int));
#endif
		bzero ((char *)&insock, sizeof (insock));
		insock.sin_family = AF_INET;
		insock.sin_addr.s_addr = htonl(INADDR_ANY);

		/* ポート番号を取得 */
		sp = getservbyname(IR_SERVICE_NAME ,"tcp");
		if (sp != NULL) {
			insock.sin_port = sp->s_port;
		}
		else {
			insock.sin_port = IR_DEFAULT_PORT;
		}
		ir_debug( Dmsg(5, "INET PORT NO:[%d]\n",insock.sin_port));

		if (bind(request, (struct sockaddr *)&insock, sizeof(insock)) < 0) {
			ir_debug( Dmsg(5,"Warning: Server could not bind.\n"));
			status = SOCK_BIND_ERROR;
			close(request);
			goto last;
		}

		if (listen (request, 5) < 0) {
			ir_debug( Dmsg(5,"Warning: Server could not listen.\n"));
			close(request);
			goto last;
		}

		oldflags = fcntl(request, F_GETFL, 0);
		if (fcntl(request, F_SETFL, oldflags | O_NONBLOCK) < 0) {
			ir_debug( Dmsg(5,"Warning: Server could not set nonblocking mode.\n"));
			close(request);
			goto last;
		}

		status = request;
	}

last:
	return(status);
}


int main(int argc, char *argv[])
{
	int optno, parentid;
	SockHolder sock_holder;
	int status;

	sock_holder.unsock = INVALID_SOCK;
	bzero(&(sock_holder.unaddr), sizeof(sock_holder.unaddr));
	sock_holder.insock = INVALID_SOCK;

	while ((optno = getopt(argc, argv, "w::") != -1)) {
		switch(optno) {
		case 'w':
			/* Websocket option */
			websock_flg = 1;
			break;

		/* 他のオプションは EarlyInit で受け取るが徐々に getopt へ移行する */
		}
	}

	EarlyInit(argc, argv);
	global_user_table = UserTable_new();
	global_event_mgr = EventMgr_new();
	if ((global_user_table == NULL) || (global_event_mgr == NULL)) {
		fprintf(stderr, "UserTable/EventMgr new failed; probably due to lack of memor\n");
		goto fail;
	}

	/* WebSocket を使用する場合は INET を有効にする */
	if (websock_flg) {
		UseInet = 1;
		sock_holder.insock = open_inet_socket();
		if (sock_holder.insock < 0) {
			goto fail;
		}
		ir_debug( Dmsg(3,"INETドメインはできた\n") );
	}

	/* UNIXドメインソケットは必ずオープン */
	sock_holder.unsock = open_unix_socket(&(sock_holder.unaddr));
	if (sock_holder.unsock < 0) {
		goto fail;
	}
	ir_debug( Dmsg(3,"UNIXドメインはできた\n") );

	if (SockHolder_tie(&sock_holder, global_event_mgr) != 0) {
		fprintf(stderr, "SockHolder_tie() failed; probably due to lack of memor\n");
		goto fail;
	}

	parentid = BecomeDaemon();
  
	DetachTTY();

	status = EventMgr_run(global_event_mgr);
	goto last;

fail:
	status = 2;
last:
	/* UNIX domain socket close */
	if (sock_holder.unsock != INVALID_SOCK) {
		close(sock_holder.unsock);
		unlink(sock_holder.unaddr.sun_path);
	}
	/* INET close */
	if (sock_holder.insock != INVALID_SOCK) {
		close(sock_holder.insock);
	}
	EventMgr_delete(global_event_mgr);
	UserTable_delete(global_user_table);
	CloseServer();
	return(status);
}


