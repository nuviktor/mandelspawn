/* MamaP.h - private header for MandelSpawn master widget */
/* Copyright (C) 1990-1993 Andreas Gustafsson */

#ifndef _MamaP_h
#define _MamaP_h

#include "Ms.h"			/* public header of child widget */
#include "Mama.h"

/* widget instance structure */
typedef struct {
	struct wf_state *workforce;	/* X-independent slave handling stuff */
	unsigned n_hues;	/* max. iterations */
	unsigned n_colours;	/* number of colours */
	char *spectrum;		/* text definitions of our colours */
	unsigned long *pixels;	/* pixel values for our colours */
	unsigned n_popups_created;	/* number of popups created (not existing) */
	Bool bw;		/* force black-and-white operation */
	Colormap my_colormap;	/* may be the default colormap */
	Bool wrap;		/* wrap colourmap when too few colours */
	Bool debug;		/* debugging mode */
	XVisualInfo visual_info;	/* the visual we are using */
	XColor *colourset;	/* the colours in the colormap */
	struct {
		int on;		/* animation on/off flag */
		int speed;	/* animation speed (in colours/second) */
		int phase;	/* current phase in animation cycle */
		XtIntervalId timeout_id;	/* timeout between updates */
	} animation;
	unsigned long white;
	unsigned long black;
} MamaPart;

#define ANIM_OFF 0
#define ANIM_ON 1
#define ANIM_STOPPING 2

typedef struct _MamaRec {
	CorePart core;
	MamaPart mama;
} MamaRec;

/* Widget class structure */
typedef struct {
	int dummy;
} MamaClassPart;

typedef struct _MsClassRec {
	CoreClassPart core_class;
	MamaClassPart mama;
} MamaClassRec;

extern MamaClassRec mamaClassRec;
#endif				/* _MamaP_h */
