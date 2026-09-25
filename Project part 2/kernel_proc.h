#ifndef __KERNEL_PROC_H
#define __KERNEL_PROC_H

/**
  @file kernel_proc.h
  @brief The process table and process management.

  @defgroup proc Processes
  @ingroup kernel
  @brief The process table and process management.

  This file defines the PCB structure and basic helpers for
  process access.

  @{
*/ 

#include "tinyos.h"
#include "kernel_sched.h"

/**
  @brief PID state

  A PID can be either free (no process is using it), ALIVE (some running process is
  using it), or ZOMBIE (a zombie process is using it).
  */
typedef enum pid_state_e {
  FREE,   /**< @brief The PID is free and available */
  ALIVE,  /**< @brief The PID is given to a process */
  ZOMBIE  /**< @brief The PID is held by a zombie */
} pid_state;
/**
  @brief Socket type

  A socket can be either unbound, listener, or peer
 */
enum socket_type{
  SOCKET_UNBOUND,
  SOCKET_LISTENER,
  SOCKET_PEER
};
/**
  @brief Unbound socket.

  This structure holds all information pertaining to a unbound socket.
 */
typedef struct unbound_socket{
  rlnode unbound_socket;
}unbound_socket;

/**
  @brief Listener socket.

  This structure holds all information pertaining to a listener socket.
 */
typedef struct listener_socket{
  rlnode queue;
  CondVar req_available;
}listener_socket;

/**
  @brief Peer socket.

  This structure holds all information pertaining to a peer socket.
 */
typedef struct peer_socket{
  socket_cb* peer;
  pipe_cb* write_pipe;
  pipe_cb* read_pipe;
}peer_socket;

/**
  @brief Socket Control Block.

  This structure holds all information pertaining to a process.
 */
typedef struct socket_control_block{
  uint refcount;
  FCB* fcb;
  enum socket_type type;
  port_t port;

  union {
    unbound_socket unbound_s;
    listener_socket listener_s;
    peer_socket peer_s;
  };
}socket_cb;
/**
  @brief Connection Request Control Block.

  This structure holds all information pertaining to a request.
 */
typedef struct connection_request{
  int admitted;
  socket_cb* peer;

  CondVar connected_cv;
  rlnode queue_node;
}connection_request;
/**
  @brief ProcInfo Control Block.

  This structure holds all information pertaining to a procinfo.
 */
typedef struct procinfo_control_block{
  procinfo procinfo;
  FCB* fcb;
  int PCBcursor;
}procinfo_cb;

/**
  @brief Process Control Block.

  This structure holds all information pertaining to a process.
 */
typedef struct process_control_block {
  pid_state  pstate;      /**< @brief The pid state for this PCB */

  PCB* parent;            /**< @brief Parent's pcb. */
  int exitval;            /**< @brief The exit value of the process */

  TCB* main_thread;       /**< @brief The main thread */
  Task main_task;         /**< @brief The main thread's function */
  int argl;               /**< @brief The main thread's argument length */
  void* args;             /**< @brief The main thread's argument string */

  rlnode children_list;   /**< @brief List of children */
  rlnode exited_list;     /**< @brief List of exited children */

  rlnode children_node;   /**< @brief Intrusive node for @c children_list */
  rlnode exited_node;     /**< @brief Intrusive node for @c exited_list */

  rlnode thread_list;     /**< @brief List of threads */
  int thread_count;       /**< @brief The total threads */

  CondVar child_exit;     /**< @brief Condition variable for @c WaitChild. 

                             This condition variable is  broadcast each time a child
                             process terminates. It is used in the implementation of
                             @c WaitChild() */

  FCB* FIDT[MAX_FILEID];  /**< @brief The fileid table of the process */



} PCB;
/**
  @brief Process Thread Control Block.

  This structure holds all information pertaining to a thread.
 */
typedef struct process_thread_control_block{
  TCB* tcb;  /**< @brief The the tcb */

  Task task;   /**< @brief The thread task */
  int argl;   /**< @brief The argl */
  void* args;   /**< @brief The args */
  int exitval;  /**< @brief The exit value */
  int exited;   /**< @brief Initialize at 0. If thread exited 1 */
  int detached; /**< @brief Initialize at 0. If thread detached 1 */
  CondVar exit_cv;  /**< @brief Condition variable for exit cv */
  int refcount ;  /**< @brief Counter for thread references */
  rlnode ptcb_list_node;  /**< @brief List nodes */
} PTCB;
/**
  @brief Pipe Control Block.

  This structure holds all information pertaining to a pipe.
 */
typedef struct pipe_control_block{
  FCB* reader;
  FCB* writer;

  CondVar has_space; /* For blocking writer if no space is available */
  CondVar has_data; /* For blocking reader until data are available */

  int w_position; /* write position in buffer */
  int r_position; /* read position in buffer */

  char BUFFER[PIPE_BUFFER_SIZE];
  int total_data; /* How many slots are writen and not red */
} pipe_cb;
/**
  @brief Function to create a new pipe
 */
int sys_Pipe(pipe_t* pipe);
/**
  @brief Function to write to pipe
 */
int pipe_write(void* pipecb_t,const char *buf, unsigned int n);
/**
  @brief Function to read from pipe
 */
int pipe_read(void* pipecb_t,char *buf, unsigned int n);
/**
  @brief Function to close the writer from a pipe
 */
int pipe_writer_close(void* _pipecb);
/**
  @brief Function to close the reader from a pipe
 */
int pipe_reader_close(void* _pipecb);
/*
  @brief Function to create a new ptcb
 */
PTCB* createPTCB(PCB* pcb, Task task, int argl, void* args);
/**
  @brief Function to start a thread;
 */
void start_basic_thread();

/**
  @brief Initialize the process table.

  This function is called during kernel initialization, to initialize
  any data structures related to process creation.
*/
void initialize_processes();

/**
  @brief Get the PCB for a PID.

  This function will return a pointer to the PCB of 
  the process with a given PID. If the PID does not
  correspond to a process, the function returns @c NULL.

  @param pid the pid of the process 
  @returns A pointer to the PCB of the process, or NULL.
*/
PCB* get_pcb(Pid_t pid);

/**
  @brief Get the PID of a PCB.

  This function will return the PID of the process 
  whose PCB is pointed at by @c pcb. If the pcb does not
  correspond to a process, the function returns @c NOPROC.

  @param pcb the pcb of the process 
  @returns the PID of the process, or NOPROC.
*/
Pid_t get_pid(PCB* pcb);

/** @} */

#endif
