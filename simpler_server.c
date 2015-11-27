/**************************************************************************
* 
* simpler_server
*
* 11/26/2014
* LBC
* A simpler C network server.  A simplification of simple_server.
* The simplification includes removal off "getaddrinfo" code.
*
* Notes
*  - General program flow:  
*    - Open a socket with "socket"
*    - Bind to the open socket with "bind"
*    - Use "recvfrom" to read data sent to the bound socket by the client
*    - Use "getnameinfo" to get the hostname and port of the client (just so you 
*      can print this information to the terminal)
*    - Use "sendto" to echo the data back to the client
*  
***************************************************************************/
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

// Set the size of the buffer used to receive messages from client.
#define BUF_SIZE 512

int main(int argc, char *argv[])
{

  // Define the sockaddr_in structure.  This is an IPv4 internet socket address;
  // thus the name sockaddr_in (socket address _ internet)!
  struct sockaddr_in inetsockaddr;

  int sfd, s;
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
  //   SOCK_DGRAM:  UDP
  //   0:  Protocol (use 0 to indicate default protocol associated with AF_INET)
  sfd = socket(AF_INET, SOCK_DGRAM , 0);
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

  /* Read datagrams and echo them back to sender */

  for (;;) {
 
      // Get the length of the client socket.
      peer_addr_len = sizeof(struct sockaddr_storage);

      // Now read the message sent from the client with the "recvfrom" call.
      // Here's the arguments to "recvfrom":
      //  sfd = file descriptor for our bound socket
      //  buf = our buffer ... this is where we put the message
      //  BUF_SIZE = the size of our buffer
      //  0 = this is where you put the flags for recvfrom ... none in our case
      //  peer_addr = the address of our client ... note this is cast as a sockaddr 
      //  structure
      //  peer_addr_len = the size of the client address (will be different IP4 vs IP6)
      nread = recvfrom(sfd, buf, BUF_SIZE, 0, (struct sockaddr *) &peer_addr, &peer_addr_len);
      if (nread == -1)
          continue;               /* Ignore failed request */

      // Define a couple of char arrays to hold the names of the client and it's port.
      // We'll use these below with "getnameinfo".  NI_MAXHOST and NI_MAXSERV are platform
      // dependent constants for the maximum allowable length of a hostname and port.
      char host[NI_MAXHOST], service[NI_MAXSERV];

      // Now use these arrays along with "getnameinfo" to get the client's hostname and port
      // number.  This isn't really necessary but we use this information to print a record
      // of each connection to the terminal.  It could also be used to print to syslog or some
      // other type of log.  The call to "getnameinfo" will return an integer (0 for success).
      // I'm not going to list the parameters for "getnameinfo".  If you want to see them, use man.      
      s = getnameinfo((struct sockaddr *) &peer_addr, peer_addr_len, host, NI_MAXHOST,
                      service, NI_MAXSERV, NI_NUMERICSERV);
     if (s == 0)
          printf("Received %ld bytes from %s:%s\n",
                  (long) nread, host, service);
      else
          fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

      // Now send the same message back to the client.  Use "sendto" to send the message.
      // Parameters for "sendto" are:
      //   sfd = the file descriptor of our bound socket
      //   buf = our buffer (which contains the message from the client)
      //   nread = the number of bytes read from the client
      //   0 = flags ... we aren't using any flags
      //   peer_addr = the socket address of the client ... note it must be cast as sockaddr
      //   peer_addr_len = the length of the client's socket address
      if (sendto(sfd, buf, nread, 0, (struct sockaddr *) &peer_addr, peer_addr_len) != nread)
          fprintf(stderr, "Error sending response\n");
  }
}
