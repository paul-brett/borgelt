/*----------------------------------------------------------------------
  File    : t1inn.c
  Contents: program to convert nominal attributes to 1-in-n coding
  Author  : Christian Borgelt
  History : 2003.08.11 file created
            2007.01.10 binary attribute treatment made selectable
            2007.02.13 adapted to redesigned module attset
            2010.10.08 adapted to modules tabread and tabwrite
            2010.12.12 adapted to a generic error reporting function
            2011.01.14 adapted to functions tab_read() and tab_write()
            2013.07.21 adapted to definitions ATTID, VALID, WEIGHT etc.
            2014.10.24 changed from LGPL license to MIT license
            2015.07.22 naming of 1-in-n columns improved
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#ifndef AS_PARSE
#define AS_PARSE
#endif
#ifndef TAB_READ
#define TAB_READ
#endif
#ifndef TAB_WRITE
#define TAB_WRITE
#endif
#include "attmap.h"
#include "table.h"
#include "error.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "t1inn"
#define DESCRIPTION "convert nominal attributes to 1-in-n coding"
#define VERSION     "version 2.2 (2015.07.22)         " \
                    "(c) 2003-2015   Christian Borgelt"

/* --- error codes --- */
/* error codes 0 to -5 defined in attset.h */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* wrong number of arguments */
#define E_PARSE     (-9)        /* parse error(s) */

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
  /* E_PARSE    -9 */  "parse error(s) on file %s",
  /*           -10 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR    *prgname;         /* program name for error messages */
static SCANNER  *scan   = NULL;   /* scanner (for domain file) */
static TABREAD  *tread  = NULL;   /* table reader */
static TABWRITE *twrite = NULL;   /* table writer */
static ATTSET   *attset = NULL;   /* attribute set */
static ATTMAP   *attmap = NULL;   /* attribute map */
static TABLE    *table  = NULL;   /* data table */
static double   *vec    = NULL;   /* vector of mapped values */

/*----------------------------------------------------------------------
  Main Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (vec)    free(vec);             \
  if (table)  tab_delete(table,  0); \
  if (attmap) am_delete(attmap,  0); \
  if (attset) as_delete(attset);     \
  if (tread)  trd_delete(tread,  1); \
  if (twrite) twr_delete(twrite, 1); \
  if (scan)   scn_delete(scan,   1);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counter */
  char    *s;                   /* to traverse the options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_dom  = NULL;      /* name of domain file */
  CCHAR   *fn_hdr  = NULL;      /* name of header file */
  CCHAR   *fn_tab  = NULL;      /* name of table  file */
  CCHAR   *fn_out  = NULL;      /* name of output file */
  CCHAR   *recseps = NULL;      /* record     separators */
  CCHAR   *fldseps = NULL;      /* field      separators */
  CCHAR   *blanks  = NULL;      /* blanks     characters */
  CCHAR   *nullchs = NULL;      /* null value characters */
  CCHAR   *comment = NULL;      /* comment    characters */
  int     mode     = AS_ATT|AS_NOXATT; /* table file read  mode */
  int     mout     = AS_ATT;    /* table file write mode */
  int     matt     = 0;         /* mode for attribute map */
  double  ind      = 1.0;       /* indicator value for 1-in-n */
  int     nullout  = '?';       /* null character for output */
  int     digits   = 6;         /* number of significant digits */
  CCHAR   *name, *val;          /* attribute (value) name */
  TUPLE   *tpl;                 /* to traverse the tuples */
  ATTID   m, c, x;              /* number of attributes/columns */
  TPLID   n, r;                 /* number of data tuples */
  double  w;                    /* weight of data tuples */
  clock_t t;                    /* timer for measurement */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print startup/usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no argument is given */
    printf("usage: %s [options] domfile [-d|-h hdrfile] "
                     "infile outfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-i       indicator value for 1-in-n encoding    "
                    "(default: %g)\n", ind);
    printf("-2       use two output columns for binary attributes\n");
    printf("-o#      number of significant digits to print  "
                    "(default: %d)\n", digits);
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
    printf("domfile  file containing domain descriptions\n");
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
  for (i = 1; i < argc; i++) {  /* traverse the arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate option */
          case '2': matt  |= AM_BIN2COL;            break;
          case 'o': digits = (int)strtol(s, &s, 0); break;
          case 'w': mout  &= ~AS_ATT;               break;
          case 'r': optarg = &recseps;              break;
          case 'f': optarg = &fldseps;              break;
          case 'b': optarg = &blanks;               break;
          case 'u': optarg = &nullchs;              break;
          case 'C': optarg = &comment;              break;
          case 'n': mout  |= AS_WEIGHT;
                    mode  |= AS_WEIGHT;             break;
          case 'd': mode  |= AS_DFLT;               break;
          case 'h': optarg = &fn_hdr;               break;
          default : error(E_OPTION, *--s);          break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-option */
        case  0: fn_dom = s;      break;
        case  1: fn_tab = s;      break;
        case  2: fn_out = s;      break;
        default: error(E_ARGCNT); break;
      }                         /* note filenames */
    }
  }
  if (optarg) error(E_OPTARG);  /* check option argument */
  if (k != 3) error(E_ARGCNT);  /* check number of arguments */
  if (fn_hdr && (strcmp(fn_hdr, "-") == 0))
    fn_hdr = "";                /* convert "-" to "" for consistency */
  i = ( fn_hdr && !*fn_hdr) ? 1 : 0;
  if  (!fn_dom || !*fn_dom) i++;
  if  (!fn_tab || !*fn_tab) i++;/* check assignments of stdin: */
  if (i > 1) error(E_STDIN);    /* stdin must not be used twice */
  if ((mout & AS_ALIGN) && (mout & AS_ATT))
    mout |= AS_ALNHDR;          /* set align-to-header flag */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- parse domain descriptions --- */
  attset = as_create("domains", att_delete);
  if (!attset) error(E_NOMEM);  /* create an attribute set */
  scan = scn_create();          /* create a scanner */
  if (!scan)   error(E_NOMEM);  /* for the domain file */
  t = clock();                  /* start timer, open intput file */
  if (scn_open(scan, NULL, fn_dom) != 0)
    error(E_FOPEN, scn_name(scan));
  fprintf(stderr, "reading %s ... ", scn_name(scan));
  if ((as_parse(attset, scan, AT_ALL, 0) != 0)
  ||  !scn_eof(scan, 1))        /* parse domain descriptions */
    error(E_PARSE, scn_name(scan));
  scn_delete(scan, 1);          /* delete the scanner and */
  scan = NULL;                  /* print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s)]", as_attcnt(attset));
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- read data table --- */
  tread = trd_create();         /* create a table reader and */
  if (!tread) error(E_NOMEM);   /* configure the characters */
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
  w = tab_tplwgt(table);        /* and print a success message */
  fprintf(stderr, "[%"ATTID_FMT" attribute(s),", m);
  fprintf(stderr, " %"TPLID_FMT, n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- write output file --- */
  twrite = twr_create();        /* create a table writer and */
  if (!twrite) error(E_NOMEM);  /* set the table output characters */
  twr_xchars(twrite, recseps, fldseps, blanks, nullchs);
  if (nullchs && *nullchs) nullout = *nullchs;
  t = clock();                  /* start timer, open output file */
  if (twr_open(twrite, NULL, fn_out) != 0)
    error(E_FOPEN, twr_name(twrite));
  fprintf(stderr, "writing %s ... ", twr_name(twrite));
  attmap = am_create(attset, matt, ind);
  if (!attmap) error(E_NOMEM);  /* create an attribute map and */
  if (mout & AS_ATT) {          /* if to write table header */
    for (c = 0; c < am_attcnt(attmap); c++) {
      if (c > 0) twr_fldsep(twrite); /* print a separator */
      name = att_name(am_att(attmap, c));
      m = am_cnt(attmap, c);    /* get att. name and column counter */
      if (m <= 1) {             /* single column: print only name */
        twr_puts(twrite, name); continue; }
      for (x = 1; x <= m; x++){ /* multiple columns: */
        if (x > 1) twr_fldsep(twrite);
        val = att_valname(am_att(attmap, c), x-1);
        twr_printf(twrite, "%s_%s", name, val);
      }                         /* print a separator and */
    }                           /* name and column counter */
    if (mout & AS_WEIGHT) {     /* print a weight indicator */
      twr_fldsep(twrite); twr_putc(twrite, '#'); }
    twr_recsep(twrite);         /* terminate the output line */
  }                             /* (write a table header) */
  m   = am_dim(attmap);         /* get the number of output fields */
  vec = (double*)malloc((size_t)m *sizeof(double));
  if (!vec) error(E_NOMEM);     /* create the output vector */
  for (r = 0; r < n; r++) {     /* traverse the data tuples */
    tpl = tab_tpl(table, r);    /* get the next data tuple and */
    tpl_toas(tpl);              /* copy it to the attribute set */
    am_exec(attmap, NULL, AM_INPUTS, vec);
    for (c = 0; c < m; c++) {   /* execute the attribute map */
      if (c > 0) twr_fldsep(twrite);
      if (isnan(vec[c])) twr_putc(twrite, nullout);
      else twr_printf(twrite, "%.*g", digits, vec[c]);
    }                           /* print the vector element */
    if (mout & AS_WEIGHT) {     /* print a weight indicator */
      twr_fldsep(twrite); twr_printf(twrite, "%g", tpl_getwgt(tpl)); }
    twr_recsep(twrite);         /* terminate the output line */
  }                             /* (write the output tuple) */
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
