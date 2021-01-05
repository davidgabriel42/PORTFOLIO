/*
 *
 * CS 446.646 Project 1 (Pthreads)
 *
 * Compile with --std=c99
 */

#include <limits.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#define MAX_WORKER_THREADS 7
/*state vector implemented as struct of volatiles*/
enum thread_state { failed = 2, working = 1, avail = 0};
struct PAR_SUM_STATE
{
pthread_t volatile tid[MAX_WORKER_THREADS]; //thread id | index used to map other vectors
int  volatile job_count;
int  volatile thread_count;
int* volatile data;
long volatile worker_input_vector[MAX_WORKER_THREADS];
long volatile worker_output_vector[MAX_WORKER_THREADS];
//bool volatile worker_input_semaphore[MAX_WORKER_THREADS];
long volatile sum;
long volatile odd;
long volatile min;
long volatile max;
bool volatile done;
enum thread_state worker_thread_state[MAX_WORKER_THREADS];
} state;
/*"lock" is used to synchronize changes to state.*/
pthread_mutex_t lock;
// function prototypes
long calculate_square(long number);
void init_state(struct PAR_SUM_STATE * state_ptr);
void worker_thread(void* state);
void copy_state(struct PAR_SUM_STATE * state_from, struct PAR_SUM_STATE * state_to);
int next_avail_thread(struct PAR_SUM_STATE * state, int n_threads);
//functions
long calculate_square(long number)
{
  long the_square = number * number;
  sleep(number);
  return the_square;
}
//initialize state p thread conditionals
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
  state_ptr->min = 0;
  state_ptr->max = 0;
  state_ptr->done = 0;
}
//retreieve index from pthread_t vector which maps to thread state
int find_thread(struct PAR_SUM_STATE * state, pthread_t tid)
{
  #ifdef DEBUG
  printf("this_tid:%d\n",tid);
  #endif
  for (int i = 0; i<MAX_WORKER_THREADS; i++)
  {
    #ifdef DEBUG
    printf("tid-%d\tindex-%d\n",state->tid[i],i);
    #endif
    if(state->tid[i] == tid)
    {
      printf("\nthread found\n");
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

//worker thread function
void worker_thread(void* state_in)
{
  struct PAR_SUM_STATE *state_ptr = (struct PAR_SUM_STATE*)state_in;
  struct PAR_SUM_STATE *state_copy_ptr;
  struct PAR_SUM_STATE state_thread_copy;
  pthread_t tid = pthread_self();
  long result;
  int i;
  state_copy_ptr = &state_thread_copy;
//grab state
  pthread_mutex_unlock(&lock); //critical
  *state_copy_ptr = *state_ptr;
  //memcpy(&state_thread_copy, &state_in, sizeof(state_thread_copy));
  pthread_mutex_unlock(&lock);//non-critical
//parallel section
  i = find_thread(&state_thread_copy, tid);

  #ifdef DEBUG
//  for(int j = 0; j < state_thread_copy. 
  printf("\nInside thread:%d \nindex:%d\tinput:%d\tresult:%d\n",\
(unsigned int)tid,i,state_thread_copy.worker_input_vector[i],result);
  #endif


  result = calculate_square(state_thread_copy.worker_input_vector[i]);
//stats
/*!!clean up , use local state
  sum += the_square;
  if (number % 2 == 1) {odd++;}
  if (number < min) {
    min = number;
  }

  // and what was the biggest one?
  if (number > max) {
    max = number;
  }
*/
//push state back
  pthread_mutex_unlock(&lock); //critical
  pthread_mutex_unlock(&lock);//non-critical

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
  pthread_attr_t attr; //thread attrs
  struct PAR_SUM_STATE *state_ptr = &state;

  #ifdef DEBUG
  printf("Scanning file %s\nNumber of threads: %i\n",fn, n_threads );
  #endif

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


  #ifdef DEBUG
  printf("init'd\n" );
  #endif

  //block on mutex, then spawn thread and release mutex
  while (fscanf(fin, "%c %ld\n", &action, &num) == 2) 
  {
      if (action == 'p') 
    {// process, do some work
      #ifdef DEBUG
      printf("p:%d\n",num);
      #endif

      pthread_mutex_lock(&lock); //critical
      #ifdef DEBUG
      printf("lock acquired\n" );
      #endif
      if(state_ptr->thread_count < n_threads)
      {
        state_ptr->thread_count += 1;
        k = next_avail_thread(state_ptr, n_threads);
        //add worker operands to state and init worker thead
        #ifdef DEBUG
        printf("Thread index %d available.\n",k);
        #endif
        state_ptr->worker_input_vector[k] = num;
        state_ptr->worker_thread_state[k] = working;
        pthread_create(&state.tid[k],  NULL, &worker_thread\
, pthread_arg);
        #ifdef DEBUG
        for (int l = 0; l < MAX_WORKER_THREADS; l++)
        {
          printf("\ntid-%d\tstate-%d",(unsigned int)state_ptr->tid[l],\
state_ptr->worker_thread_state[l] );
        }
        #endif
      }
    pthread_mutex_unlock(&lock); //non critical

    #ifdef DEBUG
    printf("lock released\n" );
    #endif

    if(state_ptr->thread_count == n_threads){break;}
    }
    else if (action == 'w') 
    {     // wait, nothing new happening
      #ifdef DEBUG
      printf("w:%d\n",num);
      #endif
      sleep(num);
    }
    else 
    {
        printf("ERROR: Unrecognized action: '%c'\n", action);
        exit(EXIT_FAILURE);
    }
  }
  fclose(fin);

  while (1)
  {
    printf(".");
    sleep(1);
  }


  // assert (pthread_attr_init(&attr));
  #ifdef DEBUG
  printf("debug: Created pthread" );
  #endif
  //assert (pthread_create(&tid, &attr, (void*)calculate_square, (void*)num) == 0);
  #ifdef DEBUG
//  printf(" %s\n",tid );
  #endif

//  assert (pthread_create(&tid, &attr, calculate_square, argv[1])==0);
//  assert (pthread_join(tid,NULL)==0);

  fclose(fin);

// print results
//  printf("%ld %ld %ld %ld\n", sum, odd, min, max);

  
  while(1)
  {
    printf(".");
    sleep(1);
    #ifdef DEBUG
//    printf("\ndebug:threads launched = %d\n", state->thread_count );
    #endif

  }

  // clean up and return
  return (EXIT_SUCCESS);
}
