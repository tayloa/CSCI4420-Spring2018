#include <stdlib.h>
#include <math.h>
#include <stdio.h>

int main(int argc, char **argv) {
  int total=0;
  int num=atoi(argv[1]);
  //int rank = world_rank;
  //int size = MPI_Comm_size(MPI_COMM_WORLD, &size);
  // int index = rank;
  // int cont = 0;
  // while(cont==0){
  //   index+=size;
  for(double i = 1; i<num;i++){
    double check = sqrt (i);
    int conf=0;
    for(double j = 2;j<check+1;j++){
      int first = (int)i;
      int second = (int)j;
      int ans=first%second;
      if(ans==0){
        conf=1;
      }
    }
    if(conf==0){
      total+=1;
      printf("%d\n",(int)i);
    }
  }
  printf("TOTAL: %d\n",total);
}
