#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <limits.h>
#include <time.h>
double MPI_Wtime(void);

int end_now = 0;
int max_value=10;
int total=0;

// check if a number is prime
int isPrime(int num) {
  for(int i = 2; i<=(int) sqrt(num);i++){
    if(num%i==0){
      return 0;
    }
  }
  return 1;

}

void sig_handler(int signo) {
    if (signo == SIGUSR1) {
        end_now = 1;
    }
}

int main ( int argc, char **argv ){
   int id;
   int count;

   MPI_Init (&argc, &argv);
   MPI_Comm_size (MPI_COMM_WORLD, &count);
   MPI_Comm_rank (MPI_COMM_WORLD, &id);

   signal(SIGUSR1, sig_handler);

   int j = 0;
   int i = id;
   int localTotal = 0;

   if (id == 0) {
     printf("%*s\t%*s\n",12, "N", 12, "Primes" );
   }

   double startTime, endTime;
   time_t start,end;
   start = clock();
   startTime = MPI_Wtime();

   while (1){
       MPI_Allreduce(MPI_IN_PLACE,&end_now,1,MPI_INT,MPI_MAX,MPI_COMM_WORLD);
       if (end_now == 1 || i == INT_MAX || i < 0) {
            break;
       } else if ( i < max_value) {
           if (j >= 10) {
             MPI_Barrier(MPI_COMM_WORLD);
             j = 0;
           }
            if (isPrime(i) && i > 1) {
              localTotal+=1;
            }
          i+=count;
          j+=1;
        } else {
          MPI_Reduce(&localTotal,&total,1,MPI_INT,MPI_SUM,0,MPI_COMM_WORLD);
          end = clock();
          endTime = MPI_Wtime();
          if (id == 0) {
            // printf("%lf\n", ((double)(end - start)/CLOCKS_PER_SEC) / (endTime - startTime));
            printf("%12d\t%12d Serial:%lf Parallel:%lf\n", max_value, total, (double)(end - start)/CLOCKS_PER_SEC, endTime - startTime);
          }
          max_value=max_value*10;
        }
    }
    MPI_Allreduce(&localTotal,&total,1,MPI_INT,MPI_SUM,MPI_COMM_WORLD);
    MPI_Allreduce(MPI_IN_PLACE,&i,1,MPI_INT,MPI_MAX,MPI_COMM_WORLD);
    if (id == 0) {
      printf("%12d\t%12d\n", i, total);
    }
    MPI_Finalize();
}
