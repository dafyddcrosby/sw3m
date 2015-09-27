/* $Id: table.h,v 1.12 2003/09/22 21:02:21 ukai Exp $ */
#ifndef _TABLE_H
#define _TABLE_H
#include "Str.h"
#include "fm.h"

#define MAX_TABLE 20		/* maximum nest level of table */
#define MAX_TABLE_N 20		/* maximum number of table in same level */

#define MAXROW 50
#define MAXCOL 50

#define MAX_WIDTH 80

typedef enum _bordermode {
  BORDER_NONE,
  BORDER_THIN,
  BORDER_THICK,
  BORDER_NOWIN
} BorderMode;

typedef enum _table_attr {
  HTT_X      = 0x0001,
  HTT_Y      = 0x0002,
  HTT_NOWRAP = 0x0004,
  HTT_ALIGN  = 0x0030,
  HTT_LEFT   = 0x0000,
  HTT_CENTER = 0x0010,
  HTT_RIGHT  = 0x0020,
  HTT_TRSET  = 0x0040,
  HTT_VALIGN = 0x0700,
  HTT_TOP    = 0x0100,
  HTT_MIDDLE = 0x0200,
  HTT_BOTTOM = 0x0400,
  HTT_VTRSET = 0x0800
} table_attr;

/* flag */
typedef enum _tablemode {
  TBL_IN_ROW = 1,
  TBL_EXPAND_OK = 2,
  TBL_IN_COL = 4
} TableMode;

#define MAXCELL 20
#define MAXROWCELL 1000
struct table_cell {
    short col[MAXCELL];
    short colspan[MAXCELL];
    short index[MAXCELL];
    short maxcell;
    short icell;
    short width[MAXCELL];
    short minimum_width[MAXCELL];
    short fixed_width[MAXCELL];
};

struct table_in {
    struct table *ptr;
    short col;
    short row;
    short cell;
    short indent;
    TextLineList *buf;
};

struct table_linfo {
    Lineprop prev_ctype;
    signed char prev_spaces;
    Str prevchar;
    short length;
};

struct table {
    int row;
    int col;
    int maxrow;
    int maxcol;
    int max_rowsize;
    BorderMode border_mode;
    int total_width;
    int total_height;
    int tabcontentssize;
    int indent;
    int cellspacing;
    int cellpadding;
    int vcellpadding;
    int vspace;
    TableMode flag;
#ifdef TABLE_EXPAND
    int real_width;
#endif				/* TABLE_EXPAND */
    Str caption;
#ifdef ID_EXT
    Str id;
#endif
    GeneralList ***tabdata;
    table_attr **tabattr;
    table_attr trattr;
#ifdef ID_EXT
    Str **tabidvalue;
    Str *tridvalue;
#endif
    short tabwidth[MAXCOL];
    short minimum_width[MAXCOL];
    short fixed_width[MAXCOL];
    struct table_cell cell;
    short *tabheight;
    struct table_in *tables;
    short ntable;
    short tables_size;
    TextList *suspended_data;
    /* use for counting skipped spaces */
    struct table_linfo linfo;
    int sloppy_width;
};

#define TBLM_PRE	RB_PRE
#define TBLM_SCRIPT	RB_SCRIPT
#define TBLM_STYLE	RB_STYLE
#define TBLM_PLAIN	RB_PLAIN
#define TBLM_NOBR	RB_NOBR
#define TBLM_PRE_INT	RB_PRE_INT
#define TBLM_INTXTA	RB_INTXTA
#define TBLM_INSELECT	RB_INSELECT
#define TBLM_PREMODE	(TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN | TBLM_INTXTA)
#define TBLM_SPECIAL	(TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN | TBLM_NOBR)
#define TBLM_DEL	RB_DEL
#define TBLM_S		RB_S
#define TBLM_ANCHOR	0x1000000

struct table_mode {
    unsigned int pre_mode;
    char indent_level;
    char caption;
    short nobr_offset;
    char nobr_level;
    short anchor_offset;
    unsigned char end_tag;
};

extern struct table * begin_table(BorderMode border, int spacing, int padding, int vspace);

#endif /* _TABLE_H */
/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */
