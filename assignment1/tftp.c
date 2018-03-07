#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdint.h>
#include <signal.h>

#define TIMEOUT 1
#define RETRIES 10
#define BUF_LEN 512

enum opcode {
     RRQ=1,
     WRQ,
     DATA,
     ACK,
     ERROR
};

//structure for messages
typedef union {

     uint16_t opcode;
	//RRQ and WRQ structure
     struct {
          uint16_t opcode;
          uint8_t filename[514];
     } request;
	//DATA structure
     struct {
          uint16_t opcode;
          uint16_t number;
          uint8_t data[512];
     } data;
	//ACK structure
     struct {
          uint16_t opcode;
          uint16_t number;
     } ack;
	//ERROR structure
     struct {
          uint16_t opcode;
          uint16_t code;
          uint8_t string[512];
     } error;

} message;

// Basic function to send errors to client based off RFC 1350 error codes
ssize_t sendError(int s, int code, char *info,
                        struct sockaddr_in *sock, socklen_t slen)
{
     message m;
     ssize_t c;

     if(strlen(info) >= 512) {
          fprintf(stderr, "server: error string too long\n");
          return -1;
     }

     m.opcode = htons(ERROR);
     m.error.code = code;
     strcpy(m.error.string, info);

     if ((c = sendto(s, &m, 4 + strlen(info) + 1, 0,
                     (struct sockaddr *) sock, slen)) < 0) {
          perror("server: sendto()");
     }

     return c;
}


/*
Funtion to handle any message recieved from the client
only handles RRQ and WRQ
*/
void handler(message *m,ssize_t len, struct sockaddr_in *client_sock, socklen_t slen){

	int s;
	struct timeval tv;
  char *filename, *mode_s, *end;
  FILE *fd;

  //open new socket for client
	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
          perror("server: socket()");
          exit(1);
  }

  //set socket's timeout values
	tv.tv_sec  = TIMEOUT;
  tv.tv_usec = 0;

	if (setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        perror("setsockopt failed\n");

    if (setsockopt (s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        perror("setsockopt failed\n");

  // Get filename and opcode from message struct
	filename = m->request.filename;
	uint16_t opcode=ntohs(m->opcode);

  // Read request handler
	if(opcode==RRQ){

		if( access( filename, F_OK ) == -1 ) {

      //file does not exist
			perror(filename);
			sendError(s,htons(1),"File not found",client_sock,slen);
			exit(1);
    }

    else {

      //open file for reading
  		fd = fopen(filename, "r");
     	if (fd == NULL) {
          perror("server: fopen()");
          sendError(s, htons(0), strerror(errno), client_sock, slen);
          exit(1);
     	}

		//read the file in 512 byte chunks
		uint16_t blockNum=0;
		int needClose=0;

    // This loop will run as long as there 512 bytes to be read
		while(needClose==0) {

      int count;
			ssize_t lenRead=0;
			ssize_t n=0;
			message m2;
			uint8_t read[BUF_LEN];
			for(int i = 0; i<512;i++){
				read[i]='\0';
			}

      // Store data in a buffer
			lenRead=fread(read,1,BUF_LEN,fd);
			blockNum+=1;

      // Check if we will need to transfer more packets
			if(lenRead<BUF_LEN){
				needClose=1;
			}

			//try 10 times to send the data
			for(count=RETRIES;count;count--){
				message hold;
				struct sockaddr_in hold_socket;
				hold.opcode = htons(DATA);
     			hold.data.number = htons(blockNum);
				for(int i = 0; i<512;i++){
					hold.data.data[i]='\0';
				}

        memmove(hold.data.data, read, lenRead);

        //send data
     		n = sendto(s, &hold, lenRead+4, 0, (struct sockaddr *) client_sock, slen);
        if(n<0){
					perror("sendto");
					exit(1);
				}

				//recieve ack
				n=recvfrom(s,&m2,sizeof(m2),0,(struct sockaddr *) &hold_socket,&slen);

        //recieved invalid message
				if (n>=0 && n<4){
					exit(1);
				}

        // Break when we recieve a valid message
				if(n>=4){
					n=0;
					break;
				}
			}

      // Check if we ran out of transfer retries
			if (!count){
				printf("transfer timed out\n");
				exit(1);
			}

			if(ntohs(m2.opcode)!=ACK){
				printf("recieved invalid message during transfer\n");
				exit(1);
			}

			if(ntohs(m2.ack.number)!=blockNum){
				printf("recieved invalid ack number\n");
				sendError(s,htons(0),"Invalid ACK number",client_sock,slen);
				exit(1);

			}
			if(ntohs(m2.opcode)==ERROR){
				printf("recieved error message from client");
				exit(1);
			}
		}
		}
	}

  // Write request handler
	else if(opcode ==WRQ){

    // Check if the file already exists
		if( access( filename, F_OK ) != -1 ) {
			sendError(s,htons(6),"File already exists",client_sock,slen);
			exit(1);
    }
    else {
	 		fd = fopen(filename, "w");
     		if (fd == NULL) {
          		perror("server: fopen()");
          		sendError(s, htons(0), strerror(errno), client_sock, slen);
          		exit(1);
     		}

			//read the file in 512 byte chunks
			uint16_t blockNum=0;
			message a;
			a.opcode=htons(ACK);
			a.ack.number=htons(blockNum);
			ssize_t n=0;

      //send initial ack message
			n=sendto(s,&a,sizeof(a.ack),0,(struct sockaddr *) client_sock,slen);
			if(n<0){
				printf("transfer killed\n");
				exit(1);
			}

      // Run the loop while there is still data that needs to be transferred
			int needClose=0;
			while(needClose==0){
				int count;
				message m2;

        //try 10 times to send the ack
				for(count=RETRIES; count;count--){
          printf("%d\n",count );
					struct sockaddr_in hold_socket;

          //recieved data
					n=recvfrom(s,&m2,sizeof(m2),0,(struct sockaddr *) &hold_socket,&slen);
					if(n>=0&&n<4){

            //recieved invalid message
						sendError(s, htons(0), "Invalid request size", client_sock, slen);
						exit(1);
					}

          // Break when we recieve a valid message
					if (n>=4) {
						break;
					}

					//send ack
					a.ack.number=htons(blockNum);
					n=sendto(s,&a,sizeof(a.ack),0,(struct sockaddr *) client_sock,slen);
					if(n<0){
						printf("transfer killed\n");
						exit(1);
					}
				}

				if(!count){
					printf("transfer timed out\n");
					exit(1);
				}
				blockNum++;

        // Break when we are done recieving data
				if (strlen(m2.data.data)<512){
					needClose=1;
				}

				if(ntohs(m2.opcode)==ERROR){
					printf("Recieved ERROR message\n");
					exit(1);
				}
				if(ntohs(m2.opcode)!=DATA){
					printf("Recieved incorrect message\n");
					sendError(s,htons(0),"Incorrect message passed during transfer",client_sock, slen);
				}
				if (ntohs(m2.ack.number) != blockNum) {
					printf("Invalid block number\n");
					sendError(s,htons(0),"Invalid block number\n",client_sock, slen);
				}

				//write data to file
				n = fwrite(m2.data.data, 1, n - 4, fd);
				if(n<0){
					perror("fwrite failed");
					exit(1);
				}
				a.ack.number=htons(blockNum);
				n=sendto(s,&a,sizeof(a.ack),0,(struct sockaddr *) client_sock,slen);

         if (n < 0) {
              printf("transfer killed\n");
              exit(1);
         }
			}

			printf("Completed transfer\n");
			fclose(fd);
			close(s);
		}
	}
}





int main() {
    ssize_t n;
    char buffer[BUF_LEN];
    socklen_t sockaddr_len;
    int server_socket;
    unsigned short int opcode;
    unsigned short int * opcode_ptr;
    struct sockaddr_in sock_info;

    sockaddr_len = sizeof(sock_info);

    /* Set up UDP socket */
    memset(&sock_info, 0, sockaddr_len);

    sock_info.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_info.sin_port = htons(0);
    sock_info.sin_family = PF_INET;

    if((server_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }

    if(bind(server_socket, (struct sockaddr *)&sock_info, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }

    /* Get port and print it */
    getsockname(server_socket, (struct sockaddr *)&sock_info, &sockaddr_len);

    printf("Starting TFTP Server on port: %d\n", ntohs(sock_info.sin_port));

    /* Receive the first packet and deal w/ it accordingly */
    while(1) {


        struct sockaddr_in client_sock;
        socklen_t slen = sizeof(client_sock);

        message m;

        // Recieve a message and create a message object
        n = recvfrom(server_socket, &m, sizeof(m), 0, (struct sockaddr *) &client_sock, &slen);

        // Error check for recieve
        if(n < 0) {
            // if(errno == EINTR) goto intr_recv;
            perror("recvfrom");
            exit(-1);
        }

        /* check the opcode */
        opcode = ntohs(m.opcode);

        if(opcode != RRQ && opcode != WRQ) {
            /* Illegal TFTP Operation */
            *opcode_ptr = htons(ERROR);
            *(opcode_ptr + 1) = htons(4);
            *(buffer + 4) = 0;

        intr_send:
            n = sendto(server_socket, buffer, 5, 0,
                       (struct sockaddr *)&sock_info, sockaddr_len);
            if(n < 0) {
                n = sendto(server_socket, buffer, 5, 0,
                           (struct sockaddr *)&sock_info, sockaddr_len);
                perror("sendto");
                exit(-1);
            }
        }

        else {
            if(fork() == 0) {
      				close(server_socket);
      				handler(&m, n, &client_sock, slen);
      				exit(0);
            }
            else {
                /* Parent - continue to wait */
            }
        }
    }
    return 0;
}
