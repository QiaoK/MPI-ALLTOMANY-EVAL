/*
 * Copyright (C) 2018, Northwestern University
 * See COPYRIGHT notice in top-level directory.
 *
 * This program evaluates the performance of an all-to-many communication
 * pattern implemented using MPI asynchronous functions MPI_Isend, MPI_Irecv,
 * and MPI_Waitall.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* getopt() unlink() */

#include <mpi.h>

/*----< err_handler() >------------------------------------------------------*/
void err_handler(int err, int lineno, char *err_msg)
{
    int rank, errorStringLen;
    char errorString[MPI_MAX_ERROR_STRING];

    if (err == MPI_SUCCESS) return;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Error_string(err, errorString, &errorStringLen);
    if (err_msg == NULL) err_msg = "";
    printf("rank %d: line %d MPI error (%s) : %s\n", rank, lineno, err_msg, errorString);
}

/*----< usage() >------------------------------------------------------------*/
static void
usage(char *argv0)
{
    char *help =
    "Usage: %s [OPTION]... [FILE]...\n"
    "       [-h] Print help\n"
    "       [-p] number of MPI processes per node\n"
    "       [-b] message block unit size\n"
    "       [-n] number of iterations\n";
    fprintf(stderr, help, argv0);
}

/*----< main() >-------------------------------------------------------------*/
int main(int argc, char** argv)
{
    extern int optind;
    int i, j, k, err, rank, nprocs, nprocs_node, nrecvs, span, blocklen;
    int verbose=0, ntimes, s_len, r_len, s_max, s_min;
    void *s_buf, *r_buf;
    double t_post_irecv, t_post_isend, t_cur, t_wait, t_all, t_max;
    MPI_Request *req;
    MPI_Status *sts;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    ntimes = 100;
    nprocs_node = 1;
    blocklen = 10;
    span = 8;
    t_post_irecv = t_post_isend = t_wait = 0.0;

    /* command-line arguments */
    while ((i = getopt(argc, argv, "hp:b:n:")) != EOF)
        switch(i) {
            case 'p': nprocs_node = atoi(optarg);
                      break;
            case 'b': blocklen = atoi(optarg);
                      break;
            case 'n': ntimes = atoi(optarg);
                      break;
            case 'h':
            default:  if (rank==0) usage(argv[0]);
                      MPI_Finalize();
                      return 1;
        }

    if (nprocs_node < 0 || nprocs_node > nprocs) {
        if (!rank) printf("Error: illegal nprocs_node=%d\n",nprocs_node);
        err = 1;
        goto fn_exit;
    }

    if (nprocs % nprocs_node > 0) {
        if (!rank)
            printf("Error: nprocs=%d should be divisible by nprocs_node=%d \n",nprocs, nprocs_node);
        err = 1;
        goto fn_exit;
    }

    if (blocklen < 0) {
        if (!rank) printf("Error: illegal blocklen=%d\n",blocklen);
        err = 1;
        goto fn_exit;
    }

    /* number of receiver processes */
    nrecvs = nprocs / nprocs_node;

    /* calculate message size per isend */
    s_len = ((rank % nprocs_node) % span + 1) * blocklen;
    s_buf = malloc(s_len * nrecvs);

    /* calculate message size for all irecv */
    r_len = 0;
    if (rank % nprocs_node == 0)
        for (i=0; i<nprocs; i++)
            r_len += ((i % nprocs_node) % span + 1) * blocklen;
    r_buf = malloc(r_len);

    req = (MPI_Request*) malloc(nprocs * 2 * sizeof(MPI_Request));
    sts = (MPI_Status*) malloc(nprocs * 2 * sizeof(MPI_Status));

    MPI_Barrier(MPI_COMM_WORLD); /*-------------------------------------------*/

    t_all = MPI_Wtime();
    for (k=0; k<ntimes; k++) {
        j = 0;

        /* receivers (a subset of MPI_COMM_WORLD) */
        t_cur = MPI_Wtime();
        if (rank % nprocs_node == 0) {
            char *ptr = (char*)r_buf;
            for (i=0; i<nprocs; i++) {
                r_len = ((i % nprocs_node) % span + 1) * blocklen;
                if (verbose) printf("%2d recv from %2d: r_len=%d\n",rank,i,r_len);
                err = MPI_Irecv(ptr, r_len, MPI_BYTE, i, 0, MPI_COMM_WORLD, &req[j++]);
                err_handler(err, __LINE__, "MPI_Irecv");
                ptr += r_len;
            }
        }
        t_post_irecv += MPI_Wtime() - t_cur;

        /* senders (all processes in MPI_COMM_WORLD) */
        t_cur = MPI_Wtime();
        for (i=0; i<nrecvs; i++) {
            char *ptr = (char*)s_buf;
            int r_rank = i * nprocs_node;
            if (verbose) printf("%2d send to %2d: s_len=%d\n",rank,r_rank,s_len);
            err = MPI_Isend(ptr, s_len, MPI_BYTE, r_rank, 0, MPI_COMM_WORLD, &req[j++]);
            err_handler(err, __LINE__, "MPI_Isend");
            ptr += s_len;
        }
        t_post_isend += MPI_Wtime() - t_cur;

        /* wait for all irecv/isend to complete */
        t_cur = MPI_Wtime();
        err = MPI_Waitall(j, req, sts);
        err_handler(err, __LINE__, "MPI_Waitall");
        t_wait += MPI_Wtime() - t_cur;
    }
    t_all = MPI_Wtime() - t_all;

    free(s_buf);
    free(r_buf);
    free(req);
    free(sts);

    MPI_Reduce(&t_post_irecv, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_post_irecv = t_max;
    MPI_Reduce(&t_post_isend, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_post_isend = t_max;
    MPI_Reduce(&t_wait, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_wait = t_max;
    MPI_Reduce(&t_all, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_all = t_max;
    MPI_Reduce(&s_len, &s_max, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&s_len, &s_min, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("-----------------------------------------------------------\n");
        printf("All-to-many communication implemented using MPI isend/irecv\n");
        printf("-----------------------------------------------------------\n");
        printf("Total number of MPI processes        = %d\n",nprocs);
        printf("Total number of sender   processes   = %d\n",nprocs);
        printf("Total number of receiver processes   = %d\n",nrecvs);
        printf("Total number of iterations           = %d\n",ntimes);
        printf("Message block unit size              = %d bytes\n",blocklen);
        printf("Max message size                     = %d bytes\n",s_max);
        printf("Min message size                     = %d bytes\n",s_min);
        printf("Max time of posting irecv            = %.4f sec\n",t_post_irecv);
        printf("Max time of posting isend            = %.4f sec\n",t_post_isend);
        printf("Max time of MPI_Waitall()            = %.4f sec\n",t_wait);
        printf("Max end-to-end time                  = %.4f sec\n",t_all);
        printf("(Max means the maximum among all %d processes)\n",nprocs);
        printf("-----------------------------------------------------------\n");
    }

fn_exit:
    MPI_Finalize();
    return (err > 0);
}

