/*
 *
 * CS 446.646 Project 1 (Pthreads)
 *
 * Compile with --std=c99
 * Author:David Gabriel
 * contact@david-gabriel.com
 * Multithreaded implementation of sum.c with task queue
 * Max threads is hardcoded and can affect times on high performance
 * systems. Suggest recompile with bespoke value if this is limiting
 */

#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#define MAX_WORKER_THREADS 1500
/*state vector implemented as struct of volatiles*/
enum thread_state {complete = 3, failed = 2, working = 1, avail = 0};
struct PAR_SUM_STATE
{
pthread_t volatile tid[MAX_WORKER_THREADS];
int  volatile job_count;
int  volatile thread_count;
int* volatile data;
long volatile worker_input_vector[MAX_WORKER_THREADS];
long volatile worker_output_vector[MAX_WORKER_THREADS];
long volatile sum;
long volatile odd;
long volatile min;
long volatile max;
bool volatile done;
enum thread_state worker_thread_state[MAX_WORKER_THREADS];
} state;
const struct timespec rem,req = {0,1000};

/*"lock" is used to synchronize changes to state.*/
pthread_mutex_t lock;
// function prototypes
long calculate_square(long number);
void init_state(struct PAR_SUM_STATE * state_ptr);
void worker_thread(void* state);
void copy_state(struct PAR_SUM_STATE * state_from, struct PAR_SUM_STATE * state_to);
int next_avail_thread(struct PAR_SUM_STATE * state, int n_threads);
void spawn_thread(struct PAR_SUM_STATE * state,int i,pthread_t tid);
//functions
int nanosleep(const struct timespec *req, struct timespec *rem);
long calculate_square(long number)
{
  long the_square = number * number;
  sleep(number);
  return the_square;
}
//initialize state p thread conditional variables and accumulators
void init_state(struct PAR_SUM_STATE * state_ptr)
{
  state_ptr->job_count = 0;
  state_ptr->thread_count= 0;
  state_ptr->data = NULL;
  for (int i = 0; i<MAX_WORKER_THREADS; i++)
  {
      state_ptr->worker_input_vector[i] = 0;
      state_ptr->worker_output_vector[i] = 0;
      state_ptr->worker_thread_state[i] = avail;
  }
  state_ptr->sum = 0;
  state_ptr->odd = 0;
  state_ptr->min = LONG_MAX;
  state_ptr->max = 0;
  state_ptr->done = 0;
}
//retreieve index from pthread_t vector which maps to thread state
int find_thread(struct PAR_SUM_STATE * state, pthread_t tid)
{
  printf("this_tid:%d\n",tid);
  for (int i = 0; i<MAX_WORKER_THREADS; i++)
  {
    printf("tid-%d\tindex-%d\n",state->tid[i],i);
    if(state->tid[i] == tid)
    {
      printf("Thread found\n");
      return i;
    }
  }
  return -1;
}
//find ready thread
int next_avail_thread(struct PAR_SUM_STATE * state,int n_threads)
{
  for (int i = 0; i<n_threads; i++)
  {
  if(state->worker_thread_state[i] == avail){return i;}
  }
  return -1;
}
void spawn_thread(struct PAR_SUM_STATE * state,int i,pthread_t tid)
{
}
//worker thread function
void worker_thread(void* state_in)
{
  struct PAR_SUM_STATE *state_ptr = (struct PAR_SUM_STATE*)state_in;
  struct PAR_SUM_STATE *state_copy_ptr;
  struct PAR_SUM_STATE state_thread_copy;
  pthread_t tid = pthread_self();
  long result;
  int i;
  int odd =0;
  state_copy_ptr = &state_thread_copy;
//grab state
  pthread_mutex_unlock(&lock); //critical
  *state_copy_ptr = *state_ptr;//this is the most annoying/worst part
//of c syntax I can recently recall. Deep copy/shallow copy syntax 
//should be inverted IMO
  pthread_mutex_unlock(&lock);//non-critical
//parallel section
  i = find_thread(&state_thread_copy, tid);
  result = calculate_square(state_thread_copy.worker_input_vector[i]);

  printf("\nInside thread:%d \nindex:%d\tinput:%d\tresult:%d\n",\
(unsigned int)tid,i,state_thread_copy.worker_input_vector[i],result);


//push state back
  pthread_mutex_unlock(&lock); //critical
  state_ptr->worker_output_vector[i] = result;
  if(state_thread_copy.worker_input_vector[i] %2==1){  state_ptr->odd += 1;}

  if(state_thread_copy.worker_input_vector[i]> state_ptr->max){state_ptr->max\
 = state_thread_copy.worker_input_vector[i];}

  if(state_thread_copy.worker_input_vector[i]< state_ptr->min){state_ptr->min\
 = state_thread_copy.worker_input_vector[i];}

  state_ptr->worker_thread_state[i] = complete;
  pthread_mutex_unlock(&lock);//non-critical
  return NULL;
}

//MAIN---------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
  // check and parse command line options
  if (argc != 3)
  {
    printf("Usage: sumsq <infile> <n_threads>\n");
    exit(EXIT_FAILURE);
  }

  init_state(&state);
  int  k = 0;
  char *fn = argv[1];
  int n_threads;
  n_threads =atoi( argv[2]);
  pthread_attr_t attr;
  struct PAR_SUM_STATE *state_ptr = &state;
  printf("Scanning file %s\nNumber of threads: %i\n",fn, n_threads );
  if ((int)n_threads > MAX_WORKER_THREADS) 
  {
    printf("<n_threads> > MAX_THREADS\n");
    exit(EXIT_FAILURE);
  }

  // load numbers and add them to the queue
  FILE* fin = fopen(fn, "r");
  char action;
  long num;
  void *pthread_arg = &state;
//  #ifdef DEBUG
  printf("Initialized\n" );
//  #endif
  //iterate on file input
  while (fscanf(fin, "%c %ld\n", &action, &num) == 2) 
  {
  //check for complete threads
  for (int l = 0; l < MAX_WORKER_THREADS; l++)
    {
      if (state_ptr->worker_thread_state[l] == complete)
      {
        state_ptr->sum += state_ptr->worker_output_vector[l];
        state_ptr->worker_thread_state[l] = avail;
        state_ptr->thread_count --;
        printf("\t\t\tsum: %d\n",state_ptr->sum);
      }
    }
    //block on mutex, then spawn thread and release mutex, contains
    //reentry
    if (action == 'p') 
    {// process, do some work
      printf("p:%d\n",num);
      pthread_mutex_lock(&lock); //critical
      printf("lock acquired\n" );
      if(state_ptr->thread_count < n_threads)
      {
      jump: //always enter holding lock
      state_ptr->thread_count += 1;
      k = next_avail_thread(state_ptr, n_threads);
      //add worker operands to state and init worker thead
      printf("Thread index %d available.\n",k);
      state_ptr->worker_input_vector[k] = num;
      state_ptr->worker_thread_state[k] = working;
      pthread_create(&state.tid[k],  NULL, &worker_thread\
,pthread_arg);
        printf("releasing lock\n" );
        pthread_mutex_unlock(&lock); //non critical
      }
      //handle out of threads condition, terminates with rentry
      else
      {
        printf("All threads allocated, master waiting.\n");
        while(1)
        {
          for (int m = 0; m < MAX_WORKER_THREADS; m++)
          {
            if(state_ptr->worker_thread_state[m] == complete)
            {
              state_ptr->sum += state_ptr->worker_output_vector[m];
              state_ptr->worker_thread_state[m] = avail;
              state_ptr->thread_count --;
              printf("\t\t\tsum: %d\n",state_ptr->sum);
            }
          }

          pthread_mutex_unlock(&lock);//non critical
          nanosleep(&req, &rem);
          pthread_mutex_lock(&lock); //critical
          if(state_ptr->thread_count< n_threads)
          {
            goto jump;
          }
        }
      }
    }
    else if (action == 'w') 
    {     // wait, nothing new happening
      sleep(num);
    }
    else 
    {
        printf("ERROR: Unrecognized action: '%c'\n", action);
        exit(EXIT_FAILURE);
    }
  }
  fclose(fin);
  printf("Task queue empty\n");
//wait to complete
  bool finished = 0;
  while(1)
  {
    if(finished){break;}
    for (int m = 0; m < MAX_WORKER_THREADS; m++)
    {
      pthread_mutex_lock(&lock); //critical
      if(state_ptr->worker_thread_state[m] == complete)
        {
        printf("%d,%d\n", m,state_ptr->worker_thread_state[m]);
        state_ptr->sum += state_ptr->worker_output_vector[m];
        state_ptr->worker_thread_state[m] = avail;
        state_ptr->thread_count --;
        printf("\t\t\tsum: %d\n",state_ptr->sum);
        }
      if(state_ptr->thread_count == 0)
      {
        state_ptr->done = 1;
        finished = 1;
        pthread_mutex_unlock(&lock);
        break;
      }
      pthread_mutex_unlock(&lock); //non critical
    }
  }
  //display results
  printf("Completed\nsum:%ld odd:%ld min:%ld max:%ld\n"\
, state_ptr->sum, state_ptr->odd, state_ptr->min, state_ptr->max);
  return (EXIT_SUCCESS);
}
