/* $Id: main.c,v 1.24 2001/11/27 04:45:28 ukai Exp $ */
#define MAINPROGRAM
#include "fm.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include "terms.h"
#include "myctype.h"
#include "regex.h"
#ifdef USE_MOUSE
#ifdef USE_GPM
#include <gpm.h>
#endif				/* USE_GPM */
#if defined(USE_GPM) || defined(USE_SYSMOUSE)
extern int do_getch();
#define getch()	do_getch()
#endif				/* defined(USE_GPM) || *
				 * defined(USE_SYSMOUSE) */
#endif

#define DSTR_LEN	256

Hist *LoadHist;
Hist *SaveHist;
Hist *URLHist;
Hist *ShellHist;
Hist *TextHist;

typedef struct {
    int cmd;
    void *user_data;
} Event;
#define N_EVENT_QUEUE 10
static Event eventQueue[N_EVENT_QUEUE];
static int n_event_queue;

#ifdef USE_ALARM
static int alarm_sec = 0;
static short alarm_status = AL_UNSET;
static Buffer *alarm_buffer;
static Event alarm_event;
static MySignalHandler SigAlarm(SIGNAL_ARG);
#endif

#ifdef USE_MARK
static char *MarkString = NULL;
#endif
static char *SearchString = NULL;
int (*searchRoutine) (Buffer *, char *);

JMP_BUF IntReturn;

static void cmd_loadfile(char *path);
static void cmd_loadURL(char *url, ParsedURL *current);
static void cmd_loadBuffer(Buffer *buf, int prop, int link);
static void keyPressEventProc(int c);
#ifdef USE_MARK
static void cmd_mark(Lineprop *p);
#endif				/* USE_MARK */
int show_params_p = 0;
void show_params(FILE * fp);

static int display_ok = FALSE;
static void dump_source(Buffer *);
static void dump_head(Buffer *);
static void dump_extra(Buffer *);
int prec_num = 0;
int prev_key = -1;
int on_target = 1;

void set_buffer_environ(Buffer *);

static void _followForm(int);
static void _goLine(char *);
#define PREC_NUM (prec_num ? prec_num : 1)
#define PREC_LIMIT 10000
static int searchKeyNum(void);

#include "gcmain.c"

#define help() fusage(stdout, 0)
#define usage() fusage(stderr, 1)

static void
fusage(FILE * f, int err)
{
    fprintf(f, "version %s\n", version);
    fprintf(f, "usage: w3m [options] [URL or filename]\noptions:\n");
    fprintf(f, "    -t tab           set tab width\n");
    fprintf(f, "    -r               ignore backspace effect\n");
    fprintf(f, "    -l line          # of preserved line (default 10000)\n");
#ifdef JP_CHARSET
#ifndef DEBIAN			/* disabled by ukai: -s is used for squeeze multi lines */
    fprintf(f, "    -e               EUC-JP\n");
    fprintf(f, "    -s               Shift_JIS\n");
    fprintf(f, "    -j               JIS\n");
#endif
    fprintf(f, "    -O e|s|j|N|m|n   display code\n");
    fprintf(f, "    -I e|s           document code\n");
#endif				/* JP_CHARSET */
    fprintf(f, "    -B               load bookmark\n");
    fprintf(f, "    -bookmark file   specify bookmark file\n");
    fprintf(f, "    -T type          specify content-type\n");
    fprintf(f, "    -m               internet message mode\n");
    fprintf(f, "    -v               visual startup mode\n");
#ifdef USE_COLOR
    fprintf(f, "    -M               monochrome display\n");
#endif				/* USE_COLOR */
    fprintf(f, "    -F               automatically render frame\n");
    fprintf(f,
	    "    -cols width      specify column width (used with -dump)\n");
    fprintf(f,
	    "    -ppc count       specify the number of pixels per character (4.0...32.0)\n");
    fprintf(f, "    -dump            dump formatted page into stdout\n");
    fprintf(f,
	    "    -dump_head       dump response of HEAD request into stdout\n");
    fprintf(f, "    -dump_source     dump page source into stdout\n");
    fprintf(f, "    -dump_both       dump HEAD and source into stdout\n");
    fprintf(f,
	    "    -dump_extra      dump HEAD, source, and extra information into stdout\n");
    fprintf(f, "    -post file       use POST method with file content\n");
    fprintf(f, "    -header string   insert string as a header\n");
    fprintf(f, "    +<num>           goto <num> line\n");
    fprintf(f, "    -num             show line number\n");
    fprintf(f, "    -no-proxy        don't use proxy\n");
#ifdef USE_MOUSE
    fprintf(f, "    -no-mouse        don't use mouse\n");
#endif				/* USE_MOUSE */
#ifdef USE_COOKIE
    fprintf(f,
	    "    -cookie          use cookie (-no-cookie: don't use cookie)\n");
#endif				/* USE_COOKIE */
    fprintf(f, "    -pauth user:pass proxy authentication\n");
#ifndef KANJI_SYMBOLS
    fprintf(f, "    -no-graph        don't use graphic character\n");
#endif				/* not KANJI_SYMBOLS */
#ifdef DEBIAN			/* replaced by ukai: pager requires -s */
    fprintf(f, "    -s               squeeze multiple blank lines\n");
#else
    fprintf(f, "    -S               squeeze multiple blank lines\n");
#endif
    fprintf(f, "    -W               toggle wrap search mode\n");
    fprintf(f, "    -X               don't use termcap init/deinit\n");
    fprintf(f, "    -o opt=value     assign value to config option\n");
    fprintf(f, "    -config file     specify config file\n");
    fprintf(f, "    -debug           DO NOT USE\n");
    if (show_params_p)
	show_params(f);
    exit(err);
}

static int option_assigned = 0;
extern void parse_proxy(void);

static GC_warn_proc orig_GC_warn_proc = NULL;
#define GC_WARN_KEEP_MAX (20)

static void
wrap_GC_warn_proc(char *msg, GC_word arg)
{
    if (fmInitialized) {
      /* *INDENT-OFF* */
      static struct {
	  char *msg;
	  GC_word arg;
      } msg_ring[GC_WARN_KEEP_MAX] = {
	  {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0},
	  {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0},
	  {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0},
	  {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0},
      };
      /* *INDENT-ON* */
	static int i = 0;
	static int n = 0;
	static int lock = 0;
	int j;

	j = (i + n) % (sizeof(msg_ring) / sizeof(msg_ring[0]));
	msg_ring[j].msg = msg;
	msg_ring[j].arg = arg;

	if (n < sizeof(msg_ring) / sizeof(msg_ring[0]))
	    ++n;
	else
	    ++i;

	if (!lock) {
	    lock = 1;

	    for (; n > 0; --n, ++i) {
		i %= sizeof(msg_ring) / sizeof(msg_ring[0]);
		disp_message_nsec(Sprintf
				  (msg_ring[i].msg,
				   (unsigned long)msg_ring[i].arg)->ptr, FALSE,
				  1, TRUE, FALSE);
	    }

	    lock = 0;
	}
    }
    else if (orig_GC_warn_proc)
	orig_GC_warn_proc(msg, arg);
    else
	fprintf(stderr, msg, (unsigned long)arg);
}

static void
sig_chld(int signo)
{
    int stat;
#ifdef HAVE_WAITPID
    pid_t pid;

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
	;
    }
#elif HAVE_WAIT3
    int pid;

    while ((pid = wait3(&stat, WNOHANG, NULL)) > 0) {
	;
    }
#else
    wait(&stat);
#endif
    signal(SIGCHLD, sig_chld);
    return;
}

Str
make_optional_header_string(char *s)
{
    char *p;
    Str hs;

    for (p = s; *p && *p != ':'; p++) ;
    if (*p != ':' || p == s)
	return NULL;
    if (strchr(s, '\n')) {
	return NULL;
    }
    hs = Strnew_size(p - s);
    strncpy(hs->ptr, s, p - s);
    hs->length = p - s;
    if (!Strcasecmp_charp(hs, "content-type")) {
	override_content_type = TRUE;
    }
    Strcat_charp(hs, ": ");
    if (*(p + 1)) {		/* not null header */
	for (p = p + 1; isspace(*p); p++) ;	/* skip white spaces */
	Strcat_charp(hs, p);
    }
    Strcat_charp(hs, "\r\n");
    return hs;
}

int
MAIN(int argc, char **argv, char **envp)
{
    Buffer *newbuf = NULL;
    char *p, c;
    int i;
    InputStream redin;
    char *line_str = NULL;
    char **load_argv;
    FormList *request;
    int load_argc = 0;
    int load_bookmark = FALSE;
    int visual_start = FALSE;
    char search_header = FALSE;
    char *default_type = NULL;
    char *post_file = NULL;
    Str err_msg;

#ifndef HAVE_SYS_ERRLIST
    prepare_sys_errlist();
#endif				/* not HAVE_SYS_ERRLIST */

    srand48(time(0));

    NO_proxy_domains = newTextList();
    fileToDelete = newTextList();

    load_argv = New_N(char *, argc - 1);
    load_argc = 0;

    CurrentDir = currentdir();
    BookmarkFile = NULL;
    rc_dir = expandName(RC_DIR);
    i = strlen(rc_dir);
    if (i > 1 && rc_dir[i - 1] == '/')
	rc_dir[i - 1] = '\0';
    config_file = rcFile(CONFIG_FILE);
    create_option_search_table();

    /* argument search 1 */
    for (i = 1; i < argc; i++) {
	if (*argv[i] == '-') {
	    if (!strcmp("-config", argv[i])) {
		argv[i] = "-dummy";
		if (++i >= argc)
		    usage();
		config_file = argv[i];
		argv[i] = "-dummy";
	    }
	    else if (!strcmp("-h", argv[i]))
		help();
	}
    }

    /* initializations */
    init_rc(config_file);
#ifdef USE_COOKIE
    initCookie();
#endif				/* USE_COOKIE */
    setLocalCookie();		/* setup cookie for local CGI */

    LoadHist = newHist();
    SaveHist = newHist();
    ShellHist = newHist();
    TextHist = newHist();
    URLHist = newHist();
#ifdef USE_HISTORY
    loadHistory(URLHist);
#endif				/* not USE_HISTORY */

    if (!non_null(HTTP_proxy) &&
	((p = getenv("HTTP_PROXY")) ||
	 (p = getenv("http_proxy")) || (p = getenv("HTTP_proxy")))) {
	HTTP_proxy = p;
	parseURL(p, &HTTP_proxy_parsed, NULL);
    }
#ifdef USE_GOPHER
    if (!non_null(GOPHER_proxy) &&
	((p = getenv("GOPHER_PROXY")) ||
	 (p = getenv("gopher_proxy")) || (p = getenv("GOPHER_proxy")))) {
	GOPHER_proxy = p;
	parseURL(p, &GOPHER_proxy_parsed, NULL);
    }
#endif				/* USE_GOPHER */
    if (!non_null(FTP_proxy) &&
	((p = getenv("FTP_PROXY")) ||
	 (p = getenv("ftp_proxy")) || (p = getenv("FTP_proxy")))) {
	FTP_proxy = p;
	parseURL(p, &FTP_proxy_parsed, NULL);
    }
    if (!non_null(NO_proxy) &&
	((p = getenv("NO_PROXY")) ||
	 (p = getenv("no_proxy")) || (p = getenv("NO_proxy")))) {
	NO_proxy = p;
	set_no_proxy(p);
    }

    if (Editor == NULL && (p = getenv("EDITOR")) != NULL)
	Editor = p;
    if (Mailer == NULL && (p = getenv("MAILER")) != NULL)
	Mailer = p;

    /* argument search 2 */
    i = 1;
    while (i < argc) {
	if (*argv[i] == '-') {
	    if (!strcmp("-t", argv[i])) {
		if (++i >= argc)
		    usage();
		if (atoi(argv[i]) > 0)
		    Tabstop = atoi(argv[i]);
	    }
	    else if (!strcmp("-r", argv[i]))
		ShowEffect = FALSE;
	    else if (!strcmp("-l", argv[i])) {
		if (++i >= argc)
		    usage();
		if (atoi(argv[i]) > 0)
		    PagerMax = atoi(argv[i]);
	    }
#ifdef JP_CHARSET
#ifndef DEBIAN			/* XXX: use -o kanjicode={S|J|E} */
	    else if (!strcmp("-s", argv[i]))
		DisplayCode = CODE_SJIS;
	    else if (!strcmp("-j", argv[i]))
		DisplayCode = CODE_JIS_n;
	    else if (!strcmp("-e", argv[i]))
		DisplayCode = CODE_EUC;
#endif
	    else if (!strncmp("-I", argv[i], 2)) {
		if (argv[i][2])
		    argv[i] += 2;
		else if (++i >= argc)
		    usage();
		c = str_to_code(argv[i]);
		switch (c) {
		case CODE_EUC:
		case CODE_SJIS:
		case CODE_INNER_EUC:
		    DocumentCode = c;
		    UseContentCharset = FALSE;
		    UseAutoDetect = FALSE;
		    break;
		default:
		    DocumentCode = '\0';
		    UseContentCharset = TRUE;
		    UseAutoDetect = TRUE;
		    break;
		}
	    }
	    else if (!strncmp("-O", argv[i], 2)) {
		if (argv[i][2])
		    argv[i] += 2;
		else if (++i >= argc)
		    usage();
		c = str_to_code(argv[i]);
		if (c != CODE_INNER_EUC && c != CODE_ASCII)
		    DisplayCode = c;
	    }
#endif				/* JP_CHARSET */
#ifndef KANJI_SYMBOLS
	    else if (!strcmp("-no-graph", argv[i]))
		no_graphic_char = TRUE;
#endif				/* not KANJI_SYMBOLS */
	    else if (!strcmp("-T", argv[i])) {
		if (++i >= argc)
		    usage();
		DefaultType = default_type = argv[i];
	    }
	    else if (!strcmp("-m", argv[i]))
		SearchHeader = search_header = TRUE;
	    else if (!strcmp("-v", argv[i]))
		visual_start = TRUE;
#ifdef USE_COLOR
	    else if (!strcmp("-M", argv[i]))
		useColor = FALSE;
#endif				/* USE_COLOR */
	    else if (!strcmp("-B", argv[i]))
		load_bookmark = TRUE;
	    else if (!strcmp("-bookmark", argv[i])) {
		if (++i >= argc)
		    usage();
		BookmarkFile = argv[i];
		if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/') {
		    Str tmp = Strnew_charp(CurrentDir);
		    if (Strlastchar(tmp) != '/')
			Strcat_char(tmp, '/');
		    Strcat_charp(tmp, BookmarkFile);
		    BookmarkFile = cleanupName(tmp->ptr);
		}
	    }
	    else if (!strcmp("-F", argv[i]))
		RenderFrame = TRUE;
	    else if (!strcmp("-W", argv[i])) {
		if (WrapSearch) {
		    WrapSearch = FALSE;
		}
		else {
		    WrapSearch = TRUE;
		}
	    }
	    else if (!strcmp("-dump", argv[i]))
		w3m_dump = DUMP_BUFFER;
	    else if (!strcmp("-dump_source", argv[i]))
		w3m_dump = DUMP_SOURCE;
	    else if (!strcmp("-dump_head", argv[i]))
		w3m_dump = DUMP_HEAD;
	    else if (!strcmp("-dump_both", argv[i]))
		w3m_dump = (DUMP_HEAD | DUMP_SOURCE);
	    else if (!strcmp("-dump_extra", argv[i]))
		w3m_dump = (DUMP_HEAD | DUMP_SOURCE | DUMP_EXTRA);
	    else if (!strcmp("-halfdump", argv[i]))
		w3m_dump = DUMP_HALFDUMP;
	    else if (!strcmp("-halfload", argv[i])) {
		w3m_dump = 0;
		w3m_halfload = TRUE;
		DefaultType = default_type = "text/html";
	    }
	    else if (!strcmp("-backend", argv[i])) {
		w3m_backend = TRUE;
	    }
	    else if (!strcmp("-backend_batch", argv[i])) {
		w3m_backend = TRUE;
		if (++i >= argc)
		    usage();
		if (!backend_batch_commands)
		    backend_batch_commands = newTextList();
		pushText(backend_batch_commands, argv[i]);
	    }
	    else if (!strcmp("-cols", argv[i])) {
		if (++i >= argc)
		    usage();
		COLS = atoi(argv[i]);
	    }
	    else if (!strcmp("-ppc", argv[i])) {
		double ppc;
		if (++i >= argc)
		    usage();
		ppc = atof(argv[i]);
		if (ppc >= MINIMUM_PIXEL_PER_CHAR &&
		    ppc <= MAXIMUM_PIXEL_PER_CHAR)
		    pixel_per_char = ppc;
	    }
	    else if (!strcmp("-num", argv[i]))
		showLineNum = TRUE;
	    else if (!strcmp("-no-proxy", argv[i]))
		Do_not_use_proxy = TRUE;
	    else if (!strcmp("-post", argv[i])) {
		if (++i >= argc)
		    usage();
		post_file = argv[i];
	    }
	    else if (!strcmp("-header", argv[i])) {
		Str hs;
		if (++i >= argc)
		    usage();
		if ((hs = make_optional_header_string(argv[i])) != NULL) {
		    if (header_string == NULL)
			header_string = hs;
		    else
			Strcat(header_string, hs);
		}
	    }
#ifdef USE_MOUSE
	    else if (!strcmp("-no-mouse", argv[i])) {
		use_mouse = FALSE;
	    }
#endif				/* USE_MOUSE */
#ifdef USE_COOKIE
	    else if (!strcmp("-no-cookie", argv[i])) {
		use_cookie = FALSE;
		accept_cookie = FALSE;
	    }
	    else if (!strcmp("-cookie", argv[i])) {
		use_cookie = TRUE;
		accept_cookie = TRUE;
	    }
#endif				/* USE_COOKIE */
	    else if (!strcmp("-pauth", argv[i])) {
		if (++i >= argc)
		    usage();
		proxy_auth_cookie = encodeB(argv[i]);
		while (argv[i][0]) {
		    argv[i][0] = '\0';
		    argv[i]++;
		}
	    }
#ifdef DEBIAN
	    else if (!strcmp("-s", argv[i]))
#else
	    else if (!strcmp("-S", argv[i]))
#endif
		squeezeBlankLine = TRUE;
	    else if (!strcmp("-X", argv[i]))
		Do_not_use_ti_te = TRUE;
	    else if (!strcmp("-o", argv[i])) {
		if (++i >= argc || !strcmp(argv[i], "?")) {
		    show_params_p = 1;
		    usage();
		}
		if (!set_param_option(argv[i])) {
		    /* option set failed */
		    fprintf(stderr, "%s: bad option\n", argv[i]);
		    show_params_p = 1;
		    usage();
		}
		option_assigned = 1;
	    }
	    else if (!strcmp("-dummy", argv[i])) {
		/* do nothing */
	    }
	    else if (!strcmp("-debug", argv[i]))
		w3m_debug = TRUE;
	    else {
		usage();
	    }
	}
	else if (*argv[i] == '+') {
	    line_str = argv[i] + 1;
	}
	else {
	    load_argv[load_argc++] = argv[i];
	}
	i++;
    }

    if (option_assigned)
	sync_with_option();

#ifdef	__WATT32__
    if (w3m_debug)
	dbug_init();
    sock_init();
#endif

    Firstbuf = NULL;
    Currentbuf = NULL;
    CurrentKey = -1;
    if (BookmarkFile == NULL)
	BookmarkFile = rcFile(BOOKMARK);

    if (!isatty(1) && !w3m_dump) {
	/* redirected output */
	w3m_dump = DUMP_BUFFER;
    }
    if (w3m_dump) {
	if (COLS == 0)
	    COLS = 80;
    }
#ifdef USE_BINMODE_STREAM
    setmode(fileno(stdout), O_BINARY);
#endif
    if (w3m_backend)
	backend();
    if (!w3m_dump) {
	initKeymap();
#ifdef USE_MENU
	initMenu();
	CurrentMenuData = NULL;
#endif				/* MENU */
	fmInit();
    }
    orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
    err_msg = Strnew();
    if (load_argc == 0) {
	if (w3m_halfdump)
	    printf("<pre>\n");
	/* no URL specified */
	if (!isatty(0)) {
	    redin = newFileStream(fdopen(dup(0), "rb"), (void (*)())pclose);
	    newbuf = openGeneralPagerBuffer(redin);
	    dup2(1, 0);
	}
	else if (load_bookmark) {
	    newbuf = loadGeneralFile(BookmarkFile, NULL, NO_REFERER, 0, NULL);
	    if (newbuf == NULL)
		Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
	}
	else if (visual_start) {
	    Str s_page;
	    s_page =
		Strnew_charp
		("<title>W3M startup page</title><center><b>Welcome to ");
	    Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
	    Strcat_m_charp(s_page,
			   "w3m</a>!<p><p>This is w3m version ",
			   version,
			   "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
			   NULL);
#ifdef DEBIAN
	    Strcat_m_charp(s_page,
			   "<p>Debian package is maintained by <a href='mailto:ukai@debian.or.jp'>Fumitoshi UKAI</a>.",
			   "You can read <a href='file:///usr/share/doc/w3m/'>w3m documents on your local system</a>.",
			   NULL);
#endif				/* DEBIAN */
	    newbuf = loadHTMLString(s_page);
	    if (newbuf == NULL)
		Strcat_charp(err_msg, "w3m: Can't load string.\n");
	    else if (newbuf != NO_BUFFER)
		newbuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
	}
	else if ((p = getenv("HTTP_HOME")) != NULL ||
		 (p = getenv("WWW_HOME")) != NULL) {
	    newbuf = loadGeneralFile(p, NULL, NO_REFERER, 0, NULL);
	    if (newbuf == NULL)
		Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
	}
	else {
	    if (fmInitialized)
		fmTerm();
	    usage();
	}
	if (newbuf == NULL) {
	    if (fmInitialized)
		fmTerm();
	    if (err_msg->length)
		fprintf(stderr, "%s", err_msg->ptr);
	    w3m_exit(2);
	}
	i = -1;
    }
    else {
	i = 0;
    }
    for (; i < load_argc; i++) {
	if (i >= 0) {
	    SearchHeader = search_header;
	    DefaultType = default_type;
	    if (w3m_halfdump)
		printf("<pre>\n");
	    if (w3m_dump == DUMP_HEAD) {
		request = New(FormList);
		request->method = FORM_METHOD_HEAD;
		newbuf =
		    loadGeneralFile(load_argv[i], NULL, NO_REFERER, 0,
				    request);
	    }
	    else {
		if (post_file && i == 0) {
		    FILE *fp;
		    Str body;
		    if (!strcmp(post_file, "-"))
			fp = stdin;
		    else
			fp = fopen(post_file, "r");
		    if (fp == NULL) {
			Strcat(err_msg,
			       Sprintf("w3m: Can't open %s.\n", post_file));
			continue;
		    }
		    body = Strfgetall(fp);
		    if (fp != stdin)
			fclose(fp);
		    request =
			newFormList(NULL, "post", NULL, NULL, NULL, NULL,
				    NULL);
		    request->body = body->ptr;
		    request->boundary = NULL;
		    request->length = body->length;
		}
		else {
		    request = NULL;
		}
		newbuf =
		    loadGeneralFile(load_argv[i], NULL, NO_REFERER, 0,
				    request);
	    }
	    if (newbuf == NULL) {
		Strcat(err_msg,
		       Sprintf("w3m: Can't load %s.\n", load_argv[i]));
		continue;
	    }
	    else if (newbuf == NO_BUFFER)
		continue;
	    switch (newbuf->real_scheme) {
#ifdef USE_NNTP
	    case SCM_NNTP:
	    case SCM_NEWS:
#endif				/* USE_NNTP */
	    case SCM_MAILTO:
		break;
	    case SCM_LOCAL:
	    case SCM_LOCAL_CGI:
		unshiftHist(LoadHist, conv_from_system(load_argv[i]));
		break;
	    default:
		pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
		break;
	    }
	}
	else if (newbuf == NO_BUFFER)
	    continue;
	newbuf->search_header = search_header;
	if (Currentbuf == NULL)
	    Firstbuf = Currentbuf = newbuf;
	else {
	    Currentbuf->nextBuffer = newbuf;
	    Currentbuf = newbuf;
	}
	if (w3m_dump) {
	    if (w3m_dump & DUMP_EXTRA)
		dump_extra(Currentbuf);
	    if (w3m_dump & DUMP_HEAD)
		dump_head(Currentbuf);
	    if (w3m_dump & DUMP_SOURCE)
		dump_source(Currentbuf);
	    if (w3m_dump == DUMP_BUFFER) {
		if (Currentbuf->frameset != NULL && RenderFrame)
		    rFrame();
		saveBuffer(Currentbuf, stdout);
	    }
	    if (w3m_halfdump)
		printf("</pre><title>%s</title>\n",
		       html_quote(newbuf->buffername));
	}
	else {
	    if (Currentbuf->frameset != NULL && RenderFrame)
		rFrame();
	    Currentbuf = newbuf;
#ifdef USE_BUFINFO
	    saveBufferInfo();
#endif
	}
    }
    if (w3m_dump) {
#ifdef USE_COOKIE
	save_cookies();
#endif				/* USE_COOKIE */
	w3m_exit(0);
    }

    if (!Firstbuf || Firstbuf == NO_BUFFER) {
	if (newbuf == NO_BUFFER) {
	    if (fmInitialized)
		inputChar("Hit any key to quit w3m:");
	}
	if (fmInitialized)
	    fmTerm();
	if (err_msg->length)
	    fprintf(stderr, "%s", err_msg->ptr);
	if (newbuf == NO_BUFFER) {
#ifdef USE_COOKIE
	    save_cookies();
#endif				/* USE_COOKIE */
	    if (!err_msg->length)
		w3m_exit(0);
	}
	w3m_exit(2);
    }
    if (err_msg->length)
	disp_message_nsec(err_msg->ptr, FALSE, 1, TRUE, FALSE);

    SearchHeader = FALSE;
    DefaultType = NULL;
#ifdef JP_CHARSET
    UseContentCharset = TRUE;
    UseAutoDetect = TRUE;
#endif

#ifdef SIGWINCH
    signal(SIGWINCH, resize_hook);
#else				/* not SIGWINCH */
    setlinescols();
    setupscreen();
#endif				/* not SIGWINCH */
#ifdef SIGCHLD
    signal(SIGCHLD, sig_chld);
#endif
    Currentbuf = Firstbuf;
    displayBuffer(Currentbuf, B_NORMAL);
    if (line_str) {
	_goLine(line_str);
    }
    onA();
    for (;;) {
	/* event processing */
	if (n_event_queue > 0) {
	    for (i = 0; i < n_event_queue; i++) {
		CurrentKey = -1;
		CurrentKeyData = eventQueue[i].user_data;
#ifdef USE_MENU
		CurrentMenuData = NULL;
#endif
		w3mFuncList[eventQueue[i].cmd].func();
	    }
	    n_event_queue = 0;
	}
	CurrentKeyData = NULL;
	/* get keypress event */
#ifdef USE_MOUSE
	if (use_mouse)
	    mouse_active();
#endif				/* USE_MOUSE */
#ifdef USE_ALARM
	if (alarm_status == AL_IMPLICIT) {
	    alarm_buffer = Currentbuf;
	    alarm_status = AL_IMPLICIT_DONE;
	}
	else if (alarm_status == AL_IMPLICIT_DONE
		 && alarm_buffer != Currentbuf) {
	    alarm_sec = 0;
	    alarm_status = AL_UNSET;
	}
	if (alarm_sec > 0) {
	    signal(SIGALRM, SigAlarm);
	    alarm(alarm_sec);
	}
#endif
	c = getch();
#ifdef USE_ALARM
	if (alarm_sec > 0) {
	    alarm(0);
	}
#endif
#ifdef USE_MOUSE
	if (use_mouse)
	    mouse_inactive();
#endif				/* USE_MOUSE */
	if (IS_ASCII(c)) {	/* Ascii */
	    if (((prec_num && c == '0') || '1' <= c) && (c <= '9')) {
		prec_num = prec_num * 10 + (int)(c - '0');
		if (prec_num > PREC_LIMIT)
		    prec_num = PREC_LIMIT;
	    }
	    else {
		set_buffer_environ(Currentbuf);
		keyPressEventProc((int)c);
		prec_num = 0;
	    }
	}
	prev_key = CurrentKey;
	CurrentKey = -1;
    }
}

static void
keyPressEventProc(int c)
{
    CurrentKey = c;
    w3mFuncList[(int)GlobalKeymap[c]].func();
    onA();
}

void
pushEvent(int event, void *user_data)
{
    if (n_event_queue < N_EVENT_QUEUE) {
	eventQueue[n_event_queue].cmd = event;
	eventQueue[n_event_queue].user_data = user_data;
	n_event_queue++;
    }
}

static void
dump_source(Buffer *buf)
{
    FILE *f;
    char c;
    if (buf->sourcefile == NULL)
	return;
    f = fopen(buf->sourcefile, "r");
    if (f == NULL)
	return;
    while (c = fgetc(f), !feof(f)) {
	putchar(c);
    }
    fclose(f);
}

static void
dump_head(Buffer *buf)
{
    TextListItem *ti;

    if (buf->document_header == NULL) {
	if (w3m_dump & DUMP_EXTRA)
	    printf("\n");
	return;
    }
    for (ti = buf->document_header->first; ti; ti = ti->next) {
	printf("%s", ti->ptr);
    }
    puts("");
}

static void
dump_extra(Buffer *buf)
{
    printf("W3m-current-url: %s\n", parsedURL2Str(&buf->currentURL)->ptr);
    if (buf->baseURL)
	printf("W3m-base-url: %s\n", parsedURL2Str(buf->baseURL)->ptr);
#ifdef JP_CHARSET
    printf("W3m-document-charset: %s\n", code_to_str(buf->document_code));
#endif
}

void
nulcmd(void)
{				/* do nothing */
}

#ifdef __EMX__
void
pcmap(void)
{
    w3mFuncList[(int)PcKeymap[(int)getch()]].func();
}
#else				/* not __EMX__ */
void
pcmap(void)
{
}
#endif

void
escmap(void)
{
    char c;
    c = getch();
    if (IS_ASCII(c)) {
	CurrentKey = K_ESC | c;
	w3mFuncList[(int)EscKeymap[(int)c]].func();
    }
}

void
escbmap(void)
{
    char c;
    c = getch();

    if (IS_DIGIT(c))
	escdmap(c);
    else if (IS_ASCII(c)) {
	CurrentKey = K_ESCB | c;
	w3mFuncList[(int)EscBKeymap[(int)c]].func();
    }
}

void
escdmap(char c)
{
    int d;

    d = (int)c - (int)'0';
    c = getch();
    if (IS_DIGIT(c)) {
	d = d * 10 + (int)c - (int)'0';
	c = getch();
    }
    if (c == '~') {
	CurrentKey = K_ESCD | d;
	w3mFuncList[(int)EscDKeymap[d]].func();
    }
}

void
tmpClearBuffer(Buffer *buf)
{
    if (buf->pagerSource == NULL && writeBufferCache(buf) == 0) {
	buf->firstLine = NULL;
	buf->topLine = NULL;
	buf->currentLine = NULL;
	buf->lastLine = NULL;
    }
}

static Str currentURL(void);

void
saveBufferInfo()
{
    FILE *fp;

    if (w3m_dump)
	return;
    if ((fp = fopen(rcFile("bufinfo"), "w")) == NULL) {
	return;
    }
    fprintf(fp, "%s\n", currentURL()->ptr);
    fclose(fp);
}

static void
pushBuffer(Buffer *buf)
{
    Buffer *b;

    if (clear_buffer)
	tmpClearBuffer(Currentbuf);
    if (Firstbuf == Currentbuf) {
	buf->nextBuffer = Firstbuf;
	Firstbuf = Currentbuf = buf;
    }
    else if ((b = prevBuffer(Firstbuf, Currentbuf)) != NULL) {
	b->nextBuffer = buf;
	buf->nextBuffer = Currentbuf;
	Currentbuf = buf;
    }
#ifdef USE_BUFINFO
    saveBufferInfo();
#endif

}

static void
delBuffer(Buffer *buf)
{
    if (buf == NULL)
	return;
    if (Currentbuf == buf)
	Currentbuf = buf->nextBuffer;
    Firstbuf = deleteBuffer(Firstbuf, buf);
    if (!Currentbuf)
	Currentbuf = Firstbuf;
}

static void
repBuffer(Buffer *oldbuf, Buffer *buf)
{
    Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
    Currentbuf = buf;
}


MySignalHandler
intTrap(SIGNAL_ARG)
{				/* Interrupt catcher */
    LONGJMP(IntReturn, 0);
    SIGNAL_RETURN;
}

#ifdef SIGWINCH
MySignalHandler
resize_hook(SIGNAL_ARG)
{
    setlinescols();
    setupscreen();
    if (Currentbuf)
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
    signal(SIGWINCH, resize_hook);
    SIGNAL_RETURN;
}
#endif				/* SIGWINCH */

/* 
 * Command functions: These functions are called with a keystroke.
 */

#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#define MIN(a, b)  ((a) < (b) ? (a) : (b))

static void
nscroll(int n, int mode)
{
    Line *curtop = Currentbuf->topLine;
    int lnum, tlnum, llnum, diff_n;

    if (Currentbuf->firstLine == NULL)
	return;
    lnum = Currentbuf->currentLine->linenumber;
    Currentbuf->topLine = lineSkip(Currentbuf, curtop, n, FALSE);
    if (Currentbuf->topLine == curtop) {
	lnum += n;
	if (lnum < Currentbuf->topLine->linenumber)
	    lnum = Currentbuf->topLine->linenumber;
	else if (lnum > Currentbuf->lastLine->linenumber)
	    lnum = Currentbuf->lastLine->linenumber;
    }
    else {
	tlnum = Currentbuf->topLine->linenumber;
	llnum = Currentbuf->topLine->linenumber + LASTLINE - 1;
#ifdef NEXTPAGE_TOPLINE
	if (nextpage_topline)
	    diff_n = 0;
	else
#endif
	    diff_n = n - (tlnum - curtop->linenumber);
	if (lnum < tlnum)
	    lnum = tlnum + diff_n;
	if (lnum > llnum)
	    lnum = llnum + diff_n;
    }
    gotoLine(Currentbuf, lnum);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, mode);
}

/* Move page forward */
void
pgFore(void)
{
#ifdef VI_PREC_NUM
    if (vi_prec_num)
	nscroll(searchKeyNum() * (LASTLINE - 1), B_NORMAL);
    else
#endif
	nscroll(prec_num ? searchKeyNum() : searchKeyNum() * (LASTLINE - 1),
		prec_num ? B_SCROLL : B_NORMAL);
}

/* Move page backward */
void
pgBack(void)
{
#ifdef VI_PREC_NUM
    if (vi_prec_num)
	nscroll(-searchKeyNum() * (LASTLINE - 1), B_NORMAL);
    else
#endif
	nscroll(-(prec_num ? searchKeyNum() : searchKeyNum() * (LASTLINE - 1)),
		prec_num ? B_SCROLL : B_NORMAL);
}

/* 1 line up */
void
lup1(void)
{
    nscroll(searchKeyNum(), B_SCROLL);
}

/* 1 line down */
void
ldown1(void)
{
    nscroll(-searchKeyNum(), B_SCROLL);
}

/* move cursor position to the center of screen */
void
ctrCsrV(void)
{
    int offsety;
    if (Currentbuf->firstLine == NULL)
	return;
    offsety = LASTLINE / 2 - Currentbuf->cursorY;
    if (offsety != 0) {
#if 0
	Currentbuf->currentLine = lineSkip(Currentbuf,
					   Currentbuf->currentLine, offsety,
					   FALSE);
#endif
	Currentbuf->topLine =
	    lineSkip(Currentbuf, Currentbuf->topLine, -offsety, FALSE);
	arrangeLine(Currentbuf);
	displayBuffer(Currentbuf, B_NORMAL);
    }
}

void
ctrCsrH(void)
{
    int offsetx;
    if (Currentbuf->firstLine == NULL)
	return;
    offsetx = Currentbuf->cursorX - Currentbuf->COLS / 2;
    if (offsetx != 0) {
	columnSkip(Currentbuf, offsetx);
	arrangeCursor(Currentbuf);
	displayBuffer(Currentbuf, B_NORMAL);
    }
}

/* Redraw screen */
void
rdrwSc(void)
{
    clear();
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Search regular expression forward */
void
srchfor(void)
{
    MySignalHandler(*prevtrap) ();
    char *str;
    int i, n = searchKeyNum();
    int wrapped = 0;

    str = inputStrHist("Forward: ", NULL, TextHist);
    if (str != NULL && *str == '\0')
	str = SearchString;
    if (str == NULL || *str == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    SearchString = str;
    prevtrap = signal(SIGINT, intTrap);
    crmode();
    if (SETJMP(IntReturn) == 0)
	for (i = 0; i < n; i++)
	    wrapped = forwardSearch(Currentbuf, SearchString);
    signal(SIGINT, prevtrap);
    term_raw();
    displayBuffer(Currentbuf, B_NORMAL);
    onA();
    if (wrapped) {
	disp_message("Search wrapped", FALSE);
    }
    searchRoutine = forwardSearch;
}

/* Search regular expression backward */
void
srchbak(void)
{
    MySignalHandler(*prevtrap) ();
    char *str;
    int i, n = searchKeyNum();
    int wrapped = 0;

    str = inputStrHist("Backward: ", NULL, TextHist);
    if (str != NULL && *str == '\0')
	str = SearchString;
    if (str == NULL || *str == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    SearchString = str;
    prevtrap = signal(SIGINT, intTrap);
    crmode();
    if (SETJMP(IntReturn) == 0)
	for (i = 0; i < n; i++)
	    wrapped = backwardSearch(Currentbuf, SearchString);
    signal(SIGINT, prevtrap);
    term_raw();
    displayBuffer(Currentbuf, B_NORMAL);
    onA();
    if (wrapped) {
	disp_message("Search wrapped", FALSE);
    }
    searchRoutine = backwardSearch;
}

static void
srch_nxtprv(int reverse)
{
    int i;
    int wrapped = 0;
    static int (*routine[2]) (Buffer *, char *) = {
    forwardSearch, backwardSearch};
    MySignalHandler(*prevtrap) ();

    if (searchRoutine == NULL) {
	disp_message("No previous regular expression", TRUE);
	return;
    }
    if (reverse != 0)
	reverse = 1;
    if (searchRoutine == backwardSearch)
	reverse ^= 1;
    prevtrap = signal(SIGINT, intTrap);
    crmode();
    if (SETJMP(IntReturn) == 0)
	for (i = 0; i < PREC_NUM; i++)
	    wrapped = (*routine[reverse]) (Currentbuf, SearchString);
    signal(SIGINT, prevtrap);
    term_raw();
    displayBuffer(Currentbuf, B_NORMAL);
    onA();
    if (wrapped) {
	disp_message("Search wrapped", FALSE);
    }
    else {
	disp_message(Sprintf("%s%s",
			     routine[reverse] ==
			     forwardSearch ? "Forward: " : "Backward: ",
			     SearchString)->ptr, FALSE);
    }
}

/* Search next matching */
void
srchnxt(void)
{
    srch_nxtprv(0);
}

/* Search previous matching */
void
srchprv(void)
{
    srch_nxtprv(1);
}

static void
shiftvisualpos(Buffer *buf, int shift)
{
    buf->visualpos -= shift;
    if (buf->visualpos >= buf->COLS)
	buf->visualpos = buf->COLS - 1;
    else if (buf->visualpos < 0)
	buf->visualpos = 0;
    arrangeLine(buf);
    if (buf->visualpos == -shift && buf->cursorX == 0)
	buf->visualpos = 0;
}

/* Shift screen left */
void
shiftl(void)
{
    int column;

    if (Currentbuf->firstLine == NULL)
	return;
    column = Currentbuf->currentColumn;
    columnSkip(Currentbuf, searchKeyNum() * (-Currentbuf->COLS + 1) + 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Shift screen right */
void
shiftr(void)
{
    int column;

    if (Currentbuf->firstLine == NULL)
	return;
    column = Currentbuf->currentColumn;
    columnSkip(Currentbuf, searchKeyNum() * (Currentbuf->COLS - 1) - 1);
    shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
    displayBuffer(Currentbuf, B_NORMAL);
}

void
col1R(void)
{
    Buffer *buf = Currentbuf;
    Line *l = buf->currentLine;
    int j, column, n = searchKeyNum();

    if (l == NULL)
	return;
    for (j = 0; j < n; j++) {
	column = buf->currentColumn;
	columnSkip(Currentbuf, 1);
	if (column == buf->currentColumn)
	    break;
	shiftvisualpos(Currentbuf, 1);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void
col1L(void)
{
    Buffer *buf = Currentbuf;
    Line *l = buf->currentLine;
    int j, n = searchKeyNum();

    if (l == NULL)
	return;
    for (j = 0; j < n; j++) {
	if (buf->currentColumn == 0)
	    break;
	columnSkip(Currentbuf, -1);
	shiftvisualpos(Currentbuf, -1);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void
setEnv(void)
{
    char *env;
    char *var, *value;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    env = searchKeyData();
    if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
	if (env != NULL && *env != '\0')
	    env = Sprintf("%s=", env)->ptr;
	env = inputStrHist("Set environ: ", env, TextHist);
	if (env == NULL || *env == '\0') {
	    displayBuffer(Currentbuf, B_NORMAL);
	    return;
	}
    }
    if ((value = strchr(env, '=')) != NULL && value > env) {
	var = allocStr(env, value - env);
	value++;
	set_environ(var, value);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void
pipeBuf(void)
{
    Buffer *buf;
    char *cmd, *tmpf;
    FILE *f;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
	cmd = inputLineHist("Pipe buffer to: ", "", IN_COMMAND, ShellHist);
	if (cmd == NULL || *cmd == '\0') {
	    displayBuffer(Currentbuf, B_NORMAL);
	    return;
	}
    }
    tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    f = fopen(tmpf, "w");
    if (f == NULL) {
	disp_message(Sprintf("Can't save buffer to %s", cmd)->ptr, TRUE);
	return;
    }
    saveBuffer(Currentbuf, f);
    fclose(f);
    pushText(fileToDelete, tmpf);
    if (strcasestr(cmd, "%s"))
	cmd = Sprintf(cmd, tmpf)->ptr;
    else
	cmd = Sprintf("%s < %s", cmd, tmpf)->ptr;
    buf = getpipe(cmd);
    if (buf == NULL) {
	disp_message("Execution failed", FALSE);
    }
    else {
	buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
	if (buf->type == NULL)
	    buf->type = "text/plain";
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command and read output ac pipe. */
void
pipesh(void)
{
    Buffer *buf;
    char *cmd;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
	cmd = inputLineHist("(read shell[pipe])!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
	cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    buf = getpipe(cmd);
    if (buf == NULL) {
	disp_message("Execution failed", FALSE);
    }
    else {
	buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
	if (buf->type == NULL)
	    buf->type = "text/plain";
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command and load entire output to buffer */
void
readsh(void)
{
    Buffer *buf;
    MySignalHandler(*prevtrap) ();
    char *cmd;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
	cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
	cmd = conv_to_system(cmd);
    if (cmd == NULL || *cmd == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    prevtrap = signal(SIGINT, intTrap);
    crmode();
    buf = getshell(cmd);
    signal(SIGINT, prevtrap);
    term_raw();
    if (buf == NULL) {
	disp_message("Execution failed", FALSE);
    }
    else {
	buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
	if (buf->type == NULL)
	    buf->type = "text/plain";
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command */
void
execsh(void)
{
    char *cmd;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    cmd = searchKeyData();
    if (cmd == NULL || *cmd == '\0') {
	cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist);
    }
    if (cmd != NULL)
	cmd = conv_to_system(cmd);
    if (cmd != NULL && *cmd != '\0') {
	fmTerm();
	system(cmd);
	printf("\n[Hit any key]");
	fflush(stdout);
	fmInit();
	getch();
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Load file */
void
ldfile(void)
{
    char *fn;

    fn = searchKeyData();
    if (fn == NULL || *fn == '\0') {
	fn = inputFilenameHist("(Load)Filename? ", NULL, LoadHist);
    }
    if (fn != NULL)
	fn = conv_to_system(fn);
    if (fn == NULL || *fn == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    cmd_loadfile(fn);
}

/* Load help file */
void
ldhelp(void)
{
    cmd_loadURL(helpFile(HELP_FILE), NULL);
}

static void
cmd_loadfile(char *fn)
{
    Buffer *buf;

    buf = loadGeneralFile(file_to_url(fn), NULL, NO_REFERER, 0, NULL);
    if (buf == NULL) {
	char *emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
	disp_err_message(emsg, FALSE);
    }
    else if (buf != NO_BUFFER) {
	pushBuffer(buf);
	if (RenderFrame && Currentbuf->frameset != NULL)
	    rFrame();
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Move cursor left */
static void
_movL(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
	return;
    for (i = 0; i < m; i++)
	cursorLeft(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

void
movL(void)
{
    _movL(Currentbuf->COLS / 2);
}

void
movL1(void)
{
    _movL(1);
}

/* Move cursor downward */
static void
_movD(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
	return;
    for (i = 0; i < m; i++)
	cursorDown(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

void
movD(void)
{
    _movD((LASTLINE + 1) / 2);
}

void
movD1(void)
{
    _movD(1);
}

/* move cursor upward */
static void
_movU(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
	return;
    for (i = 0; i < m; i++)
	cursorUp(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

void
movU(void)
{
    _movU((LASTLINE + 1) / 2);
}

void
movU1(void)
{
    _movU(1);
}

/* Move cursor right */
static void
_movR(int n)
{
    int i, m = searchKeyNum();
    if (Currentbuf->firstLine == NULL)
	return;
    for (i = 0; i < m; i++)
	cursorRight(Currentbuf, n);
    displayBuffer(Currentbuf, B_NORMAL);
}

void
movR(void)
{
    _movR(Currentbuf->COLS / 2);
}

void
movR1(void)
{
    _movR(1);
}

/* movLW, movRW */
/* 
 * From: Takashi Nishimoto <g96p0935@mse.waseda.ac.jp> Date: Mon, 14 Jun
 * 1999 09:29:56 +0900 */

#define IS_WORD_CHAR(c,p) (IS_ALNUM(c) && CharType(p) == PC_ASCII)

static int
prev_nonnull_line(Line *line)
{
    Line *l;

    for (l = line; l != NULL && l->len == 0; l = l->prev) ;
    if (l == NULL || l->len == 0)
	return -1;

    Currentbuf->currentLine = l;
    if (l != line)
	Currentbuf->pos = Currentbuf->currentLine->len;
    return 0;
}

void
movLW(void)
{
    char *lb;
    Lineprop *pb;
    Line *pline;
    int ppos;
    int i, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
	return;

    for (i = 0; i < n; i++) {
	pline = Currentbuf->currentLine;
	ppos = Currentbuf->pos;

	if (prev_nonnull_line(Currentbuf->currentLine) < 0)
	    goto end;

	while (1) {
	    lb = Currentbuf->currentLine->lineBuf;
	    pb = Currentbuf->currentLine->propBuf;
	    while (Currentbuf->pos > 0 &&
		   !IS_WORD_CHAR(lb[Currentbuf->pos - 1],
				 pb[Currentbuf->pos - 1])) {
		Currentbuf->pos--;
	    }
	    if (Currentbuf->pos > 0)
		break;
	    if (prev_nonnull_line(Currentbuf->currentLine->prev) < 0) {
		Currentbuf->currentLine = pline;
		Currentbuf->pos = ppos;
		goto end;
	    }
	    Currentbuf->pos = Currentbuf->currentLine->len;
	}

	lb = Currentbuf->currentLine->lineBuf;
	pb = Currentbuf->currentLine->propBuf;
	while (Currentbuf->pos > 0 &&
	       IS_WORD_CHAR(lb[Currentbuf->pos - 1],
			    pb[Currentbuf->pos - 1])) {
	    Currentbuf->pos--;
	}
    }
  end:
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

static int
next_nonnull_line(Line *line)
{
    Line *l;

    for (l = line; l != NULL && l->len == 0; l = l->next) ;

    if (l == NULL || l->len == 0)
	return -1;

    Currentbuf->currentLine = l;
    if (l != line)
	Currentbuf->pos = 0;
    return 0;
}

void
movRW(void)
{
    char *lb;
    Lineprop *pb;
    Line *pline;
    int ppos;
    int i, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
	return;

    for (i = 0; i < n; i++) {
	pline = Currentbuf->currentLine;
	ppos = Currentbuf->pos;

	if (next_nonnull_line(Currentbuf->currentLine) < 0)
	    goto end;

	lb = Currentbuf->currentLine->lineBuf;
	pb = Currentbuf->currentLine->propBuf;

	while (lb[Currentbuf->pos] &&
	       IS_WORD_CHAR(lb[Currentbuf->pos], pb[Currentbuf->pos]))
	    Currentbuf->pos++;

	while (1) {
	    while (lb[Currentbuf->pos] &&
		   !IS_WORD_CHAR(lb[Currentbuf->pos], pb[Currentbuf->pos]))
		Currentbuf->pos++;
	    if (lb[Currentbuf->pos])
		break;
	    if (next_nonnull_line(Currentbuf->currentLine->next) < 0) {
		Currentbuf->currentLine = pline;
		Currentbuf->pos = ppos;
		goto end;
	    }
	    Currentbuf->pos = 0;
	    lb = Currentbuf->currentLine->lineBuf;
	    pb = Currentbuf->currentLine->propBuf;
	}
    }
  end:
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Question and Quit */
void
qquitfm(void)
{
    char *ans;
    if (!confirm_on_quit)
	quitfm();
    ans = inputChar("Do you want to exit w3m? (y or n)");
    if (ans != NULL && tolower(*ans) == 'y')
	quitfm();
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Quit */
void
quitfm(void)
{
    fmTerm();
#ifdef USE_COOKIE
    save_cookies();
#endif				/* USE_COOKIE */
#ifdef USE_HISTORY
    if (SaveURLHist)
	saveHistory(URLHist, URLHistSize);
#endif				/* USE_HISTORY */
    w3m_exit(0);
}

/* Select buffer */
void
selBuf(void)
{
    Buffer *buf;
    int ok;
    char cmd;

    ok = FALSE;
    do {
	buf = selectBuffer(Firstbuf, Currentbuf, &cmd);
	switch (cmd) {
	case 'B':
	    ok = TRUE;
	    break;
	case '\n':
	case ' ':
	    Currentbuf = buf;
	    ok = TRUE;
	    break;
	case 'D':
	    delBuffer(buf);
	    if (Firstbuf == NULL) {
		/* No more buffer */
		Firstbuf = nullBuffer();
		Currentbuf = Firstbuf;
	    }
	    break;
	case 'q':
	    qquitfm();
	    break;
	case 'Q':
	    quitfm();
	    break;
	}
    } while (!ok);

    if (clear_buffer) {
	for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer)
	    tmpClearBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
void
susp(void)
{
#ifndef SIGSTOP
    char *shell;
#endif				/* not SIGSTOP */
    move(LASTLINE, 0);
    clrtoeolx();
    refresh();
    fmTerm();
#ifndef SIGSTOP
    shell = getenv("SHELL");
    if (shell == NULL)
	shell = "/bin/sh";
    system(shell);
#else				/* SIGSTOP */
    kill((pid_t) 0, SIGSTOP);
#endif				/* SIGSTOP */
    fmInit();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Go to specified line */
static void
_goLine(char *l)
{
    if (l == NULL || *l == '\0' || Currentbuf->currentLine == NULL) {
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
    Currentbuf->pos = 0;
    if (((*l == '^') || (*l == '$')) && prec_num) {
	gotoRealLine(Currentbuf, prec_num);
    }
    else if (*l == '^') {
	Currentbuf->topLine = Currentbuf->currentLine = Currentbuf->firstLine;
    }
    else if (*l == '$') {
	Currentbuf->topLine =
	    lineSkip(Currentbuf, Currentbuf->lastLine, -(LASTLINE + 1) / 2,
		     TRUE);
	Currentbuf->currentLine = Currentbuf->lastLine;
    }
    else
	gotoRealLine(Currentbuf, atoi(l));
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void
goLine(void)
{
    if (prec_num)
	_goLine("^");
    _goLine(inputStr("Goto line: ", ""));
    prec_num = 0;
}

void
goLineF(void)
{
    _goLine("^");
}

void
goLineL(void)
{
    _goLine("$");
}

/* Go to the beginning of the line */
void
linbeg(void)
{
    if (Currentbuf->firstLine == NULL)
	return;
    Currentbuf->pos = 0;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Go to the bottom of the line */
void
linend(void)
{
    if (Currentbuf->firstLine == NULL)
	return;
    Currentbuf->pos = Currentbuf->currentLine->len - 1;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* Run editor on the current buffer */
void
editBf(void)
{
    char *fn = Currentbuf->filename;
    char *type = Currentbuf->type;
    int top, linenum, cursorY, pos, currentColumn;
    Buffer *buf, *fbuf = NULL;
    Str cmd;

    if (fn == NULL || Currentbuf->pagerSource != NULL ||	/* Behaving as a pager */
	(Currentbuf->type == NULL && Currentbuf->edit == NULL) ||	/* Reading shell */
	Currentbuf->real_scheme != SCM_LOCAL || !strcmp(Currentbuf->currentURL.file, "-") ||	/* file is std *
												 * input  */
	Currentbuf->bufferprop & BP_FRAME) {	/* Frame */
	disp_err_message("Can't edit other than local file", TRUE);
	return;
    }
    if (Currentbuf->frameset != NULL)
	fbuf = Currentbuf->linkBuffer[LB_FRAME];
    if (Currentbuf->firstLine == NULL) {
	top = 1;
	linenum = 1;
    }
    else {
	top = Currentbuf->topLine->linenumber;
	linenum = Currentbuf->currentLine->linenumber;
    }
    cursorY = Currentbuf->cursorY;
    pos = Currentbuf->pos;
    currentColumn = Currentbuf->currentColumn;
    if (Currentbuf->edit) {
	cmd = unquote_mailcap(Currentbuf->edit,
			      Currentbuf->real_type,
			      fn,
			      checkHeader(Currentbuf, "Content-Type:"), NULL);
    }
    else {
	char *file = shell_quote(fn);
	if (strcasestr(Editor, "%s")) {
	    if (strcasestr(Editor, "%d"))
		cmd = Sprintf(Editor, linenum, file);
	    else
		cmd = Sprintf(Editor, file);
	}
	else {
	    if (strcasestr(Editor, "%d"))
		cmd = Sprintf(Editor, linenum);
	    else if (strcasestr(Editor, "vi"))
		cmd = Sprintf("%s +%d", Editor, linenum);
	    else
		cmd = Strnew_charp(Editor);
	    Strcat_m_charp(cmd, " ", file, NULL);
	}
    }
    fmTerm();
    system(cmd->ptr);
    fmInit();
    SearchHeader = Currentbuf->search_header;
    DefaultType = Currentbuf->real_type;
    buf = loadGeneralFile(file_to_url(fn), NULL, NO_REFERER, 0, NULL);
    SearchHeader = FALSE;
    DefaultType = NULL;
    if (buf == NULL) {
	disp_err_message("Re-loading failed", FALSE);
	buf = nullBuffer();
    }
    else if (buf == NO_BUFFER) {
	buf = nullBuffer();
    }
    buf->search_header = Currentbuf->search_header;
    if (fbuf != NULL)
	Firstbuf = deleteBuffer(Firstbuf, fbuf);
    repBuffer(Currentbuf, buf);
    if ((type != NULL) && (buf->type != NULL) &&
	((!strcasecmp(buf->type, "text/plain") &&
	  !strcasecmp(type, "text/html")) ||
	 (!strcasecmp(buf->type, "text/html") &&
	  !strcasecmp(type, "text/plain")))) {
	vwSrc();
	if (Currentbuf != buf)
	    Firstbuf = deleteBuffer(Firstbuf, buf);
    }
    if (Currentbuf->firstLine == NULL) {
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
    Currentbuf->topLine =
	lineSkip(Currentbuf, Currentbuf->topLine, top - 1, FALSE);
    gotoLine(Currentbuf, linenum);
    Currentbuf->pos = pos;
    Currentbuf->currentColumn = currentColumn;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Run editor on the current screen */
void
editScr(void)
{
    int lnum;
    Str cmd;
    Str tmpf;
    FILE *f;

    tmpf = tmpfname(TMPF_DFL, NULL);
    f = fopen(tmpf->ptr, "w");
    if (f == NULL) {
	cmd = Sprintf("Can't open %s", tmpf->ptr);
	disp_err_message(cmd->ptr, TRUE);
	return;
    }
    saveBuffer(Currentbuf, f);
    fclose(f);
    if (Currentbuf->currentLine)
	lnum = Currentbuf->currentLine->linenumber;
    else
	lnum = 1;
    if (strcasestr(Editor, "%s")) {
	if (strcasestr(Editor, "%d"))
	    cmd = Sprintf(Editor, lnum, tmpf->ptr);
	else
	    cmd = Sprintf(Editor, tmpf->ptr);
    }
    else {
	if (strcasestr(Editor, "%d"))
	    cmd = Sprintf(Editor, lnum);
	else if (strcasestr(Editor, "vi"))
	    cmd = Sprintf("%s +%d", Editor, lnum);
	else
	    cmd = Strnew_charp(Editor);
	Strcat_m_charp(cmd, " ", tmpf->ptr, NULL);
    }
    fmTerm();
    system(cmd->ptr);
    fmInit();
    unlink(tmpf->ptr);
    gotoLine(Currentbuf, lnum);
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

#ifdef USE_MARK

/* Set / unset mark */
void
_mark(void)
{
    Line *l;
    if (!use_mark)
	return;
    if (Currentbuf->firstLine == NULL)
	return;
    l = Currentbuf->currentLine;
    cmd_mark(&l->propBuf[Currentbuf->pos]);
    redrawLine(Currentbuf, l, l->linenumber - Currentbuf->topLine->linenumber);
}

static void
cmd_mark(Lineprop *p)
{
    if (!use_mark)
	return;
    if ((*p & PM_MARK) && (*p & PE_STAND))
	*p &= ~PE_STAND;
    else if (!(*p & PM_MARK) && !(*p & PE_STAND))
	*p |= PE_STAND;
    *p ^= PM_MARK;
}

/* Go to next mark */
void
nextMk(void)
{
    Line *l;
    int i;

    if (!use_mark)
	return;
    if (Currentbuf->firstLine == NULL)
	return;
    i = Currentbuf->pos + 1;
    l = Currentbuf->currentLine;
    if (i >= l->len) {
	i = 0;
	l = l->next;
    }
    while (l != NULL) {
	for (; i < l->len; i++) {
	    if (l->propBuf[i] & PM_MARK) {
		Currentbuf->currentLine = l;
		Currentbuf->pos = i;
		arrangeCursor(Currentbuf);
		displayBuffer(Currentbuf, B_NORMAL);
		return;
	    }
	}
	l = l->next;
	i = 0;
    }
    disp_message("No mark exist after here", TRUE);
}

/* Go to previous mark */
void
prevMk(void)
{
    Line *l;
    int i;

    if (!use_mark)
	return;
    if (Currentbuf->firstLine == NULL)
	return;
    i = Currentbuf->pos - 1;
    l = Currentbuf->currentLine;
    if (i < 0) {
	l = l->prev;
	if (l != NULL)
	    i = l->len - 1;
    }
    while (l != NULL) {
	for (; i >= 0; i--) {
	    if (l->propBuf[i] & PM_MARK) {
		Currentbuf->currentLine = l;
		Currentbuf->pos = i;
		arrangeCursor(Currentbuf);
		displayBuffer(Currentbuf, B_NORMAL);
		return;
	    }
	}
	l = l->prev;
	if (l != NULL)
	    i = l->len - 1;
    }
    disp_message("No mark exist before here", TRUE);
}

/* Mark place to which the regular expression matches */
void
reMark(void)
{
    Line *l;
    char *str;
    char *p, *p1, *p2;

    if (!use_mark)
	return;
    str = inputStrHist("(Mark)Regexp: ", MarkString, TextHist);
    if (str == NULL || *str == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    if ((p = regexCompile(str, 1)) != NULL) {
	disp_message(p, TRUE);
	return;
    }
    MarkString = str;
    for (l = Currentbuf->firstLine; l != NULL; l = l->next) {
	p = l->lineBuf;
	for (;;) {
	    if (regexMatch(p, &l->lineBuf[l->len] - p, p == l->lineBuf) == 1) {
		matchedPosition(&p1, &p2);
		cmd_mark(l->propBuf + (p1 - l->lineBuf));
		p = p2;
	    }
	    else
		break;
	}
    }

    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}
#endif				/* USE_MARK */

#ifdef JP_CHARSET
static char *
cURLcode(char *url, Buffer *buf)
{
    char *p;
    Str s;

    for (p = url; *p; p++) {
	if (!IS_ASCII(*p)) {
	    /* URL contains Kanji... uugh */
	    s = conv(url, InnerCode, buf->document_code);
	    return s->ptr;
	}
    }
    return url;
}
#else				/* not JP_CHARSET */
#define cURLcode(url,buf) (url)
#endif				/* not JP_CHARSET */

static Buffer *
loadNormalBuf(Buffer *buf, int renderframe)
{
    pushBuffer(buf);
    if (renderframe && RenderFrame && Currentbuf->frameset != NULL)
	rFrame();
    return buf;
}

static Buffer *
loadLink(char *url, char *target, char *referer, FormList *request)
{
    Buffer *buf, *nfbuf;
    union frameset_element *f_element = NULL;
    int flag = 0;
    ParsedURL *base, pu;

    message(Sprintf("loading %s", url)->ptr, 0, 0);
    refresh();

    base = baseURL(Currentbuf);
    if (base == NULL ||
	base->scheme == SCM_LOCAL || base->scheme == SCM_LOCAL_CGI)
	referer = NO_REFERER;
    if (referer == NULL)
	referer = parsedURL2Str(&Currentbuf->currentURL)->ptr;
    buf = loadGeneralFile(cURLcode(url, Currentbuf),
			  baseURL(Currentbuf), referer, flag, request);
    if (buf == NULL) {
	char *emsg = Sprintf("Can't load %s", url)->ptr;
	disp_err_message(emsg, FALSE);
	return NULL;
    }

    parseURL2(url, &pu, base);
    pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);

    if (buf == NO_BUFFER) {
	return NULL;
    }
    if (!on_target)		/* open link as an indivisual page */
	return loadNormalBuf(buf, TRUE);

    if (do_download)		/* download (thus no need to render frame) */
	return loadNormalBuf(buf, FALSE);

    if (target == NULL ||	/* no target specified (that means * this
				 * page is not a frame page) */
	!strcmp(target, "_top") ||	/* this link is specified to * be
					 * opened as an indivisual * page */
	!(Currentbuf->bufferprop & BP_FRAME)	/* This page is not a *
						 * * * * * frame page */
	) {
	return loadNormalBuf(buf, TRUE);
    }
    nfbuf = Currentbuf->linkBuffer[LB_N_FRAME];
    if (nfbuf == NULL) {
	/* original page (that contains <frameset> tag) doesn't exist */
	return loadNormalBuf(buf, TRUE);
    }

    f_element = search_frame(nfbuf->frameset, target);
    if (f_element == NULL) {
	/* specified target doesn't exist in this frameset */
	return loadNormalBuf(buf, TRUE);
    }

    /* frame page */

    /* stack current frameset */
    pushFrameTree(&(nfbuf->frameQ), copyFrameSet(nfbuf->frameset), Currentbuf);
    /* delete frame view buffer */
    delBuffer(Currentbuf);
    Currentbuf = nfbuf;
    /* nfbuf->frameset = copyFrameSet(nfbuf->frameset); */
    resetFrameElement(f_element, buf, referer, request);
    discardBuffer(buf);
    rFrame();
    {
	Anchor *al = NULL;
	char *label = pu.label;

	if (label && f_element->element->attr == F_BODY) {
	    al = searchAnchor(f_element->body->nameList, label);
	}
	if (!al) {
	    label = Strnew_m_charp("_", target, NULL)->ptr;
	    al = searchURLLabel(Currentbuf, label);
	}
	if (al) {
	    gotoLine(Currentbuf, al->start.line);
#ifdef LABEL_TOPLINE
	    if (label_topline)
		Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
					       Currentbuf->currentLine->
					       linenumber -
					       Currentbuf->topLine->linenumber,
					       FALSE);
#endif
	    Currentbuf->pos = al->start.pos;
	    arrangeCursor(Currentbuf);
	}
    }
    displayBuffer(Currentbuf, B_NORMAL);
    return buf;
}

static void
gotoLabel(char *label)
{
    Buffer *buf;
    Anchor *al;
    int i;

    al = searchURLLabel(Currentbuf, label);
    if (al == NULL) {
	disp_message(Sprintf("%s is not found", label)->ptr, TRUE);
	return;
    }
    buf = newBuffer(Currentbuf->width);
    copyBuffer(buf, Currentbuf);
    for (i = 0; i < MAX_LB; i++)
	buf->linkBuffer[i] = NULL;
    buf->currentURL.label = allocStr(label, 0);
    pushHashHist(URLHist, parsedURL2Str(&buf->currentURL)->ptr);
    (*buf->clone)++;
    pushBuffer(buf);
    gotoLine(Currentbuf, al->start.line);
#ifdef LABEL_TOPLINE
    if (label_topline)
	Currentbuf->topLine = lineSkip(Currentbuf, Currentbuf->topLine,
				       Currentbuf->currentLine->linenumber
				       - Currentbuf->topLine->linenumber,
				       FALSE);
#endif
    Currentbuf->pos = al->start.pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
}

/* follow HREF link */
void
followA(void)
{
    Line *l;
    Anchor *a;
    ParsedURL u;

    if (Currentbuf->firstLine == NULL)
	return;
    l = Currentbuf->currentLine;

    a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL) {
	_followForm(FALSE);
	return;
    }
    if (*a->url == '#') {	/* index within this buffer */
	gotoLabel(a->url + 1);
	return;
    }
    parseURL2(a->url, &u, baseURL(Currentbuf));
    if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&Currentbuf->currentURL)) == 0) {
	/* index within this buffer */
	if (u.label) {
	    gotoLabel(u.label);
	    return;
	}
    }
    if (!strncasecmp(a->url, "mailto:", 7)) {
	/* invoke external mailer */
	Str tmp;
	char *to = shell_quote(url_unquote(a->url + 7));
	if (strcasestr(Mailer, "%s"))
	    tmp = Sprintf(Mailer, to);
	else
	    tmp = Strnew_m_charp(Mailer, " ", to, NULL);
	fmTerm();
	system(tmp->ptr);
	fmInit();
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
#ifdef USE_NNTP
    else if (!strncasecmp(a->url, "news:", 5) && strchr(a->url, '@') == NULL) {
	/* news:newsgroup is not supported */
	disp_err_message("news:newsgroup_name is not supported", TRUE);
	return;
    }
#endif				/* USE_NNTP */
    loadLink(a->url, a->target, a->referer, NULL);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* follow HREF link in the buffer */
void
bufferA(void)
{
    on_target = FALSE;
    followA();
    on_target = TRUE;
}

/* view inline image */
void
followI(void)
{
    Line *l;
    Anchor *a;
    Buffer *buf;

    if (Currentbuf->firstLine == NULL)
	return;
    l = Currentbuf->currentLine;

    a = retrieveCurrentImg(Currentbuf);
    if (a == NULL)
	return;
    message(Sprintf("loading %s", a->url)->ptr, 0, 0);
    refresh();
    buf =
	loadGeneralFile(cURLcode(a->url, Currentbuf), baseURL(Currentbuf),
			NULL, 0, NULL);
    if (buf == NULL) {
	char *emsg = Sprintf("Can't load %s", a->url)->ptr;
	disp_err_message(emsg, FALSE);
    }
    else if (buf != NO_BUFFER) {
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

static FormItemList *
save_submit_formlist(FormItemList *src)
{
    FormList *list;
    FormList *srclist;
    FormItemList *srcitem;
    FormItemList *item;
    FormItemList *ret = NULL;
#ifdef MENU_SELECT
    FormSelectOptionItem *opt;
    FormSelectOptionItem *curopt;
    FormSelectOptionItem *srcopt;
#endif				/* MENU_SELECT */

    if (src == NULL)
	return NULL;
    srclist = src->parent;
    list = New(FormList);
    list->method = srclist->method;
    list->action = Strdup(srclist->action);
    list->charset = srclist->charset;
    list->enctype = srclist->enctype;
    list->nitems = srclist->nitems;
    list->body = srclist->body;
    list->boundary = srclist->boundary;
    list->length = srclist->length;

    for (srcitem = srclist->item; srcitem; srcitem = srcitem->next) {
	item = New(FormItemList);
	item->type = srcitem->type;
	item->name = Strdup(srcitem->name);
	item->value = Strdup(srcitem->value);
	item->checked = srcitem->checked;
	item->accept = srcitem->accept;
	item->size = srcitem->size;
	item->rows = srcitem->rows;
	item->maxlength = srcitem->maxlength;
	item->readonly = srcitem->readonly;
#ifdef MENU_SELECT
	opt = curopt = NULL;
	for (srcopt = srcitem->select_option; srcopt; srcopt = srcopt->next) {
	    if (!srcopt->checked)
		continue;
	    opt = New(FormSelectOptionItem);
	    opt->value = Strdup(srcopt->value);
	    opt->label = Strdup(srcopt->label);
	    opt->checked = srcopt->checked;
	    if (item->select_option == NULL) {
		item->select_option = curopt = opt;
	    }
	    else {
		curopt->next = opt;
		curopt = curopt->next;
	    }
	}
	item->select_option = opt;
	if (srcitem->label)
	    item->label = Strdup(srcitem->label);
#endif				/* MENU_SELECT */
	item->parent = list;
	item->next = NULL;

	if (list->lastitem == NULL) {
	    list->item = list->lastitem = item;
	}
	else {
	    list->lastitem->next = item;
	    list->lastitem = item;
	}

	if (srcitem == src)
	    ret = item;
    }

    return ret;
}

#ifdef JP_CHARSET
static Str
conv_form_encoding(Str val, FormItemList *fi, Buffer *buf)
{
    return conv_str(val, InnerCode, fi->parent->charset ? fi->parent->charset
		    : (buf->document_code ? buf->document_code : CODE_EUC));
}
#else
#define conv_form_encoding(val, fi, buf) (val)
#endif

static void
query_from_followform(Str *query, FormItemList *fi, int multipart)
{
    FormItemList *f2;
    FILE *body = NULL;

    if (multipart) {
	*query = tmpfname(TMPF_DFL, NULL);
	body = fopen((*query)->ptr, "w");
	if (body == NULL) {
	    return;
	}
	fi->parent->body = (*query)->ptr;
	fi->parent->boundary =
	    Sprintf("------------------------------%d%ld%ld%ld", getpid(),
		    fi->parent, fi->parent->body, fi->parent->boundary)->ptr;
    }
    *query = Strnew();
    for (f2 = fi->parent->item; f2; f2 = f2->next) {
	if (f2->name == NULL)
	    continue;
	/* <ISINDEX> is translated into single text form */
	if (f2->name->length == 0 &&
	    (multipart || f2->type != FORM_INPUT_TEXT))
	    continue;
	switch (f2->type) {
	case FORM_INPUT_RESET:
	    /* do nothing */
	    continue;
	case FORM_INPUT_SUBMIT:
	case FORM_INPUT_IMAGE:
	    if (f2 != fi || f2->value == NULL)
		continue;
	    break;
	case FORM_INPUT_RADIO:
	case FORM_INPUT_CHECKBOX:
	    if (!f2->checked)
		continue;
	}
	if (multipart) {
	    if (f2->type == FORM_INPUT_IMAGE) {
		*query = Strdup(conv_form_encoding(f2->name, fi, Currentbuf));
		Strcat_charp(*query, ".x");
		form_write_data(body, fi->parent->boundary, (*query)->ptr,
				"1");
		*query = Strdup(f2->name);
		Strcat_charp(*query, ".y");
		form_write_data(body, fi->parent->boundary, (*query)->ptr,
				"1");
	    }
	    else if (f2->name && f2->name->length > 0 && f2->value != NULL) {
		/* not IMAGE */
		*query = conv_form_encoding(f2->value, fi, Currentbuf);
		if (f2->type == FORM_INPUT_FILE)
		    form_write_from_file(body, fi->parent->boundary,
					 conv_form_encoding(f2->name, fi,
							    Currentbuf)->ptr,
					 (*query)->ptr,
					 Str_conv_to_system(f2->value)->ptr);
		else
		    form_write_data(body, fi->parent->boundary,
				    conv_form_encoding(f2->name, fi,
						       Currentbuf)->ptr,
				    (*query)->ptr);
	    }
	}
	else {
	    /* not multipart */
	    if (f2->type == FORM_INPUT_IMAGE) {
		Strcat(*query, conv_form_encoding(f2->name, fi, Currentbuf));
		Strcat_charp(*query, ".x=1&");
		Strcat(*query, conv_form_encoding(f2->name, fi, Currentbuf));
		Strcat_charp(*query, ".y=1");
	    }
	    else {
		/* not IMAGE */
		if (f2->name && f2->name->length > 0) {
		    Strcat(*query,
			   Str_form_quote(conv_form_encoding
					  (f2->name, fi, Currentbuf)));
		    Strcat_char(*query, '=');
		}
		if (f2->value != NULL) {
		    if (fi->parent->method == FORM_METHOD_INTERNAL)
			Strcat(*query, Str_form_quote(f2->value));
		    else {
			Strcat(*query,
			       Str_form_quote(conv_form_encoding
					      (f2->value, fi, Currentbuf)));
		    }
		}
	    }
	    if (f2->next)
		Strcat_char(*query, '&');
	}
    }
    if (multipart) {
	fprintf(body, "--%s--\r\n", fi->parent->boundary);
	fclose(body);
    }
    else {
	/* remove trailing & */
	while (Strlastchar(*query) == '&')
	    Strshrink(*query, 1);
    }
}

/* submit form */
void
submitForm(void)
{
    _followForm(TRUE);
}

/* process form */
void
followForm(void)
{
    _followForm(FALSE);
}

static void
_followForm(int submit)
{
    Line *l;
    Anchor *a, *a2;
    char *p;
    FormItemList *fi, *f2;
    Str tmp, tmp2;
    int multipart = 0, i;

    if (Currentbuf->firstLine == NULL)
	return;
    l = Currentbuf->currentLine;

    a = retrieveCurrentForm(Currentbuf);
    if (a == NULL)
	return;
    fi = (FormItemList *)a->url;
    switch (fi->type) {
    case FORM_INPUT_TEXT:
	if (submit)
	    goto do_submit;
	if (fi->readonly)
	    disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
	p = inputStrHist("TEXT:", fi->value ? fi->value->ptr : NULL, TextHist);
	if (p == NULL || fi->readonly)
	    return;
	fi->value = Strnew_charp(p);
	formUpdateBuffer(a, Currentbuf, fi);
	if (fi->accept || fi->parent->nitems == 1)
	    goto do_submit;
	break;
    case FORM_INPUT_FILE:
	if (submit)
	    goto do_submit;
	if (fi->readonly)
	    disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
	p = inputFilenameHist("Filename:", fi->value ? fi->value->ptr : NULL,
			      NULL);
	if (p == NULL || fi->readonly)
	    return;
	fi->value = Strnew_charp(p);
	formUpdateBuffer(a, Currentbuf, fi);
	if (fi->accept || fi->parent->nitems == 1)
	    goto do_submit;
	break;
    case FORM_INPUT_PASSWORD:
	if (submit)
	    goto do_submit;
	if (fi->readonly) {
	    disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
	    return;
	}
	p = inputLine("TEXT:", fi->value ? fi->value->ptr : NULL, IN_PASSWORD);
	if (p == NULL)
	    return;
	fi->value = Strnew_charp(p);
	formUpdateBuffer(a, Currentbuf, fi);
	if (fi->accept)
	    goto do_submit;
	break;
    case FORM_TEXTAREA:
	if (submit)
	    goto do_submit;
	if (fi->readonly)
	    disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
	input_textarea(fi);
	formUpdateBuffer(a, Currentbuf, fi);
	break;
    case FORM_INPUT_RADIO:
	if (submit)
	    goto do_submit;
	if (fi->readonly) {
	    disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
	    return;
	}
	formRecheckRadio(a, Currentbuf, fi);
	break;
    case FORM_INPUT_CHECKBOX:
	if (submit)
	    goto do_submit;
	if (fi->readonly) {
	    disp_message_nsec("Read only field!", FALSE, 1, TRUE, FALSE);
	    return;
	}
	fi->checked = !fi->checked;
	formUpdateBuffer(a, Currentbuf, fi);
	break;
#ifdef MENU_SELECT
    case FORM_SELECT:
	if (submit)
	    goto do_submit;
	if (!formChooseOptionByMenu(fi,
				    Currentbuf->cursorX - Currentbuf->pos +
				    a->start.pos + Currentbuf->rootX,
				    Currentbuf->cursorY))
	    break;
	formUpdateBuffer(a, Currentbuf, fi);
	if (fi->parent->nitems == 1)
	    goto do_submit;
	break;
#endif				/* MENU_SELECT */
    case FORM_INPUT_IMAGE:
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_BUTTON:
      do_submit:
	tmp = Strnew();
	tmp2 = Strnew();
	multipart = (fi->parent->method == FORM_METHOD_POST &&
		     fi->parent->enctype == FORM_ENCTYPE_MULTIPART);
	query_from_followform(&tmp, fi, multipart);

	tmp2 = Strdup(fi->parent->action);
	if (!Strcmp_charp(tmp2, "!CURRENT_URL!")) {
	    /* It means "current URL" */
	    tmp2 = parsedURL2Str(&Currentbuf->currentURL);
	    if ((p = strchr(tmp2->ptr, '?')) != NULL)
		Strshrink(tmp2, (tmp2->ptr + tmp2->length) - p);
	}

	if (fi->parent->method == FORM_METHOD_GET) {
	    Strcat_charp(tmp2, "?");
	    Strcat(tmp2, tmp);
	    loadLink(tmp2->ptr, a->target, NULL, NULL);
	}
	else if (fi->parent->method == FORM_METHOD_POST) {
	    Buffer *buf;
	    if (multipart) {
		struct stat st;
		stat(fi->parent->body, &st);
		fi->parent->length = st.st_size;
	    }
	    else {
		fi->parent->body = tmp->ptr;
		fi->parent->length = tmp->length;
	    }
	    buf = loadLink(tmp2->ptr, a->target, NULL, fi->parent);
	    if (multipart) {
		unlink(fi->parent->body);
	    }
	    if (buf && !(buf->bufferprop & BP_REDIRECTED)) {	/* buf must be Currentbuf */
		/* BP_REDIRECTED means that the buffer is obtained through
		 * Location: header. In this case, buf->form_submit must not be set
		 * because the page is not loaded by POST method but GET method.
		 */
		buf->form_submit = save_submit_formlist(fi);
	    }
	}
	else if ((fi->parent->method == FORM_METHOD_INTERNAL && Strcmp_charp(fi->parent->action, "map") == 0) || Currentbuf->bufferprop & BP_INTERNAL) {	/* internal */
	    do_internal(tmp2->ptr, tmp->ptr);
	    return;
	}
	else {
	    disp_err_message("Can't send form because of illegal method.",
			     FALSE);
	}
	break;
    case FORM_INPUT_RESET:
	for (i = 0; i < Currentbuf->formitem->nanchor; i++) {
	    a2 = &Currentbuf->formitem->anchors[i];
	    f2 = (FormItemList *)a2->url;
	    if (f2->parent == fi->parent &&
		f2->name && f2->value &&
		f2->type != FORM_INPUT_SUBMIT &&
		f2->type != FORM_INPUT_HIDDEN &&
		f2->type != FORM_INPUT_RESET) {
		f2->value = f2->init_value;
		f2->checked = f2->init_checked;
#ifdef MENU_SELECT
		f2->label = f2->init_label;
		f2->selected = f2->init_selected;
#endif				/* MENU_SELECT */
		formUpdateBuffer(a2, Currentbuf, f2);
	    }
	}
	break;
    case FORM_INPUT_HIDDEN:
    default:
	break;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static void
drawAnchorCursor0(Buffer *buf, int hseq, int prevhseq, int tline, int eline,
		  int active)
{
    int i, j;
    Line *l;
    Anchor *an;

    l = buf->topLine;
    for (j = 0; j < buf->href->nanchor; j++) {
	an = &buf->href->anchors[j];
	if (an->start.line < tline)
	    continue;
	if (an->start.line >= eline)
	    return;
	for (;; l = l->next) {
	    if (l == NULL)
		return;
	    if (l->linenumber == an->start.line)
		break;
	}
	if (hseq >= 0 && an->hseq == hseq) {
	    for (i = an->start.pos; i < an->end.pos; i++) {
		if (l->propBuf[i] & (PE_IMAGE | PE_ANCHOR | PE_FORM)) {
		    if (active)
			l->propBuf[i] |= PE_ACTIVE;
		    else
			l->propBuf[i] &= ~PE_ACTIVE;
		}
	    }
	    if (active)
		redrawLineRegion(buf, l, l->linenumber - tline,
				 an->start.pos, an->end.pos);
	}
	else if (prevhseq >= 0 && an->hseq == prevhseq) {
	    if (active)
		redrawLineRegion(buf, l, l->linenumber - tline,
				 an->start.pos, an->end.pos);
	}
    }
}


void
drawAnchorCursor(Buffer *buf)
{
    Anchor *an;
    int hseq, prevhseq;
    int tline, eline;

    if (buf->firstLine == NULL)
	return;
    if (buf->href == NULL)
	return;

    an = retrieveCurrentAnchor(buf);
    if (an != NULL)
	hseq = an->hseq;
    else
	hseq = -1;
    tline = buf->topLine->linenumber;
    eline = tline + LASTLINE;
    prevhseq = buf->hmarklist->prevhseq;

    drawAnchorCursor0(buf, hseq, prevhseq, tline, eline, 1);
    drawAnchorCursor0(buf, hseq, -1, tline, eline, 0);
    buf->hmarklist->prevhseq = hseq;
}

/* underline an anchor if cursor is on the anchor. */
void
onA(void)
{
    drawAnchorCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the top anchor */
void
topA(void)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    BufferPoint *po;
    Anchor *an;
    int hseq;

    if (Currentbuf->firstLine == NULL)
	return;
    if (!hl || hl->nmark == 0)
	return;

    hseq = 0;
    do {
	if (hseq >= hl->nmark)
	    return;
	po = hl->marks + hseq;
	an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
	if (an == NULL)
	    an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
	hseq++;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the last anchor */
void
lastA(void)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    BufferPoint *po;
    Anchor *an;
    int hseq;

    if (Currentbuf->firstLine == NULL)
	return;
    if (!hl || hl->nmark == 0)
	return;

    hseq = hl->nmark - 1;
    do {
	if (hseq < 0)
	    return;
	po = hl->marks + hseq;
	an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
	if (an == NULL)
	    an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
	hseq--;
    } while (an == NULL);

    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next anchor */
void
nextA(void)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    BufferPoint *po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
	return;
    if (!hl || hl->nmark == 0)
	return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (an == NULL)
	an = retrieveCurrentForm(Currentbuf);

    y = Currentbuf->currentLine->linenumber;
    x = Currentbuf->pos;

    for (i = 0; i < n; i++) {
	pan = an;
	if (an && an->hseq >= 0) {
	    int hseq = an->hseq + 1;
	    do {
		if (hseq >= hl->nmark) {
		    pan = an;
		    goto _end;
		}
		po = &hl->marks[hseq];
		an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
		if (an == NULL)
		    an = retrieveAnchor(Currentbuf->formitem, po->line,
					po->pos);
		hseq++;
	    } while (an == NULL || an == pan);
	}
	else {
	    an = closest_next_anchor(Currentbuf->href, NULL, x, y);
	    an = closest_next_anchor(Currentbuf->formitem, an, x, y);
	    if (an == NULL) {
		an = pan;
		break;
	    }
	    x = an->start.pos;
	    y = an->start.line;
	}
    }

  _end:
    if (an == NULL || an->hseq < 0)
	return;
    po = &hl->marks[an->hseq];
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the previous anchor */
void
prevA(void)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    BufferPoint *po;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
	return;
    if (!hl || hl->nmark == 0)
	return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (an == NULL)
	an = retrieveCurrentForm(Currentbuf);

    y = Currentbuf->currentLine->linenumber;
    x = Currentbuf->pos;

    for (i = 0; i < n; i++) {
	pan = an;
	if (an && an->hseq >= 0) {
	    int hseq = an->hseq - 1;
	    do {
		if (hseq < 0) {
		    an = pan;
		    goto _end;
		}
		po = hl->marks + hseq;
		an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
		if (an == NULL)
		    an = retrieveAnchor(Currentbuf->formitem, po->line,
					po->pos);
		hseq--;
	    } while (an == NULL || an == pan);
	}
	else {
	    an = closest_prev_anchor(Currentbuf->href, NULL, x, y);
	    an = closest_prev_anchor(Currentbuf->formitem, an, x, y);
	    if (an == NULL) {
		an = pan;
		break;
	    }
	    x = an->start.pos;
	    y = an->start.line;
	}
    }

  _end:
    if (an == NULL || an->hseq < 0)
	return;
    po = hl->marks + an->hseq;
    gotoLine(Currentbuf, po->line);
    Currentbuf->pos = po->pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next left/right anchor */
static void
nextX(int d, int dy)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    Anchor *an, *pan;
    Line *l;
    int i, x, y, n = searchKeyNum();

    if (Currentbuf->firstLine == NULL)
	return;
    if (!hl || hl->nmark == 0)
	return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (an == NULL)
	an = retrieveCurrentForm(Currentbuf);

    l = Currentbuf->currentLine;
    x = Currentbuf->pos;
    y = l->linenumber;
    pan = NULL;
    for (i = 0; i < n; i++) {
	if (an)
	    x = (d > 0) ? an->end.pos : an->start.pos - 1;
	an = NULL;
	while (1) {
	    for (; x >= 0 && x < l->len; x += d) {
		an = retrieveAnchor(Currentbuf->href, y, x);
		if (!an)
		    an = retrieveAnchor(Currentbuf->formitem, y, x);
		if (an) {
		    pan = an;
		    break;
		}
	    }
	    if (!dy || an)
		break;
	    l = (dy > 0) ? l->next : l->prev;
	    if (!l)
		break;
	    x = (d > 0) ? 0 : l->len - 1;
	    y = l->linenumber;
	}
	if (!an)
	    break;
    }

    if (pan == NULL)
	return;
    gotoLine(Currentbuf, y);
    Currentbuf->pos = pan->start.pos;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next downward/upward anchor */
static void
nextY(int d)
{
    HmarkerList *hl = Currentbuf->hmarklist;
    Anchor *an, *pan;
    int i, x, y, n = searchKeyNum();
    int hseq;

    if (Currentbuf->firstLine == NULL)
	return;
    if (!hl || hl->nmark == 0)
	return;

    an = retrieveCurrentAnchor(Currentbuf);
    if (an == NULL)
	an = retrieveCurrentForm(Currentbuf);

    x = Currentbuf->pos;
    y = Currentbuf->currentLine->linenumber + d;
    pan = NULL;
    hseq = -1;
    for (i = 0; i < n; i++) {
	if (an)
	    hseq = abs(an->hseq);
	an = NULL;
	for (; y >= 0 && y <= Currentbuf->lastLine->linenumber; y += d) {
	    an = retrieveAnchor(Currentbuf->href, y, x);
	    if (!an)
		an = retrieveAnchor(Currentbuf->formitem, y, x);
	    if (an && hseq != abs(an->hseq)) {
		pan = an;
		break;
	    }
	}
	if (!an)
	    break;
    }

    if (pan == NULL)
	return;
    gotoLine(Currentbuf, pan->start.line);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next left anchor */
void
nextL(void)
{
    nextX(-1, 0);
}

/* go to the next left-up anchor */
void
nextLU(void)
{
    nextX(-1, -1);
}

/* go to the next right anchor */
void
nextR(void)
{
    nextX(1, 0);
}

/* go to the next right-down anchor */
void
nextRD(void)
{
    nextX(1, 1);
}

/* go to the next downward anchor */
void
nextD(void)
{
    nextY(1);
}

/* go to the next upward anchor */
void
nextU(void)
{
    nextY(-1);
}

static int
checkBackBuffer(Buffer *buf)
{
    Buffer *fbuf = buf->linkBuffer[LB_N_FRAME];

    if (fbuf) {
	if (fbuf->frameQ)
	    return TRUE;	/* Currentbuf has stacked frames */
	/* when no frames stacked and next is frame source, try next's
	 * nextBuffer */
	if (RenderFrame && fbuf == buf->nextBuffer) {
	    if (fbuf->nextBuffer != NULL)
		return TRUE;
	    else
		return FALSE;
	}
    }

    if (buf->nextBuffer)
	return TRUE;

    return FALSE;
}

/* delete current buffer and back to the previous buffer */
void
backBf(void)
{
    Buffer *buf = Currentbuf->linkBuffer[LB_N_FRAME];

    if (!checkBackBuffer(Currentbuf)) {
	disp_message("Can't back...", TRUE);
	return;
    }

    delBuffer(Currentbuf);

    if (buf) {
	if (buf->frameQ) {
	    struct frameset *fs;
	    long linenumber = buf->frameQ->linenumber;
	    long top = buf->frameQ->top_linenumber;
	    short pos = buf->frameQ->pos;
	    short currentColumn = buf->frameQ->currentColumn;
	    AnchorList *formitem = buf->frameQ->formitem;

	    fs = popFrameTree(&(buf->frameQ));
	    deleteFrameSet(buf->frameset);
	    buf->frameset = fs;

	    if (buf == Currentbuf) {
		rFrame();
		Currentbuf->topLine = lineSkip(Currentbuf,
					       Currentbuf->firstLine, top - 1,
					       FALSE);
		gotoLine(Currentbuf, linenumber);
		Currentbuf->pos = pos;
		Currentbuf->currentColumn = currentColumn;
		arrangeCursor(Currentbuf);
		formResetBuffer(Currentbuf, formitem);
	    }
	}
	else if (RenderFrame && buf == Currentbuf) {
	    delBuffer(Currentbuf);
	}
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void
deletePrevBuf()
{
    Buffer *buf = Currentbuf->nextBuffer;
    if (buf)
	delBuffer(buf);
}

static void
cmd_loadURL(char *url, ParsedURL *current)
{
    Buffer *buf;

    if (!strncasecmp(url, "mailto:", 7)) {
	/* invoke external mailer */
	Str tmp;
	char *to = shell_quote(url + 7);
	if (strcasestr(Mailer, "%s"))
	    tmp = Sprintf(Mailer, to);
	else
	    tmp = Strnew_m_charp(Mailer, " ", to, NULL);
	fmTerm();
	system(tmp->ptr);
	fmInit();
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
#ifdef USE_NNTP
    else if (!strncasecmp(url, "news:", 5) && strchr(url, '@') == NULL) {
	/* news:newsgroup is not supported */
	disp_err_message("news:newsgroup_name is not supported", TRUE);
	return;
    }
#endif				/* USE_NNTP */

    refresh();
    buf = loadGeneralFile(url, current, NO_REFERER, 0, NULL);
    if (buf == NULL) {
	char *emsg = Sprintf("Can't load %s", conv_from_system(url))->ptr;
	disp_err_message(emsg, FALSE);
    }
    else if (buf != NO_BUFFER) {
	pushBuffer(buf);
	if (RenderFrame && Currentbuf->frameset != NULL)
	    rFrame();
    }
    displayBuffer(Currentbuf, B_NORMAL);
}


/* go to specified URL */
void
goURL(void)
{
    char *url;
    ParsedURL p_url;

    url = searchKeyData();
    if (url == NULL) {
	if (!(Currentbuf->bufferprop & BP_INTERNAL))
	    pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
	url = inputLineHist("Goto URL: ", NULL, IN_URL, URLHist);
	if (url != NULL)
	    SKIP_BLANKS(url);
    }
#ifdef JP_CHARSET
    if (url != NULL) {
	if (Currentbuf->document_code)
	    url = conv(url, InnerCode, Currentbuf->document_code)->ptr;
	else
	    url = conv_to_system(url);
    }
#endif
    if (url == NULL || *url == '\0') {
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
    if (*url == '#') {
	gotoLabel(url + 1);
	return;
    }
    parseURL2(url, &p_url, baseURL(Currentbuf));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, baseURL(Currentbuf));
}

static void
cmd_loadBuffer(Buffer *buf, int prop, int link)
{
    if (buf == NULL) {
	disp_err_message("Can't load string", FALSE);
    }
    else if (buf != NO_BUFFER) {
	buf->bufferprop |= (BP_INTERNAL | prop);
	if (!(buf->bufferprop & BP_NO_URL))
	    copyParsedURL(&buf->currentURL, &Currentbuf->currentURL);
	if (link != LB_NOLINK) {
	    buf->linkBuffer[REV_LB[link]] = Currentbuf;
	    Currentbuf->linkBuffer[link] = buf;
	}
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* load bookmark */
void
ldBmark(void)
{
    cmd_loadURL(BookmarkFile, NULL);
}


/* Add current to bookmark */
void
adBmark(void)
{
    Str tmp;

    tmp = Sprintf("file://%s/" W3MBOOKMARK_CMDNAME
		  "?mode=panel&bmark=%s&url=%s&title=%s",
		  w3m_lib_dir(),
		  (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
		  (Str_form_quote(parsedURL2Str(&Currentbuf->currentURL)))->
		  ptr,
		  (Str_form_quote(Strnew_charp(Currentbuf->buffername)))->ptr);
    cmd_loadURL(tmp->ptr, NULL);
}

/* option setting */
void
ldOpt(void)
{
    cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}

/* set an option */
void
setOpt(void)
{
    char *opt;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    opt = searchKeyData();
    if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
	if (opt != NULL && *opt != '\0') {
	    char *v = get_param_option(opt);
	    opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
	}
	opt = inputStrHist("Set option: ", opt, TextHist);
	if (opt == NULL || *opt == '\0') {
	    displayBuffer(Currentbuf, B_NORMAL);
	    return;
	}
    }
    if (set_param_option(opt))
	sync_with_option();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* error message list */
void
msgs(void)
{
    cmd_loadBuffer(message_list_panel(), BP_NO_URL, LB_NOLINK);
}

/* page info */
void
pginfo(void)
{
    Buffer *buf;

    if ((buf = Currentbuf->linkBuffer[LB_N_INFO]) != NULL) {
	Currentbuf = buf;
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    if ((buf = Currentbuf->linkBuffer[LB_INFO]) != NULL)
	delBuffer(buf);
    buf = page_info_panel(Currentbuf);
#ifdef JP_CHARSET
    if (buf != NULL)
	buf->document_code = Currentbuf->document_code;
#endif				/* JP_CHARSET */
    cmd_loadBuffer(buf, BP_NORMAL, LB_INFO);
}

void
follow_map(struct parsed_tagarg *arg)
{
#ifdef MENU_MAP
    Anchor *a;
    char *url;
    int x;
    ParsedURL p_url;

    a = retrieveCurrentImg(Currentbuf);
    if (a != NULL)
	x = Currentbuf->cursorX - Currentbuf->pos + a->start.pos +
	    Currentbuf->rootX;
    else
	x = Currentbuf->cursorX + Currentbuf->rootX;
    url = follow_map_menu(Currentbuf, arg, x, Currentbuf->cursorY + 2);
    if (url == NULL || *url == '\0')
	return;
    if (*url == '#') {
	gotoLabel(url + 1);
	return;
    }
    parseURL2(url, &p_url, baseURL(Currentbuf));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, baseURL(Currentbuf));
#else
    Buffer *buf;

    buf = follow_map_panel(Currentbuf, arg);
    if (buf != NULL)
	cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
#endif
}

#ifdef USE_COOKIE
/* cookie list */
void
cooLst(void)
{
    Buffer *buf;

    buf = cookie_list_panel();
    if (buf != NULL)
	cmd_loadBuffer(buf, BP_NO_URL, LB_NOLINK);
}
#endif				/* USE_COOKIE */

#ifdef USE_HISTORY
/* History page */
void
ldHist(void)
{
    cmd_loadBuffer(historyBuffer(URLHist), BP_NO_URL, LB_NOLINK);
}
#endif				/* USE_HISTORY */

/* download HREF link */
void
svA(void)
{
    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    do_download = TRUE;
    followA();
    do_download = FALSE;
}

/* download IMG link */
void
svI(void)
{
    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    do_download = TRUE;
    followI();
    do_download = FALSE;
}

/* save buffer */
void
svBuf(void)
{
    char *file;
    FILE *f;
    int is_pipe;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    file = searchKeyData();
    if (file == NULL || *file == '\0') {
	file = inputLineHist("Save buffer to: ", NULL, IN_COMMAND, SaveHist);
    }
    if (file != NULL)
	file = conv_to_system(file);
    if (file == NULL || *file == '\0') {
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
    if (*file == '|') {
	is_pipe = TRUE;
	f = popen(file + 1, "w");
    }
    else {
	file = expandName(file);
	if (checkOverWrite(file) < 0)
	    return;
	f = fopen(file, "w");
	is_pipe = FALSE;
    }
    if (f == NULL) {
	char *emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
	disp_err_message(emsg, TRUE);
	return;
    }
    saveBuffer(Currentbuf, f);
    if (is_pipe)
	pclose(f);
    else
	fclose(f);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* save source */
void
svSrc(void)
{
    char *file;

    if (Currentbuf->sourcefile == NULL)
	return;
    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    PermitSaveToPipe = TRUE;
    if (Currentbuf->real_scheme == SCM_LOCAL)
	file = conv_from_system(guess_save_name(NULL,
						Currentbuf->currentURL.
						real_file));
    else
	file = guess_save_name(Currentbuf, Currentbuf->currentURL.file);
    doFileCopy(Currentbuf->sourcefile, file);
    PermitSaveToPipe = FALSE;
    displayBuffer(Currentbuf, B_NORMAL);
}

static void
_peekURL(int only_img)
{

    Anchor *a;
    ParsedURL pu;
    static Str s = NULL;
    static int offset = 0, n;

    if (Currentbuf->firstLine == NULL)
	return;
    if (CurrentKey == prev_key && s != NULL) {
	if (s->length - offset >= COLS)
	    offset++;
	else if (s->length <= offset)	/* bug ? */
	    offset = 0;
	goto disp;
    }
    else {
	offset = 0;
    }
    a = (only_img ? NULL : retrieveCurrentAnchor(Currentbuf));
    if (a == NULL) {
	a = retrieveCurrentImg(Currentbuf);
	if (a == NULL) {
	    a = retrieveCurrentForm(Currentbuf);
	    if (a == NULL) {
		s = NULL;
		return;
	    }
	    s = Strnew_charp(form2str((FormItemList *)a->url));
	    goto disp;
	}
    }
    parseURL2(a->url, &pu, baseURL(Currentbuf));
    s = parsedURL2Str(&pu);
  disp:
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (COLS - 1))
	disp_message_nomouse(&s->ptr[(n - 1) * (COLS - 1)], TRUE);
    else
	disp_message_nomouse(&s->ptr[offset], TRUE);
}

/* peek URL */
void
peekURL(void)
{
    _peekURL(0);
}

/* peek URL of image */
void
peekIMG(void)
{
    _peekURL(1);
}

/* show current URL */
static Str
currentURL(void)
{
    if (Currentbuf->bufferprop & BP_INTERNAL)
	return Strnew_size(0);
    return parsedURL2Str(&Currentbuf->currentURL);
}

void
curURL(void)
{
    static Str s = NULL;
    static int offset = 0, n;

    if (Currentbuf->bufferprop & BP_INTERNAL)
	return;
    if (CurrentKey == prev_key && s != NULL) {
	if (s->length - offset >= COLS)
	    offset++;
	else if (s->length <= offset)	/* bug ? */
	    offset = 0;
    }
    else {
	offset = 0;
	s = currentURL();
    }
    n = searchKeyNum();
    if (n > 1 && s->length > (n - 1) * (COLS - 1))
	disp_message_nomouse(&s->ptr[(n - 1) * (COLS - 1)], TRUE);
    else
	disp_message_nomouse(&s->ptr[offset], TRUE);
}

/* view HTML source */
void
vwSrc(void)
{
    char *fn;
    Buffer *buf;

    if (Currentbuf->type == NULL || Currentbuf->bufferprop & BP_FRAME)
	return;
    if ((buf = Currentbuf->linkBuffer[LB_SOURCE]) != NULL ||
	(buf = Currentbuf->linkBuffer[LB_N_SOURCE]) != NULL) {
	Currentbuf = buf;
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    if (Currentbuf->sourcefile == NULL) {
	if (Currentbuf->pagerSource &&
	    !strcasecmp(Currentbuf->type, "text/plain")) {
	    FILE *f;
	    Str tmpf = tmpfname(TMPF_SRC, NULL);
	    f = fopen(tmpf->ptr, "w");
	    if (f == NULL)
		return;
	    saveBufferDelNum(Currentbuf, f, showLineNum);
	    fclose(f);
	    fn = tmpf->ptr;
	}
	else {
	    return;
	}
    }
    else if (Currentbuf->real_scheme == SCM_LOCAL) {
	fn = Currentbuf->filename;
    }
    else {
	fn = Currentbuf->sourcefile;
    }
    if (!strcasecmp(Currentbuf->type, "text/html")) {
#ifdef JP_CHARSET
	char old_code = DocumentCode;
	DocumentCode = Currentbuf->document_code;
#endif
	buf = loadFile(fn);
#ifdef JP_CHARSET
	DocumentCode = old_code;
#endif
	if (buf == NULL)
	    return;
	buf->type = "text/plain";
	if (Currentbuf->real_type &&
	    !strcasecmp(Currentbuf->real_type, "text/html"))
	    buf->real_type = "text/plain";
	else
	    buf->real_type = Currentbuf->real_type;
	buf->bufferprop |= BP_SOURCE;
	buf->buffername = Sprintf("source of %s", Currentbuf->buffername)->ptr;
	buf->linkBuffer[LB_N_SOURCE] = Currentbuf;
	Currentbuf->linkBuffer[LB_SOURCE] = buf;
    }
    else if (!strcasecmp(Currentbuf->type, "text/plain")) {
	DefaultType = "text/html";
	buf = loadGeneralFile(file_to_url(fn), NULL, NO_REFERER, 0, NULL);
	DefaultType = NULL;
	if (buf == NULL || buf == NO_BUFFER)
	    return;
	if (Currentbuf->real_type &&
	    !strcasecmp(Currentbuf->real_type, "text/plain"))
	    buf->real_type = "text/html";
	else
	    buf->real_type = Currentbuf->real_type;
	if (!strcmp(buf->buffername, conv_from_system(lastFileName(fn))))
	    buf->buffername =
		Sprintf("HTML view of %s", Currentbuf->buffername)->ptr;
	buf->linkBuffer[LB_SOURCE] = Currentbuf;
	Currentbuf->linkBuffer[LB_N_SOURCE] = buf;
    }
    else {
	return;
    }
    buf->currentURL = Currentbuf->currentURL;
    buf->real_scheme = Currentbuf->real_scheme;
    buf->sourcefile = Currentbuf->sourcefile;
    buf->clone = Currentbuf->clone;
    (*buf->clone)++;
    pushBuffer(buf);
    displayBuffer(Currentbuf, B_NORMAL);
}

/* reload */
void
reload(void)
{
    Buffer *buf, *fbuf = NULL;
    char *type = Currentbuf->type;
    Str url;
    int top, linenum, cursorY, pos, currentColumn;
    FormList *request;
    int multipart;

    if (Currentbuf->bufferprop & BP_INTERNAL) {
	disp_err_message("Can't reload...", FALSE);
	return;
    }
    if (Currentbuf->currentURL.scheme == SCM_LOCAL &&
	!strcmp(Currentbuf->currentURL.file, "-")) {
	/* file is std input */
	disp_err_message("Can't reload stdin", TRUE);
	return;
    }
    if (Currentbuf->firstLine == NULL) {
	top = 1;
	linenum = 1;
    }
    else {
	top = Currentbuf->topLine->linenumber;
	linenum = Currentbuf->currentLine->linenumber;
    }
    cursorY = Currentbuf->cursorY;
    pos = Currentbuf->pos;
    currentColumn = Currentbuf->currentColumn;
    if (Currentbuf->bufferprop & BP_FRAME &&
	(fbuf = Currentbuf->linkBuffer[LB_N_FRAME])) {
	if (fmInitialized) {
	    message("Rendering frame", 0, 0);
	    refresh();
	}
	if (!(buf = renderFrame(fbuf, 1)))
	    return;
	if (fbuf->linkBuffer[LB_FRAME]) {
	    if (buf->sourcefile &&
		fbuf->linkBuffer[LB_FRAME]->sourcefile &&
		!strcmp(buf->sourcefile,
			fbuf->linkBuffer[LB_FRAME]->sourcefile))
		fbuf->linkBuffer[LB_FRAME]->sourcefile = NULL;
	    delBuffer(fbuf->linkBuffer[LB_FRAME]);
	}
	fbuf->linkBuffer[LB_FRAME] = buf;
	buf->linkBuffer[LB_N_FRAME] = fbuf;
	pushBuffer(buf);
	Currentbuf = buf;
	if (Currentbuf->firstLine == NULL) {
	    displayBuffer(Currentbuf, B_FORCE_REDRAW);
	    return;
	}
	gotoLine(Currentbuf, linenum);
	Currentbuf->pos = pos;
	Currentbuf->currentColumn = currentColumn;
	arrangeCursor(Currentbuf);
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
    else if (Currentbuf->frameset != NULL)
	fbuf = Currentbuf->linkBuffer[LB_FRAME];
    multipart = 0;
    if (Currentbuf->form_submit) {
	request = Currentbuf->form_submit->parent;
	if (request->method == FORM_METHOD_POST
	    && request->enctype == FORM_ENCTYPE_MULTIPART) {
	    Str query;
	    struct stat st;
	    multipart = 1;
	    query_from_followform(&query, Currentbuf->form_submit, multipart);
	    stat(request->body, &st);
	    request->length = st.st_size;
	}
    }
    else {
	request = NULL;
    }
    url = parsedURL2Str(&Currentbuf->currentURL);
    message("Reloading...", 0, 0);
    refresh();
    SearchHeader = Currentbuf->search_header;
    DefaultType = Currentbuf->real_type;
    buf = loadGeneralFile(url->ptr, NULL, NO_REFERER, RG_NOCACHE, request);
    SearchHeader = FALSE;
    DefaultType = NULL;
    if (multipart)
	unlink(request->body);
    if (buf == NULL) {
	disp_err_message("Can't reload...", FALSE);
	return;
    }
    else if (buf == NO_BUFFER) {
	return;
    }
    buf->search_header = Currentbuf->search_header;
    buf->form_submit = Currentbuf->form_submit;
    if (fbuf != NULL)
	Firstbuf = deleteBuffer(Firstbuf, fbuf);
    repBuffer(Currentbuf, buf);
    if ((type != NULL) && (buf->type != NULL) &&
	((!strcasecmp(buf->type, "text/plain") &&
	  !strcasecmp(type, "text/html")) ||
	 (!strcasecmp(buf->type, "text/html") &&
	  !strcasecmp(type, "text/plain")))) {
	vwSrc();
	if (Currentbuf != buf)
	    Firstbuf = deleteBuffer(Firstbuf, buf);
    }
    if (Currentbuf->firstLine == NULL) {
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
    Currentbuf->topLine =
	lineSkip(Currentbuf, Currentbuf->firstLine, top - 1, FALSE);
    gotoLine(Currentbuf, linenum);
    Currentbuf->pos = pos;
    Currentbuf->currentColumn = currentColumn;
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* mark URL-like patterns as anchors */
void
chkURL(void)
{
    static char *url_like_pat[] = {
	"http://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$]*[a-zA-Z0-9_/]",
#ifdef USE_SSL
	"https://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$]*[a-zA-Z0-9_/]",
#endif				/* USE_SSL */
#ifdef USE_GOPHER
	"gopher://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./_]*",
#endif				/* USE_GOPHER */
	"ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifdef USE_NNTP
	"news:[^<> 	][^<> 	]*",
	"nntp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./_]*",
#endif				/* USE_NNTP */
	"mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#ifdef INET6
	"http://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$]*",
#ifdef USE_SSL
	"https://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$]*",
#endif				/* USE_SSL */
	"ftp://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
#endif				/* INET6 */
	NULL
    };
    int i;
    for (i = 0; url_like_pat[i]; i++) {
	reAnchor(Currentbuf, url_like_pat[i]);
    }
    Currentbuf->check_url |= CHK_URL;

    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

#ifdef USE_NNTP
/* mark Message-ID-like patterns as NEWS anchors */
void
chkNMID(void)
{
    static char *url_like_pat[] = {
	"<[^<> 	][^<> 	]*@[A-z0-9\\.\\-_][A-z0-9\\.\\-_]*>",
	NULL,
    };
    int i;
    for (i = 0; url_like_pat[i]; i++) {
	reAnchorNews(Currentbuf, url_like_pat[i]);
    }
    Currentbuf->check_url |= CHK_NMID;

    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}
#endif				/* USE_NNTP */

/* render frame */
void
rFrame(void)
{
    Buffer *buf;

    if ((buf = Currentbuf->linkBuffer[LB_FRAME]) != NULL) {
	Currentbuf = buf;
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    if (Currentbuf->frameset == NULL) {
	if ((buf = Currentbuf->linkBuffer[LB_N_FRAME]) != NULL) {
	    Currentbuf = buf;
	    displayBuffer(Currentbuf, B_NORMAL);
	}
	return;
    }
    if (fmInitialized) {
	message("Rendering frame", 0, 0);
	refresh();
    }
    buf = renderFrame(Currentbuf, 0);
    if (buf == NULL)
	return;
    buf->linkBuffer[LB_N_FRAME] = Currentbuf;
    Currentbuf->linkBuffer[LB_FRAME] = buf;
    pushBuffer(buf);
    if (fmInitialized && display_ok)
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* spawn external browser */
static void
invoke_browser(char *url)
{
    Str tmp;
    char *browser = NULL;
    int bg = 0;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    browser = searchKeyData();
    if (browser == NULL || *browser == '\0') {
	switch (prec_num) {
	case 0:
	case 1:
	    browser = ExtBrowser;
	    break;
	case 2:
	    browser = ExtBrowser2;
	    break;
	case 3:
	    browser = ExtBrowser3;
	    break;
	}
	if (browser == NULL || *browser == '\0') {
	    browser = inputStr("Browse command: ", NULL);
	    if (browser != NULL)
		browser = conv_to_system(browser);
	}
    }
    else {
	browser = conv_to_system(browser);
    }
    if (browser == NULL || *browser == '\0')
	return;
    url = shell_quote(url);
    if (strcasestr(browser, "%s")) {
	tmp = Sprintf(browser, url);
	Strremovetrailingspaces(tmp);
	if (Strlastchar(tmp) == '&') {
	    Strshrink(tmp, 1);
	    bg = 1;
	}
    }
    else {
	tmp = Strnew_charp(browser);
	Strcat_char(tmp, ' ');
	Strcat_charp(tmp, url);
    }
    fmTerm();
    mySystem(tmp->ptr, bg);
    fmInit();
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void
extbrz()
{
    if (Currentbuf->bufferprop & BP_INTERNAL) {
	disp_err_message("Can't browse...", FALSE);
	return;
    }
    if (Currentbuf->currentURL.scheme == SCM_LOCAL &&
	!strcmp(Currentbuf->currentURL.file, "-")) {
	/* file is std input */
	disp_err_message("Can't browse stdin", TRUE);
	return;
    }
    invoke_browser(parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

void
linkbrz()
{
    Anchor *a;
    ParsedURL pu;

    if (Currentbuf->firstLine == NULL)
	return;
    a = retrieveCurrentAnchor(Currentbuf);
    if (a == NULL)
	return;
    parseURL2(a->url, &pu, baseURL(Currentbuf));
    invoke_browser(parsedURL2Str(&pu)->ptr);
}

/* show current line number and number of lines in the entire document */
void
curlno()
{
    Str tmp;
    int cur = 0, all, col, len = 0;

    if (Currentbuf->currentLine != NULL) {
	Line *l = Currentbuf->currentLine;
	cur = l->real_linenumber;
	if (l->width < 0)
	    l->width = COLPOS(l, l->len);
	len = l->width;
    }
    col = Currentbuf->currentColumn + Currentbuf->cursorX + 1;
    all =
	(Currentbuf->lastLine ? Currentbuf->lastLine->
	 real_linenumber : Currentbuf->allLine);
    if (all == 0 && Currentbuf->lastLine != NULL)
	all = Currentbuf->currentLine->real_linenumber;
    if (all == 0)
	all = 1;
    if (Currentbuf->pagerSource && !(Currentbuf->bufferprop & BP_CLOSE))
	tmp = Sprintf("line %d col %d/%d", cur, col, len);
    else
	tmp = Sprintf("line %d/%d (%d%%) col %d/%d",
		      cur, all, cur * 100 / all, col, len);
#ifdef JP_CHARSET
    Strcat_charp(tmp, "  ");
    Strcat_charp(tmp, code_to_str(Currentbuf->document_code));
#endif				/* not JP_CHARSET */

    disp_message(tmp->ptr, FALSE);
}

#ifdef USE_MOUSE

static void
process_mouse(int btn, int x, int y)
{
    int delta_x, delta_y, i;
    static int press_btn = MOUSE_BTN_RESET, press_x, press_y;

    if (btn == MOUSE_BTN_UP) {
	switch (press_btn) {
	case MOUSE_BTN1_DOWN:
	    if (press_x != x || press_y != y) {
		delta_x = x - press_x;
		delta_y = y - press_y;

		if (abs(delta_x) < abs(delta_y) / 3)
		    delta_x = 0;
		if (abs(delta_y) < abs(delta_x) / 3)
		    delta_y = 0;
		if (reverse_mouse) {
		    delta_y = -delta_y;
		    delta_x = -delta_x;
		}
		if (delta_y > 0) {
		    prec_num = delta_y;
		    ldown1();
		}
		else if (delta_y < 0) {
		    prec_num = -delta_y;
		    lup1();
		}
		if (delta_x > 0) {
		    prec_num = delta_x;
		    col1L();
		}
		else if (delta_x < 0) {
		    prec_num = -delta_x;
		    col1R();
		}
	    }
	    else {
		if (y == LASTLINE) {
		    switch (x) {
		    case 0:
		    case 1:
			backBf();
			break;
		    case 2:
		    case 3:
			pgBack();
			break;
		    case 4:
		    case 5:
			pgFore();
			break;
		    }
		    return;
		}
		if (y == Currentbuf->cursorY &&
		    (x == Currentbuf->cursorX + Currentbuf->rootX
#ifdef JP_CHARSET
		     || (Currentbuf->currentLine != NULL &&
			 (Currentbuf->currentLine->
			  propBuf[Currentbuf->pos] & PC_KANJI1)
			 && x == Currentbuf->cursorX + Currentbuf->rootX + 1)
#endif				/* JP_CHARSET */
		    )) {
		    followA();
		    return;
		}
		if (x >= Currentbuf->rootX)
		    cursorXY(Currentbuf, x - Currentbuf->rootX, y);
		displayBuffer(Currentbuf, B_NORMAL);

	    }
	    break;
	case MOUSE_BTN2_DOWN:
	    backBf();
	    break;
	case MOUSE_BTN3_DOWN:
#ifdef USE_MENU
	    if (x >= Currentbuf->rootX)
		cursorXY(Currentbuf, x - Currentbuf->rootX, y);
	    onA();
	    mainMenu(x, y);
#endif				/* USE_MENU */
	    break;
	case MOUSE_BTN4_DOWN_RXVT:
	    for (i = 0; i < MOUSE_SCROLL_LINE; i++)
		ldown1();
	    break;
	case MOUSE_BTN5_DOWN_RXVT:
	    for (i = 0; i < MOUSE_SCROLL_LINE; i++)
		lup1();
	    break;
	}
    }
    else if (btn == MOUSE_BTN4_DOWN_XTERM) {
	for (i = 0; i < MOUSE_SCROLL_LINE; i++)
	    ldown1();
    }
    else if (btn == MOUSE_BTN5_DOWN_XTERM) {
	for (i = 0; i < MOUSE_SCROLL_LINE; i++)
	    lup1();
    }

    if (btn != MOUSE_BTN4_DOWN_RXVT || press_btn == MOUSE_BTN_RESET) {
	press_btn = btn;
	press_x = x;
	press_y = y;
    }
    else {
	press_btn = MOUSE_BTN_RESET;
    }
}

void
msToggle(void)
{
    if (use_mouse) {
	use_mouse = FALSE;
    }
    else {
	use_mouse = TRUE;
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void
mouse()
{
    int btn, x, y;

    btn = (unsigned char)getch() - 32;
    x = (unsigned char)getch() - 33;
    if (x < 0)
	x += 0x100;
    y = (unsigned char)getch() - 33;
    if (y < 0)
	y += 0x100;

    if (x < 0 || x >= COLS || y < 0 || y > LASTLINE)
	return;
    process_mouse(btn, x, y);
}

#ifdef USE_GPM
int
gpm_process_mouse(Gpm_Event * event, void *data)
{
    int btn, x, y;
    if (event->type & GPM_UP)
	btn = MOUSE_BTN_UP;
    else if (event->type & GPM_DOWN) {
	switch (event->buttons) {
	case GPM_B_LEFT:
	    btn = MOUSE_BTN1_DOWN;
	    break;
	case GPM_B_MIDDLE:
	    btn = MOUSE_BTN2_DOWN;
	    break;
	case GPM_B_RIGHT:
	    btn = MOUSE_BTN3_DOWN;
	    break;
	}
    }
    else {
	GPM_DRAWPOINTER(event);
	return 0;
    }
    x = event->x;
    y = event->y;
    process_mouse(btn, x - 1, y - 1);
    return 0;
}
#endif				/* USE_GPM */

#ifdef USE_SYSMOUSE
int
sysm_process_mouse(int x, int y, int nbs, int obs)
{
    int btn;
    int bits;

    if (obs & ~nbs)
	btn = MOUSE_BTN_UP;
    else if (nbs & ~obs) {
	bits = nbs & ~obs;
	btn = bits & 0x1 ? MOUSE_BTN1_DOWN :
	    (bits & 0x2 ? MOUSE_BTN2_DOWN :
	     (bits & 0x4 ? MOUSE_BTN3_DOWN : 0));
    }
    else			/* nbs == obs */
	return 0;
    process_mouse(btn, x, y);
    return 0;
}
#endif				/* USE_SYSMOUSE */
#endif				/* USE_MOUSE */

void
dispVer()
{
    disp_message(Sprintf("w3m version %s", version)->ptr, FALSE);
}

void
wrapToggle(void)
{
    if (WrapSearch) {
	WrapSearch = FALSE;
	disp_message("Wrap search off", FALSE);
    }
    else {
	WrapSearch = TRUE;
	disp_message("Wrap search on", FALSE);
    }
}

static char *
GetWord(Buffer *buf)
{
    Line *l = buf->currentLine;
    char *lb;
    int b, e;

    if (l == NULL)
	return NULL;
    lb = l->lineBuf;

    e = buf->pos;
    while (e > 0 && !IS_ALPHA(lb[e]))
	e--;
    if (!IS_ALPHA(lb[e]))
	return NULL;
    b = e;
    while (b > 0 && IS_ALPHA(lb[b - 1]))
	b--;
    while (e < l->len && IS_ALPHA(lb[e]))
	e++;
    return Strnew_charp_n(&lb[b], e - b)->ptr;
}

#ifdef USE_DICT
static void
execdict(char *word)
{
    Buffer *buf;
    Str cmd;
    MySignalHandler(*prevtrap) ();

    if (word == NULL || *word == '\0') {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    cmd = Strnew_charp(DICTCMD);
    Strcat_char(cmd, ' ');
    Strcat_charp(cmd, word);
    prevtrap = signal(SIGINT, intTrap);
    crmode();
    buf = getshell(cmd->ptr);
    buf->filename = word;
    word = conv_from_system(word);
    buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
    signal(SIGINT, prevtrap);
    term_raw();
    if (buf == NULL) {
	disp_message("Execution failed", FALSE);
    }
    else if (buf->firstLine == NULL) {
	/* if the dictionary doesn't describe the word. */
	disp_message(Sprintf("Word \"%s\" Not Found", word)->ptr, FALSE);
    }
    else {
	buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
	pushBuffer(buf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

void
dictword(void)
{
    char *word = inputStr("(dictionary)!", "");

    if (word != NULL)
	word = conv_to_system(word);
    if (word == NULL || *word == '\0')
	return;
    execdict(word);
}

void
dictwordat(void)
{
    execdict(GetWord(Currentbuf));
}
#endif				/* USE_DICT */

void
set_buffer_environ(Buffer *buf)
{
    Anchor *a;
    Str s;
    ParsedURL pu;

    if (buf == NULL)
	return;
    set_environ("W3M_SOURCEFILE", buf->sourcefile);
    set_environ("W3M_FILENAME", buf->filename);
    set_environ("W3M_CURRENT_WORD", GetWord(buf));
    set_environ("W3M_TITLE", buf->buffername);
    set_environ("W3M_URL", parsedURL2Str(&buf->currentURL)->ptr);
    if (buf->real_type)
	set_environ("W3M_TYPE", buf->real_type);
    else
	set_environ("W3M_TYPE", "unknown");
#ifdef JP_CHARSET
    set_environ("W3M_CHARSET", code_to_str(buf->document_code));
#endif				/* JP_CHARSET */
    a = retrieveCurrentAnchor(buf);
    if (a == NULL) {
	set_environ("W3M_CURRENT_LINK", "");
    }
    else {
	parseURL2(a->url, &pu, baseURL(buf));
	s = parsedURL2Str(&pu);
	set_environ("W3M_CURRENT_LINK", s->ptr);
    }
    a = retrieveCurrentImg(buf);
    if (a == NULL) {
	set_environ("W3M_CURRENT_IMG", "");
    }
    else {
	parseURL2(a->url, &pu, baseURL(buf));
	s = parsedURL2Str(&pu);
	set_environ("W3M_CURRENT_IMG", s->ptr);
    }
    a = retrieveCurrentForm(buf);
    if (a == NULL) {
	set_environ("W3M_CURRENT_FORM", "");
    }
    else {
	s = Strnew_charp(form2str((FormItemList *)a->url));
	set_environ("W3M_CURRENT_FORM", s->ptr);
    }
}

char *
searchKeyData(void)
{
    KeyListItem *item;

    if (CurrentKeyData != NULL && *CurrentKeyData != '\0')
	return allocStr(CurrentKeyData, 0);
#ifdef USE_MENU
    if (CurrentMenuData != NULL && *CurrentMenuData != '\0')
	return allocStr(CurrentMenuData, 0);
#endif
    if (CurrentKey < 0)
	return NULL;
    item = searchKeyList(&w3mKeyList, CurrentKey);
    if (item == NULL || item->data == NULL || *item->data == '\0')
	return NULL;
    return allocStr(item->data, 0);
}

static int
searchKeyNum(void)
{
    char *d;
    int n = 1;

    d = searchKeyData();
    if (d != NULL)
	n = atoi(d);
    return n * PREC_NUM;
}

void
deleteFiles()
{
    Buffer *buf;
    char *f;
    while (Firstbuf && Firstbuf != NO_BUFFER) {
	buf = Firstbuf->nextBuffer;
	discardBuffer(Firstbuf);
	Firstbuf = buf;
    }
    while ((f = popText(fileToDelete)) != NULL)
	unlink(f);
}

void
w3m_exit(int i)
{
    deleteFiles();
#ifdef USE_SSL
    free_ssl_ctx();
#endif
    exit(i);
}

#ifdef USE_ALARM
static MySignalHandler
SigAlarm(SIGNAL_ARG)
{
    if (alarm_sec > 0) {
	CurrentKey = -1;
	CurrentKeyData = (char *)alarm_event.user_data;
#ifdef USE_MENU
	CurrentMenuData = NULL;
#endif
	w3mFuncList[alarm_event.cmd].func();
	onA();
	if (alarm_status == AL_IMPLICIT) {
	    alarm_buffer = Currentbuf;
	    alarm_status = AL_IMPLICIT_DONE;
	}
	else if (alarm_status == AL_IMPLICIT_DONE
		 && alarm_buffer != Currentbuf) {
	    alarm_sec = 0;
	    alarm_status = AL_UNSET;
	}
	if (alarm_sec > 0) {
	    signal(SIGALRM, SigAlarm);
	    alarm(alarm_sec);
	}
    }
    SIGNAL_RETURN;
}

void
setAlarm(void)
{
    char *data;
    int sec = 0, cmd = -1;
    extern int w3mNFuncList;

    CurrentKeyData = NULL;	/* not allowed in w3m-control: */
    data = searchKeyData();
    if (data == NULL || *data == '\0') {
	data = inputStrHist("(Alarm)sec command: ", "", TextHist);
	if (data == NULL) {
	    displayBuffer(Currentbuf, B_NORMAL);
	    return;
	}
    }
    if (*data != '\0') {
	sec = atoi(getWord(&data));
	if (sec > 0)
	    cmd = getFuncList(getWord(&data), w3mFuncList, w3mNFuncList);
    }
    if (cmd >= 0) {
	setAlarmEvent(sec, AL_EXPLICIT, cmd, getQWord(&data));
    }
    else {
	alarm_sec = 0;
    }
    displayBuffer(Currentbuf, B_NORMAL);
}

void
setAlarmEvent(int sec, short status, int cmd, void *data)
{
    if (status == AL_EXPLICIT
	|| (status == AL_IMPLICIT && alarm_status != AL_EXPLICIT)) {
	alarm_sec = sec;
	alarm_status = status;
	alarm_event.cmd = cmd;
	alarm_event.user_data = data;
	signal(SIGALRM, SigAlarm);
	alarm(alarm_sec);
    }
}
#endif