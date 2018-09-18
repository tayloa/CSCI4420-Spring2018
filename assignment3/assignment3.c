/* include fig01 */
#include <stdio.h>			// For stdout, stderr
#include <string.h>			// For strlen(), strcpy(), bzero()
#include <arpa/inet.h>	// For inet_addr()
#include <sys/socket.h>
#include <unistd.h>     // For getopt() and optind
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

/* Size of send queue (messages). */
#define MAXLINE 10
#define LISTENQ 1024

typedef struct User {
   int fd, isOperator,maxChannels;
   char* name;
   char** channels;
}User;

typedef struct Channel {
  char* name;
  struct User* users;
  int maxUsers;
  int numUsers;
}Channel;

Channel channels[1024];
int countChannels;
int numChannels;
User allUsers[1024];
int totalUsers;

char* names[2];
char* moves[2];
int global_index;
int done;
char* password;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
void commandQuit(User dest);

//send a message to a user
void sendUser(char* self, User dest, char* msg){
  char info[1024];
  strcpy(info,self);
  strcat(info,msg);
  strcat(info,"\n");
  if ((send(dest.fd,info, strlen(info), 0)) <= 0) {
      commandQuit(dest);
  }

}

void sendChannel(User self,Channel channel, char* msg){
  char name[1024];
  strcpy(name,channel.name);
  strcat(name,"> ");
  strcat(name,self.name);
  strcat(name,": ");
  for(int i = 0; i<channel.maxUsers;i++){
    if(strcmp(channel.users[i].name,"-")!=0){
      sendUser(name,channel.users[i],msg);
    }

  }
}

//check if nickname is valid
int checkName(char* name) {
  if (!isalpha(name[0])) {
    return 0;
  }

  for (int i=1;i < strlen(name); i++) {
    if (!isalpha(name[i]) && !isdigit(name[i]) && name[i] != '_') {
      return 0;
    }
  }
  return 1;
}
//check if channel name is vaid
int checkChannel(char* name) {
  if(strlen(name)<=1 || strlen(name)>20){
    return 0;
  }
  if(name[0]!='#'){
    return 0;
  }
  if (!isalpha(name[1])) {
    return 0;
  }

  for (int i=2;i < strlen(name)-1; i++) {
    if (!isalpha(name[i]) && !isdigit(name[i]) && name[i] != '_') {
      return 0;
    }
  }
  return 1;
}

//send message that user has joined
void sendMsgJoin(User self,Channel dest){
  //send message to self
  char*temp=calloc(1024,sizeof(char));
  temp=strcpy(temp,"Joined channel ");
  temp=strcat(temp,dest.name);
  temp=strcat(temp,"\n");
  send(self.fd,temp,strlen(temp),0);
  //send message to everyone else in channel
  temp=calloc(1024,sizeof(char));
  temp=strcpy(temp,self.name);
  temp=strcat(temp," joined the channel.");
  char*name = calloc(21,sizeof(char));
  name=strcpy(name,dest.name);
  name=strcat(name,"> ");
  for(int i = 0; i<dest.maxUsers;i++){
    if(strcmp(dest.users[i].name,"-")!=0 && strcmp(dest.users[i].name,self.name)!=0){
      sendUser(name,dest.users[i],temp);
    }
  }
}


//JOIN comand
void commandJoin(char* title,struct User self){
  //check if title is of valid structure
  char* temp;
  Channel hold;
  if(countChannels>0)
  if(!checkChannel(title)){
    temp="Invalid channel name.\n";
    if ((send(self.fd,temp, strlen(temp), 0)) <= 0) {
      close(self.fd);
      self.fd=-1;
      pthread_exit(NULL);
    }
  }
  pthread_mutex_lock (&lock);
  int check = 0;
  for(int i = 0; i<countChannels;i++){
    //found channel already exists
    if(strcmp(title,channels[i].name)==0){
      check=1;
      int check2=0;
      for(int j = 0; j<channels[i].maxUsers;j++){
        //found user already in channel
        if(strcmp(self.name,channels[i].users[j].name)==0){
          check2=1;
          //just quietly continue
          break;
        }
      }
      //user not in channel yet
      if(check2==0){
        hold=channels[i];
        int check3=0;
        for(int j = 0; j<channels[i].maxUsers;j++){
          if(strcmp(channels[i].users[j].name,"-")==0){
            //found null user in correct channel, add new user in this slot
            int k;
            for(k = 0; k<self.maxChannels;k++){
              //found null channel name in user's list
              if(strcmp(self.channels[k],"-")==0){
                self.channels[k]=title;
                break;
              }
            }
            //didn't find null channel name so add to end of list
            if(k==self.maxChannels){
              self.channels[self.maxChannels]=title;
              self.maxChannels+=1;
            }
            channels[i].users[j]=self;
            check3=1;
            break;
          }
        }
        if(check3==0){
          //no null users, increase the space and add user
          int k;
          for(k = 0; k<self.maxChannels;k++){
            //found null channel name
            if(strcmp(self.channels[k],"-")==0){
              self.channels[k]=title;
              break;
            }
          }
          if(k==self.maxChannels){
            self.channels[k]=title;
            self.maxChannels+=1;
          }
          channels[i].users[channels[i].maxUsers]=self;
          channels[i].maxUsers+=1;
          channels[i].numUsers+=1;

        }
      }
      //exit for loop
    }
  }
  //channel not found
  if(check==0){
    //create new channel
    hold.name=calloc(1024,sizeof(char));
    hold.name=strcpy(hold.name,title);
    User users[1024];
    hold.users=users;
    hold.maxUsers=1;
    hold.numUsers=1;
    int k;
    for(k = 0; k<self.maxChannels;k++){
      //found null channel name
      if(strcmp(self.channels[k],"-")==0){
        self.channels[k]=title;
        break;
      }
    }
    if(k==self.maxChannels){
      self.channels[k]=calloc(1024,sizeof(char));
      self.channels[k]=strcpy(self.channels[k],title);
      self.maxChannels+=1;
    }
    hold.users[0]=self;
    //add new channel to list of channels and increase count
    channels[countChannels]=hold;
    countChannels+=1;
    numChannels+=1;
  }
  sendMsgJoin(self,hold);
  pthread_mutex_unlock (&lock);
}
//PRIVMSG command
void commandPrivmsg(User self,char* command){
  int end;
  int i;
  int start;
  int a=0;
  char* title=calloc(1024,sizeof(char*));
  char* msg=calloc(1024,sizeof(char*));
  for(i = 0; i<strlen(command);i++){
    if(command[i]==' ' && a==0){
      start=i+1;
      a=1;
      i++;
    }
    if(command[i]!=' '&& a==1){
      title[i-start]=command[i];
    }
    if(command[i]==' ' && a==1){
      end=i+1;
      a=3;
      title[i-start]='\0';
      i++;
    }
    if(command[i]!='\n' && a==3){
      msg[i-end]=command[i];
    }
    if(a==3 && (command[i]=='\0')){
      msg[i-end]='\0';
    }
  }
  if(strlen(msg)==0 || strlen(title)==0){
    //INVALID COMMAND
  }else{
    if(checkChannel(title)){
      //need to message channel
      Channel dest;
      int found = 0;
      for(int i = 0; i<countChannels;i++){
        if(strcmp(title,channels[i].name)==0){
          found =1;
          dest=channels[i];
          break;
        }
      }
      if(found==1){
        int confirm = 0;
        for(int i = 0; i<dest.maxUsers;i++){
          if(strcmp(self.name,dest.users[i].name)==0){
            confirm=1;
          }
        }
        if(confirm==1){
          sendChannel(self,dest,msg);
        }else{
          //USER is not in channel, output error
          char* temp=calloc(1024,sizeof(char));
          temp=strcpy(temp,"User is not in ");
          temp=strcat(temp,dest.name);
          temp=strcat(temp,", could not send message\n");
          send(self.fd,temp, strlen(temp), 0);
        }
      }else{
        //invalid user send error message
        char* temp = "Channel does not exist\n";
        send(self.fd,temp, strlen(temp), 0);
      }
    }else{
      //need to message user
      User dest;
      int found = 0;
      for(int i = 0; i<totalUsers;i++){
        if(strcmp(title,allUsers[i].name)==0){
          dest=allUsers[i];
          found = 1;
          break;
        }
      }
      if (found ==1){
        char info[1024];
        strcpy(info,self.name);
        strcat(info,"> ");
        sendUser(info,dest,msg);
      }else{
        //invalid user send error message
        char* temp = "User does not exist\n";
        send(self.fd,temp, strlen(temp), 0);
      }
    }
  }
}

// LIST command
void commandList(int client) {
  pthread_mutex_lock (&lock);
  char* temp=calloc(1024,sizeof(char));
  sprintf(temp,"There are currently %d channels.\n", numChannels);
  // Build the response string
  for (int i=0; i < numChannels;i++) {
    char* clean_name=calloc(19,sizeof(char)); // channel name without '#'
    strncpy(clean_name, channels[i].name+1, 19);
    char* temp2=calloc(21,sizeof(char));
    printf("%d\n",channels[i].numUsers );
    sprintf(temp2 + strlen(temp2), "* %s\n", clean_name);
    strcat(temp, temp2);
  }
  send(client,temp, strlen(temp), 0);
  pthread_mutex_unlock (&lock);
}

// LIST users command
// Find the channel and display its users
void getUsers(int client, char* cname) {
  pthread_mutex_lock (&lock);
  printf("%s\n",cname );
  char* temp=calloc(1024,sizeof(char));
  for (int i=0; i < numChannels;i++) {
    if (strcmp(channels[i].name, cname) == 0) {
      sprintf(temp,"There are currently %d members.\n",channels[i].numUsers );
      temp=strcpy(temp,channels[i].name);
      temp=strcat(temp," members:");
      for (int j=0; j < channels[i].numUsers;j++) {
        /*
        printf("%s\n",channels[i].users[j].name );
        char clean_name[20];
        strncpy(clean_name, channels[i].users[j].name, 20);
        char temp2[21];
        sprintf(temp2 + strlen(temp2), "* %s\n", clean_name);
        strcat(temp, temp2);*/
        if(strcmp(channels[i].users[j].name,"-")!=0){
          temp=strcat(temp," ");
          temp=strcat(temp,channels[i].users[j].name);
        }
      }
    }
  }
  temp=strcat(temp,"\n");
  send(client,temp, strlen(temp), 0);
  pthread_mutex_unlock (&lock);
}

int commandOperator(User self, char* pass) {
  if (strcmp(pass, password) == 0) {
    for (int i=0; i < totalUsers; i++) {
      if (strcmp(allUsers[i].name, self.name) == 0) {
        allUsers[i].isOperator = 1;
        return 1;
      }
    }
  }
  return 0;
}

// kick command
void commandKick(User self, char* cname, char* user) {
  Channel hold;
  int found=0, check=0, operator=0;
  char* output= calloc(1024,sizeof(char));
  output=strcpy(output,cname);
  output=strcat(output,"> ");
  output=strcat(output,user);
  output=strcat(output," has been kicked from the channel.\n");

  // check for operator status
  for (int i=0; i < totalUsers; i++) {
    if(strcmp(self.name,allUsers[i].name)==0) {
      if (allUsers[i].isOperator) {
        operator = 1;
        break;
      }
    }
  }

  if (operator) {
    for(int i=0; i<countChannels;i++){
      if(strcmp(channels[i].name,cname)==0) {
        found=1;
        // remove the user from the channel
        for(int j=0; j<channels[i].maxUsers;j++) {
          if(strcmp(channels[i].users[j].name,user)==0) {
            send(channels[i].users[j].fd,output,strlen(output),0);
            channels[i].users[j].name="-";
            check = 1;
            break;
          }
        }

        // if the channel and user were found, alert all users of the kick
        if (found && check) {
          for(int j = 0; j<channels[i].maxUsers;j++){
            if(strcmp(channels[i].users[j].name,"-")!=0){
              send(channels[i].users[j].fd,output,strlen(output),0);
            }
          }
        }
      }
    }
  }
}

//part command
void part(User self,Channel dest){
  char* output= calloc(1024,sizeof(char));
  output=strcpy(output,dest.name);
  output=strcat(output,"> ");
  output=strcat(output,self.name);
  output=strcat(output," has left the channel.\n");
  for(int i = 0; i<dest.maxUsers;i++){
    if(strcmp(dest.users[i].name,self.name)==0){
      dest.users[i].name="-";
      send(self.fd,output,strlen(output),0);
    }
    if(strcmp(dest.users[i].name,"-")!=0){
      send(dest.users[i].fd,output,strlen(output),0);
    }
  }
}

//part from only one channel
void partOne(User self,char* dest){
  int found = 0;
  Channel hold;
  for(int i = 0; i<countChannels;i++){
    if(strcmp(channels[i].name,dest)==0){
      found=1;
      hold=channels[i];
    }
  }
  if (found) {
    int check = 0;
    for(int i = 0; i<hold.maxUsers;i++){
      if(strcmp(hold.users[i].name,self.name)==0){
        check=1;
      }
    }
    if (check==1) {
      part(self,hold);
    } else {
      char*temp = calloc(1024,sizeof(char));
      temp=strcpy(temp,"You are not currently in ");
      temp=strcat(temp,hold.name);
      temp=strcat(temp,"\n");
      send(self.fd,temp,strlen(temp),0);
    }
  } else {
    char*temp="Channel does not exist\n";
    send(self.fd,temp,strlen(temp),0);
  }
}

//part from all channels
void partAll(User self){
  char* name = calloc(1024,sizeof(char*));
  name=strcpy(name,self.name);
  for(int i = 0; i<countChannels;i++){
    printf("TRY: %s\n",channels[i].name);
    for(int j = 0; j<channels[i].maxUsers;j++){
      printf("CHECK: %s\n",channels[i].users[j].name);
      if(strcmp(channels[i].users[j].name,name)==0){
        //user is in this channel, leave it
        printf("FOUND: %s\n",channels[i].name);
        part(self,channels[i]);
        printf("%s\n",self.name);
      }
    }
  }
}

//QUIT command
void commandQuit(User self){
  partAll(self);
  for(int i = 0; i<totalUsers;i++){
    if(strcmp(allUsers[i].name,self.name)==0){
      allUsers[i].name="-";
    }
  }
  self.name="-";
  close(self.fd);
}


void * serviceClient(void *fd) {
  pthread_detach(pthread_self());
  int index = 0;
  int client = *((int *) fd);

  // Ask the new client for their name
  int num;
  char* temp;
  char* hold;
  char* command = calloc(1024,sizeof(char));
  char* name = calloc(1024,sizeof(char));

  if ((num = recv(client, command, 1024,0))<= 0) {
      pthread_exit(NULL);
  }
  //get rid of \n
  command[strlen(command)-1]='\0';
  //token
  hold=strtok(command," ");
  if(strcmp(hold,"USER")!=0){
    temp = "Invalid command, please identify yourself with USER.\n";
    send(client,temp, strlen(temp), 0);
    close(client);
    pthread_exit(NULL);
  }

  // Check name format
  name = strtok(NULL," ");
  if (strlen(name) > 20 || !checkName(name)) {
   temp = "Invalid name.\n";
   send(client,temp, strlen(temp), 0);
   close(client);
   pthread_exit(NULL);
  }

   //make sure user nickname is unique
   int already=0;
   for(int i = 0; i<totalUsers;i++){
     if(strcmp(allUsers[i].name,name)==0){
       already=1;
     }
   }
   if(already==1){
     temp = "Invalid USER command, name already taken.\n";
     send(client,temp, strlen(temp), 0);
     close(client);
     pthread_exit(NULL);
   }

   // Set up User
   User self;
   self.name = name;
   self.fd = client;
   self.isOperator = 0;
   char* a[1024];
   self.channels = a;
   self.maxChannels=0;
   //add user to list of allUsers
   int inserted=0;
   for(int i = 0; i<totalUsers;i++){
     if(strcmp(allUsers[i].name,name)==0){
       allUsers[i]=self;
       inserted=1;
       break;
     }
   }
   if(inserted==0){
     //increase maxUsers and insert
     allUsers[totalUsers]=self;
     totalUsers+=1;
   }

   // Get commands from the user
   int cont=0;
   while ( cont==0 ) {
     command = calloc(1024,sizeof(char));
     if ((num = recv(client, command, 1024,0))<= 0) {
       pthread_exit(NULL);
     }
     command[strlen(command)-1]='\0';
     printf("COMMAND: \"%s\"\n",command);
     char b[1024];
     strcpy(b,command);
     hold=strtok(b," ");

     if (strcmp("LIST",hold) == 0) {
       hold = strtok(NULL," ");
      // If LIST, return all the channels, if "LIST *channelname*"", return all channel's users
      if (hold!=NULL) {
        getUsers(client, hold);
      } else {
        commandList(client);
      }
     }
     else if (strcmp("JOIN",hold) == 0) {
       hold=strtok(NULL," ");
       if(hold==NULL){
         temp="Invalid command, please try again.\n";
         if ((send(client,temp, strlen(temp), 0)) <= 0) {
           close(client);
           pthread_exit(NULL);
         }
       }
       commandJoin(hold,self);
     }
     else if (strcmp("PART",hold) == 0) {
       hold = strtok(NULL," ");
      // If LIST, return all the channels, if "LIST *channelname*"", return all channel's users
      if (hold!=NULL) {
        partOne(self, hold);
      } else {
        partAll(self);
      }
     }
     else if (strcmp("OPERATOR",hold) == 0) {
       char* temp = "Invalid OPERATOR command.\n";
       // If a password has been set, check if the user entered the correct one
       if (password) {
         char* pass = strtok(NULL," ");
         if (!commandOperator(self, pass)) {
           temp = "OPERATOR status denied.\n";
         } else {
           temp = "OPERATOR status bestowed.\n";
         }
      }
      send(client,temp, strlen(temp), 0);
     }
     else if (strcmp("PRIVMSG",hold) == 0) {
       commandPrivmsg(self,command);
     }
     else if (strcmp("KICK",hold) == 0) {
       char* cname = strtok(NULL," ");
       char* user = strtok(NULL," ");
       commandKick(self,cname,user);
     }
     else if (strcmp("QUIT",hold) == 0) {
       commandQuit(self);
       close(client);
       pthread_exit(NULL);
     }
     else{
       temp="Invalid command, please try again.\n";
       if ((send(client,temp, strlen(temp), 0)) <= 0) {
         close(client);
         pthread_exit(NULL);
       }
     }
   }
}

int main(int argc, char **argv) {
  numChannels=0;
  countChannels=0;
  totalUsers=0;
	int					i, maxi, maxfd, listenfd, connfd, sockfd;
	int					nready, client[2];
	ssize_t				n;
	fd_set				rset, allset;
	char				buf[MAXLINE];
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;
  pthread_t *threads = calloc(2, sizeof(pthread_t));


	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(9877);

  if (argc > 1) {
    password = strtok (argv[1],"=");
    password = strtok (NULL,"=");
    if (!checkName(password)) {
      printf("Startup failed - invalid password.\n");
      exit(1);
    }
  }

  if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("socket");
      exit(-1);
  }
  if(bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) {
      perror("bind");
      exit(-1);
  }

	listen(listenfd, LISTENQ);

	printf("%d\n", ntohs(servaddr.sin_port));

  // infinite loop
  while(1){
    global_index=0;
    done=2;
  	maxfd = listenfd;		/* initialize */
  	maxi = -1;					/* index into client[] array */
    struct timeval tv;
    tv.tv_sec = 1000000;
    tv.tv_usec = 0;

  	for (i = 0; i < 2; i++)
  		client[i] = -1;		/* -1 indicates available entry */
    i = 0;
  	FD_ZERO(&allset);
  	FD_SET(listenfd, &allset);

    while( 1 ) {
  	  rset = allset;		/* structure assignment */
  		nready = select(maxfd+1, &rset, NULL, NULL, &tv);

      if (FD_ISSET(listenfd, &rset)) {	/* new client connection */
  			clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
        if (connfd == -1) {
          perror("Accept failed");
        }
  			else {
          printf("Accepted a connection on fd: %d\n",client[i]);
          i++;
          pthread_t client_thread;
          if (pthread_create(&client_thread, NULL, serviceClient, (void *) &connfd) != 0) {
            perror("Could not create a client thread");
            pthread_exit(NULL);
          }
        }
      }
    }
  }
}
