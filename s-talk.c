#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include "List.h"
#define MYPORT "4950"   // the port users will be connecting to

#define MAXBUFLEN 256
#define NUMT 4 

static pthread_mutex_t stdout_lock;
static pthread_mutex_t peer_list_lock;


pthread_cond_t printCondOne;
pthread_cond_t sendCondOne;
pthread_cond_t sendCondTwo;
pthread_cond_t printCondTwo;
pthread_t buf[NUMT];

LIST *printingList; 
LIST *sendingList; 

static sem_t print_sem; 
static int term; 

int sockfd = 0;  

struct addrinfo *servinfo2; 

struct addrinfo hints, *servinfo, *p, *p2;


struct addrinfo server_addr;
struct addrinfo peer_addr;

int flag1 = 0; 
int flag2 = 0; 

void itemFree(void *ptr){
     ptr = NULL;
}

//to initiate a s-talk session: s-talk(myport#, remote machine name, remote port#)
//every line typed at each terminal will appear on both terminals. 
//an s-talk session i sterminated when some special character is types by either user -- such as "!"
//there will be two UNIX processes 
//within each process tje p threads environment will be running four threads

//One of the threads does nothing other than await from the keyboard
    //KEY BOARD thread, on receipt of input, will add the input to the list of messages that need to be sent to the remote s-talk client
//the other thread does nothing other than await a UDP datagram
    // will take on receipt of input from the remote s-talk client, will put the message onto the list of messages that need to be printed to the local screen
//Another will be a thread which prints characters to the screen
    //take each message off this list and output it to the screen
//finally, a thread whcih sends data to the remote UNIX process over the network using UDP. 
    //the UDP output thread will take each message off this list and send it over the network to the remote client


//Lists that are shared between the threads will need to be carefully controlled to prevent concurrent access -- this is where Mutex comes and signals and waits on condition variables


//Need to know: https://www.bogotobogo.com/cplusplus/multithreading_pthread.php
    //Mutexes: Routines that deal with synchronization
    //to create threads:
          //main() = single, default thread.
          // pthread_create()= creates a new thread and makes it executable. 
                  //Arguments: 
                        //thread = an identified for the new thread returned by the subroutine. When a thread is created an identifier is written to the memory location to which this variable points. 
                        //attr = an attribute object that may be used to set thread attributes. 
                        //start_routine = the routine that the thread will execute once it is created. We should pass the address of a function taking a pointer to void as a parameter. Using a new thread explicitly provides a pointer to a function where the new thread should start executing
                        //arg = A single argument that may be passed to start_routine. Must be passed as a void pointer. Null otherwise.
    //once a thread is created, threads are peers and may create other threads. 

//identifying a socket:
//binding system call
//first parameter socket: socket that was created with the socket system call
//second parameter:  the structure sockaddr is a generic container that just allows the OS to be able to read the first couple of bytes that identify the address family. 
//references for cancellation points: http://man7.org/linux/man-pages/man7/pthreads.7.html
//references for general procedure on mutexs and condition threads: https://stackoverflow.com/questions/20772476/when-to-use-pthread-condition-variables
//refrences for setting up sockets and binds and getaddrinfo: Beej tutorial on coursys apges. 

void *waitKeyboard(void *ptr){
  char line[256];
  char *p;
  int numbytes; 

  printf("\nsuccess #1 for getting to waitkeyboard\n");
  //printf("\n"); 
  while (term != 1){
    pthread_mutex_lock(&stdout_lock);

    //blocked and wait for the send thread to send a udp datagram to the remote user
    while(flag1 == 1){
      pthread_cond_wait(&sendCondOne, &stdout_lock);
    }
       printf("success #2 for being in while loop for Keyboard \n"); 
       memset(line, '\0', 256);
       fgets(line, 256, stdin);
       int len = strlen(line);
       if(len > 0){
           if(line[len-1] == '\n'){
              line[len-1] = 0;
           }
           if(line[len-2] == 33){
                  //from coursys discussion page in topic "Discussion: Session Termination"
                  ListAdd(printingList, line);
                  pthread_mutex_unlock(&stdout_lock);
                  pthread_mutex_unlock(&peer_list_lock);
                  pthread_cond_signal(&sendCondTwo);
                
                  term = 1;
                  close(sockfd);
                  ListFree(printingList, itemFree);
                  ListFree(sendingList, itemFree);

                  pthread_mutex_destroy(&stdout_lock);
                  pthread_mutex_destroy(&peer_list_lock);
                  pthread_cond_destroy(&sendCondTwo);
                  pthread_cond_destroy(&sendCondOne);
                  pthread_cond_destroy(&printCondOne);
                  pthread_cond_destroy(&printCondTwo);

                  pthread_cancel(buf[1]);
                  pthread_cancel(buf[2]);
                  pthread_cancel(buf[3]);
                  pthread_exit(0);
                }else{
                  
                  printf("success #3 for being in while loop for Keyboard \n"); 
                  ListAdd(printingList, line);

                  printf("success #4 for being in while loop for Keyboard \n"); 
                  flag1 = 1; 

                  pthread_mutex_unlock(&stdout_lock);
                  pthread_cond_signal(&sendCondTwo);
                }
    
    }    
  }
  pthread_exit(0);
}


void *waitUDP(void *sock){
  struct sockaddr_storage their_addr;
  socklen_t addr_len; 
  char buf2[256];
  int numbytes;
  //printf("success");  

  addr_len = sizeof(their_addr);
  while(term != 1){

     pthread_mutex_lock(&peer_list_lock);
     int L = ListCount(printingList);
     //wait for the thread that prints to screen to print to screen. After print list is empty this will be unblocked
     while(flag2== 1){
      pthread_cond_wait(&printCondOne, &peer_list_lock);
     }

      printf("success #1 for being in while loop for waitUDP\n"); 
      if((numbytes = recvfrom(sockfd, buf2, 256, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1){
          perror("recvfrom hello");
          exit(1);
       }
       if(strlen(buf2)>0){
          if(buf2[0] == 33){
                        printf("cancelling session...");
                        term = 1; 
                        close(sockfd);
                        pthread_exit(0);
                 }

                  printf("success #2 for being in while loop for waitUDP\n"); 
                  ListAdd(sendingList, buf2);
                  flag2 = 1; 
                  pthread_mutex_unlock(&peer_list_lock);
                  pthread_cond_signal(&printCondTwo);
                 }
  
}

   pthread_exit(0);
}


void *printKeyboard(void *ptr){
  //LIST *sendingList = (LIST *) ptr;
  char msg[256]; 
  char *removed; 

  sleep(1);
  while( term != 1){
    //sem_wait(&print_sem);
    printf("success #1 for being in while loop for printKeyboard\n"); 

    pthread_mutex_lock(&peer_list_lock);
    int L = ListCount(printingList);
    while(flag2 == 0 || L > 0){
      L = ListCount(printingList);
      sleep(1);
      pthread_cond_wait(&printCondTwo, &peer_list_lock);
    }
    
    while(flag2 == 1){
      int k = ListCount(sendingList);
      if(k>0){
        sleep(1);
        snprintf(msg, sizeof(msg), "%s", ListFirst(sendingList));
        ListRemove(sendingList);
        printf("success #2 for being in while loop for printKeyboard\n"); 
        pthread_mutex_unlock(&peer_list_lock);
        pthread_cond_signal(&printCondOne);
        flag2 = 0; 
        if(sizeof(msg) > 0 ){
          printf("message: %s\n", msg);
        }
      }
    } 
}

  pthread_exit(0);
}


void  *sendDatagram(void *sock)
{
  //LIST *printingList = (LIST *) sock; 
  char msg[256]; 
  char string; 
  char *removed; 
  socklen_t remLen = sizeof(struct sockaddr_in);

  //int sockFD = (int) &sock;
  while(term != 1){

    printf("success for being in while loop for sending to UDP\n"); 
    pthread_mutex_lock(&stdout_lock);

    if(flag1 == 0){
        pthread_cond_wait(&sendCondTwo, &stdout_lock);
    }
    
          int k = ListCount(printingList);
          if(k >0){
              printf("success #2 for being in while loop for sending to UDP\n"); 
              snprintf(msg, sizeof(msg), "%s", ListFirst(printingList));

              printf("success #3 for being in while loop for sending to UDP\n");
              ListRemove(printingList);
              flag1 = 0; 
              pthread_mutex_unlock(&stdout_lock);
              pthread_cond_signal(&sendCondOne);//this signals keyboard input thread to go
              printf("success #3 for being in while loop for sending to UDP\n"); 
              if(sendto(sockfd, msg, 256, 0, servinfo2 -> ai_addr, servinfo2->ai_addrlen) == -1){
                          perror("talker: sendto");
                          exit(1);
                      
              }
          
      
    }

   
}
  pthread_exit(0);
}

int main(int argc, char *argv[]){
      //int sockfd;
      
      int rv;
      int rv2; 
      int numbytes;
      struct sockaddr_storage their_addr;
      struct hostent *host;
     // char buf[MAXBUFLEN];
      socklen_t addr_len;
      char s[INET6_ADDRSTRLEN];
      int local_port;
      int remote_port; 
      
      pthread_attr_t attr;
      char portNumChar[6];
      char portNumChar2[6];
      char *endptr; 
      term = 0; 

    printingList = ListCreate();
    sendingList = ListCreate(); 

    //references for setting mutexs and condition variables: https://linux.die.net/man/3/pthread_cond_init and https://linux.die.net/man/3/pthread_mutex_init
    pthread_cond_init(&printCondOne, NULL);
    pthread_cond_init(&sendCondOne, NULL);
    pthread_cond_init(&printCondTwo, NULL);
    pthread_cond_init(&sendCondTwo, NULL);

    pthread_mutex_init(&stdout_lock, NULL);
    pthread_mutex_init(&peer_list_lock, NULL);

    if (argc < 4 ) {
        fprintf(stderr,"usage: talker hostname message\n");
        exit(1);
    }
    
    printf("success"); 

    //sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    local_port = atoi(argv[1]);
    remote_port = atoi(argv[3]);
    //local_port = strtol(argv[1], &endptr, 0);
    //remote_port = strtol(argv[3], &endptr, 0);
    strcpy(s, argv[2]);

    printf("part 1 done :) \n");

    memset(&server_addr, 0, sizeof(server_addr));
    printf("part 2 done :)");
    server_addr.ai_family= AF_INET;
    server_addr.ai_socktype = SOCK_DGRAM;  
    server_addr.ai_flags = AI_PASSIVE; 
    //get add with local port and null; 
    
    strcpy(portNumChar, argv[1]);
    //sprintf(portNumChar, "%d", local_port);
    //memset(&server_addr, 0, sizeof(server_addr));

    if ((rv = getaddrinfo(NULL, portNumChar, &server_addr, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
   
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("listener: bind");
            close(sockfd);
            continue;

        }
        break;
    }

    freeaddrinfo(servinfo);
    //returns sockfd

    printf("part 3 done :)");

    //thid part takes remote_port and s

    //sprintf(portNumChar2, "%d", remote_port);
    memset(&peer_addr, 0, sizeof(peer_addr));

   //peer_addr.ai_flags = AI_PASSIVE;
    peer_addr.ai_family = AF_INET;
    peer_addr.ai_socktype = SOCK_DGRAM;

    strcpy(portNumChar2, argv[3]);

    if ((rv2 = getaddrinfo(s, portNumChar2, &peer_addr, &servinfo2)) != 0) {
        fprintf(stderr, "getaddrinfo for remote: %s\n", gai_strerror(rv2));
        return 1;
    }
  //This part prints out the IP address of the remote user given their remote machine name for debugging purposes
 

    freeaddrinfo(servinfo2);
  
    
    printf("part 4 done :)");
   

    
    printf("part 5 done :)");
    pthread_create(&buf[0], NULL, waitKeyboard, printingList);
    printf("part 6 done :)");
    pthread_create(&buf[1], NULL , printKeyboard, sendingList);
    printf("part 7 done :)");
    pthread_create(&buf[2], NULL, sendDatagram, sendingList);
    printf("part 8 done :)");
    pthread_create(&buf[3], NULL,  waitUDP, printingList);
    printf("part 9 done :)");
    
    pthread_join(buf[0], NULL);
    pthread_join(buf[1], NULL);
    pthread_join(buf[2], NULL);
    pthread_join(buf[3], NULL);


    pthread_cancel(buf[3]);
    close(sockfd);
    return 0; 


    /*
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET; // set to AF_INET to force IPv4
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE; // use my IP


  return 0; */
}



















