/* Copyright (c) 2003 Canna Project. All rights reserved.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * author and contributors not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.  The author and contributors no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * THE AUTHOR AND CONTRIBUTORS DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTUOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */

#include "server.h"
#include "RKindep/file.h"
#include "RKindep/strops.h"
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>

/* TODO: better error reporting */

#define COMM_DEBUG
#define MAX_LISTENERS 3
#define FIRST_WANT 4

typedef struct {
  sock_type l_fd;
  GetConnectionInfoProc l_info_proc;
  void *l_info_obj;
} ListenerRec;


struct tagClientBuf {
  sock_type fd;
  const ListenerRec *parent;
  ClientPtr client;
  size_t nwant;
  char *sendptr;
  unsigned int nfail;
  RkiStrbuf recvbuf;
  RkiStrbuf sendbuf;
};

typedef struct tagClibufList {
  struct tagClibufList *cbl_next;
  int cbl_finalized;
  ClientBuf cbl_body;
} ClibufList;
#define MEMBER_TO_OBJ(t, x, m) ((t *)((char *)(x) - offsetof(t, m)))
#define CBL_BODY_TO_ENTRY(clibuf) MEMBER_TO_OBJ(ClibufList, clibuf, cbl_body)

EventMgr *global_event_mgr = NULL;

static void ClientBuf_init(ClientBuf *obj,
      const ListenerRec *parent, sock_type fd);
static void ClientBuf_destroy(ClientBuf *obj);
static int ClientBuf_recv(ClientBuf *obj);
static int ClientBuf_send(ClientBuf *obj);
#define ClientBuf_getfd_fast(obj) ((obj)->fd)
#define CLIENT_BUF_IS_SENDING(obj) \
    ((obj)->sendbuf.sb_curr != (obj)->sendbuf.sb_buf)

extern int websock_flg;


#ifdef DEBUG
const char *CallFuncName;
#endif
int (*CallFunc)(ClientPtr *clientp);

static int
process_request(ClientPtr *clientp, ClientBuf *client_buf, BYTE *data, size_t len)
{
  int request;
  int nwant, r;
  ClientPtr client = *clientp;
  const char *username = client ? client->username : NULL;
  const char *hostname = client ? client->hostname : NULL;

#ifdef DEBUG
  CallFuncName = NULL;
  int i = 0;
  ir_debug( Dmsg(5, "data:  ") );
  for (i=0; i<len; i++) {
	  ir_debug( Dmsg(5, "%02x ", data[i]) );
  }
  ir_debug( Dmsg(5, "\n") );
#endif
  if (client && client->version_hi > 1)
    nwant = parse_wide_request(&request, data, len, username, hostname);
  else
    nwant = parse_euc_request(&request, data, len, username, hostname);

  if (nwant)
    return nwant; /* 失敗、またはもっとデータが必要 */

  /* 実際のプロトコルに応じた処理（関数を呼ぶ） */

  if (client) /* initialize等の場合は呼ばない */
      (void)ClientStat(client, SETTIME, request, 0);
  /* プロトコルの種類毎に統計を取る */
  if (client && client->version_hi > 1) {
#ifdef EXTENSION
    if( request < W_MAXREQUESTNO )
#endif
      ++client->pcount[request];
  } else if (client) {
#ifdef EXTENSION
    if( request < MAXREQUESTNO )
#endif
      ++client->pcount[request];
  }

#ifdef DEBUG
  if (CallFuncName)
    Dmsg( 3,"Now Call %s\n", CallFuncName );
#endif
  if (!client)
    r = ir_nosession(clientp, client_buf);
  else
    r = (*CallFunc) (clientp);
  ir_debug(Dmsg(8,"%s returned %d\n", CallFuncName, r));

  /* クライアントの累積サーバ使用時間を設定する */
  if (client && client == *clientp) /* initialize,finalize等のときは呼ばない */
    ClientStat(client, GETTIME, request, 0);

  if (r)
    r = -1; /* どういう失敗でもとりあえず-1を返す */
  return r;
}

static int
set_nonblock(sock)
sock_type sock;
{
  int oldflags;
  oldflags = fcntl(sock, F_GETFL, 0 /* dummy */);
  return fcntl(sock, F_SETFL, oldflags | O_NONBLOCK);
}

static void
ClientBuf_init(obj, parent, fd)
ClientBuf *obj;
const ListenerRec *parent;
sock_type fd;
{
  obj->fd = fd;
  obj->parent = parent;
  obj->client = NULL;
  obj->nwant = FIRST_WANT;
#ifdef COMM_DEBUG
  obj->sendptr = (char *)0xdeadbeef;
#endif
  obj->nfail = 0;
  RkiStrbuf_init(&obj->recvbuf);
  RkiStrbuf_init(&obj->sendbuf);
}

static void
ClientBuf_destroy(obj)
ClientBuf *obj;
{
  close(obj->fd);
  close_session(&obj->client, 0);
  RkiStrbuf_destroy(&obj->recvbuf);
  RkiStrbuf_destroy(&obj->sendbuf);
}

static int ClientBuf_recv(ClientBuf *obj)
{
	ssize_t size;
	int newwant;
	RkiStrbuf *buf = &obj->recvbuf;
	int savederr;

	assert(obj->nwant && !CLIENT_BUF_IS_SENDING(obj));


	if (RKI_STRBUF_RESERVE(buf, buf->sb_curr - buf->sb_buf + obj->nwant)) {
		nomem_msg("ClientBuf_recv()");
		return -1;
	}
	ir_debug(Dmsg(7, "ClientBuf_recv(): receiving %d bytes, nwant=%d\n",
			buf->sb_end - buf->sb_curr, obj->nwant));
	size = recv(obj->fd, buf->sb_curr, buf->sb_end - buf->sb_curr, 0);
	savederr = errno;
	ir_debug(Dmsg(7, "ClientBuf_recv(): recv() returned %d\n", size));
	if (size < 0) {
		if (savederr == EINTR || savederr == EWOULDBLOCK || savederr == EAGAIN) {
			if (++obj->nfail < 5)
				return 0;
			ir_debug(Dmsg(7,
					"ClientBuf_recv(): too many temporary errors. errno=%d\n", savederr));
		}
		goto recvfail;
	} else if (size == 0) {
		goto recvfail;
	}

	obj->nfail = 0;
	buf->sb_curr += size;
	if (size < obj->nwant) {
		obj->nwant -= size;
		return 0;
	}
	obj->nwant = 0;

	newwant = process_request(&obj->client, obj, (BYTE *)buf->sb_buf, buf->sb_curr - buf->sb_buf);
	ir_debug(Dmsg(7, "ClientBuf_recv(): newwant=%d\n", size));

	if (newwant < 0)
		return -1;
	obj->nwant = newwant;
	return 0;

recvfail:
	PrintMsg("[%s] Receive request failed\n", obj->client ? obj->client->username : "unknown");
	ir_debug(Dmsg(5, "ClientBuf_recv(): Receive request failed\n"));
	return -1;
}

static int
ClientBuf_send(obj)
ClientBuf *obj;
{
  ssize_t size;
  RkiStrbuf *buf = &obj->sendbuf;
  int savederr = 0;

  assert(!obj->nwant && CLIENT_BUF_IS_SENDING(obj));
  ir_debug(Dmsg(7, "ClientBuf_send(): sending %d bytes\n",
	buf->sb_curr - obj->sendptr));
  size = send(obj->fd, obj->sendptr, buf->sb_curr - obj->sendptr, 0);
  savederr = errno;
  ir_debug(Dmsg(7, "ClientBuf_send(): send() returned %d\n", size));
  if (size < 0) {
    if (savederr == EINTR || savederr == EWOULDBLOCK || savederr == EAGAIN) {
      if (++obj->nfail < 5)
	return 0;
      ir_debug(Dmsg(7,
	    "ClientBuf_send(): too many temporary errors. errno=%d\n",
	    savederr));
    }
    goto fail;
  }
  assert(size > 0);
  obj->nfail = 0;
  obj->sendptr += size;
  if (obj->sendptr == buf->sb_curr) {
    ir_debug(Dmsg(5, "クライアントへの返信が完了, fd=%d\n", obj->fd));
#ifdef COMM_DEBUG
    obj->sendptr = (char *)0xdeadbeef;
#endif
    obj->nwant = FIRST_WANT;
#if 0 && defined(COMM_DEBUG)
    RkiStrbuf_clear(&obj->recvbuf);
    RkiStrbuf_clear(buf);
#else
    obj->recvbuf.sb_curr = obj->recvbuf.sb_buf;
    buf->sb_curr = buf->sb_buf;
#endif
  }
  return 0;

fail:
  PrintMsg("[%s] Send request failed\n",
      obj->client ? obj->client->username : "unknown");
  Dmsg(1, "Send Error[ %d ]\n", savederr);
  return -1;
}

int
ClientBuf_store_reply(obj, data, len)
ClientBuf *obj;
const BYTE *data;
size_t len;
{
  ir_debug(Dmsg(7, "ClientBuf_store_reply() start\n"));
  assert(!obj->nwant && !CLIENT_BUF_IS_SENDING(obj));
  if (RkiStrbuf_addmem(&obj->sendbuf, data, len)) {
    nomem_msg("ClientBuf_store_reply()");
    return -1;
  }
  obj->sendptr = obj->sendbuf.sb_buf;
  return 0;
}

int
ClientBuf_get_connection_info(obj, addr, hostname)
ClientBuf *obj;
Address *addr;
char **hostname;
{
  const ListenerRec *parent = obj->parent;
  return (*parent->l_info_proc) (parent->l_info_obj, obj->fd, addr, hostname);
}

sock_type
ClientBuf_getfd(obj)
ClientBuf *obj;
{
  return obj->fd;
}

ClientPtr
ClientBuf_getclient(obj)
ClientBuf *obj;
{
  return obj->client;
}

struct tagEventMgr {
  ListenerRec listeners[MAX_LISTENERS];
  size_t nlisteners;
  ClibufList *cbl;
  size_t nclibufs;
  int quitflag;
  int exit_status;
};

EventMgr *
EventMgr_new()
{
  EventMgr *obj = malloc(sizeof(EventMgr));
  if (!obj)
    return NULL;
  obj->nlisteners = 0;
  obj->cbl = NULL;
  obj->nclibufs = 0;
  obj->quitflag = 0;
  obj->exit_status = 220; /* これは絶対に返らない */
  return obj;
}

void
EventMgr_delete(obj)
EventMgr *obj;
{
  ClibufList *curr;
#ifdef COMM_DEBUG
  size_t nclibufs = 0;
#endif
  if (!obj)
    return;
  curr = obj->cbl;
  while (curr) {
    ClibufList *next = curr->cbl_next;
#ifdef COMM_DEBUG
    ++nclibufs;
#endif
    ClientBuf_destroy(&curr->cbl_body);
    free(curr);
    curr = next;
  }
#ifdef COMM_DEBUG
  assert(nclibufs == obj->nclibufs);
#endif
  free(obj);
}

int
EventMgr_add_listener_sock(obj, listenerfd, info_proc, info_obj)
EventMgr *obj;
sock_type listenerfd;
GetConnectionInfoProc info_proc;
void *info_obj;
{
  ListenerRec *entry = obj->listeners + obj->nlisteners;

  assert(obj->nlisteners < MAX_LISTENERS);
  assert(listenerfd != INVALID_SOCK);
  if (listenerfd >= RKI_FD_SETSIZE) {
    PrintMsg("EventMgr_add_listener_sock(): out of rki_fd_set: fd=%d\n",
	listenerfd);
    return -1;
  }
  entry->l_fd = listenerfd;
  entry->l_info_proc = info_proc;
  entry->l_info_obj = info_obj;
  ++obj->nlisteners;
  return 0;
}

void
EventMgr_quit_later(obj, status)
EventMgr *obj;
int status;
{
  obj->quitflag = 1;
  obj->exit_status = status;
}

void
EventMgr_finalize_notify(obj, clibuf)
EventMgr *obj;
const ClientBuf *clibuf;
{
  ClibufList *entry = CBL_BODY_TO_ENTRY(clibuf);
  assert(clibuf);
  assert(!entry->cbl_finalized);
  entry->cbl_finalized = 1;
}

static int
EventMgr_accept(obj, listener_entry)
EventMgr *obj;
ListenerRec *listener_entry;
{
  ClibufList *cbl_ent = NULL;
  sock_type connfd = INVALID_SOCK;

  ir_debug(Dmsg(7, "EventMgr_accept() start\n"));
  connfd = accept(listener_entry->l_fd, NULL, 0);
  if (connfd == INVALID_SOCK) {
    /* rarely happens; probably ECONNABORTED or EINTR */
    PrintMsg("EventMgr_accept(): accept: errno=%d\n", errno);
    goto fail;
  } else if (connfd >= RKI_FD_SETSIZE) {
    PrintMsg("EventMgr_accept(): out of rki_fd_set: fd=%d\n", connfd);
    goto fail;
  }
  if (set_nonblock(connfd)) {
    PrintMsg("EventMgr_accept(): set_nonblock(): errno=%d\n", errno);
    goto fail;
  }
  if (!(cbl_ent = malloc(sizeof(ClibufList))))
    goto nomem;
  ClientBuf_init(&cbl_ent->cbl_body, listener_entry, connfd);
  cbl_ent->cbl_finalized = 0;
  cbl_ent->cbl_next = obj->cbl;
  obj->cbl = cbl_ent;
  ++obj->nclibufs;
  ir_debug(Dmsg(5, "クライアントとの接続に成功, fd=%d\n", connfd));
  return 0;

nomem:
  nomem_msg("EventMgr_accept()");
fail:
  if (cbl_ent)
    ClientBuf_destroy(&cbl_ent->cbl_body);
  free(cbl_ent);
  if (connfd != INVALID_SOCK)
    close(connfd);
  return -1;
}

static void EventMgr_check_fds(EventMgr *obj, rki_fd_set *rfds, rki_fd_set *wfds)
{
	int listenerno;
	ClibufList **cbl_link;

	ir_debug(Dmsg(7, "EventMgr_check_fds() start\n"));
	for (listenerno = 0; listenerno < obj->nlisteners; ++listenerno) {
		if (RKI_FD_ISSET(obj->listeners[listenerno].l_fd, rfds))
			EventMgr_accept(obj, obj->listeners + listenerno);
	}

	for (cbl_link = &obj->cbl; *cbl_link; cbl_link = &(*cbl_link)->cbl_next) {
		ClibufList *cbl_ent = *cbl_link;
		ClientBuf *client_buf = &cbl_ent->cbl_body;
		int fd = ClientBuf_getfd_fast(client_buf);
		int error = 0;

		if (RKI_FD_ISSET(fd, rfds)) {
			if (websock_flg) {
				/* Izumo WebSockの場合はここで分岐 */
				/* 現在はフラグを分岐条件としているがいずれは変更したい */
				error = IzumoWebSockRcvSnd(fd);
			}
			else {
				/* Cannaプロトコルの場合 */
				error = ClientBuf_recv(client_buf);
			}
		}
		else if (RKI_FD_ISSET(fd, wfds)) {
			error = ClientBuf_send(client_buf);
		}
		else {
			/* nothing to do */
		}

		if (error || (cbl_ent->cbl_finalized && !CLIENT_BUF_IS_SENDING(client_buf))) {
			ir_debug(Dmsg(5, "クライアントとの接続を切る, fd=%d\n", ClientBuf_getfd_fast(client_buf)));
			*cbl_link = cbl_ent->cbl_next;
			--obj->nclibufs;
			ClientBuf_destroy(client_buf);
			free(cbl_ent);
			if (!*cbl_link)
				break;
		}
	}
}

int
EventMgr_run(obj)
EventMgr *obj;
{
  struct timeval timeout;
  int sync_flag = 0;

  timeout.tv_sec = 108; /* why? */
  timeout.tv_usec = 0;

  for (;;) {
    rki_fd_set rfds, wfds;
    int nfds = 0;
    int listenerno;
    ClibufList *cbl_ent;
    struct timeval timeout_tmp;
    int r;
    int needwrite = 0;
    int savederr;

    RKI_FD_ZERO(&rfds);
    RKI_FD_ZERO(&wfds);
    for (listenerno = 0; listenerno < obj->nlisteners; ++listenerno) {
      int fd = obj->listeners[listenerno].l_fd;
      RKI_FD_SET(fd, &rfds);
      nfds = RKI_MAX(nfds, fd + 1);
    }
    for (cbl_ent = obj->cbl; cbl_ent; cbl_ent = cbl_ent->cbl_next) {
      int fd = ClientBuf_getfd_fast(&cbl_ent->cbl_body);
      if (CLIENT_BUF_IS_SENDING(&cbl_ent->cbl_body)) {
	RKI_FD_SET(fd, &wfds);
	needwrite = 1;
      } else {
	if (!obj->quitflag)
	  RKI_FD_SET(fd, &rfds);
      }
      nfds = RKI_MAX(nfds, fd + 1);
    }
    
    if (obj->quitflag && !needwrite)
      break;
    ir_debug(Dmsg(5, "\nselect()で待ちを開始\n"));
    timeout_tmp = timeout;
    r = select(nfds, &rfds, &wfds, NULL, &timeout_tmp);
    savederr = errno;
    ir_debug(Dmsg(5, "select() returned %d\n", r));
    if (r < 0) {
      if (savederr != EINTR) {
	/* What happened? */
	PrintMsg("EventMgr_run(): select: errno=%d\n", savederr);
	obj->exit_status = 3;
	break;
      }
    } else if (r == 0) {
      /* select の制限時間を越えたので sync 処理を行う */
      if (sync_flag == 0) {/* sync_flag が 0 の時は Allsync を行う */
	ir_debug(Dmsg(5, "EventMgr_run(): select: all sync start\n"));
	AllSync();
	sync_flag = 1; /* 一回行なったので フラグを立てる */
      }
    } else {
      sync_flag = 0; /* データが来たのでフラグを下げる */
      EventMgr_check_fds(obj, &rfds, &wfds);
    }
    if (CheckSignal()) {
      obj->exit_status = 1;
      break;
    }
  }
  return obj->exit_status;
}

void
EventMgr_clibuf_first(obj, it)
EventMgr *obj;
EventMgrIterator *it;
{
  ClibufList *entry = obj->cbl;
  it->entry = entry;
  if (entry)
    it->it_val = &entry->cbl_body;
  else
    it->it_val = NULL;
}

void
EventMgr_clibuf_end(obj, it)
EventMgr *obj;
EventMgrIterator *it;
{
  it->it_val = NULL;
}

void
EventMgrIterator_next(obj)
EventMgrIterator *obj;
{
  ClibufList *entry = (ClibufList *)obj->entry;
  ClibufList *next = entry->cbl_next;
  obj->entry = next;
  if (next)
    obj->it_val = &next->cbl_body;
  else
    obj->it_val = NULL;
}

#ifdef USE_UNIX_SOCKET
static int
get_addr_unix(dummy, connfd, addr, hostname)
/* ARGSUSED */
void *dummy;
sock_type connfd;
Address *addr;
char **hostname;
{
  char buf[MAXDATA];

  if(gethostname(buf, MAXDATA - 7) < 0) {
    PrintMsg("gethostname failed\n");
    return -1;
  }
  buf[MAXDATA - 7] = '\0';
  strcat(buf, "(UNIX)") ;
  if ((*hostname = strdup(buf)) == NULL)
    return -1;
  addr->family = AF_UNIX;
  addr->len = 0;
  return 0;
}

static int
get_addr_inet(dummy, connfd, addr, hostname)
/* ARGSUSED */
void *dummy;
sock_type connfd;
Address *addr;
char **hostname;
{
#ifdef INET6
  struct sockaddr_storage from;
#else /* !INET6 */
  struct sockaddr_in	from;
  struct hostent	*hp;
#endif /* !INET6 */
  char		buf[MAXDATA];
  canna_socklen_t 	fromlen = sizeof( from ) ;
  struct sockaddr	*fromp = (struct sockaddr *)&from;

  bzero( &from, fromlen ) ;
  if (getpeername(connfd, (struct sockaddr *)&from, &fromlen) < 0) {
    PrintMsg( "getpeername error No.%d\n", errno );
    return -1;
  }

#ifdef INET6
  if (fromp->sa_family == AF_INET || fromp->sa_family == AF_INET6) {
    int res = getnameinfo(fromp, fromlen, buf, MAXDATA, NULL, 0, 0);
    if (res) {
      /* cannot store even a numeric hostname */
      PrintMsg( "getaddrinfo error No.%d\n", res );
      return -1;
    }
  }
#else /* !INET6 */
  if( from.sin_family == AF_INET ) {
    hp = gethostbyaddr((char *)&from.sin_addr, sizeof( struct in_addr ),
	from.sin_family);
    if ( hp )
      strncpy( buf, hp->h_name, MAXDATA-1 ) ;
    else
      strncpy( buf, inet_ntoa( from.sin_addr ), MAXDATA-1 ) ;
  }
#endif /* !INET6 */
  else {
    PrintMsg( "unknown protocol family: %d\n", fromp->sa_family );
    return -1;
  }

  if ((*hostname = strdup(buf)) == NULL)
    return -1;
  addr->saddr = from;
  addr->family = fromp->sa_family;
  addr->len = fromlen;
  return 0;
}
#endif

void
SockHolder_delete(obj)
SockHolder *obj;
{
  if (!obj)
    return;
#ifdef USE_UNIX_SOCKET
  if (obj->unsock != INVALID_SOCK) {
    close(obj->unsock);
    unlink(obj->unaddr.sun_path);
  }
#endif
#ifdef USE_INET_SOCKET
  if (obj->insock != INVALID_SOCK)
    close(obj->insock);
# ifdef INET6
  if (obj->in6sock != INVALID_SOCK)
    close(obj->insock);
# endif
#endif
  free(obj);
}

int
SockHolder_tie(obj, event_mgr)
SockHolder *obj;
EventMgr *event_mgr;
{
#ifdef USE_UNIX_SOCKET
  assert(obj->unsock != INVALID_SOCK);
  if (EventMgr_add_listener_sock(event_mgr,
	obj->unsock, &get_addr_unix, NULL))
    return -1;
#endif
#ifdef USE_INET_SOCKET
  if (UseInet) {
    if (EventMgr_add_listener_sock(event_mgr,
	  obj->insock, &get_addr_inet, NULL))
      return -1;
  }
# ifdef INET6
  if (UseInet6) {
    if (EventMgr_add_listener_sock(event_mgr,
	  obj->in6sock, &get_addr_inet, NULL))
      return -1;
  }
# endif
#endif
  return 0;
}

/* vim: set sw=2: */
