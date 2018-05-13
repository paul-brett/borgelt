/*----------------------------------------------------------------------
  File    : dom.c
  Contents: program to determine attribute domains
  Author  : Christian Borgelt
  History : 1995.11.24 file created
            1995.11.26 adapted to modified attset functions
            1995.12.08 output file variable made global (for clean up)
            1995.12.21 sorting of values in domains added
            1996.01.17 adapted to modified function as_read()
            1996.02.23 adapted to modified attset functions
            1996.11.22 options -b, -f, and -r added (characters)
            1997.02.26 tuple weights (option -n) added
            1998.01.11 null value characters (option -u) added
            1998.09.12 numerial/alphabetical sorting (option -S) added
            1998.09.25 table reading simplified
            1999.02.07 input from stdin, output to stdout added
            1999.02.12 default header handling improved
            1999.04.17 simplified using the new module 'io'
            2001.07.14 adapted to modified module tabread
            2002.06.18 meaning of option -i (intervals) inverted
            2003.08.16 slight changes in error message output
            2007.02.13 adapted to modified module attset
            2010.10.08 adapted to modified module tabread
            2010.12.12 adapted to a generic error reporting function
            2010.12.31 adapted to redesigned table reading concept
            2013.07.18 adapted to definitions ATTID, VALID, WEIGHT etc.
            2013.07.26 adapted to additional parameter of att_valsort()
            2014.10.24 changed from LGPL license to MIT license
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#ifndef AS_READ
#define AS_READ
#endif
#ifndef AS_DESC
#define AS_DESC
#endif
#include "attset.h"
#include "table.h"
#include "error.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "dom"
#define DESCRIPTION "determine attribute domains"
#define VERSION     "version 2.1 (2014.10.24)         " \
                    "(c) 1995-2014   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */

#define SEC_SINCE(t)  ((double)(clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
static const char *errmsgs[] = {   /* error messages */
  /* E_NONE     0 */  "no error",
  /* E_NOMEM   -1 */  "not enough memory",
  /* E_FOPEN   -2 */  "cannot open file %s",
  /* E_FREAD   -3 */  "read error on file %s",
  /* E_FWRITE  -4 */  "write error on file %s",
  /* E_STDIN   -5 */  "double assignment of standard input",
  /* E_OPTION  -6 */  "unknown option -%c",
  /* E_OPTARG  -7 */  "missing option argument",
  /* E_ARGCNT  -8 */  "wrong number of arguments",
  /*           -9 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR   *prgname;        /* program name for error messages */
static ATTSET  *attset = NULL;  /* attribute set */
static TABREAD *tread  = NULL;  /* table reader */
static FILE    *out    = NULL;  /* output file */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (attset) as_delete(attset); \
  if (tread)  trd_delete(tread, 1); \
  if (out && (out != stdout)) fclose(out);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

static int numcmp (const char *s, const char *t)
{                               /* --- numerical comparison of names */
  long a, b;                    /* integer values of names */

  a = strtol(s, NULL, 10);      /* convert names */
  b = strtol(t, NULL, 10);      /* to integer numbers */
  if (a < b) return -1;         /* compare numbers and */
  if (a > b) return  1;         /* only if they are equal */
  return strcmp(s, t);          /* compare the names directly */
}  /* numcmp() */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counter */
  char    *s;                   /* to traverse the options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_hdr  = NULL;      /* name of header file */
  CCHAR   *fn_tab  = NULL;      /* name of table  file */
  CCHAR   *fn_dom  = NULL;      /* name of domain file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  int     mode     = AS_ATT;    /* table file read mode */
  int     atype    = 0;         /* flag for automatic type determ. */
  int     sort     = 0;         /* flag for domain sorting */
  int     ivals    = AS_IVALS;  /* flag for numeric intervals */
  int     sd2p     = 6;         /* number of significant digits */
  int     maxlen   = 0;         /* maximal output line length */
  ATT     *att;                 /* to traverse attributes */
  ATTID   m, c;                 /* number of attributes */
  TPLID   n;                    /* number of data tuples */
  double  w;                    /* weight of data tuples */
  clock_t t;                    /* timer for measurement */
  int     (*cmp)(const char*, const char*);

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] [-d|-h hdrfile] "
                     "tabfile domfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-s       sort domains alphabetically            "
                    "(default: order of appearance)\n");
    printf("-S       sort domains numerically/alphabetically\n");
    printf("-a       automatic type determination           "
                    "(default: all nominal)\n");
    printf("-i       do not print intervals for numeric attributes\n");
    printf("-o#      number of significant digits           "
                    "(default: %d)\n", sd2p);
    printf("-l#      output line length                     "
                    "(default: no limit)\n");
    printf("-r#      record     separators                  "
                    "(default: \"\\n\")\n");
    printf("-f#      field      separators                  "
                    "(default: \" \\t,\")\n");
    printf("-b#      blank      characters                  "
                    "(default: \" \\t\\r\")\n");
    printf("-u#      null value characters                  "
                    "(default: \"?*\")\n");
    printf("-C#      comment    characters                  "
                    "(default: \"#\")\n");
    printf("-n       number of tuple occurrences in last field\n");
    printf("-d       use default header "
                    "(attribute names = field numbers)\n");
    printf("-h       read table header  "
                    "(attribute names) from hdrfile\n");
    printf("hdrfile  file containing table header "
                    "(attribute names)\n");
    printf("tabfile  table file to read "
                    "(attribute names in first record)\n");
    printf("domfile  file to write domain descriptions to\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse the arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 's': sort   = 1;                     break;
          case 'S': sort   = 2;                     break;
          case 'a': atype  = 1;                     break;
          case 'i': ivals  = 0;                     break;
          case 'o': sd2p   = (int)strtol(s, &s, 0); break;
          case 'l': maxlen = (int)strtol(s, &s, 0); break;
          case 'r': optarg = &recseps;              break;
          case 'f': optarg = &fldseps;              break;
          case 'b': optarg = &blanks;               break;
          case 'u': optarg = &nullchs;              break;
          case 'C': optarg = &comment;              break;
          case 'n': mode  |= AS_WEIGHT;             break;
          case 'd': mode  |= AS_DFLT;               break;
          case 'h': optarg = &fn_hdr;               break;
          default : error(E_OPTION, *--s);          break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_tab = s;      break;
        case  1: fn_dom = s;      break;
        default: error(E_ARGCNT); break;
      }                         /* note filenames */
    }
  }
  if (optarg) error(E_OPTARG);  /* check option argument */
  if (k != 2) error(E_ARGCNT);  /* check number of arguments */
  if (fn_hdr && (strcmp(fn_hdr, "-") == 0))
    fn_hdr = "";                /* convert "-" to "" for consistency */
  i = ( fn_hdr && !*fn_hdr) ? 1 : 0;
  if  (!fn_tab || !*fn_tab) i++;/* check assignments of stdin: */
  if (i > 1) error(E_STDIN);    /* stdin must not be used twice */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- read data table --- */
  attset = as_create("domains", att_delete);
  if (!attset) error(E_NOMEM);  /* create an attribute set */
  tread = trd_create();         /* create a table reader and */
  if (!tread)  error(E_NOMEM);  /* configure the characters */
  trd_allchs(tread, recseps, fldseps, blanks, nullchs, comment);
  if (fn_hdr) {                 /* if a header file is given */
    t = clock();                /* start timer, open input file */
    if (trd_open(tread, NULL, fn_hdr) != 0)
      error(E_FOPEN, trd_name(tread));
    fprintf(stderr, "reading %s ... ", trd_name(tread));
    k = as_read(attset, tread, (mode & ~AS_DFLT) | AS_ATT);
    if (k < 0) error(-k, as_errmsg(attset, NULL, 0));
    trd_close(tread);           /* read table header, close file */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s)]", as_attcnt(attset));
    fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
    mode &= ~(AS_ATT|AS_DFLT);  /* print a success message and */
  }                             /* remove the attribute flags */
  t = clock();                  /* start timer, open input file */
  if (trd_open(tread, NULL, fn_tab) != 0)
    error(E_FOPEN, trd_name(tread));
  fprintf(stderr, "reading %s ... ", trd_name(tread));
  w = 0; n = 0;                 /* init. tuple counter and weight */
  k = as_read(attset, tread, mode);
  i = mode;                     /* read the table header/1st tuple */
  mode = (mode & ~(AS_DFLT|AS_ATT)) | AS_INST;
  if ((k == 0) && (i & AS_ATT)) /* read the first data tuple */
    k = as_read(attset, tread, mode);   /* (if not done yet) */
  while (k == 0) {              /* while a tuple was read */
    n++; w += as_getwgt(attset);/* count tuple and sum its weight */
    k = as_read(attset, tread, mode);
  }                             /* try to read the next tuple */
  if (k < 0)                    /* check for and report read error */
    error(-k, as_errmsg(attset, NULL, 0));
  trd_delete(tread, 1);         /* delete the table reader and */
  tread = NULL;                 /* print a success message */
  m = as_attcnt(attset);        /* get the number of attributes */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- convert/sort domains --- */
  if (atype) {                  /* if automatic type determination, */
    for (c = 0; c < m; c++)     /* try to convert attributes */
      att_convert(as_att(attset, c), AT_AUTO, NULL);
  }
  if (sort) {                   /* if to sort domains (values) */
    cmp = (sort > 1) ? numcmp : strcmp;
    for (c = 0; c < m; c++) {   /* traverse the attributes */
      att = as_att(attset, c);  /* that are nominal */
      if (att_type(att) == AT_NOM) att_valsort(att, 1, cmp, NULL, 0);
    }                           /* sort the domain (values) */
  }
  for (c = 0; c < m; c++)       /* set number of significant digits */
    att_setsd2p(as_att(attset, c), sd2p);

  /* --- write domain file --- */
  t = clock();                  /* start timer, open output file */
  if (fn_dom && (strcmp(fn_dom, "-") == 0)) fn_dom = "";
  if (fn_dom && *fn_dom) { out = fopen(fn_dom, "w"); }
  else                   { out = stdout; fn_dom = "<stdout>"; }
  fprintf(stderr, "writing %s ... ", fn_dom);
  if (!out) error(E_FOPEN, fn_dom);
  if (as_desc(attset, out, AS_TITLE|ivals, maxlen)  != 0)
    error(E_FWRITE, fn_dom);    /* write domain descriptions */
  if (((out == stdout) ? fflush(out) : fclose(out)) != 0)
    error(E_FWRITE, fn_dom);    /* close the output file and */
  out = NULL;                   /* print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s)]", as_attcnt(attset));
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
