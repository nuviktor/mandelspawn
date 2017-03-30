/* label.c - report underflow using an Athena Label widget */

/*
  (This has not been tested with X11R3 so it is currently disabled
  on X11R3 and older.  It might work anyway.) 
*/

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

#include <stdio.h>

#include <X11/IntrinsicP.h>
#include <X11/Xos.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/Label.h>

#include <X11/Shell.h>		/* needed for XtNvisual */

#include "MsP.h"		/* this is really part of the Ms widget */
#include "Mama.h"

extern Screen *myScreen;

char underflow_msg[] = "Lost precision\n\nZoom out or reduce window size\n\
to regain picture";

void MsCreateUnderflowLabel(w)
MsWidget w;
{
	Arg arglist[10];
	int num_args = 0;

	XtSetArg(arglist[num_args], XtNlabel, underflow_msg);
	num_args++;
	XtSetArg(arglist[num_args], XtNx, 10);
	num_args++;
	XtSetArg(arglist[num_args], XtNy, 10);
	num_args++;

	XtSetArg(arglist[num_args], XtNbackground, MamaWhite(w->ms.mama));
	num_args++;
	XtSetArg(arglist[num_args], XtNforeground, MamaBlack(w->ms.mama));
	num_args++;

	w->ms.underflow_label =
	    XtCreateManagedWidget(underflow_msg, labelWidgetClass,
				  (Widget) w, arglist, num_args);
}
