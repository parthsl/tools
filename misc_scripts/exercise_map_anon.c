#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

int main(){

    int ARRAYLEN=5;
    int *ptr = mmap ( NULL, N*sizeof(int), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0 );
    if(ptr == MAP_FAILED){
        printf("Error\n");
        return -1;
    }

    for(int i=0; i<ARRAYLEN; i++){
        printf("[%d] ",ptr[i]);
    }

    int err = munmap(ptr, 10*sizeof(int));

    return 0;
}
