#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <Imlib2.h>
#include <giblib/giblib.h>
#include <unistd.h>

#define BACKGROUNDCOLOR "#333333"
#define BARCOLOR		"#888888"
#define BARHEIGHT		20
#define TESTSTATUS		"Moonlight v0.1"
#define TESTPANEL		"Cirno is baka"

#define DEFAULTMASK		(EnterWindowMask | ExposureMask | FocusChangeMask | PropertyChangeMask | SubstructureNotifyMask)
#define BUTTONMASK		(ButtonPressMask | ButtonReleaseMask)
#define MOUSEMASK		(BUTTONMASK | PointerMotionMask)

////////////////////
// Just for debug //
////////////////////
int aaa = 0;

typedef struct {
	GC gc;
	XGCValues *GCvalues;
	int X, Y, Width, Height;
	Drawable drawable;
	unsigned long DefaultGCMask;
} DrawingContext;

typedef struct 
{
	Display *Display;
	int DefaultScreen;
	Window Root;
	DrawingContext *DC;
	unsigned long DefaultEventMask;
	Cursor cursor, denycursor;
	XEvent Event;
} MoonlightProps;

typedef struct Client Client;

struct Client
{
	Window window;
	int x, y, Width, Height;
	Client *Next;
};

typedef struct
{
	int x;
	int y;
} Point;

//
Window Bar;
//

// Handlers
static void KeyPressHandler (XEvent *);
static void EnterNotifyHandler (XEvent *);
static void MapRequestHandler (XEvent *);
static void UnmapNotifyHandler (XEvent *);
static void DestroyNotifyHandler (XEvent *);
static void ButtonPressHandler (XEvent *);

// Init functions
void MoonlightDisplayInit ();
static void MoonlightDisplayTerminate ();
static void StartEventHandler ();

static void ScanForWindows ();
unsigned long GetColor (const char *);
void GetMousePosition (Point *);

static void CreateBar ();
static void DrawBar ();
static void Fill (DrawingContext *);
static void Spawn (const char *);
static void Die (const char *, ...);

void CreateNewClient (Window);
Client *FindClient (Window);
void KillClient (Client *);
static void MoveClient (Client *);
void DragClient (Client *);

static void (*handler[LASTEvent]) (XEvent *) =
{
	[KeyPress] = KeyPressHandler,
	[EnterNotify] = EnterNotifyHandler,
	[MapRequest] = MapRequestHandler,
	[UnmapNotify] = UnmapNotifyHandler,
	[DestroyNotify] = DestroyNotifyHandler,
	[ButtonPress] = ButtonPressHandler
};

MoonlightProps Moonlight;
Client *HeaderClient = NULL;

void
MoonlightDisplayInit ()
{
	if (!(Moonlight.Display = XOpenDisplay(NULL)))
	{
		printf ("Connection to X failed. Is $DISPLAY variable (%s) correct?",
			getenv("DISPLAY"));
		exit (1);
	}
	
	Moonlight.DefaultScreen = DefaultScreen (Moonlight.Display);
	Moonlight.Root = RootWindow (Moonlight.Display, Moonlight.DefaultScreen);
	Moonlight.DefaultEventMask =
		SubstructureRedirectMask | KeyPressMask | DEFAULTMASK;

	Moonlight.cursor = XCreateFontCursor (Moonlight.Display, XC_left_ptr);
	Moonlight.denycursor = XCreateFontCursor (Moonlight.Display, XC_X_cursor);
	
	XSetWindowAttributes RootWindowAttrs =
	{
		.cursor = Moonlight.cursor 
	};

	XChangeWindowAttributes
	(
		Moonlight.Display, Moonlight.Root,
		CWEventMask|CWCursor, &RootWindowAttrs
	);
	
	XGCValues rgcv = {
		.function = GXxor, 
		.background = GetColor(BACKGROUNDCOLOR),
		.foreground = GetColor(BACKGROUNDCOLOR),
		.plane_mask = AllPlanes
	};
	
	DrawingContext rdc =
	{
		.Width = DisplayWidth (Moonlight.Display, Moonlight.DefaultScreen),
		.Height = DisplayHeight (Moonlight.Display, Moonlight.DefaultScreen),
		.X = 0, .Y = 0,
		.GCvalues = &rgcv,
		.DefaultGCMask = 
			GCFunction | GCPlaneMask | GCBackground | GCForeground
	};

	Moonlight.DC = &rdc;
	Moonlight.DC->gc = XCreateGC
		(Moonlight.Display, Moonlight.Root, Moonlight.DC->DefaultGCMask, Moonlight.DC->GCvalues);	
	
	Moonlight.DC->drawable = XCreatePixmap (Moonlight.Display, Moonlight.Root, Moonlight.DC->Width, Moonlight.DC->Height, DefaultDepth(Moonlight.Display, Moonlight.DefaultScreen));
	
	Fill (Moonlight.DC);

	// Is not good D:
	// Using feh while I don't know how to change wallpaper another way
	
	//system ("feh /home/flazher/dev/wm/moon.png --bg-scale");
	XClearWindow (Moonlight.Display, Moonlight.Root);
	CreateBar();
	DrawBar();
	XSelectInput (Moonlight.Display, Moonlight.Root, SubstructureRedirectMask | KeyPressMask | DEFAULTMASK);
	XFlush (Moonlight.Display);
}

static void
MoonlightDisplayTerminate ()
{
	XFreeGC (Moonlight.Display, Moonlight.DC->gc);
	XFreePixmap (Moonlight.Display, Moonlight.DC->drawable);
	XFreeCursor (Moonlight.Display, Moonlight.cursor);
	XFreeCursor (Moonlight.Display, Moonlight.denycursor);
}

static void
Die (const char *message, ...)
{
	va_list vl;
	va_start (vl, message);
	vfprintf (stderr, message, vl);
	va_end (vl);
	exit (1);
}

int
main(int argc, char **argv, char **envp)
{
	MoonlightDisplayInit ();
	//ScanForWindows ();
	StartEventHandler ();
	MoonlightDisplayTerminate ();
	XCloseDisplay (Moonlight.Display);

	return 0;
}

unsigned long
GetColor (const char *colorhex)
{
	Colormap CMap = DefaultColormap (Moonlight.Display, Moonlight.DefaultScreen);
	XColor Color;
	if (!XAllocNamedColor (Moonlight.Display, CMap, colorhex, &Color, &Color))
		Die("Cannot allocate color %s\n", colorhex);
	
	return Color.pixel;
}

static void
Fill (DrawingContext *DC)
{
	XFillRectangle
	(
 		Moonlight.Display, DC->drawable,
		DC->gc, 0, 0,
		DC->Width, DC->Height
	);
}

static void
CreateBar()
{
	XSetWindowAttributes BarAttrs =
	{
		.cursor = Moonlight.denycursor,
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = Moonlight.DefaultEventMask
	};

	Bar = XCreateWindow
	(
		Moonlight.Display, Moonlight.Root,
		0, Moonlight.DC->Height - BARHEIGHT, Moonlight.DC->Width, BARHEIGHT, 0,
		DefaultDepth (Moonlight.Display, Moonlight.DefaultScreen),
		CopyFromParent, DefaultVisual (Moonlight.Display, Moonlight.DefaultScreen),
		CWOverrideRedirect|CWBackPixmap|CWEventMask|CWCursor, &BarAttrs
	);
}

static void
DrawBar()
{
	XGCValues bgcv =
	{
		.function = GXxor,
		.background = GetColor(BARCOLOR),
		.foreground = GetColor(BARCOLOR),
		.plane_mask = AllPlanes
	};

	DrawingContext BarDC = 
	{
		.GCvalues = &bgcv,
		.X = 0, .Y = Moonlight.DC->Height - BARHEIGHT,
		.Width = Moonlight.DC->Width, .Height = BARHEIGHT,
		.drawable =
			XCreatePixmap
			(Moonlight.Display, Bar, BarDC.Width,
				 BarDC.Height, DefaultDepth(Moonlight.Display,
				 Moonlight.DefaultScreen)
			)
	};
	
	BarDC.gc = XCreateGC
	(
		 Moonlight.Display, Bar,
		 Moonlight.DC->DefaultGCMask, BarDC.GCvalues
	);

	Fill (&BarDC);
	XDrawString (Moonlight.Display, BarDC.drawable, BarDC.gc, Moonlight.DC->Width - strlen(TESTSTATUS)*6, 15, TESTSTATUS, strlen(TESTSTATUS));
	XDrawString (Moonlight.Display, BarDC.drawable, BarDC.gc, 5, 15, TESTPANEL, strlen(TESTPANEL));
	XSetWindowBackgroundPixmap(Moonlight.Display, Bar, BarDC.drawable);
	XClearWindow (Moonlight.Display, Bar);
	XMapRaised (Moonlight.Display, Bar);
	XFreeGC (Moonlight.Display, BarDC.gc);
	XFreePixmap (Moonlight.Display, BarDC.drawable);
}

static void
StartEventHandler ()
{
	XSync (Moonlight.Display, True);
	while (!XNextEvent (Moonlight.Display, &Moonlight.Event))
		if (handler[Moonlight.Event.type])
			handler[Moonlight.Event.type](&Moonlight.Event);
}

static void
EnterNotifyHandler (XEvent *Event)
{
	XFocusChangeEvent *Ev = &Event->xfocus;
	XSetInputFocus (Moonlight.Display, Ev->window, RevertToPointerRoot, CurrentTime);
}

static void
KeyPressHandler (XEvent *Event)
{
	(aaa ? (Event->xkey.window == Moonlight.Root ? MoonlightDisplayTerminate() : 0 ) : Spawn ("/usr/bin/urxvt"));
	aaa++;
}

static void MapRequestHandler (XEvent *Event)
{
	XWindowChanges WindowCh;
	WindowCh.border_width = 5;

	Client *client;
	if (!(client = FindClient (Event->xmaprequest.window)))
		CreateNewClient (Event->xmaprequest.window);
	
	XSetWindowAttributes NewWindowAttrs;
	NewWindowAttrs.event_mask = Moonlight.DefaultEventMask | MOUSEMASK;
	NewWindowAttrs.override_redirect = False;
	NewWindowAttrs.cursor = Moonlight.denycursor;
	
	XChangeWindowAttributes (Moonlight.Display, Event->xmaprequest.window, CWEventMask|CWOverrideRedirect|CWCursor, &NewWindowAttrs);
	XConfigureWindow (Moonlight.Display, Event->xmaprequest.window, CWBorderWidth, &WindowCh);
	XMapRaised (Moonlight.Display, Event->xmaprequest.window);
	XSelectInput (Moonlight.Display, Event->xmaprequest.window, Moonlight.DefaultEventMask | MOUSEMASK);
}

static void
UnmapNotifyHandler (XEvent *Event)
{
	Client *client;
	if (!(client = FindClient (Event->xunmap.window))) return;
	KillClient (client);
}

static void
DestroyNotifyHandler (XEvent *Event)
{
	Client *client;
	if (!(client = FindClient (Event->xdestroywindow.window))) return;
	KillClient (client);
}

static void
ButtonPressHandler (XEvent *Event)
{
	Client *client = FindClient (Event->xbutton.window);
//	char *dbg = malloc(128);
//	sprintf (dbg,"/home/flazher/dev/wm/a BUTTON_PRESSED,WIN==%i", (client ? Event->xbutton.window : 0));
//	Spawn (dbg);

	if (Event->xbutton.window != Moonlight.Root && client)
	{
		MoveClient (client);
	}
}

static void
ScanForWindows ()
{
	XWindowChanges WindowCh;
	Window Win1, Win2, *Windows = NULL;
	unsigned int WindowsCount;

	WindowCh.border_width = 10;

	if (XQueryTree (Moonlight.Display, Moonlight.Root, &Win1, &Win2, &Windows, &WindowsCount))
	{
		for (int i = 0; i < WindowsCount; i++)
		{
			XConfigureWindow (Moonlight.Display, Windows[i], CWBorderWidth, &WindowCh);
			XSelectInput (Moonlight.Display, Windows[i], Moonlight.DefaultEventMask);
			XMapWindow (Moonlight.Display, Windows[i]);
		}

		if (Windows)
			XFree(Windows);
	}
}

static void
Spawn (const char *command)
{
	pid_t pid;
	if ((pid = fork()) < 0)
		Die ("Error due to forking process");
	else if (!pid)
	{
		setsid();
		execlp ("/bin/sh", "sh", "-c", command, NULL);
		exit (0);
	}
}

void
CreateNewClient (Window Win)
{
	XWindowAttributes WindowAttrs;
	
	Client *client;
	client = malloc (sizeof *client);
	client->Next = HeaderClient;
	HeaderClient = client;

	XGetWindowAttributes (Moonlight.Display, Win, &WindowAttrs);

	client->window = Win;
	client->x = WindowAttrs.x;
	client->y = WindowAttrs.y;
	client->Width = WindowAttrs.width;
	client->Height = WindowAttrs.height;
}

Client *
FindClient (Window Win)
{
	Client *client;
	for (client = HeaderClient; client; client = client->Next)	
		if (Win == client->window) return client;
	return NULL;
}

void
KillClient (Client *client)
{
	if (client == HeaderClient)
		HeaderClient = client->Next;
	else for (Client *ptr = HeaderClient; ptr && ptr->Next; ptr = ptr->Next)
		if (ptr->Next == client) { ptr->Next = ptr->Next->Next; break; }
	free (client);
	XSync (Moonlight.Display, False);
}

void
DragClient (Client *client)
{
	XEvent Event;
	Point MousePosition;
	
	int WindowOldX = client->x, WindowOldY = client->y;
	XGrabServer (Moonlight.Display);
	GetMousePosition (&MousePosition);

	for (;;)
	{
		XMaskEvent
		(
			Moonlight.Display, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &Event);
		switch (Event.type)
		{
			case MotionNotify:
				client->x +=Event.xmotion.x - MousePosition.x;
				client->y +=Event.xmotion.y - MousePosition.y;
				XMoveWindow (Moonlight.Display, client->window, client->x, client->y);
				break;
			case ButtonRelease:
				XUngrabServer (Moonlight.Display);
				XUngrabPointer (Moonlight.Display, CurrentTime);
				return;
		}
	}
}

void
GetMousePosition (Point *point)
{
	Window MouseRoot, MouseWindow;
	int x, y;
	unsigned int Mask;
	XQueryPointer
	(
		Moonlight.Display, Moonlight.Root,
		&MouseRoot, &MouseWindow, &point->x,
		&point->y, &x, &y, &Mask
	);
}

static void
MoveClient (Client *client)
{
	DragClient (client);
	//XMoveWindow (Moonlight.Display, client->window, client->x, client->y);	
	//XUnmapWindow (Moonlight.Display, Bar);//client->window);
}
