/*
 * manyo.c
 *
 *  Created on: 2015/04/01
 *      Author: hashimom
 */
#include <stdio.h>
#include "manyo.h"
#include "connect.h"

int mny_open(char* pass, int portno, char *user)
{
	int ret = -1;

	mny_con_create_socket(pass);

	return(ret);
}

int mny_convert(int id, char *yomi, char *conv)
{
	return(-1);
}

int mny_close(int id)
{
	return mny_con_close();
}

