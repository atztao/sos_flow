/*
 *  sosd.c (daemon)
 *
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef SOSD_CLOUD_SYNC_WITH_MPI
#include "sosd_cloud_mpi.h"
#endif
#ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
#include "sosd_cloud_evpath.h"
#endif
#ifdef SOSD_CLOUD_SYNC_WITH_STUBS
#include "sosd_cloud_stubs.h"
#endif

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sosd.h"
#include "sosd_db_sqlite.h"

#include "sos_pipe.h"
#include "sos_qhashtbl.h"
#include "sos_buffer.h"

#define USAGE          "usage:   $ sosd  --port <number>  --buffer_len <bytes>  --listen_backlog <len>  --role <role>  --work_dir <path>"

void SOSD_display_logo(void);

int main(int argc, char *argv[])  {
    int elem, next_elem;
    int retval;
    SOS_role my_role;

    SOSD.daemon.work_dir    = (char *) &SOSD_DEFAULT_DIR;
    SOSD.daemon.name        = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);
    SOSD.daemon.lock_file   = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);
    SOSD.daemon.log_file    = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);

    my_role = SOS_ROLE_DAEMON; /* This can be overridden by command line argument. */

    /* Process command-line arguments */
    if ( argc < 7 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }
    SOSD.net.port_number    = -1;
    SOSD.net.buffer_len     = -1;
    SOSD.net.listen_backlog = -1;
    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) { fprintf(stderr, "%s\n", USAGE); exit(1); }
        if (      strcmp(argv[elem], "--port"            ) == 0) { SOSD.net.server_port    = argv[next_elem];       }
        else if ( strcmp(argv[elem], "--buffer_len"      ) == 0) { SOSD.net.buffer_len     = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--listen_backlog"  ) == 0) { SOSD.net.listen_backlog = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--work_dir"        ) == 0) { SOSD.daemon.work_dir    = argv[next_elem];       }
        else if ( strcmp(argv[elem], "--role"            ) == 0) {
            if (      strcmp(argv[next_elem], "SOS_ROLE_DAEMON" ) == 0)  { my_role = SOS_ROLE_DAEMON; }
            else if ( strcmp(argv[next_elem], "SOS_ROLE_DB" ) == 0)      { my_role = SOS_ROLE_DB; }
            else {  fprintf(stderr, "Unknown role: %s %s\n", argv[elem], argv[next_elem]); }
        } else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]); }
        elem = next_elem + 1;
    }
    SOSD.net.port_number = atoi(SOSD.net.server_port);
    if ( (SOSD.net.port_number < 1)
         || (SOSD.net.buffer_len < 1)
         || (SOSD.net.listen_backlog < 1) )
        { fprintf(stderr, "%s\n", USAGE); exit(1); }

    #ifndef SOSD_CLOUD_SYNC
    if (my_role != SOS_ROLE_DAEMON) {
        printf("NOTE: Terminating an instance of sosd with pid: %d\n", getpid());
        printf("NOTE: SOSD_CLOUD_SYNC is disabled but this instance is not a SOS_ROLE_DAEMON!\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    #endif

    memset(&SOSD.daemon.pid_str, '\0', 256);

    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) { printf("Preparing to initialize:\n"); fflush(stdout); }
    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) { printf("   ... creating SOS_runtime object for daemon use.\n"); fflush(stdout); }
    SOSD.sos_context = (SOS_runtime *) malloc(sizeof(SOS_runtime));
    memset(SOSD.sos_context, '\0', sizeof(SOS_runtime));
    SOSD.sos_context->role = my_role;

    #ifdef SOSD_CLOUD_SYNC
    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) { printf("   ... calling SOSD_cloud_init()...\n"); fflush(stdout); }
    SOSD_cloud_init( &argc, &argv);
    #else
    dlog(0, "   ... WARNING: There is no CLOUD_SYNC configured for this SOSD.\n");
    #endif

    SOS_SET_CONTEXT(SOSD.sos_context, "main");
    dlog(0, "Initializing SOSD:\n");

    dlog(0, "   ... calling SOS_init(argc, argv, %s, SOSD.sos_context) ...\n", SOS_ENUM_STR( my_role, SOS_ROLE ));
    SOSD.sos_context = SOS_init_with_runtime( &argc, &argv, my_role, SOS_LAYER_SOS_RUNTIME, SOSD.sos_context );

    dlog(0, "   ... calling SOSD_init()...\n");
    SOSD_init();
    if (SOS->config.comm_rank == 0) {
        SOSD_display_logo();
    }

    dlog(0, "   ... done. (SOSD_init + SOS_init are complete)\n");
    dlog(0, "Calling register_signal_handler()...\n");
    if (SOSD_DAEMON_LOG) SOS_register_signal_handler(SOSD.sos_context);
    if (SOS->role == SOS_ROLE_DAEMON) {
        dlog(0, "Calling daemon_setup_socket()...\n");
        SOSD_setup_socket();
    }

    dlog(0, "Calling daemon_init_database()...\n");
    SOSD_db_init_database();

    dlog(0, "Initializing the sync framework...\n");
    #ifdef SOSD_CLOUD_SYNC
    SOSD_sync_context_init(SOS, &SOSD.sync.cloud, sizeof(SOS_buffer *), (void *) SOSD_THREAD_cloud_sync);
    #else
    #endif
    SOSD_sync_context_init(SOS, &SOSD.sync.local, sizeof(SOS_buffer *), (void *) SOSD_THREAD_local_sync);
    SOSD_sync_context_init(SOS, &SOSD.sync.db,   sizeof(SOSD_db_task *), (void *) SOSD_THREAD_db_sync);

    dlog(0, "Starting the cloud threads...\n");
    SOSD_cloud_start();

    dlog(0, "Entering listening loop...\n");

    /* Go! */
    switch (SOS->role) {
    case SOS_ROLE_DAEMON:   SOSD_listen_loop(); break;
    case SOS_ROLE_DB:
        #ifdef SOSD_CLOUD_SYNC
        SOSD_cloud_listen_loop();
        #endif
        break;
    default: break;
    }

    sleep(10);

    /* Done!  Cleanup and shut down. */
    dlog(0, "Closing the sync queues:\n");
    dlog(0, "  .. SOSD.sync.local.queue\n");
    pipe_producer_free(SOSD.sync.local.queue->intake);
    dlog(0, "     (waiting for local.queue->elem_count == 0)\n");
    dlog(0, "  .. SOSD.sync.cloud.queue\n");
    pipe_producer_free(SOSD.sync.cloud.queue->intake);
    dlog(0, "  .. SOSD.sync.db.queue\n");
    pipe_producer_free(SOSD.sync.db.queue->intake);

    SOSD.db.ready = -1;
    pthread_mutex_lock(SOSD.db.lock);

    dlog(0, "Destroying uid configurations.\n");
    SOS_uid_destroy( SOSD.guid );
    dlog(0, "  ... done.\n");
    dlog(0, "Closing the database.\n");
    SOSD_db_close_database();
    if (SOS->role == SOS_ROLE_DAEMON) {
        dlog(0, "Closing the socket.\n");
        shutdown(SOSD.net.server_socket_fd, SHUT_RDWR);
    }
    #if (SOSD_CLOUD_SYNC > 0)
    dlog(0, "Detaching from the cloud of sosd daemons.\n");
    SOSD_cloud_finalize();
    #endif

    dlog(0, "Shutting down SOS services.\n");
    SOS_finalize(SOS);

    if (SOSD_DAEMON_LOG) { fclose(sos_daemon_log_fptr); }
    if (SOSD_DAEMON_LOG) { free(SOSD.daemon.log_file); }

    close(sos_daemon_lock_fptr);
    remove(SOSD.daemon.lock_file);
    free(SOSD.daemon.name);
    free(SOSD.daemon.lock_file);

    return(EXIT_SUCCESS);
} //end: main()




void SOSD_listen_loop() {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_listen_loop");
    SOS_msg_header header;
    SOS_buffer    *buffer;
    SOS_buffer    *rapid_reply;
    int            recv_len;
    int            offset;
    int            i;

    SOS_buffer_init_sized_locking(SOS, &buffer, SOS_DEFAULT_BUFFER_LEN, false);
    SOS_buffer_init_sized_locking(SOS, &rapid_reply, SOS_DEFAULT_REPLY_LEN, false);

    SOSD_PACK_ACK(rapid_reply);

    dlog(0, "Entering main loop...\n");
    while (SOSD.daemon.running) {
        offset = 0;
        SOS_buffer_wipe(buffer);

        dlog(5, "Listening for a message...\n");
        SOSD.net.peer_addr_len = sizeof(SOSD.net.peer_addr);
        SOSD.net.client_socket_fd = accept(SOSD.net.server_socket_fd, (struct sockaddr *) &SOSD.net.peer_addr, &SOSD.net.peer_addr_len);
        i = getnameinfo((struct sockaddr *) &SOSD.net.peer_addr, SOSD.net.peer_addr_len, SOSD.net.client_host, NI_MAXHOST, SOSD.net.client_port, NI_MAXSERV, NI_NUMERICSERV);
        if (i != 0) { dlog(0, "Error calling getnameinfo() on client connection.  (%s)\n", strerror(errno)); break; }

        buffer->len = recv(SOSD.net.client_socket_fd, (void *) buffer->data, buffer->max, 0);
        dlog(6, "  ... recv() returned %d bytes.\n", buffer->len);

        if (buffer->len < 0) {
            dlog(1, "  ... recv() call returned an errror.  (%s)\n", strerror(errno));
        }

        memset(&header, '\0', sizeof(SOS_msg_header));
        if (buffer->len >= sizeof(SOS_msg_header)) {
            int offset = 0;
            SOS_buffer_unpack(buffer, &offset, "iigg",
                              &header.msg_size,
                              &header.msg_type,
                              &header.msg_from,
                              &header.pub_guid);
        } else {
            dlog(0, "  ... Received short (useless) message.\n");
            continue;
        }

        dlog(5, "Received connection.\n");
        dlog(5, "  ... msg_size == %d         (buffer->len == %d)\n", header.msg_size, buffer->len);
        switch (header.msg_type) {
        case SOS_MSG_TYPE_REGISTER:   dlog(5, "  ... msg_type = REGISTER (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_GUID_BLOCK: dlog(5, "  ... msg_type = GUID_BLOCK (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_ANNOUNCE:   dlog(5, "  ... msg_type = ANNOUNCE (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_PUBLISH:    dlog(5, "  ... msg_type = PUBLISH (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_VAL_SNAPS:  dlog(5, "  ... msg_type = VAL_SNAPS (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_ECHO:       dlog(5, "  ... msg_type = ECHO (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_SHUTDOWN:   dlog(5, "  ... msg_type = SHUTDOWN (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_CHECK_IN:   dlog(5, "  ... msg_type = CHECK_IN (%d)\n", header.msg_type); break;
        default:                      dlog(1, "  ... msg_type = UNKNOWN (%d)\n", header.msg_type); break;
        }
        dlog(5, "  ... msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
        dlog(5, "  ... pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid);


        switch (header.msg_type) {
        case SOS_MSG_TYPE_REGISTER:   SOSD_handle_register   (buffer); break; 
        case SOS_MSG_TYPE_GUID_BLOCK: SOSD_handle_guid_block (buffer); break;

        case SOS_MSG_TYPE_ANNOUNCE:
        case SOS_MSG_TYPE_PUBLISH:
        case SOS_MSG_TYPE_VAL_SNAPS:
            dlog(5, "  ... [ddd] <---- pushing buffer @ [%ld] onto the local_sync queue. buffer->len == %d   (%s)\n", (long) buffer, buffer->len, SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE));
            pipe_push(SOSD.sync.local.queue->intake, (void *) &buffer, 1);
            buffer = NULL;
            SOS_buffer_init_sized_locking(SOS, &buffer, SOS_DEFAULT_BUFFER_LEN, false);
            //Send generic ACK message back to the client:
            dlog(5, "  ... sending ACK w/reply->len == %d\n", rapid_reply->len);
            i = send( SOSD.net.client_socket_fd, (void *) rapid_reply->data, rapid_reply->len, 0);
            if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
            else { dlog(5, "  ... send() returned the following bytecount: %d\n", i); }    
            dlog(5, "  ... Done.\n");
            break;

        case SOS_MSG_TYPE_ECHO:       SOSD_handle_echo       (buffer); break;
        case SOS_MSG_TYPE_SHUTDOWN:   SOSD_handle_shutdown   (buffer); break;
        case SOS_MSG_TYPE_CHECK_IN:   SOSD_handle_check_in   (buffer); break;
        default:                      SOSD_handle_unknown    (buffer); break;
        }

        close( SOSD.net.client_socket_fd );
    }

    SOS_buffer_destroy(buffer);
    SOS_buffer_destroy(rapid_reply);

    dlog(1, "Leaving the socket listening loop.\n");

    return;
}

/* -------------------------------------------------- */


void* SOSD_THREAD_local_sync(void *args) {
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_SET_CONTEXT(my->sos_context, "SOSD_THREAD_local_sync");
    struct timespec  wait;
    SOS_msg_header   header;
    SOS_buffer      *buffer;
    int              offset;
    int              count;

    pthread_mutex_lock(my->lock);
    wait.tv_sec  = 0;
    wait.tv_nsec = 10000;
    while (SOS->status == SOS_STATUS_RUNNING) {
        pthread_cond_timedwait(my->cond, my->lock, &wait);
        buffer = NULL;

        count = pipe_pop(my->queue->outlet, (void *) &buffer, 1);
        if (count == 0) {
            dlog(6, "Nothing remains in the queue, and the intake is closed.  Leaving thread.\n");
            break;
        }

        dlog(6, "  [ddd] >>>>> Popped a buffer @ [%ld] off the queue. ... buffer->len == %d   [&my == %ld]\n", (long) buffer, buffer->len, (long) my);


        if (buffer == NULL) {
            dlog(6, "   ... *buffer == NULL!\n");
            wait.tv_sec  = 0;
            wait.tv_nsec = 10000;
            continue;
        }

        int offset = 0;
        SOS_buffer_unpack(buffer, &offset, "iigg",
            &header.msg_size,
            &header.msg_type,
            &header.msg_from,
            &header.pub_guid);
        
        switch(header.msg_type) {
        case SOS_MSG_TYPE_ANNOUNCE:   SOSD_handle_announce   (buffer); break;
        case SOS_MSG_TYPE_PUBLISH:    SOSD_handle_publish    (buffer); break;
        case SOS_MSG_TYPE_VAL_SNAPS:  SOSD_handle_val_snaps  (buffer); break;
        default:
            dlog(0, "ERROR: An invalid message type (%d) was placed in the local_sync queue!\n", header.msg_type);
            dlog(0, "ERROR: Destroying it.\n");
            SOS_buffer_destroy(buffer);
            break;
        }

        pipe_push(SOSD.sync.cloud.queue->intake, (void *) &buffer, 1);

        wait.tv_sec  = 0;
        wait.tv_nsec = 10000;
    }

    pthread_mutex_unlock(my->lock);
    pthread_exit(NULL);
}


void* SOSD_THREAD_db_sync(void *args) {
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_SET_CONTEXT(my->sos_context, "SOSD_THREAD_db_sync");
    struct timeval   now;
    struct timespec  wait;
    SOSD_db_task   **task_list;
    SOSD_db_task    *task;
    int              task_index;
    int              queue_depth;
    int              count;

    pthread_mutex_lock(my->lock);
    gettimeofday(&now, NULL);
    wait.tv_sec  = 0 + (now.tv_sec);
    wait.tv_nsec = 50000000 + (1000 * now.tv_usec);
    while (SOS->status == SOS_STATUS_RUNNING) {
        pthread_cond_timedwait(my->cond, my->lock, &wait);

        pthread_mutex_lock(my->queue->sync_lock);
        queue_depth = my->queue->elem_count;

        dlog(6, "There are %d elements in the queue.\n", queue_depth);

        if (queue_depth > 0) {
            //task_list = (SOSD_db_task **) malloc(queue_depth * sizeof(SOSD_db_task *));
            task_list = (SOSD_db_task **) malloc(1 * sizeof(SOSD_db_task *));
            queue_depth = 1;                
        } else {
            dlog(6, "   ... going back to sleep.\n");
            pthread_mutex_unlock(my->queue->sync_lock);
            gettimeofday(&now, NULL);
            wait.tv_sec  = 0 + (now.tv_sec);
            wait.tv_nsec = 50000000 + (1000 * now.tv_usec);
            continue;
        }

        count = 0;
        count = pipe_pop_eager(my->queue->outlet, (void *) task_list, queue_depth);
        if (count == 0) {
            dlog(0, "Nothing remains in the queue and the intake is closed.  Leaving thread.\n");
            free(task_list);
            break;
        }

        dlog(6, "Popped %d elements into %d spaces.\n", count, queue_depth);

        my->queue->elem_count -= count;
        pthread_mutex_unlock(my->queue->sync_lock);

        SOSD_db_transaction_begin();

        for (task_index = 0; task_index < count; task_index++) {
            task = task_list[task_index];
            switch(task->type) {
            case SOS_MSG_TYPE_ANNOUNCE:   dlog(6, "[zzz] ANNOUNCE\n"); SOSD_db_insert_pub(task->pub); break;
            case SOS_MSG_TYPE_PUBLISH:    dlog(6, "[zzz] PUBLISH-\n"); SOSD_db_insert_data(task->pub); break;
            case SOS_MSG_TYPE_VAL_SNAPS:  dlog(6, "[zzz] SNAPS---\n"); SOSD_db_insert_vals(task->pub, task->pub->snap_queue, NULL); break;
            }
            free(task);
        }

        SOSD_db_transaction_commit();
        free(task_list);
        gettimeofday(&now, NULL);
        wait.tv_sec  = 0 + (now.tv_sec);
        wait.tv_nsec = 5000000 + (1000 * now.tv_usec);

    }

    pthread_mutex_unlock(my->lock);
    pthread_exit(NULL);
}



void* SOSD_THREAD_cloud_sync(void *args) {
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_SET_CONTEXT(my->sos_context, "SOSD_THREAD_cloud_sync");
    SOS_buffer      *buffer;
    struct timespec  wait;
    int     count;

    pthread_mutex_lock(my->lock);
    wait.tv_sec  = 0;
    wait.tv_nsec = 10000;
    while (SOS->status == SOS_STATUS_RUNNING) {
        pthread_cond_timedwait(my->cond, my->lock, &wait);

        count = pipe_pop(my->queue->outlet, (void *) &buffer, 1);
        if (count == 0) {
            dlog(0, "Nothing in the queue and the intake is closed.  Leaving thread.\n");
            break;
        }

        dlog(5, "TODO: Destroying a queue.\n");
        SOS_buffer_destroy(buffer);

        wait.tv_sec  = 0;
        wait.tv_nsec = 10000;
    }

    pthread_mutex_unlock(my->lock);
    pthread_exit(NULL);
}


/* -------------------------------------------------- */


void SOSD_handle_echo(SOS_buffer *buffer) { 
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_echo");
    SOS_msg_header header;
    int            offset;
    int            rc;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_ECHO\n");

    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    rc = send(SOSD.net.client_socket_fd, (void *) buffer->data, buffer->len, 0);
    if (rc == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
        
    return;
}


void SOSD_handle_val_snaps(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_val_snaps");
    SOSD_db_task  *task;
    SOS_msg_header header;
    SOS_pub       *pub;
    char           pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    int            offset;
    int            rc;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_VAL_SNAPS\n");

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, header.pub_guid);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, pub_guid_str);

    if (pub == NULL) {
        dlog(0, "ERROR: No pub exists for header.pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid); 
        dlog(0, "ERROR: Destroying message and returning.\n");
        SOS_buffer_destroy(buffer);
        return;
    }

    dlog(5, "Injecting snaps into pub->snap_queue...\n");
    SOS_val_snap_queue_from_buffer(buffer, pub->snap_queue, pub);


    dlog(5, "Queue these val snaps up for the database...\n");
    task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
    task->pub = pub;
    task->type = SOS_MSG_TYPE_VAL_SNAPS;
    pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
    pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
    SOSD.sync.db.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);

    dlog(5, "  ... done.\n");
        
    return;
}



/* TODO: { VERSIONING } Add SOSD version to this message. */
void SOSD_handle_register(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_register");
    SOS_msg_header header;
    SOS_buffer    *reply;
    int            offset;
    int            i;
    SOS_guid       guid_block_from;
    SOS_guid       guid_block_to;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_REGISTER\n");

    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);


    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);

    if (header.msg_from == 0) {
        /* A new client is registering with the daemon.
         * Supply them a block of GUIDs ...
         */
        SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK, &guid_block_from, &guid_block_to);

        offset = 0;
        SOS_buffer_pack(reply, &offset, "gg",
                        guid_block_from,
                        guid_block_to);

    } else {
        /* An existing client (such as a scripting language wrapped library)
         * is coming back online, so don't give them any GUIDs.
         */
        SOSD_PACK_ACK(reply);
    }

    i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
    }

    SOS_buffer_destroy(reply);

    return;
}


void SOSD_handle_guid_block(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_register");
    SOS_msg_header header;
    SOS_guid       block_from   = 0;
    SOS_guid       block_to     = 0;
    SOS_buffer    *reply;
    int            offset;
    int            i;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_GUID_BLOCK\n");

    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK, &block_from, &block_to);

    offset = 0;
    SOS_buffer_pack(reply, &offset, "gg",
        block_from,
        block_to);

    i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
    }

    SOS_buffer_destroy(reply);

    return;
}


void SOSD_handle_announce(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_announce");
    SOSD_db_task   *task;
    SOS_msg_header  header;
    SOS_buffer     *reply;
    SOS_pub        *pub;
    
    char            pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    int             offset;
    int             i;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_ANNOUNCE\n");

    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, header.pub_guid);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, pub_guid_str);

    if (pub == NULL) {
        dlog(5, "     ... NOPE!  Adding new pub to the table.\n");
        /* If it's not in the table, add it. */
        pub = SOS_pub_create(SOS, pub_guid_str, SOS_NATURE_DEFAULT);
        strncpy(pub->guid_str, pub_guid_str, SOS_DEFAULT_STRING_LEN);
        pub->guid = header.pub_guid;
        SOSD.pub_table->put(SOSD.pub_table, pub_guid_str, pub);
    } else {
        dlog(5, "     ... FOUND IT!\n");
    }
    dlog(5, "     ... SOSD.pub_table.size() = %d\n", SOSD.pub_table->size(SOSD.pub_table));
    dlog(5, "Calling SOSD_apply_announce() ...\n");

    SOSD_apply_announce(pub, buffer);
    pub->announced = SOSD_PUB_ANN_DIRTY;

    task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
    task->pub = pub;
    task->type = SOS_MSG_TYPE_ANNOUNCE;
    pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
    pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
    SOSD.sync.db.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);

    dlog(5, "  ... pub(%" SOS_GUID_FMT ")->elem_count = %d\n", pub->guid, pub->elem_count);

    return;
}


void SOSD_handle_publish(SOS_buffer *buffer)  {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_handle_publish");
    SOSD_db_task   *task;
    SOS_msg_header  header;
    SOS_buffer     *reply;
    SOS_pub        *pub;
    char            pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    int             offset;
    int             i;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_PUBLISH\n");

    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, header.pub_guid);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, pub_guid_str);

    /* Check the table for this pub ... */
    dlog(5, "  ... checking SOS->pub_table for GUID(%s):\n", pub_guid_str);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, pub_guid_str);

    if (pub == NULL) {
        /* If it's not in the table, add it. */
    dlog(0, "ERROR: PUBLISHING INTO A PUB (guid:%" SOS_GUID_FMT ") NOT FOUND! (WEIRD!)\n", header.pub_guid);
    dlog(0, "ERROR: .... ADDING previously unknown pub to the table... (this is bogus, man)\n");
        pub = SOS_pub_create(SOS, pub_guid_str, SOS_NATURE_DEFAULT);
        strncpy(pub->guid_str, pub_guid_str, SOS_DEFAULT_STRING_LEN);
        pub->guid = header.pub_guid;
        SOSD.pub_table->put(SOSD.pub_table, pub_guid_str, pub);
    } else {
        dlog(5, "     ... FOUND it!\n");
    }
    dlog(5, "     ... SOSD.pub_table.size() = %d\n", SOSD.pub_table->size(SOSD.pub_table));

    SOSD_apply_publish(pub, buffer);

    task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
    task->pub = pub;
    task->type = SOS_MSG_TYPE_PUBLISH;
    pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
    pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
    SOSD.sync.db.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);
    
    return;
}



void SOSD_handle_shutdown(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_shutdown");
    SOS_msg_header header;
    SOS_buffer    *reply;
    int            offset;
    int            i;

    dlog(1, "header.msg_type = SOS_MSG_TYPE_SHUTDOWN\n");

    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                             &header.msg_size,
                             &header.msg_type,
                             &header.msg_from,
                             &header.pub_guid);

    if (SOS->role == SOS_ROLE_DAEMON) {
        SOSD_PACK_ACK(reply);
        
        i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
        if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
        else { dlog(5, "  ... send() returned the following bytecount: %d\n", i); }
    }

    #if (SOSD_CLOUD_SYNC > 0)
    SOSD_cloud_shutdown_notice();
    #endif

    SOSD.daemon.running = 0;
    SOS->status = SOS_STATUS_SHUTDOWN;

    /*
     * We don't need to do this here, the handlers are called by the same thread as
     * the listener, so setting the flag (above) is sufficient to stop the listener
     * loop and initiate a clean shutdown.
     *
    shutdown(SOSD.net.server_socket_fd, SHUT_RDWR);
     *
     */

    SOS_buffer_destroy(reply);

    return;
}



void SOSD_handle_check_in(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_check_in");
    SOS_msg_header header;
    unsigned char  function_name[SOS_DEFAULT_STRING_LEN] = {0};
    SOS_buffer    *reply;
    int            offset;
    int            i;

    dlog(1, "header.msg_type = SOS_MSG_TYPE_CHECK_IN\n");

    SOS_buffer_init(SOS, &reply);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    if (SOS->role == SOS_ROLE_DAEMON) {
        /* Build a reply: */
        memset(&header, '\0', sizeof(SOS_msg_header));
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_FEEDBACK;
        header.msg_from = 0;
        header.pub_guid = 0;

        offset = 0;
        SOS_buffer_pack(reply, &offset, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);

        /* TODO: { FEEDBACK } Currently this is a hard-coded 'exec function' case. */
        snprintf(function_name, SOS_DEFAULT_STRING_LEN, "demo_function");

        SOS_buffer_pack(reply, &offset, "is",
            SOS_FEEDBACK_EXEC_FUNCTION,
            function_name);

        /* Go back and set the message length to the actual length. */
        header.msg_size = offset;
        offset = 0;
        SOS_buffer_pack(reply, &offset, "i", header.msg_size);

        dlog(1, "Replying to CHECK_IN with SOS_FEEDBACK_EXEC_FUNCTION(%s)...\n", function_name);

        i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
        if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
        else { dlog(5, "  ... send() returned the following bytecount: %d\n", i); }
    }

    SOS_buffer_destroy(reply);
    dlog(5, "Done!\n");

    return;
}


void SOSD_handle_unknown(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_handle_unknown");
    SOS_msg_header  header;
    SOS_buffer     *reply;
    int             offset;
    int             i;

    dlog(1, "header.msg_type = UNKNOWN\n");

    SOS_buffer_init(SOS, &reply);

    offset = 0;
    SOS_buffer_unpack(reply, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    dlog(1, "header.msg_size == %d\n", header.msg_size);
    dlog(1, "header.msg_type == %d\n", header.msg_type);
    dlog(1, "header.msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
    dlog(1, "header.pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid);

    if (SOS->role == SOS_ROLE_DB) {
        SOS_buffer_destroy(reply);
        return;
    }

    SOSD_PACK_ACK(reply);

    i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else { dlog(5, "  ... send() returned the following bytecount: %d\n", i); }

    SOS_buffer_destroy(reply);

    return;
}



void SOSD_setup_socket() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_setup_socket");
    int i;
    int yes;
    int opts;

    yes = 1;

    memset(&SOSD.net.server_hint, '\0', sizeof(struct addrinfo));
    SOSD.net.server_hint.ai_family     = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    SOSD.net.server_hint.ai_socktype   = SOCK_STREAM;   /* SOCK_STREAM vs. SOCK_DGRAM vs. SOCK_RAW */
    SOSD.net.server_hint.ai_flags      = AI_PASSIVE;    /* For wildcard IP addresses */
    SOSD.net.server_hint.ai_protocol   = 0;             /* Any protocol */
    SOSD.net.server_hint.ai_canonname  = NULL;
    SOSD.net.server_hint.ai_addr       = NULL;
    SOSD.net.server_hint.ai_next       = NULL;

    i = getaddrinfo(NULL, SOSD.net.server_port, &SOSD.net.server_hint, &SOSD.net.result);
    if (i != 0) { dlog(0, "Error!  getaddrinfo() failed. (%s) Exiting daemon.\n", strerror(errno)); exit(EXIT_FAILURE); }

    for ( SOSD.net.server_addr = SOSD.net.result ; SOSD.net.server_addr != NULL ; SOSD.net.server_addr = SOSD.net.server_addr->ai_next ) {
        dlog(1, "Trying an address...\n");

        SOSD.net.server_socket_fd = socket(SOSD.net.server_addr->ai_family, SOSD.net.server_addr->ai_socktype, SOSD.net.server_addr->ai_protocol );
        if ( SOSD.net.server_socket_fd < 1) {
            dlog(0, "  ... failed to get a socket.  (%s)\n", strerror(errno));
            continue;
        }

        /*
         *  Allow this socket to be reused/rebound quickly by the daemon.
         */
        if ( setsockopt( SOSD.net.server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            dlog(0, "  ... could not set socket options.  (%s)\n", strerror(errno));
            continue;
        }

        if ( bind( SOSD.net.server_socket_fd, SOSD.net.server_addr->ai_addr, SOSD.net.server_addr->ai_addrlen ) == -1 ) {
            dlog(0, "  ... failed to bind to socket.  (%s)\n", strerror(errno));
            close( SOSD.net.server_socket_fd );
            continue;
        } 
        /* If we get here, we're good to stop looking. */
        break;
    }

    if ( SOSD.net.server_socket_fd < 0 ) {
        dlog(0, "  ... could not socket/setsockopt/bind to anything in the result set.  last errno = (%d:%s)\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... got a socket, and bound to it!\n");
    }

    freeaddrinfo(SOSD.net.result);

    /*
     *   Enforce that this is a BLOCKING socket:
     */
    opts = fcntl(SOSD.net.server_socket_fd, F_GETFL);
    if (opts < 0) { dlog(0, "ERROR!  Cannot call fcntl() on the server_socket_fd to get its options.  Carrying on.  (%s)\n", strerror(errno)); }
 
    opts = opts & !(O_NONBLOCK);
    i    = fcntl(SOSD.net.server_socket_fd, F_SETFL, opts);
    if (i < 0) { dlog(0, "ERROR!  Cannot use fcntl() to set the server_socket_fd to BLOCKING more.  Carrying on.  (%s).\n", strerror(errno)); }


    listen( SOSD.net.server_socket_fd, SOSD.net.listen_backlog );
    dlog(0, "Listening on socket.\n");

    return;
}
 


void SOSD_init() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_init");
    pid_t pid, ppid, sid;
    int rc;

    /* [daemon name]
     *     assign a name appropriate for whether it is participating in a cloud or not
     */
    switch (SOS->role) {
    case SOS_ROLE_DAEMON:  snprintf(SOSD.daemon.name, SOS_DEFAULT_STRING_LEN, "%s", SOSD_DAEMON_NAME /* ".mon" */); break;
    case SOS_ROLE_DB:      snprintf(SOSD.daemon.name, SOS_DEFAULT_STRING_LEN, "%s", SOSD_DAEMON_NAME /* ".dat" */); break;
    default: break;
    }

    /* [lock file]
     *     create and hold lock file to prevent multiple daemon spawn
     */
    #if (SOSD_CLOUD_SYNC > 0)
    snprintf(SOSD.daemon.lock_file, SOS_DEFAULT_STRING_LEN, "%s/%s.%d.lock", SOSD.daemon.work_dir, SOSD.daemon.name, SOS->config.comm_rank);
    #else
    snprintf(SOSD.daemon.lock_file, SOS_DEFAULT_STRING_LEN, "%s/%s.local.lock", SOSD.daemon.work_dir, SOSD.daemon.name);
    #endif
    sos_daemon_lock_fptr = open(SOSD.daemon.lock_file, O_RDWR | O_CREAT, 0640);
    if (sos_daemon_lock_fptr < 0) {
        fprintf(stderr, "\nERROR!  Unable to start daemon (%s): Could not access lock file %s in directory %s\n", SOSD.daemon.name, SOSD.daemon.lock_file, SOSD.daemon.work_dir);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    if (lockf(sos_daemon_lock_fptr, F_TLOCK, 0) < 0) {
        fprintf(stderr, "\nERROR!  Unable to start daemon (%s): AN INSTANCE IS ALREADY RUNNING!\n", SOSD.daemon.name);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("Lock file obtained.  (%s)\n", SOSD.daemon.lock_file); fflush(stdout); }


    /* [log file]
     *      system logging initialize
     */
    #if (SOSD_CLOUD_SYNC > 0)
    snprintf(SOSD.daemon.log_file, SOS_DEFAULT_STRING_LEN, "%s.%d.log", SOSD.daemon.name, SOS->config.comm_rank);
    #else
    snprintf(SOSD.daemon.log_file, SOS_DEFAULT_STRING_LEN, "%s.local.log", SOSD.daemon.name);
    #endif
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("Opening log file: %s\n", SOSD.daemon.log_file); fflush(stdout); }
    sos_daemon_log_fptr = fopen(SOSD.daemon.log_file, "w"); /* Open a log file, even if we don't use it... */
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("  ... done.\n"); fflush(stdout); }



    if (!SOSD_ECHO_TO_STDOUT) {
        dlog(1, "Logging output up to this point has been suppressed, but all initialization has gone well.\n");
        dlog(1, "Log file is now open.  Proceeding...\n");
        dlog(1, "SOSD_init():\n");
    }

    /* [mode]
     *      interactive or detached/daemon
     */
    #if (SOSD_DAEMON_MODE > 0)
    {
    dlog(1, "  ...mode: DETACHED DAEMON (fork/umask/sedsid)\n");
        /* [fork]
         *     split off from the parent process (& terminate parent)
         */
        ppid = getpid();
        pid  = fork();
        
        if (pid < 0) {
            dlog(0, "ERROR! Unable to start daemon (%s): Could not fork() off parent process.\n", SOSD.daemon.name);
            exit(EXIT_FAILURE);
        }
        if (pid > 0) { exit(EXIT_SUCCESS); } //close the parent
        
        /* [child session]
         *     create/occupy independent session from parent process
         */
        umask(0);
        sid = setsid();
        if (sid < 0) {
            dlog(0, "ERROR!  Unable to start daemon (%s): Could not acquire a session id.\n", SOSD_DAEMON_NAME); 
            exit(EXIT_FAILURE);
        }
        if ((chdir(SOSD.daemon.work_dir)) < 0) {
            dlog(0, "ERROR!  Unable to start daemon (%s): Could not change to working directory: %s\n", SOSD_DAEMON_NAME, SOSD.daemon.work_dir);
            exit(EXIT_FAILURE);
        }

        /* [file handles]
         *     close unused IO handles
         */
        if (SOS_DEBUG < 1) {
            dlog(1, "Closing traditional I/O for the daemon...\n");
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }

        
        dlog(1, "  ... session(%d) successfully split off from parent(%d).\n", getpid(), ppid);
    }
    #else
    {
        dlog(1, "  ... mode: ATTACHED INTERACTIVE\n");
    }
    #endif

    sprintf(SOSD.daemon.pid_str, "%d", getpid());
    dlog(1, "  ... pid: %s\n", SOSD.daemon.pid_str);

    /* Now we can write our PID out to the lock file safely... */
    rc = write(sos_daemon_lock_fptr, SOSD.daemon.pid_str, strlen(SOSD.daemon.pid_str));



    /* [guid's]
     *     configure the issuer of guids for this daemon
     */
    dlog(1, "Obtaining this instance's guid range...\n");
    #if (SOSD_CLOUD_SYNC > 0)
    SOS_guid guid_block_size = (SOS_guid) (SOS_DEFAULT_UID_MAX / (SOS_guid) SOS->config.comm_size);
        SOS_guid guid_my_first   = (SOS_guid) SOS->config.comm_rank * guid_block_size;
        SOS_uid_init(SOS, &SOSD.guid, guid_my_first, (guid_my_first + (guid_block_size - 1)));
    #else
        dlog(1, "DATA NOTE:  Running in local mode, CLOUD_SYNC is disabled.\n");
        dlog(1, "DATA NOTE:  GUID values are unique only to this node.\n");
        SOS_uid_init(&SOSD.guid, 1, SOS_DEFAULT_UID_MAX);
    #endif
    dlog(1, "  ... (%" SOS_GUID_FMT " ---> %" SOS_GUID_FMT ")\n", SOSD.guid->next, SOSD.guid->last);

    /* [hashtable]
     *    storage system for received pubs.  (will enque their key -> db)
     */
    dlog(1, "Setting up a hash table for pubs...\n");
    SOSD.pub_table = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(1, "Daemon initialization is complete.\n");
    SOSD.daemon.running = 1;
    return;
}



 void SOSD_sync_context_init(SOS_runtime *sos_context, SOSD_sync_context *sync_context, size_t elem_size, void* (*thread_func)(void *thread_param)) {
    SOS_SET_CONTEXT(sos_context, "SOSD_sync_context_init");

    sync_context->sos_context = sos_context;
    SOS_pipe_init(SOS, &sync_context->queue, elem_size);
    sync_context->handler = (pthread_t *) malloc(sizeof(pthread_t));
    sync_context->lock    = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    sync_context->cond    = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));

    pthread_mutex_init(sync_context->lock, NULL);
    pthread_cond_init(sync_context->cond, NULL);
    pthread_create(sync_context->handler, NULL, (void *) thread_func, (void *) sync_context);

    return;
}


void SOSD_claim_guid_block(SOS_uid *id, int size, long *pool_from, long *pool_to) {
    SOS_SET_CONTEXT(id->sos_context, "SOSD_guid_claim_range");

    pthread_mutex_lock( id->lock );

    if ((id->next + size) > id->last) {
        /* This is basically a failure case if any more GUIDs are requested. */
        *pool_from = id->next;
        *pool_to   = id->last;
        id->next   = id->last + 1;
    } else {
        *pool_from = id->next;
        *pool_to   = id->next + size;
        id->next   = id->next + size + 1;
    }

    pthread_mutex_unlock( id->lock );

    return;
}


void SOSD_apply_announce( SOS_pub *pub, SOS_buffer *buffer ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_apply_announce");

    dlog(6, "Calling SOS_announce_from_buffer()...\n");
    SOS_announce_from_buffer(buffer, pub);

    return;
}


void SOSD_apply_publish( SOS_pub *pub, SOS_buffer *buffer ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_apply_publish");

    dlog(6, "Calling SOS_publish_from_buffer()...\n");
    SOS_publish_from_buffer(buffer, pub, pub->snap_queue);

    return;
}





void SOSD_display_logo(void) {
    int choice = 0;

    srand(getpid());
    choice = rand() % 6;

    printf("--------------------------------------------------------------------------------\n");
    printf("\n");

    switch (choice) {
    case 0:
        printf("               _/_/_/    _/_/      _/_/_/    ))) ))  )    Scalable\n");
        printf("            _/        _/    _/  _/          ((( ((  (     Observation\n");
        printf("             _/_/    _/    _/    _/_/        ))) ))  )    System\n");
        printf("                _/  _/    _/        _/      ((( ((  (     for Scientific\n");
        printf("         _/_/_/      _/_/    _/_/_/          ))) ))  )    Workflows\n");
        break;

    case 1:
        printf("     @@@@@@    @@@@@@    @@@@@@   @@@@@@@@  @@@        @@@@@@   @@@  @@@  @@@\n");
        printf("    @@@@@@@   @@@@@@@@  @@@@@@@   @@@@@@@@  @@@       @@@@@@@@  @@@  @@@  @@@\n");
        printf("    !@@       @@!  @@@  !@@       @@!       @@!       @@!  @@@  @@!  @@!  @@!\n");
        printf("    !@!       !@!  @!@  !@!       !@!       !@!       !@!  @!@  !@!  !@!  !@!\n");
        printf("    !!@@!!    @!@  !@!  !!@@!!    @!!!:!    @!!       @!@  !@!  @!!  !!@  @!@\n");
        printf("     !!@!!!   !@!  !!!   !!@!!!   !!!!!:    !!!       !@!  !!!  !@!  !!!  !@!\n");
        printf("         !:!  !!:  !!!       !:!  !!:       !!:       !!:  !!!  !!:  !!:  !!:\n");
        printf("        !:!   :!:  !:!      !:!   :!:        :!:      :!:  !:!  :!:  :!:  :!:\n");
        printf("    :::: ::   ::::: ::  :::: ::    ::        :: ::::  ::::: ::   :::: :: ::: \n");
        printf("    :: : :     : :  :   :: : :     :        : :: : :   : :  :     :: :  : :  \n");
        printf("\n");
        printf("       |                                                                |    \n");
        printf("    -- + --   Scalable Observation System for Scientific Workflows   -- + -- \n");
        printf("       |                                                                |    \n");
        break;

    case 2:
        printf("      {__ __      {____       {__ __      {__ {__                      \n");
        printf("    {__    {__  {__    {__  {__    {__  {_    {__                      \n");
        printf("     {__      {__        {__ {__      {_{_ {_ {__   {__    {__     {___\n");
        printf("       {__    {__        {__   {__      {__   {__ {__  {__  {__  _  {__\n");
        printf("          {__ {__        {__      {__   {__   {__{__    {__ {__ {_  {__\n");
        printf("    {__    {__  {__     {__ {__    {__  {__   {__ {__  {__  {_ {_ {_{__\n");
        printf("      {__ __      {____       {__ __    {__  {___   {__    {___    {___\n");
        printf("\n");
        printf("     [...Scalable..Observation..System..for..Scientific..Workflows...]\n");
        break;

    case 3:
        printf("               ._____________________________________________________ ___  _ _\n");
        printf("              /_____/\\/\\/\\/\\/\\____/\\/\\/\\/\\______/\\/\\/\\/\\/\\________ ___ _ _\n");
        printf("             /___/\\/\\__________/\\/\\____/\\/\\__/\\/\\_______________ ___  _   __ _\n");
        printf("            /_____/\\/\\/\\/\\____/\\/\\____/\\/\\____/\\/\\/\\/\\_________  _ __\n");
        printf("           /___________/\\/\\__/\\/\\____/\\/\\__________/\\/\\_______ ___ _  __ _   _\n");
        printf("          /___/\\/\\/\\/\\/\\______/\\/\\/\\/\\____/\\/\\/\\/\\/\\_________ ___ __ _  _   _\n");
        printf("         /_____________________________________________________ _  _      _\n");
        printf("        /___/\\/\\/\\__/\\/\\___________________________________ ___ _ __ __  _    _\n");
        printf("       /___/\\/\\______/\\/\\______/\\/\\/\\____/\\/\\______/\\/\\___ ___ _ ___ __ _  _\n");
        printf("      /___/\\/\\/\\____/\\/\\____/\\/\\__/\\/\\__/\\/\\__/\\__/\\/\\___ __ _ _  _  _  _\n");
        printf("     /___/\\/\\______/\\/\\____/\\/\\__/\\/\\__/\\/\\/\\/\\/\\/\\/\\____ __ ___  _\n");
        printf("    /___/\\/\\______/\\/\\/\\____/\\/\\/\\______/\\/\\__/\\/\\_____ ___     _\n");
        printf("   |__________________________________________________ ___ _  _ _  ___  _\n");
        printf("\n");
        printf("   * * *   Scalable Observation System for Scientific Workflows   * * *\n");
        break;


    case 4:
        printf("_._____. .____  .____.._____. _  ..\n");
        printf("__  ___/_  __ \\_  ___/__  __/__  /________      __    .:|   Scalable\n");
        printf(".____ \\_  / / /.___ \\__  /_ ._  /_  __ \\_ | /| / /    .:|   Observation\n");
        printf("____/ // /_/ /.___/ /_  __/ _  / / /_/ /_ |/ |/ /     .:|   System for Scientific\n");
        printf("/____/ \\____/ /____/ /_/    /_/  \\____/____/|__/      .:|   Workflows\n");
        break;

    case 5:
        printf("     O)) O)      O))))       O)) O)      O)) O))                      \n");
        printf("   O))    O))  O))    O))  O))    O))  O)    O))                      \n");
        printf("    O))      O))        O)) O))      O)O) O) O))   O))    O))     O)))\n");
        printf("      O))    O))        O))   O))      O))   O)) O))  O))  O))     O))\n");
        printf("         O)) O))        O))      O))   O))   O))O))    O)) O)) O)  O))\n");
        printf("   O))    O))  O))     O)) O))    O))  O))   O)) O))  O))  O) O) O)O))\n");
        printf("     O)) O)      O))))       O)) O)    O))  O)))   O))    O)))    O)))\n");
        printf("\n");
        printf("       +------------------------------------------------------+\n");
        printf("       | Scalable Observation System for Scientific Workflows |\n");
        printf("       +------------------------------------------------------+\n");
        printf("\n");
        break;
    }

    printf("\n");
    printf("   Version: %s\n", SOS_VERSION);
    printf("   Builder: %s\n", SOS_BUILDER);
    printf("\n");
    printf("--------------------------------------------------------------------------------\n");

    return;
}
