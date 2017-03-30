/* Mama.h - public header for MandelSpawn master widget */
/* Copyright (C) 1990-1993 Andreas Gustafsson */

#ifndef _Mama_h
#define _Mama_h

#define XtNSpectrum 	"spectrum"
#define XtNColours	"colours"
#define XtNHues		"hues"
#define XtNBw		"bw"
#define XtNWrap		"wrap"
#define XtNDebug	"debug"
#define XtNAnimate	"animate"
#define XtNAnimationSpeed "animation_speed"

typedef struct _MamaRec *MamaWidget;
typedef struct _MamaClassRec *MamaWidgetClass;

extern WidgetClass mamaWidgetClass;

/* Methods */
void Shutdown();		/* shut down the whole operation */
unsigned MaxIterations();
void PopupAnother();
void SlaveStatistics();
unsigned MamaWidth();
unsigned MamaHeight();
unsigned long *MamaPixels();
unsigned long MamaWhite();
unsigned long MamaBlack();
struct wf_state *MamaWorkforce();
XVisualInfo *MamaVisualInfo();
Colormap MamaColormap();
void AnimationToggle();

#endif				/* _Mama_h */
