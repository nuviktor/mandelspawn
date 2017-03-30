/* xio.c -- glue between Xt and the I/O module */

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

#include <X11/Intrinsic.h>
#include "io.h"

extern XtAppContext thisApp;

/* Ugly state variables */

static XtInputId xio_input_id;
static int xio_recv_fd;

/* Callback function to be called when data arrives from a slave */

static void MsSocketInputCallback(client_data, source, id)
caddr_t client_data;
int *source;
XtInputId *id;
{
	io_state *io = (io_state *) client_data;
	if (*id != xio_input_id || *source != xio_recv_fd) {
		XtAppError(thisApp, "unexpected input from computation server");
	}
	io_handle_socket_input(io);
}

static void TimeoutCallback(client_data, id)
caddr_t client_data;
XtIntervalId *id;
{
	io_state *io = (io_state *) client_data;
	io_tick(io);
	/* reset the timeout */
	XtAppAddTimeOut(thisApp, 1000,
			(XtTimerCallbackProc) TimeoutCallback, client_data);
}

void xio_init(io)
io_state *io;
{
	xio_recv_fd = io_get_recv_fd(io);
	/* pass a pointer to the I/O state struct as "client data" */
	xio_input_id =
	    XtAppAddInput(thisApp,
			  xio_recv_fd,
			  (caddr_t) XtInputReadMask,
			  (XtInputCallbackProc) MsSocketInputCallback,
			  (caddr_t) io);
	(void)XtAppAddTimeOut(thisApp, 1000,
			      (XtTimerCallbackProc) TimeoutCallback,
			      (caddr_t) io);
}
