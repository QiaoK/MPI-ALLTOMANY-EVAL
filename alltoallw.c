/*
 * Copyright (C) 2018, Northwestern University
 * See COPYRIGHT notice in top-level directory.
 *
 * This program evaluates the performance of an all-to-many communication
 * pattern, implemented using MPI_Alltoallw.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* getopt() */

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
    int *s_size, *sdispls, *r_size, *rdispls;
    void *s_buf, *r_buf;
    double t_all, t_max;
    MPI_Datatype *dtypes;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    ntimes = 100;
    nprocs_node = 1;
    blocklen = 10;
    span = 8;

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

    /* calculate message size per send */
    s_len = ((rank % nprocs_node) % span + 1) * blocklen;
    s_buf = malloc(s_len * nrecvs);

    /* calculate message size for all recv */
    r_len = 0;
    if (rank % nprocs_node == 0)
        for (i=0; i<nprocs; i++)
            r_len += ((i % nprocs_node) % span + 1) * blocklen;
    r_buf = malloc(r_len);

    /* allocate and set the array of MPI datatypes */
    dtypes = (MPI_Datatype*) malloc(nprocs * sizeof(MPI_Datatype));
    for (i=0; i<nprocs; i++) dtypes[i] = MPI_BYTE;

    /* allocate arrays of send/recv sizes and displacements */
    s_size  = (int*) calloc(nprocs * 4, sizeof(int));
    sdispls = s_size  + nprocs;
    r_size  = sdispls + nprocs;
    rdispls = r_size  + nprocs;

    MPI_Barrier(MPI_COMM_WORLD); /*-------------------------------------------*/

    t_all = MPI_Wtime();
    for (k=0; k<ntimes; k++) {

        /* senders (all processes in MPI_COMM_WORLD) */
        s_size[0]  = s_len;
        sdispls[0] = 0;
        for (i=1; i<nrecvs; i++) {
            int r_rank = i * nprocs_node;
            s_size[r_rank]  = s_len;
            sdispls[r_rank] = s_len * i;
        }

        /* receivers (a subset of MPI_COMM_WORLD) */
        if (rank % nprocs_node == 0) {
            r_size[0]  = blocklen;
            rdispls[0] = 0;
            for (i=1; i<nprocs; i++) {
                r_size[i]  = ((i % nprocs_node) % span + 1) * blocklen;
                rdispls[i] = rdispls[i-1] + r_size[i-1];
            }
        }

        err = MPI_Alltoallw(s_buf, s_size, sdispls, dtypes,
                            r_buf, r_size, rdispls, dtypes, MPI_COMM_WORLD);
    }
    t_all = MPI_Wtime() - t_all;

    free(s_buf);
    free(r_buf);
    free(s_size);
    free(dtypes);

    MPI_Reduce(&t_all, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_all = t_max;
    MPI_Reduce(&s_len, &s_max, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&s_len, &s_min, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("-----------------------------------------------------------\n");
        printf("All-to-many communication implemented using MPI_Alltoallw\n");
        printf("-----------------------------------------------------------\n");
        printf("Total number of MPI processes        = %d\n",nprocs);
        printf("Total number of sender   processes   = %d\n",nprocs);
        printf("Total number of receiver processes   = %d\n",nrecvs);
        printf("Total number of iterations           = %d\n",ntimes);
        printf("Message block unit size              = %d bytes\n",blocklen);
        printf("Max message size                     = %d bytes\n",s_max);
        printf("Min message size                     = %d bytes\n",s_min);
        printf("Max end-to-end time                  = %.4f sec\n",t_all);
        printf("(Max means the maximum among all %d processes)\n",nprocs);
        printf("-----------------------------------------------------------\n");
    }

fn_exit:
    MPI_Finalize();
    return (err > 0);
}

