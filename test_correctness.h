#define MAP_DATA(a,b,c,d) ((a)*3+(b)*5+(c)*7+(d)*11)
#define DEBUG 1

/*----< verify correctness >-------------------------------------------------*/
void fill_send_buf(char* s_buf, int n_recvs, int send_size, int blocklen, int rank){
    int i, j, k;
    char* ptr=s_buf;
    for (i=0; i<n_recvs; i++){
        for (j=0; j<send_size; j++){
            // just to be as random as possible
            for (k=0;k<blocklen;k++){
                *ptr=(char)MAP_DATA(i,j,k,rank);
                //printf("rank %d, to %d value %d = %d\n",rank, i, j, (int)ptr[0]);
                ptr++;
            }
        }
    }
}

int test_correctness(char* r_buf, int nprocs, int blocklen, int span, int nprocs_node, int receiver_number){
    int i, j, k;
    char* ptr=r_buf;
    for (i=0; i<nprocs;i++){
        int send_size = ((i % nprocs_node) % span + 1);
        for (j=0; j<send_size; j++){
            for (k=0; k<blocklen; k++){
                if (ptr[0]!=(char)MAP_DATA(receiver_number,j,k,i)){
                    printf("receiver %d, %d !=%d from %d value %d\n",receiver_number, (int)ptr[0],(int)((char)MAP_DATA(receiver_number,j,k,i)),i, j);
                    return 1;
                }
                ptr++;
            }

        }
    }
    return 0;
}

