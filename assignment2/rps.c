/* include fig01 */
#include <stdio.h>			// For stdout, stderr
#include <string.h>			// For strlen(), strcpy(), bzero()
#include <errno.h>      // For errno, EINTR
#include <time.h>
#include <sys/time.h>		// For struct timeval
#include <arpa/inet.h>	// For inet_addr()
#include <sys/socket.h>
#include <unistd.h>     // For getopt() and optind
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <dns_sd.h>

/* Size of send queue (messages). */
#define MAXLINE 10
#define LISTENQ 1024

char* names[2];
char* moves[2];
int global_index;
int done;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// DNS SD stuff
DNSServiceErrorType err;

void foundError(){
  pthread_mutex_lock (&lock);
  done++;
  pthread_mutex_unlock (&lock);
}

void * serviceClient(void *fd) {
  int index = 0;
  int client = *((int *) fd);

  // Ask the new client for their name
  pthread_mutex_lock (&lock);
  index = global_index;
  global_index++;
  pthread_mutex_unlock (&lock);

  // Ask the new client for their name
  char* name = calloc(1024,sizeof(char));
  char* temp = "What is your name?\n";
  int num = 0;
  int check = 1;

  while (check == 1) {
    if ((send(client,temp, strlen(temp), 0)) <= 0) {
          foundError();
          close(client);
          pthread_exit(NULL);
    }
    if ((num = recv(client, name, 1024,0))<= 0) {
      foundError();
      pthread_exit(NULL);
    }
    // Convert to upper case
    name[num-1] = '\0';
    int check2 = 0;
    for (int j=0; j < strlen(name) ; j++) {
      if (!isalpha(name[j])) {
        check2 = 1;
        break;
      }
      else {
        name[j] = toupper((unsigned char) name[j]);
      }
    }
    if (check2 == 0) {
      check = 0;
    }
  }

  names[index] = calloc(num,sizeof(char));
  names[index] = name;

  //Ask the client for their moves
  char* move = calloc(1024,sizeof(char));
  temp = "Rock, paper, or scissors?\n";
  check = 1;

  while (check == 1) {
    if ((send(client,temp, strlen(temp), 0)) <= 0) {
        foundError();
          close(client);
          pthread_exit(NULL);
    }
    if ((num = recv(client, move, 1024,0))<=0) {
        foundError();
          pthread_exit(NULL);
    }
    // Convert to upper case
    move[num-1] = '\0';
    int check2 = 0;
    for (int j=0; j < strlen(move) ; j++) {
      if (!isalpha(move[j])) {
        check2 = 1;
        break;
      }
      else {
        move[j] = toupper((unsigned char) move[j]);
      }
    }
    //Ensure valid move is entered
    if(strcmp(move,"ROCK")!=0 && strcmp(move,"PAPER")!=0 && strcmp(move,"SCISSORS")!=0){
      check2=1;
    }
    if (check2 == 0) {
      check = 0;
    }
  }

  moves[index] = calloc(num,sizeof(char));
  moves[index] = move;

  //All data gathered
  pthread_mutex_lock (&lock);
  done--;
  pthread_mutex_unlock (&lock);
}

void HandleEvents(DNSServiceRef serviceRef, struct sockaddr_in servaddr) {
  // Bonjour setup
  int stopNow = 0;
  int dns_sd_fd = DNSServiceRefSockFD(serviceRef);
	int nfds = dns_sd_fd + 1;
	fd_set readfds;

  // Socket and TCP setup
  int					i, maxi, maxfd, listenfd, connfd, sockfd;
  int					nready, client[2];
  ssize_t				n;
  fd_set				rset, allset;
  char				buf[MAXLINE];
  socklen_t			clilen;
  struct sockaddr_in	cliaddr;//, servaddr;
  pthread_t *threads = calloc(2, sizeof(pthread_t));

  if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("socket");
      exit(-1);
  }
  if(bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) {
      perror("bind");
      exit(-1);
  }

	listen(listenfd, LISTENQ);

  //infinite loop
  while(!stopNow) {
    global_index=0;
    done=2;
    maxfd = DNSServiceRefSockFD(serviceRef);
    int nfds = maxfd + 1;
  	maxi = -1;					/* index into client[] array */
    struct timeval tv;
    tv.tv_sec = 60;
    tv.tv_usec = 0;

  	for (i = 0; i < 2; i++) {
  		client[i] = -1;		/* -1 indicates available entry */
    }

  	FD_ZERO(&allset); // clear the fd set
  	FD_SET(maxfd, &allset); // add maxfd to the fd set

    // Wait for 2 client connections
    i = 0;
    while( i < 2 ) {
  	  rset = allset;		/* structure assignment */
  		nready = select(nfds, &readfds, (fd_set*)NULL, (fd_set*)NULL, &tv);

      if (FD_ISSET(maxfd, &readfds)) {	/* new client connection */
        err = DNSServiceProcessResult(serviceRef);
        if (err) {
          fprintf(stderr,"DNSServiceProcessResult returned %d\n", err);
          stopNow = 1;
        }
  			clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
        if (connfd == -1) {
          perror("Accept failed");
        }
  			else {
          client[i] = connfd;
          i++;
        }
      }
    }

    // Make the threads
    for(i = 0; i < 2; i++) {
      pthread_t client_thread;
      if (pthread_create(&client_thread, NULL, serviceClient, (void *) &client[i]) != 0) {
        perror("Could not create a client thread");
        pthread_exit(NULL);
      }
      threads[i] = client_thread;
    }
    for(i=0; i < 2;i++) {
      pthread_join(threads[i],NULL);
    }

    //Done collecting information: play the game and send the results to the clients
    if(done==0) {
      char* player1;
      char* player2;
      char* temp=calloc(1024,sizeof(char));
      if(strcmp(moves[0],"ROCK")==0) {
        if(strcmp(moves[1],"ROCK")==0) {
          strcat(temp,"TIE\n");
          for(int j = 0; j<2;j++){
            if ((send(client[j],temp, strlen(temp), 0)) == -1) {
              fprintf(stderr, "Failure Sending Message\n");
              exit(1);
            }
          }
        } else if(strcmp(moves[1],"PAPER")==0) {
          strcat(temp,"PAPER covers ROCK!  ");
          strcat(temp,names[1]);
          strcat(temp," defeats ");
          strcat(temp,names[0]);
          strcat(temp,"!\n");
          for(int j = 0; j<2;j++){
            if ((send(client[j],temp, strlen(temp), 0)) == -1) {
              fprintf(stderr, "Failure Sending Message\n");
              exit(1);
            }
        }
        } else if(strcmp(moves[1],"SCISSORS")==0) {
          strcat(temp,"ROCK smashes SCISSORS!  ");
          strcat(temp,names[0]);
          strcat(temp," defeats ");
          strcat(temp,names[1]);
          strcat(temp,"!\n");
          for(int j = 0; j<2;j++){
            if ((send(client[j],temp, strlen(temp), 0)) == -1) {
                  fprintf(stderr, "Failure Sending Message\n");
                  exit(1);
            }
          }
        }
      } else if(strcmp(moves[0],"PAPER")==0) {
        if(strcmp(moves[1],"ROCK")==0){
          strcat(temp,"PAPER covers ROCK!  ");
          strcat(temp,names[0]);
          strcat(temp," defeats ");
          strcat(temp,names[1]);
          strcat(temp,"!\n");
          for(int j = 0; j<2;j++) {
            if ((send(client[j],temp, strlen(temp), 0)) == -1) {
                  fprintf(stderr, "Failure Sending Message\n");
                  exit(1);
            }
          }
        } else if(strcmp(moves[1],"PAPER")==0) {
          strcat(temp,"TIE\n");
          for(int j = 0; j<2;j++) {
            if ((send(client[j],temp, strlen(temp), 0)) == -1) {
                  fprintf(stderr, "Failure Sending Message\n");
                  exit(1);
            }
          }
        } else if(strcmp(moves[1],"SCISSORS")==0) {
          strcat(temp,"SCISSORS cuts PAPER!  ");
          strcat(temp,names[1]);
          strcat(temp," defeats ");
          strcat(temp,names[0]);
          strcat(temp,"!\n");
          for(int j = 0; j<2;j++) {
            if ((send(client[j],temp, strlen(temp), 0)) == -1) {
                  fprintf(stderr, "Failure Sending Message\n");
                  exit(1);
            }
          }
        }
      } else if(strcmp(moves[0],"SCISSORS")==0) {
        if(strcmp(moves[1],"ROCK")==0) {
          strcat(temp,"ROCK smashes SCISSORS!  ");
          strcat(temp,names[1]);
          strcat(temp," defeats ");
          strcat(temp,names[0]);
          strcat(temp,"!\n");
          for(int j = 0; j<2;j++) {
            if ((send(client[j],temp, strlen(temp), 0)) == -1) {
                  fprintf(stderr, "Failure Sending Message\n");
                  exit(1);
            }
          }
        } else if(strcmp(moves[1],"PAPER")==0) {
          strcat(temp,"SCISSORS cuts PAPER!  ");
          strcat(temp,names[0]);
          strcat(temp," defeats ");
          strcat(temp,names[1]);
          strcat(temp,"!\n");
          for(int j = 0; j<2;j++) {
            if ((send(client[j],temp, strlen(temp), 0)) == -1) {
              fprintf(stderr, "Failure Sending Message\n");
              exit(1);
            }
          }
        } else if(strcmp(moves[1],"SCISSORS")==0) {
          strcat(temp,"TIE\n");
          for(int j = 0; j<2;j++){
            if ((send(client[j],temp, strlen(temp), 0)) == -1) {
              fprintf(stderr, "Failure Sending Message\n");
              exit(1);
            }
          }
        }
      }
    } else {
      printf("CLIENT DISCONNECT\n");
      char* temp = "Opponent disconnected. Game Over\n";
      for(int j = 0; j<2; j++){
        send(client[j],temp, strlen(temp), 0);
      }
    }
    printf("GAME OVER\n");
    for(int j = 0; j<2;j++) {
      close(client[j]);
    }
  }
}

// Register the client
static DNSServiceErrorType MyDNSServiceRegister() {
	DNSServiceErrorType error;
	DNSServiceRef serviceRef;

  struct sockaddr_in	cliaddr, servaddr;
  pthread_t *threads = calloc(2, sizeof(pthread_t));

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = DNSServiceRefSockFD(serviceRef);

  error = DNSServiceRegister(&serviceRef,
								0,                  // no flags
								0,                  // all network interfaces
								"tayloa5",          // name
								"_rps._tcp",        // service type
								"local",            // register in default domain(s)
								NULL,               // use default host name
								servaddr.sin_port,           // port number
								0,                  // length of TXT record
								NULL,               // no TXT record
                NULL,               // call back function
								NULL);              // no context

	if (error == kDNSServiceErr_NoError) {
		HandleEvents(serviceRef, servaddr);
		DNSServiceRefDeallocate(serviceRef);
	}

	return error;
}

int main(int argc, char **argv) {

  // Start the service
  DNSServiceErrorType error = MyDNSServiceRegister();
  fprintf(stderr, "DNSServiceDiscovery returned %d\n", error);
  return 0;
}
