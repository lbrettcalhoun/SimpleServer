/**************************************************************************
* 
* command_server_tcp
*
* 12/29/2014
* LBC
* A simple C network server (TCP) that illustrates fork and client commands.  
* Use netcat for the client.
*
* Notes
*  - General program flow:  
*    - Open a socket with "socket"
*    - Bind to the open socket with "bind"
*    - Listen for incoming connections with "listen"
*    - Accept incoming connections with "accept"
*    - Use "inet_ntop" to get the hostname of the client (just so you 
*      can print this information to the terminal)
*    - Create a separate process for the connection with "fork"
*    - Use "recv" to read command sent by the client
*    - Parse the command sent by the client 
*    - Use "send" to send the data to the client
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

void *get_in_addr (struct sockaddr *socketaddress) { // A helper function to select IPv4 versus IPv6 socket address
  if (socketaddress->sa_family == AF_INET) 
    return &(((struct sockaddr_in *)socketaddress)->sin_addr);
  return &(((struct sockaddr_in6 *)socketaddress)->sin6_addr);
}

// Set the size of the buffer used to receive commands from client and send data to client.
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
  // of bytes read/sent from client.  Note the use of ssize_t instead of int for number of bytes read
  // or sent.  We use ssize_t (instead of int) to hold either the number of bytes or the value
  // of an error condition from the read call.  Much more flexible than just a plain int!
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len;
  ssize_t nread, nsend;

  // Buffers for receiving and sending data from/to client
  char readbuf[BUF_SIZE];
  char sendbuf[BUF_SIZE];

  // SIGACTION structure
  struct sigaction signalaction;

  // Variable to hold the client hostname
  char peername[INET6_ADDRSTRLEN];

  // Variable to hold the commands from the client
  char ccommand;

  // Chid process ID
  int cpid;

  // Process the argument
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
    close (sfd);
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  // Last but not least, the accept loop
  printf ("Waiting to accept connections ...\n");

  while (1) {

    // Accept the client connection
    peer_addr_len = sizeof(peer_addr);
    asfd = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_len);
    
    // Just keep the server running and issue an error message if you cannot accept the client connection
    if (asfd == -1) {
      fprintf(stderr, "Could not accept socket\n");
    }

    // Fork the child process. This time, exit with failure if you cannot fork the process.  The child
    // process will have a duplicate set of file descriptors so you can use the asfd just as you were in the parent process! 
    cpid = fork();
    if (cpid == -1) {
      close (asfd);
      close (sfd);
      fprintf (stderr, "Could not fork process\n");
      exit (EXIT_FAILURE);
    }
  
    // Execute the following block of code in the child process ... cpid will equal 0 in the child process
    if (cpid == 0) {

      // Use your get_in_addr function and inet_ntop to get the name of the client and print to terminal 
      inet_ntop(peer_addr.ss_family, get_in_addr((struct sockaddr *)&peer_addr), peername, sizeof(peername));    
      printf("Accepted a connection from %s ...\n", peername);
  
      // Send a header and prompt to the client.  Just continue on failure.
      strncpy (sendbuf, "Welcome to Command Server V1.0\n\ncommand:  ", sizeof(sendbuf)); 
      if (send (asfd, sendbuf, sizeof(sendbuf), 0) == -1) {
        fprintf (stderr, "Could not send header to client\n");
      }
  
      // While loop for reading commands from client
      while (1) {

        // Read the commands from the client.  The recv function will sit and wait for a command to arrive.
        // Just continue on failure.
        nread = recv (asfd, readbuf, sizeof(sendbuf), 0); 
        if (nread == -1) {
          fprintf (stderr, "Could not read from client\n");
        }
        else {
          printf ("Received %d bytes from client\n", nread);
        }
    
        // Process the client command. You guessed it ... continue on failure.
        ccommand = readbuf[0];
        switch (ccommand) {
          case 'H':
            strncpy (sendbuf, "Command Server Help.\n\nH(elp):  This help.\nC(ommand):  Print command.\nQ(uit):  Quit.\n\ncommand:  ", sizeof(sendbuf));
            nsend = send (asfd, sendbuf, strlen(sendbuf), 0);
            if (nsend == -1) {
              fprintf (stderr, "Could not send data to client\n");
            } 
            else { 
              printf ("Sent %d bytes to client\n", nsend);
            }
            break;
          case 'C':
            strncpy (sendbuf, "command\n\ncommand:  ", sizeof(sendbuf));
            nsend = send (asfd, sendbuf, strlen(sendbuf), 0);
            if (nsend == -1) {
              fprintf (stderr, "Could not send data to client\n");
            } 
            else {
              printf ("Sent %d bytes to client\n", nsend);
            }
            break;
          case 'Q':
            strncpy (sendbuf, "Goodbye.\n\n", sizeof(sendbuf));
            nsend = send (asfd, sendbuf, strlen(sendbuf), 0);
            if (nsend == -1) {
              fprintf (stderr, "Could not send data to client\n");
            } 
            else {
              printf ("Sent %d bytes to client\n", nsend);
            }
            _exit (EXIT_SUCCESS); // Exit the child process
          default:
            strncpy (sendbuf, "Invalid command.\n\ncommand:  ", sizeof(sendbuf));
            nsend = send (asfd, sendbuf, strlen(sendbuf), 0);
            if (nread == -1) {
              fprintf (stderr, "Could not send data to client\n");
            } 
            else {
              printf ("Sent %d bytes to client\n", nsend);
            }
            break;
    
        } // End switch

      } // End client command while

    } // End cpid if

  } // End accept while

  exit (EXIT_SUCCESS);

} // End main
