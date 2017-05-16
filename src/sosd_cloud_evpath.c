
#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sosd.h"
#include "sosd_cloud_evpath.h"
#include "string.h"
#include "evpath.h"

bool SOSD_evpath_ready_to_listen = false;
bool SOSD_cloud_shutdown_underway = false;
void SOSD_evpath_register_connection(SOS_buffer *msg);

// Extract the buffer from EVPath and drop it into the SOSD
// message processing queue:
static int
SOSD_evpath_message_handler(
    CManager cm,
    void *vevent,
    void *client_data,
    attr_list attrs)
{
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_evpath_message_handler");
    buffer_rec_ptr evp_buffer = vevent;

    SOS_msg_header    header;
    SOS_buffer       *buffer; 
    SOS_buffer_init_sized_locking(SOS, &buffer, (evp_buffer->size + 1), false);
    memcpy(buffer->data, evp_buffer->data, evp_buffer->size);

    int entry        = 0;
    int entry_count  = 0;
    int displaced    = 0;
    int offset       = 0;

    SOS_buffer_unpack(buffer, &offset, "i", &entry_count);
    dlog(1, "  ... message contains %d entries.\n", entry_count);

    // Extract one-at-a-time single messages into 'msg'
    for (entry = 0; entry < entry_count; entry++) {
        dlog(1, "[ccc] ... processing entry %d of %d @ offset == %d \n",
            (entry + 1), entry_count, offset);
        memset(&header, '\0', sizeof(SOS_msg_header));
        displaced = SOS_buffer_unpack(buffer, &offset, "iigg",
                &header.msg_size,
                &header.msg_type,
                &header.msg_from,
                &header.pub_guid);
        dlog(1, "     ... header.msg_size == %d\n",
            header.msg_size);
        dlog(1, "     ... header.msg_type == %s  (%d)\n",
            SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_type);
        dlog(1, "     ... header.msg_from == %" SOS_GUID_FMT "\n",
            header.msg_from);
        dlog(1, "     ... header.pub_guid == %" SOS_GUID_FMT "\n",
            header.pub_guid);

        offset -= displaced;


        //Create a new message buffer:
        SOS_buffer *msg;
        SOS_buffer_init_sized_locking(SOS, &msg, (1 + header.msg_size), false);

        dlog(1, "[ccc] (%d of %d) <<< bringing in msg(%15s).size == %d from offset:%d\n",
                (entry + 1), entry_count, SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE),
                header.msg_size, offset);

        //Copy the data into the new message directly:
        memcpy(msg->data, (buffer->data + offset), header.msg_size);
        msg->len = header.msg_size;
        offset += header.msg_size;

        //Enqueue this new message into the local_sync:
        switch (header.msg_type) {
            case SOS_MSG_TYPE_ANNOUNCE:
            case SOS_MSG_TYPE_PUBLISH:
            case SOS_MSG_TYPE_VAL_SNAPS:
                pthread_mutex_lock(SOSD.sync.local.queue->sync_lock);
                pipe_push(SOSD.sync.local.queue->intake, &msg, 1);
                SOSD.sync.local.queue->elem_count++;
                pthread_mutex_unlock(SOSD.sync.local.queue->sync_lock);
                break;

            case SOS_MSG_TYPE_REGISTER:
                SOSD_evpath_register_connection(msg);
                break;

            case SOS_MSG_TYPE_SHUTDOWN:
                SOSD.daemon.running = 0;
                SOSD.sos_context->status = SOS_STATUS_SHUTDOWN;
                SOS_buffer *shutdown_msg;
                SOS_buffer *shutdown_rep;
                SOS_buffer_init_sized_locking(SOS, &shutdown_msg, 1024, false);
                SOS_buffer_init_sized_locking(SOS, &shutdown_rep, 1024, false);
                offset = 0;
                SOS_buffer_pack(shutdown_msg, &offset, "i", offset);
                SOSD_send_to_self(shutdown_msg, shutdown_rep);
                SOS_buffer_destroy(shutdown_msg);
                SOS_buffer_destroy(shutdown_rep);
                break;

            case SOS_MSG_TYPE_TRIGGERPULL:
                SOSD_evpath_handle_triggerpull(msg);
                break;

            case SOS_MSG_TYPE_ACK:
                fprintf(stderr, "sosd(%d) received ACK message"
                    " from rank %" SOS_GUID_FMT " !\n",
                        SOSD.sos_context->config.comm_rank, header.msg_from);
                fflush(stdout);
                break;

            default:    SOSD_handle_unknown    (msg); break;
        }
    }

    return 0;
}


void SOSD_evpath_register_connection(SOS_buffer *msg) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_evpath_register_connection");

    dlog(3, "Registering a new connection...");

    SOS_msg_header header;
    int offset = 0;
    SOS_buffer_unpack(msg, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    SOSD_evpath *evp = &SOSD.daemon.evpath;
    SOSD_evpath_node *node = evp->node[header.msg_from];

    node->contact_string = NULL;
    SOS_buffer_unpack_safestr(msg, &offset, &node->contact_string);

    dlog(3, "   ... sosd(%" SOS_GUID_FMT ") gave us contact string: %s\n",
        header.msg_from,
        node->contact_string);


    dlog(3, "   ... constructing stone path: ");
    node->cm = CManager_create();
    CMlisten(node->cm);
    CMfork_comm_thread(node->cm);
   
    node->out_stone    = EValloc_stone(node->cm);
    node->contact_list = attr_list_from_string(node->contact_string);
    EVassoc_bridge_action(
        node->cm,
        node->out_stone,
        node->contact_list,
        node->rmt_stone);
    node->src = EVcreate_submit_handle(
        node->cm,
        node->out_stone,
        SOSD_buffer_format_list);

    node->active = true;
    dlog(3, "done.\n");

    // Send them back an ACK.

    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &reply, 128, false);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_ACK;
    header.msg_from = SOS->config.comm_rank;
    header.pub_guid = 0;

    int msg_count = 1;
    offset = 0;
    SOS_buffer_pack(reply, &offset, "iiigg",
        msg_count,
        header.msg_size,
        header.msg_type,
        header.msg_from,
        header.pub_guid);

    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(reply, &offset, "ii",
        msg_count,
        header.msg_size);

    buffer_rec rec;
    rec.data = (unsigned char *) reply->data;
    rec.size = reply->len; 
    EVsubmit(node->src, &rec, NULL);

    dlog(3, "Registration complete.\n");

    return;
}


// NOTE: Right now this only supports triggers being pulled
//   on the aggregator nodes.
void SOSD_evpath_handle_triggerpull(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_evpath_handle_triggerpull");

    SOS_msg_header header;
    int offset = 0;
    SOS_buffer_unpack(msg, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);
    
     if (SOS->role == SOS_ROLE_AGGREGATOR) {

        // AGGREGATOR
        
        SOSD_evpath *evp = &SOSD.daemon.evpath;
        buffer_rec rec;

        dlog(2, "Wrapping the trigger message...\n");

        SOS_buffer *wrapped_msg;
        SOS_buffer_init_sized_locking(SOS, &wrapped_msg,
                (msg->len + 128), false);

        int msg_count = 1;
        offset = 0;
        SOS_buffer_pack(wrapped_msg, &offset, "i", msg_count);
        memcpy((wrapped_msg->data + offset), msg->data, msg->len);

        header.msg_size += (offset + msg->len);
        msg_count = 1;
        offset = 0;
        SOS_buffer_pack(wrapped_msg, &offset, "ii", msg_count, header.msg_size);

        int id = 0;
        for (id = 0; id < SOS->config.comm_size; id++) {
            if (evp->node[id]->active == true) {
                dlog(2, "   ...sending feedback msg to sosd(%d).\n", id);
                rec.data = (unsigned char *) wrapped_msg->data;
                rec.size = wrapped_msg->len; 
                EVsubmit(evp->node[id]->src, &rec, NULL);
            }
        }

    } else if (SOS->role == SOS_ROLE_LISTENER) {

        // LISTENER

        int data_length = -1;
        int data_offset = offset;

        SOS_buffer_unpack(msg, &offset, "i", &data_length);
        char *data = calloc(data_length + 1, sizeof(char));
        offset = data_offset;
        SOS_buffer_unpack(msg, &offset, "b", &data);

        fprintf(stderr, "sosd(%d) got a TRIGGERPULL message from"
                " sosd(%" SOS_GUID_FMT "): %s\n",
                SOS->config.comm_rank,
                header.msg_from,
                data);
        fflush(stderr);

    }

    SOS_buffer_destroy(msg);

    return;
}


/* name.........: SOSD_cloud_init
 * parameters...: argc, argv (passed in by address)
 * return val...: 0 if no errors
 * description..:
 *     This routine stands up the off-node transport services for the daemon
 *     and launches any particular threads it needs to in order to do that.
 *
 *     In the MPI-version, this function is responsible for populating the
 *     following global values.  Some reasonable values will at least need
 *     to be plugged into the SOS->config.* variables.
 *
 *        SOS->config.comm_rank
 *        SOS->config.comm_size
 *        SOS->config.comm_support = MPI_THREAD_*
 *        SOSD.daemon.cloud_sync_target_set[n]  (int: rank)
 *        SOSD.daemon.cloud_sync_target_count
 *        SOSD.daemon.cloud_sync_target
 *
 *    The SOSD.daemon.cloud_sync stuff can likely change here, if EVPATH
 *    is going to handle it's business differently.  The sync_target refers
 *    to the centralized store (here, stone?) that this daemon is pointing to
 *    for off-node transport.  The general system allows for multiple "back-
 *    plane stores" launched alongside the daemons, to provide reasonable
 *    scalability and throughput.
 */
int SOSD_cloud_init(int *argc, char ***argv) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_init.EVPATH");

    SOSD_evpath_ready_to_listen = false;
    SOSD_evpath *evp = &SOSD.daemon.evpath;

    evp->meetup_path = NULL;
    evp->meetup_path = getenv("SOS_EVPATH_MEETUP");
    if ((evp->meetup_path == NULL) || (strlen(evp->meetup_path) < 1)) {
        evp->meetup_path = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);
        if (!getcwd(evp->meetup_path, SOS_DEFAULT_STRING_LEN)) {
            fprintf(stderr, "ERROR: The SOS_EVPATH_MEETUP evar was not set,"
                    " and getcwd() failed! Set the evar and retry.\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "STATUS: The SOS_EVPATH_MEETUP evar was not set."
                " Using getcwd() path:\n\t%s\n", evp->meetup_path);
        fflush(stderr);
    }

    int expected_node_count =
        SOSD.daemon.aggregator_count + 
        SOSD.daemon.listener_count;

    SOS->config.comm_size = expected_node_count;;
    SOS->config.comm_support = -1; // Used for MPI only.

    // Do some sanity checks.

    if (SOSD.daemon.aggregator_count == 0) {
        fprintf(stderr, "ERROR: SOS requires an aggregator.\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }


    // The cloud sync stuff gets calculated after we know
    // how many targets have connected as aggregators,
    // and have assigned the aggregators their internal rank
    // indices...
    SOSD.daemon.cloud_sync_target_count = SOSD.daemon.aggregator_count;

    dlog(0, "Initializing EVPath...\n");

    int aggregation_rank = -1;
    if (SOSD.sos_context->role == SOS_ROLE_AGGREGATOR) {
        aggregation_rank = SOSD.sos_context->config.comm_rank;
        SOSD.daemon.cloud_sync_target = -1;
    } else {
        aggregation_rank = SOSD.sos_context->config.comm_rank
            % SOSD.daemon.aggregator_count;
        SOSD.daemon.cloud_sync_target = aggregation_rank;
    }
    dlog(0, "   ... aggregation_rank: %d\n", aggregation_rank);

    char *contact_filename = (char *) calloc(2048, sizeof(char));
    snprintf(contact_filename, 2048, "%s/sosd.%05d.key",
        evp->meetup_path, aggregation_rank);
    dlog(0, "   ... contact_filename: %s\n", contact_filename);

    dlog(0, "   ... creating connection manager:\n");
    dlog(0, "      ... evp->recv.cm\n");
    evp->recv.cm = CManager_create();
    CMlisten(evp->recv.cm);
    SOSD_evpath_ready_to_listen = true;
    CMfork_comm_thread(evp->recv.cm);
    dlog(0, "      ... configuring stones:\n");
    evp->recv.out_stone = EValloc_stone(evp->recv.cm);
    EVassoc_terminal_action(
            evp->recv.cm,
            evp->recv.out_stone,
            SOSD_buffer_format_list,
            SOSD_evpath_message_handler,
            NULL);
    dlog(0, "      ... evp->send.cm\n");
    evp->send.cm = CManager_create();
    CMlisten(evp->send.cm);
    CMfork_comm_thread(evp->send.cm);
    evp->send.out_stone = EValloc_stone(evp->send.cm);
    dlog(0, "      ... done.\n");
    dlog(0, "  ... done.\n");

    // Get the location we're listening on...
    evp->recv.contact_string =
        attr_list_to_string(CMget_contact_list(evp->recv.cm));
    fprintf(stderr, "\n\nNOTE: sosd(%d) evp->recv.contact_string: %s\n\n\n",
        SOSD.sos_context->config.comm_rank, evp->recv.contact_string);
    fflush(stderr);

    if (SOSD.sos_context->role == SOS_ROLE_AGGREGATOR) {

        // AGGREGATOR
        //   ... the aggregator needs to wait on the registration messages
        //   before being able to create sending stones.

        dlog(0, "   ... demon role: AGGREGATOR\n");
        // Make space to track connections back to the listeners:
        dlog(0, "   ... creating objects to coordinate with listeners: ");
        evp->node = (SOSD_evpath_node **)
            malloc(expected_node_count * sizeof(SOSD_evpath_node *));
        int node_idx = 0; 
        for (node_idx = 0; node_idx < expected_node_count; node_idx++) {
            // Allocate space to store returning connections to clients...
            // NOTE: Fill in later, as clients connect.
            evp->node[node_idx] =
                (SOSD_evpath_node *) calloc(1, sizeof(SOSD_evpath_node));
            snprintf(evp->node[node_idx]->name, 256, "%d", node_idx);
            evp->node[node_idx]->active            = false;
            evp->node[node_idx]->contact_string    = NULL;
            evp->node[node_idx]->src               = NULL;
            evp->node[node_idx]->out_stone         = 0;
            evp->node[node_idx]->rmt_stone         = 0;
        }
        dlog(0, "done.\n");

        FILE *contact_file;
        contact_file = fopen(contact_filename, "w");
        fprintf(contact_file, "%s", evp->recv.contact_string);
        fflush(contact_file);
        fclose(contact_file);

    } else {

        //LISTENER

        dlog(0, "   ... waiting for coordinator to share contact"
                " information.\n");
        while (!SOS_file_exists(contact_filename)) {
            usleep(100000);
        }

        evp->send.contact_string = (char *)calloc(1024, sizeof(char));
        while(strnlen(evp->send.contact_string, 1024) < 1) {
            FILE *contact_file;
            contact_file = fopen(contact_filename, "r");
            if (fgets(evp->send.contact_string, 1024, contact_file) == NULL) {
                dlog(0, "Error reading the contact key file. Aborting.\n");
                exit(EXIT_FAILURE);
            }
            fclose(contact_file);
            usleep(500000);
        }

        dlog(0, "   ... targeting aggregator at: %s\n", evp->send.contact_string);
        dlog(0, "   ... configuring stones:\n");
        evp->send.contact_list = attr_list_from_string(evp->send.contact_string);
        dlog(0, "      ... try: bridge action.\n");
        EVassoc_bridge_action(
            evp->send.cm,
            evp->send.out_stone,
            evp->send.contact_list,
            evp->send.rmt_stone);
        dlog(0, "      ... try: submit handle.\n");
        evp->send.src = EVcreate_submit_handle(
            evp->send.cm,
            evp->send.out_stone,
            SOSD_buffer_format_list);
        dlog(0, "done.\n");

        // evp->send.src is where we drop messages to send...
        // Example:  EVsubmit(evp->source, &msg, NULL);
        SOS_buffer *buffer;
        SOS_buffer_init_sized_locking(SOS, &buffer, 2048, false);

        SOS_msg_header header;
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = SOSD.sos_context->config.comm_rank;
        header.pub_guid = 0;

        int msg_count = 1;

        int offset = 0;
        SOS_buffer_pack(buffer, &offset, "iiigg",
            msg_count,
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);

        SOS_buffer_pack(buffer, &offset, "s", evp->recv.contact_string);

        header.msg_size = offset;
        offset = 0;
        
        SOS_buffer_pack(buffer, &offset, "ii",
            msg_count,
            header.msg_size);

        SOSD_cloud_send(buffer, NULL);
        SOS_buffer_destroy(buffer);
    }

    free(contact_filename);
    dlog(0, "   ... done.\n");

    return 0;
}


/* name.......: SOSD_cloud_start
 * description: In the event that initialization and activation are not
 *    necessarily the same, when this function returns the communication
 *    between sosd instances is active, and all cloud functions are
 *    operating.
 */
int SOSD_cloud_start(void) {
    return 0;
}



/* name.......: SOSD_cloud_send
 * description: Send a message to the target aggregator.
 *              NOTE: For EVPath, the reply buffer is not used,
 *              since it has an async communication model.
 */
int SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_send.EVPATH");

    buffer_rec rec;
    rec.data = (unsigned char *) buffer->data;
    rec.size = buffer->len; 
    EVsubmit(SOSD.daemon.evpath.send.src, &rec, NULL);

    return 0;
}


/* name.......: SOSD_cloud_enqueue
 * description: Accept a message into the async send-queue.  (non-blocking)
 *              The purpose of this abstraction is to eventually allow
 *              SOSD to manage the bundling of multiple messages before
 *              passing them off to the underlying transport API. In the
 *              case of fine-grained messaging layers like EVPath, this
 *              is likely overkill, but nevertheless, here it is.
 */
void  SOSD_cloud_enqueue(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_enqueue");
    SOS_msg_header header;
    int offset;

    if (SOSD_cloud_shutdown_underway) { return; }
    if (buffer->len == 0) {
        dlog(1, "ERROR: You attempted to enqueue a zero-length message.\n");
        return;
    }

    memset(&header, '\0', sizeof(SOS_msg_header));

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);

    dlog(6, "Enqueueing a %s message of %d bytes...\n", SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_size);
    if (buffer->len != header.msg_size) { dlog(1, "  ... ERROR: buffer->len(%d) != header.msg_size(%d)", buffer->len, header.msg_size); }

    pthread_mutex_lock(SOSD.sync.cloud_send.queue->sync_lock);
    pipe_push(SOSD.sync.cloud_send.queue->intake, (void *) &buffer, sizeof(SOS_buffer *));
    SOSD.sync.cloud_send.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.cloud_send.queue->sync_lock);

    dlog(1, "  ... done.\n");
   return;
}


/* name.......: SOSD_cloud_fflush
 * description: Force the send-queue to flush and transmit.
 * note.......: With EVPath, this might be totally unnecessary.  (i.e. "Let EVPath handle it...")
 */
void  SOSD_cloud_fflush(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_fflush.EVPATH");

    // NOTE: This not used with EVPath.

    return;
}


/* name.......: SOSD_cloud_finalize
 * description: Shut down the cloud operation, flush / close files, etc.
 */
int   SOSD_cloud_finalize(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_finalize.EVPATH");

    SOSD_evpath *evp = &SOSD.daemon.evpath;

    if (SOSD.sos_context->role != SOS_ROLE_AGGREGATOR) {
        return 0;
    }
    char *contact_filename = (char *) calloc(2048, sizeof(char));
    snprintf(contact_filename, 2048, "%s/sosd.%05d.key",
        evp->meetup_path, SOS->config.comm_rank);
    dlog(1, "   Removing key file: %s\n", contact_filename);

    if (remove(contact_filename) == -1) {
        dlog(0, "   Error, unable to delete key file!\n");
    }

    return 0;
}


/* name.......: SOSD_cloud_shutdown_notice
 * description: Send notifications to any daemon ranks that are not in the
 *              business of listening to the node on the SOS_CMD_PORT socket.
 *              Only certain daemon ranks participate/call this function.
 */
void  SOSD_cloud_shutdown_notice(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_shutdown_notice.EVPATH");

    SOS_buffer *shutdown_msg;
    SOS_buffer_init(SOS, &shutdown_msg);

    dlog(1, "Providing shutdown notice to the cloud_sync backend...\n");
    SOSD_cloud_shutdown_underway = true;

    if ((SOS->config.comm_rank - SOSD.daemon.cloud_sync_target_count)
            < SOSD.daemon.cloud_sync_target_count)
    {
        dlog(1, "  ... preparing notice to SOS_ROLE_AGGREGATOR at"
                " rank %d\n", SOSD.daemon.cloud_sync_target);
        /* The first N listener ranks will notify the N aggregators... */
        SOS_msg_header header;
        SOS_buffer    *shutdown_msg;
        SOS_buffer    *reply;
        int            embedded_msg_count;
        int            offset;
        int            msg_inset;

        SOS_buffer_init(SOS, &shutdown_msg);
        SOS_buffer_init_sized_locking(SOS, &reply, 10, false);

        embedded_msg_count = 1;
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
        header.msg_from = SOS->my_guid;
        header.pub_guid = 0;

        offset = 0;
        SOS_buffer_pack(shutdown_msg, &offset, "i", embedded_msg_count);
        msg_inset = offset;

        header.msg_size = SOS_buffer_pack(shutdown_msg, &offset, "iigg",
                                          header.msg_size,
                                          header.msg_type,
                                          header.msg_from,
                                          header.pub_guid);
        offset = 0;
        SOS_buffer_pack(shutdown_msg, &offset, "ii",
                        embedded_msg_count,
                        header.msg_size);

        dlog(1, "  ... sending notice\n");
        SOSD_cloud_send(shutdown_msg, reply); 
        dlog(1, "  ... sent successfully\n");

        SOS_buffer_destroy(shutdown_msg);
        SOS_buffer_destroy(reply);

    }
    
    dlog(1, "  ... done\n");

    return;
}


/* name.......: SOSD_cloud_listen_loop
 * description: When there is a feedback/control mechanism in place
 *              between the daemons and a heirarchical authority / policy
 *              enactment chain, this will be the loop that is monitoring
 *              incoming messages from other sosd daemon instances.
 */
void  SOSD_cloud_listen_loop(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_listen_loop");

    while(!SOSD_evpath_ready_to_listen) {
            usleep(50000);
    }

    return;
}


