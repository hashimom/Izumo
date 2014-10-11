/* Copyright 1994 Pubdic+ Project.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Pubdic+
 * Project not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Pubdic+ Project makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * PUBDIC+ PROJECT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN 
 * NO EVENT SHALL PUBDIC+ PROJECT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF 
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR 
 * OTHER TORTUOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR 
 * PERFORMANCE OF THIS SOFTWARE. 
 */

#include <stdio.h>
#include "ccompat.h"

RCSID("$Id: pod.c,v 1.3.6.1 2004/05/04 22:04:43 aida_s Exp $");

typedef unsigned short Wchar;

static char *program;
static int compare, ignore_hinshi_to_compare, sort_by_frequency, merge_sj3;
static int merge_kind, wnn_type_output, canna_type_output, sj3_type_output;
static int list_kinds;
static int copy_frequency, extract_kana = 0;
static long specific_kind;
static FILE *in1, *in2;
static char *common_out, *old_out, *new_out, *hinshi_table, *bunrui;
static char *description_table;
static int selhinshi = 0;

/* hinshi_direction */
#define INORDER 0
#define REVERSE 1

static int hinshi_direction = INORDER; /* see above */

#define READBUFSIZE 128
#define DICBUFSIZE (2 << 13)
#define DICBUFINDEXMASK (DICBUFSIZE - 1)
#define HINSHIBUFSIZE (2 << 13)
#define HINSHIBUFINDEXMASK (HINSHIBUFSIZE - 1)

/* status of intern() */
#define FOUND 0
#define CREATE 1

# define SS2 0x8e
# define SS3 0x8f
# define MSB 0x80
# define MSK 0x7f

# define WCG0 0x0000
# define WCG1 0x8080
# define WCG2 0x0080
# define WCG3 0x8000
# define WCMSK 0x8080

int
Mbstowcs(d, ss, n)
Wchar *d;
char *ss;
int n;
{
  register Wchar *p = d;
  register int ch;
  register unsigned char *s = (unsigned char *)ss;

  while ((ch = *s++) && (p - d < n)) {
    if (ch & MSB) {
      if (ch == SS2) { /* kana */
	*p++ = (Wchar)*s++;
      }
      else if (ch == SS3) {
	*p++ = (Wchar)((*s << 8) | (*(s + 1) & MSK));
	s += 2;
      }
      else {
	*p++ = (Wchar)((ch << 8) | (*s++ & 0xff));
      }
    }
    else {
      *p++ = (Wchar)ch;
    }
  }
  *p = (Wchar)0;
  return p - d;
}

int
Wcstombs(d, s, n)
char *d;
Wchar *s;
int n;
{
  register unsigned char *p = (unsigned char *)d;
  register Wchar ch;

  while ((ch = *s++) && ((char *)p - d + 2 < n)) {
    switch (ch & WCMSK) {
    case WCG0:
      *p++ = ch & 0xff;
      break;

    case WCG1:
      *p++ = (ch >> 8) & 0xff;
      *p++ = ch & 0xff;
      break;

    case WCG2:
      *p++ = SS2;
      *p++ = ch & 0xff;
      break;

    case WCG3:
      *p++ = SS3;
      *p++ = (ch >> 8) & 0xff;
      *p++ = (ch & 0xff) | MSB;
      break;
    }
  }
  *p = '\0';
  return (char *)p - d;
}

int
Wscmp(s1, s2)
register Wchar *s1, *s2;
{
  register int res;

  /* �ʲ��Υ����ɤϤ��������ȥ�å����ʤΤǡ�������ä��Ƥ�������
     �ʲ��ǤϤ��Υ�������ˤ���褦�ʤ��Ȥ򤷤����櫓�Ǥ��롣

  while (*s1 && *s2 && && *s1 == *s2) {
    s1++; s2++;
  }
  return *s1 - *s2;

     ���ʤ����s1 �� s2 �� EOS ('\0') ��ؤ��Ƥ��ʤ��ơ��������ͤ�
     �ۤʤ�֤Ϥ��줾��Υݥ��󥿤�ʤ�롣�����줫�� EOS �ˤʤ뤫��
     �ͤ���äƤ������ˤϡ�*s1 - *s2 ���֤���
   */

  while (!(res = *s1 - *s2++) && *s1++)
    ;
  return res;
}

Wchar *
Wscpy(d, s)
Wchar *d;
register Wchar *s;
{
  register Wchar *p = d, ch;

  while (ch = *s++) {
    *p++ = ch;
  }
  *p = (Wchar)0;
  return d;
}

int
Wslen(s)
Wchar *s;
{
  register Wchar *p = s;

  while (*p) p++;
  return p - s;
}

int
Watoi(s)
Wchar *s;
{
  register int res = 0;
  register Wchar ch;

  while ((ch = *s++) && ((Wchar)'0' <= ch) && (ch <= (Wchar)'9')) {
    res *= 10;
    res += ch - (Wchar)'0';
  }
  return res;
}

static void
Fputws(s, f)
Wchar *s;
FILE *f;
{
  char buf[READBUFSIZE];

  if (Wcstombs(buf, s, READBUFSIZE)) {
    (void)fputs(buf, f);
  }
}

Wchar *
Fgetws(buf, siz, f)
Wchar *buf;
int siz;
FILE *f;
{
  char mbuf[READBUFSIZE], *p;

  p = fgets(mbuf, READBUFSIZE, f);
  if (p) {
    if (Mbstowcs(buf, mbuf, siz)) {
      return buf;
    }
  }
  return (Wchar *)0;
}

/* s �����ƥ������ʤ��鹽������Ƥ��뤫�ɤ������֤��ؿ� */

static int
all_kana(s)
Wchar *s;
{
  static Wchar xa = 0, xke, aa, *p;

  if (!xa) {
    (void)Mbstowcs(&xa,  "\045\041", 1);
    (void)Mbstowcs(&xke, "\045\166", 1);
    (void)Mbstowcs(&aa,  "\041\074", 1);
  }

  for (p = s ; *p ; p++) {
    if (!(*p == aa || (xa <= *p && *p <= xke))) {
      return 0;
    }
  }

  return 1;
}

/* ����å����õ�� */

static Wchar *
findslash(s)
Wchar *s;
{
  while (*s) {
    if (*s == (Wchar)'/') {
      return s;
    }
    s++;
  }
  return (Wchar *)0;
}

/* �ȡ�������ļ��Ф� */

static Wchar *
extstr(p, pp, key_return)
Wchar *p, **pp;
int *key_return;
{
  Wchar *res;
  int key = 0;

  while (*p == (Wchar)' ' || *p == (Wchar)'\t') p++;
  res = p;
  while (*p && *p != (Wchar)' ' && *p != (Wchar)'\t' && *p != (Wchar)'\n') {
    key += (int)*p++;
  }
  *p++ = (Wchar)'\0';
  if (pp) *pp = p;
  if (key_return) *key_return = key;
  return res;
}

/* �ʻ��ɽ����¤�� */

struct hinshipack {
  int nhinshis;
  Wchar *hinshi;
  unsigned flags; /* see below */
  struct hinshipack *next;
};

/* values of flags */
#define REPLACED 1

static struct hinshipack *partsofspeech[HINSHIBUFSIZE];

static void
malloc_failed()
{
  (void)fprintf(stderr, "%s: malloc failed.\n", program);
}

/* �ʻ�̾���ʻ�̾�ơ��֥����Ͽ���� */

static struct hinshipack *
internhinshi(str, flag)
Wchar *str;
int flag;
{
  struct hinshipack *p, **pp;
  Wchar *s;
  int key = 0;

  for (s = str ; *s ; s++) key += (int)*s;
  key = ((unsigned)key & HINSHIBUFINDEXMASK);
  for (pp = partsofspeech + key ; p = *pp ; pp = &(p->next)) {
    if (!Wscmp(p->hinshi, str)) {
      return p;
    }
  }
  if (flag) {
    p = (struct hinshipack *)malloc(sizeof(struct hinshipack));
    if (p) {
      *pp = p;
      (void)bzero((char *)p, sizeof(struct hinshipack));
      p->hinshi = (Wchar *)malloc((Wslen(str) + 1) * sizeof(Wchar));
      if (p->hinshi) {
	(void)Wscpy(p->hinshi, str);
	p->nhinshis = 1;
	return p;
      }
      free((char *)p);
    }
    malloc_failed();
  }
  return (struct hinshipack *)0;
}

/* �ʻ�̾���֤������� */

static void
replace_hinshi()
{
  FILE *f;
  Wchar readbuf[READBUFSIZE], *to, *from, *s;
  struct hinshipack *hinshientry, *p;
  int i, err = 0;

  f = fopen(hinshi_table, "r");
  if (!f) {
    (void)fprintf(stderr, 
	    "%s: can not open the table file of parts of speech \"%s\".\n",
	    program, hinshi_table);
    exit(1);
  }
  while (s = Fgetws(readbuf, READBUFSIZE, f)) {
    from = extstr(s, &s, (int *)0);
    to = extstr(s, &s, (int *)0);
    if (hinshi_direction == REVERSE) {
      Wchar *xx = from;
      from = to;
      to = xx;
    }

    hinshientry = internhinshi(from, 0);
    if (hinshientry) {
      Wchar *xx;

      xx = (Wchar *)malloc((Wslen(to) + 1) * sizeof(Wchar));
      if (xx) {
	Wchar *cp;	
	int n = 1;

	(void)Wscpy(xx, to);
	free((char *)hinshientry->hinshi);
	hinshientry->hinshi = xx;
	for (cp = xx ; *cp ; cp++) {
	  if (*cp == (Wchar)'/') {
	    *cp = (Wchar)0;
	    n++;
	  }
	}
	hinshientry->nhinshis = n;
	hinshientry->flags |= REPLACED;
      }
      else {
	malloc_failed();
      }
    }
  }
  (void)fclose(f);

  for (i = 0 ; i < HINSHIBUFSIZE ; i++) {
    for (p = partsofspeech[i] ; p ; p = p->next) {
      if (!(p->flags & REPLACED)) {
	(void)fprintf(stderr, "%s: The replacement for \"", program);
	Fputws(p->hinshi, stderr);
	(void)fprintf(stderr, "\" is not mentioned in the table.\n");
	err = 1;
      }
    }
  }
  if (err) {
    exit(1);
  }
}

static void
select_hinshi(n)
int n;
{
  Wchar *s, *t, *xx;
  struct hinshipack *p;
  int i;

  if (!n) return;

  for (i = 0 ; i < HINSHIBUFSIZE ; i++) {
    for (p = partsofspeech[i] ; p ; p = p->next) {
      switch (n) {
      case 1:
	s = findslash(p->hinshi);
	if (s) {
	  *s = (Wchar)0;
	}
	break;

      case 2:
	s = findslash(p->hinshi);
	if (s) {
	  s++;
	  t = findslash(s);
	  if (t) {
	    xx = (Wchar *)malloc((t - s + 1) * sizeof(Wchar));
	    if (xx) {
	      *t = (Wchar)0;
	      (void)Wscpy(xx, s);
	      t = p->hinshi;
	      p->hinshi = xx;
	      (void)free((char *)t);
	    }
	  }
	}
	break;

      case 3:
	s = findslash(p->hinshi);
	if (s) {
	  t = findslash(s + 1);
	  if (t) {
	    t++;
	    xx = (Wchar *)malloc((Wslen(t) + 1) * sizeof(Wchar));
	    if (xx) {
	      (void)Wscpy(xx, t);
	      t = p->hinshi;
	      p->hinshi = xx;
	      (void)free((char *)t);
	    }
	  }
	}
	break;

      default:
	break;
      }
    }
  }
}

/* ���߷����ɲä��뤿��Υ롼��ե����������ɽ��(���Ȼפ�) */

struct descpack {
  Wchar *hinshi, *tandesc, *yomdesc;
  struct descpack *next;
};

static void
freedesc(p)
struct descpack *p;
{
  free((char *)p->hinshi);
  free((char *)p->tandesc);
  free((char *)p->yomdesc);
  free((char *)p);
}

static struct descpack *description[HINSHIBUFSIZE];

/* �롼�����Ͽ */

static struct descpack *
interndesc(hin, tan, yom)
Wchar *hin, *tan, *yom;
{
  struct descpack *p, **pp, *next = (struct descpack *)0;
  Wchar *s;
  int key = 0;

  for (s = hin ; *s ; s++) key += (int)*s;
  key = ((unsigned)key & HINSHIBUFINDEXMASK);
  for (pp = description + key ; p = *pp ; pp = &(p->next)) {
    if (!Wscmp(p->hinshi, hin)) {
      if (!Wscmp(p->tandesc, tan) && !Wscmp(p->yomdesc, yom)) {
	return p;
      }
      else {
	*pp = next = p->next;
	freedesc(p);
	break;
      }
    }
  }
  p = (struct descpack *)malloc(sizeof(struct descpack));
  if (p) {
    *pp = p;
    (void)bzero((char *)p, sizeof(struct descpack));
    p->next = next;
    p->hinshi = (Wchar *)malloc((Wslen(hin) + 1) * sizeof(Wchar));
    if (p->hinshi) {
      (void)Wscpy(p->hinshi, hin);
      p->tandesc = (Wchar *)malloc((Wslen(tan) + 1) * sizeof(Wchar));
      if (p->tandesc) {
	(void)Wscpy(p->tandesc, tan);
	p->yomdesc = (Wchar *)malloc((Wslen(yom) + 1) * sizeof(Wchar));
	if (p->yomdesc) {
	  (void)Wscpy(p->yomdesc, yom);
	  return p;
	}
	free((char *)p->tandesc);
      }
      free((char *)p->hinshi);
    }
    free((char *)p);
  }
  malloc_failed();
  return (struct descpack *)0;
}

/* �롼���õ�� */

static struct descpack *
searchdesc(hin)
Wchar *hin;
{
  struct descpack *p, **pp;
  Wchar *s;
  int key = 0;

  for (s = hin ; *s ; s++) key += (int)*s;
  key = ((unsigned)key & HINSHIBUFINDEXMASK);
  for (pp = description + key ; p = *pp ; pp = &(p->next)) {
    if (!Wscmp(p->hinshi, hin)) {
      return p;
    }
  }
  return (struct descpack *)0;
}

static void
store_description()
{
  FILE *f;
  Wchar readbuf[READBUFSIZE], *hin, *tan, *yom, *s;

  if (!description_table) {
    return;
  }

  f = fopen(description_table, "r");
  if (!f) {
    (void)fprintf(stderr, 
	    "%s: can not open the table file of parts of speech \"%s\".\n",
	    program, description_table);
    exit(1);
  }
  while (s = Fgetws(readbuf, READBUFSIZE, f)) {
    Wchar nl[1];

    nl[0] = (Wchar)0;
    hin = tan = yom = nl;
    hin = extstr(s, &s, (int *)0);
    if (*hin) {
      tan = extstr(s, &s, (int *)0);
      if (*tan) {
	yom = extstr(s, &s, (int *)0);
      }
    }

    (void)interndesc(hin, tan, yom);
  }
  (void)fclose(f);
}

/* ����ȥ�μ��̤�ɽ����¤�Τ���¾ */

struct kindpack {
  Wchar *kind;
  long kindbit;
} kinds[sizeof(long) * 8];
static int nkinds;

#define KIHONBIT 1L

/* ���̤���Ͽ */

static long
internkind(s)
Wchar *s;
{
  int i;
  Wchar *p;

  p = findslash(s);
  if (p) {
    long res;

    *p = (Wchar)'\0';
    res = internkind(s);
    res |= internkind(p + 1);
    return res;
  }
  else {
    for (i = 0 ; i < nkinds ; i++) {
      if (!Wscmp(s, kinds[i].kind)) {
	return kinds[i].kindbit;
      }
    }
    if (nkinds < (sizeof(long) * 8) &&
	(kinds[nkinds].kind = (Wchar *)malloc((Wslen(s) + 1) * sizeof(Wchar)))
	) {
      (void)Wscpy(kinds[nkinds].kind, s);
      kinds[nkinds].kindbit = 1 << nkinds;
      return kinds[nkinds++].kindbit;
    }
    return 0;
  }
}

/* ���̤ΰ����ν��� */

static void
listkinds()
{
  int i;

  for (i = 0 ; i < nkinds ; i++) {
    Fputws(kinds[i].kind, stdout);
    (void)putchar('\n');
  }
}

static int
kindcompar(k1, k2)
struct kindpack *k1, *k2;
{
  return Wscmp(k1->kind, k2->kind);
}

static void
sortkind()
{
  qsort((char *)kinds, nkinds, sizeof(struct kindpack), kindcompar);
}

/* �����ɽ����¤�� */

struct dicpack {
  Wchar *yomi, *tango;
  struct hinshipack *hinshi;
  int hindo;
  long kind;
  Wchar *extdata;
  unsigned flags; /* SEE BELOW */
  struct dicpack *next;
};

#define COMMON 001
#define NEW    002

static struct dicpack *dic[DICBUFSIZE], **pdic;
static int ndicentries = 0;

/*

 intern -- ���񥨥�ȥ�θ���/��Ͽ

 �裶������ stat �Ȥ��ƥ̥�Ǥʤ����ɥ쥹�����ꤵ�줿���ˤϡ�Ʊ������ȥ�
 ����Ͽ����Ƥ��ʤ����ˤ���Ͽ��Ԥ������ɥ쥹���̥�ξ��ˤ���Ͽ���ʤ���

 flags �ˤ�äƤ�����Ȼ���򤹤롣(�ʲ��򸫤Ƥ�)��

 hinshi �� 0 ���Ϥ��ƤϤ����ʤ���kind �� 0 ���Ϥ��Ƥ�Ĥ�����-m �λ�����ʤ�
 �ޥå��Ϥ��ʤ��Τ���ա�

 */

/* flags */
#define IGNORE_HINSHI 1L
#define IGNORE_KIND   2L

static struct dicpack *
intern(key, yomi, kouho, hinshi, hindo, kind, stat, flags)
int key, hindo, *stat;
Wchar *yomi, *kouho, *hinshi;
long kind, flags;
{
  struct dicpack *p, **pp;
  struct descpack *dp;
  Wchar nl[1], *yomdesc = nl, *tandesc = nl;
  Wchar *yom = (Wchar *)0, *tan = (Wchar *)0, *dhinshi, *dh;

  nl[0] = (Wchar)'\0';

  if (description_table) {
    dhinshi = dh = hinshi; /* ����ʤ��ʻ��õ�� */
    while (*dh) {
      if (*dh++ == (Wchar)'/') {
	dhinshi = dh;
      }
    }
    dp = searchdesc(dhinshi);
    if (dp) {
      yomdesc = dp->yomdesc;
      tandesc = dp->tandesc;
      if (Wslen(yomdesc)) {
	Wchar *t;
	t = (Wchar *)malloc((Wslen(yomi) + Wslen(yomdesc) + 1)
			    * sizeof(Wchar));
	if (t) {
	  (void)Wscpy(t, yomi);
	  yom = yomi = t;
	  (void)Wscpy(yomi + Wslen(yomi), yomdesc);
	}
      }
      if (Wslen(tandesc)) {
	Wchar *t;
	t = (Wchar *)malloc((Wslen(kouho) + Wslen(tandesc) + 1)
			    * sizeof(Wchar));
	if (t) {
	  (void)Wscpy(t, kouho);
	  tan = kouho = t;
	  (void)Wscpy(kouho + Wslen(kouho), tandesc);
	}
      }
    }
    else {
      char foo[64];

      (void)fprintf(stderr, "no description rule for ");
      (void)Wcstombs(foo, dhinshi, 64);
      (void)fprintf(stderr, "%s.\n", foo);
    }
  }

  key = ((unsigned)key & DICBUFINDEXMASK);
  for (pp = dic + key ; p = *pp ; pp = &(p->next)) {
    if (!Wscmp(p->yomi, yomi) && !Wscmp(p->tango, kouho) &&
	((flags & IGNORE_HINSHI) || !Wscmp(p->hinshi->hinshi, hinshi)) &&
	((flags & IGNORE_KIND) || ((p->kind & kind) == kind)) ) {
      /* match */
      if (stat) *stat = FOUND;
      if (yom) free((char *)yom);
      if (tan) free((char *)tan);
      return p;
    }
  }
  if (stat) {
    p = (struct dicpack *)malloc(sizeof(struct dicpack));
    if (p) {
      *pp = p;
      (void)bzero((char *)p, sizeof(struct dicpack));
      p->yomi = (Wchar *)malloc((Wslen(yomi) + 1) * sizeof(Wchar));
      if (p->yomi) {
	(void)Wscpy(p->yomi, yomi);
	p->tango = (Wchar *)malloc((Wslen(kouho) + 1) * sizeof(Wchar));
	if (p->tango) {
	  (void)Wscpy(p->tango, kouho);
	  p->hinshi = internhinshi(hinshi, 1);
	  if (p->hinshi) {
	    p->hindo = hindo;
	    *stat = CREATE;
	    ndicentries++;
	    p->kind = kind;
	    p->extdata = (Wchar *)0;
	    if (yom) free((char *)yom);
	    if (tan) free((char *)tan);
	    return p;
	  }
	  free((char *)p->tango);
	}
	free((char *)p->yomi);
      }
      free((char *)p);
    }
    malloc_failed();
  }
  if (yom) free((char *)yom);
  if (tan) free((char *)tan);
  return (struct dicpack *)0;
}

#if 0 /* unused */
/* ��Ͽ����Ƥ��륨��ȥ���Ф��� fn ��¹Ԥ��� */

static void
for_all_interned(fn)
void (*fn)();
{
  int i;
  struct dicpack *p; 

  for (i = 0 ; i < DICBUFSIZE ; i++) {
    for (p = dic[i] ; p ; p = p->next) {
      (*fn)(p);
    }
  }
}
#endif

static void
storepd(file)
FILE *file;
{
  Wchar readbuf[READBUFSIZE], *p, *yomi, *hinshi, *kouho, *hindo, *kind;
  int nhindo, key, tkey, stat;
  long kindbit;
  struct dicpack *dicentry;

  while (p = Fgetws(readbuf, READBUFSIZE, file)) {
    key = 0;
    yomi = extstr(p, &p, &tkey); key += tkey;
    kouho = extstr(p, &p, &tkey); key += tkey;
    hinshi = extstr(p, &p, (int *)0);
    hindo = extstr(p, &p, (int *)0);
    nhindo = Watoi(hindo);

    kind = extstr(p, (Wchar **)0, (int *)0);
    if (*kind) {
      kindbit = internkind(kind);
    }
    else {
      kindbit = KIHONBIT;
    }

    dicentry = intern(key, yomi, kouho, hinshi, nhindo,
		      kindbit, &stat, IGNORE_KIND);
    if (dicentry) {
      dicentry->kind |= kindbit;
    }
  }
}

static void
comparepd(file)
FILE *file;
{
  Wchar readbuf[READBUFSIZE], *p, *yomi, *hinshi, *kouho, *hindo, *kind;
  int nhindo, key, tkey, stat, *statp = &stat;
  struct dicpack *dicentry;
  long kindbit, flags = 0L;

  while (p = Fgetws(readbuf, READBUFSIZE, file)) {
    key = 0;
    yomi = extstr(p, &p, &tkey); key += tkey;
    kouho = extstr(p, &p, &tkey); key += tkey;
    hinshi = extstr(p, &p, (int *)0);
    if (ignore_hinshi_to_compare) {
      flags |= IGNORE_HINSHI;
    }
    hindo = extstr(p, &p, (int *)0);
    nhindo = Watoi(hindo);

    kind = extstr(p, (Wchar **)0, (int *)0);
    if (*kind) {
      kindbit = internkind(kind);
    }
    else {
      kindbit = KIHONBIT;
    }
    if (merge_kind || merge_sj3) {
      flags |= IGNORE_KIND;
    }
    if (copy_frequency) {
      statp = (int *)0;
    }

    dicentry = intern(key, yomi, kouho, hinshi, nhindo,
		      kindbit, statp, flags);

    if (dicentry) {
      if (copy_frequency) {
	dicentry->hindo = nhindo;
	dicentry->flags &= ~COMMON;
      }
      else if (ignore_hinshi_to_compare && stat == FOUND) {
	/* ���ξ�硢Ʊ�������Υ��������֤� */
	struct dicpack *pd;

	for (pd = dicentry ; pd ; pd = pd->next) {
	  if (!Wscmp(pd->yomi, yomi) && !Wscmp(pd->tango, kouho)) {
	    pd->flags |= COMMON;
	    if (!merge_sj3) {
	      pd->kind |= kindbit;
	    }

	    if (merge_sj3) {
	      int len = 0;
	      Wchar *dat;

	      if (pd->extdata) {
		len = Wslen(pd->extdata);
	      }
	      dat = (Wchar *)malloc((Wslen(hinshi) + 1 + len) * sizeof(Wchar));
	      if (dat) {
		if (len) {
		  (void)Wscpy(dat, pd->extdata);
		  (void)free((char *)pd->extdata);
		}
		(void)Wscpy(dat + len, hinshi);
		pd->extdata = dat;
	      }
	    }
	  }
	}
      }
      else {
	dicentry->kind |= kindbit;
	if (stat == FOUND) {
	  dicentry->flags |= COMMON;
	}
	else { /* CREATE */
	  dicentry->flags |= NEW;
	}
      }
    }
  }
}

static void
canna_yomioutput(ws, cf)
Wchar *ws;
FILE *cf;
{
  Wchar yomi[READBUFSIZE];
  Wchar *yp;
  Wchar c;
  static Wchar u[3] = {0xa5f4, 0xa4a6, 0xa1ab};		/* �� �� �� */

  /*
   * ����ʼ�����ɤߤˡ֥��פ�����ˡ֤����פ���Ѥ���Τ�
   * �����Ѵ���Ԥʤ�
   */
  yp = yomi;
  while ((c = *ws++) != 0) {
    if (c == u[0]) {
      *yp++ = u[1];
      *yp++ = u[2];
    } else {
      *yp++ = c;
    }
  }
  *yp = 0;

  Fputws(yomi, cf);
}

static void
canna_output(cf, p, h, n)
FILE *cf;
struct dicpack *p;
Wchar *h;
int n;
{
  for (; n-- > 0 ; h += Wslen(h) + 1) {
    canna_yomioutput(p->yomi, cf);
    (void)putc(' ', cf);
    Fputws(h, cf);
    if (p->hindo) {
      (void)fprintf(cf, "*%d", p->hindo);
    }
    (void)putc(' ', cf);
    Fputws(p->tango, cf);
    (void)putc('\n', cf);
  }
}

static void
entry_out(cf, p, h, n, ex)
FILE *cf;
struct dicpack *p;
Wchar *h;
int n;
Wchar *ex;
{
  int i, f = 1;
  long b;

  for (; n-- > 0 ; h += Wslen(h) + 1) {
    Fputws(p->yomi, cf); (void)putc(' ', cf);
    Fputws(p->tango, cf); (void)putc(' ', cf);
    if (merge_sj3 && ex) {
      Fputws(ex, cf); (void)putc('/', cf);
    }
    Fputws(h, cf);
    if (!sj3_type_output) {
      (void)fprintf(cf, " %d", p->hindo);
    }

    if (!wnn_type_output) {
      if (bunrui) {
	(void)printf(" %s", bunrui);
      }
      else {
	if (specific_kind) {
	  b = (specific_kind & p->kind);
	}
	else {
	  b = p->kind;
	}
	if (b != KIHONBIT) { /* ���ܤ������ä��鲿��񤫤ʤ� */
	  for (i = 0 ; i < nkinds ; i++) {
	    if (b & kinds[i].kindbit) {
	      if (f) {
		(void)putc(' ', cf);
		f = 0;
	      }
	      else {
		(void)putc('/', cf);
	      }
	      Fputws(kinds[i].kind, cf);
	    }
	  }
	}
      }
    }
    (void)putc('\n', cf);
  }
}

/* p ��ɽ����륨��ȥ��ե����� cf �˽��Ϥ��� */

static void
printentry(cf, p)
FILE *cf;
struct dicpack *p;
{
  if (specific_kind && !(p->kind & specific_kind)) {
    return;
  }

  if (extract_kana && !all_kana(p->tango)) {
    return;
  }

  if (selhinshi && !p->hinshi->hinshi[0]) {
    return;
  }

  if (canna_type_output) {
    canna_output(cf, p, p->hinshi->hinshi, p->hinshi->nhinshis);
  }
  else {
    entry_out(cf, p, p->hinshi->hinshi, p->hinshi->nhinshis, p->extdata);
  }
}

#if 0 /* unused */
static void
showdeleted(p)
struct dicpack *p;
{
  if (!(p->flags & COMMON)) {
    (void)printf("- ");
    printentry(stdout, p);
  }
}
#endif

static void
showentry(pd, n)
struct dicpack **pd;
int n;
{
  FILE *cf = (FILE *)0, *of = (FILE *)0, *nf = (FILE *)0;
  struct dicpack *p;
  int i;

  if (common_out) {
    if (common_out[0] != '-' || common_out[1]) {
      cf = fopen(common_out, "w");
      if (!cf) {
	(void)fprintf(stderr,
		      "%s: can not open file \"%s\".\n", program, common_out);
	exit(1);
      }
    }
    else {
      cf = stdout;
    }
  }
  if (old_out) {
    if (old_out[0] != '-' || old_out[1]) {
      of = fopen(old_out, "w");
      if (!of) {
	(void)fprintf(stderr,
		      "%s: can not open file \"%s\".\n", program, old_out);
	exit(1);
      }
    }
    else {
      of = stdout;
    }
  }
  if (new_out) {
    if (new_out[0] != '-' || new_out[1]) {
      nf = fopen(new_out, "w");
      if (!nf) {
	(void)fprintf(stderr,
		      "%s: can not open file \"%s\".\n", program, new_out);
	exit(1);
      }
    }
    else {
      nf = stdout;
    }
  }

  for (i = 0 ; i < n ; i++) {
    p = pd[i];
    if (compare) {
      if (p->flags & COMMON) {
	if (cf) {
	  printentry(cf, p);
	}
      }
      else if (p->flags & NEW) {
	if (nf) {
	  printentry(nf, p);
	}
      }
      else {
	if (of) {
	  printentry(of, p);
	}
      }
    }
    else { /* just print the normalized dictionary */
      printentry(stdout, p);
    }
  }
}

static int
diccompar(p1, p2)
struct dicpack **p1, **p2;
{
  int n;
  if (n = Wscmp((*p1)->yomi, (*p2)->yomi)) {
    return n;
  }
  else if (n = Wscmp((*p1)->tango, (*p2)->tango)) {
    return n;
  }
  else if (n = Wscmp((*p1)->hinshi->hinshi, (*p2)->hinshi->hinshi)) {
    return n;
  }
  else { /* impossible */
    return 0;
  }
}

static int
dichindocompar(p1, p2)
struct dicpack **p1, **p2;
{
  int n;
  if (n = Wscmp((*p1)->yomi, (*p2)->yomi)) {
    return n;
  }
  else if (n = ((*p2)->hindo - (*p1)->hindo)) {
    return n;
  }
  else if (n = Wscmp((*p1)->tango, (*p2)->tango)) {
    return n;
  }
  else if (n = Wscmp((*p1)->hinshi->hinshi, (*p2)->hinshi->hinshi)) {
    return n;
  }
  else { /* impossible */
    return 0;
  }
}

static void
shrinkargs(argv, n, count)
char **argv;
int n, count;
{
  int i;

  for (i = 0 ; i + n < count ; i++) {
    argv[i] = argv[i + n];
  }
}

static void
parseargs(argc, argv)
int argc;
char *argv[];
{
  int i;

  for (program = argv[0] + strlen(argv[0]) ; argv[0] < program ; program--) {
    if (program[0] == '/') {
      program++;
      break;
    }
  }

  for (i = 1 ; i < argc ;) {
    if (argv[i][0] == '-' && argv[i][2] == '\0') {
      switch (argv[i][1]) {
      case '1':
      case '2':
      case '3':
	selhinshi = argv[i][1] - '0';
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;

      case 'b':
	bunrui = argv[i + 1];
	shrinkargs(argv + i, 2, argc - i); argc -= 2;
	break;
	
      case 'c':
	common_out = argv[i + 1];
	shrinkargs(argv + i, 2, argc - i); argc -= 2;
	break;

      case 'd':
	description_table = argv[i + 1];
	shrinkargs(argv + i, 2, argc - i); argc -= 2;
	break;

      case 'f':
	copy_frequency = 1;
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;

      case 'h':
	ignore_hinshi_to_compare = 1;
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;

      case 'i':
	canna_type_output = 1;
	wnn_type_output = 0;
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;
	
      case 'j':
	extract_kana = 1;
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;

      case 'k':
	{
	  Wchar buf[READBUFSIZE];

	  (void)Mbstowcs(buf, argv[i + 1], READBUFSIZE);
	  specific_kind |= internkind(buf);
	}
	shrinkargs(argv + i, 2, argc - i); argc -= 2;
	break;

      case 'l':
	list_kinds = 1;
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;

      case 'm':
	merge_kind = 1;
	shrinkargs(argv + i, 1, argc - 1); argc -= 1;
	break;

      case 'n':
	new_out = argv[i + 1];
	shrinkargs(argv + i, 2, argc - i); argc -= 2;
	break;

      case 'o':
	old_out = argv[i + 1];
	shrinkargs(argv + i, 2, argc - i); argc -= 2;
	break;

      case 'p':
	sort_by_frequency = 1;
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;

      case 'r':
	hinshi_table = argv[i + 1];
	shrinkargs(argv + i, 2, argc - i); argc -= 2;
	hinshi_direction = REVERSE;
	break;

      case 's':
	hinshi_table = argv[i + 1];
	shrinkargs(argv + i, 2, argc - i); argc -= 2;
	break;

      case 'v':
	sj3_type_output = 1;
	wnn_type_output = 1; /* Wnn �����Ȼ��Ƥ���Τ�Ω�Ƥ� */
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;

      case 'w':
	canna_type_output = 0;
	sj3_type_output = 0;
	wnn_type_output = 1;
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;

      case 'x':
	merge_sj3 = 1;
	ignore_hinshi_to_compare = 1;
	shrinkargs(argv + i, 1, argc - i); argc -= 1;
	break;

      default:
	i++;
	break;
      }
    }
    else {
      i++;
    }
  }

  if (argc < 2) {
    (void)fprintf(stderr,
		  "Usage: %s dic1 [dic2] [-c filecommon] ...\n", program);
    exit(1);
  }

  if (argv[1][0] != '-' || argv[1][1]) {
    in1 = fopen(argv[1], "r");
    if (!in1) {
      (void)fprintf(stderr,
		    "%s: can not open file \"%s\".\n", program, argv[1]);
      exit(1);
    }
  }
  if (argc == 3) {
    if (argv[2][0] != '-' || argv[2][1]) {
      in2 = fopen(argv[2], "r");
      if (!in2) {
	(void)fprintf(stderr,
		      "%s: can not open file \"%s\".\n", program, argv[2]);
	exit(1);
      }
    }
  }
  else {
    in2 = (FILE *)0;
  }
  if (description_table) {
    store_description();
  }
}

static Wchar kihonh[] = {
  (Wchar)'k', (Wchar)'i', (Wchar)'h', (Wchar)'o', (Wchar)'n', (Wchar)0,
};

int
main(argc, argv)
int argc;
char *argv[];
{
  in1 = in2 = stdin;

  (void)internkind(kihonh); /* ���ܼ����ѡ�1L �Ȥ�����Ͽ */
  parseargs(argc, argv);
  storepd(in1);
  (void)fclose(in1);

  if (in2) {
    compare = 1;
    comparepd(in2);
    (void)fclose(in2);
  }

  if (list_kinds) {
    listkinds();
    exit(0);
  }

  if (selhinshi) {
    select_hinshi(selhinshi);
  }
  else if (hinshi_table) {
    replace_hinshi();
  }

  pdic = (struct dicpack **)malloc(ndicentries * sizeof(struct dicpack *));
  if (pdic) {
    int i, j;
    struct dicpack *p; 

    for (i = 0, j = 0 ; i < DICBUFSIZE ; i++) {
      for (p = dic[i] ; p ; p = p->next) {
	pdic[j++] = p;
      }
    }
    if (sort_by_frequency) {
      qsort((char *)pdic, ndicentries, sizeof(struct dicpack *), dichindocompar);
    }
    else {
      qsort((char *)pdic, ndicentries, sizeof(struct dicpack *), diccompar);
    }
    sortkind();
    showentry(pdic, ndicentries);
  }
  else {
    malloc_failed();
  }
  exit(0);
  /* NOTREACHED */
}
