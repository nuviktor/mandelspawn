/* io.c -- low-level client/server communications and I/O multiplexing */

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

#include "inet.h"
#include "io.h"

#include <errno.h>
extern int errno;		/* at least Sony's <errno.h> misses this */

struct io_state {
	io_transport trans;	/* transport mechanism */
	io_multiplex mux;	/* I/O multiplexing and timeout mechanism */
	int recv_fd;
	int send_fd;
	char *rcvbuf;
	int bufsize;
	char *closure;
	void (*recv_fun) ();
	void (*tick_fun) ();
	int child_pid;		/* PID of child server, if any */
	int done;
	int ignore_ewouldblock;
};

/* run the tick function */

void io_tick(io)
io_state *io;
{
	(*(io->tick_fun)) (io->closure);
}

/* initialize the I/O module */

/*
  When a message is received, call recv_fun with the closure,
  message buffer, and message size as arguments.   When a timeout
  occurs, call tick_fun with the closure as the only argument. 
*/

io_state *io_init(trans, mux, recv_fd, send_fd, closure, rcvbuf, bufsize,
		  recv_fun, tick_fun)
io_transport trans;		/* transport mechanism */
io_multiplex mux;		/* I/O multiplexing and timeout mechanism */
int recv_fd, send_fd;
char *closure;
char *rcvbuf;			/* buffer to put received messages in */
int bufsize;			/* size of the above */
void (*recv_fun) ();		/* function to call on reception of a message */
void (*tick_fun) ();		/* funtion to call on timeout */
{
	io_state *io = (io_state *) malloc(sizeof(io_state));
	io->trans = trans;
	io->mux = mux;
	io->recv_fd = recv_fd;
	io->send_fd = send_fd;
	io->rcvbuf = rcvbuf;
	io->bufsize = bufsize;
	io->closure = closure;
	io->recv_fun = recv_fun;
	io->tick_fun = tick_fun;
	io->done = 0;
	io->ignore_ewouldblock = 0;
	return (io);
}

void io_deinit(io)
io_state *io;
/*ARGSUSED*/
{				/* pure laziness */
}

int io_get_recv_fd(io)
io_state *io;
{
	return io->recv_fd;
}

void io_ignore_ewouldblock(io)
io_state *io;
{
	io->ignore_ewouldblock = 1;
}

int io_send(io, buffer, bufsize, to, tolen)
io_state *io;
char *buffer;
int bufsize;
char *to;			/* this may be a "struct sockaddr *" or NULL */
unsigned int tolen;
{
	int status;
	switch (io->trans) {
#ifdef HAVE_SOCKETS
	case IO_TRANS_UDP:
		status = sendto(io->send_fd, buffer, bufsize, 0,
				(struct sockaddr *)to, tolen);
		break;
#endif
	case IO_TRANS_PIPE:
		status = write(io->send_fd, buffer, bufsize);
		break;
	default:
		io_error("transport confusion");
		break;
	}
	return (status);
}

void io_done(io)
io_state *io;
{
	io->done = 1;
}

#ifdef HAVE_SOCKETS
void io_handle_socket_input(io)
io_state *io;
{
	int fromlen = sizeof(NET_ADDRESS);
	/*
	   This previously used read(), but some non-BSD TCP/IP implementations
	   don't allow it to be used with connectionless sockets.  Also,
	   while the Sun implementation does allow for a null pointer
	   for the "from" argument in recvfrom(), the "fromlen" argument 
	   may not be a null pointer. 
	 */
	if (recvfrom(io->recv_fd, io->rcvbuf, io->bufsize,
		     0, (struct sockaddr *)0, &fromlen) == -1)
	{
		if (errno != EWOULDBLOCK || !io->ignore_ewouldblock) {
			perror("read");
			io_error("slave input socket read");
		}
	} else {
		(*(io->recv_fun)) (io->closure, io->rcvbuf, io->bufsize);
	}
}
#endif				/* HAVE_SOCKETS */

#ifdef HAVE_SELECT
void io_main_select(io)
io_state *io;
{
	fd_set readfds;
	struct timeval tv, zero_tv;
	zero_tv.tv_sec = 0;
	zero_tv.tv_usec = 0;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	/* main loop */
	while (!io->done) {
		int nready;

		/*
		   Keep polling as long as there is data; it's no use bothering 
		   with timeouts if we're fully occupied handling incoming data.
		 */
		do {
 redo_1:
			FD_ZERO(&readfds);
			FD_SET(io->recv_fd, &readfds);
			nready =
			    select(io->recv_fd + 1, &readfds, (fd_set *) 0,
				   (fd_set *) 0, &zero_tv);
			if (nready == -1) {
				if (errno == EINTR)
					goto redo_1;
				io_error("select");
			} else if (nready > 0
				   && FD_ISSET(io->recv_fd, &readfds))
				io_handle_socket_input(io);
		} while (nready);

		/*
		   No more data; handle any pending timeouts and then go to sleep for
		   a second or until new data arrive.
		 */
		io_tick(io);

 redo_2:
		FD_ZERO(&readfds);
		FD_SET(io->recv_fd, &readfds);
		nready =
		    select(io->recv_fd + 1, &readfds, (fd_set *) 0,
			   (fd_set *) 0, &tv);
		if (nready == -1) {
			if (errno == EINTR)
				goto redo_2;
			io_error("select");
		} else if (nready > 0 && FD_ISSET(io->recv_fd, &readfds))
			io_handle_socket_input(io);
	}
}
#endif				/* HAVE_SELECT */

void io_main_nomux(io)
io_state *io;
{
	while (!io->done) {
		read(io->recv_fd, io->rcvbuf, io->bufsize);
		(*(io->recv_fun)) (io->closure, io->rcvbuf, io->bufsize);
	}
}

/*
  The main poll routine.  This is for the non-X version only; in the X
  version, this functionality is supplied by the main event loop. 
*/

void io_main(io)
io_state *io;
{
	switch (io->mux) {
#ifdef HAVE_SELECT
	case IO_MUX_SELECT:
		io_main_select(io);
		break;
#endif
	case IO_MUX_NONE:
		io_main_nomux(io);
		break;
	case IO_MUX_XT:
	default:
		io_error("mux confusion");
		break;
	}
}
