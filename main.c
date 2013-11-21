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

////////////////////
// Just for debug //
////////////////////

int aaa = 0;

///////////////////////////////////
// TODO: 
// 		+ REMOVE PIXMAP AND GCs OF
// 		REMOVED CLIENTS
//
// 		+ FIX RESIZE
// 		- AND MOVING
//
// 		+ UPDATE PIXMAPS OF
// 		EXPOSED CLIENTS
//		
//		- CHANGE BAR AND CLIENT
//		WIDTH/HEIGHT/X/Y WHEN
//		WINDOW DO IT ITSELF
//
///////////////////////////////////

typedef struct {
	GC gc;
	XGCValues GCvalues;
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
	char *name;
	DrawingContext BarDC;
	Window window, bar;
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

// Handlers
static void KeyPressHandler (XEvent *);
static void EnterNotifyHandler (XEvent *);
static void MapRequestHandler (XEvent *);
static void UnmapNotifyHandler (XEvent *);
static void DestroyNotifyHandler (XEvent *);
void ButtonPressHandler (XEvent *);
static void ConfigureRequestHandler (XEvent *);
static void ExposeHandler (XEvent *);

// Init functions
void MoonlightDisplayInit ();
static void MoonlightDisplayTerminate ();
static void StartEventHandler ();

unsigned long GetColor (const char *);
Point GetMousePosition ();

static void CreateBar ();
static void DrawBar ();
static void Fill (DrawingContext *);
static void Redraw (Window);
static void Spawn (const char *);
static void Die (const char *, ...);

void CreateNewClient (Window);
Window CreateWindowBar (Client *, XWindowAttributes *);
void RedrawWindowBar (Client *);
Client *FindClient (Window);
void KillClient (Client *);
static void MoveClient (Client *);
static void ResizeClient (Client *);
void DragClient (Client *);

void RecalculateWindowSize (Client *, int, int, int);
int DeterminateCornerPosition (Client *, int, int);

static void (*handler[LASTEvent]) (XEvent *) =
{
	[KeyPress] 			= KeyPressHandler,
	[EnterNotify] 		= EnterNotifyHandler,
	[MapRequest] 		= MapRequestHandler,
	[UnmapNotify] 		= UnmapNotifyHandler,
	[DestroyNotify] 	= DestroyNotifyHandler,
	[ButtonPress] 		= ButtonPressHandler,
	[ConfigureRequest] 	= ConfigureRequestHandler,
	[Expose]			= ExposeHandler
};

MoonlightProps Moonlight;
Client *HeaderClient = NULL;

void
MoonlightDisplayInit ()
{
	if (!(Moonlight.Display = XOpenDisplay(NULL)))
		Die ("Connection to X failed. Is $DISPLAY variable (%s) correct?", getenv("DISPLAY"));
	
	Moonlight.DefaultScreen = DefaultScreen (Moonlight.Display);
	Moonlight.Root = RootWindow (Moonlight.Display, Moonlight.DefaultScreen);
	Moonlight.DefaultEventMask =
		SubstructureRedirectMask | KeyPressMask | DEFAULTMASK;

	Moonlight.cursor = XCreateFontCursor (Moonlight.Display, XC_left_ptr);
	Moonlight.denycursor = XCreateFontCursor (Moonlight.Display, XC_X_cursor);
	
	XSetWindowAttributes RootWindowAttrs = { .cursor = Moonlight.cursor };

	XChangeWindowAttributes
	(
		Moonlight.Display, Moonlight.Root,
		CWEventMask|CWCursor, &RootWindowAttrs
	);
	
	DrawingContext rdc =
	{
		.Width = DisplayWidth (Moonlight.Display, Moonlight.DefaultScreen),
		.Height = DisplayHeight (Moonlight.Display, Moonlight.DefaultScreen),
		.X = 0, .Y = 0,
		.DefaultGCMask = 
			GCFunction | GCPlaneMask | GCBackground | GCForeground
	};
	
	rdc.GCvalues.function = GXxor; 
	rdc.GCvalues.background = GetColor(BACKGROUNDCOLOR);
	rdc.GCvalues.foreground = GetColor(BACKGROUNDCOLOR);
	rdc.GCvalues.plane_mask = AllPlanes;

	Moonlight.DC = &rdc;
	Moonlight.DC->gc = XCreateGC
		(Moonlight.Display, Moonlight.Root, Moonlight.DC->DefaultGCMask, &Moonlight.DC->GCvalues);	
	
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
	DrawingContext BarDC = 
	{
		.X = 0, .Y = Moonlight.DC->Height - BARHEIGHT,
		.Width = Moonlight.DC->Width, .Height = BARHEIGHT,
		.drawable =
			XCreatePixmap
			(Moonlight.Display, Bar, BarDC.Width,
				 BarDC.Height, DefaultDepth(Moonlight.Display,
				 Moonlight.DefaultScreen)
			)
	};

	BarDC.GCvalues.function = GXxor;
	BarDC.GCvalues.background = GetColor(BARCOLOR);
	BarDC.GCvalues.foreground = GetColor(BARCOLOR);
	BarDC.GCvalues.plane_mask = AllPlanes;
	
	BarDC.gc = XCreateGC
	(
		 Moonlight.Display, Bar,
		 Moonlight.DC->DefaultGCMask, &BarDC.GCvalues
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

Window CreateWindowBar (Client *client, XWindowAttributes *WindowAttrs)
{
	XSetWindowAttributes BarAttrs =
	{
		.cursor = Moonlight.denycursor,
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = Moonlight.DefaultEventMask
	};

	Window WindowBar = XCreateWindow
	(
		Moonlight.Display, Moonlight.Root,
		WindowAttrs->x + 3, WindowAttrs->y - WINBARHEIGHT, WindowAttrs->width + (WINBORDERSIZE<<1), WINBARHEIGHT, 0,
		DefaultDepth (Moonlight.Display, Moonlight.DefaultScreen),
		CopyFromParent, DefaultVisual (Moonlight.Display, Moonlight.DefaultScreen),
		CWOverrideRedirect|CWBackPixmap|CWEventMask|CWCursor, &BarAttrs
	);

	XGrabButton
	(
		Moonlight.Display, AnyButton, AnyModifier,
		WindowBar, False,
		MOUSEMASK, GrabModeAsync, GrabModeAsync, None, None
	);

	client->BarDC.Width = WindowAttrs->width;
	client->BarDC.Height = WINBARHEIGHT;
	client->BarDC.X = 0;
	client->BarDC.Y = 0;
	client->BarDC.DefaultGCMask = GCFunction | GCPlaneMask | GCBackground | GCForeground;
	client->BarDC.GCvalues.function = GXclear;
	client->BarDC.GCvalues.background = GetColor("#888888");
	client->BarDC.GCvalues.foreground = GetColor("#222222");
	client->BarDC.GCvalues.plane_mask = AllPlanes;
	client->BarDC.gc = XCreateGC
		(Moonlight.Display, WindowBar, client->BarDC.DefaultGCMask, &client->BarDC.GCvalues);	
	return WindowBar;
}

void
RedrawWindowBar (Client *client)
{
	client->BarDC.Width = client->width;
	client->BarDC.drawable =
		XCreatePixmap
		(
		 	Moonlight.Display, client->bar, client->width, WINBARHEIGHT,
			DefaultDepth(Moonlight.Display, Moonlight.DefaultScreen)
		);
	client->BarDC.GCvalues.function = GXclear;	
	XChangeGC (Moonlight.Display, client->BarDC.gc, GCFunction, &client->BarDC.GCvalues);
	
	Fill (&client->BarDC);
	
	client->BarDC.GCvalues.function = GXxor;	
	XChangeGC (Moonlight.Display, client->BarDC.gc, GCFunction, &client->BarDC.GCvalues);

	Fill (&client->BarDC);

	if (client->name)
	XDrawString (Moonlight.Display, client->BarDC.drawable, client->BarDC.gc, 15, 15, client->name, strlen(client->name));
	
	XSetWindowBackgroundPixmap(Moonlight.Display, client->bar, client->BarDC.drawable);
	XClearWindow (Moonlight.Display, client->bar);
	//XFreeGC (Moonlight.Display, client->BarDC.gc);
	XFreePixmap (Moonlight.Display, client->BarDC.drawable);
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
	Client *client;
	if ((client = FindClient (Event->xfocus.window)))
	{
		XRaiseWindow (Moonlight.Display, client->window);
		XRaiseWindow (Moonlight.Display, client->bar);
	}

	XRaiseWindow (Moonlight.Display, Event->xfocus.window);
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
	Client *client;
	if (!(client = FindClient (Event->xmaprequest.window)))
		CreateNewClient (Event->xmaprequest.window);
	
	XSetWindowAttributes WindowAttrs = { .event_mask = Moonlight.DefaultEventMask };
	XWindowChanges WindowCh = { .border_width = WINBORDERSIZE };
	XSelectInput (Moonlight.Display, Event->xmaprequest.window, Moonlight.DefaultEventMask);
	
	XGrabButton
	(
		Moonlight.Display, AnyButton, AnyModifier,
		Event->xmaprequest.window, False,
		MOUSEMASK, GrabModeAsync, GrabModeAsync, None, None
	);

	XChangeWindowAttributes
	(
		Moonlight.Display, Event->xmaprequest.window,
		CWEventMask, &WindowAttrs
	);

	XConfigureWindow (Moonlight.Display, Event->xmaprequest.window, CWBorderWidth | CWEventMask, &WindowCh);
	XMapWindow (Moonlight.Display, Event->xmaprequest.window);
}

static void
UnmapNotifyHandler (XEvent *Event)
{

}

static void
DestroyNotifyHandler (XEvent *Event)
{
	Client *client;
	if (!(client = FindClient (Event->xdestroywindow.window))) return;
	XDestroyWindow (Moonlight.Display, client->bar);
	KillClient (client);
}

void
ButtonPressHandler (XEvent *Event)
{
	Point MousePosition = GetMousePosition ();
	Client *client = FindClient (Event->xbutton.window);
	if (Event->xbutton.window != Moonlight.Root && client)
	{
		/*    __
      		 /  \*
             |__|
            //^^|\  Ternary operators are cool
            |O-O||  /
             \__/
           /|[><]|\
           ||    ||
	*/
		(Event->xbutton.window == client->bar ? MoveClient (client) :
		(MousePosition.x > client->x + client->width / 2 ||
		 MousePosition.y > client->y + client->height / 2 ? 
		 ResizeClient (client) : MoveClient (client)));
	}
}

void
ConfigureRequestHandler (XEvent *Event)
{
	XWindowChanges WindowCh = 
	{
		.x = Event->xconfigurerequest.x,
		.y = Event->xconfigurerequest.y,
		.width = Event->xconfigurerequest.width,
		.height = Event->xconfigurerequest.height
	};

	XConfigureWindow (Moonlight.Display, Event->xconfigurerequest.window, Event->xconfigurerequest.value_mask, &WindowCh);
}

static void
Redraw (Window Win)
{
	Client *client;
	if (!(client = FindClient (Win))) return;
	XWindowChanges WindowCh = 
	{ .x = client->x, .y = client->y,
	.width = client->width, .height = client->height };

	XConfigureWindow (Moonlight.Display, Win,
	CWX | CWY | CWWidth | CWHeight, &WindowCh);

	XClearWindow (Moonlight.Display, Win);
}

static void
ExposeHandler (XEvent *Event)
{
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
	XFetchName (Moonlight.Display, Win, &client->name);
	
	client->x = WindowAttrs.x;
	client->y = WindowAttrs.y;
	client->width = WindowAttrs.width;
	client->height = WindowAttrs.height;

	client->window = Win;
	client->bar = CreateWindowBar (client, &WindowAttrs);
	RedrawWindowBar (client);
	XMapWindow (Moonlight.Display, client->bar);
}

Client *
FindClient (Window Win)
{
	Client *client;
	for (client = HeaderClient; client; client = client->Next)	
		if (Win == client->window || Win == client->bar) return client;
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
	XWindowAttributes WindowAttrs;
	XGetWindowAttributes (Moonlight.Display, client->window, &WindowAttrs);

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
				XMoveWindow (Moonlight.Display, client->bar, client->x, client->y - WINBARHEIGHT);

				break;
			case ButtonRelease:
				XUngrabServer (Moonlight.Display);
				XUngrabPointer (Moonlight.Display, CurrentTime);
				return;
		}
	}
}

void
ResizeClient (Client *client)
{
	XEvent Event;
	Point MousePosition = GetMousePosition (), OldPoint;
	XGrabServer (Moonlight.Display);
	OldPoint.x = MousePosition.x - client->x;
	OldPoint.y = MousePosition.y - client->y;
	
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
				OldPoint.x = Event.xmotion.x;
				OldPoint.y = Event.xmotion.y;
				RedrawWindowBar (client);
				break;
			case ButtonRelease:
				XUngrabServer (Moonlight.Display);
				XUngrabPointer (Moonlight.Display, CurrentTime);
				Redraw (client->window);
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

int
DeterminateCornerPosition (Client *client, int MouseX, int MouseY)
{
	return
	(MouseX > client->width / 2 ?
		(MouseY > client->height / 2 ?
		 	BOTTOM_RIGHT_CORNER : TOP_RIGHT_CORNER) :
		(MouseY > client->height / 2 ?
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
	
	XWindowAttributes BarAttrs;
	XGetWindowAttributes (Moonlight.Display, client->bar, &BarAttrs);
	XMoveResizeWindow
	(
		Moonlight.Display, client->bar,
		BarAttrs.x, BarAttrs.y,
		client->width + (WINBORDERSIZE<<1), BarAttrs.height
	);
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
