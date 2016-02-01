#include "worker.h"
#include "stdio.h"
#include "stdlib.h"
#include "adios_read.h"
#include <stdbool.h>

/* 
 * Worker B is the second application in the workflow.
 * It waits for input from worker A, does some
 * "computation" and communication,
 * and it produces "output" that is "input" to worker_c.
 */

int iterations = 1;
bool send_to_c = true;

void validate_input(int argc, char* argv[]) {
    if (argc < 2) {
        my_printf("Usage: %s <num iterations> <send to next worker>\n", argv[0]);
        exit(1);
    }
    if (commsize < 2) {
        my_printf("%s requires at least 2 processes.\n", argv[0]);
        exit(1);
    }
    iterations = atoi(argv[1]);
    if (argc > 2) {
        int tmp = atoi(argv[2]);
        if (tmp == 0) {
            send_to_c = false;
        }
    }
}

int worker(int argc, char* argv[]) {
    TAU_PROFILE_TIMER(timer, __func__, __FILE__, TAU_USER);
    TAU_PROFILE_START(timer);
    my_printf("%d of %d In worker B\n", myrank, commsize);

    /* validate input */
    validate_input(argc, argv);

    my_printf("Worker B will execute until it sees n iterations.\n", iterations);

    /* ADIOS: These declarations are required to match the generated
     *        gread_/gwrite_ functions.  (And those functions are
     *        generated by calling 'gpp.py adios_config.xml') ...
     *        EXCEPT THAT THE generation of Reader code is broken.
     *        So, we will write the reader code manually.
     */
    uint64_t  adios_groupsize;
    uint64_t  adios_totalsize;
    uint64_t  adios_handle;
    void * data = NULL;
    uint64_t start[2], count[2];
    int i, j, steps = 0;
    int NX = 10;
    int NY = 1;
    double t[NX];
    double p[NX];

    /* ADIOS: Can duplicate, split the world, whatever.
     *        This allows you to have P writers to N files.
     *        With no splits, everyone shares 1 file, but
     *        can write lock-free by using different areas.
     */
    MPI_Comm  adios_comm, adios_comm_b_to_c;
    MPI_Comm_dup(MPI_COMM_WORLD, &adios_comm);
    MPI_Comm_dup(MPI_COMM_WORLD, &adios_comm_b_to_c);

    enum ADIOS_READ_METHOD method = ADIOS_READ_METHOD_FLEXPATH;
    adios_read_init_method(method, adios_comm, "verbose=3");
    if (adios_errno != err_no_error) {
        fprintf (stderr, "rank %d: Error %d at init: %s\n", myrank, adios_errno, adios_errmsg());
        exit(4);
    }
    if (send_to_c) {
        adios_init("adios_config.xml", adios_comm);
    }

    /* ADIOS: Set up the adios communications and buffers, open the file.
    */
    ADIOS_FILE *fp; // file handler
    ADIOS_VARINFO *vi; // information about one variable 
    ADIOS_SELECTION * sel;
    char      adios_filename_a_to_b[256];
    char      adios_filename_b_to_c[256];
    enum ADIOS_LOCKMODE lock_mode = ADIOS_LOCKMODE_NONE;
    double timeout_sec = 1.0;
    sprintf(adios_filename_a_to_b, "adios_a_to_b.bp");
    sprintf(adios_filename_b_to_c, "adios_b_to_c.bp");
    my_printf ("rank %d: Worker B opening file: %s\n", myrank, adios_filename_a_to_b);
    fp = adios_read_open(adios_filename_a_to_b, method, adios_comm, lock_mode, timeout_sec);
    if (adios_errno == err_file_not_found) {
        fprintf (stderr, "rank %d: Stream not found after waiting %d seconds: %s\n",
        myrank, timeout_sec, adios_errmsg());
        exit(1);
    } else if (adios_errno == err_end_of_stream) {
        // stream has been gone before we tried to open
        fprintf (stderr, "rank %d: Stream terminated before open. %s\n", myrank, adios_errmsg());
        exit(2);
    } else if (fp == NULL) {
        // some other error happened
        fprintf (stderr, "rank %d: Error %d at opening: %s\n", myrank, adios_errno, adios_errmsg());
        exit(3);
    } else {
        my_printf("Found file %s\n", adios_filename_a_to_b);
        my_printf ("File info:\n");
        my_printf ("  current step:   %d\n", fp->current_step);
        my_printf ("  last step:      %d\n", fp->last_step);
        my_printf ("  # of variables: %d:\n", fp->nvars);

        vi = adios_inq_var(fp, "temperature");
        adios_inq_var_blockinfo(fp, vi);

        printf ("ndim = %d\n",  vi->ndim);
        printf ("nsteps = %d\n",  vi->nsteps);
        printf ("dims[%llu][%llu]\n",  vi->dims[0], vi->dims[1]);

        uint64_t slice_size = vi->dims[0]/commsize;
        if (myrank == commsize-1) {
            slice_size = slice_size + vi->dims[0]%commsize;
        }

        start[0] = myrank * slice_size;
        count[0] = slice_size;
        start[1] = 0;
        count[1] = vi->dims[1];

        data = malloc (slice_size * vi->dims[1] * 8);

        /* Processing loop over the steps (we are already in the first one) */
        while (adios_errno != err_end_of_stream && steps < iterations) {
            steps++; // steps start counting from 1

            TAU_PROFILE_TIMER(adios_recv_timer, "ADIOS recv", __FILE__, TAU_USER);
            TAU_PROFILE_START(adios_recv_timer);
            sel = adios_selection_boundingbox (vi->ndim, start, count);
            adios_schedule_read (fp, sel, "temperature", 0, 1, data);
            adios_perform_reads (fp, 1);

            if (myrank == 0)
                printf ("--------- B Step: %d --------------------------------\n",
                        fp->current_step);

#if 0
            printf("B rank=%d: [0:%lld,0:%lld] = [", myrank, vi->dims[0], vi->dims[1]);
            for (i = 0; i < slice_size; i++) {
                printf (" [");
                for (j = 0; j < vi->dims[1]; j++) {
                    printf ("%g ", *((double *)data + i * vi->dims[1] + j));
                }
                printf ("]");
            }
            printf (" ]\n\n");
#endif

            // advance to 1) next available step with 2) blocking wait
            adios_advance_step (fp, 0, timeout_sec);
            if (adios_errno == err_step_notready)
            {
                printf ("B rank %d: No new step arrived within the timeout. Quit. %s\n",
                        myrank, adios_errmsg());
                break; // quit while loop
            }
            TAU_PROFILE_STOP(adios_recv_timer);

            /* Do some exchanges with neighbors */
            //do_neighbor_exchange();
            /* "Compute" */
            compute(steps);

            for (i = 0; i < NX; i++) {
                t[i] = steps*100.0 + myrank*NX + i;
            }

            for (i = 0; i < NY; i++) {
                p[i] = steps*1000.0 + myrank*NY + i;
            }

            if (send_to_c) {
                TAU_PROFILE_TIMER(adios_send_timer, "ADIOS send", __FILE__, TAU_USER);
                TAU_PROFILE_START(adios_send_timer);
                /* ADIOS: write to the next application in the workflow */
                if (steps == 0) {
                    adios_open(&adios_handle, "b_to_c", adios_filename_b_to_c, "w", adios_comm_b_to_c);
                } else {
                    adios_open(&adios_handle, "b_to_c", adios_filename_b_to_c, "a", adios_comm_b_to_c);
                }
                /* ADIOS: Actually write the data out.
                *        Yes, this is the recommended method, and this way, changes in
                *        configuration with the .XML file will, even in the worst-case
                *        scenario, merely require running 'gpp.py adios_config.xml'
                *        and typing 'make'.
                */
                #include "gwrite_b_to_c.ch"
                /* ADIOS: Close out the file completely and finalize.
                *        If MPI is being used, this must happen before MPI_Finalize().
                */
                adios_close(adios_handle);
                TAU_PROFILE_STOP(adios_send_timer);
            }
            MPI_Barrier(adios_comm_b_to_c);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        adios_read_close(fp);
        /* ADIOS: Close out the file completely and finalize.
        *        If MPI is being used, this must happen before MPI_Finalize().
        */
        adios_read_finalize_method(method);
    }
    if (send_to_c) {
        adios_finalize(myrank);
    }

    free(data);
    MPI_Comm_free(&adios_comm);
    MPI_Comm_free(&adios_comm_b_to_c);

    TAU_PROFILE_STOP(timer);
    /* exit */
    return 0;
}

//int compute(int iteration) { return 0; }
