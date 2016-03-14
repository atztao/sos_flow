#include <stdio.h>
#include <time.h>

#include "sos.h"
#include "test.h"
#include "pub.h"

#define ATTEMPT_MAX   20000


int SOS_test_pub() {
    int error_total = 0;
    int pass_fail = 0;

    SOS_test_section_start(1, "SOS_pub");

    SOS_test_run(2, "pub_create", SOS_test_pub_create(), pass_fail, error_total);
    SOS_test_run(2, "pub_growth", SOS_test_pub_growth(), pass_fail, error_total);

    SOS_test_section_report(1, "SOS_pub", error_total);

    return error_total;
}


int SOS_test_pub_create() {
    SOS_pub *pub;
    char pub_title[60] = {0};

    random_string(pub_title, 60);

    pub = NULL;
    pub = SOS_pub_create(pub_title);

    if (pub == NULL) {
        return FAIL;
    }

    if (pub->elem_max != SOS_DEFAULT_ELEM_MAX) {
        SOS_pub_destroy(pub);
        return FAIL;
    }

    SOS_pub_destroy(pub);
    return PASS;
}




int SOS_test_pub_growth() {
    unsigned char buffer[1024] = {0};
    char some_string[25] = {0};
    char val_name[512] = {0};
    int attempt = 0;

    SOS_pub *pub;
    pub = SOS_pub_create("test_pub_growth");

    struct timespec ts;
    ts.tv_sec  = 0;
    ts.tv_nsec = 62500000;

    for (attempt = 0; attempt < (ATTEMPT_MAX / 2); attempt++) {
        random_string(some_string, 25);
        snprintf(val_name, 512, "%d%s", attempt, some_string);
        SOS_pack(pub, val_name, SOS_VAL_TYPE_INT, (SOS_val) attempt);
        if (SOS_RUN_MODE == SOS_ROLE_CLIENT) {
            SOS_announce(pub);
            SOS_publish(pub);
            printf("  %10d\b\b\b\b\b\b\b\b\b\b\b\b", attempt);
            nanosleep(&ts, NULL);
        }

    }

    if (pub->elem_count != (ATTEMPT_MAX / 2)) {
        SOS_pub_destroy(pub);
        return FAIL;
    }

    if (pub->elem_max < pub->elem_count) {
        SOS_pub_destroy(pub);
        return FAIL;
    }

    SOS_pub_destroy(pub);
    return PASS;
}