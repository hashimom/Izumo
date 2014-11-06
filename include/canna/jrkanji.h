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

/*
 *
 *	8/16 bit String Manipulations.
 *
 *      "@(#)kanji.h	2.3    88/10/03 10:25:34"
 *      "@(#) 102.1 $Id: jrkanji.h,v 1.8.2.2 2003/12/27 17:15:20 aida_s Exp $"
 */

#ifndef _JR_KANJI_H_
#define _JR_KANJI_H_

#ifndef _WCHAR_T
# if defined(WCHAR_T) || defined(_WCHAR_T_) || defined(__WCHAR_T) \
  || defined(_GCC_WCHAR_T) || defined(_WCHAR_T_DEFINED)
#  define _WCHAR_T
# endif
#endif

#ifdef pro
#define CANNA_PRO_PREDEFINED
#else
#if defined(__STDC__) || defined(__cplusplus)
#define pro(x) x
#else
#define pro(x) ()
#endif
#endif

#include <canna/sysdep.h>
#include <canna/keydef.h>
#include <canna/mfdef.h>

/* �ɤΤ褦�ʾ��󤬤��뤫�򼨤��ե饰 */

#define KanjiModeInfo   	0x1
#define KanjiGLineInfo  	0x2
#define KanjiYomiInfo		0x4
#define KanjiThroughInfo	0x8
#define KanjiEmptyInfo		0x10

#define KanjiExtendInfo		0x20
#define KanjiKigoInfo		0x40 
#define KanjiRussianInfo	0x80
#define KanjiGreekInfo		0x100
#define KanjiLineInfo		0x200

#define KanjiAttributeInfo	0x400
#define KanjiSpecialFuncInfo	0x800

/* KanjiControl �ط� */

#define KC_INITIALIZE		0
#define KC_FINALIZE		1
#define KC_CHANGEMODE		2
#define KC_SETWIDTH		3
#define KC_SETUNDEFKEYFUNCTION	4
#define KC_SETBUNSETSUKUGIRI    5
#define KC_SETMODEINFOSTYLE	6
#define KC_SETHEXINPUTSTYLE	7
#define KC_INHIBITHANKAKUKANA	8
#define KC_DEFINEKANJI		9
#define KC_KAKUTEI		10
#define KC_KILL			11
#define KC_MODEKEYS		12
#define KC_QUERYMODE		13
#define KC_QUERYCONNECTION	14
#define KC_SETSERVERNAME        15
#define KC_PARSE		16
#define KC_YOMIINFO		17
#define KC_STOREYOMI		18
#define KC_SETINITFILENAME	19
#define KC_DO			20
#define KC_GETCONTEXT		21
#define KC_CLOSEUICONTEXT	22
#define KC_INHIBITCHANGEMODE	23
#define KC_LETTERRESTRICTION	24
#define KC_QUERYMAXMODESTR	25
#define KC_SETLISTCALLBACK	26
#define KC_SETVERBOSE		27
#define KC_LISPINTERACTION	28
#define KC_DISCONNECTSERVER	29
#define KC_SETAPPNAME	        30
#define KC_DEBUGMODE	        31
#define KC_DEBUGYOMI	        32
#define KC_KEYCONVCALLBACK	33
#define KC_QUERYPHONO		34
#define KC_CHANGESERVER		35
#define KC_SETUSERINFO          36
#define KC_QUERYCUSTOM          37
#define KC_CLOSEALLCONTEXT      38
#define KC_ATTRIBUTEINFO	39

#define MAX_KC_REQUEST          (KC_ATTRIBUTEINFO + 1)

#define kc_normal	0
#define kc_through	1
#define kc_kakutei	2
#define kc_kill		3

#define CANNA_NOTHING_RESTRICTED	0
#define CANNA_ONLY_ASCII		1
#define CANNA_ONLY_ALPHANUM		2	
#define CANNA_ONLY_HEX			3
#define CANNA_ONLY_NUMERIC		4
#define CANNA_NOTHING_ALLOWED		5

#ifdef IROHA_BC
#define IROHA_NOTHING_RESTRICTED	CANNA_NOTHING_RESTRICTED
#define IROHA_ONLY_ASCII		CANNA_ONLY_ASCII
#define IROHA_ONLY_ALPHANUM		CANNA_ONLY_ALPHANUM
#define IROHA_ONLY_HEX			CANNA_ONLY_HEX
#define IROHA_ONLY_NUMERIC		CANNA_ONLY_NUMERIC
#define IROHA_NOTHING_ALLOWED		CANNA_NOTHING_ALLOWED
#endif

#define CANNA_ATTR_INPUT                    ((char)'.')
#define CANNA_ATTR_TARGET_CONVERTED         ((char)'O')
#define CANNA_ATTR_CONVERTED                ((char)'_')
#define CANNA_ATTR_TARGET_NOTCONVERTED      ((char)'x')
#define CANNA_ATTR_INPUT_ERROR              ((char)'E')

#define CANNA_MAXAPPNAME       256

typedef struct {
    unsigned char *echoStr;    /* local echo string */
    int length;		        /* length of echo string */
    int revPos;                 /* reverse position  */
    int revLen;                 /* reverse length    */
    unsigned long info;		/* ����¾�ξ��� */
    unsigned char *mode;	/* �⡼�ɾ��� */
    struct {
      unsigned char *line;
      int           length;
      int           revPos;
      int           revLen;
    } gline;			/* ����ɽ���Τ���ξ��� */
} jrKanjiStatus;

typedef struct {
  int  val;
  unsigned char *buffer;
  int  bytes_buffer;
  jrKanjiStatus *ks;
} jrKanjiStatusWithValue;

typedef struct {
  char *uname;		/* �桼��̾ */
  char *gname;		/* ���롼��̾ */
  char *srvname;	/* ������̾ */
  char *topdir;		/* ���󥹥ȡ���ǥ��쥯�ȥ� */
  char *cannafile;	/* �������ޥ����ե�����̾ */
  char *romkanatable;   /* ���޻������Ѵ��ơ��֥�̾ */
  char *appname;	/* ���ץꥱ�������̾ */
} jrUserInfoStruct;

typedef struct {
  char *codeinput;	/* �����ɼ��� */
  int  quicklyescape;	/* ����Ϣ³���� flag */
  int  indexhankaku;	/* �����ɥ饤��Υ���ǥå������� */
  int  indexseparator;	/* �����ɥ饤��Υ���ǥå������� */
  int  selectdirect;	/* ���������ˤ������ flag */
  int  numericalkeysel;	/* ���������ˤ������������ */
  int  kouhocount;	/* �����ɽ�� */
} jrCInfoStruct;

#define CANNA_EUC_LISTCALLBACK
typedef struct {
  char *client_data;
  int (*callback_func) pro((char *, int, char **, int, int *));
} jrEUCListCallbackStruct;

#ifndef CANNAWC_DEFINED
# if defined(_WCHAR_T) || defined(CANNA_NEW_WCHAR_AWARE)
#  define CANNAWC_DEFINED
#  ifdef CANNA_NEW_WCHAR_AWARE
#   ifdef CANNA_WCHAR16
typedef canna_uint16_t cannawc;
#   else
typedef canna_uint32_t cannawc;
#   endif
#  elif defined(_WCHAR_T) /* !CANNA_NEW_WCHAR_AWARE */
typedef wchar_t cannawc;
#  endif
# endif
#endif

#ifdef CANNAWC_DEFINED

typedef struct {
    cannawc *echoStr;		/* local echo string */
    int length;		        /* length of echo string */
    int revPos;                 /* reverse position  */
    int revLen;                 /* reverse length    */
    unsigned long info;		/* ����¾�ξ��� */
    cannawc  *mode;		/* �⡼�ɾ��� */
    struct {
      cannawc       *line;
      int           length;
      int           revPos;
      int           revLen;
    } gline;			/* ����ɽ���Τ���ξ��� */
} wcKanjiStatus;

typedef struct {
  int  val;
  cannawc *buffer;
  int  n_buffer;
  wcKanjiStatus *ks;
} wcKanjiStatusWithValue;

typedef struct {
  char *client_data;
  int (*callback_func) pro((char *, int, cannawc **, int, int *));
} jrListCallbackStruct;

typedef struct {
  char *attr;
  long caretpos;
} wcKanjiAttribute;

#define listCallbackStruct jrListCallbackStruct

#define CANNA_LIST_Start           0
#define CANNA_LIST_Select          1
#define CANNA_LIST_Quit            2
#define CANNA_LIST_Forward         3
#define CANNA_LIST_Backward        4
#define CANNA_LIST_Next            5
#define CANNA_LIST_Prev            6
#define CANNA_LIST_BeginningOfLine 7
#define CANNA_LIST_EndOfLine       8
#define CANNA_LIST_Query           9
#define CANNA_LIST_Bango          10
#define CANNA_LIST_PageUp         11
#define CANNA_LIST_PageDown       12
#define CANNA_LIST_Convert	  13
#define CANNA_LIST_Insert	  14

#endif /* CANNAWC_DEFINED */

#define CANNA_NO_VERBOSE   0
#define CANNA_HALF_VERBOSE 1
#define CANNA_FULL_VERBOSE 2

#define CANNA_CTERMINAL 0
#define CANNA_XTERMINAL 1

#ifdef __cplusplus
extern "C" {
#endif
extern char *jrKanjiError;
#ifdef CANNA_NEW_WCHAR_AWARE /* to avoid problems in old programs */
extern int (*jrBeepFunc) pro((void));
# define CANNA_JR_BEEP_FUNC_DECLARED
#endif
#ifdef __cplusplus
}
#endif

#define wcBeepFunc jrBeepFunc

#ifdef __cplusplus
extern "C" {
#endif

int jrKanjiString pro((const int, const int, char *, const int,
			    jrKanjiStatus *));
int jrKanjiControl pro((const int, const int, char *));
int kanjiInitialize pro((char ***));
int kanjiFinalize pro((char ***));
int createKanjiContext pro((void));
int jrCloseKanjiContext pro((const int, jrKanjiStatusWithValue *));

#ifdef CANNAWC_DEFINED
#ifdef CANNA_NEW_WCHAR_AWARE
# define wcKanjiString cannawcKanjiString
# define wcKanjiControl cannawcKanjiControl
# define wcCloseKanjiContext cannawcCloseKanjiContext
#endif /*CANNA_NEW_WCHAR_AWARE */
int wcKanjiString pro((const int, const int, cannawc *, const int,
			    wcKanjiStatus *));
int wcKanjiControl pro((const int, const int, char *));
int wcCloseKanjiContext pro((const int, wcKanjiStatusWithValue *));
#endif /* CANNAWC_DEFINED */

#ifdef __cplusplus
}
#endif

#ifdef CANNA_PRO_PREDEFINED
#undef CANNA_PRO_PREDEFINED
#else
#undef pro
#endif

#endif /* _JR_KANJI_H_ */

