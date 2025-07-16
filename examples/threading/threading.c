#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* my_thread_param = (struct thread_data *) thread_param;

    usleep(my_thread_param->thread_wait_obtain);
    int ret = pthread_mutex_lock(my_thread_param->thread_mutex);
    if (ret != 0)
    {
        ERROR_LOG("pthread_mutex_lock failed with %d", ret);
    }
    else
    {
        usleep(my_thread_param->thread_wait_release);
        int ret = pthread_mutex_unlock(my_thread_param->thread_mutex);
        if (ret != 0)
        {
            ERROR_LOG("pthread_mutex_unlock failed with %d", ret);
        }
    }
    return my_thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data *threadfunc_data = malloc(sizeof(struct thread_data));
    if (!threadfunc_data)
    {
        ERROR_LOG("struct thread_data malloc failed");
        return false;
    }

    threadfunc_data->thread_id = thread;
    threadfunc_data->thread_wait_obtain = wait_to_obtain_ms;
    threadfunc_data->thread_wait_release = wait_to_release_ms;
    threadfunc_data->thread_mutex = mutex;
    threadfunc_data->thread_complete_success = true;

    int ret = pthread_create(threadfunc_data->thread_id, NULL, threadfunc, threadfunc_data);
    if (ret != 0)
    {
        ERROR_LOG("pthread_create failed");
        free(threadfunc_data);
        return false;
    }
    return threadfunc_data->thread_complete_success;
}

