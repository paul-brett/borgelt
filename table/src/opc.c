/*----------------------------------------------------------------------
  File    : opc.c
  Contents: program to determine one point coverages
  Author  : Christian Borgelt
  History : 1997.02.26 file created
            1997.03.27 argument 'domfile' removed
            1997.03.28 adapted to changes of table functions
            1997.03.29 condensed one point coverages added
            1997.09.22 meaning of option -c changed to opposite
            1998.01.11 null value characters (option -u) added
            1998.02.27 header output made optional (option -w)
            1998.09.12 adapted to modified module attset
            1998.09.25 table reading simplified
            1998.09.27 hash table added for tuple lookup
            1998.09.28 hash table version debugged
            1999.02.07 input from stdin, output to stdout added
            1999.02.12 default header handling improved
            1999.03.17 adapted to table one point coverage functions
            1999.04.17 simplified using the new module 'io'
            1999.10.21 normalization of one point coverages added
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
#include <stdarg.h>
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
#define PRGNAME     "opc"
#define DESCRIPTION "determine one point coverages"
#define VERSION     "version 2.1 (2014.10.24)         " \
                    "(c) 1997-2014   Christian Borgelt"

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
static CCHAR    *prgname;       /* program name for error messages */
static TABREAD  *tread  = NULL; /* table reader */
static TABWRITE *twrite = NULL; /* table writer */
static ATTSET   *attset = NULL; /* attribute set */
static TABLE    *table  = NULL; /* data table */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (table)  tab_delete(table, 0);  \
  if (attset) as_delete (attset);    \
  if (tread)  trd_delete(tread,  1); \
  if (twrite) twr_delete(twrite, 1);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counter */
  char    *s;                   /* to the traverse options */
  CCHAR   **optarg = NULL;      /* to get option arguments */
  CCHAR   *fn_hdr  = NULL;      /* name of header file */
  CCHAR   *fn_tab  = NULL;      /* name of table  file */
  CCHAR   *fn_opc  = NULL;      /* name of one point coverages file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  int     mode     = AS_ATT;    /* table file read and write mode */
  int     mout     = AS_ATT|AS_WEIGHT;
  int     cond     = TAB_COND;  /* flag for condensed form */
  int     opc      = 1;         /* flag for one point coverages */
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
    printf("usage: %s [options] [-d|-h hdrfile] "
                     "tabfile opcfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-p       do not compute one point coverages "
                    "(reduce only)\n");
    printf("-c       do not compute condensed form (expand fully)\n");
    printf("-z       normalize one point coverages\n");
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
    printf("hdrfile  file containing table header "
                    "(attribute names)\n");
    printf("tabfile  table file to read "
                    "(attribute names in first record)\n");
    printf("opcfile  file to write one point coverages to\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 'p': opc    = 0;                            break;
          case 'c': cond   = (cond & TAB_NORM) | TAB_FULL; break;
          case 'z': cond  |= TAB_NORM;                     break;
          case 'w': mout  &= ~AS_ATT;                      break;
          case 'a': mout  |= AS_ALIGN;                     break;
          case 'r': optarg = &recseps;                     break;
          case 'f': optarg = &fldseps;                     break;
          case 'b': optarg = &blanks;                      break;
          case 'u': optarg = &nullchs;                     break;
          case 'C': optarg = &comment;                     break;
          case 'n': mode  |= AS_WEIGHT;                    break;
          case 'd': mode  |= AS_DFLT;                      break;
          case 'h': optarg = &fn_hdr;                      break;
          default : error(E_OPTION, *--s);                 break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_tab = s;      break;
        case  1: fn_opc = s;      break;
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
  table = tab_create("table", attset, tpl_delete);
  if (!table) error(E_NOMEM);   /* create a data table */
  t = clock();                  /* start timer, open input file */
  if (trd_open(tread, NULL, fn_tab) != 0)
    error(E_FOPEN, trd_name(tread));
  fprintf(stderr, "reading %s ... ", trd_name(tread));
  k = tab_read(table, tread, mode);
  if (k < 0) error(-k, tab_errmsg(table, NULL, 0));
  trd_delete(tread, 1);         /* read the table body and */
  tread = NULL;                 /* delete the table reader */
  m = tab_attcnt(table);        /* get the number of attributes */
  n = tab_tplcnt(table);        /* and the number of data tuples */
  w = tab_tplwgt(table);        /* and print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- compute one point coverages --- */
  t = clock();                  /* start timer, print log message */
  if (opc) fprintf(stderr, "computing one point coverages ... ");
  else     fprintf(stderr, "reducing table ... ");
  tab_reduce(table);            /* reduce the data table */
  if (opc && (tab_opc(table, cond) != 0))
    error(E_NOMEM);             /* determine one point coverages */
  n = tab_tplcnt(table);        /* get the number of dat tuples */
  w = tab_tplwgt(table);        /* and print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- write output table --- */
  twrite = twr_create();        /* create a table writer and */
  if (!twrite) error(E_NOMEM);  /* configure the characters */
  twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
  t = clock();                  /* start timer, open output file */
  if (twr_open(twrite, NULL, fn_opc) != 0)
    error(E_FOPEN, twr_name(twrite));
  fprintf(stderr, "writing %s ... ", twr_name(twrite));
  k = tab_write(table, twrite, mout); /* write one point coverages */
  if (k) error(E_FWRITE, twr_name(twrite));
  if (twr_close(twrite) != 0) error(E_FWRITE, twr_name(twrite));
  twr_delete(twrite, 1);        /* close the output file and */
  twrite = NULL;                /* delete the table writer */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
