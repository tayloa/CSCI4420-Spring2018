#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>

// timeout --signal=USR1 15 mpirun.openmpi -np 2 ./a.out
int end_now = 0;

int isprime(int n) {
int i,squareroot;
if (n>10) {
   squareroot = (int) sqrt(n);
   for (i=3; i<=squareroot; i=i+2)
      if ((n%i)==0)
         return 0;
   return 1;
   }
/* Assume first four primes are counted elsewhere. Forget everything else */
else
   return 0;
}

void sig_handler(int signo)
{
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

   while (1){
       // MPI_Allreduce is called here to sum up the subtotal calculated by child processes
       MPI_Allreduce(&end_now,&end_now,1,MPI_INT,MPI_MAX,MPI_COMM_WORLD);
        if (end_now == 1){
            printf("BREAK\n");
            break;
        }
    }
    MPI_Finalize ();
}
