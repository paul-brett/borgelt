/*----------------------------------------------------------------------
  File    : stats.c
  Contents: compute simple descriptive statistics
  Author  : Christian Borgelt
  History : 2015.04.15 file created
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "error.h"
#include "strlist.h"
#include "tabread.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "stats"
#define DESCRIPTION "compute simple descriptive statistics"
#define VERSION     "version 1.0 (2014.04.15)        " \
                    "(c) 2015   Christian Borgelt"

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
static TABREAD *tread  = NULL;  /* table reader */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (tread) trd_delete(tread, 1);
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
  CCHAR   *fn_out  = NULL;      /* name of output file */
  CCHAR   *recseps = NULL;      /* record  separators */
  CCHAR   *fldseps = NULL;      /* field   separators */
  CCHAR   *blanks  = NULL;      /* blank   characters */
  CCHAR   *comment = NULL;      /* comment characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  int     m;                    /* number of attributes/columns */
  int     n;                    /* number of data tuples */
  clock_t t;                    /* timer for measurement */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] [-d|-h hdrfile]", argv[0]);
    printf(" tabfile [outfile]\n");
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
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
    printf("-d       use default header "
                    "(attribute names = field numbers)\n");
    printf("-h       read table header  "
                    "(attribute names) from hdrfile\n");
    printf("hdrfile  file containing table header "
                    "(attribute names)\n");
    printf("tabfile  table file to read "
                    "(attribute names in first record)\n");
    printf("outfile  file to write statistics to (optional)\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 'r': optarg = &recseps;     break;
          case 'f': optarg = &fldseps;     break;
          case 'b': optarg = &blanks;      break;
          case 'u': optarg = &nullchs;     break;
          case 'C': optarg = &comment;     break;
          case 'h': optarg = &fn_hdr;      break;
          default : error(E_OPTION, *--s); break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_tab = s;      break;
        case  1: fn_out = s;      break;
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
  names = sl_create(16*1024);   /* create a string list */
  if (!names) error(E_NOMEM);   /* for the attribute names */
  tread = trd_create();         /* create a table reader and */
  if (!tread) error(E_NOMEM);   /* configure the characters */
  trd_allchs(tread, recseps, fldseps, blanks, nullchs, comment);
  if (fn_hdr) {                 /* if a header file is given */
    t = clock();                /* start timer, open input file */
    if (trd_open(tread, NULL, fn_hdr) != 0)
      error(E_FOPEN, trd_name(tread));
    fprintf(stderr, "reading %s ... ", trd_name(tread));
    do {                        /* read the attribute names */
      d = trd_read(tread);      /* read the next field */
      if (d <= TRD_ERR) error(E_FREAD, trd_name(tread));
      if (d <= TRD_EOF) break;  /* check for error and end of file */
      s = trd_field(trd);       /* add attribute name to the list */
      i = sl_add(names, (s[0] == '\0') ? "-" : s);
    } while (d != TRD_REC);     /* while there is another field */
    trd_close(tread);           /* read table header, close file */
    fprintf(stderr, "[%d attribute(s)]", sl_cnt(names));
    fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
  }
  t = clock();                  /* start timer, open input file */
  if (trd_open(tread, NULL, fn_tab) != 0)
    error(E_FOPEN, trd_name(tread));
  fprintf(stderr, "reading %s ... ", trd_name(tread));
  if (!fn_hdr) {                /* if to read header from table file */
    do {                        /* read the attribute names */
      d = trd_read(tread);      /* read the next field */
      if (d <= TRD_ERR) error(E_FREAD, trd_name(tread));
      if (d <= TRD_EOF) break;  /* check for error and end of file */
      s = trd_field(trd);       /* get the attribute name */
      if (sl_add(names, (s[0] == '\0') ? "-" : s) != 0)
        error(E_NOMEM);         /* add attribute name to the list */
    } while (d != TRD_REC);     /* while there is another field */
  }
  n = sl_cnt(names);            /* get the number of attributes */



  m = 0;                        /* initialize the record counter */



    do {                        /* read the attribute names */
      d = trd_read(tread);      /* read the next field */
      if (d <= TRD_ERR) error(E_FREAD, trd_name(tread));
      if (d <= TRD_EOF) break;  /* check for error and end of file */
      s = trd_field(trd);       /* get the attribute name */
      if (sl_add(names, (s[0] == '\0') ? "-" : s) != 0)
        error(E_NOMEM);         /* add attribute name to the list */
    } while (d != TRD_REC);     /* while there is another field */


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






  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
