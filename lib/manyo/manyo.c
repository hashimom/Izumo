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
 * Izumo -manyo.c-
 *   Author: Hashimoto Masahiko
 *
 */
#include <stdio.h>
#include <string.h>
#include <iconv.h>
#include "config.h"
#include "RK.h"
#include "manyo.h"
#include "connect.h"

#define MNY_MAX_BUF (1024)

enum {
	CONV_UTF82EUC = 0,
	CONV_EUC2UTF8,
};


#if 0
static unsigned short getS16(char *target_p)
{
	return((unsigned short)((target_p[0]<<8) | (target_p[1])));
}
#endif

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

static void exec_iconv(const char *instr, const char *outstr, int ch)
{
	iconv_t cd;
	size_t src_len = strlen(instr);
	size_t dest_len = MNY_MAX_BUF - 1;

	if (ch == CONV_UTF82EUC)
		cd = iconv_open("EUC-JP", "UTF-8");
	else
		cd = iconv_open("UTF-8", "EUC-JP");

	iconv(cd, (char **)&instr, &src_len, (char **)&outstr, &dest_len);
	iconv_close(cd);
}

static size_t utf82ushort(const char *src, unsigned short *dest, size_t destlen)
{
	char eucsrc[destlen];
	register size_t i, j;
	register unsigned ec;

	exec_iconv(src, eucsrc, CONV_UTF82EUC);

	/* EUC -> UShort (from Canna) ※EUC変換後の文字列が足りない？ Izumoでは終端をjの値と比較している */
	for (i = 0, j = 0 ; j < strlen(eucsrc) && j + 1 < destlen ; i++) {
		ec = (unsigned)(unsigned char)eucsrc[i];
		if (ec & 0x80) {
			switch (ec) {
			case 0x8e: /* SS2 */
				dest[j++] = (unsigned short)(0x80 | ((unsigned)eucsrc[++i] & 0x7f));
				break;
			case 0x8f: /* SS3 */
				dest[j++] = (unsigned short)(0x8000 |
						(((unsigned)eucsrc[i + 1] & 0x7f) << 8) |
						((unsigned)eucsrc[i + 2] & 0x7f));
				i += 2;
				break;
			default:
				/* Izumo移植時なぜか逆だったので修正している。上の「SS2」「SS3」も修正が必要かも */
				dest[j++] = (unsigned short)(0x8080 |
						(((unsigned)eucsrc[i + 1] & 0x7f) << 8) |
						((unsigned)eucsrc[i] & 0x7f));
				i++;
				break;
			}
		}
		else {
			dest[j++] = (unsigned short)ec;
		}
	}
	dest[j] = (unsigned short)0;
	return j;
}


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
	int conno = 0, ret, usernamelen;
	char buf[MNY_MAX_BUF];
	memset(buf, 0, MNY_MAX_BUF);
	usernamelen = strlen(user);

	mny_con_create_socket(pass);

	setI32(buf, 0x01);  /* プロトコル番号 */
	setI32(buf+4, (usernamelen+5)); /* データ長 */
	strcpy(buf+8, "3.3:"); /* バージョン番号 */
	strcpy(buf+12, user);
	setI32(buf+12+usernamelen, 0); /* 末尾 */
	mny_con_sndrcv(buf, 16+usernamelen);

	if (conno >= 0) {
		ret = mny_mountdic(conno, "bushu");
		ret = mny_mountdic(conno, "user");
		ret = mny_mountdic(conno, "hojoswd");
		ret = mny_mountdic(conno, "hojomwd");
		ret = mny_mountdic(conno, "fuzokugo");
		ret = mny_mountdic(conno, "iroha");
	}

	return(conno);
}

int mny_convert(int id, char *yomi, char *conv)
{
	int ret, yomilen;
	char buf[MNY_MAX_BUF];
	unsigned short yomibuf[MNY_MAX_BUF];

	memset(buf, 0, MNY_MAX_BUF);
	memset(yomibuf, 0, sizeof(yomibuf));

	yomilen = utf82ushort(yomi, yomibuf, MNY_MAX_BUF);

	/* convert */
	*(buf)   = 0x0f;
	*(buf+1) = 0x00;
	setS16(buf+2, 8+yomilen);
	setI32(buf+4, ((RK_XFER << RK_XFERBITS) | RK_KFER));
	setS16(buf+8, id);
	memcpy(buf+10, yomibuf, yomilen);
	setS16(buf+10+yomilen, 0);

	mny_con_sndrcv(buf, 12+yomilen);
	printf("%s\n", buf);
	return(0);
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

