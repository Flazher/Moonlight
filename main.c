#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

typedef struct {
	GC gc;
	XGCValues GCvalues;
	int x, y, Width, Height;
	Drawable drawable;
	unsigned long DefaultGCMask;
} DrawingContext;

typedef struct 
{
	Display *Display;
	int DefaultScreen;
	Window Root;
	DrawingContext DC;
	Window BackgroundWindow;
	unsigned long DefaultEventMask;
} MoonlightProps;

void MoonlightDisplayInit (MoonlightProps *Moonlight);
static void MoonlightDisplayTerminate (MoonlightProps *Moonlight);

MoonlightProps Moonlight;

void
MoonlightDisplayInit (MoonlightProps *Moonlight)
{
	MoonlightProps *M = Moonlight;
	if (!(M->Display = XOpenDisplay(NULL)))
	{
		printf ("Connection to X failed. Is $DISPLAY variable (%s) correct?",
			getenv("DISPLAY"));
		exit (1);
	}
	
	M->DefaultScreen = DefaultScreen (M->Display);
	M->Root = RootWindow (M->Display, M->DefaultScreen);
	M->DefaultEventMask = ExposureMask | KeyPressMask;

	M->DC.GCvalues.function   = GXxor; 
	M->DC.GCvalues.background = WhitePixel (M->Display, M->DefaultScreen);
	M->DC.GCvalues.foreground = WhitePixel (M->Display, M->DefaultScreen);
	M->DC.GCvalues.plane_mask = AllPlanes;
	M->DC.Width = DisplayWidth (M->Display, M->DefaultScreen);
	M->DC.Height = DisplayHeight (M->Display, M->DefaultScreen);
	
	M->DC.DefaultGCMask = 
		GCFunction | GCPlaneMask | GCBackground | GCForeground;
	
	XEvent ev;
	
	M->BackgroundWindow = XCreateSimpleWindow
	(
		M->Display, M->Root, 0, 0, M->DC.Width, M->DC.Height,
		0, M->DC.GCvalues.background, M->DC.GCvalues.background
	);

	XMapWindow(M->Display, M->BackgroundWindow);

	M->DC.gc = XCreateGC
		(M->Display, M->BackgroundWindow, M->DC.DefaultGCMask, &M->DC.GCvalues);	
	
	XFlush (M->Display);
	XSelectInput(M->Display, M->BackgroundWindow, M->DefaultEventMask);
	
	while (1)
	{
		XNextEvent(M->Display, &ev);
		
		if (ev.type == Expose) {
			XFillRectangle(M->Display, M->BackgroundWindow, M->DC.gc, 0, 0, 500, 500);
		}
		if (ev.type == KeyPress)
			break;
	}
}

static void
MoonlightDisplayTerminate (MoonlightProps *Moonlight)
{
	XFreeGC (Moonlight->Display, Moonlight->DC.gc);
}

int main(int argc, char **argv)
{
	MoonlightDisplayInit (&Moonlight);
	MoonlightDisplayTerminate (&Moonlight);
	XCloseDisplay (Moonlight.Display);

	return 0;
}
