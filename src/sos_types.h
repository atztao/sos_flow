#ifndef SOS_TYPES_H
#define SOS_TYPES_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"


#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>

#include "sos_qhashtbl.h"
#include "sos_pipe.h"
#include "sos_buffer.h"


#define FOREACH_ROLE(ROLE)                      \
    ROLE(SOS_ROLE_UNASSIGNED)                   \
    ROLE(SOS_ROLE_CLIENT)                       \
    ROLE(SOS_ROLE_LISTENER)                     \
    ROLE(SOS_ROLE_AGGREGATOR)                   \
    ROLE(SOS_ROLE_ANALYTICS)                    \
    ROLE(SOS_ROLE_RUNTIME_UTILITY)              \
    ROLE(SOS_ROLE_OFFLINE_TEST_MODE)            \
    ROLE(SOS_ROLE___MAX)

#define FOREACH_STATUS(STATUS)                  \
    STATUS(SOS_STATUS_INIT)                     \
    STATUS(SOS_STATUS_RUNNING)                  \
    STATUS(SOS_STATUS_HALTING)                  \
    STATUS(SOS_STATUS_SHUTDOWN)                 \
    STATUS(SOS_STATUS___MAX)

#define FOREACH_MSG_TYPE(MSG_TYPE)              \
    MSG_TYPE(SOS_MSG_TYPE_NULLMSG)              \
    MSG_TYPE(SOS_MSG_TYPE_REGISTER)             \
    MSG_TYPE(SOS_MSG_TYPE_UNREGISTER)           \
    MSG_TYPE(SOS_MSG_TYPE_GUID_BLOCK)           \
    MSG_TYPE(SOS_MSG_TYPE_ANNOUNCE)             \
    MSG_TYPE(SOS_MSG_TYPE_PUBLISH)              \
    MSG_TYPE(SOS_MSG_TYPE_VAL_SNAPS)            \
    MSG_TYPE(SOS_MSG_TYPE_ECHO)                 \
    MSG_TYPE(SOS_MSG_TYPE_PROBE)                \
    MSG_TYPE(SOS_MSG_TYPE_SHUTDOWN)             \
    MSG_TYPE(SOS_MSG_TYPE_ACK)                  \
    MSG_TYPE(SOS_MSG_TYPE_CHECK_IN)             \
    MSG_TYPE(SOS_MSG_TYPE_QUERY)                \
    MSG_TYPE(SOS_MSG_TYPE_MATCH_PUBS)           \
    MSG_TYPE(SOS_MSG_TYPE_MATCH_VALS)           \
    MSG_TYPE(SOS_MSG_TYPE_FEEDBACK)             \
    MSG_TYPE(SOS_MSG_TYPE_SENSITIVITY)          \
    MSG_TYPE(SOS_MSG_TYPE_DESENSITIZE)          \
    MSG_TYPE(SOS_MSG_TYPE_TRIGGERPULL)          \
    MSG_TYPE(SOS_MSG_TYPE_KMEAN_DATA)           \
    MSG_TYPE(SOS_MSG_TYPE___MAX)

#define FOREACH_RECEIVES(RECEIVES)              \
    RECEIVES(SOS_RECEIVES_DIRECT_MESSAGES)      \
    RECEIVES(SOS_RECEIVES_TIMED_CHECKIN)        \
    RECEIVES(SOS_RECEIVES_MANUAL_CHECKIN)       \
    RECEIVES(SOS_RECEIVES_NO_FEEDBACK)          \
    RECEIVES(SOS_RECEIVES_DAEMON_MODE)          \
    RECEIVES(SOS_RECEIVES___MAX)

#define FOREACH_FEEDBACK_TYPE(FEEDBACK_TYPE)    \
    FEEDBACK_TYPE(SOS_FEEDBACK_TYPE_PAYLOAD)    \
    FEEDBACK_TYPE(SOS_FEEDBACK_TYPE_QUERY)      \
    FEEDBACK_TYPE(SOS_FEEDBACK_TYPE_MATCH_PUBS) \
    FEEDBACK_TYPE(SOS_FEEDBACK_TYPE_MATCH_VALS) \
    FEEDBACK_TYPE(SOS_FEEDBACK_TYPE___MAX)

#define FOREACH_QUERY_STATE(QUERY_STATE)        \
    QUERY_STATE(SOS_QUERY_STATE_INCOMING)       \
    QUERY_STATE(SOS_QUERY_STATE_PENDING)        \
    QUERY_STATE(SOS_QUERY_STATE_PROCESSED)      \
    QUERY_STATE(SOS_QUERY_STATE_REPLIED)        \
    QUERY_STATE(SOS_QUERY_STATE___MAX)

#define FOREACH_PRI(PRI)                        \
    PRI(SOS_PRI_DEFAULT)                        \
    PRI(SOS_PRI_LOW)                            \
    PRI(SOS_PRI_IMMEDIATE)                      \
    PRI(SOS_PRI___MAX)

#define FOREACH_GEOMETRY(VOLUME)                \
    VOLUME(SOS_GEOMETRY_POINT)                  \
    VOLUME(SOS_GEOMETRY_HEXAHEDRON)             \
    VOLUME(SOS_GEOMETRY___MAX)

#define FOREACH_VAL_TYPE(VAL_TYPE)              \
    VAL_TYPE(SOS_VAL_TYPE_INT)                  \
    VAL_TYPE(SOS_VAL_TYPE_LONG)                 \
    VAL_TYPE(SOS_VAL_TYPE_DOUBLE)               \
    VAL_TYPE(SOS_VAL_TYPE_STRING)               \
    VAL_TYPE(SOS_VAL_TYPE_BYTES)                \
    VAL_TYPE(SOS_VAL_TYPE___MAX)

#define FOREACH_VAL_SYNC(VAL_SYNC)              \
    VAL_SYNC(SOS_VAL_SYNC_DEFAULT)              \
    VAL_SYNC(SOS_VAL_SYNC_RENEW)                \
    VAL_SYNC(SOS_VAL_SYNC_LOCAL)                \
    VAL_SYNC(SOS_VAL_SYNC_CLOUD)                \
    VAL_SYNC(SOS_VAL_SYNC___MAX)

#define FOREACH_VAL_STATE(VAL_STATE)            \
    VAL_STATE(SOS_VAL_STATE_CLEAN)              \
    VAL_STATE(SOS_VAL_STATE_DIRTY)              \
    VAL_STATE(SOS_VAL_STATE_EMPTY)              \
    VAL_STATE(SOS_VAL_STATE___MAX)

#define FOREACH_VAL_CLASS(VAL_CLASS)            \
    VAL_CLASS(SOS_VAL_CLASS_DATA)               \
    VAL_CLASS(SOS_VAL_CLASS_EVENT)              \
    VAL_CLASS(SOS_VAL_CLASS___MAX)

#define FOREACH_VAL_SEMANTIC(VAL_SEM)           \
    VAL_SEM(SOS_VAL_SEMANTIC_DEFAULT)           \
    VAL_SEM(SOS_VAL_SEMANTIC_TIME_START)        \
    VAL_SEM(SOS_VAL_SEMANTIC_TIME_STOP)         \
    VAL_SEM(SOS_VAL_SEMANTIC_TIME_STAMP)        \
    VAL_SEM(SOS_VAL_SEMANTIC_TIME_SPAN)         \
    VAL_SEM(SOS_VAL_SEMANTIC_SAMPLE)            \
    VAL_SEM(SOS_VAL_SEMANTIC_COUNTER)           \
    VAL_SEM(SOS_VAL_SEMANTIC_LOG)               \
    VAL_SEM(SOS_VAL_SEMANTIC___MAX)

#define FOREACH_VAL_FREQ(VAL_FREQ)              \
    VAL_FREQ(SOS_VAL_FREQ_DEFAULT)              \
    VAL_FREQ(SOS_VAL_FREQ_RARE)                 \
    VAL_FREQ(SOS_VAL_FREQ_COMMON)               \
    VAL_FREQ(SOS_VAL_FREQ_CONTINUOUS)           \
    VAL_FREQ(SOS_VAL_FREQ_IRREGULAR)            \
    VAL_FREQ(SOS_VAL_FREQ___MAX)

#define FOREACH_VAL_PATTERN(PATTERN)            \
    PATTERN(SOS_VAL_PATTERN_DEFAULT)            \
    PATTERN(SOS_VAL_PATTERN_STATIC)             \
    PATTERN(SOS_VAL_PATTERN_RISING)             \
    PATTERN(SOS_VAL_PATTERN_PLATEAU)            \
    PATTERN(SOS_VAL_PATTERN_OSCILLATING)        \
    PATTERN(SOS_VAL_PATTERN_ARC)                \
    PATTERN(SOS_VAL_PATTERN___MAX)

#define FOREACH_REGEX(REGEX)                    \
    REGEX(SOS_REGEX_UNUSED)                     \
    REGEX(SOS_REGEX_DOT)                        \
    REGEX(SOS_REGEX_BEGIN)                      \
    REGEX(SOS_REGEX_END)                        \
    REGEX(SOS_REGEX_QUESTIONMARK)               \
    REGEX(SOS_REGEX_STAR)                       \
    REGEX(SOS_REGEX_PLUS)                       \
    REGEX(SOS_REGEX_CHAR)                       \
    REGEX(SOS_REGEX_CHAR_CLASS)                 \
    REGEX(SOS_REGEX_INV_CHAR_CLASS)             \
    REGEX(SOS_REGEX_DIGIT)                      \
    REGEX(SOS_REGEX_NOT_DIGIT)                  \
    REGEX(SOS_REGEX_ALPHA)                      \
    REGEX(SOS_REGEX_NOT_ALPHA)                  \
    REGEX(SOS_REGEX_WHITESPACE)                 \
    REGEX(SOS_REGEX_NOT_WHITESPACE)             \
    REGEX(SOS_REGEX___MAX)

#define FOREACH_VAL_COMPARE(COMPARE)            \
    COMPARE(SOS_VAL_COMPARE_SELF)               \
    COMPARE(SOS_VAL_COMPARE_RELATIONS)          \
    COMPARE(SOS_VAL_COMPARE___MAX)

#define FOREACH_SCOPE(SCOPE)                    \
    SCOPE(SOS_SCOPE_DEFAULT)                    \
    SCOPE(SOS_SCOPE_SELF)                       \
    SCOPE(SOS_SCOPE_NODE)                       \
    SCOPE(SOS_SCOPE_AGGREGATOR)                 \
    SCOPE(SOS_SCOPE_GLOBAL)                     \
    SCOPE(SOS_SCOPE___MAX)

#define FOREACH_LAYER(LAYER)                    \
    LAYER(SOS_LAYER_DEFAULT)                    \
    LAYER(SOS_LAYER_APP)                        \
    LAYER(SOS_LAYER_OS)                         \
    LAYER(SOS_LAYER_LIB)                        \
    LAYER(SOS_LAYER_ENVIRONMENT)                \
    LAYER(SOS_LAYER_SOS_RUNTIME)                \
    LAYER(SOS_LAYER___MAX)

#define FOREACH_NATURE(NATURE)                  \
    NATURE(SOS_NATURE_DEFAULT)                  \
    NATURE(SOS_NATURE_CREATE_INPUT)             \
    NATURE(SOS_NATURE_CREATE_OUTPUT)            \
    NATURE(SOS_NATURE_CREATE_VIZ)               \
    NATURE(SOS_NATURE_EXEC_WORK)                \
    NATURE(SOS_NATURE_BUFFER)                   \
    NATURE(SOS_NATURE_SUPPORT_EXEC)             \
    NATURE(SOS_NATURE_SUPPORT_FLOW)             \
    NATURE(SOS_NATURE_CONTROL_FLOW)             \
    NATURE(SOS_NATURE_KMEAN_2D)                 \
    NATURE(SOS_NATURE_SOS)                      \
    NATURE(SOS_NATURE___MAX)

#define FOREACH_RETAIN(RETAIN)                  \
    RETAIN(SOS_RETAIN_DEFAULT)                  \
    RETAIN(SOS_RETAIN_SESSION)                  \
    RETAIN(SOS_RETAIN_IMMEDIATE)                \
    RETAIN(SOS_RETAIN___MAX)

#define FOREACH_LOCALE(LOCALE)                  \
    LOCALE(SOS_LOCALE_DEFAULT)                  \
    LOCALE(SOS_LOCALE_INDEPENDENT)              \
    LOCALE(SOS_LOCALE_DAEMON_DBMS)              \
    LOCALE(SOS_LOCALE_APPLICATION)              \
    LOCALE(SOS_LOCALE___MAX)


#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum { FOREACH_ROLE(GENERATE_ENUM)          } SOS_role;
typedef enum { FOREACH_STATUS(GENERATE_ENUM)        } SOS_status;
typedef enum { FOREACH_MSG_TYPE(GENERATE_ENUM)      } SOS_msg_type;
typedef enum { FOREACH_RECEIVES(GENERATE_ENUM)      } SOS_receives;
typedef enum { FOREACH_FEEDBACK_TYPE(GENERATE_ENUM) } SOS_feedback_type;
typedef enum { FOREACH_QUERY_STATE(GENERATE_ENUM)   } SOS_query_state;
typedef enum { FOREACH_PRI(GENERATE_ENUM)           } SOS_pri;
typedef enum { FOREACH_GEOMETRY(GENERATE_ENUM)      } SOS_geometry;
typedef enum { FOREACH_VAL_TYPE(GENERATE_ENUM)      } SOS_val_type;
typedef enum { FOREACH_VAL_STATE(GENERATE_ENUM)     } SOS_val_state;
typedef enum { FOREACH_VAL_SYNC(GENERATE_ENUM)      } SOS_val_sync;
typedef enum { FOREACH_VAL_SEMANTIC(GENERATE_ENUM)  } SOS_val_semantic;
typedef enum { FOREACH_VAL_FREQ(GENERATE_ENUM)      } SOS_val_freq;
typedef enum { FOREACH_VAL_PATTERN(GENERATE_ENUM)   } SOS_val_pattern;
typedef enum { FOREACH_VAL_COMPARE(GENERATE_ENUM)   } SOS_val_compare;
typedef enum { FOREACH_VAL_CLASS(GENERATE_ENUM)     } SOS_val_class;
typedef enum { FOREACH_REGEX(GENERATE_ENUM)         } SOS_regex_rule;
typedef enum { FOREACH_SCOPE(GENERATE_ENUM)         } SOS_scope;
typedef enum { FOREACH_LAYER(GENERATE_ENUM)         } SOS_layer;
typedef enum { FOREACH_NATURE(GENERATE_ENUM)        } SOS_nature;
typedef enum { FOREACH_RETAIN(GENERATE_ENUM)        } SOS_retain;
typedef enum { FOREACH_LOCALE(GENERATE_ENUM)        } SOS_locale;

static const char *SOS_ROLE_str[] =          { FOREACH_ROLE(GENERATE_STRING)         };
static const char *SOS_STATUS_str[] =        { FOREACH_STATUS(GENERATE_STRING)       };
static const char *SOS_MSG_TYPE_str[] =      { FOREACH_MSG_TYPE(GENERATE_STRING)     };
static const char *SOS_RECEIVES_str[] =      { FOREACH_RECEIVES(GENERATE_STRING)     };
static const char *SOS_FEEDBACK_TYPE_str[] = { FOREACH_FEEDBACK_TYPE(GENERATE_STRING)};
static const char *SOS_QUERY_STATE_str[] =   { FOREACH_QUERY_STATE(GENERATE_STRING)  };
static const char *SOS_PRI_str[] =           { FOREACH_PRI(GENERATE_STRING)          };
static const char *SOS_GEOMETRY_str[] =      { FOREACH_GEOMETRY(GENERATE_STRING)     };
static const char *SOS_VAL_TYPE_str[] =      { FOREACH_VAL_TYPE(GENERATE_STRING)     };
static const char *SOS_VAL_STATE_str[] =     { FOREACH_VAL_STATE(GENERATE_STRING)    };
static const char *SOS_VAL_SYNC_str[] =      { FOREACH_VAL_SYNC(GENERATE_STRING)     };
static const char *SOS_VAL_FREQ_str[] =      { FOREACH_VAL_FREQ(GENERATE_STRING)     };
static const char *SOS_VAL_SEMANTIC_str[] =  { FOREACH_VAL_SEMANTIC(GENERATE_STRING) };
static const char *SOS_VAL_PATTERN_str[] =   { FOREACH_VAL_PATTERN(GENERATE_STRING)  };
static const char *SOS_VAL_COMPARE_str[] =   { FOREACH_VAL_COMPARE(GENERATE_STRING)  };
static const char *SOS_VAL_CLASS_str[] =     { FOREACH_VAL_CLASS(GENERATE_STRING)    };
static const char *SOS_REGEX_str[] =         { FOREACH_REGEX(GENERATE_STRING)        };
static const char *SOS_SCOPE_str[] =         { FOREACH_SCOPE(GENERATE_STRING)        };
static const char *SOS_LAYER_str[] =         { FOREACH_LAYER(GENERATE_STRING)        };
static const char *SOS_NATURE_str[] =        { FOREACH_NATURE(GENERATE_STRING)       };
static const char *SOS_RETAIN_str[] =        { FOREACH_RETAIN(GENERATE_STRING)       };
static const char *SOS_LOCALE_str[] =        { FOREACH_LOCALE(GENERATE_STRING)       };

#define SOS_ENUM_IN_RANGE(__SOS_var_name, __SOS_max_name)  (__SOS_var_name >= 0 && __SOS_var_name < __SOS_max_name)
#define SOS_ENUM_STR(__SOS_var_name, __SOS_enum_type)  SOS_ENUM_IN_RANGE(__SOS_var_name, (__SOS_enum_type ## ___MAX)) ? __SOS_enum_type ## _str[__SOS_var_name] : "** " #__SOS_enum_type " is INVALID **"
// Examples:
//      char *layer    = SOS_ENUM_STR( pub->meta.layer,        SOS_LAYER     );
//      char *type     = SOS_ENUM_STR( pub->data[i]->type,     SOS_VAL_TYPE  );
//      char *semantic = SOS_ENUM_STR( pub->data[i]->sem_hint, SOS_SEM       );
//

// Define our magic values for testing object initialization.
//  Ex: 0x5afec0de
//      0x5afeca11
//      0x5afeda7a
//      0x10ad1e55
//      0x5e1ec7ed
#define SOS_VAR_UNDEFINED    0x5e77ab1e
#define SOS_VAR_INITIALIZED  0xca11ab1e
#define SOS_VAR_DESTROYED    0xdeadb10c

// This allows pointers to be used as return values and explicitly tested for
// these semantics, rather than relying on less stable or specific 'NULL's.
char global_placeholder_RETURN_FAIL;
char global_placeholder_RETURN_BUSY;
#define SOS_RET_FAIL      &global_placeholder_RETURN_FAIL
#define SOS_RET_BUSY      &global_placeholder_RETURN_BUSY

typedef     uint64_t SOS_guid;
#define SOS_GUID_FMT      PRIu64




typedef union {
    int                 i_val;
    long                l_val;
    double              d_val;
    char               *c_val;
    void               *bytes;   /* Use addr. of SOS_buffer object. */
} SOS_val;

//
//  SOS_position:
//
//        [Z]
//         | [Y]
//         | /
//         |/
//  . . ... ------[X]
//        ,:
//       . .
//      .  .
//
typedef struct {
    double              x;
    double              y;
    double              z;
} SOS_position;

//
//  SOS_volume_hexahedron:
//
//          [p7]---------[p6]
//         /________    / |
//      [p4]--------[p5]  |
//       ||  |       ||   |
//       || [p3]-----||--[p2]
//       ||/________ || /
//      [p0]--------[p1]
//
typedef struct {
    SOS_position        p[8];
} SOS_volume_hexahedron;


typedef struct {
    double              pack;
    double              send;
    double              recv;
} SOS_time;


typedef struct {
    SOS_val_freq        freq;
    SOS_val_semantic    semantic;
    SOS_val_class       classifier;
    SOS_val_pattern     pattern;
    SOS_val_compare     compare;
    SOS_guid            relation_id;
} SOS_val_meta;

typedef struct {
    int                 elem;
    SOS_guid            guid;
    SOS_guid            pub_guid;
    SOS_guid            relation_id;
    long                frame;
    SOS_time            time;
    SOS_val_semantic    semantic;
    SOS_val_type        type;
    int                 val_len;
    SOS_val             val;
    void               *next_snap;
} SOS_val_snap;

typedef struct {
    SOS_guid            guid;
    int                 val_len;
    SOS_val             val;
    SOS_val_type        type;
    SOS_val_meta        meta;
    SOS_val_state       state;
    SOS_val_sync        sync;
    SOS_time            time;
    char                name[SOS_DEFAULT_STRING_LEN];
    SOS_val_snap       *cached_latest;
                        //cached_latest: Used only within daemons.
} SOS_data;

typedef struct {
    int                 channel;
    SOS_nature          nature;
    SOS_layer           layer;
    SOS_pri             pri_hint;
    SOS_scope           scope_hint;
    SOS_retain          retain_hint;
} SOS_pub_meta;

typedef struct {
    void               *sos_context;
    pthread_mutex_t    *lock;
    int                 sync_pending;
    SOS_guid            guid;
    char                guid_str[SOS_DEFAULT_STRING_LEN];
    int                 process_id;
    int                 thread_id;
    int                 comm_rank;
    SOS_pub_meta        meta;
    int                 announced;
    long                frame;
    int                 elem_max;
    int                 elem_count;
    int                 pragma_len;
    unsigned char       pragma_msg[SOS_DEFAULT_STRING_LEN];
    char                node_id[SOS_DEFAULT_STRING_LEN];
    char                prog_name[SOS_DEFAULT_STRING_LEN];
    char                prog_ver[SOS_DEFAULT_STRING_LEN];
    char                title[SOS_DEFAULT_STRING_LEN];
    int                 cache_depth;
    SOS_data          **data;
    qhashtbl_t         *name_table;
    SOS_pipe           *snap_queue;
} SOS_pub;




typedef struct {
    SOS_guid            guid;
    SOS_guid            client_guid;
    char                handle[SOS_DEFAULT_STRING_LEN];
    void               *target;
    int                 daemon_trigger_count;
    int                 client_receipt_count;
} SOS_sensitivity;


typedef struct {
    SOS_guid            guid;
    SOS_guid            source_guid;
    char                handle[SOS_DEFAULT_STRING_LEN];
    void               *data;
    int                 data_len;
    int                 apply_count; /* -1 == constant */
} SOS_action;


typedef struct {
    char               *sql;
    int                 sql_len;
    SOS_action          action;
} SOS_trigger;

// NOTE: Function signature for feedback handlers
typedef void (*SOS_feedback_handler_f)
    (int   payload_type,
     int   payload_size,
     void *payload_data);

typedef struct {
    void               *sos_context;
    char                local_host[NI_MAXHOST];
    char                local_port[NI_MAXSERV];
    int                 local_socket_fd;
    struct addrinfo    *local_addr;
    struct addrinfo     local_hint;
    struct addrinfo    *result_list;
    struct addrinfo    *remote_addr;
    int                 remote_len;
    char                remote_host[NI_MAXHOST];
    char                remote_port[NI_MAXSERV];
    int                 remote_socket_fd;
    struct addrinfo     remote_hint;
    int                 port_number;
    int                 timeout;
    int                 buffer_len;
    int                 listen_backlog;
    pthread_mutex_t    *send_lock;
    SOS_buffer         *recv_part;
    struct sockaddr_storage   peer_addr;
    socklen_t                 peer_addr_len;
} SOS_socket;


typedef struct {
    int                 msg_size;
    SOS_msg_type        msg_type;
    SOS_guid            msg_from;
    SOS_guid            ref_guid;
#ifdef USE_MUNGE
    char               *ref_cred;
#endif
} SOS_msg_header;

typedef struct {
    char               *node_id;
    int                 comm_rank;
    int                 comm_size;
    int                 comm_support;
    char               *program_name;
    int                 process_id;
    int                 thread_id;
    SOS_layer           layer;
    SOS_locale          locale;
    SOS_receives        receives;
    SOS_feedback_handler_f feedback_handler;
    int                 receives_port;
    int                 receives_ready;
    bool                offline_test_mode;
    bool                runtime_utility;
    int                 pub_cache_depth;
} SOS_config;


typedef struct {
    void               *sos_context;
    int                 listener_port;
    int                 listener_count;
    int                 aggregator_count;
    char               *build_dir;
    char               *install_dir;
    char               *source_dir;
    char               *project_dir;
    char               *work_dir;
    char               *discovery_dir;
    bool                db_disabled;
    int                 db_frame_limit;
    int                 pub_cache_depth;
} SOS_options;

typedef struct {
    void               *sos_context;
    SOS_guid            next;
    SOS_guid            last;
    pthread_mutex_t    *lock;
} SOS_uid;

typedef struct {
    SOS_uid            *local_serial;
    SOS_uid            *my_guid_pool;
} SOS_unique_set;


typedef struct {
    bool                feedback_active;
    pthread_t          *feedback;
    pthread_mutex_t    *feedback_lock;
    pthread_cond_t     *feedback_cond;
    qhashtbl_t         *sense_table;
} SOS_task_set;

typedef struct {
    SOS_config          config;
    SOS_role            role;
    SOS_status          status;
    SOS_unique_set      uid;
    SOS_task_set        task;
    SOS_socket         *daemon;
    SOS_guid            my_guid;
#ifdef USE_MUNGE
    char               *my_cred;
#endif
} SOS_runtime;

#pragma GCC diagnostic pop
#endif
