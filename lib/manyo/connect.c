/*
 * connect.c
 *
 *  Created on: 2015/04/01
 *      Author: hashimom
 */
#include <stdio.h>
//#include <string.h>
//#include <stdlib.h>
//#include <iconv.h>
//#include <unistd.h>
//#include <locale.h>
//#include <sys/types.h>
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
	ret = read(fd, buf, 256);

	return(ret);
}


int mny_con_close()
{
	return close(fd);
}



