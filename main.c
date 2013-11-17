#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <Imlib2.h>
#include <giblib/giblib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "moonlight.h"

#define CEVENT	"[\x1B[36mEVENT\x1B[0m]\t\t"
#define CMAP	"[\x1B[32mMAP\x1B[0m]\t\t"
#define CUNMAP	"[\x1B[31mUNMAP\x1B[0m]\t\t"
#define CDESTR	"[\x1B[31mDESTROY\x1B[0m]\t"
#define CREQU	"[\x1B[33mREQUEST\x1B[0m]\t"
#define CSUCCS	"\t\t  [\x1B[32m*\x1B[0m]"
#define CFAULT	"\t\t  [\x1B[31m*\x1B[0m]"
#define CACT	"[\x1B[34mACTION\x1B[0m]\t"

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
	int number;
	Window window;
	int x, y, width, height;
	Client *Next;
};

typedef struct
{
	int x;
	int y;
} Point;

enum
{
	TOP_LEFT_CORNER,
	TOP_RIGHT_CORNER,
	BOTTOM_LEFT_CORNER,
	BOTTOM_RIGHT_CORNER
};

//
Window Bar;
//

// Debug functions
//
static void DBG (const char *msg, ...);
static void GetClientProperties (Client *);

// Handlers
static void KeyPressHandler (XEvent *);
static void EnterNotifyHandler (XEvent *);
static void MapRequestHandler (XEvent *);
static void UnmapNotifyHandler (XEvent *);
static void DestroyNotifyHandler (XEvent *);
void ButtonPressHandler (XEvent *);
static void ConfigureRequestHandler (XEvent *);

// Init functions
void MoonlightDisplayInit ();
static void MoonlightDisplayTerminate ();
static void StartEventHandler ();

static void ScanForWindows ();
unsigned long GetColor (const char *);
Point GetMousePosition ();

static void CreateBar ();
static void DrawBar ();
static void Fill (DrawingContext *);
static void Spawn (const char *);
static void Die (const char *, ...);

void CreateNewClient (Window);
Client *FindClient (Window);
void KillClient (Client *);
static void MoveClient (Client *);
static void ResizeClient (Client *);
void DragClient (Client *);
void SweepClient (Client *);

void RecalculateWindowSize (Client *, int, int, int);
int DeterminateCornerPosition (Client *, int, int);

//
static void SendConfigureEvent (Client *);
//

static void (*handler[LASTEvent]) (XEvent *) =
{
	[KeyPress] = KeyPressHandler,
	[EnterNotify] = EnterNotifyHandler,
	[MapRequest] = MapRequestHandler,
	[UnmapNotify] = UnmapNotifyHandler,
	[DestroyNotify] = DestroyNotifyHandler,
	[ButtonPress] = ButtonPressHandler,
	[ConfigureRequest] = ConfigureRequestHandler
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
	DBG ("-- Initializing with root window #%li --", Moonlight.Root);
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
	
	system ("feh moon.png --bg-scale");
	XClearWindow (Moonlight.Display, Moonlight.Root);
	CreateBar();
	DrawBar();
	XSelectInput (Moonlight.Display, Moonlight.Root, SubstructureRedirectMask | KeyPressMask | DEFAULTMASK | MOUSEMASK);
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
	DBG ("%s Window %lu map request (parent %lu)", CMAP, Event->xmaprequest.window, Moonlight.Root);
	
	XWindowChanges WindowCh;
	WindowCh.border_width = 5;

	Client *client;
	if (!(client = FindClient (Event->xmaprequest.window)))
		CreateNewClient (Event->xmaprequest.window);
	
	XSetWindowAttributes NewWindowAttrs = 
	{
		.event_mask = Moonlight.DefaultEventMask | MOUSEMASK
	};

	
	XSelectInput (Moonlight.Display, Event->xmaprequest.window, Moonlight.DefaultEventMask);// | MOUSEMASK);
	XConfigureWindow (Moonlight.Display, Event->xmaprequest.window, CWBorderWidth, &WindowCh);
	XMapWindow (Moonlight.Display, Event->xmaprequest.window);
}

static void
UnmapNotifyHandler (XEvent *Event)
{
	DBG ("%s Window %u has been unmapped", CUNMAP, Event->xunmap.window);

	Client *client;
	if (!(client = FindClient (Event->xunmap.window))) return;
	KillClient (client);
}

static void
DestroyNotifyHandler (XEvent *Event)
{
	DBG ("%s Window %u had been destroyed", CDESTR, Event->xunmap.window);

	Client *client;
	if (!(client = FindClient (Event->xdestroywindow.window))) return;
	KillClient (client);
}

void
ButtonPressHandler (XEvent *Event)
{
	DBG ("%s Window %u catching buttonpress event for window", CEVENT, Event->xbutton.window);

	Point MousePosition = GetMousePosition ();
	Client *client = FindClient (Event->xbutton.window);
	if (Event->xbutton.window != Moonlight.Root && client)
	{
		(MousePosition.x > client->x + client->width / 2 ||
		 MousePosition.y > client->y + client->height / 2 ? 
		 ResizeClient (client) : MoveClient (client));
	}
}

void
ConfigureRequestHandler (XEvent *Event)
{
	Client *client = FindClient (Event->xconfigurerequest.window);
	DBG ("%s Window %li requested to be configured", CREQU, Event->xconfigurerequest.window);
	if (client)
	{
		XWindowChanges WindowCh = 
		{
			.x = client->x,
			.y = client->y,
			.width = client->width,
			.height = client->height
		
		};
		XConfigureWindow (Moonlight.Display, client->window, Event->xconfigurerequest.value_mask, &WindowCh);
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
	client->number = (HeaderClient ? HeaderClient->number + 1 : 1);
	client->Next = HeaderClient;
	HeaderClient = client;

	XGetWindowAttributes (Moonlight.Display, Win, &WindowAttrs);
	
	client->window = Win;
	client->x = WindowAttrs.x;
	client->y = WindowAttrs.y;
	client->width = WindowAttrs.width;
	client->height = WindowAttrs.height;

	DBG ("%s Creating new client # %u for window %u", CACT, client->number, Win);
}

Client *
FindClient (Window Win)
{
	DBG ("%s Find_client-request for window #%li", CREQU, Win);
	Client *client;
	for (client = HeaderClient; client; client = client->Next)	
		if (Win == client->window)
		{
			DBG ("%s Found it! Client #%i", CSUCCS, client->number); return client;
		}
	DBG ("%s And client was not found", CFAULT);
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
	Point MousePosition = GetMousePosition ();
	
	XGrabServer (Moonlight.Display);
	int XDiff = MousePosition.x - client->x, YDiff = MousePosition.y - client->y;

	for (;;)
	{
		XMaskEvent
		(
			Moonlight.Display, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &Event);
		switch (Event.type)
		{
			case MotionNotify:
				client->x = client->x + Event.xmotion.x - XDiff;
				client->y = client->y + Event.xmotion.y - YDiff;
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
SweepClient (Client *client)
{
	XEvent Event;
	Point MousePosition = GetMousePosition (), OldPoint;
	XGrabServer (Moonlight.Display);
	OldPoint.x = MousePosition.x;
	OldPoint.y = MousePosition.y;
	
	for (;;)
	{
		XMaskEvent
		(
			Moonlight.Display, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &Event);
		switch (Event.type)
		{
			case MotionNotify:
				RecalculateWindowSize
				(
				 	client,
					DeterminateCornerPosition (client, Event.xmotion.x, Event.xmotion.y),
					Event.xmotion.x - OldPoint.x,
					Event.xmotion.y - OldPoint.y
				);
				DBG("%s Corner %i", CEVENT, DeterminateCornerPosition (client, Event.xmotion.x, Event.xmotion.y));
				OldPoint.x = Event.xmotion.x;
				OldPoint.y = Event.xmotion.y;
				break;
			case ButtonRelease:
				XUngrabServer (Moonlight.Display);
				XUngrabPointer (Moonlight.Display, CurrentTime);
				XClearWindow (Moonlight.Display, client->window);
				SendConfigureEvent (client);
				GetClientProperties (client);
				return;
		}
	}
}

Point
GetMousePosition ()
{
	Point Result;
	Window MouseRoot, MouseWindow;
	int x, y;
	unsigned int Mask;
	XQueryPointer
	(
		Moonlight.Display, Moonlight.Root,
		&MouseRoot, &MouseWindow, &Result.x,
		&Result.y, &x, &y, &Mask
	);
	return Result;
}

static void
MoveClient (Client *client)
{
	DragClient (client);
}

static void
ResizeClient (Client *client)
{
	SweepClient (client);
}

int
DeterminateCornerPosition (Client *client, int MouseX, int MouseY)
{
	return
	(MouseX > client->x + client->width / 2 ?
		(MouseY > client->y + client->height / 2 ?
		 	BOTTOM_RIGHT_CORNER : TOP_RIGHT_CORNER) :
		(MouseY > client->y + client->height / 2 ?
			BOTTOM_LEFT_CORNER : TOP_LEFT_CORNER
		)
	);
}

void
RecalculateWindowSize (Client *client, int corner, int deltaX, int deltaY)
{
	client->width +=
		(corner == BOTTOM_RIGHT_CORNER || corner == TOP_RIGHT_CORNER ? deltaX : 0);
	client->height +=
		(corner == BOTTOM_RIGHT_CORNER || corner == BOTTOM_LEFT_CORNER ? deltaY : 0);

	XMoveResizeWindow
	(
		Moonlight.Display, client->window,
		client->x, client->y,
		client->width, client->height
	);
}

static void
SendConfigureEvent (Client *client)
{
	DBG ("%s Send configure event for %li window", CACT, client->window);

	XConfigureEvent ConfEvent =
	{
		.type = ConfigureNotify,
		.event = client->window,
		.window = client->window,
		.x = client->x,
		.y = client->y,
		.width = client->width,
		.height = client->height,
	};
	XSendEvent (Moonlight.Display, client->window, False, StructureNotifyMask | PropertyChangeMask, (XEvent *)&ConfEvent);
}

static void DBG (const char *msg, ...)
{
	va_list vl;
	int fd = open ("debug", O_CREAT | O_WRONLY | O_APPEND, 0666);
	char *dbgmsg = malloc (512);
	va_start (vl, msg);
	vsprintf (dbgmsg, msg, vl);
	va_end (vl);
	strcat (dbgmsg, "\n");
	write (fd, dbgmsg, strlen (dbgmsg));
	close (fd);
	free (dbgmsg);
}

static void GetClientProperties (Client *client)
{
	XWindowAttributes XA;

	XGetWindowAttributes (Moonlight.Display, client->window, &XA);

	DBG ("** Client and associated window properties **\n\tWindow: x: %i, y: %i\n\t width: %i, height: %i\n\tClient: x: %i, y: %i\n\t width: %i, height: %i\n\n", XA.x, XA.y, XA.width, XA.height, client->x, client->y, client->width, client->height);
}
