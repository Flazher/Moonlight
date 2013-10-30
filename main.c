#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

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
	DrawingContext DC;
	unsigned long DefaultEventMask;
} MoonlightProps;

void MoonlightDisplayInit ();
static void MoonlightDisplayTerminate ();
unsigned long GetColor (const char *colorhex);

MoonlightProps Moonlight;

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
	Moonlight.DefaultEventMask = ExposureMask | KeyPressMask;

	Moonlight.DC.GCvalues.function   = GXxor; 
	Moonlight.DC.GCvalues.background = GetColor("#333333");
	Moonlight.DC.GCvalues.foreground = GetColor("#333333");
	Moonlight.DC.GCvalues.plane_mask = AllPlanes;
	Moonlight.DC.Width = DisplayWidth (Moonlight.Display, Moonlight.DefaultScreen);
	Moonlight.DC.Height = DisplayHeight (Moonlight.Display, Moonlight.DefaultScreen);
	Moonlight.DC.X = 0;
	Moonlight.DC.Y = 0;
	
	Moonlight.DC.DefaultGCMask = 
		GCFunction | GCPlaneMask | GCBackground | GCForeground;
	
	XEvent ev;
	
	Moonlight.DC.gc = XCreateGC
		(Moonlight.Display, Moonlight.Root, Moonlight.DC.DefaultGCMask, &Moonlight.DC.GCvalues);	
	
	Moonlight.DC.drawable = XCreatePixmap (Moonlight.Display, Moonlight.Root, Moonlight.DC.Width, Moonlight.DC.Height, DefaultDepth(Moonlight.Display, Moonlight.DefaultScreen));

	/*
	 * What is this dummy rectangle for?
	 *
	 * I don't want to separate solid wallpaper/image background code
	 * So, here is Moonlight.DC.drawable pixmap filled by one big rectangle in cause of
	 * wallpaper is solid.
	 *
	 * I'll fix it later. Maybe. If I'll become a bit smarter.
	 */

	XFillRectangle (Moonlight.Display, Moonlight.DC.drawable, Moonlight.DC.gc, Moonlight.DC.X, Moonlight.DC.Y, Moonlight.DC.Width, Moonlight.DC.Height);
	
	XSetWindowBackgroundPixmap(Moonlight.Display, Moonlight.Root, Moonlight.DC.drawable);
	XMapWindow(Moonlight.Display, Moonlight.Root);
	XClearWindow (Moonlight.Display, Moonlight.Root);
	XFlush (Moonlight.Display);
	XSelectInput(Moonlight.Display, Moonlight.Root, Moonlight.DefaultEventMask);
	
	while (1)
	{
		XNextEvent(Moonlight.Display, &ev);
		
		if (ev.type == Expose) {
			// Here is my expose handler lolololol	
		}
		if (ev.type == KeyPress)
			break;
	}
}

static void
MoonlightDisplayTerminate ()
{
	XFreeGC (Moonlight.Display, Moonlight.DC.gc);
	XFreePixmap (Moonlight.Display, Moonlight.DC.drawable);
}

int main(int argc, char **argv)
{
	MoonlightDisplayInit ();
	MoonlightDisplayTerminate ();
	XCloseDisplay (Moonlight.Display);

	return 0;
}

unsigned long GetColor (const char *colorhex)
{
	Colormap CMap = DefaultColormap (Moonlight.Display, Moonlight.DefaultScreen);
	XColor Color;
	if (!XAllocNamedColor (Moonlight.Display, CMap, colorhex, &Color, &Color))
	{
		printf("Cannot allocate color %s\n", colorhex);
		exit(1);
	}
	return Color.pixel;
}
