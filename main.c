#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <X11/Xlib.h>

struct MoonlightPropsStruct
{
	Display *Display;
	int DefaultScreen;
	Window Root;
	XWindowAttributes RootAttributes;
	Pixmap BackgroundPixmap;
};

typedef struct MoonlightPropsStruct MoonlightProps;
typedef struct MonitorStruct Monitor;

static void MoonlightDisplayInit (MoonlightProps *Moonlight);

MoonlightProps Moonlight;

static void MoonlightDisplayInit (MoonlightProps *Moonlight)
{
	if (!(Moonlight->Display = XOpenDisplay(NULL)))
	{
		printf ("Connection to X failed. Is $DISPLAY variable (%s) correct?",
			getenv("DISPLAY"));
		exit (1);
	}

	Moonlight->DefaultScreen = DefaultScreen(Moonlight->Display);
	Moonlight->Root = RootWindow(Moonlight->Display, Moonlight->DefaultScreen);
	if (!XGetWindowAttributes(Moonlight->Display, Moonlight->Root, &Moonlight->RootAttributes))
	{
		printf("Can't get default screen attributes. Quiting.\n");
		exit (1);
	}

	//Moonlight->BackgroundPixmap = XCreatePixmap(Moonlight->Display, Moonlight->Root, Moonlight->RootAttributes.width, Moonlight->RootAttributes.height, 0);
}


int main(int argc, char **argv)
{
	MoonlightDisplayInit (&Moonlight);
	
	XCloseDisplay (Moonlight.Display);

	return 0;
}
