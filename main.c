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
#ifdef XFT
#include <X11/Xft/Xft.h>
#endif

int aaa = 0;

typedef struct {
	GC gc;
	XGCValues GCvalues;
	int Width, Height;
	Drawable drawable;
	unsigned long DefaultGCMask;
} DrawingContext;

typedef struct 
{
	Display *Display;
	int DefaultScreen;
	Window Root;
	DrawingContext DC;
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
	#ifdef XFT
	XftDraw *xftdraw;
	#endif
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

Window Menu = NULL;
Bool MenuIsHidden = True;

DrawingContext MenuDC;

//
Window Bar;
//

#ifdef XFT
XftColor DefaultFG;
XftFont *DefaultXFTFont;
XFontStruct *DefaultFont;
#endif

// p
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
static void PropertyNotifyHandler (XEvent *);

// Init functions
void MoonlightDisplayInit ();
static void MoonlightDisplayTerminate ();
static void StartEventHandler ();

XColor GetColor (const char *);
XftColor XColor2Xft (XColor XC);

Point GetMousePosition ();

static DrawingContext CreateDrawingContext (Window, int, int, XColor, XColor);

static void CreateBar ();
static void DrawBar ();
static void ShowMenu ();
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
	[Expose]			= ExposeHandler,
	[PropertyNotify]	= PropertyNotifyHandler
};

MoonlightProps Moonlight;
Client *HeaderClient = NULL;

void
MoonlightDisplayInit ()
{
	if (!(Dpy = XOpenDisplay(NULL)))
		Die ("Connection to X failed. Is $DISPLAY variable (%s) correct?", getenv("DISPLAY"));
	
	Moonlight.DefaultScreen = DefaultScreen (Dpy);
	Moonlight.Root = RootWindow (Dpy, Moonlight.DefaultScreen);
	Moonlight.DefaultEventMask =
		SubstructureRedirectMask | KeyPressMask | DEFAULTMASK;

	Moonlight.cursor = XCreateFontCursor (Dpy, XC_left_ptr);
	Moonlight.denycursor = XCreateFontCursor (Dpy, XC_X_cursor);
	
	XSetWindowAttributes RootWindowAttrs = { .cursor = Moonlight.cursor };

	XChangeWindowAttributes
	(
		Dpy, Moonlight.Root,
		CWEventMask|CWCursor, &RootWindowAttrs
	);

	#ifdef XFT
	DefaultFG = XColor2Xft (GetColor (BARTEXTCOLOR));
	if (!(DefaultFont = XLoadQueryFont (Dpy, DEFAULTFONT))) Die ("Font %s was not found", DEFAULTFONT);
	if (!(DefaultXFTFont = XftFontOpenXlfd (Dpy, DefaultScreen (Dpy), DEFAULTFONT)))
		Die ("Font %s was not found", DEFAULTFONT);
	#endif
	
	Moonlight.DC = CreateDrawingContext 
	(
	 	Moonlight.Root, DisplayWidth (Dpy, Moonlight.DefaultScreen), 
		DisplayHeight (Dpy, Moonlight.DefaultScreen), GetColor (BACKGROUNDCOLOR), GetColor (BACKGROUNDCOLOR)
	);
	DBG ("%i", Moonlight.DC.GCvalues.background);
	//Fill (&Moonlight.DC);
	XClearWindow (Dpy, Moonlight.Root);
	CreateBar();
	DrawBar();
	XSelectInput (Dpy, Moonlight.Root, SubstructureRedirectMask | KeyPressMask | DEFAULTMASK | MOUSEMASK);
	XFlush (Dpy);
}

static void
MoonlightDisplayTerminate ()
{
	XFreeGC (Dpy, Moonlight.DC.gc);
	XFreeGC (Dpy, MenuDC.gc);
	XFreePixmap (Dpy, MenuDC.drawable);
	XFreePixmap (Dpy, Moonlight.DC.drawable);
	XFreeCursor (Dpy, Moonlight.cursor);
	XFreeCursor (Dpy, Moonlight.denycursor);
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
	XCloseDisplay (Dpy);

	return 0;
}

XColor
GetColor (const char *colorhex)
{
	Colormap CMap = DefaultColormap (Dpy, Moonlight.DefaultScreen);
	XColor Color;
	if (!XAllocNamedColor (Dpy, CMap, colorhex, &Color, &Color))
		Die("Cannot allocate color %s\n", colorhex);
	
	return Color;
}

XftColor
XColor2Xft (XColor XC)
{
	XftColor XFTC =
	{
		.color.red = XC.red,
		.color.green = XC.green,
		.color.blue = XC.blue,
		.color.alpha = 0xffff,
		.pixel = XC.pixel
	};
	return XFTC;
}

static void
Fill (DrawingContext *DC)
{
	XFillRectangle
	(
 		Dpy, DC->drawable,
		DC->gc, 0, 0,
		DC->Width, DC->Height
	);
}

static void
CreateBar()
{
	XSetWindowAttributes BarAttrs =
	{
		.cursor = Moonlight.cursor,
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = Moonlight.DefaultEventMask
	};

	Bar = XCreateWindow
	(
		Dpy, Moonlight.Root,
		0, Moonlight.DC.Height - BARHEIGHT, Moonlight.DC.Width, BARHEIGHT, 0,
		DefaultDepth (Dpy, Moonlight.DefaultScreen),
		CopyFromParent, DefaultVisual (Dpy, Moonlight.DefaultScreen),
		CWOverrideRedirect|CWBackPixmap|CWEventMask|CWCursor, &BarAttrs
	);
}

static void
DrawBar()
{
	DrawingContext BarDC = CreateDrawingContext (Bar, Moonlight.DC.Width, BARHEIGHT, GetColor (BARCOLOR), GetColor (BARCOLOR));
	Fill (&BarDC);

	#ifdef XFT
	XftDraw *dummy = XftDrawCreate(Dpy, BarDC.drawable, DefaultVisual(Dpy, DefaultScreen(Dpy)), DefaultColormap(Dpy, DefaultScreen(Dpy)));
	XftDrawStringUtf8 (dummy, &DefaultFG, DefaultXFTFont, Moonlight.DC.Width - strlen(TESTSTATUS)*6, BARHEIGHT / 2 + 5, TESTSTATUS, strlen(TESTSTATUS));
	XftDrawStringUtf8 (dummy, &DefaultFG, DefaultXFTFont, 5, BARHEIGHT / 2 + 5, TESTPANEL, strlen(TESTPANEL));

	#else
	XDrawString (Dpy, BarDC.drawable, BarDC.gc, Moonlight.DC->Width - strlen(TESTSTATUS)*6, BARHEIGHT / 2 + 5, TESTSTATUS, strlen(TESTSTATUS));
	XDrawString (Dpy, BarDC.drawable, BarDC.gc, 5, BARHEIGHT / 2 + 5, TESTPANEL, strlen(TESTPANEL));
	#endif
	XSetWindowBackgroundPixmap(Dpy, Bar, BarDC.drawable);
	XClearWindow (Dpy, Bar);
	XMapRaised (Dpy, Bar);
	XFreeGC (Dpy, BarDC.gc);
	XFreePixmap (Dpy, BarDC.drawable);
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
		Dpy, Moonlight.Root,
		WindowAttrs->x, WindowAttrs->y - WINBARHEIGHT, WindowAttrs->width + (WINBORDERSIZE<<1), WINBARHEIGHT, 0,
		DefaultDepth (Dpy, Moonlight.DefaultScreen),
		CopyFromParent, DefaultVisual (Dpy, Moonlight.DefaultScreen),
		CWOverrideRedirect|CWBackPixmap|CWEventMask|CWCursor, &BarAttrs
	);

	XGrabButton
	(
		Dpy, AnyButton, AnyModifier,
		WindowBar, False,
		MOUSEMASK, GrabModeAsync, GrabModeAsync, None, None
	);
	
	client->BarDC = CreateDrawingContext (WindowBar, WindowAttrs->width, WINBARHEIGHT, GetColor (WINBARCOLOR), GetColor (WINBARCOLOR));
	#ifdef XFT
	client->xftdraw = XftDrawCreate(Dpy, client->BarDC.drawable, DefaultVisual(Dpy, DefaultScreen(Dpy)), DefaultColormap(Dpy, DefaultScreen(Dpy)));
	#endif

	return WindowBar;
}

void
RedrawWindowBar (Client *client)
{
	client->BarDC.Width = client->width;
	client->BarDC.GCvalues.function = GXclear;	
	XChangeGC (Dpy, client->BarDC.gc, GCFunction, &client->BarDC.GCvalues);
	Fill (&client->BarDC);
	client->BarDC.GCvalues.function = GXor;	
	client->BarDC.GCvalues.foreground = GetColor (WINBARCOLOR).pixel;
	XChangeGC (Dpy, client->BarDC.gc, GCFunction | GCForeground, &client->BarDC.GCvalues);
	Fill (&client->BarDC);
	client->BarDC.GCvalues.foreground = GetColor (BARTEXTCOLOR).pixel;
	XChangeGC (Dpy, client->BarDC.gc, GCForeground, &client->BarDC.GCvalues);
	if (client->name)
	#ifdef XFT
	{
		XftDrawStringUtf8 (client->xftdraw, &DefaultFG, DefaultXFTFont, 15, 15, (FcChar8 *)client->name, strlen (client->name));
		XftDrawStringUtf8 (client->xftdraw, &DefaultFG, DefaultXFTFont, client->BarDC.Width - 27, 15, "-  X", 4);
	}
	#else
		XDrawString (Dpy, client->BarDC.drawable, client->BarDC.gc, 15, 15, client->name, strlen(client->name));
	#endif
	XClearWindow (Dpy, client->bar);
	XSetWindowBackgroundPixmap(Dpy, client->bar, client->BarDC.drawable);
}

static void
StartEventHandler ()
{
	XSync (Dpy, True);
	while (!XNextEvent (Dpy, &Moonlight.Event))
		if (handler[Moonlight.Event.type])
			handler[Moonlight.Event.type](&Moonlight.Event);
}

static void
EnterNotifyHandler (XEvent *Event)
{
	Client *client;
	if ((client = FindClient (Event->xfocus.window)))
	{
		XRaiseWindow (Dpy, client->window);
		XRaiseWindow (Dpy, client->bar);
	}

	XRaiseWindow (Dpy, Event->xfocus.window);
	XFocusChangeEvent *Ev = &Event->xfocus;
	XSetInputFocus (Dpy, Ev->window, RevertToPointerRoot, CurrentTime);
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
	{
		CreateNewClient (Event->xmaprequest.window);
		XSetWindowAttributes WindowAttrs = { .event_mask = Moonlight.DefaultEventMask };
		XWindowChanges WindowCh = { .border_width = WINBORDERSIZE };
		XSelectInput (Dpy, Event->xmaprequest.window, Moonlight.DefaultEventMask);
	
		XGrabButton
		(
			Dpy, AnyButton, MODKEY,
			Event->xmaprequest.window, False,
			MOUSEMASK, GrabModeAsync, GrabModeAsync, None, None
		);

		XChangeWindowAttributes
		(
			Dpy, Event->xmaprequest.window,
			CWEventMask, &WindowAttrs
		);

		XConfigureWindow (Dpy, Event->xmaprequest.window, CWBorderWidth | CWEventMask, &WindowCh);
	}
	XMapWindow (Dpy, Event->xmaprequest.window);
	if (client) XMapRaised (Dpy, client->bar);
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
	if (client->BarDC.drawable) XFreePixmap (Dpy, client->BarDC.drawable);
	if (client->BarDC.gc) XFreeGC (Dpy, client->BarDC.gc);
	#ifdef XFT
	if (client->xftdraw) XftDrawDestroy (client->xftdraw);
	#endif
	XDestroyWindow (Dpy, client->bar);
	if (client->name) XFree (client->name);
	KillClient (client);
}

void
ButtonPressHandler (XEvent *Event)
{
	Point MousePosition = GetMousePosition ();
	Client *client = FindClient (Event->xbutton.window);
	
	switch (Event->xbutton.button)
	{
		case Button1: 
			if (Event->xbutton.window != Moonlight.Root && client)
				(Event->xbutton.window == client->bar ? MoveClient (client) :
				(MousePosition.x > client->x + client->width / 2 ||
		 		MousePosition.y > client->y + client->height / 2 ? 
		 		ResizeClient (client) : MoveClient (client)));
			break;
		case Button3:
			if (Event->xbutton.window == Moonlight.Root)
				ShowMenu ();
			break;
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

	XConfigureWindow (Dpy, Event->xconfigurerequest.window, Event->xconfigurerequest.value_mask, &WindowCh);
}

static void
PropertyNotifyHandler (XEvent *Event)
{
	Client *client;
	if (!(client = FindClient (Event->xproperty.window))) return;
	if (client->name) XFree (client->name);
	XFetchName (Dpy, Event->xproperty.window, &client->name);
	RedrawWindowBar (client);
}

static void
Redraw (Window Win)
{
	Client *client;
	if (!(client = FindClient (Win))) return;
	XWindowChanges WindowCh = 
	{ .x = client->x, .y = client->y,
	.width = client->width, .height = client->height };

	XConfigureWindow (Dpy, Win,
	CWX | CWY | CWWidth | CWHeight, &WindowCh);

	XClearWindow (Dpy, Win);
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

	XGetWindowAttributes (Dpy, Win, &WindowAttrs);
	XFetchName (Dpy, Win, &client->name);
	
	client->x = WindowAttrs.x;
	client->y = WindowAttrs.y;
	client->width = WindowAttrs.width;
	client->height = WindowAttrs.height;

	client->window = Win;
	client->bar = CreateWindowBar (client, &WindowAttrs);
	RedrawWindowBar (client);
	XMapWindow (Dpy, client->bar);
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
	XSync (Dpy, False);
}

void
DragClient (Client *client)
{
	XEvent Event;
	Point MousePosition = GetMousePosition ();
	XWindowAttributes WindowAttrs;
	XGetWindowAttributes (Dpy, client->window, &WindowAttrs);

	XGrabServer (Dpy);
	int XDiff = MousePosition.x - client->x, YDiff = MousePosition.y - client->y;

	for (;;)
	{
		XMaskEvent
		(
			Dpy, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &Event);
		switch (Event.type)
		{
			case MotionNotify:
				client->x = client->x + Event.xmotion.x - XDiff;
				client->y = (Event.xbutton.window == client->bar ? ~WINBARHEIGHT: 0) + client->y + Event.xmotion.y - YDiff;
				XMoveWindow (Dpy, client->window, client->x, client->y);
				XMoveWindow (Dpy, client->bar, client->x, client->y - WINBARHEIGHT);

				break;
			case ButtonRelease:
				XUngrabServer (Dpy);
				XUngrabPointer (Dpy, CurrentTime);
				return;
		}
	}
}

void
ResizeClient (Client *client)
{
	XEvent Event;
	Point MousePosition = GetMousePosition (), OldPoint;
	XGrabServer (Dpy);
	OldPoint.x = MousePosition.x - client->x;
	OldPoint.y = MousePosition.y - client->y;
	
	for (;;)
	{
		XMaskEvent
		(
			Dpy, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &Event);
		switch (Event.type)
		{
			case MotionNotify:
				Redraw (client->window);
				RedrawWindowBar (client);
				RecalculateWindowSize
				(
				 	client,
					DeterminateCornerPosition (client, Event.xmotion.x, Event.xmotion.y),
					Event.xmotion.x - OldPoint.x,
					Event.xmotion.y - OldPoint.y
				);
				OldPoint.x = Event.xmotion.x;
				OldPoint.y = Event.xmotion.y;
				break;
			case ButtonRelease:
				XUngrabServer (Dpy);
				XUngrabPointer (Dpy, CurrentTime);
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
		Dpy, Moonlight.Root,
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
		Dpy, client->window,
		client->x, client->y,
		client->width, client->height
	);
	
	XWindowAttributes BarAttrs;
	XGetWindowAttributes (Dpy, client->bar, &BarAttrs);
	XMoveResizeWindow
	(
		Dpy, client->bar,
		BarAttrs.x, BarAttrs.y,
		client->width + (WINBORDERSIZE<<1), BarAttrs.height
	);
}

static void
ShowMenu ()
{
	Point MousePosition = GetMousePosition ();
	if (!Menu)
	{
		XSetWindowAttributes MenuAttrs =
		{
			.cursor = Moonlight.cursor, .override_redirect = True,
			.background_pixmap = ParentRelative, .event_mask = Moonlight.DefaultEventMask
		};
	
		Menu = XCreateWindow
		(
			Dpy, Moonlight.Root, MousePosition.x, MousePosition.y, MENUWIDTH, MENUHEIGHT, 0,
			DefaultDepth (Dpy, Moonlight.DefaultScreen), CopyFromParent,
			DefaultVisual (Dpy, Moonlight.DefaultScreen),
			CWOverrideRedirect|CWBackPixmap|CWEventMask|CWCursor, &MenuAttrs
		);

		MenuDC = CreateDrawingContext (Menu, MENUWIDTH, MENUHEIGHT, GetColor (MENUCOLOR), GetColor (MENUCOLOR));
		Fill (&MenuDC);
		XSetWindowBackgroundPixmap(Dpy, Menu, MenuDC.drawable);
	}
	else
		XMoveWindow (Dpy, Menu, MousePosition.x, MousePosition.y);
	
	XClearWindow (Dpy, Menu);
	(MenuIsHidden ? XMapRaised (Dpy, Menu) : XUnmapWindow (Dpy, Menu));
	MenuIsHidden ^= True;
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

static DrawingContext
CreateDrawingContext (Window Win, int Width, int Height, XColor Background, XColor Foreground)
{
	DrawingContext DC = {
		.Width = Width, .Height = Height,
		.DefaultGCMask = GCFunction | GCPlaneMask | GCBackground | GCForeground,
		.GCvalues.function = GXcopy,
		.GCvalues.background = Background.pixel,
		.GCvalues.foreground = Foreground.pixel,
		.GCvalues.plane_mask = AllPlanes,
		.drawable = XCreatePixmap (Dpy, Win, Width, Height, DefaultDepth(Dpy, Moonlight.DefaultScreen))
	};
		DC.gc = XCreateGC (Dpy, DC.drawable, DC.DefaultGCMask, &DC.GCvalues);
		return DC;
}
