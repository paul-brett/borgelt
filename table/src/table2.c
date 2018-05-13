/*----------------------------------------------------------------------
  File    : table2.c
  Contents: tuple and table management, read and write functions
  Author  : Christian Borgelt
  History : 1999.04.17 file created as io.c
            1998.09.25 first step of major redesign completed
            1999.02.04 all occurences of long int changed to int
            1999.03.15 one point coverage functions transf. from opc.c
            1999.03.17 one point coverage functions redesigned
            2001.06.24 module split into two files (now 1 and 3)
            2003.08.16 slight changes in error message output
            2007.02.13 adapted to modified modules attset and tabread
            2010.10.07 adapted to modified modules attset and tabread
            2010.10.08 adapted to new module tabwrite, time parameters
            2010.12.30 functions with va_list arguments added
            2013.07.19 adapted to definitions ATTID, VALID, TPLID etc.
----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <assert.h>
#include "table.h"
#ifdef STORAGE
#include "storage.h"
#endif

/*----------------------------------------------------------------------
  Table Functions
----------------------------------------------------------------------*/
#ifdef TAB_READ

int tab_read (TABLE *tab, TABREAD *trd, int mode, ...)
{                               /* --- read a table */
  int     r;                    /* buffer for result */
  va_list args;                 /* list of variable arguments */

  assert(tab && trd);           /* check the function arguments */
  va_start(args, mode);         /* start variable argument evaluation */
  r = tab_vread(tab, trd, mode, &args);           /* read the table */
  va_end(args);                 /* end variable argument evaluation */
  return r;                     /* return the read result */
}  /* tab_read() */

/*--------------------------------------------------------------------*/

int tab_vread (TABLE *tab, TABREAD *trd, int mode, va_list *args)
{                               /* --- read a table */
  int r;                        /* result of write operation */

  assert(tab && trd && args);   /* check the function arguments */
  r = as_vread(tab->attset, trd, mode, args);
  if (r < 0) return r;          /* read the first record and */
  if (r > 0) return 0;          /* check for error and end of file */
  if ((mode & AS_DFLT)          /* if a tuple has been read, */
  || !(mode & AS_ATT)) {        /* store the read tuple */
    if (tab_tpladd(tab, NULL) != 0) return E_NOMEM; }
  if (mode & TAB_ONE) return 0; /* check for single record reading */
  mode = (mode & ~(AS_DFLT|AS_ATT|TAB_ONE)) | AS_INST;
  while (1) {                   /* record read loop */
    r = as_vread(tab->attset, trd, mode, args);
    if (r < 0) return r;        /* read the next record and */
    if (r > 0) return 0;        /* check for error and end of file */
    if (tab_tpladd(tab, NULL) != 0) return E_NOMEM;
  }                             /* store the read tuple */
}  /* tab_vread() */

#endif  /* #ifdef TAB_READ */
/*--------------------------------------------------------------------*/
#ifdef TAB_WRITE

int tab_write (TABLE *tab, TABWRITE *twr, int mode, ...)
{                               /* --- write a table */
  int     r;                    /* buffer for result */
  va_list args;                 /* list of variable arguments */

  assert(tab && twr);           /* check the function arguments */
  va_start(args, mode);         /* start variable argument evaluation */
  r = tab_vwrite(tab, twr, mode, &args);         /* write the table */
  va_end(args);                 /* end variable argument evaluation */
  return r;                     /* return the write result */
}  /* tab_write() */

/*--------------------------------------------------------------------*/

int tab_vwrite (TABLE *tab, TABWRITE *twr, int mode, va_list *args)
{                               /* --- write a table */
  TPLID i, n;                   /* loop variable, number of tuples */
  int   r;                      /* result of write operation */

  assert(tab && twr && args);   /* check the function arguments */
  if (mode & AS_ATT) {          /* if to write a table header */
    if (mode & AS_ALIGN)        /* if to align the fields, */
      mode |= AS_ALNHDR;        /* align them to the header */
    r = as_vwrite(tab->attset, twr, mode, args);
    if (r < 0) return r;        /* write the attribute names */
  }                             /* with the table writer */
  mode = AS_INST | (mode & ~AS_ATT);
  n    = tab_tplcnt(tab);       /* get the number of tuples */
  for (i = 0; i < n; i++) {     /* and traverse the tuples */
    tpl_toas(tab_tpl(tab, i));  /* copy tuple to its attribute set */
    r = as_vwrite(tab->attset, twr, mode, args);
    if (r < 0) return r;        /* write the attribute values */
  }                             /* with the table writer */
  return twr_error(twr);        /* check for a write error */
}  /* tab_vwrite() */

#endif  /* #ifdef TAB_WRITE */
