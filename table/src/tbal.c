/*----------------------------------------------------------------------
  File    : tbal.c
  Contents: program to balance value frequencies
  Author  : Christian Borgelt
  History : 1999.02.13 file created from file skel1.c
            1999.04.17 simplified using the new module 'io'
            2001.07.14 adapted to modified module tabread
            2003.08.16 slight changes in error message output
            2007.02.13 adapted to redesigned module attset
            2010.10.08 adapted to modules tabread and tabwrite
            2010.12.12 adapted to a generic error reporting function
            2011.01.14 adapted to functions tab_read() and tab_write()
            2013.07.21 adapted to definitions ATTID, VALID, WEIGHT etc.
            2013.08.01 table reduction made optional (option -R)
            2014.10.24 changed from LGPL license to MIT license
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#ifndef TAB_READ
#define TAB_READ
#endif
#ifndef TAB_WRITE
#define TAB_WRITE
#endif
#include "table.h"
#include "error.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "tbal"
#define DESCRIPTION "balance or adapt class frequencies"
#define VERSION     "version 2.1 (2014.10.24)         " \
                    "(c) 1999-2014   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */
#define E_MISATT    (-9)        /* no class attribute found */
#define E_UNKATT   (-10)        /* unknown class attribute */
#define E_FLDCNT   (-11)        /* wrong number of fields */
#define E_EXPVAL   (-12)        /* value expected */
#define E_UNKVAL   (-10)        /* unknown   value */
#define E_DUPVAL   (-11)        /* duplicate value */
#define E_EXPNUM   (-12)        /* number expected */
#define E_NUMBER   (-13)        /* invalid number */

#define SEC_SINCE(t)  ((double)(clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
static const char *errmsgs[] = {   /* error messages */
  /* E_NONE      0 */  "no error",
  /* E_NOMEM    -1 */  "not enough memory",
  /* E_FOPEN    -2 */  "cannot open file %s",
  /* E_FREAD    -3 */  "read error on file %s",
  /* E_FWRITE   -4 */  "write error on file %s",
  /* E_STDIN    -5 */  "double assignment of standard input",
  /* E_OPTION   -6 */  "unknown option -%c",
  /* E_OPTARG   -7 */  "missing option argument",
  /* E_ARGCNT   -8 */  "wrong number of arguments",
  /* E_MISATT   -9 */  "no (class) attribute found",
  /* E_UNKATT  -10 */  "unknown class attribute '%s'",
  /* E_FLDCNT  -11 */  "%s:%d(%d): wrong number of fields/columns",
  /* E_EXPVAL  -12 */  "%s:%d(%d): attribute value expected",
  /* E_UNKVAL  -10 */  "%s:%d(%d): unknown attribute value '%s'",
  /* E_DUPVAL  -11 */  "%s:%d(%d): duplicate attribute value '%s'",
  /* E_EXPNUM  -12 */  "%s:%d(%d): number expected",
  /* E_NUMBER  -13 */  "%s:%d(%d): invalid number %s",
  /*           -16 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR    *prgname;       /* program name for error messages */
static TABREAD  *tread  = NULL; /* table reader */
static TABWRITE *twrite = NULL; /* table writer */
static ATTSET   *attset = NULL; /* attribute set */
static TABLE    *table  = NULL; /* data table */
static double   *frqs   = NULL; /* value frequency vector */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (frqs)   free(frqs);            \
  if (table)  tab_delete(table, 0);  \
  if (attset) as_delete (attset);    \
  if (tread)  trd_delete(tread,  1); \
  if (twrite) twr_delete(twrite, 1);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counters */
  char    *s, *b;               /* to traverse options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_hdr  = NULL;      /* name of header    file */
  CCHAR   *fn_tab  = NULL;      /* name of table     file */
  CCHAR   *fn_frq  = NULL;      /* name of frequency file */
  CCHAR   *fn_out  = NULL;      /* name of output    file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  CCHAR   *clsname = NULL;      /* name of class column to balance */
  int     mode     = AS_ATT;            /* table file read  mode */
  int     mout     = AS_ATT|AS_WEIGHT;  /* table file write mode */
  int     reduce   = 0;         /* flag for table reduction */
  double  sum      = 0;         /* weight of tuples in output table */
  int     d;                    /* delimiter type */
  VALID   valcnt;               /* number of attribute values */
  VALID   v, c;                 /* loop variables for values/classes */
  ATTID   clsid;                /* identifier/index of class column */
  ATT     *att;                 /* class attribute */
  ATTID   m;                    /* number of attributes/columns */
  TPLID   n;                    /* number of data tuples */
  double  w;                    /* weight of data tuples */
  clock_t t;                    /* timer for measurement */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] [-q frqfile] [-d|-h hdrfile] "
                     "tabfile outfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-c#      name of class attribute                "
                    "(default: last attribute)\n");
    printf("-s#      sum of tuple weights in output table   "
                    "(default: as in input)\n");
    printf("         (-2: lower, -1: boost, 0: shift weights)\n");
    printf("-q       adjust to relative frequencies in frqfile\n");
    printf("frqfile  file containing value/relative frequency pairs\n");
    printf("-R       reduce by combining equal tuples       "
                    "(default: do not reduce)\n");
    printf("-a       align fields of output table           "
                    "(default: single separator)\n");
    printf("-w       do not write attribute names to output file\n");
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
    printf("outfile  file to write output table to\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 'c': optarg = &clsname;      break;
          case 's': sum    = strtod(s, &s); break;
          case 'q': optarg = &fn_frq;       break;
          case 'R': reduce = 1;             break;
          case 'a': mout  |=  AS_ALIGN;     break;
          case 'w': mout  &= ~AS_ATT;       break;
          case 'r': optarg = &recseps;      break;
          case 'f': optarg = &fldseps;      break;
          case 'b': optarg = &blanks;       break;
          case 'u': optarg = &nullchs;      break;
          case 'C': optarg = &comment;      break;
          case 'n': mode  |= AS_WEIGHT;     break;
          case 'd': mode  |= AS_DFLT;       break;
          case 'h': optarg = &fn_hdr;       break;
          default : error(E_OPTION, *--s);  break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_tab = s;      break;
        case  1: fn_out = s;      break;
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
  if ((mout & AS_ATT) && (mout & AS_ALIGN))
    mout |= AS_ALNHDR;          /* set align-to-header flag */
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
  table = tab_create("table", attset, tpl_delete);
  if (!table) error(E_NOMEM);   /* create a data table */
  t = clock();                  /* start timer, open input file */
  if (trd_open(tread, NULL, fn_tab) != 0)
    error(E_FOPEN, trd_name(tread));
  fprintf(stderr, "reading %s ... ", trd_name(tread));
  if (!fn_hdr                   /* if the attribute names are in the */
  && (mode & (AS_ATT|AS_DFLT))) {     /* table file or default names */
    k = tab_read(table, tread, mode | TAB_ONE);
    if (k < 0) error(-k, tab_errmsg(table, NULL, 0));
    mode &= ~(AS_ATT|AS_DFLT);  /* read the first table record */
  }                             /* and remove the attribute flag */
  if (!clsname) {               /* if no class name is given, */
    clsid = as_attcnt(attset)-1;/* use last attribute as default */
    if (clsid < 0) error(E_MISATT); }
  else {                        /* if a class name is given */
    clsid = as_attid(attset, clsname);
    if (clsid < 0) error(E_UNKATT, clsname);
  }                             /* get the class identifier */
  k = tab_read(table, tread, mode);
  if (k < 0) error(-k, tab_errmsg(table, NULL, 0));
  trd_close(tread);             /* read the table body */
  m = tab_attcnt(table);        /* get the number of attributes */
  n = tab_tplcnt(table);        /* and the number of data tuples */
  w = tab_tplwgt(table);        /* and print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- read frequencies --- */
  if (fn_frq) {                 /* if class frequencies are given */
    t = clock();                /* start timer, open input file */
    if (trd_open(tread, NULL, fn_frq) != 0)
      error(E_FOPEN, trd_name(tread));
    fprintf(stderr, "reading %s ... ", trd_name(tread));
    att    = as_att(attset, clsid);
    valcnt = att_valcnt(att);   /* get att. and number of values */
    frqs  = (double*)malloc((size_t)valcnt *sizeof(double));
    if (!frqs) error(E_NOMEM);  /* allocate a frequency vector */
    for (v = 0; v < valcnt; v++) frqs[v] = -1;
    c = 0;                      /* init. the frequency counter */
    while (1) {                 /* frequency read loop */
      d = trd_read(tread);      /* read the next field */
      if (d <= TRD_ERR) error(E_FREAD,  trd_name(tread));
      if (d <= TRD_EOF) break;  /* check for error and end of file */
      b = trd_field(tread);     /* get the field contents and */
      if (!*b) {                /* check whether it is empty */
        if (d != TRD_REC) error(E_EXPVAL, TRD_INFO(tread));
        continue;               /* if the whole line is empty, */
      }                         /* simply skip the line */
      if (d != TRD_FLD) error(E_EXPNUM, TRD_INFO(tread));
      v = att_valid(att, b);    /* get the value identifier */
      if (v < 0)        error(E_EXPVAL, TRD_INFO(tread));
      if (frqs[v] >= 0) error(E_DUPVAL, TRD_INFO(tread));
      d = trd_read(tread);      /* read the next field */
      if (d == TRD_ERR) error(E_FREAD,  trd_name(tread));
      if (d != TRD_REC) error(E_FLDCNT, TRD_INFO(tread));
      b = trd_field(tread);     /* get the field contents and */
      frqs[i] = strtod(b, &s);  /* decode the value frequency */
      if (!b || *s || (frqs[i] < 0))
        error(E_NUMBER, TRD_INFO(tread));
      c++;                      /* convert frequency to a number */
    }                           /* and count the class value */
    fprintf(stderr, "[%"VALID_FMT" class(es)]", c);
    fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
    for (v = 0; v < valcnt; v++) { if (frqs[v] < 0) frqs[v] = 1; }
  }                             /* fill with default frequencies */
  trd_delete(tread, 1);         /* close the input file and */
  tread = NULL;                 /* delete the table reader */

  /* --- balance class frequencies --- */
  t = clock();                  /* start timer, print log message */
  fprintf(stderr, "balancing class frequencies ... ");
  if (reduce)                   /* reduce the table (if requested) */
    n = tab_reduce(table);      /* and then balance it */
  w = tab_balance(table, clsid, sum, frqs,
                  (sum < 0) && (floor(sum) != sum));
  if (w < 0) error(E_NOMEM);    /* balance the class frequencies */
  c = att_valcnt(as_att(attset, clsid));
  fprintf(stderr, "[%"VALID_FMT" class(es)]", c);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- write output table --- */
  twrite = twr_create();        /* create a table writer and */
  if (!twrite) error(E_NOMEM);  /* configure the characters */
  twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
  t = clock();                  /* start timer, open output file */
  if (twr_open(twrite, NULL, fn_out) != 0)
    error(E_FOPEN, twr_name(twrite));
  fprintf(stderr, "writing %s ... ", twr_name(twrite));
  k = tab_write(table, twrite, mout); /* write balanced table */
  if (k) error(E_FWRITE, twr_name(twrite));
  if (twr_close(twrite) != 0) error(E_FWRITE, twr_name(twrite));
  twr_delete(twrite, 1);        /* close the output file and */
  twrite = NULL;                /* delete the table writer */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
#if 0
  if (w != (double)n) fprintf(stderr, "/%g", w);
#else
  if (fabs(1-w/(double)n) > 1e-6) fprintf(stderr, "/%g", w);
#endif
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
