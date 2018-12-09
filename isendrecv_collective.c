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
#include <string.h>
#include <unistd.h> /* getopt() */

#include <mpi.h>
#include "test_correctness.h"

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
    int i, j, k, w, err, rank, nprocs, nprocs_node, nrecvs, span, blocklen, node_message_size, r_rank;
    int verbose=0, ntimes, s_len, r_len, s_max, s_min;
    void *s_buf, *r_buf, *tmp_buf, *s_buf2;
    char* ptr, *ptr2, *ptr3;
    double t_gather, t_all_to_all, t_cur, t_wait, t_all, t_max;
    MPI_Request *req, *intra_req;
    MPI_Status *sts, *intra_sts;

    MPI_Comm intra_node_comm,comm;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    ntimes = 100;
    nprocs_node = 1;
    blocklen = 10;
    span = 8;
    t_gather = 0;
    t_all_to_all = 0;
    t_wait = 0;
    t_all = 0;

    /* command-line arguments */
    while ((i = getopt(argc, argv, "hp:b:n:")) != EOF){
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
    /* Fill the send buffer with designated values*/
    fill_send_buf(s_buf, nrecvs, ((rank % nprocs_node) % span + 1), blocklen, rank);

    /* calculate message size for all irecv */
    r_len = 0;
    if (rank % nprocs_node == 0)
        for (i=0; i<nprocs; i++)
            r_len += ((i % nprocs_node) % span + 1) * blocklen;
    r_buf = malloc(r_len);

    /* construct Gatherv variables used in every iteration*/
/*
    int *recvcounts = (int*)malloc(sizeof(int)*nprocs_node);
    int *displs = (int*)malloc(sizeof(int)*nprocs_node);
    displs[0]=0;
    for (i=0;i<nprocs_node;i++){
        recvcounts[i] = ((i % nprocs_node) % span + 1) * blocklen * nrecvs;
        if (i<nprocs_node-1){
            displs[i+1]=displs[i]+recvcounts[i];
        }
    }
    node_message_size=displs[nprocs_node-1]+recvcounts[nprocs_node-1];
*/
    node_message_size=0;
    for (i=0;i<nprocs_node;i++){
        node_message_size += ((i % nprocs_node) % span + 1) * blocklen * nrecvs;
    }
    if (rank%nprocs_node==0){
        req = (MPI_Request*) malloc(nrecvs * sizeof(MPI_Request)*2);
        sts = (MPI_Status*) malloc(nrecvs * sizeof(MPI_Status)*2);
        intra_req = (MPI_Request*) malloc((1+nprocs_node) * sizeof(MPI_Request));
        intra_sts = (MPI_Status*) malloc((1+nprocs_node) * sizeof(MPI_Status));
        tmp_buf = malloc(sizeof(char)*node_message_size);
        s_buf2 = malloc(sizeof(char)*node_message_size);
    }else{
        intra_req = (MPI_Request*) malloc(sizeof(MPI_Request));
        intra_sts = (MPI_Status*) malloc(sizeof(MPI_Status));
    }
    /* Construct intra node communicator used by every iteration*/
/*
    t_cur = MPI_Wtime();
    MPI_Comm_split(MPI_COMM_WORLD, rank/nprocs_node, rank, &intra_node_comm);
    t_comm = MPI_Wtime() - t_cur;
*/

    MPI_Barrier(MPI_COMM_WORLD); /*-------------------------------------------*/

    t_all = MPI_Wtime();
    for (k=0; k<ntimes; k++) {
        j = 0;

        t_cur = MPI_Wtime();
        /*Every node gather all messages to be sent to a root process (the process is the only process that actually need to receive from all the rest of processes)*/        
/*
        err = MPI_Gatherv(s_buf, s_len*nrecvs, MPI_BYTE,
                tmp_buf, recvcounts, displs,
                MPI_BYTE, 0, intra_node_comm);
*/
        err = MPI_Issend(s_buf, s_len*nrecvs, MPI_BYTE, (rank/nprocs_node)*nprocs_node, 0, MPI_COMM_WORLD, &intra_req[j++]);
        if (rank%nprocs_node==0){
            ptr = (char*)tmp_buf;
            for (i=0; i<nprocs_node; i++){
                r_len = ((i % nprocs_node) % span + 1) * blocklen * nrecvs;
                r_rank = rank + i;
                err = MPI_Irecv(ptr, r_len, MPI_BYTE, r_rank, 0, MPI_COMM_WORLD, &intra_req[j++]);
                ptr += r_len;
            }
        }
        err = MPI_Waitall(j, intra_req, intra_sts);
        err_handler(err, __LINE__, "MPI_Waitall");
        t_gather += MPI_Wtime()-t_cur;
        t_cur = MPI_Wtime();
        /*Receivers exchange messages (all-to-all) */
        if(rank%nprocs_node==0){
            j=0;
            ptr = (char*)s_buf2;
            for (i=0; i<nrecvs; i++){
                ptr3 = ptr;
                ptr2 = (char*)tmp_buf;
                for (w=0; w<nprocs_node; w++){
                    memcpy(ptr, ptr2+i*((w % nprocs_node) % span + 1) * blocklen, sizeof(char)*((w % nprocs_node) % span + 1) * blocklen);
                    ptr += ((w % nprocs_node) % span + 1) * blocklen;
                    ptr2 += ((w % nprocs_node) % span + 1) * blocklen * nrecvs;
                }
                r_rank = i*nprocs_node;
                err = MPI_Issend(ptr3, node_message_size/nrecvs, MPI_BYTE, r_rank, 0, MPI_COMM_WORLD, &req[j++]);
            }
            ptr = (char*)r_buf;
            for (i=0; i<nrecvs; i++){
                r_len = node_message_size/nrecvs;
                r_rank = i*nprocs_node;
                if (verbose) printf("%2d recv from %2d: r_len=%d\n",rank,0,r_len);
                err = MPI_Irecv(ptr, r_len, MPI_BYTE, r_rank, 0, MPI_COMM_WORLD, &req[j++]);
                ptr += r_len;
            }
            t_all_to_all += MPI_Wtime()-t_cur;
            /* wait for all irecv/isend to complete */
            t_cur = MPI_Wtime();
            err = MPI_Waitall(j, req, sts);
            err_handler(err, __LINE__, "MPI_Waitall");
            t_wait += MPI_Wtime() - t_cur;
        }
    }
    t_all = MPI_Wtime() - t_all;
    #if DEBUG==1
        if (rank%nprocs_node==0){
            if (test_correctness((char*)r_buf, nprocs, blocklen, span, nprocs_node, rank/nprocs_node)==1){
                printf("Wrong result\n");
            }
        }
    #endif

    free(s_buf);
    free(r_buf);
    //free(recvcounts);
    //free(displs);
    free(intra_req);
    free(intra_sts);
    if (rank%nprocs_node==0){
        free(tmp_buf);
        free(s_buf2);
        free(req);
        free(sts);
    }

    //MPI_Comm_free(&intra_node_comm);

    MPI_Reduce(&t_all_to_all, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_all_to_all = t_max;
    MPI_Reduce(&t_gather, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_gather = t_max;
    MPI_Reduce(&t_wait, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_wait = t_max;
    MPI_Reduce(&t_all, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_all = t_max;
/*
    MPI_Reduce(&t_comm, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    t_comm = t_max;
*/
    MPI_Reduce(&s_len, &s_max, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&s_len, &s_min, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("-----------------------------------------------------------\n");
        printf("All-to-many communication implemented using MPI gather + isend/irecv\n");
        printf("-----------------------------------------------------------\n");
        printf("Total number of MPI processes        = %d\n",nprocs);
        printf("Total number of sender   processes   = %d\n",nprocs);
        printf("Total number of receiver processes   = %d\n",nrecvs);
        printf("Total number of iterations           = %d\n",ntimes);
        printf("Message block unit size              = %d bytes\n",blocklen);
        printf("Max message size                     = %d bytes\n",s_max);
        printf("Min message size                     = %d bytes\n",s_min);
        //printf("Max time of Communicator             = %.4f sec\n",t_comm);
        printf("Max time of Gather                   = %.4f sec\n",t_gather);
        printf("Max time of All to All               = %.4f sec\n",t_all_to_all);
        printf("Max time of MPI_Waitall()            = %.4f sec\n",t_wait);
        printf("Max end-to-end time                  = %.4f sec\n",t_all);
        printf("(Max means the maximum among all %d processes)\n",nprocs);
        printf("-----------------------------------------------------------\n");
    }

fn_exit:
    MPI_Finalize();
    return (err > 0);
}

