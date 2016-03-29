/*
 *  sosd_db_sqlite.c
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sqlite3.h"

#include "sos.h"
#include "sos_debug.h"
#include "sosd_db_sqlite.h"


#define SOSD_DB_PUBS_TABLE_NAME "tblPubs"
#define SOSD_DB_DATA_TABLE_NAME "tblData"
#define SOSD_DB_VALS_TABLE_NAME "tblVals"
#define SOSD_DB_ENUM_TABLE_NAME "tblEnums"
#define SOSD_DB_SOSD_TABLE_NAME "tblSOSDConfig"


void SOSD_db_insert_enum(const char *type, const char **var_strings, int max_index);


sqlite3      *database;
sqlite3_stmt *stmt_insert_pub;
sqlite3_stmt *stmt_insert_data;
sqlite3_stmt *stmt_insert_val;
sqlite3_stmt *stmt_insert_enum;
sqlite3_stmt *stmt_insert_sosd;


char *sql_create_table_pubs = ""                                        \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_PUBS_TABLE_NAME " ( "         \
    " row_id "          " INTEGER PRIMARY KEY, "                        \
    " guid "            " INTEGER, "                                    \
    " title "           " STRING, "                                     \
    " process_id "      " INTEGER, "                                    \
    " thread_id "       " INTEGER, "                                    \
    " comm_rank "       " INTEGER, "                                    \
    " node_id "         " STRING, "                                     \
    " prog_name "       " STRING, "                                     \
    " prog_ver "        " STRING, "                                     \
    " meta_channel "    " INTEGER, "                                    \
    " meta_nature "     " STRING, "                                     \
    " meta_layer "      " STRING, "                                     \
    " meta_pri_hint "   " STRING, "                                     \
    " meta_scope_hint " " STRING, "                                     \
    " meta_retain_hint "" STRING, "                                     \
    " pragma "          " STRING); ";

char *sql_create_table_data = ""                                        \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_DATA_TABLE_NAME " ( "         \
    " row_id "          " INTEGER PRIMARY KEY, "                        \
    " pub_guid "        " INTEGER, "                                    \
    " guid "            " INTEGER, "                                    \
    " name "            " STRING, "                                     \
    " val_type "        " STRING, "                                     \
    " meta_freq "       " STRING, "                                     \
    " meta_class "      " STRING, "                                     \
    " meta_pattern "    " STRING, "                                     \
    " meta_compare "    " STRING); ";

char *sql_create_table_vals = ""                                        \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_VALS_TABLE_NAME " ( "         \
    " row_id "          " INTEGER PRIMARY KEY, "                        \
    " guid "            " INTEGER, "                                    \
    " val "             " STRING, "                                     \
    " frame "           " INTEGER, "                                    \
    " meta_semantic "   " STRING, "                                     \
    " meta_mood "       " STRING, "                                     \
    " time_pack "       " DOUBLE, "                                     \
    " time_send "       " DOUBLE, "                                     \
    " time_recv "       " DOUBLE); ";


char *sql_create_table_enum = ""                                        \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_ENUM_TABLE_NAME " ( "         \
    " row_id "          " INTEGER PRIMARY KEY, "                        \
    " type "            " STRING, "                                     \
    " text "            " STRING, "                                     \
    " enum_val "        " INTEGER); ";

char *sql_create_table_sosd_config = ""                \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_SOSD_TABLE_NAME " ( " \
    " row_id "          " INTEGER PRIMARY KEY, "                \
    " daemon_id "       " STRING, "                             \
    " key "             " STRING, "                             \
    " value "           " STRING); ";



char *sql_insert_pub = ""                                               \
    "INSERT INTO " SOSD_DB_PUBS_TABLE_NAME " ("                         \
    " guid,"                                                            \
    " title,"                                                           \
    " process_id,"                                                      \
    " thread_id,"                                                       \
    " comm_rank,"                                                       \
    " node_id,"                                                         \
    " prog_name,"                                                       \
    " prog_ver,"                                                        \
    " meta_channel,"                                                    \
    " meta_nature,"                                                     \
    " meta_layer,"                                                      \
    " meta_pri_hint,"                                                   \
    " meta_scope_hint,"                                                 \
    " meta_retain_hint,"                                                \
    " pragma "                                                          \
    ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?); ";

const char *sql_insert_data = ""                                        \
    "INSERT INTO " SOSD_DB_DATA_TABLE_NAME " ("                         \
    " pub_guid,"                                                        \
    " guid,"                                                            \
    " name,"                                                            \
    " val_type,"                                                        \
    " meta_freq,"                                                       \
    " meta_class,"                                                      \
    " meta_pattern,"                                                    \
    " meta_compare "                                                    \
    ") VALUES (?,?,?,?,?,?,?,?); ";

const char *sql_insert_val = ""                                         \
    "INSERT INTO " SOSD_DB_VALS_TABLE_NAME " ("                         \
    " guid,"                                                            \
    " val, "                                                            \
    " frame, "                                                          \
    " meta_semantic, "                                                  \
    " meta_mood, "                                                      \
    " time_pack,"                                                       \
    " time_send,"                                                       \
    " time_recv "                                                       \
    ") VALUES (?,?,?,?,?,?,?,?); ";


const char *sql_insert_enum = ""                \
    "INSERT INTO " SOSD_DB_ENUM_TABLE_NAME " (" \
    " type,"                                    \
    " text,"                                    \
    " enum_val "                                \
    ") VALUES (?,?,?); ";

const char *sql_insert_sosd_config = ""         \
    "INSERT INTO " SOSD_DB_SOSD_TABLE_NAME " (" \
    " daemon_id, "                              \
    " key, "                                    \
    " value "                                   \
    ") VALUES (?,?,?); ";

char *sql_cmd_begin_transaction    = "BEGIN DEFERRED TRANSACTION;";
char *sql_cmd_commit_transaction   = "COMMIT TRANSACTION;";


void SOSD_db_init_database() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_init_database");
    int     retval;
    int     flags;

    dlog(1, "Opening database...\n");

    SOSD.db.ready = 0;
    SOSD.db.file  = (char *) malloc(SOS_DEFAULT_STRING_LEN);
    memset(SOSD.db.file, '\0', SOS_DEFAULT_STRING_LEN);

    SOSD.db.lock  = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init( SOSD.db.lock, NULL );
    pthread_mutex_lock( SOSD.db.lock );

    flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

    #if (SOSD_CLOUD_SYNC > 0)
    snprintf(SOSD.db.file, SOS_DEFAULT_STRING_LEN, "%s/%s.%d.db", SOSD.daemon.work_dir, SOSD.daemon.name, SOS->config.comm_rank);
    #else
    snprintf(SOSD.db.file, SOS_DEFAULT_STRING_LEN, "%s/%s.local.db", SOSD.daemon.work_dir, SOSD.daemon.name);
    #endif

    /*
     *   In unix-none mode, the database is accessible from multiple processes
     *   but you need to do (not currently in our code) explicit calls to SQLite
     *   locking functions (need to find out what they are.)   -CW
     *
     *   "unix-none"     =no locking (NOP's)
     *   "unix-excl"     =single-access only
     *   "unix-dotfile"  =uses a file as the lock.
     */

    retval = sqlite3_open_v2(SOSD.db.file, &database, flags, "unix-dotfile");
    if( retval ){
        dlog(0, "ERROR!  Can't open database: %s   (%s)\n", SOSD.db.file, sqlite3_errmsg(database));
        sqlite3_close(database);
        exit(EXIT_FAILURE);
    } else {
        dlog(1, "Successfully opened database.\n");
    }

    SOSD_db_create_tables();

    dlog(2, "Preparing transactions...\n");

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_pub);
    retval = sqlite3_prepare_v2(database, sql_insert_pub, strlen(sql_insert_pub) + 1, &stmt_insert_pub, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_data);
    retval = sqlite3_prepare_v2(database, sql_insert_data, strlen(sql_insert_data) + 1, &stmt_insert_data, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_val);
    retval = sqlite3_prepare_v2(database, sql_insert_val, strlen(sql_insert_val) + 1, &stmt_insert_val, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_enum);
    retval = sqlite3_prepare_v2(database, sql_insert_enum, strlen(sql_insert_enum) + 1, &stmt_insert_enum, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_sosd_config);
    retval = sqlite3_prepare_v2(database, sql_insert_sosd_config, strlen(sql_insert_sosd_config) + 1, &stmt_insert_sosd, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }


    SOSD.db.ready = 1;
    pthread_mutex_unlock(SOSD.db.lock);

    SOSD_db_transaction_begin();
    dlog(2, "  Inserting the enumeration table...\n");

    /* TODO: {DB, ENUM}  Make this a macro expansion... */
    SOSD_db_insert_enum("ROLE",          SOS_ROLE_string,          SOS_ROLE___MAX          );
    SOSD_db_insert_enum("TARGET",        SOS_TARGET_string,        SOS_TARGET___MAX        );
    SOSD_db_insert_enum("STATUS",        SOS_STATUS_string,        SOS_STATUS___MAX        );
    SOSD_db_insert_enum("MSG_TYPE",      SOS_MSG_TYPE_string,      SOS_MSG_TYPE___MAX      );
    SOSD_db_insert_enum("FEEDBACK",      SOS_FEEDBACK_string,      SOS_FEEDBACK___MAX      );
    SOSD_db_insert_enum("PRI",           SOS_PRI_string,           SOS_PRI___MAX           );
    SOSD_db_insert_enum("VAL_TYPE",      SOS_VAL_TYPE_string,      SOS_VAL_TYPE___MAX      );
    SOSD_db_insert_enum("VAL_STATE",     SOS_VAL_STATE_string,     SOS_VAL_STATE___MAX     );
    SOSD_db_insert_enum("VAL_FREQ",      SOS_VAL_FREQ_string,      SOS_VAL_FREQ___MAX      );
    SOSD_db_insert_enum("VAL_SEMANTIC",  SOS_VAL_SEMANTIC_string,  SOS_VAL_SEMANTIC___MAX  );
    SOSD_db_insert_enum("VAL_PATTERN",   SOS_VAL_PATTERN_string,   SOS_VAL_PATTERN___MAX   );
    SOSD_db_insert_enum("VAL_COMPARE",   SOS_VAL_COMPARE_string,   SOS_VAL_COMPARE___MAX   );
    SOSD_db_insert_enum("VAL_CLASS",     SOS_VAL_CLASS_string,     SOS_VAL_CLASS___MAX     );
    SOSD_db_insert_enum("MOOD",          SOS_MOOD_string,          SOS_MOOD___MAX          );
    SOSD_db_insert_enum("SCOPE",         SOS_SCOPE_string,         SOS_SCOPE___MAX         );
    SOSD_db_insert_enum("LAYER",         SOS_LAYER_string,         SOS_LAYER___MAX         );
    SOSD_db_insert_enum("NATURE",        SOS_NATURE_string,        SOS_NATURE___MAX        );
    SOSD_db_insert_enum("RETAIN",        SOS_RETAIN_string,        SOS_RETAIN___MAX        );

    SOSD_db_transaction_commit();

    dlog(2, "  ... done.\n");

    return;
}


void SOSD_db_close_database() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_close_database");

    dlog(2, "Closing database.   (%s)\n", SOSD.db.file);
    pthread_mutex_lock( SOSD.db.lock );
    dlog(2, "  ... finalizing statements.\n");
    SOSD.db.ready = 0;
    CALL_SQLITE (finalize(stmt_insert_pub));
    CALL_SQLITE (finalize(stmt_insert_data));
    CALL_SQLITE (finalize(stmt_insert_val));
    CALL_SQLITE (finalize(stmt_insert_enum));
    CALL_SQLITE (finalize(stmt_insert_sosd));
    dlog(2, "  ... closing database file.\n");
    sqlite3_close_v2(database);
    dlog(2, "  ... destroying the mutex.\n");
    pthread_mutex_unlock(SOSD.db.lock);
    pthread_mutex_destroy(SOSD.db.lock);
    free(SOSD.db.lock);
    free(SOSD.db.file);
    dlog(2, "  ... done.\n");

    return;
}


void SOSD_db_transaction_begin() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_transaction_begin");
    int   rc;
    char *err = NULL;

    pthread_mutex_lock(SOSD.db.lock);

    dlog(6, " > > > > > > > > > > > > > > > > > > > > > > > > > > > > \n");
    dlog(6, ">>>>>>>>>> >> >> >> BEGIN TRANSACTION >> >> >> >>>>>>>>>>\n");
    dlog(6, " > > > > > > > > > > > > > > > > > > > > > > > > > > > > \n");
    rc = sqlite3_exec(database, sql_cmd_begin_transaction, NULL, NULL, &err);
    if (rc) { dlog(2, "##### ERROR ##### : (%d)\n", rc); }

    return;
}


void SOSD_db_transaction_commit() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_transaction_commit");
    int   rc;
    char *err = NULL;

    dlog(6, " < < < < < < < < < < < < < < < < < < < < < < < < < < < <  \n");
    dlog(6, "<<<<<<<<<< << << << COMMIT TRANSACTION << << << <<<<<<<<<<\n");
    dlog(6, " < < < < < < < < < < < < < < < < < < < < < < < < < < < <  \n");
    rc = sqlite3_exec(database, sql_cmd_commit_transaction, NULL, NULL, &err);
    if (rc) { dlog(2, "##### ERROR ##### : (%d)\n", rc); }

    pthread_mutex_unlock(SOSD.db.lock);

    return;
}


void SOSD_db_insert_pub( SOS_pub *pub ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_insert_pub");
    int i;

    dlog(5, "Inserting pub(%ld)->data into database(%s).\n", pub->guid, SOSD.db.file);

    /*
     *  NOTE: SQLite3 behaves strangely unless you pass it variables stored on the stack.
     */

    long           guid              = pub->guid;
    char          *title             = pub->title;
    int            process_id        = pub->process_id;
    int            thread_id         = pub->thread_id;
    int            comm_rank         = pub->comm_rank;
    char          *node_id           = pub->node_id;
    char          *prog_name         = pub->prog_name;
    char          *prog_ver          = pub->prog_ver;
    int            meta_channel      = pub->meta.channel;
    const char    *meta_nature       = SOS_ENUM_STR(pub->meta.nature, SOS_NATURE);
    const char    *meta_layer        = SOS_ENUM_STR(pub->meta.layer, SOS_LAYER);
    const char    *meta_pri_hint     = SOS_ENUM_STR(pub->meta.pri_hint, SOS_PRI);
    const char    *meta_scope_hint   = SOS_ENUM_STR(pub->meta.scope_hint, SOS_SCOPE);
    const char    *meta_retain_hint  = SOS_ENUM_STR(pub->meta.retain_hint, SOS_RETAIN);
    unsigned char *pragma            = pub->pragma_msg;
    int            pragma_len        = pub->pragma_len;
    unsigned char  pragma_empty[2];    memset(pragma_empty, '\0', 2);

    dlog(5, "  ... binding values into the statement\n");
    dlog(6, "     ... pragma_len = %d\n", pragma_len);
    dlog(6, "     ... pragma     = \"%s\"\n", pragma);

    CALL_SQLITE (bind_int    (stmt_insert_pub, 1,  guid         ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 2,  title,            1 + strlen(title), SQLITE_STATIC  ));
    CALL_SQLITE (bind_int    (stmt_insert_pub, 3,  process_id   ));
    CALL_SQLITE (bind_int    (stmt_insert_pub, 4,  thread_id    ));
    CALL_SQLITE (bind_int    (stmt_insert_pub, 5,  comm_rank    ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 6,  node_id,          1 + strlen(node_id), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 7,  prog_name,        1 + strlen(prog_name), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 8,  prog_ver,         1 + strlen(prog_ver), SQLITE_STATIC  ));
    CALL_SQLITE (bind_int    (stmt_insert_pub, 9,  meta_channel ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 10, meta_nature,      1 + strlen(meta_nature), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 11, meta_layer,       1 + strlen(meta_layer), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 12, meta_pri_hint,    1 + strlen(meta_pri_hint), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 13, meta_scope_hint,  1 + strlen(meta_scope_hint), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 14, meta_retain_hint, 1 + strlen(meta_retain_hint), SQLITE_STATIC  ));
    if (pragma_len > 0) {
        /* Only bind the pragma if there actually is something to insert... */
        CALL_SQLITE (bind_text   (stmt_insert_pub, 15, (char const *) pragma,           pub->pragma_len, SQLITE_STATIC  ));
    } else {
        CALL_SQLITE (bind_text   (stmt_insert_pub, 15, (char const *) pragma_empty,    2, SQLITE_STATIC  ));
    }

    dlog(5, "  ... executing the query\n");

    CALL_SQLITE_EXPECT (step (stmt_insert_pub), DONE);  /* Execute the query. */

    dlog(5, "  ... success!  resetting the statement.\n");

    CALL_SQLITE (reset (stmt_insert_pub));
    CALL_SQLITE (clear_bindings (stmt_insert_pub));

    dlog(5, "  ... done.  returning to loop.\n");
    return;
}

void SOSD_db_insert_vals( SOS_pub *pub, SOS_val_snap_queue *queue, SOS_val_snap_queue *re_queue ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_insert_vals");
    char pub_guid_str[SOS_DEFAULT_STRING_LEN];
    SOS_val_snap *snap;
    SOS_val_snap *next_snap;

    dlog(2, "Attempting to inject val_snap queue for pub->title = \"%s\":\n", pub->title);
    memset(pub_guid_str, '\0', SOS_DEFAULT_STRING_LEN);
    sprintf(pub_guid_str, "%ld", pub->guid);

    dlog(2, "  ... getting locks for queues\n");
    if (re_queue != NULL) {
        dlog(2, "     ... re_queue->lock\n");
        pthread_mutex_lock( re_queue->lock );
    }
    dlog(2, "     ... queue->lock\n");
    pthread_mutex_lock( queue->lock );

    dlog(2, "  ... grabbing LIFO snap_queue head\n");
    /* Grab the linked-list (LIFO queue) */
    snap = (SOS_val_snap *) queue->from->get(queue->from, pub_guid_str);

    dlog(2, "  ... clearing the snap queue\n");
    /* Clear the queue and unlock it so new additions can inject. */
    queue->from->remove(queue->from, pub_guid_str);

    dlog(2, "  ... releasing queue->lock\n");
    pthread_mutex_unlock( queue->lock );

    dlog(2, "  ... processing snaps extracted from the queue\n");


    int           elem;
    char         *val, *val_alloc;
    long          guid;
    double        time_pack;
    double        time_send;
    double        time_recv;
    long          frame;
    const char   *semantic;
    const char   *mood;
    
    val_alloc = (char *) malloc(SOS_DEFAULT_STRING_LEN);

    while (snap != NULL) {

        elem              = snap->elem;
        guid              = snap->guid;
        time_pack         = snap->time.pack;
        time_send         = snap->time.send;
        time_recv         = snap->time.recv;
        frame             = snap->frame;
        semantic          = SOS_ENUM_STR( snap->semantic, SOS_VAL_SEMANTIC );
        mood              = SOS_ENUM_STR( snap->mood, SOS_MOOD );

        SOS_TIME( time_recv );

        if (pub->data[snap->elem]->type != SOS_VAL_TYPE_STRING) {
            val = val_alloc;
            memset(val, '\0', SOS_DEFAULT_STRING_LEN);
        }

        switch (pub->data[snap->elem]->type) {
        case SOS_VAL_TYPE_INT:    snprintf(val, SOS_DEFAULT_STRING_LEN, "%d",  snap->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   snprintf(val, SOS_DEFAULT_STRING_LEN, "%ld", snap->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: snprintf(val, SOS_DEFAULT_STRING_LEN, "%lf", snap->val.d_val); break;
        case SOS_VAL_TYPE_STRING: val = snap->val.c_val; dlog(0, "Injecting: %s\n", snap->val.c_val); break;
        default:
            dlog(5, "     ... error: invalid value type.  (%d)\n", pub->data[snap->elem]->type); break;
        }

        dlog(5, "     ... binding values\n");

        CALL_SQLITE (bind_int    (stmt_insert_val, 1,  guid         ));
        CALL_SQLITE (bind_text   (stmt_insert_val, 2,  val,              1 + strlen(val),            SQLITE_STATIC ));
        CALL_SQLITE (bind_int    (stmt_insert_val, 3,  frame        ));
        CALL_SQLITE (bind_text   (stmt_insert_val, 4,  semantic,    1 + strlen(semantic),  SQLITE_STATIC ));
        CALL_SQLITE (bind_text   (stmt_insert_val, 5,  mood,        1 + strlen(mood),      SQLITE_STATIC ));
        CALL_SQLITE (bind_double (stmt_insert_val, 6,  time_pack    ));
        CALL_SQLITE (bind_double (stmt_insert_val, 7,  time_send    ));
        CALL_SQLITE (bind_double (stmt_insert_val, 8,  time_recv    ));

        dlog(5, "     ... executing the query\n");

        CALL_SQLITE_EXPECT (step (stmt_insert_val), DONE);  /* Execute the query. */
        
        dlog(5, "     ... success!  resetting the statement.\n");
        
        CALL_SQLITE (reset (stmt_insert_val));
        CALL_SQLITE (clear_bindings (stmt_insert_val));

        dlog(5, "     ... grabbing the next snap\n");

        next_snap = (SOS_val_snap *) snap->next;
        if (re_queue != NULL) {
            snap->next = (void *) re_queue->from->get(re_queue->from, pub_guid_str);
            re_queue->from->remove(re_queue->from, pub_guid_str);
            dlog(5, "     ... re_queue this val_snap in the val_outlet   (%ld).next->(%ld)\n", (long) snap, (long) snap->next);
            re_queue->from->put(re_queue->from, pub_guid_str, (void *) snap);
        } else {
            /* You're not re-queueing them...
             *   ... you'll need to free() them yourself. */
            /* Let's go ahead and do it, there are no easily
             * identifiable cases where they need to get used
             * again when there is no re-queueing. (node_sync's
             * ring monitor doesn't call this function, so we don't
             * need to keep them around for transmission.) */
            dlog(5, "     ... freeing this val_snap b/c there is no re_queue\n");
            if (pub->data[snap->elem]->type == SOS_VAL_TYPE_STRING) { free(snap->val.c_val); }
            free(snap);
        }

        snap = next_snap;
    }

    free(val_alloc);

    dlog(2, "     ... done.\n");
    dlog(2, "  ... releasing re_queue->lock\n");

    if (re_queue != NULL) { pthread_mutex_unlock( re_queue->lock ); }
    
    dlog(5, "  ... done.  returning to loop.\n");

    return;
}


void SOSD_db_insert_data( SOS_pub *pub ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_insert_data");
    int i;

    dlog(5, "Inserting pub(%ld)->data into database(%s).\n", pub->guid, SOSD.db.file);

    for (i = 0; i < pub->elem_count; i++) {
        /*
         *  NOTE: SQLite3 behaves strangely unless you pass it variables stored on the stack.
         */
        long          pub_guid          = pub->guid;
        long          guid              = pub->data[i]->guid;
        const char   *name              = pub->data[i]->name;
        char         *val;
        const char   *val_type          = SOS_ENUM_STR( pub->data[i]->type, SOS_VAL_TYPE );
        const char   *meta_freq         = SOS_ENUM_STR( pub->data[i]->meta.freq, SOS_VAL_FREQ );
        const char   *meta_class        = SOS_ENUM_STR( pub->data[i]->meta.classifier, SOS_VAL_CLASS );
        const char   *meta_pattern      = SOS_ENUM_STR( pub->data[i]->meta.pattern, SOS_VAL_PATTERN );
        const char   *meta_compare      = SOS_ENUM_STR( pub->data[i]->meta.compare, SOS_VAL_COMPARE );

        char          val_num_as_str[SOS_DEFAULT_STRING_LEN];
        memset( val_num_as_str, '\0', SOS_DEFAULT_STRING_LEN);

        switch (pub->data[i]->type) {
        case SOS_VAL_TYPE_INT:    val = val_num_as_str; snprintf(val, SOS_DEFAULT_STRING_LEN, "%d",  pub->data[i]->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   val = val_num_as_str; snprintf(val, SOS_DEFAULT_STRING_LEN, "%ld", pub->data[i]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: val = val_num_as_str; snprintf(val, SOS_DEFAULT_STRING_LEN, "%lf", pub->data[i]->val.d_val); break;
        case SOS_VAL_TYPE_STRING: val = pub->data[i]->val.c_val; break;
        default:
            dlog(5, "ERROR: Attempting to insert an invalid data type.  (%d)  Continuing...\n", pub->data[i]->type);
            break;
            continue;
        }

        CALL_SQLITE (bind_int    (stmt_insert_data, 1,  pub_guid     ));
        CALL_SQLITE (bind_int    (stmt_insert_data, 2,  guid         ));
        CALL_SQLITE (bind_text   (stmt_insert_data, 3,  name,             1 + strlen(name), SQLITE_STATIC     ));
        CALL_SQLITE (bind_text   (stmt_insert_data, 4,  val_type,         1 + strlen(val_type), SQLITE_STATIC     ));
        CALL_SQLITE (bind_text   (stmt_insert_data, 5,  meta_freq,        1 + strlen(meta_freq), SQLITE_STATIC     ));
        CALL_SQLITE (bind_text   (stmt_insert_data, 6,  meta_class,       1 + strlen(meta_class), SQLITE_STATIC     ));
        CALL_SQLITE (bind_text   (stmt_insert_data, 7,  meta_pattern,     1 + strlen(meta_pattern), SQLITE_STATIC     ));
        CALL_SQLITE (bind_text   (stmt_insert_data, 8,  meta_compare,     1 + strlen(meta_compare), SQLITE_STATIC     ));

        dlog(5, "  ... executing insert query   pub->data[%d].(%s)\n", i, pub->data[i]->name);

        CALL_SQLITE_EXPECT (step (stmt_insert_data), DONE);

        dlog(6, "  ... success!  resetting the statement.\n");
        CALL_SQLITE (reset(stmt_insert_data));
        CALL_SQLITE (clear_bindings (stmt_insert_data));

    }
    
    dlog(5, "  ... done.  returning to loop.\n");

    return;
}




void SOSD_db_insert_enum(const char *var_type, const char **var_name, int var_max_index) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_insert_enum");

    int i;
    const char *type = var_type;
    const char *name;

    for (i = 0; i < var_max_index; i++) {
        int pos = i;
        name = var_name[i];

        CALL_SQLITE (bind_text   (stmt_insert_enum, 1,  type,         1 + strlen(type), SQLITE_STATIC     ));
        CALL_SQLITE (bind_text   (stmt_insert_enum, 2,  name,         1 + strlen(name), SQLITE_STATIC     ));
        CALL_SQLITE (bind_int    (stmt_insert_enum, 3,  pos ));

        dlog(2, "  --> SOS_%s_string[%d] == \"%s\"\n", type, i, name);

        CALL_SQLITE_EXPECT (step (stmt_insert_enum), DONE);

        CALL_SQLITE (reset(stmt_insert_enum));
        CALL_SQLITE (clear_bindings (stmt_insert_enum));

    }

    return;
}



void SOSD_db_create_tables(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_create_tables");
    int rc;
    char *err = NULL;

    dlog(1, "Creating tables in the database...\n");

    rc = sqlite3_exec(database, sql_create_table_pubs, NULL, NULL, &err);
    if( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_PUBS_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_PUBS_TABLE_NAME);
    }

    rc = sqlite3_exec(database, sql_create_table_data, NULL, NULL, &err);
    if( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_DATA_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_DATA_TABLE_NAME);
    }

    rc = sqlite3_exec(database, sql_create_table_vals, NULL, NULL, &err);
    if( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_VALS_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_VALS_TABLE_NAME);
    }

    rc = sqlite3_exec(database, sql_create_table_enum, NULL, NULL, &err);
    if ( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_ENUM_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_ENUM_TABLE_NAME);
    }

    rc = sqlite3_exec(database, sql_create_table_sosd_config, NULL, NULL, &err);
    if ( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_SOSD_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_SOSD_TABLE_NAME);
    }

    dlog(1, "  ... done.\n");
    return;
}

