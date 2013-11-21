/*
 * Temporary config file
 */

// Masks

#define DEFAULTMASK		(EnterWindowMask | ExposureMask | FocusChangeMask | PropertyChangeMask | SubstructureNotifyMask | SubstructureNotifyMask)
#define BUTTONMASK		(ButtonPressMask | ButtonReleaseMask)
#define MOUSEMASK		(BUTTONMASK | PointerMotionMask)

//Colors

#define BACKGROUNDCOLOR "#333333"
#define BARCOLOR		"#999999"
#define WINHDRCOLOR		"#222222"

// Colorful debug output

#define CEVENT	"[\x1B[36mEVENT\x1B[0m]\t\t"
#define CMAP	"[\x1B[32mMAP\x1B[0m]\t\t"
#define CUNMAP	"[\x1B[31mUNMAP\x1B[0m]\t\t"
#define CDESTR	"[\x1B[31mDESTROY\x1B[0m]\t"
#define CREQU	"[\x1B[33mREQUEST\x1B[0m]\t"
#define CSUCCS	"\t\t  [\x1B[32m*\x1B[0m]"
#define CFAULT	"\t\t  [\x1B[31m*\x1B[0m]"
#define CACT	"[\x1B[34mACTION\x1B[0m]\t"

//Size parameters

#define BARHEIGHT		20
#define WINBARHEIGHT	20
#define WINBORDERSIZE	5

//Strings

#define TESTSTATUS		"Moonlight v0.1"
#define TESTPANEL		"Here is message of the day. Best news at the moment: I'm not daddy. Fucking amazing."
