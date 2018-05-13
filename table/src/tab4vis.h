/*----------------------------------------------------------------------
  File    : tab4vis.h
  Contents: table utility functions for visualization programs
  Author  : Christian Borgelt
  History : 2001.11.08 file created from file lvq.h
            2013.08.08 adapted to definitions ATTID, VALID, WEIGHT etc.
----------------------------------------------------------------------*/
#ifndef __TAB4VIS__
#define __TAB4VIS__
#ifndef TAB_READ
#define TAB_READ
#endif
#include "table.h"

/*----------------------------------------------------------------------
  Preprocessor Definitions
----------------------------------------------------------------------*/
#define FMTCHRLEN     80        /* length of format character strings */

/*----------------------------------------------------------------------
  Type Definitions
----------------------------------------------------------------------*/
typedef struct {                /* -- data format description -- */
  int    names;                 /* column names in first record */
  char   recseps[FMTCHRLEN+1];  /* record separators */
  char   fldseps[FMTCHRLEN+1];  /* field separators */
  char   blanks [FMTCHRLEN+1];  /* blank characters */
  char   nullchs[FMTCHRLEN+1];  /* null value characters */
  char   comment[FMTCHRLEN+1];  /* comment characters */
} DATAFMT;                      /* (data format description) */

typedef struct {                /* --- range of values --- */
  double min, max;              /* minimum and maximum value */
} RANGE;                        /* (range of values) */

typedef struct {                /* -- selected attributes -- */
  ATTID  h_att;                 /* attribute for horizontal direction */
  RANGE  h_rng;                 /* horizontal range of values */
  ATTID  v_att;                 /* attribute for vertical   direction */
  RANGE  v_rng;                 /* vertical range of values */
  ATTID  c_att;                 /* class attribute (if any) */
} SELATT;                       /* (selected attributes) */

/*----------------------------------------------------------------------
  Global Variables
----------------------------------------------------------------------*/
/* --- attribute set variables --- */
extern ATTSET  *attset;         /* attribute set */
extern RANGE   *ranges;         /* ranges of attribute values */
extern CCHAR   **nms_met;       /* names of metric attributes */
extern ATTID   *map_met;        /* map metric atts. to attset ids */
extern ATTID   *inv_met;        /* inverse map (attset id to index) */
extern ATTID   metcnt;          /* number of metric attributes */
extern CCHAR   **nms_nom;       /* names of nominal attributs */
extern ATTID   *map_nom;        /* map nominal att. to attset ids */
extern ATTID   *inv_nom;        /* inverse map (attset id to index) */
extern ATTID   nomcnt;          /* number of nominal attributes */
extern SELATT  selatt;          /* attribute selection information */

/* --- data table variables --- */
extern TABLE   *table;          /* data table */
extern DATAFMT datafmt;         /* data format description */

/* --- error message --- */
extern char    tv_msg[];        /* buffer for an error message */

/*----------------------------------------------------------------------
  Functions
----------------------------------------------------------------------*/
extern void tv_clean (void);
extern int  tv_load  (const char *fname, double addfrac);

#endif
