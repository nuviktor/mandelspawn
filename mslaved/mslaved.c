/* mslaved.c - MandelSpawn computation server deamon */

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
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

#include "datarep.h"
#include "ms_real.h"
#include "ms_ipc.h"
#include "ms_job.h"
#include "ms_real.c"

/*
  Note that the timeout default below is overridden by mslavedc so that 
  manually started servers will persist throughout a typical session. 
*/
#define DEFAULT_SLAVE_TIMEOUT (60)	/* by default time out in 60 seconds */
#define DEFAULT_NICE (10)	/* use nice 10 by default */

char *me;			/* name of program */
int timeout = DEFAULT_SLAVE_TIMEOUT;	/* timeout */
int niceval = DEFAULT_NICE;	/* nice value */
int use_sockets = 1;

/* Log an error message and exit */
error(s)
char *s;
{
#ifdef HAVE_SYSLOG
	syslog(LOG_ERR, "%s: %s (%m)", me, s);
#endif
	exit(1);
}

#ifdef HAVE_SOCKETS

#define RECV(fd, data, len, opts, name, namelen) \
  (use_sockets ? \
     recvfrom(fd, data, len, opts, name, namelen) : \
     read(fd, data, len))

#define SEND(fd, data, len, opts, name, namelen) \
  (use_sockets ? \
     sendto(fd, data, len, opts, name, namelen) : \
     write(fd, data, len))

#else				/* !HAVE_SOCKETS */

#define RECV(fd, data, len, opts, name, namelen) \
  read(fd, data, len)

#define SEND(fd, data, len, opts, name, namelen) \
  write(fd, data, len)

#endif				/* !HAVE_SOCKETS */

/* Decode a single parameter */

real decode_parm(p)
char **p;
{
	real r = net_to_real(ntohli(*((netreal *) * p)));
	*p += sizeof(netreal);
	return r;
}

unsigned int mandelbrot(parms, maxiter, flags)
real *parms;
unsigned int maxiter;
int flags;
{
	register real x_re, x_im;
	register real c_re, c_im;
	register real xresq, ximsq;
	complex c0;
	complex z0;
	real old_re = zero_real(), old_im = zero_real();
	int show_interior;
	unsigned int count;

	show_interior = ! !(flags & MS_OPT_INTERIOR);

	c0.re = parms[0];
	c0.im = parms[1];
	z0.re = parms[2];
	z0.im = parms[3];

	c_re = c0.re;
	c_im = c0.im;
	x_re = z0.re;
	x_im = z0.im;

	/* The following loop is where the Real Work gets done. */
	count = 0;
	while (count < maxiter - 1) {
		/*
		   The following if statement implements limit cycle detection
		   to speed up calculation of areas inside the Mandelbrot set. 
		   Unfortunately, it also slows down the calculation of other areas,
		   to the degree that it probably pays off only when doing shallow
		   zooms with large black areas on machines with slow multiply
		   instructions.  Therefore it is now disabled by default.
		 */

#ifdef CYCLE_DETECT
		if ((count & (count - 1)) == 0) {	/* "count" is zero or a power of two; save the current position */
			old_re = x_re;
			old_im = x_im;
		} else {
			/*
			   Check if we have returned to a previously saved position; 
			   if so, the iteration has converged to a limit cycle => we 
			   are inside the Mandlebrot set and need iterate no further.
			 */
			if (x_re == old_re && x_im == old_im) {
				if (!show_interior)
					count = maxiter - 1;
				break;
			}
		}
#endif
		/* 
		   This is the familiar "z := z^2 + c; abort if |z| > 2"
		   Mandelbrot iteration, with the arithmetic operators hidden
		   in macros so that the same code can be compiled for either
		   fixed-point or floating-point arithmetic.
		   The macros (mul_real(), etc.) are defined in ms_real.h. 
		 */
		xresq = mul_real(x_re, x_re);
		ximsq = mul_real(x_im, x_im);
		if (gteq_real(add_real(xresq, ximsq), four_real()))
			break;
		x_im = add_real(twice_mul_real(x_re, x_im), c_im);
		x_re = add_real(sub_real(xresq, ximsq), c_re);
		count++;
	}
	return count;
}

/* 
  An alternative fractal.  This code is turned of for now because the
  protocol doesn't yet support multiple fractal types.  
*/

#ifdef HENON
unsigned int henon(parms, maxiter, flags)
real *parms;
unsigned int maxiter;
int flags;
{
	register real c0, c1;	/* coefficients */
	register real x0, x1, x2;	/* states */

	unsigned int count;

	c0 = parms[0];
	c1 = parms[1];
	x1 = parms[2];
	x2 = parms[3];

	count = 0;
	while (count < maxiter - 1) {
		x0 = add_real(one_real(),
			      add_real(mul_real(c0, x2),
				       mul_real(c1, mul_real(x1, x1))));
		x2 = x1;
		x1 = x0;

		if (x0 > int_to_real(10) || x0 < int_to_real(-10))
			break;

		count++;
	}
	return count;
}
#endif				/* HENON */

#define MAX_PARMS 32

/*
  Do the actual calculation and encode the results in an reply message;
  return the size of the message (in bytes) 
*/

unsigned int calculate(in, out)
Message *in, *out;
{
	int julia;		/* true if calculating a Julia set */
	int show_interior;	/* true if displaying interior structure */
	complex delta;

	int xc, yc, xmin, xmax, xsize, ymin, ymax, ysize;
	unsigned int maxiter;
	unsigned long mi_count = 0;
	unsigned int datasize;
	int flags;
	ms_job *job;
	char *p;		/* untyped pointer to parameter block in message */
	int x_parm_no, y_parm_no;	/* indices of parameters to vary with x/y coord */
	real *varx, *vary;	/* pointers to same */
	real initial_varx;
	real parm_buf[MAX_PARMS];
	int n_parms;
	int i;

#define parms parm_buf

	job = (ms_job *) in->whip.data;

	/* check that the format is supported */
	if (ntohs(in->whip.header.format) != DATA_FORMAT)
		return (0);

	/* convert values in the message to host byte order and precalculate some */
	/* useful values */
	xmin = ntohs(job->s.x);
	xsize = ntohs(job->s.width);
	xmax = xmin + xsize;
	ymin = ntohs(job->s.y);
	ysize = ntohs(job->s.height);
	ymax = ymin + ysize;

	maxiter = ntohli(job->j.iteration_limit);

	datasize = (maxiter > 256) ?
	    ((char *)(&(out->reply.data.shorts[xsize * ysize]))
	     - (char *)(&(out->reply))) :
	    ((char *)(&(out->reply.data.chars[xsize * ysize]))
	     - (char *)(&(out->reply)));

	/*
	   Perform a simple sanity check to avoid getting into semi-infinite 
	   loops because of malicious or erroneous messages.
	 */
	if (datasize > MAX_DATAGRAM)
		error("data too large");

	/* 
	   Make sure the iteration counts can be represented in the result
	   packet; 65536 iterations can still take a long time and if we
	   keep calculating the same packet for too long the timeout alarm will
	   get us.  Those who need lots of iterations can use smaller chunks.
	 */
	if (maxiter >= 65536)
		error("iteration count too large");

	flags = ntohs(job->j.flags);
	julia = ntohs(job->j.julia);

	if (julia) {
		n_parms = 4;
		x_parm_no = 2;	/* z0.re */
		y_parm_no = 3;	/* z0.im */
	} else {		/* Mandelbrot */

		n_parms = 4;
		x_parm_no = 0;	/* c0.re */
		y_parm_no = 1;	/* c0.im */
	}

	/* point to beginning of parameters */
	p = (char *)&job->j.corner.re;

	/* convert each parameter from network format to computational format */
	for (i = 0; i < n_parms; i++)
		parm_buf[i] = decode_parm(&p);

	/* set up pointers to the parameters that correspond to x and y */
	varx = &parm_buf[x_parm_no];
	vary = &parm_buf[y_parm_no];

	delta.re = decode_parm(&p);
	delta.im = decode_parm(&p);

	/* take the chunk offset into account */
	(*varx) = add_real((*varx), mul_real_int(delta.re, xmin));
	(*vary) = add_real((*vary), mul_real_int(delta.im, ymin));

	/* save initial x coordinate for reuse on subsequent scanlines */
	initial_varx = (*varx);

	for (yc = ymin; yc < ymax; yc++) {
		(*varx) = initial_varx;

		for (xc = xmin; xc < xmax; xc++) {
#ifdef HENON
			unsigned int count = henon(parm_buf, maxiter, flags);
#else
			unsigned int count =
			    mandelbrot(parm_buf, maxiter, flags);
#endif

			if (maxiter > 256)
				out->reply.data.shorts[(yc - ymin) * xsize +
						       (xc - xmin)] =
				    htons(count);
			else
				out->reply.data.chars[(yc - ymin) * xsize +
						      (xc - xmin)] = count;
			mi_count += count;

			(*varx) = add_real((*varx), delta.re);
		}
		(*vary) = add_real((*vary), delta.im);
	}
	out->reply.mi_count = htonl(mi_count);
	return (datasize);
}

void serve()
{
	Message in;
	Message out;
	int isock = 0;
	int osock = use_sockets ? 0 : 1;

	NET_ADDRESS oname;

	while (1) {
		unsigned int bytes;
		int onamelen = sizeof(oname);
		int version;
		if (timeout != 0)
			alarm(timeout);
		/* receive from anywhere, save the address of the caller in oname */
		if (RECV(isock, (char *)&in, sizeof(in), 0,
			 (struct sockaddr *)&oname, &onamelen) < 0)
			error("receiving datagram packet");
		version = ntohs(in.generic.header.version);
		if (ntohs(in.generic.header.magic) == MAGIC
		    && version == VERSION) {
			switch (ntohs(in.generic.header.type)) {
			case WHIP_MESSAGE:
				/* copy the header and id structures as such while still in */
				/* network byte order (not strictly portable but probably works) */
				out.reply.header = in.whip.header;
				out.reply.id = in.whip.id;
				/* just the message type needs to be changed */
				out.reply.header.type = htons(REPLY_MESSAGE);
				/* calculate() sets the data and mi_count fields */
				bytes = calculate(&in, &out);
				if (bytes)
					if (SEND
					    (osock, (char *)&out, (int)bytes, 0,
					     (struct sockaddr *)&oname,
					     sizeof(oname)) < 0)
						error
						    ("sending calculated data");
				break;
			case WHO_R_U_MESSAGE:
				oname.sin_port = in.who.port;	/* in network byte order already */
				out.iam.header = in.who.header;
				out.iam.pid = htons(getpid());
				if (SEND
				    (osock, (char *)&out, sizeof(IAmMessage), 0,
				     (struct sockaddr *)&oname,
				     sizeof(oname)) < 0)
					error
					    ("sending response to pid inquiry");
				break;
			default:;	/* ignore other messages */
			}
		}
	}
}

/*
  This function is called when we get a SIGALRM so that we die gracefully 
  with exit status 0; otherwise inetd would log a message about us getting
  an alarm signal.
*/

int die()
{
	exit(0);
}

int main(argc, argv)
int argc;
char **argv;
{
	static char usage_msg[] = "usage: mslaved [ -options ] mslaved\n";
	me = argv[0];
	signal(SIGALRM, die);
	while (--argc) {
		char *s = *++argv;
		if (*s++ != '-')
			goto usage;
		switch (*s++) {
		case 'n':	/* nice */
			niceval = atoi(*s ? s : (--argc, *++argv));
			break;
		case 't':	/* timeout */
			timeout = atoi(*s ? s : (--argc, *++argv));
			break;
		case 'i':	/* accept for backwards compatibility but ignore */
			break;
		case 'p':	/* pipe mode */
			use_sockets = 0;
			break;
		default:
			goto usage;
		}
	}
	nice(niceval);
	serve();		/* never returns */

 usage:
	/*
	   Try to avoid pulling in printf from the library 
	   (but syslog probably does that anyway...) 
	 */
	write(2, usage_msg, sizeof(usage_msg) - 1);
	exit(1);
}
