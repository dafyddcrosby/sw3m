/* $Id: history.c,v 1.4 2001/11/24 02:01:26 ukai Exp $ */
#include "fm.h"

#ifdef USE_HISTORY
Buffer *
historyBuffer(Hist *hist)
{
    Str src = Strnew();
    HistItem *item;
    char *q;

    Strcat_charp(src, "<html>\n<head><title>History Page</title></head>\n");
    Strcat_charp(src, "<body>\n<h1>History Page</h1>\n<hr>\n");
    Strcat_charp(src, "<ol>\n");
    if (hist && hist->list) {
	for (item = hist->list->last; item; item = item->prev) {
	    q = html_quote((char *)item->ptr);
	    Strcat_charp(src, "<li><a href=\"");
	    Strcat_charp(src, q);
	    Strcat_charp(src, "\">");
	    Strcat_charp(src, q);
	    Strcat_charp(src, "</a>\n");
	}
    }
    Strcat_charp(src, "</ol>\n</body>\n</html>");
    return loadHTMLString(src);
}

void
loadHistory(Hist *hist)
{
    FILE *f;
    Str line;

    if (hist == NULL)
	return;
    if ((f = fopen(rcFile(HISTORY_FILE), "rt")) == NULL)
	return;

    while (!feof(f)) {
	line = Strfgets(f);
	Strchop(line);
	Strremovefirstspaces(line);
	Strremovetrailingspaces(line);
	if (line->length == 0)
	    continue;
	pushHist(hist, url_quote(line->ptr));
    }
    fclose(f);
}

void
saveHistory(Hist *hist, size_t size)
{
    FILE *f;
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return;
    if ((f = fopen(rcFile(HISTORY_FILE), "w")) == NULL) {
	disp_err_message("Can't open history", FALSE);
	return;
    }
    for (item = hist->list->first; item && hist->list->nitem > size;
	 item = item->next)
	size++;
    for (; item; item = item->next)
	fprintf(f, "%s\n", (char *)item->ptr);
    fclose(f);
}
#endif				/* USE_HISTORY */

Hist *
newHist()
{
    Hist *hist;

    hist = New(Hist);
    hist->list = (HistList *)newGeneralList();
    hist->current = NULL;
    hist->hash = NULL;
    return hist;
}

HistItem *
unshiftHist(Hist *hist, char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    item = (HistItem *)newListItem((void *)allocStr(ptr, 0),
				   (ListItem *)hist->list->first, NULL);
    if (hist->list->first)
	hist->list->first->prev = item;
    else
	hist->list->last = item;
    hist->list->first = item;
    hist->list->nitem++;
    return item;
}

HistItem *
pushHist(Hist *hist, char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    item = (HistItem *)newListItem((void *)allocStr(ptr, 0),
				   NULL, (ListItem *)hist->list->last);
    if (hist->list->last)
	hist->list->last->next = item;
    else
	hist->list->first = item;
    hist->list->last = item;
    hist->list->nitem++;
    return item;
}

/* Don't mix pushHashHist() and pushHist()/unshiftHist(). */

HistItem *
pushHashHist(Hist *hist, char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    item = getHashHist(hist, ptr);
    if (item) {
	if (item->next)
	    item->next->prev = item->prev;
	else			/* item == hist->list->last */
	    hist->list->last = item->prev;
	if (item->prev)
	    item->prev->next = item->next;
	else			/* item == hist->list->first */
	    hist->list->first = item->next;
	hist->list->nitem--;
    }
    item = pushHist(hist, ptr);
    putHash_hist(hist->hash, ptr, (void *)item);
    return item;
}

HistItem *
getHashHist(Hist *hist, char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->hash == NULL) {
	hist->hash = newHash_hist(HIST_HASH_SIZE);
	for (item = hist->list->first; item; item = item->next)
	    putHash_hist(hist->hash, (char *)item->ptr, (void *)item);
    }
    return (HistItem *)getHash_hist(hist->hash, ptr, NULL);
}

char *
lastHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->list->last) {
	hist->current = hist->list->last;
	return (char *)hist->current->ptr;
    }
    return NULL;
}

char *
nextHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->current && hist->current->next) {
	hist->current = hist->current->next;
	return (char *)hist->current->ptr;
    }
    return NULL;
}

char *
prevHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->current && hist->current->prev) {
	hist->current = hist->current->prev;
	return (char *)hist->current->ptr;
    }
    return NULL;
}