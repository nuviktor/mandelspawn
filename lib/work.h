/* work.h - public definitions for work distribution module */
/* Copyright (C) 1991-1993 Andreas Gustafsson */

/* **************** types **************** */

typedef struct wf_state wf_state;

/* **************** public functions **************** */

/* 
   wf=wf_init(timeout, pipe_mux, socket_mux): initialize the work
   distribution system.  "timeout" should be well larger than the time
   to complete a single work packet (in milliseconds).
   The multiplexer "pipe_mux" is used for pipe input, 
   "socket_mux" for socket input.
*/
struct wf_state *wf_init();

/* wf_begin_dispatch(wf): prepare for a series of wf_dispatch_chunk calls */
void wf_begin_dispatch();

/* wf_dispatch_chunk(wf, client, client_data, client_datalen, 
     slave_data, slave_datalen): give a piece of work to the workforce */
void wf_dispatch_chunk();

/* wf_restart(wf): make sure the computation servers are at work */
void wf_restart();

/* wf_handle_socket_input(wf, client_data): call this when input available */
void wf_handle_socket_input();

/* wf_timed_out(client_data): call when timeout has occurred */
void wf_timed_out();

/* wf_client_died(): call when client no longer wants wf_draw() calls
   despite outstanding requests */
void wf_client_died();

/* wf_print_stats(wf): print statistics about server performance */
void wf_print_stats();

/* wf_socket(wf): get the fd of the slave communications socket
  (for select) */
int wf_socket();

#ifndef OLD_TIMEOUT
void wf_tick();
#endif

/* **** external functions called from work.c, user must define **** */

/* misc. services */
void wf_error();		/* error reporting */
void wf_warn();

void wf_draw();			/* callback */
