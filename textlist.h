/* $Id: textlist.h,v 1.3 2001/11/24 02:01:26 ukai Exp $ */
#ifndef TEXTLIST_H
#define TEXTLIST_H
#include "Str.h"

/* General doubly linked list */

typedef struct _listitem {
    void *ptr;
    struct _listitem *next;
    struct _listitem *prev;
} ListItem;

typedef struct _generallist {
    ListItem *first;
    ListItem *last;
    short nitem;
} GeneralList;

extern ListItem *newListItem(void *s, ListItem *n, ListItem *p);
extern GeneralList *newGeneralList(void);
extern void pushValue(GeneralList *tl, void *s);
extern void *popValue(GeneralList *tl);
extern void *rpopValue(GeneralList *tl);
extern GeneralList *appendGeneralList(GeneralList *, GeneralList *);

/* Text list */

typedef struct _textlistitem {
    char *ptr;
    struct _textlistitem *next;
    struct _textlistitem *prev;
} TextListItem;

typedef struct _textlist {
    TextListItem *first;
    TextListItem *last;
    short nitem;
} TextList;

#define newTextList() ((TextList *)newGeneralList())
#define pushText(tl, s) pushValue((GeneralList *)(tl), (void *)allocStr((s)?(s):"",0))
#define popText(tl) ((char *)popValue((GeneralList *)(tl)))
#define rpopText(tl) ((char *)rpopValue((GeneralList *)(tl)))
#define appendTextList(tl, tl2) ((TextList *)appendGeneralList((GeneralList *)(tl), (GeneralList *)(tl2)))

/* Line text list */

typedef struct _TextLine {
    Str line;
    short pos;
} TextLine;

typedef struct _textlinelistitem {
    TextLine *ptr;
    struct _textlinelistitem *next;
    struct _textlinelistitem *prev;
} TextLineListItem;

typedef struct _textlinelist {
    TextLineListItem *first;
    TextLineListItem *last;
    short nitem;
} TextLineList;

extern TextLine *newTextLine(Str line, int pos);
extern void appendTextLine(TextLineList *tl, Str line, int pos);
#define newTextLineList() ((TextLineList *)newGeneralList())
#define pushTextLine(tl,lbuf) pushValue((GeneralList *)(tl),(void *)(lbuf))
#define popTextLine(tl) ((TextLine *)popValue((GeneralList *)(tl)))
#define rpopTextLine(tl) ((TextLine *)rpopValue((GeneralList *)(tl)))
#define appendTextLineList(tl, tl2) ((TextLineList *)appendGeneralList((GeneralList *)(tl), (GeneralList *)(tl2)))

#endif				/* not TEXTLIST_H */