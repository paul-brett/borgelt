/*----------------------------------------------------------------------
  File    : attset3.c
  Contents: attribute set management, description and parser functions
  Author  : Christian Borgelt
  History : 1995.10.26 file created
            1996.02.23 function as_desc() added (readable description)
            1998.03.10 attribute weights added to domain description
            1998.05.31 adapted to scanner changes
            1998.09.01 several assertions added
            1998.09.06 second major redesign completed
            1999.02.04 long int changed to int (assume 32 bit system)
            2000.11.22 functions scn_format() and scn_fmtlen() exported
            2001.06.23 module split into two files (optional functions)
            2001.07.15 parser adapted to modified module scan
            2002.01.22 parser functions moved to a separate file
            2002.06.18 check for valid intervals added to as_desc()
            2004.08.12 error report for empty attribute set added
            2007.02.13 adapted to changed type identifiers
            2007.02.17 directions added, weight parsing modified
            2010.12.30 function as_vdesc() with va_list argument added
            2011.01.04 adapted to redesigned scanner module
            2011.02.09 parameter 'mand' added to function as_parse()
            2013.07.18 adapted to definitions ATTID, VALID, DTINT, DTFLT
            2015.08.02 bug in as_parse() fixed (comma before weight)
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "attset.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#ifdef AS_PARSE
/* --- error messages --- */
/* error codes 0 to -15 defined in scanner.h */
#define E_ATTEXP    (-16)       /* attribute expected */
#define E_DUPATT    (-17)       /* duplicate attribute */
#define E_VALEXP    (-18)       /* attribute value expected */
#define E_DUPVAL    (-19)       /* duplicate attribute value */
#define E_DOMAIN    (-20)       /* invalid attribute domain */
#endif  /* #ifdef AS_PARSE */

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
#ifdef AS_PARSE
static const char *msgs[] = {   /* error messages */
  /*      0 to  -7 */  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /*     -8 to -15 */  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  /* E_ATTEXP  -16 */  "#attribute expected instead of '%s'",
  /* E_DUPATT  -17 */  "#duplicate attribute '%s'",
  /* E_VALEXP  -18 */  "#attribute value expected instead of '%s'",
  /* E_DUPVAL  -19 */  "#duplicate attribute value '%s'",
  /* E_DOMAIN  -20 */  "#invalid attribute domain '%s'",
};
#endif  /* #ifdef AS_PARSE */

#ifdef AS_DESC
static const char *dirs[] = {   /* names of attribute directions */
  "none", "id", "in", NULL, "out" };
#endif  /* #ifdef AS_DESC */

/*----------------------------------------------------------------------
  Description Functions
----------------------------------------------------------------------*/
#ifdef AS_DESC

int as_desc (ATTSET *set, FILE *file, int mode, int maxlen, ...)
{                               /* --- describe an attribute set */
  int     r;                    /* buffer for result */
  va_list args;                 /* list of variable arguments */

  va_start(args, maxlen);       /* start variable argument evaluation */
  r = as_vdesc(set, file, mode, maxlen, &args);/* describe attributes */
  va_end(args);                 /* end variable argument evaluation */
  return r;                     /* return the description result */
}  /* as_desc() */

/*--------------------------------------------------------------------*/

int as_vdesc (ATTSET *set, FILE *file, int mode, int maxlen,
              va_list *args)
{                               /* --- describe an attribute set */
  int   i;                      /* loop variable */
  VALID m;                      /* loop variable */
  ATTID k;                      /* loop variable */
  ATTID off, cnt;               /* range of attributes */
  ATT   *att;                   /* to traverse attributes */
  int   len;                    /* length of value name */
  int   pos;                    /* position in output line */
  char  buf[4*AS_MAXLEN+4];     /* output buffer */

  assert(set && file);          /* check the function arguments */

  /* --- get range of attributes --- */
  if (mode & AS_RANGE) {        /* if an index range is given, */
    off = va_arg(*args, ATTID); /* get offset to first value */
    cnt = va_arg(*args, ATTID); /* and number of values */
    k   = set->cnt -off;        /* check and adapt the */
    if (cnt > k) cnt = k; }     /* number of attributes */
  else {                        /* if no index range is given, */
    off = 0;                    /* get the full index range */
    cnt = set->cnt;             /* (cover all attributes) */
  }                             /* afterwards check range params. */
  assert((off >= 0) && (cnt >= 0));

  /* --- write header (as a comment) --- */
  if (mode & AS_TITLE) {        /* if the title flag is set */
    len = (maxlen > 2) ? maxlen -2 : 70;
    fputs("/*", file); for (i = len; --i >= 0; ) putc('-', file);
    fprintf(file, "\n  %s\n", set->name);
    for (i = len; --i >= 0; ) putc('-', file); fputs("*/\n", file);
  }                             /* write a title header */
  if (maxlen <= 0) maxlen = INT_MAX;

  /* --- write attribute domains --- */
  for (k = 0; k < cnt; k++) {   /* traverse the attributes */
    att = set->atts[off+k];     /* get the next attribute */
    if ((mode & AS_MARKED)      /* if in marked mode and */
    &&  (att->mark < 0))        /* attribute is not marked, */
      continue;                 /* skip this attribute */
    scn_format(buf, att->name, 0);
    fprintf(file, "dom(%s) = ", buf);
    if      (att->type == AT_INT) { /* --- if integer attribute */
      fputs("ZZ", file);        /* write integer numbers symbol */
      if ((mode & AS_IVALS)     /* write the integer interval */
      &&  (att->min.i <= att->max.i))
        fprintf(file, " [%"DTINT_FMT", %"DTINT_FMT"]",
                         att->min.i,  att->max.i); }
    else if (att->type == AT_FLT) { /* --- if floating point */
      fputs("IR", file);        /* write real numbers symbol */
      if ((mode & AS_IVALS)     /* write the number interval */
      &&  (att->min.f <= att->max.f))
        fprintf(file, " [%.*"DTFLT_FMT", %.*"DTFLT_FMT"]",
                att->sd2p, att->min.f, att->sd2p, att->max.f); }
    else {                      /* --- if nominal attribute */
      fputs("{", file);         /* start the list of values */
      pos = att->attwd[1] +10;  /* init. the position */
      for (m = 0; m < att->cnt; m++) {
        if (m > 0) {            /* if this is not the first value, */
          putc(',', file); pos++; }           /* write a separator */
        len = (int)scn_format(buf, att->vals[m]->name, 0);
        if ((pos+len > maxlen-4)/* if line would get too long, */
        &&  (pos     > 2)) {    /* start a new line */
          fputs("\n ", file); pos = 1; }
        putc(' ', file);        /* write a separating blank and */
        fputs(buf, file);       /* the (scanable) value name, */
        pos += len +1;          /* then calculate new position */
      }                         /* in the output line */
      fputs(" }", file);        /* terminate the list of values */
    }
    if ((mode & AS_DIRS)        /* if to write attribute directions */
    &&  (att->dir >= DIR_NONE)  /* and direction is in allowed range, */
    &&  (att->dir <= DIR_OUT)){ /* write a direction indicator */
      fputs(" : ", file); fputs(dirs[att->dir], file); }
    if (mode & AS_WEIGHT)       /* if to write attribute weights */
      fprintf(file, ", %.*"WGT_FMT, att->sd2p, att->wgt);
    fputs(";\n", file);         /* terminate the attribute desc. */
  }                             /* and the output line */

  return ferror(file);          /* check for a write error */
}  /* as_vdesc() */

#endif  /* #ifdef AS_DESC */
/*----------------------------------------------------------------------
  Parser Functions
----------------------------------------------------------------------*/
#ifdef AS_PARSE

static int domains (ATTSET *set, SCANNER *scan, int tflags)
{                               /* --- parse domains descriptions */
  int    t;                     /* buffer for current token */
  CCHAR  *v;                    /* buffer for token value */
  ATT    *att;                  /* attribute read */
  int    type;                  /* attribute type */
  WEIGHT w;                     /* attribute weight */

  while (1) {                   /* parse domain definitions */
    if (scn_token(scan) != T_ID)    return 0;
    v = scn_value(scan);        /* check for 'dom' keyword */
    if ((strcmp(v, "dom")    != 0)
    &&  (strcmp(v, "domain") != 0)) return 0;
    SCN_NEXT(scan);             /* consume 'dom' */
    SCN_CHAR(scan, '(');        /* consume '(' */
    t = scn_token(scan);        /* check for a valid name */
    if ((t != T_ID) && (t != T_NUM))
      SCN_ERROR(scan, E_ATTEXP, scn_value(scan));
    att = att_create(v = scn_value(scan), AT_NOM);
    if (!att) SCN_ERROR(scan, E_NOMEM);
    t = as_attadd(set, att);    /* create a new attribute and */
    if (t) { att_delete(att);   /* add it to the attribute set */
      if (t < 0) SCN_ERROR(scan, E_NOMEM);
      if (t > 0) SCN_ERROR(scan, E_DUPATT, v);
    }                           /* check for a duplicate attribute */
    SCN_NEXT(scan);             /* consume attribute name */
    SCN_CHAR(scan, ')');        /* consume ')' */
    SCN_CHAR(scan, '=');        /* consume '=' */
    type = -1;                  /* init. attribute type to 'none' */
    t = scn_token(scan);        /* get the next token and */
    v = scn_value(scan);        /* the corresponding token value */
    if      (t == '{')          /* if a set of values follows, */
      type = tflags & AT_NOM;   /* attribute is nominal */
    else if (t == T_ID) {       /* if an identifier follows */
      if      ((strcmp(v, "ZZ")      == 0)
      ||       (strcmp(v, "Z")       == 0)
      ||       (strcmp(v, "int")     == 0)
      ||       (strcmp(v, "integer") == 0))
        type = tflags & AT_INT; /* attribute is integer-valued */
      else if ((strcmp(v, "IR")      == 0)
      ||       (strcmp(v, "R")       == 0)
      ||       (strcmp(v, "real")    == 0)
      ||       (strcmp(v, "float")   == 0))
        type = tflags & AT_FLT; /* attribute is floating point */
    }                           /* (get and check attribute type) */
    if (type <= 0) SCN_ERROR(scan, E_DOMAIN, v);
    att->type = type;           /* set attribute type */
    if (type != AT_NOM) {       /* if attribute is numeric */
      SCN_NEXT(scan);           /* consume type indicator */
      if (type == AT_INT) {     /* if attribute is integer */
        att->min.i = DTINT_MAX; /* initialize minimal */
        att->max.i = DTINT_MIN;}/* and maximal value */
      else {                    /* if attribute is floating point */
        att->min.f = DTFLT_MAX; /* initialize minimal */
        att->max.f = DTFLT_MIN; /* and maximal value */
      }
      if (scn_token(scan) == '[') { /* if a range of values is given */
        SCN_NEXT(scan);         /* consume '[' */
        SCN_NUM(scan);          /* check for a number */
        if (att_valadd(att, scn_value(scan), NULL) != 0)
          SCN_ERROR(scan, E_NUMBER);
        SCN_NEXT(scan);         /* consume lower bound */
        SCN_CHAR(scan, ',');    /* consume ',' */
        SCN_NUM(scan);          /* check for a number */
        if (att_valadd(att, scn_value(scan), NULL) != 0)
          SCN_ERROR(scan, E_NUMBER);
        SCN_NEXT(scan);         /* consume upper bound */
        SCN_CHAR(scan, ']');    /* consume ']' */
      } }
    else {                      /* if attribute is nominal */
      SCN_CHAR(scan, '{');      /* consume '{' */
      if (scn_token(scan) != '}') {
        while (1) {             /* read a list of values */
          t = scn_token(scan);   /* check for a name */
          if ((t != T_ID) && (t != T_NUM))
            SCN_ERROR(scan, E_VALEXP, scn_value(scan));
          t = att_valadd(att, v = scn_value(scan), NULL);
          if (t < 0) SCN_ERROR(scan, E_NOMEM);
          if (t > 0) SCN_ERROR(scan, E_DUPVAL, v);
          SCN_NEXT(scan);       /* get and consume attribute value */
          if (scn_token(scan) != ',') break;
          SCN_NEXT(scan);       /* if at end of list, abort loop, */
        }                       /* otherwise consume ',' */
      }
      SCN_CHAR(scan, '}');      /* consume '}' */
    }
    if (scn_token(scan) == ':'){/* if a direction indication follows */
      SCN_NEXT(scan);           /* consume ',' */
      if (scn_token(scan) != T_ID)
        SCN_ERROR(scan, E_STREXP, "in");
      v = scn_value(scan);      /* get the direction indicator */
      if      (strcmp(v, "none") == 0) att->dir = DIR_NONE;
      else if (strcmp(v, "id")   == 0) att->dir = DIR_ID;
      else if (strcmp(v, "in")   == 0) att->dir = DIR_IN;
      else if (strcmp(v, "out")  == 0) att->dir = DIR_OUT;
      else SCN_ERROR(scan, E_STREXP, "in");
      SCN_NEXT(scan);           /* get the direction code */
    }                           /* and consume the token */
    if (scn_token(scan) == ','){/* if a weight indication follows */
      SCN_NEXT(scan);           /* consume the comma */
      SCN_NUM(scan);            /* get the attribute weight */
      w = asu_str2wgt(scn_value(scan));
      if (errno || isnan(w)) SCN_ERROR(scan, E_NUMBER);
      att->wgt = w;             /* check and set attribute weight */
      SCN_NEXT(scan);           /* and consume the token */
    }
    SCN_CHAR(scan, ';');        /* consume ';' */
  }  /* while (1) .. */
}  /* domains() */

/*--------------------------------------------------------------------*/

int as_parse (ATTSET *set, SCANNER *scan, int types, int mand)
{                               /* --- parse domain descriptions */
  int   r, e = 0;               /* result of function, error flag */
  ATTID n;                      /* number of attributes */

  assert(set);                  /* check the function argument */
  n = set->cnt;                 /* note the number of attributes */
  scn_setmsgs(scan, msgs, (int)(sizeof(msgs)/sizeof(*msgs)));
  SCN_FIRST(scan);              /* set messages, get first token */
  while (1) {                   /* domain read loop (with recovery) */
    r = domains(set, scan, types);      /* read attribute domains */
    if (r == 0)       break;    /* if no error occurred, abort loop */
    e = -1;                     /* otherwise set the error flag */
    if (r == E_NOMEM) return e; /* always abort on 'out of memory' */
    scn_recover(scan, ';', 0, 0, 0);
  }                             /* otherwise recover from the error */
  if (e) return e;              /* if an error occurred abort */
  if (mand && (n >= set->cnt)){ /* check if an attribute was read */
    scn_error(scan, E_STREXP, "dom"); return -1; }
  return 0;                     /* return 'ok' */
}  /* as_parse() */

#endif  /* #ifdef AS_PARSE */
