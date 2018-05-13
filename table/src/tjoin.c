/*----------------------------------------------------------------------
  File    : tjoin.c
  Contents: program to join two tables
  Author  : Christian Borgelt
  History : 1999.02.02 file created
            1999.02.05 first version completed
            1999.02.07 attribute name options (-1,-2) added
            1999.02.12 default header handling improved
            2001.07.14 adapted to modified module tabread
            2002.02.24 check for multiple assignment of stdin added
            2007.02.13 adapted to redesigned module attset
            2010.10.08 adapted to modules tabread and tabwrite
            2010.12.12 adapted to a generic error reporting function
            2011.01.11 adapted to functions tab_read() and tab_write()
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
#define PRGNAME     "tjoin"
#define DESCRIPTION "join two tables " \
                    "(w.r.t. selected attributes/columns)"
#define VERSION     "version 2.1 (2014.10.24)         " \
                    "(c) 1999-2014   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */
#define E_JOINCNT   (-9)        /* number of join columns differs */
#define E_UNKATT   (-10)        /* unknown join attribute */
#define E_DUPNJA   (-11)        /* duplicate non-join attribute */

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
  /* E_JOINCNT  -9 */  "number of join columns differ",
  /* E_UNKATT  -10 */  "unknown join attribute '%s'",
  /* E_DUPNJF  -11 */  "duplicate non-join attribute '%s'",
  /*           -12 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR    *prgname;       /* program name for error messages */
static TABREAD  *tread  = NULL; /* table reader */
static TABWRITE *twrite = NULL; /* table writer */
static CCHAR    **names = NULL; /* attribute names array */
static ATTID    *cis[2]     = { NULL, NULL };  /* column index arrays */
static ATTSET   *attsets[2] = { NULL, NULL };  /* attribute sets */
static TABLE    *tables [2] = { NULL, NULL };  /* data tables */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (cis[0])     free(cis[0]); \
  if (tables[0])  tab_delete(tables[0], 0); \
  if (tables[1])  tab_delete(tables[1], 0); \
  if (attsets[0]) as_delete(attsets[0]); \
  if (attsets[1]) as_delete(attsets[1]); \
  if (tread)      trd_delete(tread,  1); \
  if (twrite)     twr_delete(twrite, 1); \
  if (names)      free(names);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counters */
  char    *s;                   /* to traverse options */
  CCHAR   **optarg   = NULL;    /* option argument */
  CCHAR   *fn_hdr[2] = { NULL, NULL };
  CCHAR   *fn_tab[2] = { NULL, NULL };
  CCHAR   *fn_out    = NULL;    /* names of input and output files */
  CCHAR   *recseps   = NULL;    /* record     separators */
  CCHAR   *fldseps   = NULL;    /* field      separators */
  CCHAR   *blanks    = NULL;    /* blank      characters */
  CCHAR   *nullchs   = NULL;    /* null value characters */
  CCHAR   *comment   = NULL;    /* comment    characters */
  int     mode[2]    = { AS_ATT, AS_ATT };
  int     mout       = AS_ATT;  /* table file read and write modes */
  int     xprod      = 0;       /* flag for crosss product */
  ATTID   c = 0, d = 0, x, m;   /* number of (join) columns */
  TPLID   n;                    /* number of data tuples */
  double  w;                    /* weight of data tuples */
  clock_t t;                    /* timer for measurements */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] [-d|-h hdr1] in1 "
                     "[-d|-h hdr2] in2 out\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-1#      join attribute/column in table 1 "
                    "(may appear several times)\n");
    printf("-2#      join attribute/column in table 2 "
                    "(may appear several times)\n");
    printf("-x       compute cross product                  "
                    "(default: natural join)\n");
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
    printf("hdr1/2   files containing table headers "
                    "(attribute names)\n");
    printf("in1/2    table files to read "
                    "(attribute names in first record)\n");
    printf("out      file to write the joined tables to\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  names = (CCHAR**)malloc((size_t)(argc-1) *sizeof(CCHAR*));
  if (!names) error(E_NOMEM);   /* allocate an attribute names array */
  for (i = 1; i < argc; i++) {  /* traverse the arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case '1': optarg = (CCHAR**)(argv +c++);      break;
          case '2': optarg =          names +d++;       break;
          case 'x': xprod  = 1;                         break;
          case 'a': mout  |= AS_ALIGN;                  break;
          case 'w': mout  &= ~AS_ATT;                   break;
          case 'r': optarg = &recseps;                  break;
          case 'f': optarg = &fldseps;                  break;
          case 'b': optarg = &blanks;                   break;
          case 'C': optarg = &comment;                  break;
          case 'u': optarg = &nullchs;                  break;
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
      switch (k++) {            /* evaluate non-option */
        case  0: fn_tab[0] = s;   break;
        case  1: fn_tab[1] = s;   break;
        case  2: fn_out    = s;   break;
        default: error(E_ARGCNT); break;
      }                         /* note filenames */
    }
  }
  if (optarg) error(E_OPTARG);  /* check option argument */
  if (k != 3) error(E_ARGCNT);  /* check number of arguments */
  if (c != d) error(E_JOINCNT); /* check number of join columns */
  if (fn_hdr[0] && (strcmp(fn_hdr[0], "-") == 0))
    fn_hdr[0] = "";             /* convert "-" to "" for consistency */
  if (fn_hdr[1] && (strcmp(fn_hdr[1], "-") == 0))
    fn_hdr[1] = "";             /* convert "-" to "" for consistency */
  k = 0;                        /* count the empty file names */
  for (i = 0; i < 2; i++) { if ( fn_hdr[i] && !*fn_hdr[i]) k++; }
  for (i = 0; i < 2; i++) { if (!fn_tab[i] || !*fn_tab[i]) k++; }
  if (k > 1) error(E_STDIN);    /* stdin must not be used twice */
  if ((mout & AS_ALIGN) && (mout & AS_ATT))
    mout |= AS_ALNHDR;          /* set align-to-header flag */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- read input tables --- */
  attsets[0] = as_create("domains_0", att_delete);
  attsets[1] = as_create("domains_1", att_delete);
  if (!attsets[0] || !attsets[1]) error(E_NOMEM);
  tables[0]  = tab_create("table_0", attsets[0], tpl_delete);
  tables[1]  = tab_create("table_1", attsets[1], tpl_delete);
  if (!tables[0]  || !tables[1])  error(E_NOMEM);
  tread      = trd_create();    /* create two att. sets, tables, and */
  if (!tread) error(E_NOMEM);   /* a table reader and set characters */
  trd_allchs(tread, recseps, fldseps, blanks, nullchs, comment);
  for (i = 0; i < 2; i++) {     /* traverse the data tables */
    if (fn_hdr[i]) {            /* if a header file is given */
      t = clock();              /* start timer, open input file */
      if (trd_open(tread, NULL, fn_hdr[i]) != 0)
        error(E_FOPEN, trd_name(tread));
      fprintf(stderr, "reading %s ... ", trd_name(tread));
      k = as_read(attsets[i], tread, (mode[i] & ~AS_DFLT) | AS_ATT);
      if (k < 0) error(-k, as_errmsg(attsets[i], NULL, 0));
      trd_close(tread);         /* read table header, close file */
      fprintf(stderr, "[%"ATTID_FMT" attribute(s)]",
                      as_attcnt(attsets[i]));
      fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
      mode[i] &= ~(AS_ATT|AS_DFLT);
    }                           /* remove the attribute flag */
    t = clock();                /* start timer, open input file */
    if (trd_open(tread, NULL, fn_tab[i]) != 0)
      error(E_FOPEN, trd_name(tread));
    fprintf(stderr, "reading %s ... ", trd_name(tread));
    k = tab_read(tables[i], tread, mode[i]);
    if (k < 0) error(-k, tab_errmsg(tables[i], NULL, 0));
    trd_close(tread);           /* read the first data table */
    n = tab_tplcnt(tables[i]);  /* and close the input file */
    w = tab_tplwgt(tables[i]);  /* get the number of tuples */
    fprintf(stderr, "[%"ATTID_FMT" attribute(s),",
                    tab_attcnt(tables[i]));
    fprintf(stderr, " %"TPLID_FMT, n);
    if (w != (double)n) fprintf(stderr, "/%g", w);
    fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));
  }                             /* print a success message */
  trd_delete(tread, 1);         /* delete the table reader and */
  tread = NULL;                 /* clear the reader variable */

  /* --- join input tables --- */
  t = clock();                  /* start timer, print log message */
  fprintf(stderr, "joining tables ... ");
  if (c <= 0) {                 /* if to do a natural join */
    if (!xprod) c = -1; }       /* or to compute a cross product */
  else {                        /* if to do a normal join */
    cis[0] = (ATTID*)malloc((size_t)(c+c) *sizeof(ATTID));
    if (!cis[0]) error(E_NOMEM);/* allocate a column index array */
    cis[1] = cis[0] +c;         /* and organize it (2 in 1) */
    for (x = 0; x < c; x++) {   /* traverse join column names */
      cis[0][x] = m = as_attid(attsets[0], argv [x]);
      if (m < 0) error(E_UNKATT, argv [x]);
      cis[1][x] = m = as_attid(attsets[1], names[x]);
      if (m < 0) error(E_UNKATT, names[x]);
    }                           /* build column index arrays */
  }                             /* for source and destination */
  m = tab_join(tables[0], tables[1], cis[0], cis[1], c);
  if (m < 0) error(E_NOMEM);    /* join the two tables */
  if (m > 0) error(E_DUPNJA, att_name(as_att(attsets[1], m-1)));
  fprintf(stderr, "done [%.2fs].\n", SEC_SINCE(t));

  /* --- write output table --- */
  twrite = twr_create();        /* create a table writer and */
  if (!twrite) error(E_NOMEM);  /* configure the characters */
  twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
  t = clock();                  /* start timer, open output file */
  if (twr_open(twrite, NULL, fn_out) != 0)
    error(E_FOPEN, twr_name(twrite));
  fprintf(stderr, "writing %s ... ", twr_name(twrite));
  k = tab_write(tables[0], twrite, mout);
  if (k) error(E_FWRITE, twr_name(twrite));
  if (twr_close(twrite) != 0) error(E_FWRITE, twr_name(twrite));
  twr_delete(twrite, 1);        /* close the output file and */
  twrite = NULL;                /* delete the table writer */
  n = tab_tplcnt(tables[0]);    /* get the number of tuples */
  w = tab_tplwgt(tables[0]);    /* and print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", tab_attcnt(tables[0]));
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
