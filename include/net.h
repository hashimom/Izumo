/* Copyright 1992 NEC Corporation, Tokyo, Japan.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of NEC
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  NEC Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * NEC CORPORATION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN 
 * NO EVENT SHALL NEC CORPORATION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF 
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR 
 * OTHER TORTUOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR 
 * PERFORMANCE OF THIS SOFTWARE. 
 */

/* sccs_id[]="@(#) NEC UNIX( PC-UX/EWS-UX ) net.h 1.1 90/11/01 16:25:42"; */
/* $Id: net.h,v 1.3.2.2 2003/12/27 17:15:20 aida_s Exp $ */

#ifndef NET_H
#define NET_H

#include "ccompat.h"

#if defined(nec_ews_svr2) || defined(pcux)
#include <sys/types.h>
#include <net/socket.h>
#include <net/in.h>
#include <net/netdb.h>
#include <net/un.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef UNIXCONN
#include <sys/un.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#endif

#ifdef HAVE_IN_ADDR_T
typedef in_addr_t canna_in_addr_t;
#else
typedef canna_uint32_t canna_in_addr_t;
#endif
#ifdef HAVE_SOCKLEN_T
typedef socklen_t canna_socklen_t;
#else
typedef int canna_socklen_t;
#endif

#endif /* NET_H */
