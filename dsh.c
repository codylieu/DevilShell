
#include "dsh.h"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */

job_t* active_jobs_head = NULL;
job_t* active_jobs = NULL;
job_t* stopped_jobs_head = NULL;
job_t* stopped_jobs = NULL;

/* Sets the process group id for a given job and process */
int set_child_pgid(job_t *j, process_t *p)
{
  if (j->pgid < 0) /* first child: use its pid for job pgid */
    j->pgid = p->pid;
  return(setpgid(p->pid,j->pgid));
}

//created method to wait for pid and check for status
void waiting (process_t *p)
{
  int got_pid;
  int status;
  while ((got_pid = waitpid(p->pid, &status, WUNTRACED))) 
  {
    /* go to sleep until something happens */
    /* wait woke up */
    if (got_pid == p->pid)
    {
      break;
    }
                 
    if ((got_pid == -1) && (errno != EINTR)) 
    {
    /* an error other than an interrupted system call */
    printf("#####child process not exist\n");
    p->completed=true;
    break;
    }
  }

    if( p->completed != true) 
    {
     p->status = status;

     if (WIFEXITED(status)) /* process exited normally */
     {
      printf("child process exited with value %d\n", WEXITSTATUS(status));
      p->completed=true;
     }
    else if (WIFSIGNALED(status)) /* child exited on a signal (ctrl c) */
     {
      printf("child process exited due to signal %d\n", WTERMSIG(status));
      p->completed=true;
     }

    else if (WIFSTOPPED(status)) /* child was stopped/suspended (ctrl z) */
     {
      printf("child process was stopped by signal %d\n", WIFSTOPPED(status));
      p->stopped=true;
     }
   }
}

/* Creates the context for a new child by setting the pid, pgid and tcsetpgrp */
void new_child(job_t *j, process_t *p, bool fg)
{
         /* establish a new process group, and put the child in
          * foreground if requested
          */

         /* Put the process into the process group and give the process
          * group the terminal, if appropriate.  This has to be done both by
          * the dsh and in the individual child processes because of
          * potential race conditions.  
          * */

  p->pid = getpid();

         /* also establish child process group in child to avoid race (if parent has not done it yet). */
  set_child_pgid(j, p);

  if(fg) // if fg is set
		seize_tty(j->pgid); // assign the terminal

         /* Set the handling for job control signals back to the default. */
  signal(SIGTTOU, SIG_DFL);
}

/* Spawning a process with job control. fg is true if the 
 * newly-created process is to be placed in the foreground. 
 * (This implicitly puts the calling process in the background, 
 * so watch out for tty I/O after doing this.) pgid is -1 to 
 * create a new job, in which case the returned pid is also the 
 * pgid of the new job.  Else pgid specifies an existing job's 
 * pgid: this feature is used to start the second or 
 * subsequent processes in a pipeline.
 * */

void spawn_job(job_t *j, bool fg) 
{

  pid_t pid;
  process_t *p;
  for(p = j->first_process; p; p = p->next) {

	  /* YOUR CODE HERE? */
	  /* Builtin commands are already taken care earlier */

    switch (pid = fork()) {
      // int status;

      case -1: /* fork failure */
        perror("fork");
        exit(EXIT_FAILURE);

      case 0: /* child process  */
        p->pid = getpid();	    
        new_child(j, p, fg);

	    /* YOUR CODE HERE?  Child-side code for new process. */

        if (p->ifile != NULL)
        {
          int fd = open(p->ifile, O_RDONLY);
          dup2 (fd, STDIN_FILENO);
        }

        if (p->ofile != NULL)
        {
          int fd = open(p->ofile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
          dup2 (fd, STDOUT_FILENO);
        }

        printf("%d (Launched): %s\n", j->pgid, j->commandinfo);

        execvp(p->argv[0], p->argv);
        perror("New child should have done an exec");
        exit(EXIT_FAILURE);  /* NOT REACHED */
        break;    /* NOT REACHED */

      default: /* parent */
        /* establish child process group */
        p->pid = pid;
        set_child_pgid(j, p);

        /* YOUR CODE HERE?  Parent-side code for new process.  */
        // waitpid (pid, &status, 0);
        waiting(p);
        //printf("P completed: %d", p->completed);
        //printf("P Stopped: %d", p->stopped);
        //wait(NULL);
        //p->completed = true;

        //wait for child to finish
    }

    /* YOUR CODE HERE?  Parent-side code for new job.*/

	  seize_tty(getpid()); // assign the terminal back to dsh

  }
}

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j) 
{
  if(kill(-j->pgid, SIGCONT) < 0)
    perror("kill(SIGCONT)");
}


/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 * it immediately.  
 */
bool builtin_cmd(job_t *last_job, int argc, char **argv) 
{

	    /* check whether the cmd is a built in command
        */

  if (!strcmp("quit", argv[0])) {
    /* Your code here */
    exit(EXIT_SUCCESS);
  }
  else if (!strcmp("jobs", argv[0])) {
    /* Your code here */
    job_t* currentJob = active_jobs_head;
    job_t* deletedJob = NULL;

    while(currentJob != NULL) {
      // print active jobs and then change notified to 0

      //if((currentJob->first_process)->completed == true && currentJob->notified == false) {

      //need to check if every process has completed for a job to be complete, not just the first process
      if (job_is_completed(currentJob) == true)
      {
        printf("%d (Completed): %s\n", currentJob->pgid, currentJob->commandinfo);
        deletedJob = currentJob;

         //currentJob->notified = true;
      }
      //otherwise it is stopped
      else
      {
        printf("%d (Stopped): %s\n", currentJob->pgid, currentJob->commandinfo);
      }

      currentJob = currentJob->next;

      // delete job after it is completed, don't need to save notified 
     
      if (deletedJob != NULL) 
      {
        if (deletedJob == active_jobs_head)
        {
          active_jobs_head = deletedJob->next;
          free_job(deletedJob);
        }
        else
        {
          delete_job(deletedJob, active_jobs_head);
        }
        deletedJob = NULL;
      }
    }
    return true;
  }

  else if (!strcmp("cd", argv[0])) {
    chdir(argv[1]);
    return true;
  }
  else if (!strcmp("bg", argv[0])) {
    /* Your code here */
  }
  else if (!strcmp("fg", argv[0])) {
    /* Your code here */
    if(argv[1] == NULL) {
      // continue most recent stopped job
      //use find_last_job
    }
    else {
      pid_t job_number = atoi(argv[1]);
      job_t* currentJob = active_jobs_head;

      
      while(currentJob != NULL) {
        // Need to eventually iterate through all processes?
        
        if((currentJob->first_process)->stopped == true && currentJob->pgid == job_number) {
          //seize_tty(currentJob->pgid);
          continue_job(currentJob); 
          seize_tty(currentJob->pgid);
          waiting(currentJob->first_process); //TBD need to change when implementing pipes, since not always 1st process
          seize_tty(getpid());
          break;
        }
        else if (currentJob->pgid == job_number) {
          printf("%s\n", "This process wasn't stopped`");
        }
        else {
          printf("%s\n", "This job number is not the requested one");
        }
        currentJob = currentJob->next;
      }
    }
    return true;
  }
  /* not a builtin command */
  return false;
}

/* Build prompt messaage */
// char* promptmsg() 
// {
//   // Modify this to include pid 
// 	return "dsh$ ";
// }

char* promptmsg(pid_t dsh_pgid) 
{
  /* Modify this to include pid */
  char msg[80];
  sprintf(msg, "dsh-%d$ ", dsh_pgid);
  char* result = msg;
  return result;
}

int main() 
{
	init_dsh();
	DEBUG("Successfully initialized\n");

	while(1) {
    job_t *j = NULL;
    if(!(j = readcmdline(promptmsg(getpid())))) {
			if (feof(stdin)) { /* End of file (ctrl-d) */
        fflush(stdout);
        printf("\n");
        exit(EXIT_SUCCESS);
      }
			continue; /* NOOP; user entered return or spaces with return */
    }

        /* Only for debugging purposes to show parser output; turn off in the
         * final code */
    if(PRINT_INFO) print_job(j);

        /* Your code goes here */
        /* You need to loop through jobs list since a command line can contain ;*/
        /* Check for built-in commands */
        /* If not built-in */
            /* If job j runs in foreground */
            /* spawn_job(j,true) */
            /* else */
            /* spawn_job(j,false) */

    while (j != NULL)
    {
      // if EOF builtincmd of quit
      //active jobs is last job?

      // if not a command at all ?
      bool is_built_in_cmd = builtin_cmd(j, (j->first_process)->argc, (j->first_process)->argv);
      if(!is_built_in_cmd) 
      {
        spawn_job(j, true);

        if(active_jobs_head == NULL) 
        {
        active_jobs_head = j;
        //active_jobs = j;
        }

        else 
        {
        job_t *lastJob = find_last_job(active_jobs_head);
        lastJob->next = j;
        j->next = NULL;
        //active_jobs->next = j;
        //active_jobs = active_jobs->next;
        }
      }

      j = j->next;
    }
  }
}
