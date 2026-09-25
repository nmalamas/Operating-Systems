
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void *args)
{
  PCB *current_process = CURPROC; //Get the current process
  PTCB *newPTCB = createPTCB(current_process, task, argl, args);  //Initialize new ptcb
  TCB *newTCB = spawn_thread(current_process, start_basic_thread);
  newPTCB->tcb = newTCB;  //Connect ptcb with tcb
  newPTCB->tcb->owner_ptcb = newPTCB; //Connect tcb with ptcb 
  current_process->thread_count++;  //Increase threads 
  wakeup(newPTCB->tcb);
  return (Tid_t)newPTCB;
}

/** 
  @brief Create a new PTCB.
  */
PTCB *createPTCB(PCB *pcb, Task task, int argl, void *args)
{
  PTCB *newPTCB = xmalloc(sizeof(PTCB));  //Allocate memory 
  newPTCB->task = task;
  newPTCB->argl = argl;
  newPTCB->args = args;
  newPTCB->refcount = 1;
  newPTCB->exit_cv = COND_INIT;
  newPTCB->exited = 0;
  newPTCB->exitval = 0;
  newPTCB->detached = 0;
  rlnode_init(&newPTCB->ptcb_list_node, newPTCB); //Initialize the list node 
  rlist_push_front(&pcb->thread_list, &newPTCB->ptcb_list_node);  //Add the node to the list
  return newPTCB;
}
/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
  return (Tid_t)cur_thread()->owner_ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int *exitval)
{
  PTCB *joinablePTCB = (PTCB *)tid;
  PCB *currentPCB = CURPROC;

  if(rlist_find(&currentPCB->thread_list,joinablePTCB,NULL)==NULL ){  //Check that the given ptcb exist in the current process ptcb
    return -1;
  }
  if(sys_ThreadSelf() == tid) { //Can't join myself
    return -1;
  }  

  joinablePTCB->refcount++;
  while (joinablePTCB->detached != 1 && joinablePTCB->exited != 1)  
  {
    kernel_wait(&joinablePTCB->exit_cv, SCHED_USER);  //Sleep
  }
  joinablePTCB->refcount--;
  if(joinablePTCB->detached == 1){  //Check if we woke up from detach
    return-1;
  }
  if(exitval != NULL){  //Ptcb exited. Get exitval.
    *exitval = joinablePTCB->exitval;
  }
  if(joinablePTCB->refcount==1){  //Noone is waiting so i can erase.
    rlist_remove(&joinablePTCB->ptcb_list_node);  //Remove from the tread list
    free(joinablePTCB); //Free the memory
  }
  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{

  PTCB *givenThread = (PTCB *)tid;  //Get ptcb

  if (sys_ThreadSelf() == tid){ //Check for detaching self
    givenThread->detached = 1;
    return 0;
  }
  if (rlist_find(&CURPROC->thread_list, givenThread, NULL) == NULL){  //Check that the ptcb exist in the current process
    return -1;
  }
  if(givenThread->exited == 1){ //Check if thread exited
    return -1;
  }
  givenThread->detached = 1;
  kernel_broadcast(&givenThread->exit_cv); //Signal detaching 
  return 0;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  if (CURPTCB->detached == 1) //If thread is detached we can completelly remove it 
  {
    CURPTCB->exited = 1;
    rlist_remove(&CURPTCB->ptcb_list_node);
    free(CURPTCB);
  }

  CURPTCB->exitval = exitval;
  CURPTCB->exited = 1;
  kernel_broadcast(&CURPTCB->exit_cv);  //Signal exit

  PCB *curproc = CURPROC; /* cache for efficiency */
  curproc->thread_count--;

  if (curproc->thread_count == 0)
  {
    while (rlist_len(&curproc->thread_list) != 0)
    {
      rlnode *deletedNode = curproc->thread_list.next;
      rlist_pop_front(&curproc->thread_list);
      free(deletedNode->ptcb);
    }
    if (get_pid(curproc) != 1)
    {
      /* Reparent any children of the exiting process to the 
       initial task */
      PCB *initpcb = get_pcb(1);
      while (!is_rlist_empty(&curproc->children_list))
      {
        rlnode *child = rlist_pop_front(&curproc->children_list);
        child->pcb->parent = initpcb;
        rlist_push_front(&initpcb->children_list, child);
      }

      /* Add exited children to the initial task's exited list 
       and signal the initial task */
      if (!is_rlist_empty(&curproc->exited_list))
      {
        rlist_append(&initpcb->exited_list, &curproc->exited_list);
        kernel_broadcast(&initpcb->child_exit);
      }

      /* Put me into my parent's exited list */
      rlist_push_front(&curproc->parent->exited_list, &curproc->exited_node);
      kernel_broadcast(&curproc->parent->child_exit);
    }

    assert(is_rlist_empty(&curproc->children_list));
    assert(is_rlist_empty(&curproc->exited_list));

    /* 
    Do all the other cleanup we want here, close files etc. 
   */

    /* Release the args data */
    if (curproc->args)
    {
      free(curproc->args);
      curproc->args = NULL;
    }

    /* Clean up FIDT */
    for (int i = 0; i < MAX_FILEID; i++)
    {
      if (curproc->FIDT[i] != NULL)
      {
        FCB_decref(curproc->FIDT[i]);
        curproc->FIDT[i] = NULL;
      }
    }

    /* Disconnect my main_thread */
    curproc->main_thread = NULL;

    /* Now, mark the process as exited. */
    curproc->pstate = ZOMBIE;
  }

  /* Bye-bye cruel world */
  kernel_sleep(EXITED, SCHED_USER);
}
