/*----------------------------------------------------------------------
  File    : attmap.h
  Contents: attribute map management (for numeric coding)
  Author  : Christian Borgelt
  History : 2003.08.11 file created
            2003.08.12 function am_type added
            2007.01.10 function am_target and mode AM_BIN2COL added
            2013.07.18 adapted to definitions ATTID, TPLID, WEIGHT etc.
            2015.08.02 function am_mark() added (mark mapped attributes)
            2015.11.30 mode AM_MINUS1 added (for 1-in-(n-1) encoding)
            2016.02.15 function am_clone() added (clone a mapping)
----------------------------------------------------------------------*/
#ifndef __ATTMAP__
#define __ATTMAP__
#include "table.h"

#if ATTID_MAX < VALID_MAX
#error "attmap requires ATTID_MAX >= VALID_MAX"
#endif

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
/* --- modes for am_create --- */
#define AM_MARKED     1         /* map only marked attributes */
#define AM_BIN2COL    2         /* map binary atts. to two columns */
#define AM_MINUS1     4         /* 1-in-(n-1) instead of 1-in-n */

/* --- modes for am_exec --- */
#define AM_INPUTS     1         /* map only input attributes */
#define AM_TARGET   (~1)        /* map only target attribute */
#define AM_BOTH     (~0)        /* map both input and target atts. */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* --- attribute map element --- */
  ATT    *att;                  /* attribute to map */
  int    type;                  /* attribute type indicator */
  ATTID  off;                   /* offset to the first dimension */
  ATTID  cnt;                   /* number of dimensions mapped to */
} AMEL;                         /* (attribute map element) */

typedef struct {                /* --- attribute map --- */
  ATTSET *attset;               /* underlying attribute set */
  ATTID  attcnt;                /* number of attributes */
  ATTID  incnt;                 /* number of input  dimensions */
  ATTID  outcnt;                /* number of output dimensions */
  double one;                   /* value to set for 1-in-n/(n-1) */
  AMEL   amels[1];              /* attribute map elements */
} ATTMAP;                       /* (attribute map) */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/
extern ATTMAP* am_create (ATTSET *attset, int mode, double one);
extern void    am_delete (ATTMAP *map, int delas);
extern ATTMAP* am_clone  (ATTMAP *map, int cloneas);
extern ATTSET* am_attset (ATTMAP *map);
extern ATTID   am_attcnt (ATTMAP *map);

extern void    am_target (ATTMAP *map, ATTID trgid);
extern ATTID   am_mark   (ATTMAP *map);

extern ATTID   am_dim    (ATTMAP *map);
extern ATTID   am_incnt  (ATTMAP *map);
extern ATTID   am_outcnt (ATTMAP *map);

extern ATT*    am_att    (ATTMAP *map, ATTID attid);
extern int     am_type   (ATTMAP *map, ATTID attid);
extern ATTID   am_off    (ATTMAP *map, ATTID attid);
extern ATTID   am_cnt    (ATTMAP *map, ATTID attid);

extern void    am_exec   (ATTMAP *map, const TUPLE *tpl, int mode,
                          double *vec);

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define am_attset(m)     ((m)->attset)
#define am_attcnt(m)     ((m)->attcnt)

#define am_dim(m)        ((m)->incnt)
#define am_incnt(m)      ((m)->incnt)
#define am_outcnt(m)     ((m)->outcnt)

#define am_att(m,i)      ((m)->amels[((i)<0) ?(m)->attcnt-1 :(i)].att)
#define am_type(m,i)     ((m)->amels[((i)<0) ?(m)->attcnt-1 :(i)].type)
#define am_off(m,i)      ((m)->amels[((i)<0) ?(m)->attcnt-1 :(i)].off)
#define am_cnt(m,i)      ((m)->amels[((i)<0) ?(m)->attcnt-1 :(i)].cnt)

#endif
