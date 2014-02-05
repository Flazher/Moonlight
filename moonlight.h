#define XFT

#define Dpy				Moonlight.Display

/*
 * Temporary config file
 */

// Masks

#define DEFAULTMASK		(EnterWindowMask | ExposureMask | FocusChangeMask | PropertyChangeMask | SubstructureNotifyMask | SubstructureNotifyMask)
#define BUTTONMASK		(ButtonPressMask | ButtonReleaseMask)
#define MOUSEMASK		(BUTTONMASK | PointerMotionMask)

#define MODKEY			ShiftMask

//Colors

#define BACKGROUNDCOLOR "#999999"
#define BARCOLOR		"#333333"
#define WINBARCOLOR		"#222222"
#define BARTEXTCOLOR	"#FFFFFF"
#define MENUCOLOR		"#333333"
// Colorful debug output

#define CEVENT	"[\x1B[36mEVENT\x1B[0m]\t\t"
#define CMAP	"[\x1B[32mMAP\x1B[0m]\t\t"
#define CUNMAP	"[\x1B[31mUNMAP\x1B[0m]\t\t"
#define CDESTR	"[\x1B[31mDESTROY\x1B[0m]\t"
#define CREQU	"[\x1B[33mREQUEST\x1B[0m]\t"
#define CSUCCS	"\t\t  [\x1B[32m*\x1B[0m]"
#define CFAULT	"\t\t  [\x1B[31m*\x1B[0m]"
#define CACT	"[\x1B[34mACTION\x1B[0m]\t"

// Menu entries

typedef struct MenuEntry MenuEntry;

struct MenuEntry
{
	char *title;
	Window window;
	MenuEntry *Next;
};

//Size parameters

#define BARHEIGHT		30
#define WINBARHEIGHT	20
#define WINBORDERSIZE	3

#define MENUWIDTH		200
#define MENUHEIGHT		300
#define MENUENTRYHEIGHT	30
//Strings

#define TESTSTATUS		"Moonlight v0.1"
#define TESTPANEL		"Exec: "

#define EXITDIALOG		"Exit"

//Another settings

#define FOCUSONENTER	True
#define DEFAULTFONT	 	"-*-cantarell-*-*-*-*-12-*-*-*-*-*-iso8859-2"
