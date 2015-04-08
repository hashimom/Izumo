/*
 * manyo.c
 *
 *  Created on: 2015/04/01
 *      Author: hashimom
 */
#include <stdio.h>
#include <string.h>
#include "manyo.h"
#include "connect.h"

#define MNY_MAX_BUF (256)

static unsigned short getS16(char *target_p)
{
	return((unsigned short)((target_p[0]<<8) | (target_p[1])));
}

static int getI32(char *target_p)
{
	return((int)((target_p[0]<<8) | (target_p[1]<<8) | (target_p[2]<<8) | (target_p[3])));
}

static void setI32(void *target_p, int val)
{
	register char *tmp_p = (char*)target_p;
	*(tmp_p)   = (char)((val>>24)&255);
	*(tmp_p+1) = (char)((val>>16)&255);
	*(tmp_p+2) = (char)((val>> 8)&255);
	*(tmp_p+3) = (char)((val)&255);
}

static void setS16(void *target_p, int val)
{
	register char *tmp_p = (char*)target_p;
	*(tmp_p)   = (char)((val>> 8)&255);
	*(tmp_p+1) = (char)((val)&255);
}

#if 0 /* 未使用 */
static void Val2L3(void *target_p, int val)
{
	register char *tmp_p = (char*)target_p;
	*(tmp_p)   = (char)((val>>16)&255);
	*(tmp_p+1) = (char)((val>> 8)&255);
	*(tmp_p+2) = (char)((val)&255);
}

#endif

/* mny_mountdic は機能をサーバーへ移行後に廃止予定のため公開しません */
static int mny_mountdic(int cxno, char *dicname)
{
	char buf[MNY_MAX_BUF];
	int ret, dicnamelen;

	memset(buf, 0, MNY_MAX_BUF);
	dicnamelen = strlen(dicname);

	/* mount dic */
	*(buf)   = 0x08;
	*(buf+1) = 0x00;
	setS16(buf+2, (8+dicnamelen));
	setI32(buf+4, 0);
	setS16(buf+8, cxno);
	strcpy(buf+10, dicname);
	setI32(buf+10+dicnamelen, 0);

	/* send & receive */
	mny_con_sndrcv(buf, dicnamelen+14);
	return(getI32(buf+4));
}


int mny_open(char* pass, int portno, char *user)
{
	int ret = -1, usernamelen;
	char buf[MNY_MAX_BUF];

	memset(buf, 0, MNY_MAX_BUF);

	/* create socket */
	mny_con_create_socket(pass);

	/* Open */
	usernamelen = strlen(user);
	setI32(buf, 0x01);  /* プロトコル番号 */
	setI32(buf+4, (usernamelen + 5)); /* データ長 */
	strcpy(buf+8, "3.3:"); /* バージョン番号 */
	strcpy(buf+12, user); /* ユーザー名 */
	setI32((buf+12+usernamelen), 0); /* 末尾 */

	/* send & receive */
	mny_con_sndrcv(buf, usernamelen+16);
	if (getI32(buf) > 0) {
		ret = getS16(buf + 2);

		/* mount dic */
		mny_mountdic(ret, "bushu");
		mny_mountdic(ret, "user");
		mny_mountdic(ret, "hojoswd");
		mny_mountdic(ret, "hojomwd");
		mny_mountdic(ret, "fuzokugo");
		mny_mountdic(ret, "iroha");
	}

	return(ret);
}

int mny_convert(int id, char *yomi, char *conv)
{
	return(-1);
}

int mny_close(int id)
{
	char buf[MNY_MAX_BUF];

	memset(buf, 0, MNY_MAX_BUF);

	/* Finalize */
	*(buf)   = 0x02;
	*(buf+1) = 0x00;
	setS16(buf+2, 0);

	mny_con_sndrcv(buf, 4);
	if (*(buf+4)< 0) {
		printf("Finalize error\n");
	}

	return mny_con_close();
}

