/*
 * Reads CSRI malloc trace files and displays the state of the arena in
 * an X window - Mark Moraes.
 */
#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xos.h>
#include <X11/cursorfont.h>
#include <X11/Xutil.h>

#define APP_NAME		"xmem"
#define APP_CLASS		"XMem"

#ifndef OUTPUT_ONLY
# define RightButton		Button3
# define MiddleButton		Button2
# define LeftButton		Button1
# define RightButtonMask	Button3Mask
# define MiddleButtonMask	Button2Mask
# define LeftButtonMask		Button1Mask
#endif	/* OUTPUT_ONLY */

/*
 * If MAXARGS isn't enough to hold all the arguments you pass any Xt
 * procedure, the program aborts
 */
#define MAXARGS 32
static int nargs;
static Arg wargs[MAXARGS];
#define startargs()		nargs = 0
#define setarg(name, value)	\
	if (nargs < MAXARGS) \
		XtSetArg(wargs[nargs], name, value), nargs++; \
	else \
		abort()

static Cursor WorkingCursor;
static Display *dpy;
static Window win;
static Widget w, toplevel;
static XtWorkProcId wpid = 0;
static GC gc;
static GC fillgc;
static GC cleargc;

static int running = 0;

/* Defaults */
static int defaultWidth = 512;
static int defaultHeight = 512;
static int defaultScale = 4;
static int defaultDelay = 0;

static int Width, Height;
static int Scale, Delay, Base = 0, MaxMem = 0;
static Pixel fg, bg;
static char *progname;

static char gray4_bits[] = {
   0x00, 0x07, 0x02};
#define gray4_height 3
#define gray4_width 3

static Pixmap graymap;

/* Application Resources - no particular widget */
static XtResource application_resources[] = {
	{"name", "Name", XtRString, sizeof(char *),
		(Cardinal)&progname, XtRString, APP_NAME},
	{"width", "Width", XtRInt, sizeof(int),
		(Cardinal)&Width, XtRInt, (caddr_t) &defaultWidth},
	{"height", "Height", XtRInt, sizeof(int),
		(Cardinal)&Height, XtRInt, (caddr_t) &defaultHeight},
	{"foreground", "Foreground", XtRPixel, sizeof(Pixel),
		(Cardinal)&fg, XtRString, (caddr_t) "Black"},
	{"background", "Background", XtRPixel, sizeof(Pixel),
		(Cardinal)&bg, XtRString, (caddr_t) "White"},
	{"scale", "Scale", XtRInt, sizeof(int),
		(Cardinal)&Scale, XtRInt, (caddr_t) &defaultScale},
	{"delay", "Delay", XtRInt, sizeof(int),
		(Cardinal)&Delay, XtRInt, (caddr_t) &defaultDelay},
};

/*
 *  Command line options table. The command line is parsed for these,
 *  and it sets/overrides the appropriate values in the resource
 *  database
 */
static XrmOptionDescRec optionDescList[] = {
{"-width",	"*width",	XrmoptionSepArg, 	(caddr_t) NULL},
{"-height",	"*height",	XrmoptionSepArg,	(caddr_t) NULL},
{"-fg",		"*foreground",	XrmoptionSepArg,	(caddr_t) NULL},
{"-bg",		"*background",	XrmoptionSepArg,	(caddr_t) NULL},
{"-scale",	"*scale",	XrmoptionSepArg,	(caddr_t) NULL},
{"-delay",	"*delay",	XrmoptionSepArg,	(caddr_t) NULL},
};


int
main(argc, argv)
int argc;
char **argv;
{
	XGCValues gcv;
#ifndef OUTPUT_ONLY
	void RepaintCanvas();
	void RecordMapStatus();
	void MouseInput();
	Boolean readline();
#else
	XEvent ev;
#endif

	/*
 	 * Create the top level Widget that represents encloses the
	 * application.
	 */
	toplevel = XtInitialize(argv[0], APP_CLASS,
		optionDescList, XtNumber(optionDescList), (Cardinal *) &argc,
		argv);

	XtGetApplicationResources(toplevel, (caddr_t) 0, application_resources,
		XtNumber(application_resources), (ArgList) NULL, (Cardinal)0);


	if (argc != 1) {
		(void) fprintf(stderr, "Usage: %s [Xt options]\n", argv[0]);
		exit(-1);
	}

	/*
	 * Create a simple Core class widget which we'll use for the actual
	 * game.  A Core class widget is basically just a window, with a
	 * simple Xt "wrapper" around it.
	 */
	startargs();
	setarg(XtNwidth, Width);
	setarg(XtNheight, Height);
	w = XtCreateManagedWidget(argv[0], widgetClass, toplevel, 
		wargs, XtNumber(wargs));

#ifndef OUTPUT_ONLY
	/*
	 * Set the procedures for various X Windows actions - exposure events
	 * which arrive when a window needs to be redrawn. The map event lets
	 * you know that the window is now on the screen so you can actually
	 * do stuff. The ButtonPress event lets you know that a mouse button
	 * was pressed.
	 */
	XtAddEventHandler(w, (EventMask) ExposureMask,
			  False, RepaintCanvas, "redraw_data");
	XtAddEventHandler(w, (EventMask) StructureNotifyMask,
			  False, RecordMapStatus, "map_data");
	/* One day, we'll use the translation manager here */
	XtAddEventHandler(w, (EventMask) ButtonPressMask,
			  False, MouseInput, "input_data");
#endif

	/*
	 * Create the windows, and set their attributes according to the Widget
	 * data.
	 */
	XtRealizeWidget(toplevel);
	
	/* We need these for the raw Xlib calls */
	win = XtWindow(w);
	dpy = XtDisplay(w);

	WorkingCursor = XCreateFontCursor(dpy, XC_top_left_arrow);
	XDefineCursor(dpy, win, WorkingCursor);

	/*
	 *  make the GC stuff here - one for copy, one for invert. Remember
	 *  to change the both appropriately
	 */
	gcv.foreground = fg;
	gcv.background = bg;
	gcv.function = GXcopy;
	gc = XCreateGC(dpy, win, GCForeground | GCBackground 
	 | GCFunction, &gcv);
	/* create a dithered background pixmap */
	graymap = XCreatePixmapFromBitmapData(dpy, win, gray4_bits,
					      gray4_height, gray4_width,
					      fg, bg, 1);
	gcv.stipple = graymap;
	gcv.fill_style = FillOpaqueStippled;
	fillgc = XCreateGC(dpy, win, GCForeground | GCBackground 
			   | GCFunction | GCFillStyle | GCStipple, &gcv);
	gcv.foreground = bg;
	cleargc = XCreateGC(dpy, win, GCForeground | GCBackground 
			    | GCFunction, &gcv);

	
	/*
	 *  Now process the events.
	 */
#ifndef OUTPUT_ONLY
	XtMainLoop();
#else
	/* Wait for first exposure event so we know window has been mapped */
	write(2, "waiting for window map\n", 23);
	do {
		XNextEvent(dpy, &ev);
	} while(XPending(dpy));
	XFillRectangle(dpy, win, fillgc, 0, 0, Width, Height);
	XSync(dpy, True);
	write(2, "ready for data\n", 15);
	gobble();
	XSync(dpy, True);
	printf("All done!\n");
	fflush(stdout);
	while(1)
		pause();
#endif
	return 0;
}



static
calcrowcol(addr, rowp, colp)
int *rowp;
int *colp;
{
	*rowp = addr / Width;
	*colp = addr % Width;
}

fillmem(lo, hi, clear, record)
{
	int plo = lo / Scale;
	int phi = hi / Scale;
	int srow, scol, erow, ecol;
	int i;
	Dimension newheight;
	GC tgc = clear ? cleargc : gc;
	static int warned;

	if (phi > MaxMem) {
		MaxMem = phi;
	}
	calcrowcol(plo, &srow, &scol);
	calcrowcol(phi, &erow, &ecol);

	if (erow >= Height) {
#if 0
		newheight = ((erow + 31) / 32) * 32;
		XtResizeWidget(w, (Dimension) Width, newheight);
		XtResizeWidget(toplevel, (Dimension) Width, newheight);
		Height = newheight;
#else
		if (warned < erow) {
			printf("need %d rows\n", erow);
			warned = erow;
		}
		erow = Height - 1;
		if (srow >= Height)
			srow = Height - 1;
#endif
	}
	if (srow == erow) {
		XDrawLine(dpy, win, tgc, scol, srow, ecol, srow);
	} else if (srow > erow) {
		fprintf(stderr, "srow %d > erow %d, aborting!\n", srow, erow);
		abort();
	} else {
		int j = scol;
		for(i = srow; i < erow; i++) {
			XDrawLine(dpy, win, tgc, j, i, Width - 1, i);
			j = 0;
		}
		XDrawLine(dpy, win, tgc, 0, i, ecol, i);
	}
}

#ifndef OUTPUT_ONLY

static int isMapped = 0;

/*ARGSUSED*/
void
RepaintCanvas(w, data, ev)
Widget w;
caddr_t data;
XEvent *ev;
{
	if (!isMapped)
		return;
	/*
	 * Redraw the array
	 */
	if (ev && ev->xexpose.count == 0) {
		XEvent event;
		/* Skip all excess redraws */
		while (XCheckTypedEvent(dpy, Expose, &event))
			;
	}

	XFillRectangle(dpy, win, fillgc, 0, 0, Width, Height);
	XSync(dpy, True);
#if 0
	/*
	 * This is a kludge to make it look better -- we really should
	 * maintain a list of segments and redraw them!
	 */
	if (MaxMem > 0) {
		(void) printf("Redisplay isn't fully functional -- old allocated segments will not be visible");
		fillmem(0, MaxMem, 1, 0);
	}
#endif
		
#ifdef WINDOWDEBUG
	(void) printf("repaint\n");
#endif
	XSync(dpy, True);
	/*
	 * only register the workproc after our first call to this
	 * routine
	 */
	if (wpid == 0)
		wpid = XtAddWorkProc(readline, (XtPointer)0);
}

/*ARGSUSED*/
void
RecordMapStatus(w, data, ev)
Widget w;
caddr_t data;
XEvent *ev;
{
	if (ev->type == MapNotify) {
#ifdef WINDOWDEBUG
		(void) printf("window mapped\n");
#endif
		isMapped = TRUE;
	} else if (ev->type = ConfigureNotify) {
		XFillRectangle(dpy, win, fillgc, 0, 0, Width, Height);
		XSync(dpy, True);
#ifdef WINDOWDEBUG
		(void) printf("window resized\n");
#endif
	}
}


/*ARGSUSED*/
void
MouseInput(w, data, ev)
Widget w;
caddr_t data;
XEvent *ev;
{
	unsigned long addr;

#ifdef WINDOWDEBUG
	(void) printf("Input to canvas - %d (0x%x)\n", ev->xany.type, ev->xany.type);
#endif
	switch (ev->xany.type) {
	case ButtonPress:
		addr = ((ev->xbutton.y - 1) * Width + ev->xbutton.x) * Scale
			+ Base;
		printf("%d %d %lu\n", ev->xbutton.x, ev->xbutton.y, addr);
		break;
	default:
		(void) printf("Got an event of type %d (%x).\n",
		       ev->xany.type, ev->xany.type);
		break;
	}
}
#endif

xgobble()
{
	char buf[128];
	int nlines = 1;
	int lo, hi;
	int ret;
	
	while(fgets(buf, sizeof buf, stdin) != NULL) {
		if (buf[0] == '\n') {
			XSync(dpy, True);
			continue;
		}
		if ((ret = sscanf(buf, " %d %d", &lo, &hi)) != 2) {
			fprintf(stderr, "yuck.  %d: %s", nlines, buf);
			fprintf(stderr, "ret = %d, lo = %d, hi = %d\n",
				ret, lo, hi);
			exit(1);
		}
		fillmem(lo, hi, 0, 1);
	}
}

Boolean
readline(ptr)
XtPointer ptr;
{
	char buf[128];
	int nlines = 1;
	int lo, hi, n;
	int ah = 0, at = 0, wordsize = 0;
	int nmsg = 0;
	
	if (fgets(buf, sizeof buf, stdin) == NULL) {
		return True;
	}
	switch (buf[0]) {
	case '+':
		if (sscanf(buf, "%*[+] %*d %d 0x%x", &n, &lo) == 2) {
			lo -= Base;
			if (wordsize) {
				lo -= ah * wordsize;
				n += (ah + at) * wordsize;
			}
			/* printf("+ %x %d\n", lo, n); */
			fillmem(lo, lo+n-1, 0, 1);
		}
		break;
	case '-':
		if (sscanf(buf, "- %d 0x%x", &n, &lo) == 2) {
			lo -= Base;
			if (wordsize) {
				lo -= ah * wordsize;
				n += (ah + at) * wordsize;
			}
			/* printf("- %x %d\n", lo, n); */
			fillmem(lo, lo+n-1, 1, 1);
		}
		break;
	case 'a':	/* allocheader HEADERWORDS alloctrailer TRAILERWORDS */
		/* this is OLD, modern malloc trace prints user bytes */
		if (sscanf(buf,"%*[a-z] %d %*[a-z] %d",&ah, &at) != 2){
			abort();
		}
		break;
	case 'h':	/* heapstart or heapend */
		if (strncmp(buf, "heapstart", 9) == 0)
			sscanf(buf, "%*[a-z] 0x%x", &Base);
		break;
	case 'w':	/* wordsize BYTES */
		/* this is OLD, modern malloc trace only uses words */
		if (sscanf(buf, "%*[a-z] %d", &wordsize) != 1)
			abort();
		printf("Wordsize is %d\n", wordsize);
		break;
	case 'n':
		break;
	case '\n':
		XSync(dpy, True);
		printf("%d\n", ++nmsg);
		fflush(stdout);
		break;
	}
	return False;
}

gobble()
{
	int lines = 0;

	while (!readline((XtPointer)0))
		lines++;
	printf("read %d lines\n", lines);
	if (Delay > 0) {
	    XSync(dpy, True);
	    fflush(stdout);
	    usleep(Delay*1000);
	}
}
