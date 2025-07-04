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
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int rc = 0;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    thread_func_args->thread_complete_success = true;

    usleep(thread_func_args->wait_to_obtain_ms * 1000);
    rc = pthread_mutex_lock(thread_func_args->mutex);
    if(rc != 0) {
        printf("pthread_mutex_lock failed wiht %d\n", rc);
        thread_func_args->thread_complete_success = false;
    } else {
        usleep(thread_func_args->wait_to_release_ms * 1000);
        rc = pthread_mutex_unlock(thread_func_args->mutex);
        if(rc != 0) {
            printf("pthread_mutex_unlock failed wiht %d\n", rc);
            thread_func_args->thread_complete_success = false;
        }
    }
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    int rc = 0;

    struct thread_data* thread_param = malloc(sizeof(struct thread_data));
    if(thread_param == NULL) {
        printf("failed to allocate memory");
        return false;
    }
    thread_param->mutex = mutex;
    thread_param->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_param->wait_to_release_ms = wait_to_release_ms;
    thread_param->thread_complete_success = false;

    rc = pthread_create(thread, NULL, threadfunc, thread_param);
    if(rc != 0) {
        printf("pthread_create failed wiht %d\n", rc);
        return false;
        free(thread_param);
    }

    return true;
}
