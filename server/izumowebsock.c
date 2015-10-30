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

#include <iconv.h>
#include "server.h"
#include "websocket.h"

#define BUF_LEN 0xFFFF

/* まだシングルスレッドしか対応していません… */


static enum wsState wsstate = WS_STATE_OPENING;
static struct handshake hs;

enum {
	CONV_UTF82EUC = 0,
	CONV_EUC2UTF8,
};


static void exec_iconv(const char *instr, const char *outstr, int ch)
{
	iconv_t cd;
	size_t src_len = strlen(instr);
	size_t dest_len = BUF_LEN - 1;

	if (ch == CONV_UTF82EUC)
		cd = iconv_open("EUC-JP", "UTF-8");
	else
		cd = iconv_open("UTF-8", "EUC-JP");

	iconv(cd, (char **)&instr, &src_len, (char **)&outstr, &dest_len);
	iconv_close(cd);
}

static int conv_kanji(uint8_t *instr, uint8_t *outstr)
{
	int ret = 0, nbunsetsu = 0, nstrlen = 0, j;
	char eucbuf[BUF_LEN];
	char conveucbuf[BUF_LEN];
	char workbuf[BUF_LEN];
	Ushort eucusbuf[BUF_LEN], convusbuf[BUF_LEN], workconvbuf[BUF_LEN];
	size_t eucusbuflen;

	memset(outstr, 0, BUF_LEN);
	memset(eucbuf, 0, BUF_LEN);

	exec_iconv((const char*)instr, eucbuf, CONV_UTF82EUC);

	eucusbuflen = euc2ushort(eucbuf, BUF_LEN, eucusbuf, BUF_LEN);

	nbunsetsu = RkwBgnBun(0, eucusbuf, eucusbuflen, (RK_XFER << RK_XFERBITS) | RK_KFER);
	for (j = 0; j < nbunsetsu; j++) {
		RkwGoTo(0, j);
		RkwGetKanji(0, convusbuf, BUF_LEN);
		strncat((char*)workconvbuf, (char*)workbuf, (BUF_LEN - (nstrlen + 1)));
		nstrlen = strlen((const char*)(workconvbuf + 1));
	}
	RkwEndBun(0, 0);

	ushort2euc(workconvbuf, nstrlen, conveucbuf, BUF_LEN);

	exec_iconv(conveucbuf, (const char*)outstr, CONV_EUC2UTF8);

	return(ret);
}


#if 0
static enum wsFrameType IzumoWebSock_rcv(int fd, uint8_t *indata, size_t *indatasize)
{
	uint8_t rcvdata[BUF_LEN];
	size_t rcvlen;
	enum wsFrameType ret = WS_INCOMPLETE_FRAME;

	memset(rcvdata, 0, BUF_LEN);
	rcvlen = recv(fd, rcvdata, BUF_LEN, 0);

	/* data parse */
	if (wsstate == WS_STATE_OPENING) {
		ir_debug( Dmsg(5, "Izumo Websock Handshake phase.\n"));
		ret = wsParseHandshake(rcvdata, rcvlen, &hs);
		*indatasize = rcvlen;
	}
	else {
		ret = wsParseInputFrame(rcvdata, rcvlen, &indata, indatasize);
    }

	return(ret);
}
#endif

int IzumoWebSockRcvSnd(int fd)
{
	uint8_t gBuffer[BUF_LEN];
	uint8_t rcvdata[BUF_LEN];
	uint8_t *indata = NULL;
	size_t indataSize = 0;
	size_t rcvlen;
	size_t frameSize = BUF_LEN;
	enum wsFrameType frameType = WS_INCOMPLETE_FRAME;;

	/* cwebsocketのサンプルを参考に実装。ちょっと可読性悪いので後で直す。 */

	memset(gBuffer, 0, BUF_LEN);
	memset(rcvdata, 0, BUF_LEN);
//	memset(indata, 0, BUF_LEN);

	/* receive & parse */
//	frameType = IzumoWebSock_rcv(fd, indata, &indataSize);
	rcvlen = recv(fd, rcvdata, BUF_LEN, 0);
	if (wsstate == WS_STATE_OPENING) {
		ir_debug( Dmsg(5, "Izumo Websock Handshake phase.\n"));
		frameType = wsParseHandshake(rcvdata, rcvlen, &hs);
		indataSize = rcvlen;
	}
	else {
		frameType = wsParseInputFrame(rcvdata, rcvlen, &indata, &indataSize);
    }

	/* Error */
	if ((frameType == WS_INCOMPLETE_FRAME && indataSize >= BUF_LEN) || frameType == WS_ERROR_FRAME) {
		if (frameType == WS_INCOMPLETE_FRAME)
			ir_debug( Dmsg(5,"Warning: buffer too small.\n"));
		else
			ir_debug( Dmsg(5,"Error: in incoming frame.\n"));

		frameSize = BUF_LEN;
		memset(gBuffer, 0, BUF_LEN);
		if (wsstate == WS_STATE_OPENING) {
			frameSize = sprintf((char *)gBuffer,
					"HTTP/1.1 400 Bad Request\r\n"
					"%s%s\r\n\r\n",
					versionField,
					version);
			send(fd, gBuffer, frameSize, 0);
			return(0);
		}
		else {
			wsMakeFrame(NULL, 0, gBuffer, &frameSize, WS_CLOSING_FRAME);
			if (send(fd, gBuffer, frameSize, 0) < 0)
				return(0);
			wsstate = WS_STATE_CLOSING;
        }
    }

	/* make frame */
	if (wsstate == WS_STATE_OPENING) {
		if (frameType != WS_OPENING_FRAME) {
			ir_debug( Dmsg(5,"Warning: Cant Handshake IzumoWebSock.\n"));
			return(0);
		}
		if (frameType == WS_OPENING_FRAME) {
#if 0
			// if resource is right, generate answer handshake and send it
			if (strcmp(hs.resource, "/echo") != 0) {
				frameSize = sprintf((char *)gBuffer, "HTTP/1.1 404 Not Found\r\n\r\n");
				send(fd, gBuffer, frameSize, 0);
				return(-1);
			}
#endif
			frameSize = BUF_LEN;
			memset(gBuffer, 0, BUF_LEN);
			wsGetHandshakeAnswer(&hs, gBuffer, &frameSize);
			freeHandshake(&hs);
			if (send(fd, gBuffer, frameSize, 0) < 0)
				return(0);
			wsstate = WS_STATE_NORMAL;

			/* izumo open */
			int rkret = RkwInitialize(DICHOME);
			if (rkret >= 0) {
				ir_debug( Dmsg(5, "RkwInitialize OK\n"));
				RkwMountDic(0, "bushu", 0);
				RkwMountDic(0, "user", 0);
				RkwMountDic(0, "hojoswd", 0);
				RkwMountDic(0, "hojomwd", 0);
				RkwMountDic(0, "fuzokugo", 0);
				RkwMountDic(0, "iroha", 0);
			}
		}
	}
	else {
		if (frameType == WS_CLOSING_FRAME) {
			if (wsstate == WS_STATE_CLOSING) {
				return(0);
			}
			else {
				frameSize = BUF_LEN;
				memset(gBuffer, 0, BUF_LEN);
				wsMakeFrame(NULL, 0, gBuffer, &frameSize, WS_CLOSING_FRAME);
				send(fd, gBuffer, frameSize, 0);
				return(0);
			}
		}
		/* WS_TEXT_FRAME　通信 */
		else if (frameType == WS_TEXT_FRAME) {
			uint8_t *recievedString = NULL;
			recievedString = malloc(indataSize+1);
			if (recievedString != NULL) {
				memcpy(recievedString, indata, indataSize);
				recievedString[ indataSize ] = 0;

				uint8_t *convstring = malloc(BUF_LEN);
				if (convstring != NULL) {
					conv_kanji(recievedString, convstring);
					int convlen = strlen(convstring);

					frameSize = BUF_LEN;
					memset(gBuffer, 0, BUF_LEN);
					wsMakeFrame(convstring, convlen, gBuffer, &frameSize, WS_TEXT_FRAME);
					free(recievedString);
					free(convstring);
					if (send(fd, gBuffer, frameSize, 0) < 0)
						return(0);
				}
			}
        }
    }

	return(0);
}
