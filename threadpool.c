//Barak Levy
//ID 203280185
//thread pool 

#include "threadpool.h"
#include <unistd.h>
#include <stdio.h> 
#include <string.h> 
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h> 
#include <pthread.h> 
/*
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check 
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4. create the threads, the thread init function is do_work and its argument is the initialized threadpool. 
 */

threadpool* create_threadpool(int num_threads_in_pool)
{
    if(num_threads_in_pool<1||num_threads_in_pool>MAXT_IN_POOL)
    {
    
        return NULL;
    }
    threadpool *threadPl=( threadpool*)malloc(sizeof(threadpool));
    if(threadPl==NULL)
    {
         
        return NULL;
    }
    threadPl->qsize = 0;
    threadPl->num_threads = num_threads_in_pool;
    threadPl->qhead = NULL,threadPl->qtail = NULL;
    threadPl->shutdown =0,threadPl->dont_accept = 0;
    pthread_mutex_init(&threadPl->qlock, NULL);
    pthread_cond_init(&threadPl->q_not_empty, NULL);
    pthread_cond_init(&threadPl->q_empty, NULL);
    threadPl->threads = (pthread_t*)malloc (sizeof(pthread_t)*num_threads_in_pool);
    if(threadPl->threads==NULL)
    {
        free(threadPl);
        return NULL;
    }
    for(int i=0;i<num_threads_in_pool;i++)
    {
        int check = (pthread_create(&threadPl->threads[i],NULL,do_work,threadPl));
        if(check<0)
        {
            return NULL;
        }
      
    }    
    return threadPl;                                                                
   
}
/**
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine
 */
void* do_work(void* p)
{
    threadpool * tempPool = (threadpool*)p;
    
    while(true)
    {
        pthread_mutex_lock(&tempPool->qlock); 
        if(tempPool->shutdown==1)
        {
            pthread_mutex_unlock(&tempPool->qlock);
            return NULL;
        }
        if(tempPool->qsize==0)
        {
            pthread_cond_wait(&tempPool->q_not_empty,&tempPool->qlock);
         
        }
       
        if(tempPool->shutdown==1)
        {
            pthread_mutex_unlock(&tempPool->qlock);
             return NULL;
        }
        work_t *tempWork = tempPool->qhead;
        if(tempWork==NULL)
        {
             pthread_mutex_unlock(&tempPool->qlock);
             continue;
        }
        tempPool->qhead = (tempPool->qhead)->next; 
        tempPool->qsize--; 
      
        if((tempPool->qsize==0)&&(tempPool->dont_accept==1))
        {
             pthread_mutex_unlock(&tempPool->qlock);
             pthread_cond_signal(&tempPool->q_empty);
        }
        pthread_mutex_unlock(&tempPool->qlock);
   
        //handle and free work
        int x = tempWork->routine(tempWork->arg);
        free(tempWork);
      
    }
}

/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 * this function should:
 * 1. create and init work_t element
 * 2. lock the mutex
 * 3. add the work_t element to the queue
 * 4. unlock mutex
 *
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
    pthread_mutex_lock(&from_me->qlock);
    if(from_me->dont_accept==1)
    { 
        pthread_mutex_unlock(&from_me->qlock);
        //destroy has begon return;
        return;
    }
    work_t *newWork = (work_t*)malloc(sizeof(work_t));
    if(newWork==NULL)
    {
        pthread_mutex_unlock(&from_me->qlock);
        //destroy has begon return;
        return;
    }
    newWork->arg =arg;
    newWork->routine = dispatch_to_here;
    newWork->next =NULL;
    if(from_me->qsize==0)
    {
        from_me->qtail = newWork;
        from_me->qhead = from_me->qtail;     
    }
    else
    {
        from_me->qtail->next = newWork;
        from_me->qtail = newWork;
    }
    
    from_me->qsize++;
    pthread_mutex_unlock(&from_me->qlock);
    pthread_cond_signal(&from_me->q_not_empty);
 

}
/** 
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme)
{

    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept = 1;
    if(destroyme->qsize>0)
    {
        pthread_cond_wait(&destroyme->q_empty,&destroyme->qlock);
    }
    destroyme->shutdown = 1;
    pthread_mutex_unlock(&destroyme->qlock);
    pthread_cond_broadcast(&destroyme->q_not_empty);
    
    for(int i =0;i<destroyme->num_threads;i++)
    {
        pthread_join(destroyme->threads[i],NULL);
    }
    //free whatever you have to free
    free(destroyme->threads);
    free(destroyme);
    
}







