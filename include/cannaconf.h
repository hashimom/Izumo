#ifndef CANNACONF_H
#define CANNACONF_H
#include "config.h"
#ifdef malloc
# undef malloc
#endif
#define USE_UNIX_SOCKET
#define USE_INET_SOCKET

/* UNIXCONN は仮定義 (from imake) */
#define UNIXCONN

/* others */
#define CANNA_LIGHT 1
#ifdef nec
#undef nec
#endif
/* #undef INET6 */
#endif /* !CANNACONF_H */
