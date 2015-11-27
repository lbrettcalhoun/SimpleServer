/**************************************************************************
* 
* simple_server_tcp
*
* 11/29/2014
* LBC
* A simple C network server (TCP).  
*
* Notes
*  - General program flow:  
*    - Open a socket with "socket"
*    - Bind to the open socket with "bind"
*    - Listen for incoming connections with "listen"
*    - Accept incoming connections with "accept"
*    - Create a separate process for the the connection with "fork"
*    - Use "recv" to read data sent to the bound socket by the client
*    - Use "getnameinfo" to get the hostname and port of the client (just so you 
*      can print this information to the terminal)
*    - Use "send" to echo the data back to the client
*  
***************************************************************************/
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// Define functions
void sigchld_handler (int s) {
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_in_addr (struct sockaddr *socketaddress) {
  if (socketaddress->sa_family == AF_INET) 
    return &(((struct sockaddr_in *)socketaddress)->sin_addr);
  return &(((struct sockaddr_in6 *)socketaddress)->sin6_addr);
}

// Set the size of the buffer used to receive messages from client.
#define BUF_SIZE 512
// Set the size of the backlog ... how many connections we hold in the queue
#define BACKLOG 16

int main(int argc, char *argv[])
{

  // Define the sockaddr_in structure.  This is an IPv4 internet socket address;
  // thus the name sockaddr_in (socket address _ internet)!
  struct sockaddr_in inetsockaddr;

  int sfd, asfd;
  long int port;

  // Define the rest of the stuff needed for the program.  This includes a special structure
  // (sockaddr_storage) which is guaranteed to be big enough to store a socket of any size (we 
  // use this to store the client socket address), length of the client socket address, and number
  // of bytes read from client.  Note the use of ssize_t instead of int for number of bytes read
  // from client.  We use ssize_t (instead of int) to hold either the number of bytes or the value
  // of an error condition from the read call.  Much more flexible than just a plain int!
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len;
  ssize_t nread;

  char buf[BUF_SIZE];

  struct sigaction signalaction;
  char peername[INET6_ADDRSTRLEN];

  if (argc != 2) {
      fprintf(stderr, "Usage: %s port\n", argv[0]);
      exit(EXIT_FAILURE);
  }

  sscanf (argv[1], "%ld", &port);

  // Use "memset" to fill the inetsockaddr struct with a bunch of 0s.  This
  // will "initialize" the empty struct and make sure there is no left
  // over data in the memory address associated with the struct.
  memset(&inetsockaddr, 0, sizeof(struct sockaddr_in));

  // First, populate the inetsockaddr struct members. The address family has to be
  // set to AF_INET.  Use INADDR_ANY (bind to any address ... 0.0.0.0) for the internet
  // address.  And get the port number from the command line.  Note that the port number
  // has to be supplied in "network byte order".  Use htons() to translate a decimal to 
  // "network byte order".
  inetsockaddr.sin_family = AF_INET;
  inetsockaddr.sin_addr.s_addr = INADDR_ANY;
  inetsockaddr.sin_port = htons((uint16_t)port);
 
  // Second, try to open the socket with a call to "socket".  It'll either return an 
  // integer file descriptor or an error.  Here's the parameters:
  //   AF_INET:  Address Family - Internet.  It has to match what we used in inetsockaddr.
  //   SOCK_STREAM:  TCP
  //   0:  Protocol (use 0 to indicate default protocol associated with AF_INET)
  sfd = socket(AF_INET, SOCK_STREAM , 0);
  if (sfd == -1) {
    fprintf(stderr, "Could not create socket\n");
    exit(EXIT_FAILURE);
  }
  
  // Next, if you open the socket, go ahead and try to bind to the socket.
  // The "bind" call returns an integer, 0 means success.
  // A little more info on "bind":  It actually assigns our identified address to the socket.  See "bind"
  // man page for more details.
  if (bind(sfd, (struct sockaddr *) &inetsockaddr, sizeof(struct sockaddr_in)) != 0) {
    close(sfd);
    fprintf(stderr, "Could not bind socket\n");
    exit(EXIT_FAILURE);
  }

  // Now start the listener
  if (listen(sfd, BACKLOG) != 0) {
    close (sfd);
    fprintf(stderr, "Could not listen on socket\n");
    exit(EXIT_FAILURE);
  } 

  // Setup the signal handler
  signalaction.sa_handler = sigchld_handler;
  sigemptyset(&signalaction.sa_mask); 
  signalaction.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &signalaction, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  // Last but not least, the accept loop
  printf ("Waiting to accept connections ...\n");

  while (1) {

    peer_addr_len = sizeof(peer_addr);
    asfd = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    
    if (sfd == -1) {
      fprintf(stderr, "Could not accept socket\n");
      continue;
    }

    inet_ntop(peer_addr.ss_family, get_in_addr((struct sockaddr *)&peer_addr), peername, sizeof(peername));    
    printf("Accepted a connection from %s ...\n", peername);

    if (!fork()) {
      close(sfd);
      if (send(asfd, buf, sizeof(buf), 0) != -1) {
        fprintf(stderr, "Could not send to peer %s ...\n", peername); 
      }
    }

  }
}
