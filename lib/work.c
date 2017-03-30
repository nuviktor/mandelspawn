/* work.c -- MandelSpawn work distribution */

/*
  Queue requests for calculation, possibly from different clients
  (such as multiple windows), distribute them among the computation
  servers and return the results to the client that requested them.

  See work.h for some kind of interface definition. 
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
#include <string.h>
#include <errno.h>		/* pre-X11R4 systems need this for EWOULDBLOCK */

#ifdef NO_STRDUP
char *strdup();
#endif

/*
  It is impossible to declare malloc() in a portable way.
  Be prepared to change these declarations. 
*/
void *malloc(), *realloc();
void free();

#include "datarep.h"
#include "ms_ipc.h"
#include "io.h"
#include "work.h"

/* This is for Linux */
#ifndef FNDELAY
#ifdef O_NDELAY
#define FNDELAY O_NDELAY
#endif
#endif

#ifdef NO_BCOPY
#define bcopy(s,d,n) memcpy(d,s,n)
#endif

typedef struct slave {
	char *name_string;	/* machine name of the slave */
	NET_ADDRESS name;	/* network address of the slave */
	int has_timeout;	/* the timeout has been initialized */
	unsigned timeout;	/* timeout in milliseconds */
#ifndef OLD_TIMEOUT
	long timeout_at;	/* when to timeout (seconds since epoc) */
#else
	char *timer_id;		/* timeout id (for removing the timeout) */
#endif
	unsigned int n_timeouts;	/* how many times the slave has timed out */
	unsigned int n_packets;	/* number of packets that have arrived */
	unsigned int n_late_packets;	/* number of packets that arrived too late */
	unsigned long mi_count;	/* number of iterations done by this slave */
	unsigned int no;	/* slave serial number */
	struct wf_state *backptr;	/* back pointer to the wf_state */
	int disabled;		/* slave disabled due to error */
} slave;

typedef struct chunk {
	struct chunk *next;
	struct chunk *prev;
	int drawn;		/* true if at least one reply has arrived */
	char *client;		/* pointer to widget owning this chunk */
	unsigned int no;	/* serial number within current sequence */
	char *client_data;	/* client data (unknown size) */
	char *slave_data;	/* slave data */
	unsigned int slave_datalen;	/* length of slave data */
} chunk;

struct wf_state {
	io_state *io;		/* pointer to I/O object */
	int sequence;		/* current sequence number */
	int pid;		/* pid of this process */
	unsigned n_slaves;	/* number of slaves */
	struct slave **slaves;	/* array of pointers to slave descriptors */
	int n_chunks;		/* number of chunks in the active sequence */
	unsigned int max_chunks;	/* current size of chunk index */
	struct chunk **chunks;	/* pointer to the chunk index */
	struct chunk to_draw;	/* head of queue of chunks to be drawn */
	struct chunk drawn;	/* head of queue of chunks already drawn */
	struct chunk *insert_point;	/* point in queue for inserting new work */
};

/* forward refs */
static void handle_reply_msg(), whip_slave(), timeout_set(), timeout_unset();

/* names of files containing server hostnames */
#define PERSONAL_SLAVEFILE ".mslaves"
#ifndef PUBLIC_SLAVEFILE
#define PUBLIC_SLAVEFILE "/usr/local/etc/mslaves"
#endif

#define SERVER_PROG	"mslaved"

#define INITIAL_CHUNKS 1024

#define MAX_WORKPACKET_SIZE 64
#define DATAGRAM_BYTES 1200

char *getenv();

#ifdef HAVE_SOCKETS

/*
  Get the address of a host given either the host name or the address
  in nnn.nnn.nnn.nnn notation 
*/

struct hostent *gethostbyname();

static struct in_addr *gethostaddrbywhatever(p)
char *p;
{
	struct hostent *hp;

	/* statically allocated return value buffer */
	/* (this is ugly, but gethostbyname() is no better) */
	static struct in_addr nobyinaddr;

	if ((hp = gethostbyname(p)) != NULL) {
		return ((struct in_addr *)(hp->h_addr));
	} else {		/* try it as a number nnn.nnn.nnn.nnn */
		if ((nobyinaddr.s_addr = inet_addr(p)) == -1) {
			return (0);
		}
		return (&nobyinaddr);
	}
}

/* 
  Null-terminate the current field in a whitespace-separated list
  and return a pointer to the next field or NULL if there is no
  next field.  Text after a hash sign is taken as comment and ignored.
*/

static char *next_field(p)
char *p;
{
	while (1) {
		register char c = *p;
		switch (c) {
		case ' ':
		case '\t':
			*p = '\0';
			while (1) {
				int c = *++p;
				if (c == '\n' || c == '\0')
					return (0);
				if (c != ' ' && c != '\t')
					break;
			}
			return (p);
		case '\0':
			return (0);
		case '\n':
		case '#':
			*p = '\0';
			return (0);
		default:
			break;
		}
		p++;
	}
}
#endif				/* HAVE_SOCKETS */

slave *new_slave(wf, i)
wf_state *wf;
int i;
{
	slave *s = (slave *) malloc(sizeof(slave));
	s->mi_count = 0L;
	s->has_timeout = 0;
	s->n_timeouts = s->n_packets = s->n_late_packets = 0;
	s->disabled = 0;
	s->no = i;
	s->backptr = wf;
	return (s);
}

/* Symbolic names for the ends of a pipe */
#define READ 0
#define WRITE 1

/* Initialize the workforce */

wf_state *wf_init(timeout, pipe_mux, socket_mux)
unsigned timeout;
io_multiplex pipe_mux, socket_mux;
{
	FILE *f;
	int i;
	char *filename, *home;
	unsigned int size = 16;	/* initial size of slave table */
	char buf[256];
	wf_state *wf = (wf_state *) malloc(sizeof(wf_state));

	/* general initialization: */

	/* set up the chunk index */
	wf->max_chunks = INITIAL_CHUNKS;
	wf->n_chunks = 0;
	wf->chunks = (chunk **) malloc(wf->max_chunks * sizeof(chunk *));

	/* set up the chunk queues */
	wf->to_draw.prev = wf->to_draw.next = &wf->to_draw;
	wf->drawn.prev = wf->drawn.next = &wf->drawn;

	wf->pid = getpid();
	wf->sequence = 0;

#ifdef HAVE_SOCKETS
	/* .mslaves file stuff */
	wf->slaves = (slave **) malloc(size * sizeof(slave *));
	home = getenv("HOME");
	if (!home)
		wf_error("HOME not set");
	filename =
	    malloc((unsigned)
		   (strlen(home) + 1 + strlen(PERSONAL_SLAVEFILE) + 1));
	strcpy(filename, home);
	strcat(filename, "/");
	strcat(filename, PERSONAL_SLAVEFILE);
	f = fopen(filename, "r");
	free(filename);
	if (!f) {
		f = fopen(PUBLIC_SLAVEFILE, "r");
	}
	if (!f)			/* no .mslaves file */
#endif				/* HAVE_SOCKETS */
	{
		slave *s;
		int pipe_from_server[2];
		int pipe_to_server[2];
#ifdef HAVE_SOCKETS
		wf_warn("No .mslaves file found, reverting to single-CPU mode");
#endif				/* HAVE_SOCKETS */
		wf->slaves = (slave **) malloc(sizeof(slave *));
		s = new_slave(wf, 0);
		/* s->name is not used */
		s->name_string = "localhost";
		s->timeout = timeout;
		wf->slaves[0] = s;
		wf->n_slaves = 1;

		pipe(pipe_to_server);
		pipe(pipe_from_server);

		if (!fork()) {	/* child */
			close(0);
			dup(pipe_to_server[READ]);
			close(1);
			dup(pipe_from_server[WRITE]);
			close(pipe_to_server[WRITE]);
			close(pipe_from_server[READ]);
			execlp(SERVER_PROG, SERVER_PROG, "-p", "-t0",
			       (char *)0);
			wf_error("could not exec server program");
		}

		wf->io = io_init(IO_TRANS_PIPE,
				 pipe_mux,
				 pipe_from_server[READ], pipe_to_server[WRITE],
				 (char *)wf,
				 /* this allocates space according to MAX_DATAGRAM_SIZE */
				 malloc(sizeof(Message)), sizeof(Message),
				 handle_reply_msg, wf_tick);
		close(pipe_to_server[READ]);
		close(pipe_from_server[WRITE]);
	}
#ifdef HAVE_SOCKETS
	else {			/* found a .mslaves file */
		i = 0;
		while (1) {
			char *p;	/* points to current field in .mslaves line */
			char *q;	/* points to next field in .mslaves line */
			slave *s;
			struct in_addr *ina;
			unsigned port;

			if (!fgets(buf, sizeof(buf), f))
				break;
			if (buf[0] == '\n' || buf[0] == '#')
				continue;
			p = buf;
			q = next_field(p);
			ina = gethostaddrbywhatever(p);

			if (ina == 0) {
				static char warn[] =
				    "unknown host in .mslaves, ignored: ";
				/* trying to keep up with GNU coding standards... */
				char *msg = malloc(sizeof(warn) + strlen(p));
				strcpy(msg, warn);
				strcat(msg, p);
				wf_warn(msg);
				free(msg);
				continue;
			}

			p = q;
			port = DEFAULT_PORT;
			if (p) {	/* there is a "port" field */
				q = next_field(p);
				port = (unsigned)atoi(p);
				if (port == 0) {	/* probably not an integer, and port 0 is bad anyway */
					static char warn[] =
					    "bad port field in .mslaves, machine ignored: ";
					char *msg =
					    malloc(sizeof(warn) + strlen(p));
					strcpy(msg, warn);
					strcat(msg, p);
					wf_warn(msg);
					free(msg);
					continue;
				}
			}

			if (q) {
				wf_warn("trailing junk in .mslaves");
			}

			if (i >= size) {
				size *= 2;
				wf->slaves =
				    (slave **) realloc((char *)wf->slaves,
						       size * sizeof(slave *));
			}
			s = new_slave(wf, i);
			bcopy((char *)ina, (char *)&s->name.sin_addr,
			      sizeof(struct in_addr));
			s->name.sin_family = AF_INET;
			s->name.sin_port = htons(port);
			s->name_string = strdup(buf);
			s->timeout = timeout;
			wf->slaves[i] = s;
			i++;
		}
		wf->n_slaves = i;
	}

	/* make a socket for communicating with the slaves */
	{
		int sock;
		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			wf_error("socket");
		/* make sure we don't block reading the socket */
		if (fcntl(sock, F_SETFL, FNDELAY) == -1)
			wf_error("unblocking socket");
		wf->io = io_init(IO_TRANS_UDP,
				 socket_mux, sock, sock, (char *)wf,
				 /* this allocates space according to MAX_DATAGRAM_SIZE */
				 malloc(sizeof(Message)), sizeof(Message),
				 handle_reply_msg, wf_tick);
	}
#endif				/* HAVE_SOCKETS */
	return (wf);
}

/*
  Delete the chunk "c" from anywhere in the queue.  Don't deallocate
  the chunk yet because duplicated messages may still refer to it.  
*/

static void queue_delete(c)
chunk *c;
{
	c->next->prev = c->prev;
	c->prev->next = c->next;
	c->next = c->prev = NULL;	/* just for easier debugging */
}

/* Add a chunk to the tail of the queue. */

static void queue_add(q, c)
chunk *q;
chunk *c;
{
	c->prev = q->prev;
	c->next = q;
	q->prev->next = c;
	q->prev = c;
}

/* Test the emptiness of a chunk queue. */

static int queue_empty(q)
chunk *q;
{
	return (q->next == q && q->prev == q);
}

/* Return the head of a chunk queue. */

static chunk *queue_head(q)
chunk *q;
{
	return (q->next);
}

void wf_timed_out(client_data)
char *client_data;
{
	slave *s = (slave *) client_data;
	s->has_timeout = 0;
	s->n_timeouts++;
	whip_slave(s->backptr, s);
}

/* Set a timeout for a slave; this is done when the slave is whipped. */

static void timeout_set(s)
slave *s;
{
#ifndef OLD_TIMEOUT
	s->timeout_at = time((long *)0) + s->timeout / 1000;
#else
	/* first make sure there isn't a timeout already */
	timeout_unset(s);
	s->timer_id = wf_add_timeout(s->timeout, (char *)s);
#endif
	s->has_timeout = 1;
}

/* Remove a timeout. */

static void timeout_unset(s)
slave *s;
{
#ifdef OLD_TIMEOUT
	if (s->has_timeout)
		wf_remove_timeout(s->timer_id);
#endif
	s->has_timeout = 0;
}

/* Stop the slaves. */

static void stop_slaves(wf)
wf_state *wf;
{
	int i;
	for (i = 0; i < wf->n_chunks; i++) {
		chunk *c = wf->chunks[i];
		free((char *)(c->client_data));
		free((char *)(c->slave_data));
		free((char *)c);
	}
	/* don't shrink the chunk index array; we probably need it again */
	wf->n_chunks = 0;
	wf->sequence++;
	wf->drawn.prev = wf->drawn.next = &wf->drawn;
}

/* Put the specified slave to work (if there is any) */

static void whip_slave(wf, s)
wf_state *wf;
slave *s;
{
	struct {
		WhipMessage m;
		char data[MAX_WORKPACKET_SIZE];
	} mm;			/* buffer for building the message to send */
	chunk *c;

	if (s->disabled || queue_empty(&wf->to_draw))
		return;

	c = queue_head(&wf->to_draw);
	mm.m.header.magic = htons(MAGIC);
	mm.m.header.type = htons(WHIP_MESSAGE);
	mm.m.header.version = htons(VERSION);
	mm.m.header.format = htons(DATA_FORMAT);
	mm.m.id.pid = wf->pid;
	mm.m.id.seq = wf->sequence;
	mm.m.id.slave_no = s->no;
	mm.m.id.chunk_no = c->no;

	if (c->slave_datalen > MAX_WORKPACKET_SIZE)
		wf_error("work packet too large");
	bcopy(c->slave_data, mm.m.data, c->slave_datalen);

	if (io_send(wf->io, (char *)&mm, sizeof(mm.m) + c->slave_datalen,
		    (char *)&s->name, (int)sizeof(s->name)) == -1) {
		wf_warn
		    ("error sending datagram, use of affected server disabled");
		s->disabled = 1;	/* consider this slave unusable */
	}

	timeout_set(s);
	/* move the chunk from the head to the tail of the queue */
	queue_delete(c);
	queue_add(&wf->to_draw, c);
}

/* Handle a reply from a slave. */

static void handle_reply_msg(closure, msg, msglen)
char *closure;			/* really a wf_state* */
Message *msg;
int msglen;
/*ARGSUSED*/
{
	wf_state *wf = (wf_state *) closure;
	char *client;
	slave *s;
	chunk *c;

	unsigned int pid = msg->reply.id.pid;
	unsigned int seqno = msg->reply.id.seq;
	unsigned int chunkno = msg->reply.id.chunk_no;
	unsigned int slaveno = msg->reply.id.slave_no;
	int late;

	if (pid != wf->pid)
		return;
	if (slaveno >= wf->n_slaves)
		return;
	s = wf->slaves[slaveno];
	s->n_packets++;
	if (seqno != wf->sequence) {
		s->n_late_packets++;
		return;
	}
	if (chunkno >= wf->n_chunks)
		return;
	c = wf->chunks[chunkno];

	timeout_unset(s);

	client = c->client;

	/*
	   The chunk is too late if the client has gone away or if the chunk
	   has been drawn already.
	 */
	late = (!client || c->drawn);

	if (late) {		/* ignore the message if the chunk arrived too late */
		s->n_late_packets++;
	} else {
		queue_delete(c);
		queue_add(&wf->drawn, c);
		c->drawn = 1;
	}

	/* Put the slave to work again */
	whip_slave(wf, s);

	if (!late) {
		wf_draw(client, c->client_data, (char *)&(msg->reply.data));
		s->mi_count += ntohl(msg->reply.mi_count);
	}

	/* If done, stop all the slaves and deallocate the chunks */
	if (queue_empty(&wf->to_draw))
		stop_slaves(wf);
}

/*
  Prepare for dispatching new chunks.
*/

void wf_begin_dispatch(wf)
wf_state *wf;
{
	wf->insert_point = wf->to_draw.next;
}

/*
  This function is called by the Ms widget for each chunk it 
  wants to be calculated.
*/

void
wf_dispatch_chunk(wf, client, client_data, client_datalen,
		  slave_data, slave_datalen)
wf_state *wf;
char *client;
char *client_data;
unsigned int client_datalen;
char *slave_data;
unsigned int slave_datalen;
{
	int chunkno = wf->n_chunks++;
	chunk *c = (chunk *) malloc(sizeof(chunk));
	c->client_data = (char *)malloc(client_datalen);
	bcopy(client_data, c->client_data, client_datalen);
	c->slave_data = (char *)malloc(slave_datalen);
	bcopy(slave_data, c->slave_data, slave_datalen);
	c->slave_datalen = slave_datalen;
	c->client = client;
	c->drawn = 0;
	c->no = chunkno;
	/* grow the chunk index if necessary */
	if (chunkno >= wf->max_chunks) {
		wf->max_chunks *= 2;
		wf->chunks = (chunk **) realloc((char *)wf->chunks,
						wf->max_chunks *
						sizeof(chunk *));
	}
	wf->chunks[chunkno] = c;
	queue_add(wf->insert_point, c);
}

/* Make sure all the slaves are put to work. */

void wf_restart(wf)
wf_state *wf;
{
	int i;
	for (i = 0; i < wf->n_slaves; i++) {
		whip_slave(wf, wf->slaves[i]);
	}
}

/* Handle the situation of a client aborting prematurely. */

void wf_client_died(wf, cli)
wf_state *wf;
char *cli;
{
	chunk *c;
	chunk *next_c;
	/* Remove all chunks of the dead widget from the work queue */
	/* and nullify their widget pointer fields so that late packets */
	/* won't reference the nonexistent widget */
	for (c = wf->to_draw.next; c != &wf->to_draw; c = next_c) {	/* need to use a temporary variable because */
		/* relinking the chunk fouls up the .next field */
		next_c = c->next;
		if (c->client == cli) {
			c->client = NULL;
			queue_delete(c);
			queue_add(&wf->drawn, c);
		}
	}
}

/* Print performance statistics. */

void wf_print_stats(wf, f)
wf_state *wf;
FILE *f;
{
	int i;
	int active = 0;
	unsigned long mi_tot = 0;
	fprintf(f, "\n%-22s %10s %10s %10s %10s\n",
		"Host", "iterations", "packets", "timeouts", "late");
	for (i = 0; i < wf->n_slaves; i++) {
		slave *s = wf->slaves[i];
		fprintf(f, "%-22s %10lu %10u %10u %10u\n",
			s->name_string,
			s->mi_count, s->n_packets, s->n_timeouts,
			s->n_late_packets);
		if (s->mi_count)
			active++;
		mi_tot += s->mi_count;
	}
	fprintf(f, "%d servers, %d active, %lu iterations total\n",
		wf->n_slaves, active, mi_tot);
	fflush(f);
}

/* Make information about the maximum message size available. */
unsigned wf_max_message_size()
{
	return (DATAGRAM_BYTES - sizeof(ReplyHeader));
}

#ifndef OLD_TIMEOUT
void wf_tick(closure)
char *closure;
{
	wf_state *wf = (wf_state *) closure;
	int i;
	unsigned n_slaves = wf->n_slaves;
	long now = time((long *)0);
	for (i = 0; i < n_slaves; i++) {
		slave *s = s = wf->slaves[i];
		if (s->has_timeout && s->timeout_at <= now) {
			wf_timed_out((char *)s);
		}
	}
}
#endif

wf_main(wf)
wf_state *wf;
{
	io_main(wf->io);
}

void wf_done(wf)
wf_state *wf;
{
	io_done(wf->io);
}

io_state *wf_get_io(wf)
wf_state *wf;
{
	return wf->io;
}
