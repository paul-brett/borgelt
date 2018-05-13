/*----------------------------------------------------------------------
  File    : skel1.c
  Contents: table program skeleton (without domain file reading)
  Author  : Christian Borgelt
  History : 1997.08.28 file created
            1998.09.25 table reading simplified
            1999.02.07 input from stdin, output to stdout added
            1999.02.12 default header handling improved
            2003.08.16 slight changes in error message output
            2010.11.17 adapted to modified module tabread
            2010.12.12 adapted to a generic error reporting function
            2010.12.31 adapted to functions tab_read() and tab_write()
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
#include "error.h"
#include "table.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "skel1"
#define DESCRIPTION "table program skeleton"
#define VERSION     "version 0.14 (2014.10.24)        " \
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
  int     i, k = 0;             /* loop variables, counters */
  char    *s;                   /* to the traverse options */
  CCHAR   **optarg = NULL;      /* to get option arguments */
  CCHAR   *fn_hdr  = NULL;      /* name of header file */
  CCHAR   *fn_tab  = NULL;      /* name of table  file */
  CCHAR   *recseps = NULL;      /* record  separators */
  CCHAR   *fldseps = NULL;      /* field   separators */
  CCHAR   *blanks  = NULL;      /* blank   characters */
  CCHAR   *comment = NULL;      /* comment characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  int     mode     = AS_ATT;    /* table file read mode */
  int     show     = 0;         /* whether to show the table read */
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
    printf("usage: %s [options] [-d|-h hdrfile] tabfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-s       show the table (print to stdout)\n");
    printf("-r#      record  separators    (default: \"\\n\")\n");
    printf("-f#      field   separators    (default: \" \\t,\")\n");
    printf("-b#      blank   characters    (default: \" \\t\\r\")\n");
    printf("-C#      comment characters    (default: \"#\")\n");
    printf("-u#      null value characters (default: \"?*\")\n");
    printf("-n       number of tuple occurrences in last field\n");
    printf("-d       use default header "
                    "(attribute names = field numbers)\n");
    printf("-h       read table header  "
                    "(attribute names) from hdrfile\n");
    printf("hdrfile  file containing table header "
                    "(attribute names)\n");
    printf("tabfile  table file to read "
                    "(attribute names in first record)\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 's': show   = 1;            break;
          case 'r': optarg = &recseps;     break;
          case 'f': optarg = &fldseps;     break;
          case 'b': optarg = &blanks;      break;
          case 'u': optarg = &nullchs;     break;
          case 'C': optarg = &comment;     break;
          case 'n': mode  |= AS_WEIGHT;    break;
          case 'd': mode  |= AS_DFLT;      break;
          case 'h': optarg = &fn_hdr;      break;
          default : error(E_OPTION, *--s); break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_tab = s;      break;
        default: error(E_ARGCNT); break;
      }                         /* note filename */
    }
  }
  if (optarg) error(E_OPTARG);  /* check option argument */
  if (k != 1) error(E_ARGCNT);  /* check number of arguments */
  if (fn_hdr && (strcmp(fn_hdr, "-") == 0))
    fn_hdr = "";                /* convert "-" to "" for consistency */
  if (fn_hdr && !*fn_hdr && (!fn_tab || !*fn_tab))
    error(E_STDIN);             /* stdin must not be used twice */
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
  }                             /* remove the attribute flag */
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
  w = tab_tplwgt(table);        /* and their total weight */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* >>> application specific code goes here <<< */
  if (show) {                   /* if to show the table */
    twrite = twr_create();      /* create a table writer and */
    if (!twrite) error(E_NOMEM);/* configure the characters */
    twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
    twr_open(twrite, stdout, NULL);
    k = tab_write(table, twrite, AS_ATT|AS_ALIGN);
    if (k < 0) error(k, twr_name(twrite));
    twr_delete(twrite, 0);      /* write the data table */
    twrite = NULL;              /* to standard output and */
  }                             /* delete the table writer */

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
