/*----------------------------------------------------------------------
  File    : inulls.c
  Contents: insert null values into a table
  Authors : Christian Borgelt
  History : 2002.12.20 file created
            2003.01.18 option -x extended, options -i, -w added
            2003.08.16 slight changes in error message output
            2007.02.13 adapted to redesigned module attset
            2010.10.08 adapted to modules tabread and tabwrite
            2010.12.12 adapted to a generic error reporting function
            2011.01.18 adapted to functions tab_read() and tab_write()
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
#include "arrays.h"
#include "table.h"
#include "random.h"
#include "error.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "inulls"
#define DESCRIPTION "insert null values into a table"
#define VERSION     "version 2.1 (2014.10.24)         " \
                    "(c) 2002-2014   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */
#define E_PERCENT   (-9)        /* invalid percentage */
#define E_INEX     (-10)        /* both include and exclude used */
#define E_UNKATT   (-11)        /* unknown attribute */
#define E_NOCOLS   (-12)        /* no columns to work on */

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
  /* E_PERCENT  -9 */  "invalid percentage %g",
  /* E_INEX    -10 */  "both include and exclude used",
  /* E_UNKATT  -11 */  "unknown attribute '%s'",
  /* E_NOCOLS  -12 */  "no columns to work on",
  /*           -13 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR    *prgname;       /* program name for error messages */
static TABREAD  *tread  = NULL; /* table reader */
static TABWRITE *twrite = NULL; /* table writer */
static ATTSET   *attset = NULL; /* attribute set */
static TABLE    *table  = NULL; /* data table */
static ATTID    *map    = NULL; /* column map */
static INST     **insts = NULL; /* instance vector */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (insts)  free(insts); \
  if (map)    free(map);   \
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
  char    *s;                   /* to traverse options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_hdr  = NULL;      /* name of header file */
  CCHAR   *fn_tab  = NULL;      /* name of table  file */
  CCHAR   *fn_out  = NULL;      /* name of output file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blank      characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  int     clude    = 0;         /* flag for attribute in-/exclusion */
  int     mode     = AS_ATT;    /* table file read  mode */
  int     mout     = AS_ATT;    /* table file write mode */
  double  percent  = 10.0;      /* percent null values */
  int     csvmem   = 0;         /* flag for memory conservation */
  long    seed;                 /* random number seed */
  TUPLE   *tpl;                 /* to traverse the tuples */
  INST    *inst;                /* instance of an attribute */
  size_t  nvcnt, j;             /* number of null values */
  ATTID   m, c, x, z = 0;       /* number of table columns */
  TPLID   n, r;                 /* number of data tuples */
  double  w;                    /* weight of data tuples */
  clock_t t;                    /* timer for measurements */

  prgname = argv[0];            /* get program name for error msgs. */
  seed    = (long)time(NULL);   /* and get a default seed value */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] infile outfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-i#      name of attribute to include "
                    "(multiple attributes possible)\n");
    printf("-x#      name of attribute to exclude "
                    "(multiple attributes possible)\n");
    printf("-p#      percentage of null values to insert    "
                    "(default: %g%%)\n", percent);
    printf("-s#      seed value for random number generator "
                    "(default: time)\n");
    printf("-m       conserve memory (may slow down operation)\n");
    printf("-a       align fields in output table           "
                    "(default: do not align)\n");
    printf("-w       do not write attribute names to output file\n");
    printf("-r#      record     separators                  "
                    "(default: \"\\n\")\n");
    printf("-f#      field      separators                  "
                    "(default: \" \\t\")\n");
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
    printf("infile   table file to read "
                    "(attribute names in first record)\n");
    printf("outfile  table file to write\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case 'i': if (clude < 0) error(E_INEX);
                    optarg  = (CCHAR**)(argv +z++);
                    clude   = +1;               break;
          case 'x': if (clude > 0) error(E_INEX);
                    optarg  = (CCHAR**)(argv +z++);
                    clude   = -1;               break;
          case 'p': percent = strtod(s, &s);    break;
          case 's': seed    = strtol(s, &s, 0); break;
          case 'm': csvmem  = 1;                break;
          case 'a': mout   |= AS_ALIGN;         break;
          case 'w': mout   &= ~AS_ATT;          break;
          case 'r': optarg  = &recseps;         break;
          case 'f': optarg  = &fldseps;         break;
          case 'b': optarg  = &blanks;          break;
          case 'u': optarg  = &nullchs;         break;
          case 'C': optarg  = &comment;         break;
          case 'n': mode   |= AS_WEIGHT;        break;
          case 'd': mode   |= AS_DFLT;          break;
          case 'h': optarg  = &fn_hdr;          break;
          default : error(E_OPTION, *--s);      break;
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
  if ((percent < 0) || (percent > 100))
    error(E_PERCENT, percent);  /* check the percentage */
  if (fn_hdr && (strcmp(fn_hdr, "-") == 0))
    fn_hdr = "";                /* convert "-" to "" for consistency */
  i = ( fn_hdr && !*fn_hdr) ? 1 : 0;
  if  (!fn_tab || !*fn_tab) i++;/* check assignments of stdin: */
  if (i > 1) error(E_STDIN);    /* stdin must not be used twice */
  if ((mout & AS_ALIGN) && (mout & AS_ATT))
    mout |= AS_ALNHDR;          /* set align to header flag */
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
  n = tab_tplcnt(table);        /* get the number of tuples */
  w = tab_tplwgt(table);        /* and print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", tab_attcnt(table));
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- find columns to work on --- */
  t = clock();                  /* start timer, print log message */
  fprintf(stderr, "inserting null values ... ");
  m   = tab_colcnt(table);      /* get the number of columns */
  map = (ATTID*)malloc((size_t)m *sizeof(ATTID));
  if (!map) error(E_NOMEM);     /* create a column map */
  if      (clude == 0) {        /* if to work on all columns */
    for (c = z = m; --c >= 0; ) map[c] = c; }
  else {                        /* if to in-/exclude certain columns */
    k = (clude > 0) ? 0 : 1;    /* initialize the column map */
    for (c = m; --c >= 0; ) map[c] = (ATTID)k;
    for (c = z; --c >= 0; ) {   /* traverse the attribute names */
      x = as_attid(attset, argv[c]);
      if (x < 0) error(E_UNKATT, argv[c]);
      map[x] = (ATTID)clude;    /* mark/unmark the columns */
    }                           /* to include or exclude */
    for (z = c = 0; c < m; c++) /* collect the column numbers */
      if (map[c] > 0) map[z++] = c;
  }                             /* create a column map */
  if (z <= 0) error(E_NOCOLS);  /* check the number of columns */

  /* --- insert null values --- */
  rseed((unsigned)seed);        /* traverse the nulls to insert */
  if (csvmem) {                 /* if to conserve memory */
    nvcnt = (size_t)(0.01 *percent *(double)z *(double)n +0.4999);
    for (j = 0; j < nvcnt; j++){/* traverse the null values */
      do {                      /* table field search loop */
        c = (ATTID)((double)z *drand());
        if (c <  0) c = 0;      /* get a random column index and */
        if (c >= z) c = z-1;    /* clamp it to the interval [0, z-1] */
        r = (TPLID)((double)n *drand());
        if (r <  0) r = 0;      /* get a random row    index and */
        if (r >= n) r = n-1;    /* clamp it to the interval [0, n-1] */
        inst = tpl_colval(tab_tpl(table, r), map[c]);
      } while (inst->n < 0);    /* find a known table field */
      inst->n = NV_NOM;         /* and replace its contents */
    } }                         /* with a null value */
  else {                        /* if to use an instance vector */
    insts = (INST**)malloc((size_t)z *(size_t)n *sizeof(INST*));
    if (!insts) error(E_NOMEM); /* create an instance vector */
    for (j = 0, r = n; --r >= 0; ) {
      tpl = tab_tpl(table, r);  /* traverse the tuples of the table */
      for (c = z; --c >= 0; ) insts[j++] = tpl_colval(tpl, map[c]);
    }                           /* collect the eligible instances */
    ptr_shuffle(insts, (size_t)m, drand);     /* and shuffle them */
    nvcnt = (size_t)(0.01 *percent *(double)m +0.4999);
    if (j < nvcnt) nvcnt = j;   /* compute number of null values */
    for (j = 0; j < nvcnt; j++) /* set the first 'percent' instances */
      insts[j]->n = NV_NOM;     /* to a (nominal) null value */
  }
  fprintf(stderr, "[%"SIZE_FMT" value(s)]", nvcnt);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- write the output table --- */
  twrite = twr_create();        /* create a table writer and */
  if (!twrite) error(E_NOMEM);  /* set the table output characters */
  twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
  t = clock();                  /* start timer, open output file */
  if (twr_open(twrite, NULL, fn_out) != 0)
    error(E_FOPEN, twr_name(twrite));
  fprintf(stderr, "writing %s ... ", twr_name(twrite));
  k = tab_write(table, twrite, mout); /* write the output table */
  if (k) error(-k, tab_errmsg(table, NULL, 0));
  if (twr_close(twrite) != 0) error(E_FWRITE, twr_name(twrite));
  twr_delete(twrite, 1);        /* close the output file and */
  twrite = NULL;                /* delete the table writer */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", tab_attcnt(table));
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
