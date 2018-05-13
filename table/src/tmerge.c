/*----------------------------------------------------------------------
  File    : tmerge.c
  contents: program to merge tables / project a table / align a table
  Author  : Christian Borgelt
  History : 1996.03.08 file created as proj.c
            1996.07.01 alignment option removed
            1996.11.22 options -b, -f and -r added
            1998.01.02 column/field alignment (option -a) added
            1998.01.07 no header in projected table (option -c) added
            1998.02.25 table merging added and renamed to tmerge.c
            1998.06.23 extended to merging multiple tables
            1998.09.12 adapted to modified module attset
            1998.09.25 table reading simplified
            1999.02.07 input from stdin, output to stdout added
            1999.02.12 default header handling improved
            1999.04.17 simplified using the new module 'io'
            2001.07.14 adapted to modified module tabread
            2003.08.16 slight changes in error message output
            2007.02.13 adapted to modified module attset
            2010.10.08 adapted to modules tabread and tabwrite
            2010.12.12 adapted to a generic error reporting function
            2011.01.05 adapted to functions tab_read() and tab_write()
            2013.07.21 adapted to definitions ATTID, VALID, WEIGHT etc.
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
#define PRGNAME     "tmerge"
#define DESCRIPTION "merge tables / project / align a table"
#define VERSION     "version 2.1 (2014.10.24)         " \
                    "(c) 1996-2014   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */

#define SEC_SINCE(t)  ((double)(clock()-(t)) /(double)CLOCKS_PER_SEC)

/*----------------------------------------------------------------------
  Constants
----------------------------------------------------------------------*/
static const char *errmsgs[] = {    /* error messages */
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
static CCHAR    *prgname = NULL;  /* program name for error messages */
static TABREAD  *tread   = NULL;  /* table reader */
static TABWRITE *twrite  = NULL;  /* table writer */
static ATTSET   *attset  = NULL;  /* attribute set */
static TABLE    *table   = NULL;  /* data table */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (table)  tab_delete(table,  0); \
  if (attset) as_delete (attset);    \
  if (tread)  trd_delete(tread,  1); \
  if (twrite) twr_delete(twrite, 1);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counter */
  char    *s;                   /* to traverse the options */
  CCHAR   **optarg = NULL;      /* to get option arguments */
  CCHAR   *fn_hdr[2] = { NULL, NULL };  /* names of header files */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  CCHAR   *hdr     = NULL;      /* buffer for header file name */
  int     mode[2]  = { AS_ATT, AS_ATT|AS_NOXATT };
  int     mout     = AS_ATT;    /* table file read and write modes */
  int     m, r;                 /* mode and function result */
  TPLID   n, on;                /* number of data tuples, buffers */
  double  w, ow;                /* weight of data tuples */
  clock_t t;                    /* timer for measurements */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] [-d|-h hdr1] in1 [-d|-h hdr2] "
                     "[in2 [in3 ...]] out\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-w       do not write attribute names to output file\n");
    printf("-a       align fields of output table           "
                    "(default: single separator)\n");
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
    printf("hdr1/2   files containing table header "
                    "(attribute names)\n");
    printf("in1/2/3  table files to read "
                    "(attribute names in first record)\n");
    printf("out      file to write merged tables/projected table to\n");
    return 0;                   /* print usage message */
  }                             /* and abort program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 'w': mout  &= ~AS_ATT;                   break;
          case 'a': mout  |= AS_ALIGN;                  break;
          case 'r': optarg = &recseps;                  break;
          case 'f': optarg = &fldseps;                  break;
          case 'b': optarg = &blanks;                   break;
          case 'u': optarg = &nullchs;                  break;
          case 'C': optarg = &comment;                  break;
          case 'n': mout  |= AS_WEIGHT;
                    mode[(k > 0) ? 1 : 0] |= AS_WEIGHT; break;
          case 'd': mode[(k > 0) ? 1 : 0] |= AS_DFLT;   break;
          case 'h': optarg = fn_hdr +((k > 0) ? 1 : 0); break;
          default : error(E_OPTION, *--s);              break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      argv[k++] = s;            /* note filenames */
    }                           /* by reordering/compacting */
  }                             /* the argument vector */
  if (optarg)  error(E_OPTARG); /* check option argument */
  if (k-- < 2) error(E_ARGCNT); /* check number of arguments */
  if (fn_hdr[0] && (strcmp(fn_hdr[0], "-") == 0))
    fn_hdr[0] = "";             /* convert "-" to "" for consistency */
  if (fn_hdr[1] && (strcmp(fn_hdr[1], "-") == 0))
    fn_hdr[1] = "";             /* convert "-" to "" for consistency */
  r = 0;                        /* count the empty file names */
  for (i = 0; i < 2; i++) { if (fn_hdr[i] && !*fn_hdr[i]) r++; }
  for (i = 0; i < k; i++) { if (!argv[i]  || !*argv[i])   r++; }
  if (r > 1) error(E_STDIN);    /* stdin must not be used twice */
  if ((mout & AS_ATT) && (mout & AS_ALIGN))
    mout |= AS_ALNHDR;          /* set align-to-header flag */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- read data tables --- */
  attset = as_create("domains", att_delete);
  if (!attset) error(E_NOMEM);  /* create an attribute set */
  table = tab_create("table", attset, tpl_delete);
  if (!table)  error(E_NOMEM);  /* create a data table */
  tread = trd_create();         /* create a table reader and */
  if (!tread)  error(E_NOMEM);  /* configure the characters */
  trd_allchs(tread, recseps, fldseps, blanks, nullchs, comment);
  ow = 0; on = 0;               /* init. tuple counter and weight */
  for (i = 0; i < k; i++) {     /* traverse the input files */
    m   = mode[(i > 0) ? 1 : 0];
    hdr = (i > 1) ? NULL : fn_hdr[i];
    if (hdr) {                  /* if a header file is given */
      t = clock();              /* start timer, open input file */
      if (trd_open(tread, NULL, hdr) != 0)
        error(E_FOPEN, trd_name(tread));
      fprintf(stderr, "reading %s ... ", trd_name(tread));
      r = as_read(attset, tread, (m & ~AS_DFLT) | AS_ATT);
      if (r < 0) error(-r, as_errmsg(attset, NULL, 0));
      trd_close(tread);         /* read table header, close file */
      fprintf(stderr, "[%"ATTID_FMT" attribute(s)]", as_attcnt(attset));
      fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
      m &= ~(AS_ATT|AS_DFLT);   /* print a success message and */
    }                           /* remove the attribute flags */
    t = clock();                /* start timer, open input file */
    if (trd_open(tread, NULL, argv[i]) != 0)
      error(E_FOPEN, trd_name(tread));
    fprintf(stderr, "reading %s ... ", trd_name(tread));
    r = tab_read(table, tread, m);
    if (r < 0) error(-r, tab_errmsg(table, NULL, 0));
    trd_close(tread);           /* read the table (body) */
    n = tab_tplcnt(table) -on;  /* and close the input file */
    w = tab_tplwgt(table) -ow;  /* get the number of tuples */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s),", tab_attcnt(table));
    fprintf(stderr, " %"TPLID_FMT, n);
    if (w != (double)n) fprintf(stderr, "/%g", w);
    fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));
    on += n; ow += w;           /* print a success message and */
  }                             /* update tuple counter and weight */
  trd_delete(tread, 1);         /* delete the table reader and */
  tread = NULL;                 /* clear the reader variable */

  /* --- write output table --- */
  twrite = twr_create();        /* create a table writer and */
  if (!twrite) error(E_NOMEM);  /* configure the characters */
  twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
  t = clock();                  /* start timer, open output file */
  if (twr_open(twrite, NULL, argv[k]) != 0)
    error(E_FOPEN, twr_name(twrite));
  fprintf(stderr, "writing %s ... ", twr_name(twrite));
  k = tab_write(table, twrite, mout);
  if (k) error(E_FWRITE, twr_name(twrite));
  twr_delete(twrite, 1);        /* write the one point coverages */
  twrite = NULL;                /* and delete the table writer */
  n = tab_tplcnt(table);        /* get the number of tuples */
  w = tab_tplwgt(table);        /* and print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", tab_attcnt(table));
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
