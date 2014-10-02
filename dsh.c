
#include "dsh.h"
#include <signal.h>

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
    if (got_pid == p->pid)
    {
      break;
    }
                 
    if ((got_pid == -1) && (errno != EINTR)) 
    {
    /* an error other than an interrupted system call */
    //printf("#####child process not exist\n");
    p->completed=true;
    break;
    }
  }

    if( p->completed != true) 
    {
     p->status = status;

     if (WIFEXITED(status)) /* process exited normally */
     {
      //printf("child process exited with value %d\n", WEXITSTATUS(status));
      p->completed=true;
     }
    else if (WIFSIGNALED(status)) /* child exited on a signal (ctrl c) */
     {
      //printf("child process exited due to signal %d\n", WTERMSIG(status));
      p->completed=true;
     }

    else if (WIFSTOPPED(status)) /* child was stopped/suspended (ctrl z) */
     {
      //printf("child process was stopped by signal %d\n", WIFSTOPPED(status));
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

 void closePipe(int pipefd[][2], int numberProcess)
 {
  int counter;

  for(counter = 0; counter < (numberProcess-1); counter++)
  {
    close(pipefd[counter][0]);
    close(pipefd[counter][1]);
  }
 }

 void sighandler(int sig, siginfo_t *sip, void *notused)
 {
    int status;
    process_t *p;
    job_t *j;
    bool breakLoop;

    //printf ("The process generating the signal is PID: %d\n", sip->si_pid);
    fflush (stdout);
    status = 0;

/* The WNOHANG flag means that if there's no news, we don't wait*/
    if (sip->si_pid == waitpid (sip->si_pid, &status, WNOHANG))
    {
      for(job_t *myJobs = active_jobs_head; myJobs; myJobs = myJobs->next)
      {
        for(process_t *myProcess = myJobs->first_process; myProcess; myProcess = myProcess->next)
        {
          if(sip == myProcess->pid)
          {
            p = myProcess;
            j = myJobs;
            breakLoop = true;
            break;
          }
        }
        if(breakLoop)
        {
          break;
        }
    }

           if( j == NULL)
              return;
           //printf("find job %s\n",j->commandinfo );
           if( j->bg == false)
                  return;

           /* A SIGCHLD doesn't necessarily mean death - a quick check */
           if (WIFEXITED(status)|| WTERMSIG(status))
           {
              //printf ("The child is gone\n"); /* dead */
              p->completed=true;
           }
           else if (WIFSTOPPED(status)) /* child was stopped */
           {
              //printf ("The child is stoped\n"); /* dead */
              p->stopped=true;
           }
           else if (WIFSIGNALED(status))
           {
                       //printf("#####child process not exist, ctrl-z\n");
                       p->stopped=true;
           }
    }


}
 

void spawn_job(job_t *j, bool fg) 
{

  pid_t pid;
  process_t *p;
  int countProcess = 0;
  int loopCounter = -1;
  int i;


  //count number of processes in the job
  for(p = j->first_process; p; p = p->next)
  {
    countProcess++;
  } 

  //printf("count process: %d \n", countProcess);

  int pipefd[countProcess-1][2]; //initialize 2-dimensional array [number of process][input/output]
  //printf("test");

  //open all pipes
  if(countProcess > 1)
  {
    for(i = 0; i < (countProcess-1); i++)
    {
      //printf("piped %d \n", i);
      pipe(pipefd[i]);
    }
  }


  for(p = j->first_process; p; p = p->next) {

	  /* YOUR CODE HERE? */
	  /* Builtin commands are already taken care earlier */
 
    loopCounter++;

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
          dup2(fd, STDIN_FILENO);
        }

        if (p->ofile != NULL)
        {
          int fd = open(p->ofile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
          dup2(fd, STDOUT_FILENO);
        }

        //pipe
        if(countProcess > 1)
        {
          //printf("begin piping \n");
          //first process is output        

          //need to loop through number of process and close all pipes
          if(p == j->first_process)
          {

            dup2(pipefd[loopCounter][1], 1); //duplicate output as output for next process 
            closePipe(pipefd, countProcess);

          }  

          //last process
          else if (p->next == NULL)
          {
            dup2(pipefd[loopCounter-1][0], 0);
            closePipe(pipefd, countProcess);

          }

          //for middle: dup both input and output
          
          else
          {
            dup2(pipefd[loopCounter][1], 1);
            dup2(pipefd[loopCounter-1][0], 0);

            closePipe(pipefd, countProcess);


          }

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


        if (p->next == NULL) //if last process
        {
          process_t *p2;

          p2 = j->first_process;
          while(p2 != NULL)
          {
            //printf("waiting for process \n");

            //if fg true waiting, else bg stuff

            if(fg == true)
            {
              waiting(p2);
            }
            else
            {
              struct sigaction action;
              action.sa_sigaction = sighandler;

              sigfillset(&action.sa_mask);
              action.sa_flags = SA_SIGINFO;
              sigaction(SIGCHLD, &action, NULL);
            }
            
            closePipe(pipefd, countProcess);


            p2 = p2->next;
          }
        }

        //wait for child to finish
    }

    /* YOUR CODE HERE?  Parent-side code for new job.*/
    //printf("assign terminal back to dsh\n");
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
      else if (job_is_stopped(currentJob) == true)
      {
        printf("%d (Stopped): %s\n", currentJob->pgid, currentJob->commandinfo);
      }

      else
      {
        printf("%d (Running): %s\n", currentJob->pgid, currentJob->commandinfo);
      }

      currentJob = currentJob->next;

      // delete job after it is completed, don't need to save notified 
     
      if (deletedJob != NULL) 
      {
        if (deletedJob == active_jobs_head)
        {
          active_jobs_head = deletedJob->next;
          free_job(deletedJob); //TBD warning about this?
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
    int chdir_return = chdir(argv[1]);
    if(chdir_return == -1) {
      printf("cd: %s: No such file or directory\n", argv[1]);
    }
    return true;
  }
  else if (!strcmp("bg", argv[0])) {
    /* Your code here */
    job_t* currentJob = active_jobs_head;
    process_t* p2;

    if(argv[1] == NULL) 
    {
      // continue most recent stopped job

      //use find_last_job
      currentJob = find_last_job(active_jobs_head);

      continue_job(currentJob);
      seize_tty(currentJob->pgid);


      p2 = currentJob->first_process;

      while(p2 != NULL)
      {
        p2->stopped = false;
        
        struct sigaction action;
        action.sa_sigaction = sighandler;

        sigfillset(&action.sa_mask);
        action.sa_flags = SA_SIGINFO;
        sigaction(SIGCHLD, &action, NULL);

        p2 = p2->next;
      }
      seize_tty(getpid());
    }

    else 
    {
      pid_t job_number = atoi(argv[1]);

      
      while(currentJob != NULL) {
        // Need to eventually iterate through all processes?
        
        if((job_is_stopped(currentJob)) && currentJob->pgid == job_number) {
          //seize_tty(currentJob->pgid);
          continue_job(currentJob); 
          seize_tty(currentJob->pgid);

          p2 = currentJob->first_process;
          while (p2 != NULL)
          {
            p2->stopped = false; 

            struct sigaction action;
            action.sa_sigaction = sighandler;

            sigfillset(&action.sa_mask);
            action.sa_flags = SA_SIGINFO;
            sigaction(SIGCHLD, &action, NULL);
            p2 = p2->next;
          }

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


  else if (!strcmp("fg", argv[0])) {
    /* Your code here */
    job_t* currentJob = active_jobs_head;
    process_t* p2;

    if(argv[1] == NULL) {
      // continue most recent stopped job

      //use find_last_job
      currentJob = find_last_job(active_jobs_head);
      continue_job(currentJob);
      seize_tty(currentJob->pgid);


      p2 = currentJob->first_process;

      while(p2 != NULL)
      {
        waiting(p2);
        p2 = p2->next;
      }
      seize_tty(getpid());
    }
    else {
      pid_t job_number = atoi(argv[1]);

      
      while(currentJob != NULL) {
        
        
        if((job_is_stopped(currentJob)) && currentJob->pgid == job_number) {
          //seize_tty(currentJob->pgid);
          continue_job(currentJob); 
          seize_tty(currentJob->pgid);

          p2 = currentJob->first_process;
          while (p2 != NULL)
          {
            waiting(p2);
            p2 = p2->next;
          }
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
    // if(PRINT_INFO) print_job(j);

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

        if(j->bg == true)
        {
          spawn_job(j, false);
        }
        else
        {
          spawn_job(j, true);
        }

        if(active_jobs_head == NULL) 
        {
        active_jobs_head = j;
        
        }

        else 
        {
        job_t *lastJob = find_last_job(active_jobs_head);
        lastJob->next = j;
        j->next = NULL;

        }
      }

      j = j->next;
    }
  }
}
    