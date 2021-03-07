/*-
 * Copyright (c) 1992, 1993, 1994 Henry Spencer.
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)regcomp.c	8.5 (Berkeley) 3/20/94
 */

#ifndef _NO_REGEX

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)regcomp.c	8.5 (Berkeley) 3/20/94";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>

#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define DUPMAX _POSIX2_RE_DUP_MAX /* xxx is this right? */
#define INFINITY (DUPMAX + 1)
#define NC (CHAR_MAX - CHAR_MIN + 1)
typedef unsigned char uch;

/* switch off assertions (if not already off) if no REDEBUG */
#ifndef REDEBUG
#ifndef NDEBUG
#define NDEBUG /* no assertions please */
#endif
#endif
#include <assert.h>

/* for old systems with bcopy() but no memmove() */
#ifdef USEBCOPY
#define memmove(d, s, c) bcopy(s, d, c)
#endif
#include "regex_internal.h"

typedef enum {
  CALNUM,
  CALPHA,
  CBLANK,
  CCNTRL,
  CDIGIT,
  CGRAPH,
  CLOWER,
  CPRINT,
  CPUNCT,
  CSPACE,
  CUPPER,
  CXDIGIT
} citype;

/* character-class table */
static struct cclass {
  char *name;
  citype fidx;
} cclasses[] = {{"alnum", CALNUM},
                {"alpha", CALPHA},
                {"blank", CBLANK},
                {"cntrl", CCNTRL},
                {"digit", CDIGIT},
                {"graph", CGRAPH},
                {"lower", CLOWER},
                {"print", CPRINT},
                {"punct", CPUNCT},
                {"space", CSPACE},
                {"upper", CUPPER},
                {"xdigit", CXDIGIT},
                {
                    NULL,
                }};
/* character-name table */
static struct cname {
  char *name;
  char code;
} cnames[] = {{"NUL", '\0'},
              {"SOH", '\001'},
              {"STX", '\002'},
              {"ETX", '\003'},
              {"EOT", '\004'},
              {"ENQ", '\005'},
              {"ACK", '\006'},
              {"BEL", '\007'},
              {"alert", '\007'},
              {"BS", '\010'},
              {"backspace", '\b'},
              {"HT", '\011'},
              {"tab", '\t'},
              {"LF", '\012'},
              {"newline", '\n'},
              {"VT", '\013'},
              {"vertical-tab", '\v'},
              {"FF", '\014'},
              {"form-feed", '\f'},
              {"CR", '\015'},
              {"carriage-return", '\r'},
              {"SO", '\016'},
              {"SI", '\017'},
              {"DLE", '\020'},
              {"DC1", '\021'},
              {"DC2", '\022'},
              {"DC3", '\023'},
              {"DC4", '\024'},
              {"NAK", '\025'},
              {"SYN", '\026'},
              {"ETB", '\027'},
              {"CAN", '\030'},
              {"EM", '\031'},
              {"SUB", '\032'},
              {"ESC", '\033'},
              {"IS4", '\034'},
              {"FS", '\034'},
              {"IS3", '\035'},
              {"GS", '\035'},
              {"IS2", '\036'},
              {"RS", '\036'},
              {"IS1", '\037'},
              {"US", '\037'},
              {"space", ' '},
              {"exclamation-mark", '!'},
              {"quotation-mark", '"'},
              {"number-sign", '#'},
              {"dollar-sign", '$'},
              {"percent-sign", '%'},
              {"ampersand", '&'},
              {"apostrophe", '\''},
              {"left-parenthesis", '('},
              {"right-parenthesis", ')'},
              {"asterisk", '*'},
              {"plus-sign", '+'},
              {"comma", ','},
              {"hyphen", '-'},
              {"hyphen-minus", '-'},
              {"period", '.'},
              {"full-stop", '.'},
              {"slash", '/'},
              {"solidus", '/'},
              {"zero", '0'},
              {"one", '1'},
              {"two", '2'},
              {"three", '3'},
              {"four", '4'},
              {"five", '5'},
              {"six", '6'},
              {"seven", '7'},
              {"eight", '8'},
              {"nine", '9'},
              {"colon", ':'},
              {"semicolon", ';'},
              {"less-than-sign", '<'},
              {"equals-sign", '='},
              {"greater-than-sign", '>'},
              {"question-mark", '?'},
              {"commercial-at", '@'},
              {"left-square-bracket", '['},
              {"backslash", '\\'},
              {"reverse-solidus", '\\'},
              {"right-square-bracket", ']'},
              {"circumflex", '^'},
              {"circumflex-accent", '^'},
              {"underscore", '_'},
              {"low-line", '_'},
              {"grave-accent", '`'},
              {"left-brace", '{'},
              {"left-curly-bracket", '{'},
              {"vertical-line", '|'},
              {"right-brace", '}'},
              {"right-curly-bracket", '}'},
              {"tilde", '~'},
              {"DEL", '\177'},
              {NULL, 0}};

/*
 * parse structure, passed up and down to avoid global variables and
 * other clumsinesses
 */
struct parse {
  char *next;   /* next character in RE */
  char *end;    /* end of string (-> NUL normally) */
  int error;    /* has an error been seen? */
  sop *strip;   /* malloced strip */
  sopno ssize;  /* malloced strip size (allocated) */
  sopno slen;   /* malloced strip length (used) */
  int ncsalloc; /* number of csets allocated */
  struct re_guts *g;
#define NPAREN 10       /* we need to remember () 1-9 for back refs */
  sopno pbegin[NPAREN]; /* -> ( ([0] unused) */
  sopno pend[NPAREN];   /* -> ) ([0] unused) */
};

/* ========= begin header generated by ./mkh ========= */
#ifdef __cplusplus
extern "C" {
#endif

/* === regcomp.c === */
static void p_ere(struct parse *p, int stop);
static void p_ere_exp(struct parse *p);
static void p_str(struct parse *p);
static void p_bre(struct parse *p, int end1, int end2);
static int p_simp_re(struct parse *p, int starordinary);
static int p_count(struct parse *p);
static void p_bracket(struct parse *p);
static void p_b_term(struct parse *p, cset *cs);
static void p_b_cclass(struct parse *p, cset *cs);
static void p_b_eclass(struct parse *p, cset *cs);
static char p_b_symbol(struct parse *p);
static char p_b_coll_elem(struct parse *p, int endc);
static char othercase(int ch);
static void bothcases(struct parse *p, int ch);
static void ordinary(struct parse *p, int ch);
static void nonnewline(struct parse *p);
static void repeat(struct parse *p, sopno start, int from, int to);
static int seterr(struct parse *p, int e);
static cset *allocset(struct parse *p);
static void freeset(struct parse *p, cset *cs);
static int freezeset(struct parse *p, cset *cs);
static int firstch(struct parse *p, cset *cs);
static int nch(struct parse *p, cset *cs);
#if used
static void mcadd(struct parse *p, cset *cs, char *cp);
static void mcsub(cset *cs, char *cp);
static int mcin(cset *cs, char *cp);
static char *mcfind(cset *cs, char *cp);
#endif
static void mcinvert(struct parse *p, cset *cs);
static void mccase(struct parse *p, cset *cs);
static int isinsets(struct re_guts *g, int c);
static int samesets(struct re_guts *g, int c1, int c2);
static void categorize(struct parse *p, struct re_guts *g);
static sopno dupl(struct parse *p, sopno start, sopno finish);
static void doemit(struct parse *p, sop op, size_t opnd);
static void doinsert(struct parse *p, sop op, size_t opnd, sopno pos);
static void dofwd(struct parse *p, sopno pos, sop value);
static void enlarge(struct parse *p, sopno size);
static void stripsnug(struct parse *p, struct re_guts *g);
static void findmust(struct parse *p, struct re_guts *g);
static int altoffset(sop *scan, int offset, int mccs);
static void computejumps(struct parse *p, struct re_guts *g);
static void computematchjumps(struct parse *p, struct re_guts *g);
static sopno pluscount(struct parse *p, struct re_guts *g);

#ifdef __cplusplus
}
#endif
/* ========= end header generated by ./mkh ========= */

static char nuls[10]; /* place to point scanner in event of error */

/*
 * macros for use with parse structure
 * BEWARE:  these know that the parse structure is named `p' !!!
 */
#define PEEK() (*p->next)
#define PEEK2() (*(p->next + 1))
#define MORE() (p->next < p->end)
#define MORE2() (p->next + 1 < p->end)
#define SEE(c) (MORE() && PEEK() == (c))
#define SEETWO(a, b) (MORE() && MORE2() && PEEK() == (a) && PEEK2() == (b))
#define EAT(c) ((SEE(c)) ? (NEXT(), 1) : 0)
#define EATTWO(a, b) ((SEETWO(a, b)) ? (NEXT2(), 1) : 0)
#define NEXT() (p->next++)
#define NEXT2() (p->next += 2)
#define NEXTn(n) (p->next += (n))
#define GETNEXT() (*p->next++)
#define SETERROR(e) seterr(p, (e))
#define REQUIRE(co, e) ((co) || SETERROR(e))
#define MUSTSEE(c, e) (REQUIRE(MORE() && PEEK() == (c), e))
#define MUSTEAT(c, e) (REQUIRE(MORE() && GETNEXT() == (c), e))
#define MUSTNOTSEE(c, e) (REQUIRE(!MORE() || PEEK() != (c), e))
#define EMIT(op, sopnd) doemit(p, (sop)(op), (size_t)(sopnd))
#define INSERT(op, pos) doinsert(p, (sop)(op), HERE() - (pos) + 1, pos)
#define AHEAD(pos) dofwd(p, pos, HERE() - (pos))
#define ASTERN(sop, pos) EMIT(sop, HERE() - pos)
#define HERE() (p->slen)
#define THERE() (p->slen - 1)
#define THERETHERE() (p->slen - 2)
#define DROP(n) (p->slen -= (n))

#ifndef NDEBUG
static int never = 0; /* for use in asserts; shuts lint up */
#else
#define never 0 /* some <assert.h>s have bugs too */
#endif

/* Macro used by computejump()/computematchjump() */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*
 - regcomp - interface for parser and compilation
 = extern int regcomp(regex_t *__restrict, const char *__restrict, int);
 = #define	REG_BASIC	0000
 = #define	REG_EXTENDED	0001
 = #define	REG_ICASE	0002
 = #define	REG_NOSUB	0004
 = #define	REG_NEWLINE	0010
 = #define	REG_NOSPEC	0020
 = #define	REG_PEND	0040
 = #define	REG_DUMP	0200
 */
int /* 0 success, otherwise REG_something */
    regcomp(preg, pattern, cflags) regex_t *__restrict preg;
const char *__restrict pattern;
int cflags;
{
  struct parse pa;
  struct re_guts *g;
  struct parse *p = &pa;
  int i;
  size_t len;
#ifdef REDEBUG
#define GOODFLAGS(f) (f)
#else
#define GOODFLAGS(f) ((f) & ~REG_DUMP)
#endif

  cflags = GOODFLAGS(cflags);
  if ((cflags & REG_EXTENDED) && (cflags & REG_NOSPEC)) return (REG_INVARG);

  if (cflags & REG_PEND) {
    if (preg->re_endp < pattern) return (REG_INVARG);
    len = preg->re_endp - pattern;
  } else
    len = strlen((char *)pattern);

  /* do the mallocs early so failure handling is easy */
  g = (struct re_guts *)malloc(sizeof(struct re_guts) + (NC - 1) * sizeof(cat_t));
  if (g == NULL) return (REG_ESPACE);
  p->ssize = len / (size_t)2 * (size_t)3 + (size_t)1; /* ugh */
  p->strip = (sop *)malloc(p->ssize * sizeof(sop));
  p->slen = 0;
  if (p->strip == NULL) {
    free((char *)g);
    return (REG_ESPACE);
  }

  /* set things up */
  p->g = g;
  p->next = (char *)pattern; /* convenience; we do not modify it */
  p->end = p->next + len;
  p->error = 0;
  p->ncsalloc = 0;
  for (i = 0; i < NPAREN; i++) {
    p->pbegin[i] = 0;
    p->pend[i] = 0;
  }
  g->csetsize = NC;
  g->sets = NULL;
  g->setbits = NULL;
  g->ncsets = 0;
  g->cflags = cflags;
  g->iflags = 0;
  g->nbol = 0;
  g->neol = 0;
  g->must = NULL;
  g->moffset = -1;
  g->charjump = NULL;
  g->matchjump = NULL;
  g->mlen = 0;
  g->nsub = 0;
  g->ncategories = 1; /* category 0 is "everything else" */
  g->categories = &g->catspace[-(CHAR_MIN)];
  (void)memset((char *)g->catspace, 0, NC * sizeof(cat_t));
  g->backrefs = 0;

  /* do it */
  EMIT(OEND, 0);
  g->firststate = THERE();
  if (cflags & REG_EXTENDED)
    p_ere(p, OUT);
  else if (cflags & REG_NOSPEC)
    p_str(p);
  else
    p_bre(p, OUT, OUT);
  EMIT(OEND, 0);
  g->laststate = THERE();

  /* tidy up loose ends and fill things in */
  categorize(p, g);
  stripsnug(p, g);
  findmust(p, g);
  /* only use Boyer-Moore algorithm if the pattern is bigger
   * than three characters
   */
  if (g->mlen > 3) {
    computejumps(p, g);
    computematchjumps(p, g);
    if (g->matchjump == NULL && g->charjump != NULL) {
      free(g->charjump);
      g->charjump = NULL;
    }
  }
  g->nplus = pluscount(p, g);
  g->magic = MAGIC2;
  preg->re_nsub = g->nsub;
  preg->re_g = g;
  preg->re_magic = MAGIC1;
#ifndef REDEBUG
  /* not debugging, so can't rely on the assert() in regexec() */
  if (g->iflags & BAD) SETERROR(REG_ASSERT);
#endif

  /* win or lose, we're done */
  if (p->error != 0) /* lose */
    regfree(preg);
  return (p->error);
}

/*
 - p_ere - ERE parser top level, concatenation and alternation
 == static void p_ere(struct parse *p, int stop);
 */
static void p_ere(p, stop) struct parse *p;
int stop; /* character this ERE should end at */
{
  char c;
  sopno prevback = 0;
  sopno prevfwd = 0;
  sopno conc;
  int first = 1; /* is this the first alternative? */

  for (;;) {
    /* do a bunch of concatenated expressions */
    conc = HERE();
    while (MORE() && (c = PEEK()) != '|' && c != stop)
      p_ere_exp(p);
    (void)REQUIRE(HERE() != conc, REG_EMPTY); /* require nonempty */

    if (!EAT('|')) break; /* NOTE BREAK OUT */

    if (first) {
      INSERT(OCH_, conc); /* offset is wrong */
      prevfwd = conc;
      prevback = conc;
      first = 0;
    }
    ASTERN(OOR1, prevback);
    prevback = THERE();
    AHEAD(prevfwd); /* fix previous offset */
    prevfwd = HERE();
    EMIT(OOR2, 0); /* offset is very wrong */
  }

  if (!first) { /* tail-end fixups */
    AHEAD(prevfwd);
    ASTERN(O_CH, prevback);
  }

  assert(!MORE() || SEE(stop));
}

/*
 - p_ere_exp - parse one subERE, an atom possibly followed by a repetition op
 == static void p_ere_exp(struct parse *p);
 */
static void p_ere_exp(p) struct parse *p;
{
  char c;
  sopno pos;
  int count;
  int count2;
  sopno subno;
  int wascaret = 0;

  assert(MORE()); /* caller should have ensured this */
  c = GETNEXT();

  pos = HERE();
  switch (c) {
    case '(':
      (void)REQUIRE(MORE(), REG_EPAREN);
      p->g->nsub++;
      subno = p->g->nsub;
      if (subno < NPAREN) p->pbegin[subno] = HERE();
      EMIT(OLPAREN, subno);
      if (!SEE(')')) p_ere(p, ')');
      if (subno < NPAREN) {
        p->pend[subno] = HERE();
        assert(p->pend[subno] != 0);
      }
      EMIT(ORPAREN, subno);
      (void)MUSTEAT(')', REG_EPAREN);
      break;
#ifndef POSIX_MISTAKE
    case ')': /* happens only if no current unmatched ( */
      /*
       * You may ask, why the ifndef?  Because I didn't notice
       * this until slightly too late for 1003.2, and none of the
       * other 1003.2 regular-expression reviewers noticed it at
       * all.  So an unmatched ) is legal POSIX, at least until
       * we can get it fixed.
       */
      SETERROR(REG_EPAREN);
      break;
#endif
    case '^':
      EMIT(OBOL, 0);
      p->g->iflags |= USEBOL;
      p->g->nbol++;
      wascaret = 1;
      break;
    case '$':
      EMIT(OEOL, 0);
      p->g->iflags |= USEEOL;
      p->g->neol++;
      break;
    case '|':
      SETERROR(REG_EMPTY);
      break;
    case '*':
    case '+':
    case '?':
      SETERROR(REG_BADRPT);
      break;
    case '.':
      if (p->g->cflags & REG_NEWLINE)
        nonnewline(p);
      else
        EMIT(OANY, 0);
      break;
    case '[':
      p_bracket(p);
      break;
    case '\\':
      (void)REQUIRE(MORE(), REG_EESCAPE);
      c = GETNEXT();
      ordinary(p, c);
      break;
    case '{': /* okay as ordinary except if digit follows */
      (void)REQUIRE(!MORE() || !isdigit((uch)PEEK()), REG_BADRPT);
      /* FALLTHROUGH */
    default:
      ordinary(p, c);
      break;
  }

  if (!MORE()) return;
  c = PEEK();
  /* we call { a repetition if followed by a digit */
  if (!(c == '*' || c == '+' || c == '?' || (c == '{' && MORE2() && isdigit((uch)PEEK2()))))
    return; /* no repetition, we're done */
  NEXT();

  (void)REQUIRE(!wascaret, REG_BADRPT);
  switch (c) {
    case '*': /* implemented as +? */
      /* this case does not require the (y|) trick, noKLUDGE */
      INSERT(OPLUS_, pos);
      ASTERN(O_PLUS, pos);
      INSERT(OQUEST_, pos);
      ASTERN(O_QUEST, pos);
      break;
    case '+':
      INSERT(OPLUS_, pos);
      ASTERN(O_PLUS, pos);
      break;
    case '?':
      /* KLUDGE: emit y? as (y|) until subtle bug gets fixed */
      INSERT(OCH_, pos); /* offset slightly wrong */
      ASTERN(OOR1, pos); /* this one's right */
      AHEAD(pos);        /* fix the OCH_ */
      EMIT(OOR2, 0);     /* offset very wrong... */
      AHEAD(THERE());    /* ...so fix it */
      ASTERN(O_CH, THERETHERE());
      break;
    case '{':
      count = p_count(p);
      if (EAT(',')) {
        if (isdigit((uch)PEEK())) {
          count2 = p_count(p);
          (void)REQUIRE(count <= count2, REG_BADBR);
        } else /* single number with comma */
          count2 = INFINITY;
      } else /* just a single number */
        count2 = count;
      repeat(p, pos, count, count2);
      if (!EAT('}')) { /* error heuristics */
        while (MORE() && PEEK() != '}')
          NEXT();
        (void)REQUIRE(MORE(), REG_EBRACE);
        SETERROR(REG_BADBR);
      }
      break;
  }

  if (!MORE()) return;
  c = PEEK();
  if (!(c == '*' || c == '+' || c == '?' || (c == '{' && MORE2() && isdigit((uch)PEEK2())))) return;
  SETERROR(REG_BADRPT);
}

/*
 - p_str - string (no metacharacters) "parser"
 == static void p_str(struct parse *p);
 */
static void p_str(p) struct parse *p;
{
  (void)REQUIRE(MORE(), REG_EMPTY);
  while (MORE())
    ordinary(p, GETNEXT());
}

/*
 - p_bre - BRE parser top level, anchoring and concatenation
 == static void p_bre(struct parse *p, int end1, \
 ==	int end2);
 * Giving end1 as OUT essentially eliminates the end1/end2 check.
 *
 * This implementation is a bit of a kludge, in that a trailing $ is first
 * taken as an ordinary character and then revised to be an anchor.  The
 * only undesirable side effect is that '$' gets included as a character
 * category in such cases.  This is fairly harmless; not worth fixing.
 * The amount of lookahead needed to avoid this kludge is excessive.
 */
static void p_bre(p, end1, end2) struct parse *p;
int end1; /* first terminating character */
int end2; /* second terminating character */
{
  sopno start = HERE();
  int first = 1; /* first subexpression? */
  int wasdollar = 0;

  if (EAT('^')) {
    EMIT(OBOL, 0);
    p->g->iflags |= USEBOL;
    p->g->nbol++;
  }
  while (MORE() && !SEETWO(end1, end2)) {
    wasdollar = p_simp_re(p, first);
    first = 0;
  }
  if (wasdollar) { /* oops, that was a trailing anchor */
    DROP(1);
    EMIT(OEOL, 0);
    p->g->iflags |= USEEOL;
    p->g->neol++;
  }

  (void)REQUIRE(HERE() != start, REG_EMPTY); /* require nonempty */
}

/*
 - p_simp_re - parse a simple RE, an atom possibly followed by a repetition
 == static int p_simp_re(struct parse *p, int starordinary);
 */
static int /* was the simple RE an unbackslashed $? */
    p_simp_re(p, starordinary) struct parse *p;
int starordinary; /* is a leading * an ordinary character? */
{
  int c;
  int count;
  int count2;
  sopno pos;
  int i;
  sopno subno;
#define BACKSL (1 << CHAR_BIT)

  pos = HERE(); /* repetion op, if any, covers from here */

  assert(MORE()); /* caller should have ensured this */
  c = GETNEXT();
  if (c == '\\') {
    (void)REQUIRE(MORE(), REG_EESCAPE);
    c = BACKSL | GETNEXT();
  }
  switch (c) {
    case '.':
      if (p->g->cflags & REG_NEWLINE)
        nonnewline(p);
      else
        EMIT(OANY, 0);
      break;
    case '[':
      p_bracket(p);
      break;
    case BACKSL | '{':
      SETERROR(REG_BADRPT);
      break;
    case BACKSL | '(':
      p->g->nsub++;
      subno = p->g->nsub;
      if (subno < NPAREN) p->pbegin[subno] = HERE();
      EMIT(OLPAREN, subno);
      /* the MORE here is an error heuristic */
      if (MORE() && !SEETWO('\\', ')')) p_bre(p, '\\', ')');
      if (subno < NPAREN) {
        p->pend[subno] = HERE();
        assert(p->pend[subno] != 0);
      }
      EMIT(ORPAREN, subno);
      (void)REQUIRE(EATTWO('\\', ')'), REG_EPAREN);
      break;
    case BACKSL | ')': /* should not get here -- must be user */
    case BACKSL | '}':
      SETERROR(REG_EPAREN);
      break;
    case BACKSL | '1':
    case BACKSL | '2':
    case BACKSL | '3':
    case BACKSL | '4':
    case BACKSL | '5':
    case BACKSL | '6':
    case BACKSL | '7':
    case BACKSL | '8':
    case BACKSL | '9':
      i = (c & ~BACKSL) - '0';
      assert(i < NPAREN);
      if (p->pend[i] != 0) {
        assert(i <= p->g->nsub);
        EMIT(OBACK_, i);
        assert(p->pbegin[i] != 0);
        assert(OP(p->strip[p->pbegin[i]]) == OLPAREN);
        assert(OP(p->strip[p->pend[i]]) == ORPAREN);
        (void)dupl(p, p->pbegin[i] + 1, p->pend[i]);
        EMIT(O_BACK, i);
      } else
        SETERROR(REG_ESUBREG);
      p->g->backrefs = 1;
      break;
    case '*':
      (void)REQUIRE(starordinary, REG_BADRPT);
      /* FALLTHROUGH */
    default:
      ordinary(p, (char)c);
      break;
  }

  if (EAT('*')) { /* implemented as +? */
    /* this case does not require the (y|) trick, noKLUDGE */
    INSERT(OPLUS_, pos);
    ASTERN(O_PLUS, pos);
    INSERT(OQUEST_, pos);
    ASTERN(O_QUEST, pos);
  } else if (EATTWO('\\', '{')) {
    count = p_count(p);
    if (EAT(',')) {
      if (MORE() && isdigit((uch)PEEK())) {
        count2 = p_count(p);
        (void)REQUIRE(count <= count2, REG_BADBR);
      } else /* single number with comma */
        count2 = INFINITY;
    } else /* just a single number */
      count2 = count;
    repeat(p, pos, count, count2);
    if (!EATTWO('\\', '}')) { /* error heuristics */
      while (MORE() && !SEETWO('\\', '}'))
        NEXT();
      (void)REQUIRE(MORE(), REG_EBRACE);
      SETERROR(REG_BADBR);
    }
  } else if (c == '$') /* $ (but not \$) ends it */
    return (1);

  return (0);
}

/*
 - p_count - parse a repetition count
 == static int p_count(struct parse *p);
 */
static int /* the value */
    p_count(p) struct parse *p;
{
  int count = 0;
  int ndigits = 0;

  while (MORE() && isdigit((uch)PEEK()) && count <= DUPMAX) {
    count = count * 10 + (GETNEXT() - '0');
    ndigits++;
  }

  (void)REQUIRE(ndigits > 0 && count <= DUPMAX, REG_BADBR);
  return (count);
}

/*
 - p_bracket - parse a bracketed character list
 == static void p_bracket(struct parse *p);
 *
 * Note a significant property of this code:  if the allocset() did SETERROR,
 * no set operations are done.
 */
static void p_bracket(p) struct parse *p;
{
  cset *cs = allocset(p);
  int invert = 0;

  /* Dept of Truly Sickening Special-Case Kludges */
  if (p->next + 5 < p->end && strncmp(p->next, "[:<:]]", 6) == 0) {
    EMIT(OBOW, 0);
    NEXTn(6);
    return;
  }
  if (p->next + 5 < p->end && strncmp(p->next, "[:>:]]", 6) == 0) {
    EMIT(OEOW, 0);
    NEXTn(6);
    return;
  }

  if (EAT('^')) invert++; /* make note to invert set at end */
  if (EAT(']'))
    CHadd(cs, ']');
  else if (EAT('-'))
    CHadd(cs, '-');
  while (MORE() && PEEK() != ']' && !SEETWO('-', ']'))
    p_b_term(p, cs);
  if (EAT('-')) CHadd(cs, '-');
  (void)MUSTEAT(']', REG_EBRACK);

  if (p->error != 0) /* don't mess things up further */
    return;

  if (p->g->cflags & REG_ICASE) {
    int i;
    int ci;

    for (i = p->g->csetsize - 1; i >= 0; i--)
      if (CHIN(cs, i) && isalpha(i)) {
        ci = othercase(i);
        if (ci != i) CHadd(cs, ci);
      }
    if (cs->multis != NULL) mccase(p, cs);
  }
  if (invert) {
    int i;

    for (i = p->g->csetsize - 1; i >= 0; i--)
      if (CHIN(cs, i))
        CHsub(cs, i);
      else
        CHadd(cs, i);
    if (p->g->cflags & REG_NEWLINE) CHsub(cs, '\n');
    if (cs->multis != NULL) mcinvert(p, cs);
  }

  assert(cs->multis == NULL); /* xxx */

  if (nch(p, cs) == 1) { /* optimize singleton sets */
    ordinary(p, firstch(p, cs));
    freeset(p, cs);
  } else
    EMIT(OANYOF, freezeset(p, cs));
}

/*
 - p_b_term - parse one term of a bracketed character list
 == static void p_b_term(struct parse *p, cset *cs);
 */
static void p_b_term(p, cs) struct parse *p;
cset *cs;
{
  char c;
  char start, finish;
  int i;

  /* classify what we've got */
  switch ((MORE()) ? PEEK() : '\0') {
    case '[':
      c = (MORE2()) ? PEEK2() : '\0';
      break;
    case '-':
      SETERROR(REG_ERANGE);
      return; /* NOTE RETURN */
      break;
    default:
      c = '\0';
      break;
  }

  switch (c) {
    case ':': /* character class */
      NEXT2();
      (void)REQUIRE(MORE(), REG_EBRACK);
      c = PEEK();
      (void)REQUIRE(c != '-' && c != ']', REG_ECTYPE);
      p_b_cclass(p, cs);
      (void)REQUIRE(MORE(), REG_EBRACK);
      (void)REQUIRE(EATTWO(':', ']'), REG_ECTYPE);
      break;
    case '=': /* equivalence class */
      NEXT2();
      (void)REQUIRE(MORE(), REG_EBRACK);
      c = PEEK();
      (void)REQUIRE(c != '-' && c != ']', REG_ECOLLATE);
      p_b_eclass(p, cs);
      (void)REQUIRE(MORE(), REG_EBRACK);
      (void)REQUIRE(EATTWO('=', ']'), REG_ECOLLATE);
      break;
    default: /* symbol, ordinary character, or range */
             /* xxx revision needed for multichar stuff */
      start = p_b_symbol(p);
      if (SEE('-') && MORE2() && PEEK2() != ']') {
        /* range */
        NEXT();
        if (EAT('-'))
          finish = '-';
        else
          finish = p_b_symbol(p);
      } else
        finish = start;
      if (start == finish)
        CHadd(cs, start);
      else {
        (void)REQUIRE((uch)start <= (uch)finish, REG_ERANGE);
        for (i = (uch)start; i <= (uch)finish; i++)
          CHadd(cs, i);
      }
      break;
  }
}

/*
 - p_b_cclass - parse a character-class name and deal with it
 == static void p_b_cclass(struct parse *p, cset *cs);
 */
static void p_b_cclass(p, cs) struct parse *p;
cset *cs;
{
  int c;
  char *sp = p->next;
  struct cclass *cp;
  size_t len;

  while (MORE() && isalpha((uch)PEEK()))
    NEXT();
  len = p->next - sp;
  for (cp = cclasses; cp->name != NULL; cp++)
    if (strncmp(cp->name, sp, len) == 0 && cp->name[len] == '\0') break;
  if (cp->name == NULL) {
    /* oops, didn't find it */
    SETERROR(REG_ECTYPE);
    return;
  }

  switch (cp->fidx) {
    case CALNUM:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (isalnum((uch)c)) CHadd(cs, c);
      break;
    case CALPHA:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (isalpha((uch)c)) CHadd(cs, c);
      break;
    case CBLANK:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (isblank((uch)c)) CHadd(cs, c);
      break;
    case CCNTRL:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (iscntrl((uch)c)) CHadd(cs, c);
      break;
    case CDIGIT:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (isdigit((uch)c)) CHadd(cs, c);
      break;
    case CGRAPH:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (isgraph((uch)c)) CHadd(cs, c);
      break;
    case CLOWER:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (islower((uch)c)) CHadd(cs, c);
      break;
    case CPRINT:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (isprint((uch)c)) CHadd(cs, c);
      break;
    case CPUNCT:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (ispunct((uch)c)) CHadd(cs, c);
      break;
    case CSPACE:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (isspace((uch)c)) CHadd(cs, c);
      break;
    case CUPPER:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (isupper((uch)c)) CHadd(cs, c);
      break;
    case CXDIGIT:
      for (c = CHAR_MIN; c <= CHAR_MAX; c++)
        if (isxdigit((uch)c)) CHadd(cs, c);
      break;
  }
#if 0
	for (u = cp->multis; *u != '\0'; u += strlen(u) + 1)
		MCadd(p, cs, u);
#endif
}

/*
 - p_b_eclass - parse an equivalence-class name and deal with it
 == static void p_b_eclass(struct parse *p, cset *cs);
 *
 * This implementation is incomplete. xxx
 */
static void p_b_eclass(p, cs) struct parse *p;
cset *cs;
{
  char c;

  c = p_b_coll_elem(p, '=');
  CHadd(cs, c);
}

/*
 - p_b_symbol - parse a character or [..]ed multicharacter collating symbol
 == static char p_b_symbol(struct parse *p);
 */
static char /* value of symbol */
    p_b_symbol(p) struct parse *p;
{
  char value;

  (void)REQUIRE(MORE(), REG_EBRACK);
  if (!EATTWO('[', '.')) return (GETNEXT());

  /* collating symbol */
  value = p_b_coll_elem(p, '.');
  (void)REQUIRE(EATTWO('.', ']'), REG_ECOLLATE);
  return (value);
}

/*
 - p_b_coll_elem - parse a collating-element name and look it up
 == static char p_b_coll_elem(struct parse *p, int endc);
 */
static char /* value of collating element */
    p_b_coll_elem(p, endc) struct parse *p;
int endc; /* name ended by endc,']' */
{
  char *sp = p->next;
  struct cname *cp;
  int len;

  while (MORE() && !SEETWO(endc, ']'))
    NEXT();
  if (!MORE()) {
    SETERROR(REG_EBRACK);
    return (0);
  }
  len = p->next - sp;
  for (cp = cnames; cp->name != NULL; cp++)
    if (strncmp(cp->name, sp, len) == 0 && cp->name[len] == '\0')
      return (cp->code);      /* known name */
  if (len == 1) return (*sp); /* single character */
  SETERROR(REG_ECOLLATE);     /* neither */
  return (0);
}

/*
 - othercase - return the case counterpart of an alphabetic
 == static char othercase(int ch);
 */
static char /* if no counterpart, return ch */
    othercase(ch) int ch;
{
  ch = (uch)ch;
  assert(isalpha(ch));
  if (isupper(ch))
    return (tolower(ch));
  else if (islower(ch))
    return (toupper(ch));
  else /* peculiar, but could happen */
    return (ch);
}

/*
 - bothcases - emit a dualcase version of a two-case character
 == static void bothcases(struct parse *p, int ch);
 *
 * Boy, is this implementation ever a kludge...
 */
static void bothcases(p, ch) struct parse *p;
int ch;
{
  char *oldnext = p->next;
  char *oldend = p->end;
  char bracket[3];

  ch = (uch)ch;
  assert(othercase(ch) != ch); /* p_bracket() would recurse */
  p->next = bracket;
  p->end = bracket + 2;
  bracket[0] = ch;
  bracket[1] = ']';
  bracket[2] = '\0';
  p_bracket(p);
  assert(p->next == bracket + 2);
  p->next = oldnext;
  p->end = oldend;
}

/*
 - ordinary - emit an ordinary character
 == static void ordinary(struct parse *p, int ch);
 */
static void ordinary(p, ch) struct parse *p;
int ch;
{
  cat_t *cap = p->g->categories;

  if ((p->g->cflags & REG_ICASE) && isalpha((uch)ch) && othercase(ch) != ch)
    bothcases(p, ch);
  else {
    EMIT(OCHAR, (uch)ch);
    if (cap[ch] == 0) cap[ch] = p->g->ncategories++;
  }
}

/*
 - nonnewline - emit REG_NEWLINE version of OANY
 == static void nonnewline(struct parse *p);
 *
 * Boy, is this implementation ever a kludge...
 */
static void nonnewline(p) struct parse *p;
{
  char *oldnext = p->next;
  char *oldend = p->end;
  char bracket[4];

  p->next = bracket;
  p->end = bracket + 3;
  bracket[0] = '^';
  bracket[1] = '\n';
  bracket[2] = ']';
  bracket[3] = '\0';
  p_bracket(p);
  assert(p->next == bracket + 3);
  p->next = oldnext;
  p->end = oldend;
}

/*
 - repeat - generate code for a bounded repetition, recursively if needed
 == static void repeat(struct parse *p, sopno start, int from, int to);
 */
static void repeat(p, start, from, to) struct parse *p;
sopno start; /* operand from here to end of strip */
int from;    /* repeated from this number */
int to;      /* to this number of times (maybe INFINITY) */
{
  sopno finish = HERE();
#define N 2
#define INF 3
#define REP(f, t) ((f)*8 + (t))
#define MAP(n) (((n) <= 1) ? (n) : ((n) == INFINITY) ? INF : N)
  sopno copy;

  if (p->error != 0) /* head off possible runaway recursion */
    return;

  assert(from <= to);

  switch (REP(MAP(from), MAP(to))) {
    case REP(0, 0):         /* must be user doing this */
      DROP(finish - start); /* drop the operand */
      break;
    case REP(0, 1):   /* as x{1,1}? */
    case REP(0, N):   /* as x{1,n}? */
    case REP(0, INF): /* as x{1,}? */
      /* KLUDGE: emit y? as (y|) until subtle bug gets fixed */
      INSERT(OCH_, start); /* offset is wrong... */
      repeat(p, start + 1, 1, to);
      ASTERN(OOR1, start);
      AHEAD(start); /* ... fix it */
      EMIT(OOR2, 0);
      AHEAD(THERE());
      ASTERN(O_CH, THERETHERE());
      break;
    case REP(1, 1): /* trivial case */
      /* done */
      break;
    case REP(1, N): /* as x?x{1,n-1} */
      /* KLUDGE: emit y? as (y|) until subtle bug gets fixed */
      INSERT(OCH_, start);
      ASTERN(OOR1, start);
      AHEAD(start);
      EMIT(OOR2, 0);  /* offset very wrong... */
      AHEAD(THERE()); /* ...so fix it */
      ASTERN(O_CH, THERETHERE());
      copy = dupl(p, start + 1, finish + 1);
      assert(copy == finish + 4);
      repeat(p, copy, 1, to - 1);
      break;
    case REP(1, INF): /* as x+ */
      INSERT(OPLUS_, start);
      ASTERN(O_PLUS, start);
      break;
    case REP(N, N): /* as xx{m-1,n-1} */
      copy = dupl(p, start, finish);
      repeat(p, copy, from - 1, to - 1);
      break;
    case REP(N, INF): /* as xx{n-1,INF} */
      copy = dupl(p, start, finish);
      repeat(p, copy, from - 1, to);
      break;
    default:                /* "can't happen" */
      SETERROR(REG_ASSERT); /* just in case */
      break;
  }
}

/*
 - seterr - set an error condition
 == static int seterr(struct parse *p, int e);
 */
static int /* useless but makes type checking happy */
    seterr(p, e) struct parse *p;
int e;
{
  if (p->error == 0) /* keep earliest error condition */
    p->error = e;
  p->next = nuls; /* try to bring things to a halt */
  p->end = nuls;
  return (0); /* make the return value well-defined */
}

/*
 - allocset - allocate a set of characters for []
 == static cset *allocset(struct parse *p);
 */
static cset *allocset(p) struct parse *p;
{
  int no = p->g->ncsets++;
  size_t nc;
  size_t nbytes;
  cset *cs;
  size_t css = (size_t)p->g->csetsize;
  int i;

  if (no >= p->ncsalloc) { /* need another column of space */
    p->ncsalloc += CHAR_BIT;
    nc = p->ncsalloc;
    assert(nc % CHAR_BIT == 0);
    nbytes = nc / CHAR_BIT * css;
    if (p->g->sets == NULL)
      p->g->sets = (cset *)malloc(nc * sizeof(cset));
    else
      p->g->sets = (cset *)realloc((char *)p->g->sets, nc * sizeof(cset));
    if (p->g->setbits == NULL)
      p->g->setbits = (uch *)malloc(nbytes);
    else {
      p->g->setbits = (uch *)realloc((char *)p->g->setbits, nbytes);
      /* xxx this isn't right if setbits is now NULL */
      for (i = 0; i < no; i++)
        p->g->sets[i].ptr = p->g->setbits + css * (i / CHAR_BIT);
    }
    if (p->g->sets != NULL && p->g->setbits != NULL)
      (void)memset((char *)p->g->setbits + (nbytes - css), 0, css);
    else {
      no = 0;
      SETERROR(REG_ESPACE);
      /* caller's responsibility not to do set ops */
    }
  }

  assert(p->g->sets != NULL); /* xxx */
  cs = &p->g->sets[no];
  cs->ptr = p->g->setbits + css * ((no) / CHAR_BIT);
  cs->mask = 1 << ((no) % CHAR_BIT);
  cs->hash = 0;
  cs->smultis = 0;
  cs->multis = NULL;

  return (cs);
}

/*
 - freeset - free a now-unused set
 == static void freeset(struct parse *p, cset *cs);
 */
static void freeset(p, cs) struct parse *p;
cset *cs;
{
  int i;
  cset *top = &p->g->sets[p->g->ncsets];
  size_t css = (size_t)p->g->csetsize;

  for (i = 0; i < css; i++)
    CHsub(cs, i);
  if (cs == top - 1) /* recover only the easy case */
    p->g->ncsets--;
}

/*
 - freezeset - final processing on a set of characters
 == static int freezeset(struct parse *p, cset *cs);
 *
 * The main task here is merging identical sets.  This is usually a waste
 * of time (although the hash code minimizes the overhead), but can win
 * big if REG_ICASE is being used.  REG_ICASE, by the way, is why the hash
 * is done using addition rather than xor -- all ASCII [aA] sets xor to
 * the same value!
 */
static int /* set number */
    freezeset(p, cs) struct parse *p;
cset *cs;
{
  short h = cs->hash;
  int i;
  cset *top = &p->g->sets[p->g->ncsets];
  cset *cs2;
  size_t css = (size_t)p->g->csetsize;

  /* look for an earlier one which is the same */
  for (cs2 = &p->g->sets[0]; cs2 < top; cs2++)
    if (cs2->hash == h && cs2 != cs) {
      /* maybe */
      for (i = 0; i < css; i++)
        if (!!CHIN(cs2, i) != !!CHIN(cs, i)) break; /* no */
      if (i == css) break;                          /* yes */
    }

  if (cs2 < top) { /* found one */
    freeset(p, cs);
    cs = cs2;
  }

  return ((int)(cs - p->g->sets));
}

/*
 - firstch - return first character in a set (which must have at least one)
 == static int firstch(struct parse *p, cset *cs);
 */
static int /* character; there is no "none" value */
    firstch(p, cs) struct parse *p;
cset *cs;
{
  int i;
  size_t css = (size_t)p->g->csetsize;

  for (i = 0; i < css; i++)
    if (CHIN(cs, i)) return ((char)i);
  assert(never);
  return (0); /* arbitrary */
}

/*
 - nch - number of characters in a set
 == static int nch(struct parse *p, cset *cs);
 */
static int nch(p, cs) struct parse *p;
cset *cs;
{
  int i;
  size_t css = (size_t)p->g->csetsize;
  int n = 0;

  for (i = 0; i < css; i++)
    if (CHIN(cs, i)) n++;
  return (n);
}

#if used
/*
 - mcadd - add a collating element to a cset
 == static void mcadd(struct parse *p, cset *cs, \
 ==	char *cp);
 */
static void mcadd(p, cs, cp) struct parse *p;
cset *cs;
char *cp;
{
  size_t oldend = cs->smultis;

  cs->smultis += strlen(cp) + 1;
  if (cs->multis == NULL)
    cs->multis = malloc(cs->smultis);
  else
    cs->multis = reallocf(cs->multis, cs->smultis);
  if (cs->multis == NULL) {
    SETERROR(REG_ESPACE);
    return;
  }

  (void)strcpy(cs->multis + oldend - 1, cp);
  cs->multis[cs->smultis - 1] = '\0';
}

/*
 - mcsub - subtract a collating element from a cset
 == static void mcsub(cset *cs, char *cp);
 */
static void mcsub(cs, cp) cset *cs;
char *cp;
{
  char *fp = mcfind(cs, cp);
  size_t len = strlen(fp);

  assert(fp != NULL);
  (void)memmove(fp, fp + len + 1, cs->smultis - (fp + len + 1 - cs->multis));
  cs->smultis -= len;

  if (cs->smultis == 0) {
    free(cs->multis);
    cs->multis = NULL;
    return;
  }

  cs->multis = reallocf(cs->multis, cs->smultis);
  assert(cs->multis != NULL);
}

/*
 - mcin - is a collating element in a cset?
 == static int mcin(cset *cs, char *cp);
 */
static int mcin(cs, cp) cset *cs;
char *cp;
{ return (mcfind(cs, cp) != NULL); }

/*
 - mcfind - find a collating element in a cset
 == static char *mcfind(cset *cs, char *cp);
 */
static char *mcfind(cs, cp) cset *cs;
char *cp;
{
  char *p;

  if (cs->multis == NULL) return (NULL);
  for (p = cs->multis; *p != '\0'; p += strlen(p) + 1)
    if (strcmp(cp, p) == 0) return (p);
  return (NULL);
}
#endif

/*
 - mcinvert - invert the list of collating elements in a cset
 == static void mcinvert(struct parse *p, cset *cs);
 *
 * This would have to know the set of possibilities.  Implementation
 * is deferred.
 */
static void mcinvert(p, cs) struct parse *p;
cset *cs;
{ assert(cs->multis == NULL); /* xxx */ }

/*
 - mccase - add case counterparts of the list of collating elements in a cset
 == static void mccase(struct parse *p, cset *cs);
 *
 * This would have to know the set of possibilities.  Implementation
 * is deferred.
 */
static void mccase(p, cs) struct parse *p;
cset *cs;
{ assert(cs->multis == NULL); /* xxx */ }

/*
 - isinsets - is this character in any sets?
 == static int isinsets(struct re_guts *g, int c);
 */
static int /* predicate */
    isinsets(g, c) struct re_guts *g;
int c;
{
  uch *col;
  int i;
  int ncols = (g->ncsets + (CHAR_BIT - 1)) / CHAR_BIT;
  unsigned uc = (uch)c;

  for (i = 0, col = g->setbits; i < ncols; i++, col += g->csetsize)
    if (col[uc] != 0) return (1);
  return (0);
}

/*
 - samesets - are these two characters in exactly the same sets?
 == static int samesets(struct re_guts *g, int c1, int c2);
 */
static int /* predicate */
    samesets(g, c1, c2) struct re_guts *g;
int c1;
int c2;
{
  uch *col;
  int i;
  int ncols = (g->ncsets + (CHAR_BIT - 1)) / CHAR_BIT;
  unsigned uc1 = (uch)c1;
  unsigned uc2 = (uch)c2;

  for (i = 0, col = g->setbits; i < ncols; i++, col += g->csetsize)
    if (col[uc1] != col[uc2]) return (0);
  return (1);
}

/*
 - categorize - sort out character categories
 == static void categorize(struct parse *p, struct re_guts *g);
 */
static void categorize(p, g) struct parse *p;
struct re_guts *g;
{
  cat_t *cats = g->categories;
  int c;
  int c2;
  cat_t cat;

  /* avoid making error situations worse */
  if (p->error != 0) return;

  for (c = CHAR_MIN; c <= CHAR_MAX; c++)
    if (cats[c] == 0 && isinsets(g, c)) {
      cat = g->ncategories++;
      cats[c] = cat;
      for (c2 = c + 1; c2 <= CHAR_MAX; c2++)
        if (cats[c2] == 0 && samesets(g, c, c2)) cats[c2] = cat;
    }
}

/*
 - dupl - emit a duplicate of a bunch of sops
 == static sopno dupl(struct parse *p, sopno start, sopno finish);
 */
static sopno /* start of duplicate */
    dupl(p, start, finish) struct parse *p;
sopno start;  /* from here */
sopno finish; /* to this less one */
{
  sopno ret = HERE();
  sopno len = finish - start;

  assert(finish >= start);
  if (len == 0) return (ret);
  enlarge(p, p->ssize + len); /* this many unexpected additions */
  assert(p->ssize >= p->slen + len);
  (void)memcpy((char *)(p->strip + p->slen), (char *)(p->strip + start), (size_t)len * sizeof(sop));
  p->slen += len;
  return (ret);
}

/*
 - doemit - emit a strip operator
 == static void doemit(struct parse *p, sop op, size_t opnd);
 *
 * It might seem better to implement this as a macro with a function as
 * hard-case backup, but it's just too big and messy unless there are
 * some changes to the data structures.  Maybe later.
 */
static void doemit(p, op, opnd) struct parse *p;
sop op;
size_t opnd;
{
  /* avoid making error situations worse */
  if (p->error != 0) return;

  /* deal with oversize operands ("can't happen", more or less) */
  assert(opnd < 1 << OPSHIFT);

  /* deal with undersized strip */
  if (p->slen >= p->ssize) enlarge(p, (p->ssize + 1) / 2 * 3); /* +50% */
  assert(p->slen < p->ssize);

  /* finally, it's all reduced to the easy case */
  p->strip[p->slen++] = SOP(op, opnd);
}

/*
 - doinsert - insert a sop into the strip
 == static void doinsert(struct parse *p, sop op, size_t opnd, sopno pos);
 */
static void doinsert(p, op, opnd, pos) struct parse *p;
sop op;
size_t opnd;
sopno pos;
{
  sopno sn;
  sop s;
  int i;

  /* avoid making error situations worse */
  if (p->error != 0) return;

  sn = HERE();
  EMIT(op, opnd); /* do checks, ensure space */
  assert(HERE() == sn + 1);
  s = p->strip[sn];

  /* adjust paren pointers */
  assert(pos > 0);
  for (i = 1; i < NPAREN; i++) {
    if (p->pbegin[i] >= pos) {
      p->pbegin[i]++;
    }
    if (p->pend[i] >= pos) {
      p->pend[i]++;
    }
  }

  memmove((char *)&p->strip[pos + 1], (char *)&p->strip[pos], (HERE() - pos - 1) * sizeof(sop));
  p->strip[pos] = s;
}

/*
 - dofwd - complete a forward reference
 == static void dofwd(struct parse *p, sopno pos, sop value);
 */
static void dofwd(p, pos, value) struct parse *p;
sopno pos;
sop value;
{
  /* avoid making error situations worse */
  if (p->error != 0) return;

  assert(value < 1 << OPSHIFT);
  p->strip[pos] = OP(p->strip[pos]) | value;
}

/*
 - enlarge - enlarge the strip
 == static void enlarge(struct parse *p, sopno size);
 */
static void enlarge(p, size) struct parse *p;
sopno size;
{
  sop *sp;

  if (p->ssize >= size) return;

  sp = (sop *)realloc(p->strip, size * sizeof(sop));
  if (sp == NULL) {
    SETERROR(REG_ESPACE);
    return;
  }
  p->strip = sp;
  p->ssize = size;
}

/*
 - stripsnug - compact the strip
 == static void stripsnug(struct parse *p, struct re_guts *g);
 */
static void stripsnug(p, g) struct parse *p;
struct re_guts *g;
{
  g->nstates = p->slen;
  g->strip = (sop *)realloc((char *)p->strip, p->slen * sizeof(sop));
  if (g->strip == NULL) {
    SETERROR(REG_ESPACE);
    g->strip = p->strip;
  }
}

/*
 - findmust - fill in must and mlen with longest mandatory literal string
 == static void findmust(struct parse *p, struct re_guts *g);
 *
 * This algorithm could do fancy things like analyzing the operands of |
 * for common subsequences.  Someday.  This code is simple and finds most
 * of the interesting cases.
 *
 * Note that must and mlen got initialized during setup.
 */
static void findmust(p, g) struct parse *p;
struct re_guts *g;
{
  sop *scan;
  sop *start = NULL;
  sop *newstart = NULL;
  sopno newlen;
  sop s;
  char *cp;
  sopno i;
  int offset;
  int cs, mccs;

  /* avoid making error situations worse */
  if (p->error != 0) return;

  /* Find out if we can handle OANYOF or not */
  mccs = 0;
  for (cs = 0; cs < g->ncsets; cs++)
    if (g->sets[cs].multis != NULL) mccs = 1;

  /* find the longest OCHAR sequence in strip */
  newlen = 0;
  offset = 0;
  g->moffset = 0;
  scan = g->strip + 1;
  do {
    s = *scan++;
    switch (OP(s)) {
      case OCHAR:        /* sequence member */
        if (newlen == 0) /* new sequence */
          newstart = scan - 1;
        newlen++;
        break;
      case OPLUS_: /* things that don't break one */
      case OLPAREN:
      case ORPAREN:
        break;
      case OQUEST_: /* things that must be skipped */
      case OCH_:
        offset = altoffset(scan, offset, mccs);
        scan--;
        do {
          scan += OPND(s);
          s = *scan;
          /* assert() interferes w debug printouts */
          if (OP(s) != O_QUEST && OP(s) != O_CH && OP(s) != OOR2) {
            g->iflags |= BAD;
            return;
          }
        } while (OP(s) != O_QUEST && OP(s) != O_CH);
        /* fallthrough */
      case OBOW: /* things that break a sequence */
      case OEOW:
      case OBOL:
      case OEOL:
      case O_QUEST:
      case O_CH:
      case OEND:
        if (newlen > g->mlen) { /* ends one */
          start = newstart;
          g->mlen = newlen;
          if (offset > -1) {
            g->moffset += offset;
            offset = newlen;
          } else
            g->moffset = offset;
        } else {
          if (offset > -1) offset += newlen;
        }
        newlen = 0;
        break;
      case OANY:
        if (newlen > g->mlen) { /* ends one */
          start = newstart;
          g->mlen = newlen;
          if (offset > -1) {
            g->moffset += offset;
            offset = newlen;
          } else
            g->moffset = offset;
        } else {
          if (offset > -1) offset += newlen;
        }
        if (offset > -1) offset++;
        newlen = 0;
        break;
      case OANYOF: /* may or may not invalidate offset */
        /* First, everything as OANY */
        if (newlen > g->mlen) { /* ends one */
          start = newstart;
          g->mlen = newlen;
          if (offset > -1) {
            g->moffset += offset;
            offset = newlen;
          } else
            g->moffset = offset;
        } else {
          if (offset > -1) offset += newlen;
        }
        if (offset > -1) offset++;
        newlen = 0;
        /* And, now, if we found out we can't deal with
         * it, make offset = -1.
         */
        if (mccs) offset = -1;
        break;
      default:
        /* Anything here makes it impossible or too hard
         * to calculate the offset -- so we give up;
         * save the last known good offset, in case the
         * must sequence doesn't occur later.
         */
        if (newlen > g->mlen) { /* ends one */
          start = newstart;
          g->mlen = newlen;
          if (offset > -1)
            g->moffset += offset;
          else
            g->moffset = offset;
        }
        offset = -1;
        newlen = 0;
        break;
    }
  } while (OP(s) != OEND);

  if (g->mlen == 0) { /* there isn't one */
    g->moffset = -1;
    return;
  }

  /* turn it into a character string */
  g->must = malloc((size_t)g->mlen + 1);
  if (g->must == NULL) { /* argh; just forget it */
    g->mlen = 0;
    g->moffset = -1;
    return;
  }
  cp = g->must;
  scan = start;
  for (i = g->mlen; i > 0; i--) {
    while (OP(s = *scan++) != OCHAR)
      continue;
    assert(cp < g->must + g->mlen);
    *cp++ = (char)OPND(s);
  }
  assert(cp == g->must + g->mlen);
  *cp++ = '\0'; /* just on general principles */
}

/*
 - altoffset - choose biggest offset among multiple choices
 == static int altoffset(sop *scan, int offset, int mccs);
 *
 * Compute, recursively if necessary, the largest offset among multiple
 * re paths.
 */
static int altoffset(scan, offset, mccs) sop *scan;
int offset;
int mccs;
{
  int largest;
  int try;
  sop s;

  /* If we gave up already on offsets, return */
  if (offset == -1) return -1;

  largest = 0;
  try = 0;
  s = *scan++;
  while (OP(s) != O_QUEST && OP(s) != O_CH) {
    switch (OP(s)) {
      case OOR1:
        if (try > largest) largest = try;
        try = 0;
        break;
      case OQUEST_:
      case OCH_:
        try = altoffset(scan, try, mccs);
        if (try == -1) return -1;
        scan--;
        do {
          scan += OPND(s);
          s = *scan;
          if (OP(s) != O_QUEST && OP(s) != O_CH && OP(s) != OOR2) return -1;
        } while (OP(s) != O_QUEST && OP(s) != O_CH);
        /* We must skip to the next position, or we'll
         * leave altoffset() too early.
         */
        scan++;
        break;
      case OANYOF:
        if (mccs) return -1;
      case OCHAR:
      case OANY:
        try++;
      case OBOW:
      case OEOW:
      case OLPAREN:
      case ORPAREN:
      case OOR2:
        break;
      default:
        try = -1;
        break;
    }
    if (try == -1) return -1;
    s = *scan++;
  }

  if (try > largest) largest = try;

  return largest + offset;
}

/*
 - computejumps - compute char jumps for BM scan
 == static void computejumps(struct parse *p, struct re_guts *g);
 *
 * This algorithm assumes g->must exists and is has size greater than
 * zero. It's based on the algorithm found on Computer Algorithms by
 * Sara Baase.
 *
 * A char jump is the number of characters one needs to jump based on
 * the value of the character from the text that was mismatched.
 */
static void computejumps(p, g) struct parse *p;
struct re_guts *g;
{
  int ch;
  int mindex;

  /* Avoid making errors worse */
  if (p->error != 0) return;

  g->charjump = (int *)malloc((NC + 1) * sizeof(int));
  if (g->charjump == NULL) /* Not a fatal error */
    return;
  /* Adjust for signed chars, if necessary */
  g->charjump = &g->charjump[-(CHAR_MIN)];

  /* If the character does not exist in the pattern, the jump
   * is equal to the number of characters in the pattern.
   */
  for (ch = CHAR_MIN; ch < (CHAR_MAX + 1); ch++)
    g->charjump[ch] = g->mlen;

  /* If the character does exist, compute the jump that would
   * take us to the last character in the pattern equal to it
   * (notice that we match right to left, so that last character
   * is the first one that would be matched).
   */
  for (mindex = 0; mindex < g->mlen; mindex++)
    g->charjump[(unsigned char)g->must[mindex]] = g->mlen - mindex - 1;
}

/*
 - computematchjumps - compute match jumps for BM scan
 == static void computematchjumps(struct parse *p, struct re_guts *g);
 *
 * This algorithm assumes g->must exists and is has size greater than
 * zero. It's based on the algorithm found on Computer Algorithms by
 * Sara Baase.
 *
 * A match jump is the number of characters one needs to advance based
 * on the already-matched suffix.
 * Notice that all values here are minus (g->mlen-1), because of the way
 * the search algorithm works.
 */
static void computematchjumps(p, g) struct parse *p;
struct re_guts *g;
{
  int mindex;    /* General "must" iterator */
  int suffix;    /* Keeps track of matching suffix */
  int ssuffix;   /* Keeps track of suffixes' suffix */
  int *pmatches; /* pmatches[k] points to the next i
                  * such that i+1...mlen is a substring
                  * of k+1...k+mlen-i-1
                  */

  /* Avoid making errors worse */
  if (p->error != 0) return;

  pmatches = (int *)malloc(g->mlen * sizeof(unsigned int));
  if (pmatches == NULL) {
    g->matchjump = NULL;
    return;
  }

  g->matchjump = (int *)malloc(g->mlen * sizeof(unsigned int));
  if (g->matchjump == NULL) /* Not a fatal error */
    return;

  /* Set maximum possible jump for each character in the pattern */
  for (mindex = 0; mindex < g->mlen; mindex++)
    g->matchjump[mindex] = 2 * g->mlen - mindex - 1;

  /* Compute pmatches[] */
  for (mindex = g->mlen - 1, suffix = g->mlen; mindex >= 0; mindex--, suffix--) {
    pmatches[mindex] = suffix;

    /* If a mismatch is found, interrupting the substring,
     * compute the matchjump for that position. If no
     * mismatch is found, then a text substring mismatched
     * against the suffix will also mismatch against the
     * substring.
     */
    while (suffix < g->mlen && g->must[mindex] != g->must[suffix]) {
      g->matchjump[suffix] = MIN(g->matchjump[suffix], g->mlen - mindex - 1);
      suffix = pmatches[suffix];
    }
  }

  /* Compute the matchjump up to the last substring found to jump
   * to the beginning of the largest must pattern prefix matching
   * it's own suffix.
   */
  for (mindex = 0; mindex <= suffix; mindex++)
    g->matchjump[mindex] = MIN(g->matchjump[mindex], g->mlen + suffix - mindex);

  ssuffix = pmatches[suffix];
  while (suffix < g->mlen) {
    while (suffix <= ssuffix && suffix < g->mlen) {
      g->matchjump[suffix] = MIN(g->matchjump[suffix], g->mlen + ssuffix - suffix);
      suffix++;
    }
    if (suffix < g->mlen) ssuffix = pmatches[ssuffix];
  }

  free(pmatches);
}

/*
 - pluscount - count + nesting
 == static sopno pluscount(struct parse *p, struct re_guts *g);
 */
static sopno /* nesting depth */
    pluscount(p, g) struct parse *p;
struct re_guts *g;
{
  sop *scan;
  sop s;
  sopno plusnest = 0;
  sopno maxnest = 0;

  if (p->error != 0) return (0); /* there may not be an OEND */

  scan = g->strip + 1;
  do {
    s = *scan++;
    switch (OP(s)) {
      case OPLUS_:
        plusnest++;
        break;
      case O_PLUS:
        if (plusnest > maxnest) maxnest = plusnest;
        plusnest--;
        break;
    }
  } while (OP(s) != OEND);
  if (plusnest != 0) g->iflags |= BAD;
  return (maxnest);
}

#endif /* !_NO_REGEX  */
