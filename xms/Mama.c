/* Mama.c - MandelSpawn master widget */

/*
    This file is part of MandelSpawn, a network Mandelbrot program.

    Copyright (C) 1990-1993 Andreas Gustafsson

    MandelSpawn is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 1,
    as published by the Free Software Foundation.

    MandelSpawn is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License,
    version 1, along with this program; if not, write to the Free 
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
  The Mama widget is never realized.  Its purpose is to manage 
  resources common to all the Ms windows, such as the colormap and 
  the computation servers.  Now that the computation server interface 
  has been split off into a separate module, Mama has much less to do 
  than before. 
*/

/* On some systems, <fcntl.h> may need to be included also */
#include <stdio.h>
#include <errno.h>
extern int errno;		/* at least Sony's errno.h misses this */
#include <math.h>

#include <X11/IntrinsicP.h>
#include <X11/Xos.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include "backward.h"		/* X11R2 backward compatibility stuff */

#include "color.h"

#include "MamaP.h"
#include "Ms.h"			/* public header of the child widget */

#include "io.h"
#include "work.h"

/* default timeout in milliseconds per iteration, and constant factor */
#define TIMEOUT_PER_ITER 	40	/* this makes 10 s for 250 iters */
#define TIMEOUT_CONST		3000	/* add 3 seconds for network delays */

/*
  On the Mach/i386 machines I have tested, floor(0.26) returns -0.5.  Not 
  good.  Here is a nonportable workaround that's good enough for our 
  purposes. 
*/
#ifdef BROKEN_FLOOR
#define floor(x) ((double)(long)(x))
#endif

extern XtAppContext thisApp;
extern Display *myDisplay;
extern int myScreenNo;
extern Screen *myScreen;

static void Initialize(), Destroy();

static Boolean SetValues();

/* Defaults */

/* the width/height defaults are never really used */
static Dimension default_width = 400;
static Dimension default_height = 250;
static int default_colors = 250;
static int default_hues = 250;
static char default_spectrum[] =
    "blue-aquamarine-cyan-medium sea green-forest green-lime green-\
yellow green-yellow-coral-pink-black";
static Bool default_bw = False;
static Bool default_wrap = False;
static Bool default_animate = False;
static int default_animation_speed = 10;

static XtResource resources[] = {
	/* Core resources */
	{XtNwidth, XtCWidth, XtRDimension, sizeof(Dimension),
	 XtOffset(Widget, core.width), XtRDimension,
	 (caddr_t) & default_width}
	,
	{XtNheight, XtCHeight, XtRDimension, sizeof(Dimension),
	 XtOffset(Widget, core.height), XtRDimension,
	 (caddr_t) & default_height}
	,
	/* Noncore resources */
	{XtNSpectrum, XtCString, XtRString, sizeof(String),
	 XtOffset(MamaWidget, mama.spectrum), XtRString,
	 default_spectrum}
	,
	{XtNColours, XtCValue, XtRInt, sizeof(int),
	 XtOffset(MamaWidget, mama.n_colours), XtRInt,
	 (caddr_t) & default_colors},
	{XtNHues, XtCValue, XtRInt, sizeof(int),
	 XtOffset(MamaWidget, mama.n_hues), XtRInt,
	 (caddr_t) & default_hues},
	{XtNBw, XtCValue, XtRBool, sizeof(Bool),
	 XtOffset(MamaWidget, mama.bw), XtRBool,
	 (caddr_t) & default_bw}
	,
	{XtNWrap, XtCValue, XtRBool, sizeof(Bool),
	 XtOffset(MamaWidget, mama.wrap), XtRBool,
	 (caddr_t) & default_wrap}
	,
	{XtNAnimate, XtCValue, XtRBool, sizeof(Bool),
	 XtOffset(MamaWidget, mama.animation.on), XtRBool,
	 (caddr_t) & default_animate}
	,
	{XtNAnimationSpeed, XtCValue, XtRInt, sizeof(int),
	 XtOffset(MamaWidget, mama.animation.speed), XtRInt,
	 (caddr_t) & default_animation_speed}
};

MamaClassRec mamaClassRec = {	/* core fields */
	{
	 /* superclass               */ &widgetClassRec,
	 /* class_name               */ "Mama",
	 /* widget_size              */ sizeof(MamaRec),
	 /* class_initialize         */ NULL,
	 /* class_part_initialize    */ NULL,
	 /* class_inited             */ FALSE,
	 /* initialize               */ Initialize,
	 /* initialize_hook          */ NULL,
	 /* realize                  */ NULL,
	 /* actions                  */ NULL,
	 /* num_actions              */ 0,
	 /* resources                */ resources,
	 /* resource_count           */ XtNumber(resources),
	 /* xrm_class                */ NULL,
	 /* compress_motion          */ TRUE,
	 /* compress_exposure        */ TRUE,
	 /* compress_enterleave      */ TRUE,
	 /* visible_interest         */ FALSE,
	 /* destroy                  */ Destroy,
	 /* resize                   */ NULL,
	 /* expose                   */ NULL,
	 /* set_values               */ SetValues,
	 /* set_values_hook          */ NULL,
	 /* set_values_almost        */ XtInheritSetValuesAlmost,
	 /* get_values_hook          */ NULL,
	 /* accept_focus             */ NULL,
	 /* version                  */ XtVersion,
	 /* callback_private         */ NULL,
	 /* tm_table                 */ NULL,
	 /* query_geometry           */ NULL
	 }
	,
	/* noncore fields */
	{
	 /* dummy                    */ 0
	 }
};

WidgetClass mamaWidgetClass = (WidgetClass) & mamaClassRec;

/*
  Find or create a colormap for use with a PseudoColor visual
  and allocate the color cells we need.  May use the default
  colormap if appropriate.
*/

Colormap get_pseudocolor_colormap(visual, ncolours, pixels, black_ret,
				  white_ret)
Visual *visual;			/* visual this colorormap is for */
unsigned int ncolours;		/* number of colormap entries needed */
unsigned long *pixels;		/* pixel value array return */
unsigned long *black_ret;	/* black pixel value */
unsigned long *white_ret;	/* white pixel value */
{
	Colormap cmap;
	XColor dummy;
	XColor bwcells[2];
	unsigned long pixval;
	int i;

	/* pixel values reserved for white and black, or -1 if not used */
	unsigned long white_pixel = -1, black_pixel = -1;

	if (visual == DefaultVisualOfScreen(myScreen)) {
		/* Try to use the default colormap */

		XColor black, white, dummy;
		cmap = DefaultColormapOfScreen(myScreen);

		/*
		   Try to allocate read-only cells for black and white,
		   and read-write cells for the color pixels we need
		 */
		if (XAllocNamedColor(myDisplay, cmap,
				     "black", &black, &dummy) &&
		    XAllocNamedColor(myDisplay, cmap,
				     "white", &white, &dummy) &&
		    XAllocColorCells(myDisplay, cmap,
				     False, NULL, 0, pixels, ncolours)
		    ) {
			/* OK */
			*black_ret = black.pixel;
			*white_ret = white.pixel;
			return cmap;
		}
		/* Fallthrough if allocation out of the default colormap failed */
	}

	/* Try to allocate a colormap  of our own */
	cmap = XCreateColormap(myDisplay,
			       DefaultRootWindow(myDisplay), visual, AllocAll);
	if (!cmap)
		XtAppError(thisApp, "XCreateColormap failed");

	/*
	   Because the overflow label also use this colormap, 
	   there had better exist a white and a black pixel somewhere
	   in it.  Furthermore, on systems with only one hardware
	   colormap, the popup menu (which uses the default colormap)
	   may have to be displayed using this colormap.  To increase
	   the chances of the popup menu being readable in this case,
	   the white and black pixels of the default colormap should
	   also exist in the same locations in this colormap.
	 */

	if (visual->map_entries == DefaultVisualOfScreen(myScreen)->map_entries
	    && XAllocNamedColor(myDisplay, DefaultColormapOfScreen(myScreen),
				"white", &bwcells[0], &dummy)
	    && XAllocNamedColor(myDisplay, DefaultColormapOfScreen(myScreen),
				"black", &bwcells[1], &dummy)) {
		bwcells[0].flags = DoRed | DoGreen | DoBlue;
		bwcells[1].flags = DoRed | DoGreen | DoBlue;
		XStoreColors(myDisplay, cmap, bwcells, 2);
		white_pixel = bwcells[0].pixel;
		black_pixel = bwcells[1].pixel;
	}

	pixval = 0;
	for (i = 0; i < ncolours; i++) {
		while (pixval == white_pixel || pixval == black_pixel)
			pixval++;
		pixels[i] = pixval;
		pixval++;
	}

	if (pixval > visual->map_entries) {
		XtAppError(thisApp,
			   "too many colours for this screen.  Please specify a smaller number\n\
of colours with the -colours option, or use -bw to run in black and\n\
white.");
	}

	*black_ret = black_pixel;
	*white_ret = white_pixel;
	return (cmap);
}

cmap_error(msg)
char *msg;
{
	XtAppError(thisApp, msg);
}

int name_to_color(name, color)
char *name;
color_t *color;
{
	XColor xcolor, dummy;
	if (!XLookupColor(myDisplay, DefaultColormapOfScreen(myScreen), name,
			  &xcolor, &dummy))
		return 0;
	color->comps[RED_COMP] = (double)xcolor.red / 0xFFFF;
	color->comps[GREEN_COMP] = (double)xcolor.green / 0xFFFF;
	color->comps[BLUE_COMP] = (double)xcolor.blue / 0xFFFF;
	return 1;
}

/* Set up the colormap */

static void MsColourSetup(w, mode)
MamaWidget w;
int mode;			/* PseudoColor or TrueColor */
{
	unsigned int i;
	unsigned int ncolours;
	unsigned int nhues;
	unsigned int nstops;
	unsigned long *phpixels;
	unsigned long *pixels;
	color_t *physcolors;
	XColor *colourset;
	unsigned stops_allocated = 4;	/* initial allocation of colour stops */
	char *p;

	ncolours = w->mama.n_colours;
	nhues = w->mama.n_hues;

	/* parse the "spectrum" resource */

	physcolors = (color_t *) XtMalloc(ncolours * sizeof(color_t));
	parse_spectrum(w->mama.spectrum, ncolours, physcolors);

	/*
	   Physical pixel mapping (from colour number to pixel value, 
	   "ncolours" entries); these are simply the pixels that 
	   XAllocColorCells gives us 
	 */
	phpixels = (unsigned long *)XtMalloc(ncolours * sizeof(unsigned long));

	/*
	   Logical pixel mapping (from iteration count to pixel value,
	   "nhues" entries)
	 */
	pixels = (unsigned long *)XtMalloc(nhues * sizeof(unsigned long));

	/* Allocate an array representing the RGB-to-pixel mapping */
	colourset = (XColor *) XtMalloc(ncolours * sizeof(XColor));

	/* Find or create a suitable X colormap */

	switch (mode) {
	case PseudoColor:
		/* Allocate a writeable colormap */
		w->mama.my_colormap =
		    get_pseudocolor_colormap(w->mama.visual_info.visual,
					     ncolours, phpixels, &w->mama.black,
					     &w->mama.white);
		for (i = 0; i < ncolours; i++)
			colourset[i].pixel = phpixels[i];
		break;
	case TrueColor:
		w->mama.my_colormap =
		    XCreateColormap(myDisplay, RootWindowOfScreen(myScreen),
				    w->mama.visual_info.visual, AllocNone);
		/*
		   Allocate read-only white and black pixels for the
		   overflow label
		 */
		{
			XColor black, white, dummy;

			(void)XAllocNamedColor(myDisplay, w->mama.my_colormap,
					       "black", &black, &dummy);
			(void)XAllocNamedColor(myDisplay, w->mama.my_colormap,
					       "white", &white, &dummy);
			w->mama.black = black.pixel;
			w->mama.white = white.pixel;
		}
	}

	for (i = 0; i < ncolours; i++) {
		colourset[i].red =
		    (int)(0xFFFF * physcolors[i].comps[RED_COMP] + 0.5);
		colourset[i].green =
		    (int)(0xFFFF * physcolors[i].comps[GREEN_COMP] + 0.5);
		colourset[i].blue =
		    (int)(0xFFFF * physcolors[i].comps[BLUE_COMP] + 0.5);
		colourset[i].flags = DoRed | DoGreen | DoBlue;

		if (mode == TrueColor) {
			/*
			   Allocate a read-only colour (this is very slow -
			   one server round-trip per pixel!)
			 */
			XAllocColor(myDisplay, w->mama.my_colormap,
				    &colourset[i]);
			phpixels[i] = colourset[i].pixel;
		}
	}

	if (w->mama.wrap) {	/*
				   Wrap the spectrum around several times if needed to get the 
				   desired number of "hues" from a limited number of "colours"
				 */
		for (i = 0; i < nhues - 1; i++) {
			pixels[i] = phpixels[i % ncolours];
		}
	} else {		/*
				   Don't wrap; give the same colour to several consecutive iteration
				   values
				 */
		int divfactor = (nhues + ncolours - 1) / ncolours;
		for (i = 0; i < nhues - 1; i++) {
			pixels[i] = phpixels[i / divfactor];
		}
	}
	/* ...but make sure the last colour is what the user specified. */
	pixels[nhues - 1] = phpixels[ncolours - 1];
	w->mama.pixels = pixels;

	if (mode == PseudoColor)
		XStoreColors(myDisplay, w->mama.my_colormap,
			     colourset, ncolours);

	/* save a shadow of the colormap for colormap animation */
	w->mama.colourset = colourset;

	XtFree((char *)physcolors);
	XtFree((char *)phpixels);
}

/* Various colormap animation stuff */

static void AnimationStep(w)
MamaWidget w;
{
	int ncolours = w->mama.n_colours;
	XColor *colourset = w->mama.colourset;
	unsigned long tmp;
	int i;
	tmp = colourset[ncolours - 1].pixel;
	for (i = ncolours - 2; i >= 0; i--)
		colourset[i + 1].pixel = colourset[i].pixel;
	colourset[0].pixel = tmp;
	XStoreColors(myDisplay, w->mama.my_colormap, colourset, ncolours);
	XSync(myDisplay, False);
}

void AddAnimationTimeout();

static void AnimationTimeoutCallback(client_data, id)
caddr_t client_data;
XtIntervalId *id;
{
	MamaWidget w = (MamaWidget) client_data;
	AnimationStep(w);
	if (++w->mama.animation.phase == w->mama.n_colours)
		w->mama.animation.phase = 0;

	if (w->mama.animation.on == ANIM_STOPPING)
		w->mama.animation.on = ANIM_OFF;
	else
		AddAnimationTimeout(w);
}

void AddAnimationTimeout(w)
MamaWidget w;
{
	int speed = w->mama.animation.speed;
	if (speed == 0)
		speed = 1;	/* to avoid division by zero */
	w->mama.animation.timeout_id =
	    XtAppAddTimeOut(thisApp, 1000 / speed,
			    (XtTimerCallbackProc) AnimationTimeoutCallback,
			    (caddr_t) w);
}

void RemoveAnimationTimeout(w)
MamaWidget w;
{
	XtRemoveTimeOut(w->mama.animation.timeout_id);
}

void AnimationToggle(w)
MamaWidget w;
{
	switch (w->mama.animation.on) {
	case ANIM_OFF:
		AddAnimationTimeout(w);
		w->mama.animation.on = ANIM_ON;
		break;
	case ANIM_ON:
	case ANIM_STOPPING:
		w->mama.animation.on = ANIM_STOPPING;
		break;
	}
}

/* Set up for black-and-white operation */

static void MsBwSetup(w)
MamaWidget w;
{
	unsigned long *pixels;

	unsigned niter = w->mama.n_hues;
	register int i;

	pixels = w->mama.pixels =
	    (unsigned long *)XtMalloc(niter * sizeof(unsigned long));

	/* Use alternating black/white bands */
	for (i = 0; i < niter; i++) {
		pixels[i] = (niter - i) & 0x01 ?
		    BlackPixelOfScreen(myScreen) : WhitePixelOfScreen(myScreen);
	}
	w->mama.my_colormap = DefaultColormapOfScreen(myScreen);
}

/* Initialize the widget */

static void Initialize(request, new)
MamaWidget request;
MamaWidget new;
{
	int planes;

	/* we haven't made any popups yet */
	new->mama.n_popups_created = 0;

	/* initialize the workforce */
	new->mama.workforce =
	    wf_init(new->mama.n_hues * TIMEOUT_PER_ITER + TIMEOUT_CONST,
		    IO_MUX_XT, IO_MUX_XT);

	/* initialize the Xt-based I/O multiplexing mechanism */
	xio_init(wf_get_io(new->mama.workforce));

#ifndef R4
	/* If you get "operation would block" errors, remove the #ifdef and */
	/* #ifndef around this code, and send me (gson@niksula.hut.fi) */
	/* a bug report mentioning your Xt library vendor and version. */
	/* Does anyone have a clue why lots of people used to get these errors */
	/* with X11R3? */

	io_ignore_ewouldblock(wf_get_io(new->mama.workforce));
#endif

	if (new->mama.n_colours > new->mama.n_hues) {	/* we have more colours than iterations, throw some away */
		new->mama.n_colours = new->mama.n_hues;
	}

	if (new->mama.bw)
		goto force_bw;

	/* look for a suitable visual type */

	/* first, look for 24-bit TrueColor visuals */
	if (XMatchVisualInfo(myDisplay, myScreenNo, 24, TrueColor, &new->mama.visual_info)) {	/* Wow, a 24-bit display.  Let's use it. */
		MsColourSetup(new, TrueColor);
		return;
	}

	/* next, look for PseudoColor visuals of 8, 4, or 2 bits */
	for (planes = 8; planes >= 2; planes >>= 1)
		if (XMatchVisualInfo(myDisplay, myScreenNo, planes, PseudoColor, &new->mama.visual_info)) {	/* found a suitable PseudoColor visual; set up for flashy */
			/* colour pictures */
			if (new->mama.n_colours > (1 << planes)) {
				char msg[256];
				/* leave a few colours for window borders, etc. */
				int use_colours = (1 << planes) - 4;
				sprintf(msg, "%d colours is too much for a %d-plane visual,\n\
  trying %d colours...\n", new->mama.n_colours, planes,
					use_colours);
				XtAppWarning(thisApp, msg);
				new->mama.n_colours = use_colours;
			}
			MsColourSetup(new, PseudoColor);
			return;
		}

 force_bw:
	/* no suitable TrueColor or PseudoColor visual found, try B/W. */
	/* We simply assume that the default visual can do B/W. */
	{
		XVisualInfo template, *ret;
		int nitems;
		template.visualid =
		    XVisualIDFromVisual(DefaultVisualOfScreen(myScreen));
		ret =
		    XGetVisualInfo(myDisplay, VisualIDMask, &template, &nitems);
		if (nitems != 1)
			XtAppError(thisApp,
				   "How many default visuals are there, exactly?");
		new->mama.visual_info = ret[0];
		MsBwSetup(new);
		return;
	}
}

/*
  Provide error handling services to the workforce and
  I/O modules
*/

void wf_error(msg)
char *msg;
{
	XtAppError(thisApp, msg);
}

void wf_warn(msg)
char *msg;
{
	XtAppWarning(thisApp, msg);
}

void io_error(msg)
char *msg;
{
	XtAppError(thisApp, msg);
}

void io_warn(msg)
char *msg;
{
	XtAppWarning(thisApp, msg);
}

/* Update widget resources (no changeable resources so far) */

static Boolean SetValues(current, request, new)
MamaWidget current, request, new;
{
	return (False);
}

/* Die */

void Shutdown(w)
MamaWidget w;
{
	XCloseDisplay(XtDisplay(w));
	exit(0);
}

/* This callback is called whenever a popup child dies */

static void PostMortem(w, client_data, call_data)
MsWidget w;
caddr_t client_data;
caddr_t call_data;
{
	MamaWidget ma = (MamaWidget) client_data;
	/* If our last child is dying we have nothing to live for; */
	/* commit suicide */
	if (ma->core.num_popups == 1)
		Shutdown(ma);
}

/*
  Pop up a new Ms window, making it a child of the Mama. 
  The caller must make sure there is room in the "shell_args" array 
  for one more arg.
*/

void PopupAnother(w, shell_args, num_shell_args, args, num_args)
MamaWidget w;
ArgList shell_args;
Cardinal num_shell_args;
ArgList args;
Cardinal num_args;
{
	Widget the_widget;
	Widget the_shell;
	char popup_name[32];	/* unique name for each popup widget */

	sprintf(popup_name, "ms_%d", ++w->mama.n_popups_created);

	/* make the popup use our colormap */
	XtSetArg(shell_args[num_shell_args], XtNcolormap, w->mama.my_colormap);
	num_shell_args++;

	/* XtCreateWindow() needs _both_ a visual and a depth.  Stupid. */
	XtSetArg(shell_args[num_shell_args], XtNdepth,
		 w->mama.visual_info.depth);
	num_shell_args++;
	XtSetArg(shell_args[num_shell_args], XtNvisual,
		 w->mama.visual_info.visual);
	num_shell_args++;

	/* this is needed for OpenWindows... */
	XtSetArg(shell_args[num_shell_args], XtNinput, True);
	num_shell_args++;

	the_shell =
	    XtCreatePopupShell(popup_name, topLevelShellWidgetClass, (Widget) w,
			       shell_args, num_shell_args);

	XtSetArg(args[num_args], XtNcolormap, w->mama.my_colormap);
	num_args++;
	XtSetArg(args[num_args], XtNdepth, w->mama.visual_info.depth);
	num_args++;
	XtSetArg(args[num_args], XtNvisual, w->mama.visual_info.visual);
	num_args++;

	/*
	   We can't just inherit the fg and bg pixel values either because 
	   they might refer to the wrong visual 
	 */
	XtSetArg(args[num_args], XtNbackground, w->mama.white);
	num_args++;
	XtSetArg(args[num_args], XtNforeground, w->mama.black);
	num_args++;

	the_widget = XtCreateManagedWidget("view",
					   (WidgetClass) msWidgetClass,
					   (Widget) the_shell, args, num_args);
	XtAddCallback(the_widget, XtNdestroyCallback,
		      (XtCallbackProc) PostMortem, (caddr_t) w);

	XtPopup(the_shell, XtGrabNone);
}

/* Destroy the widget */

static void Destroy(w)
MamaWidget w;
{
}

/*
  The Ms widget needs to know the maximums message size,
  how may colours there are, etc.  Make that information
  available through public functions. 
*/

unsigned MaxIterations(w)
MamaWidget w;
{
	return (w->mama.n_hues);
}

unsigned MamaHeight(w)
MamaWidget w;
{
	return (w->core.height);
}

unsigned MamaWidth(w)
MamaWidget w;
{
	return (w->core.width);
}

Colormap MamaColormap(w)
MamaWidget w;
{
	return (w->mama.my_colormap);
}

XColor *MamaColourset(w)
MamaWidget w;
{
	return (w->mama.colourset);
}

wf_state *MamaWorkforce(w)
MamaWidget w;
{
	return (w->mama.workforce);
}

XVisualInfo *MamaVisualInfo(w)
MamaWidget w;
{
	return (&w->mama.visual_info);
}

unsigned long *MamaPixels(w)
MamaWidget w;
{
	return (w->mama.pixels);
}

/*
  Get the pixel values for the white and black pixels of
  our colormap.  These are needed by the label widget creation
  code to get correct colors when using a nondefault
  visual.
*/

unsigned long MamaWhite(w)
MamaWidget w;
{
	return w->mama.white;
}

unsigned long MamaBlack(w)
MamaWidget w;
{
	return w->mama.black;
}

/* Print some performance statistics about the slaves */

void SlaveStatistics(w)
MamaWidget w;
{
	wf_print_stats(w->mama.workforce, stdout);
}
