/* io.h -- low-level client/server communications and I/O multiplexing */

/* 
   This module specifies a system-independent interface to the slave
   communications.  It offers the following services to the workforce
   handler (work.c):

     - transmission of messages to the servers
     - reception of replies from the servers
     - timeouts

   The data transport may be based on either Internet domain UDP sockets
   (IO_TRANS_UDP) or plain Unix pipes (IO_TRANS PIPE).

   The input multiplexing and timeouts may be based on either the
   XtAppAddInput and XtAddTimeOut mechanisms in Xt (IO_MUX_XT), the
   select() system call (IO_MUX_SELECT), or a rudimentary mechanism that
   supports only a single data source and no timeouts (IO_MUX_NONE).
*/

typedef enum { IO_TRANS_UDP, IO_TRANS_PIPE } io_transport;
typedef enum { IO_MUX_XT, IO_MUX_SELECT, IO_MUX_NONE } io_multiplex;

typedef struct io_state io_state;

/* methods */
io_state *io_init();
void io_deinit();
void io_main();
int io_send();
void io_done();
int io_get_recv_fd();
void io_ignore_ewouldblock();

void io_handle_socket_input();

/* services needed by the module */
void io_error();		/* error reporting */
