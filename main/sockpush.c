/* Copyright 2017, JP Norair
  *
  * Licensed under the OpenTag License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  */

#include "sockpush.h"
#include "debug.h"

#include <talloc.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>



#ifndef UNIX_PATH_MAX
#   define UNIX_PATH_MAX    104
#endif


#define SP_MAX_READERS      1
#define SP_MAX_SUBSCRIBERS  1


// Internal data types.  May change at any time.
// In .h file for hacking purposes only
// ---------------------------------------------------------------------------
typedef struct {
    void*           parent;
    unsigned int    last_read;
} sprdr_t;


typedef struct {
    int             flags;
    void*           parent;
    sp_action_t     action;
    uint8_t*        buf;
    size_t          max;
} spsubscr_t;


typedef struct {
    pthread_t       iothread;
    struct sockaddr_un addr;
    int             id;
    unsigned int    flags;
    int             fd_sock;
    
    // Data counter for bytes loaded from socket
    uint8_t*        read_buf;
    size_t          read_size;
    unsigned int    read_id;
    
    ///@todo id_mutex deprecated
    //pthread_mutex_t id_mutex;
    
    // User mutex: for modifying readers and subs information
    pthread_mutex_t user_mutex;
    
    // reader cond: for broadcasting "new line arrived" to all readers
    ///@note readline_inactive is needed for Linux, which can have unreliable
    ///      implementation of pthread_cond_wait & pthread_cond_timedwait
    bool            readline_inactive;
    pthread_cond_t  readline_cond;
    pthread_mutex_t readline_mutex;
    
    bool            readdone_inactive;
    pthread_cond_t  readdone_cond;
    pthread_mutex_t readdone_mutex;
    
    // Readers: Synchronous reading clients
    ///@todo Change Array to linked list
    size_t      readers;
    size_t      max_readers;
    size_t      waiting_readers;
    sprdr_t*    reader[SP_MAX_READERS];
    
    // Subscribers: Asynchronous reading clients
    ///@todo Change Array to linked list
    size_t      subs;
    size_t      max_subs;
    spsubscr_t* sub[SP_MAX_SUBSCRIBERS];
    
} sp_item_t;





// ---------------------------------------------------------------------------



// Implementation of pthread_mutex_timedlock() for Mac
// Checks the lock each 2ms
// ---------------------------------------------------------------------------
static int sub_mutex_timedlock(pthread_mutex_t *restrict mutex, const struct timespec *restrict timeout) {
#if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L) && defined(_POSIX_THREADS) && (_POSIX_THREADS >= 200112L)
    return pthread_mutex_timedlock(mutex, timeout);
  
#else
    int rc;
    struct timespec cur, dur;
    
    // Try to acquire the lock and, if we fail, sleep for 1ms. */
    while ((rc = pthread_mutex_trylock(mutex)) == EBUSY) {
        clock_gettime(CLOCK_REALTIME, &cur);
//fprintf(stderr, _E_RED"pthread_mutex_trylock busy\n"_E_NRM);
        if ((cur.tv_sec > timeout->tv_sec) \
        || ((cur.tv_sec == timeout->tv_sec) \
        && (cur.tv_nsec >= timeout->tv_nsec))) {
            break;
        }

        dur.tv_sec = timeout->tv_sec - cur.tv_sec;
        dur.tv_nsec = timeout->tv_nsec - cur.tv_nsec;
        if (dur.tv_nsec < 0) {
            dur.tv_sec--;
            dur.tv_nsec += 1000000000;
        }

        if ((dur.tv_sec != 0) || (dur.tv_nsec > 2000000)) {
            dur.tv_sec = 0;
            dur.tv_nsec = 2000000;
        }

        nanosleep(&dur, NULL);
    }

    return rc;
#endif
}

// ---------------------------------------------------------------------------






static void* sp_iothread(void*);




static void sub_sendtosub(spsubscr_t* sub, uint8_t* data, size_t datasize) {
    datasize = (datasize > sub->max) ? sub->max : datasize;
    
    if (sub->buf != NULL) {
        memcpy(sub->buf, data, datasize);
        data = sub->buf;
    }
    if (sub->action != NULL) {
        sub->action(sub->parent, data, datasize);
    }
}



int sp_open(sp_handle_t* handle, const char* socket_path, unsigned int flags) {
    int rc;
    sp_item_t* new_sp;
    struct stat statdata;

    if ((handle == NULL) || (socket_path == NULL)) {
        return -1;
    }

    // Set initial variables
    new_sp = calloc(1, sizeof(sp_item_t));
    if (new_sp == NULL) {
        return -2;
    }
    
    // Default socket is -1, which is an unsupported/unused value
    new_sp->fd_sock     = -1;
    
    ///@todo allocate reader array to initial size
    new_sp->max_readers = SP_MAX_READERS;
    new_sp->max_subs    = SP_MAX_SUBSCRIBERS;

    // Test if the socket_path argument is indeed a path to a socket
    if (stat(socket_path, &statdata) != 0) {
        rc = -3;
        goto sp_open_ERR;
    }
    if (S_ISSOCK(statdata.st_mode) == 0) {
        rc = -3;
        goto sp_open_ERR;
    }

    // Open the socket
    new_sp->fd_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (new_sp < 0) {
        rc = -4;
        goto sp_open_ERR;
    }
    new_sp->addr.sun_family = AF_UNIX;
    snprintf(new_sp->addr.sun_path, UNIX_PATH_MAX, "%s", socket_path);

    // Initialize Data Mutexes
    if (pthread_mutex_init(&new_sp->user_mutex, NULL) != 0) {
        rc = -5;
        goto sp_open_ERR;
    }
    //if (pthread_mutex_init(&new_sp->id_mutex, NULL) != 0) {
    //    rc = -6;
    //    goto sp_open_ERR;
    //}

    // Initialize Readline Cond setup
    if (pthread_mutex_init(&new_sp->readline_mutex, NULL) != 0) {
        rc = -7;
        goto sp_open_ERR;
    }
    if (pthread_cond_init(&new_sp->readline_cond, NULL) != 0) {
        rc = -8;
        goto sp_open_ERR;
    }
    
    // Initialize ReadDone Cond setup
    if (pthread_mutex_init(&new_sp->readdone_mutex, NULL) != 0) {
        rc = -9;
        goto sp_open_ERR;
    }
    if (pthread_cond_init(&new_sp->readdone_cond, NULL) != 0) {
        rc = -10;
        goto sp_open_ERR;
    }
    
    // Create the socket management thread
    if (pthread_create(&new_sp->iothread, NULL, &sp_iothread, new_sp) != 0) {
        rc = -11;
        goto sp_open_ERR;
    }
    
    *handle = new_sp;
    return 0;
    
    sp_open_ERR:
    switch (rc) {
        case -11: pthread_cond_destroy(&new_sp->readdone_cond);
        case -10: pthread_mutex_unlock(&new_sp->readdone_mutex);
                 pthread_mutex_destroy(&new_sp->readdone_mutex);
        case -9: pthread_cond_destroy(&new_sp->readline_cond);
        case -8: pthread_mutex_unlock(&new_sp->readline_mutex);
                 pthread_mutex_destroy(&new_sp->readline_mutex);
        case -7: //pthread_mutex_unlock(&new_sp->id_mutex);
                 //pthread_mutex_destroy(&new_sp->id_mutex);
        case -6: pthread_mutex_unlock(&new_sp->user_mutex);
                 pthread_mutex_destroy(&new_sp->user_mutex);
        case -5: close(new_sp->fd_sock);
        case -4:
        case -3: free(new_sp);
        default: break;
    }
    
    return rc;
}



int sp_close(sp_handle_t handle) {
    sp_item_t* sp = handle;
    
    if (sp == NULL) {
        return -1;
    }
    
    if (pthread_cancel(sp->iothread) != 0) {
        return -2;
    }
    
    pthread_join(sp->iothread, NULL);
    
    pthread_cond_destroy(&sp->readdone_cond);
    pthread_mutex_unlock(&sp->readdone_mutex);
    pthread_mutex_destroy(&sp->readdone_mutex);
    
    pthread_cond_destroy(&sp->readline_cond);
    pthread_mutex_unlock(&sp->readline_mutex);
    pthread_mutex_destroy(&sp->readline_mutex);
    
    //pthread_mutex_unlock(&sp->id_mutex);
    //pthread_mutex_destroy(&sp->id_mutex);
    
    pthread_mutex_unlock(&sp->user_mutex);
    pthread_mutex_destroy(&sp->user_mutex);

    close(sp->fd_sock);
    free(sp);
    
    return 0;
}


//int sp_comm(sp_handle_t handle, uint8_t* readbuf, size_t readmax, uint8_t* writebuf, size_t writesize) {
//    int rc;
//    
//    rc = sp_sendcmd(handle, writebuf, writesize);
//    if (rc > 0) {
//        rc = sp_read(handle, readbuf, readmax, 1000);
//    }
//    
//    return rc;
//}


sp_reader_t sp_reader_create(void* ctx, sp_handle_t handle) {
    sp_item_t* sp = handle;
    sprdr_t* reader = NULL;

    if (sp != NULL) {
        if (sp->readers < sp->max_readers) {
            pthread_mutex_lock(&sp->user_mutex);
            reader = talloc_size(ctx, sizeof(sprdr_t));
            if (reader != NULL) {
                reader->parent      = sp;
                reader->last_read   = sp->read_id;
                
                ///@todo Change Array to linked list
                sp->reader[sp->readers] = reader;
                sp->readers++;
            }

            pthread_mutex_unlock(&sp->user_mutex);
        }
    }
    
    return reader;
}


void sp_reader_destroy(sp_reader_t reader) {
    sp_item_t* sp;

    if (reader != NULL) {
        sp = ((sprdr_t*)reader)->parent;
        pthread_mutex_lock(&sp->user_mutex);
        sp->readers--;
        pthread_mutex_unlock(&sp->user_mutex);
        talloc_free(reader);
    }
}


void sp_reader_purge(sp_reader_t reader) {
    sp_item_t* sp;

    if (reader != NULL) {
        sp = ((sprdr_t*)reader)->parent;
        pthread_mutex_lock(&sp->user_mutex);
        ((sprdr_t*)reader)->last_read = sp->read_id;
        pthread_mutex_unlock(&sp->user_mutex);
    }
}





static int sub_loadread(sprdr_t* rdr, sp_item_t* sp, uint8_t* readbuf, size_t readmax) {
    rdr->last_read = sp->read_id;

    if (readmax > sp->read_size) {
        readmax = sp->read_size;
    }
    memcpy(readbuf, sp->read_buf, readmax);
    
    return (int)readmax;
}


int sp_read(sp_reader_t reader, uint8_t* readbuf, size_t readmax, size_t timeout_ms) {
    sp_item_t* sp;
    sprdr_t* rdr;
    struct timespec ts, cur;
    int rc = 0;
    int wait_test;
    
    rdr = reader;
    if (rdr == NULL) {
        return -1;
    }

    if ((readbuf == NULL) || (readmax == 0)) {
        return 0;
    }

    // Create timespec based on milliseconds from input
    ts.tv_sec   = timeout_ms / 1000;
    ts.tv_nsec  = (timeout_ms % 1000) * 1000000;
    
    clock_gettime(CLOCK_REALTIME, &cur);
    ts.tv_nsec += cur.tv_nsec;
    ts.tv_sec += cur.tv_sec;
    if (ts.tv_nsec > 1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec  += 1;
    }
    
    // SP object is in the parent variable
    sp = rdr->parent;


    // 1st step is just to look if there's data sitting on the buffer already
    // This timed-lock allows sp_read() to catch a line that is being read at
    // time of calling.
    if (pthread_mutex_trylock(&sp->user_mutex) == 0) {
        if (rdr->last_read != sp->read_id) {
            rc = sub_loadread(rdr, sp, readbuf, readmax);
        }
        pthread_mutex_unlock(&sp->user_mutex);
    }
    
    // If 1st step yields no data, it means that sp_iothread() is in its poll()
    // state, waiting on the file.  So until the timeout expires, we can wait
    // for a readline cond signal to be broadcasted by sp_iothread().
    if (rc == 0) {
        int waiting_readers;
    
        // Set "waiting state"
        //pthread_mutex_lock(&sp->id_mutex);
        //sp->waiting_readers++;
        //pthread_mutex_unlock(&sp->id_mutex);
        
        ///@todo there seems to be a multiple read problem at times, here (or maybe above)
        pthread_mutex_lock(&sp->readline_mutex);
        sp->waiting_readers++;
        wait_test = 0;
        sp->readline_inactive = true;
        while (sp->readline_inactive && (wait_test == 0)) {
            wait_test = pthread_cond_timedwait(&sp->readline_cond, &sp->readline_mutex, &ts);
        }
        if (wait_test == 0) {
            rc = sub_loadread(rdr, sp, readbuf, readmax);
        }
        sp->waiting_readers -= (sp->waiting_readers != 0);
        waiting_readers = (int)sp->waiting_readers;
        pthread_mutex_unlock(&sp->readline_mutex);
        
        if (waiting_readers == 0) {
            pthread_mutex_lock(&sp->readdone_mutex);
            sp->readdone_inactive = false;
            pthread_cond_signal(&sp->readdone_cond);
            pthread_mutex_unlock(&sp->readdone_mutex);
        }
        
        // Unset "waiting state"
        //pthread_mutex_lock(&sp->id_mutex);
        //sp->waiting_readers -= (sp->waiting_readers != 0);
        //pthread_mutex_unlock(&sp->id_mutex);
    }
    
    sp_read_END:
    return rc;
}



static int sub_write(sp_handle_t handle, uint8_t* writebuf, size_t writesize, bool do_terminate) {
    sp_item_t* sp = handle;
    int rc;

    /// Send to socket
    pthread_mutex_lock(&sp->user_mutex);
    rc = (int)write(sp->fd_sock, writebuf, writesize);
    if (rc < 0) {
        rc = -2;
        goto sub_write_END;
    }
    if (do_terminate) {
        rc = (int)write(sp->fd_sock, "\n", 1);
    }
    
    /// Dispatch to subscriber(s)
    if (sp->subs > 0) {
        for (int i=0; i<sp->subs; i++) {
            ///@todo Change Array to linked list
            if (sp->sub[i]->flags & SP_SUB_OUTBOUND) {
                sub_sendtosub(sp->sub[i], writebuf, rc);
            }
        }
    }
    
    sub_write_END:
    pthread_mutex_unlock(&sp->user_mutex);
    return rc;
}


int sp_write(sp_handle_t handle, uint8_t* writebuf, size_t writesize) {
    if (handle == NULL) {
        return -1;
    }
    if ((writebuf == NULL) || (writesize == 0)) {
        return 0;
    }
    return sub_write(handle, writebuf, writesize, false);
}


int sp_sendcmd(sp_handle_t handle, uint8_t* writebuf, size_t writesize) {
    if (handle == NULL) {
        return -1;
    }
    if ((writebuf == NULL) || (writesize == 0)) {
        return 0;
    }
    return sub_write(handle, writebuf, writesize, (bool)(writebuf[writesize-1] != '\n'));
}



///@todo Not yet implemented
//-----------------------------------------------------------------------------
int sp_subscribe(sp_handle_t handle, sp_action_t action, int flags, uint8_t* buf, size_t max) {
    return -1;
}

//int sp_dispatch(sp_handle_t handle, sp_status_t action, uint8_t* writebuf, size_t writesize) {
//    return -1;
//}
//-----------------------------------------------------------------------------



void* sp_iothread(void* args) {
    sp_item_t* sp = args;
    
    uint8_t linebuf[1024];
    uint8_t readbuf[1024];
    char* cursor;
    char* end;
    int backoff = 1;
    int max_backoff = 60;
    
    // This thread uses mutexes, so it's important to have deferred cancelling
    // to prevent deadlock in odd cases where thread is cancelled
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    
    // Setup reference variables needed by sp_read()
    sp->readline_inactive   = true;
    sp->read_buf            = linebuf;
    sp->read_size           = 0;
    sp->read_id             = 1;
    
    while (1) {
        /// Connect to the socket
        if (connect(sp->fd_sock, (struct sockaddr *)&sp->addr, sizeof(struct sockaddr_un)) < 0) {
            sleep(backoff);
            if (backoff < max_backoff) {
                backoff *= 2;
            }
            continue;
        }
        
        backoff = 1;
        
        while (1) {
            // ----------------------------------------------------------------
            /// Double-Buffer stream
            /// Solves for problem of reading two lines at the same time.
            cursor = (char*)readbuf;
            end    = (char*)readbuf + sizeof(readbuf);
            while(1) {
                if (read(sp->fd_sock, cursor, 1) < 1) {
                    goto sp_iothread_RECONNECT;
                }
                if ((*cursor == '\n') || (*cursor == 0)) {
                    *cursor++ = 0;
                    break;
                }
                if (++cursor >= end) {
                    cursor = (char*)readbuf;
                }
            }
            // ----------------------------------------------------------------

            pthread_mutex_lock(&sp->user_mutex);
            sp->read_id++;
            sp->read_size = (cursor - (char*)readbuf);
            memcpy(sp->read_buf, readbuf, sp->read_size);

            // publish it to subscribers, which are callbacks that need
            // to deal with data replication themselves.
            // lock the data mutex to prevent new subscribers getting added
            if (sp->subs > 0) {
                for (int i=0; i<sp->subs; i++) {
                    ///@todo Change Array to linked list
                    if (sp->sub[i]->flags & SP_SUB_INBOUND) {
                        sub_sendtosub(sp->sub[i], sp->read_buf, sp->read_size);
                    }
                }
            }

            ///@todo there seems to be a problem where a line gets read multiple times via sp_read()
            if (sp->readers > 0) {
                pthread_mutex_lock(&sp->readline_mutex);
                if (sp->waiting_readers <= 0) {
                    pthread_mutex_unlock(&sp->readline_mutex);
                }
                else {
                    sp->readline_inactive = false;
                    pthread_cond_broadcast(&sp->readline_cond);
                    pthread_mutex_unlock(&sp->readline_mutex);
                
                    pthread_mutex_lock(&sp->readdone_mutex);
                    sp->readdone_inactive = true;
                    while (sp->readdone_inactive) {
                        pthread_cond_wait(&sp->readdone_cond, &sp->readdone_mutex);
                    }
                    pthread_mutex_unlock(&sp->readdone_mutex);
                }
                
            }
            
            pthread_mutex_unlock(&sp->user_mutex);
        }
        
        sp_iothread_RECONNECT:
        close(sp->fd_sock);
    }
    
    
    return NULL;
}




//#define TESTING
#ifdef TESTING
int main(void) {

    return 0;
}
#endif
