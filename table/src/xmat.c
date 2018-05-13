/*----------------------------------------------------------------------
  File    : xmat.c
  Contents: determine a confusion matrix for two table columns
  Author  : Christian Borgelt
  History : 1998.01.04 file created
            1998.01.10 first version completed
            1998.06.20 adapted to modified st_create() function
            1998.08.03 output format of numbers improved
            1999.02.07 input from stdin, output to stdout added
            1999.02.12 default header handling improved
            1999.03.27 bugs in matrix (re)allocation removed
            2001.07.14 adapted to modified module tabread
            2003.08.16 slight changes in error message output
            2004.04.23 optional check of column permutations added
            2006.10.06 adapted to improved function trd_next()
            2007.02.13 adapted to modified module tabread
            2010.10.08 adapted to modified module tabread
            2010.12.12 adapted to a generic error reporting function
            2011.02.03 optimized using functions memset() and memcpy()
            2011.02.04 permutation check executed with Hungarian method
            2011.07.26 adapted to modified interface of st_create()
            2013.06.12 some bugs in table reading fixed() (malloc)
            2013.08.31 optional suppression of empty columns added
            2014.10.24 changed from LGPL license to MIT license
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <time.h>
#include <float.h>
#include <math.h>
#include "symtab.h"
#include "tabread.h"
#include "error.h"
#ifdef STORAGE
#include "storage.h"
#endif

#ifndef INFINITY
#define INFINITY   (DBL_MAX+DBL_MAX)
#endif                          /* MSC still does not support C99 */

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define PRGNAME     "xmat"
#define DESCRIPTION "determine a confusion matrix"
#define VERSION     "version 2.1 (2014.10.24)         " \
                    "(c) 1998-2014   Christian Borgelt"

/* --- sizes --- */
#define BLKSIZE      32         /* block size for value array */

/* --- error codes --- */
#define E_NONE        0         /* no error */
#define E_NOMEM     (-1)        /* not enough memory */
#define E_FOPEN     (-2)        /* cannot open file */
#define E_FREAD     (-3)        /* read error on file */
#define E_FWRITE    (-4)        /* write error on file */
#define E_STDIN     (-5)        /* double assignment of stdin */
#define E_OPTION    (-6)        /* unknown option */
#define E_OPTARG    (-7)        /* missing option argument */
#define E_ARGCNT    (-8)        /* too few/many arguments */
#define E_UNKATT    (-9)        /* unknown attribute */
#define E_EMPATT   (-10)        /* empty attribute name */
#define E_DUPATT   (-11)        /* duplicate attribute */
#define E_FLDCNT   (-12)        /* wrong number of fields */
#define E_WEIGHT   (-13)        /* invalid tuple weight */

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
  /* E_UNKATT   -9 */  "unknown attribute '%s'",
  /* E_EMPATT  -10 */  "%s:%"SIZE_FMT"(%"SIZE_FMT"): "
                         "empty attribute name",
  /* E_DUPATT  -11 */  "%s:%"SIZE_FMT"(%"SIZE_FMT"): "
                         "duplicate attribute '%s'",
  /* E_FLDCNT  -12 */  "%s:%"SIZE_FMT"(%"SIZE_FMT"): "
                         "wrong number of fields/columns",
  /* E_WEIGHT  -13 */  "%s:%"SIZE_FMT"(%"SIZE_FMT"): "
                         "invalid tuple weight '%s'",
  /*           -14 */  "unknown error"
};

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
static CCHAR   *prgname;        /* program name for error messages */
static TABREAD *tread  = NULL;  /* table reader */
static SYMTAB  *symtab = NULL;  /* symbol table (for values) */
static IDENT   **vals  = NULL;  /* value array (symtab entries) */
static IDENT   size    = 0;     /* size of the value array */
static IDENT   cnt     = 0;     /* current number of values */
static double  **xmat  = NULL;  /* confusion matrix */
static double  **ccm   = NULL;  /* corresponding cost matrix */
static IDENT   *map    = NULL;  /* permutation map (and inverse) */
static FILE    *out    = NULL;  /* output file */

/*----------------------------------------------------------------------
  Auxiliary Functions
----------------------------------------------------------------------*/
#ifndef NDEBUG

static void xm_delete (void)
{                               /* --- delete confusion matrix */
  IDENT i;                      /* loop variable */
  for (i = size+1; --i >= 0; )  /* traverse the matrix columns */
    if (xmat[i]) free(xmat[i]); /* and delete them */
  free(xmat);                   /* delete the column array */
}  /* xm_delete() */

#endif
/*--------------------------------------------------------------------*/

static int valcmp (const void *p1, const void *p2)
{                               /* --- compare values */
  return strcmp(st_name(*(const void**)p1), st_name(*(const void**)p2));
}  /* valcmp() */

/*----------------------------------------------------------------------
  Main Functions
----------------------------------------------------------------------*/

#ifndef NDEBUG                  /* if debug version */
  #undef  CLEANUP               /* clean up memory and close files */
  #define CLEANUP \
  if (ccm && *ccm) free(*ccm); \
  if (map)    free(map);   \
  if (xmat)   xm_delete(); \
  if (vals)   free(vals);  \
  if (symtab) st_delete(symtab); \
  if (tread)  trd_delete(tread, 1); \
  if (out && (out != stdout)) fclose(out);
#endif

GENERROR(error, exit)           /* generic error reporting function */

/*--------------------------------------------------------------------*/

int main (int argc, char *argv[])
{                               /* --- main function */
  int     i, k = 0;             /* loop variables, counter */
  char    *s, *e;               /* to traverse options */
  CCHAR   **optarg = NULL;      /* option argument */
  CCHAR   *fn_hdr  = NULL;      /* name of header file */
  CCHAR   *fn_tab  = NULL;      /* name of table  file */
  CCHAR   *fn_mat  = NULL;      /* name of matrix file */
  CCHAR   *recseps = NULL;      /* record separators */
  CCHAR   *fldseps = NULL;      /* field  separators */
  CCHAR   *blanks  = NULL;      /* blanks */
  CCHAR   *comment = NULL;      /* comment characters */
  CCHAR   *xname   = NULL;      /* name for x-direction */
  CCHAR   *yname   = NULL;      /* name for y-direction */
  int     hdflt    = 0;         /* flag for a default header */
  int     wgtfld   = 0;         /* flag for weight in last field */
  int     relnum   = 0;         /* flag for relative numbers */
  int     sort     = 0;         /* flag for sorted values */
  int     perm     = 0;         /* flag for column permutations */
  int     nozero   = 0;         /* flag for suppressing empty columns */
  size_t  max      = 6, l;      /* (maximal) length of a value name */
  void    *q;                   /* temporary buffer (reallocation) */
  IDENT   *px, *py, *p;         /* to access the symbol data */
  double  *col, *sum;           /* to traverse the matrix columns */
  IDENT   *inv, *stk;           /* inverse map and column stack */
  char    buf[32];              /* buffer for default name creation */
  CCHAR   *fmt;                 /* output format for numbers */
  int     d;                    /* delimiter type */
  IDENT   m;                    /* number of attributes */
  IDENT   n;                    /* number of data tuples */
  IDENT   x, y, z, h, j;        /* field indices, loop variables */
  double  w, u, v;              /* weight of data tuples, buffer */
  clock_t t;                    /* timer for measurements */

  prgname = argv[0];            /* get program name for error msgs. */

  /* --- print usage message --- */
  if (argc > 1) {               /* if arguments are given */
    fprintf(stderr, "%s - %s\n", argv[0], DESCRIPTION);
    fprintf(stderr, VERSION); } /* print a startup message */
  else {                        /* if no arguments is given */
    printf("usage: %s [options] [-d|-h hdrfile] "
                     "tabfile matfile\n", argv[0]);
    printf("%s\n", DESCRIPTION);
    printf("%s\n", VERSION);
    printf("-x#      attribute name for the x-direction "
                    "(columns) of the matrix\n");
    printf("-y#      attribute name for the y-direction "
                    "(rows)    of the matrix\n");
    printf("-p       compute relative numbers (in percent)\n");
    printf("-s       sort values alphabetically "
                    "(default: order of appearance)\n");
    printf("-c       find best permutation of the matrix columns\n");
    printf("-z       suppress empty columns\n");
    printf("-n       number of tuple occurrences in last field\n");
    printf("-r#      record  separators                     "
                    "(default: \"\\n\")\n");
    printf("-f#      field   separators                     "
                    "(default: \" \\t,\")\n");
    printf("-b#      blank   characters                     "
                    "(default: \" \\t\\r\")\n");
    printf("-C#      comment characters                     "
                    "(default: \"#\")\n");
    printf("-d       use default header "
                    "(attribute names = field numbers)\n");
    printf("-h       read table header  "
                    "(attribute names) from hdrfile\n");
    printf("hdrfile  file containing table header "
                    "(attribute names)\n");
    printf("tabfile  table file to read "
                    "(attribute names in first record)\n");
    printf("matfile  file to write confusion matrix to\n");
    return 0;                   /* print a usage message */
  }                             /* and abort the program */

  /* --- evaluate arguments --- */
  for (i = 1; i < argc; i++) {  /* traverse arguments */
    s = argv[i];                /* get option argument */
    if (optarg) { *optarg = s; optarg = NULL; continue; }
    if ((*s == '-') && *++s) {  /* -- if argument is an option */
      while (1) {               /* traverse characters */
        switch (*s++) {         /* evaluate options */
          case 'x': optarg = &xname;       break;
          case 'y': optarg = &yname;       break;
          case 'p': relnum = 1;            break;
          case 's': sort   = 1;            break;
          case 'c': perm   = 1;            break;
          case 'z': nozero = 1;            break;
          case 'n': wgtfld = 1;            break;
          case 'r': optarg = &recseps;     break;
          case 'f': optarg = &fldseps;     break;
          case 'C': optarg = &comment;     break;
          case 'b': optarg = &blanks;      break;
          case 'd': hdflt  = 1;            break;
          case 'h': optarg = &fn_hdr;      break;
          default : error(E_OPTION, *--s); break;
        }                       /* set option variables */
        if (!*s) break;         /* if at end of string, abort loop */
        if (optarg) { *optarg = s; optarg = NULL; break; }
      } }                       /* get option argument */
    else {                      /* -- if argument is no option */
      switch (k++) {            /* evaluate non-options */
        case  0: fn_tab = s;      break;
        case  1: fn_mat = s;      break;
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
  if (fn_hdr) hdflt = 0;        /* make header mode consistent */
  fputc('\n', stderr);          /* terminate the startup message */

  /* --- read table header --- */
  symtab = st_create(0, 0, ST_STRFN, (OBJFN*)0);
  if (!symtab) error(E_NOMEM);  /* create a symbol table */
  tread  = trd_create();        /* create table reader and */
  if (!tread)  error(E_NOMEM);  /* configure the characters */
  trd_allchs(tread, recseps, fldseps, blanks, "", comment);
  t = clock();                  /* start timer, open input file */
  if (trd_open(tread, NULL, (fn_hdr) ? fn_hdr : fn_tab) != 0)
    error(E_FOPEN, trd_name(tread));
  fprintf(stderr, "reading %s ... ", trd_name(tread));
  m = 0;                        /* init. the attribute counter */
  do {                          /* read the table header */
    d = trd_read(tread);        /* read the next table field */
    if  (d <= TRD_ERR) error(E_FREAD, trd_name(tread));
    if ((d != TRD_FLD) && wgtfld) break; /* skip tuple weight */
    s = trd_field(tread);       /* get the field buffer and */
    l = sizeof(IDENT);          /* default size of symtab entry */
    if (hdflt) {                /* if to use a default header */
      sprintf(buf, "%"IDENT_FMT, m+1);
      l += strlen(s)+1;         /* create an attribute name */
      p = (IDENT*)st_insert(symtab, buf, -1, strlen(buf)+1, l);
      if (!p) error(E_NOMEM);   /* insert name into symbol table */
      strcpy((char*)(p+1),s); } /* and note the value for later */
    else {                      /* if to use a normal header */
      if (!*s)         error(E_EMPATT, TRD_INFO(tread));
      p = (IDENT*)st_insert(symtab, s, -1, trd_len(tread)+1, l);
      if (!p) error(E_NOMEM);   /* insert name into symbol table */
      if (p == EXISTS) error(E_DUPATT, TRD_INFO(tread));
    }                           /* check for a duplicate name */
    if (m >= size) {            /* if the attribute array is full */
      size += (size > BLKSIZE) ? (size >> 1) : BLKSIZE;
      q = realloc(vals, (size_t)size *sizeof(IDENT*));
      if (!q) error(E_NOMEM);   /* allocate and set a */
      vals = (IDENT**)q;        /* (new) attribute array, */
    }                           /* then insert the name read */
    vals[*p = m++] = p;         /* into the attribute array */
  } while (d == TRD_FLD);       /* while not at end of record */
  if (m <= 0)                   /* check the number of attributes */
    error(E_FLDCNT, TRD_INFO(tread));
  if (fn_hdr) {                 /* if a table header file is given, */
    trd_close(tread);           /* close the header file */
    fprintf(stderr, "[%"IDENT_FMT" attribute(s)]", m);
    fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
  }                             /* print a success message */

  /* --- determine attribute indices --- */
  x = y = -1; px = py = NULL;   /* initialize attribute indices */
  if (xname) {                  /* if name for x-direction given */
    px = st_lookup(symtab, xname, -1);
    if (!px) error(E_UNKATT, xname);
    x = *px;                    /* get the symbol data for the name */
  }                             /* and read the att. index from it */
  if (yname) {                  /* if name for y-direction given */
    py = st_lookup(symtab, yname, -1);
    if (!py) error(E_UNKATT, yname);
    y = *py;                    /* get the symbol data for the name */
  }                             /* and read the att. index from it */
  if (y < 0) y = m -((x >= 0) ? 1 : 2);
  if (y < 0) y = 0;             /* if att. indices have not been */
  if (x < 0) x = m -1;          /* determined, set default indices */
  if (x < 0) x = 0;             /* (last att./last att. but one) */
  xname = (char*)st_name(vals[x]);  /* get names of the atts. to */
  yname = (char*)st_name(vals[y]);  /* compute confusion matrix of */

  /* --- create a confusion matrix --- */
  size = BLKSIZE;               /* get the default matrix size */
  xmat = (double**)calloc((size_t)(size+size+1), sizeof(double*));
  if (!xmat) error(E_NOMEM);    /* allocate the column array */
  for (j = 0; j < size; j++) {  /* and then traverse it */
    xmat[j] = (double*)calloc((size_t)size+2, sizeof(double));
    if (!xmat[j]) error(E_NOMEM);
  }                             /* allocate the matrix columns */

  /* --- compute confusion matrix --- */
  w = 0.0; n = 0;               /* init. the tuple counters */
  if      (fn_hdr) {            /* if a table header file is given */
    t = clock();                /* start timer, open table file */
    if (trd_open(tread, NULL, fn_tab) != 0)
      error(E_FOPEN, trd_name(tread));
    fprintf(stderr, "reading %s ... ", trd_name(tread)); }
  else if (hdflt) {             /* if to use a default header */
    px = vals[x];               /* get the 1st value already read */
    l  = strlen(s = (char*)(px+1));
    if (l > max) max = l;       /* update the maximal name length */
    px = (IDENT*)st_insert(symtab, s, 0, l+1, sizeof(IDENT));
    if (!px) error(E_NOMEM);    /* insert value into symbol table, */
    vals[*px = cnt++] = px;     /* note its name and set its index */
    py = vals[y];               /* get the 2nd value already read */
    l  = strlen(s = (char*)(py+1));
    if (l > max) max = l;       /* update the maximal name length */
    py = (IDENT*)st_insert(symtab, s, 0, l+1, sizeof(IDENT));
    if (!py) error(E_NOMEM);    /* insert value into symbol table */
    if (py == EXISTS) py = px;  /* note its name and set its index */
    else vals[*py = cnt++] = py;
    w = 1.0; n = 1;             /* init. the tuple counters */
    if (wgtfld) {               /* if tuple weight in last field */
      w = strtod(s = trd_field(tread), &e);
      if (*e || (e == s) || (w < 0)) error(E_WEIGHT, TRD_INFO(tread));
    }                           /* convert last field to a number */
    xmat[*px][*py] += w;        /* add the tuple weight to */
  }                             /* the confusion matrix element */
  u = 1.0;                      /* get the default tuple weight */
  s = trd_field(tread);         /* and retrieve the field buffer */
  while (1) {                   /* read the table records */
    for (j = 0, d = TRD_FLD; d == TRD_FLD; j++) {
      d = trd_read(tread);      /* read the next field */
      if (d <= TRD_ERR)  error(E_FREAD,  trd_name(tread));
      if (d <= TRD_EOF)  break; /* if error or end of file, abort */
      if ((j != x) && (j != y)) /* skip all irrelevant fields */
        continue;               /* (other than matrix dimensions) */
      s = trd_field(tread);     /* get the field buffer */
      p = (IDENT*)(q = st_lookup(symtab, s, 0));
      if (!p) {                 /* look up the value name */
        p = (IDENT*)st_insert(symtab, s, 0, trd_len(tread)+1,
                              sizeof(IDENT));
        if (!p) error(E_NOMEM); /* if the value is not yet known, */
      }                         /* insert it into the symbol table */
      if (j == x) px = p;       /* note pointers to the */
      if (j == y) py = p;       /* corresponding symbol data */
      if (q) continue;          /* skip known values */
      if ((l = strlen(s)) > max)
        max = l;                /* update the maximal name length */
      if (cnt >= size) {        /* if the value array is full */
        z = size +((size > BLKSIZE) ? size >> 1 : BLKSIZE);
        q = realloc(vals, (size_t)z *sizeof(IDENT*));
        if (!q) error(E_NOMEM); /* allocate and set */
        vals = (IDENT**)q;      /* a new value array */
        q = realloc(xmat, (size_t)(z+z+1) *sizeof(double*));
        if (!q) error(E_NOMEM); /* allocate and set */
        xmat = (double**)q;     /* a new column array */
        memset(xmat+cnt, 0, (size_t)(z-cnt) *sizeof(double*));
        l = (size_t)(z+2) *sizeof(double);
        for (size = z; --z >= 0; ) {
          col = (double*)realloc(xmat[z], l);
          if (!col) error(E_NOMEM);
          xmat[z] = col;        /* (re)allocate matrix column */
          if (z >= cnt)         /* and set the new column */
               memset(col,     0, (size_t) size      *sizeof(double));
          else memset(col+cnt, 0, (size_t)(size-cnt) *sizeof(double));
        }                       /* clear (new part of) the */
      }                         /* allocated matrix column */
      vals[*p = cnt++] = p;     /* note the symbol data pointer */
    }                           /* for later name output */
    if ((j != 0) && (j != m+wgtfld)) error(E_FLDCNT, TRD_INFO(tread));
    if (d <= TRD_EOF) break;    /* if at end of file, abort loop */
    if (wgtfld) {               /* if tuple weight in last field */
      u = strtod(s, &e);        /* convert last field to a number */
      if (*e || (e == s) || (u < 0)) error(E_WEIGHT, TRD_INFO(tread));
    }                           /* (get the tuple weight) */
    xmat[*px][*py] += u;        /* add weight to confusion matrix */
    w += u; n++;                /* and to the total tuple weight */
  }                             /* and count the tuple */
  trd_delete(tread, 1);         /* close the input file and */
  tread = NULL;                 /* delete the table reader */
  fprintf(stderr, "[%"IDENT_FMT" attribute(s),", m);
  fprintf(stderr, " %"IDENT_FMT"", n);
  if (w != (double)n) fprintf(stderr, "/%g", w);
  fprintf(stderr, " tuple(s)] done [%.2fs].\n", SEC_SINCE(t));

  /* --- check column permutations --- */
  inv = NULL;                   /* to prevent compiler warnings */
  if (perm) {                   /* if to check column permutations */
    fprintf(stderr, "checking column permutations ... ");
    /* The best column permutation is not determined by actually */
    /* checking all column permutations, but rather by using the */
    /* so-called Hungarian method to find a bipartite matching.  */
    map = (IDENT*)malloc(4*(size_t)cnt *sizeof(IDENT));
    if (!map) error(E_NOMEM);   /* create a row to column map  */
    inv = map  +cnt;            /* and its inverse map as well */
    stk = inv  +cnt;            /* as a column index stack */
    ccm = xmat +size +1;        /* get the work matrix */
    col = *ccm = (double*)malloc((size_t)(cnt*cnt) *sizeof(double));
    if (!col) error(E_NOMEM);   /* allocate work matrix elements */
    for (x = 0; x < cnt; x++) { /* traverse the matrix columns */
      ccm[x] = col;             /* and organize the cost matrix */
      for (sum = xmat[x], y = 0; y < cnt; y++)
        *col++ = -sum[y];       /* compute the cost matrix */
    }                           /* for the Hungarian method */
    /* Since the below implementation of the Hungarian method finds */
    /* a minimum cost matching, whereas a maximum "profit" matching */
    /* is desired here, the confusion matrix elements are negated,  */
    /* thus producing a corresponding cost matrix.                  */
    for (x = cnt; --x >= 0; ) { /* traverse the matrix columns */
      u = *(col = ccm[x]);      /* init. the column minimum */
      for (y = cnt; --y >  0; ) /* traverse the current column */
        if (col[y] < u) u = col[y];
      for (y = cnt; --y >= 0; ) /* find the minimal column element */
        col[y] -= u;            /* and then subtract this minimum */
      inv[x] = IDENT_MAX;       /* init. the column to row map */
    }                           /* (no matching found yet) */
    for (y = cnt; --y >= 0; ) { /* traverse the matrix rows */
      u = ccm[0][y];            /* init. the row minimum */
      for (x = cnt; --x >  0; ) /* traverse the matrix columns */
        if ((v = ccm[x][y]) < u) u = v;
      for (x = cnt; --x >= 0; ) /* find the minimal element in row */
        ccm[x][y] -= u;         /* and then subtract this minimum */
      map[y] = IDENT_MAX;       /* init. the row to column map */
    }                           /* (no matching found yet) */
    for (x = cnt; --x >= 0; ) { /* traverse the matrix columns */
      for (col = ccm[x], y = cnt; --y >= 0; )
        if ((map[y] >= IDENT_MAX) && (col[y] <= 0)) {
          inv[x] = y; map[y] = x; break; }
    }                           /* assign columns whenever possible */
    for (j = cnt; --j >= 0; ) { /* find the next unmatched column */
      if (inv[j] < IDENT_MAX)   /* if a column has been matched, */
        continue;               /* skip this column */
      inv[j] |= IDENT_MIN;      /* mark the root of the alt. tree */
      stk[0] = j; h = 1;        /* and push it onto the stack */
      while (1) {               /* loop to extend the matching */
        col = ccm[x = stk[--h]];/* pop a column from the stack */
        for (y = cnt; --y >= 0; ) {
          if (col[y] > 0)       /* traverse the rows to which */
            continue;           /* the column may be assigned */
          if (map[y] >= IDENT_MAX)
            break;              /* if assignment is possible, abort */
          if (map[y] >= 0) {    /* if not yet in alternating tree */
            inv[stk[h++] = map[y]] |= IDENT_MIN;
            map[y] = x | IDENT_MIN;
          }                     /* push the column onto the stack */
        }                       /* and link the row to the column */
        if (y >= 0) {           /* if possible reassignment found */
          while (inv[x] != (IDENT_MIN|IDENT_MAX)) {
            z = inv[x] & ~IDENT_MIN; /* follow the path to the root */
            inv[x] = y;              /* and reassign the columns */
            x = map[y = z] & ~IDENT_MIN;
          }                     /* (free a row for the root column) */
          inv[j] = y; break;    /* assign the current column (root) */
        }                       /* to the freed row and abort */
        /* After a reassignment, the matching has been extended by */
        /* the column that forms the root of the alternating tree. */
        if (h > 0) continue;    /* check for an empty stack */
        /* If the stack is not empty, the alternating tree is not  */
        /* yet complete, so an extension of the tree is needed.    */
        /* Otherwise the equality graph has to be extended, i.e.   */
        /* the cost matrix has to be adapted to create more zeros. */
        u = INFINITY;           /* adapt the matrix elements */
        for (x = cnt; --x >= 0; ) { /* traverse the marked columns */
          if (inv[x] >= 0) continue;
          for (col = ccm[x], y = cnt; --y >= 0; ) {
            if ((map[y] >= 0) && (col[y] < u))
              u = col[y];       /* find the minimum cost for a */
          }                     /* marked column and unmarked row */
        }                       /* (best alternative assigment) */
        assert(u > 0);          /* check for a positive minimum */
        for (x = cnt; --x >= 0; ) {
          col = ccm[x]; y = cnt;/* traverse all columns and rows */
          if (inv[x] >= 0) {    /* if the column is unmarked, */
            while (--y >= 0)    /* add minimum to marked rows */
              if (map[y] < 0) col[y] += u; }
          else {                /* if the column is marked */
            while (--y >= 0) {  /* subtract min. from unmarked rows */
              if (map[y] < 0) continue;
              if ((col[y] -= u) <= 0) stk[h++] = x;
            }                   /* (enlarge the equality graph, i.e. */
          }                     /* increase the number of zeros */
        }                       /* in the cost matrix */
      }
      for (x = cnt; --x >= 0; ) /* clear the column markers */
        if ((y = inv[x] &= ~IDENT_MIN) < IDENT_MAX) map[y] = x;
    }                           /* update the row to column mapping */
    free(*ccm);                 /* delete the cost matrix */
    memcpy(ccm, xmat, (size_t)cnt *sizeof(double*));
    for (j = 0; j < cnt; j++) xmat[j] = ccm[map[j]];
    ccm = NULL;                 /* reorganize the confusion matrix */
    for (u = 0, j = 0; j < cnt; j++)
      u += xmat[j][j];          /* compute the number of errors */
    fprintf(stderr, "[%g error(s)]", w-u);
    fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));
  }                             /* print a success message */

  /* --- compute number/percentage of errors --- */
  xmat[size] = sum = calloc((size_t)cnt+1, sizeof(double));
  if (!sum) error(E_NOMEM);     /* allocate the sum column */
  for (x = 0; x < cnt; x++) {   /* clear the sum column, then */
    col = xmat[x];              /* traverse the matrix columns */
    for (u = 0, y = 0; y < cnt; y++)
      if (y != x) { u += col[y]; sum[y] += col[y]; }
    sum[cnt]  += col[cnt] = u;  /* sum errors in rows and columns */
    col[cnt+1] = col[cnt] +col[x];  /* number of cases per column */
  }
  if (relnum) {                 /* if to compute relative numbers */
    for (u = w *0.01, x = 0; x <= cnt; x++)
      for (col = xmat[x], y = 0; y <= cnt; y++)
        col[y] /= u;            /* traverse all matrix fields and */
  }                             /* divide by the number of tuples */

  /* --- print confusion matrix --- */
  t = clock();                  /* start timer, open output file */
  if (fn_mat && (strcmp(fn_mat, "-") == 0)) fn_mat = "";
  if (fn_mat && *fn_mat) { out = fopen(fn_mat, "w"); }
  else                   { out = stdout; fn_mat = "<stdout>"; }
  if (!out) error(E_FOPEN, fn_mat);
  fprintf(stderr, "writing %s ... ", fn_mat);
  fmt = (relnum) ? " %5.1f" : " %5.0f";
  fprintf(out, "confusion matrix for '%s' vs. '%s':\n",
          yname, xname);        /* print the matrix title */
  if (sort) {                   /* sort values alphabetically */
    qsort(vals, (size_t)cnt, sizeof(int*), valcmp);
    if (perm) { for (x = 0; x < cnt; x++) inv[*vals[x]] = x; }
  }                             /* get inverse map for sorting */
  fprintf(out, " no | value");  /* print start of header */
  for (l = max-5; l > 0; l--) putc(' ', out);
  fprintf(out, " |");           /* fill value column */
  for (x = 0; x < cnt; x++) {   /* traverse the columns */
    y = *vals[x];               /* skip empty columns if requested */
    if (nozero && (xmat[y][cnt+1] == 0)) continue;
    fprintf(out, " %5"IDENT_FMT, (perm ? inv[map[y]] : y) +1);
  }                             /* print corresponding row number */
  fprintf(out, " | errors\n");  /* print end of header */
  fprintf(out, "----+-");       /* print start of separating line */
  for (l = max; l > 0; l--) putc('-', out);
  fprintf(out, "-+");           /* fill value column */
  for (x = cnt; --x >= 0; )     /* traverse the columns */
    if (!nozero || (xmat[*vals[x]][cnt+1] != 0))
      fprintf(out, "------");   /* print column segment */
  fprintf(out, "-+-------\n");  /* print end of separating line */

  for (y = 0; y < cnt; y++) {   /* traverse the rows of the matrix */
    fprintf(out, "%3"IDENT_FMT" | ",y+1); /* print row number */
    py = vals[y]; yname = (char*)st_name(py);
    fputs(yname, out);          /* print value corresp. to row */
    for (l = max -strlen(yname); l > 0; l--)
      putc(' ', out);           /* fill value column */
    fprintf(out, " |");         /* and print a separator */
    for (x = 0; x < cnt; x++)   /* traverse the matrix columns */
      if (!nozero || (xmat[*vals[x]][cnt+1] != 0))
        fprintf(out, fmt, xmat[*(vals[x])][*py]);
    fprintf(out, " | "); fprintf(out, fmt, sum[*py]);
    fprintf(out, "\n");         /* print columns of matrix row */
  }                             /* using the appropriate format */

  fprintf(out, "----+-");       /* print start of separating line */
  for (l = max; l > 0; l--) putc('-', out);
  fprintf(out, "-+");           /* fill value column */
  for (x = cnt; --x >= 0; )     /* traverse the columns */
    if (!nozero || (xmat[*vals[x]][cnt+1] != 0))
      fprintf(out, "------");   /* print column segment */
  fprintf(out, "-+-------\n");  /* print end of separating line */

  fprintf(out, "    | errors"); /* print start of error row */
  for (l = max-6; l > 0; l--) putc(' ', out);
  fprintf(out, " |");           /* fill the value column */
  for (x = 0; x < cnt; x++)     /* and print the errors */
    if (!nozero || (xmat[*vals[x]][cnt+1] != 0))
      fprintf(out, fmt, xmat[*(vals[x])][cnt]);
  fprintf(out, " | "); fprintf(out, fmt, sum[cnt]);
  fprintf(out, "\n");           /* print columns of error row */
  if ((out != stdout) && ((i = fclose(out)) != 0))
    error(E_FWRITE, fn_mat);    /* close confusion matrix file */
  out = NULL;                   /* and print a success message */
  fprintf(stderr, "[%"IDENT_FMT" row(s)/column(s)]", cnt);
  fprintf(stderr, " done [%.2fs].\n", SEC_SINCE(t));

  /* --- clean up --- */
  CLEANUP;                      /* clean up memory and close files */
  SHOWMEM;                      /* show (final) memory usage */
  return 0;                     /* return 'ok' */
}  /* main() */
