/**************************************************************************
* 
* simple_server
*
* 11/26/2014
* LBC
* A simple C network server.  Copied from the getaddrinfo man page.
* This server listens on a UDP socket and receives data from clients.
* The server then echos the data back to the client.
*
* Notes
*  - General program flow:  
*    - Use "getaddrinfo" to identify an internet socket for use with the program
*    - Open the identified socket with "socket"
*    - Bind to the open socket with "bind"
*    - Use "recvfrom" to read data sent to the bound socket by the client
*    - Use "getnameinfo" to get the hostname and port of the client (just so you 
*      can print this information to the terminal)
*    - Use "sendto" to echo the data back to the client
*  - The function "getaddrinfo" returns a linked list which can contain more than 1 inet 
*    socket (this can happen depending on the type of flags you pass via the
*    hints structure).  So you have to iterate through the linked list (which
*    probably only contains 1 but could contain more).
*  - The structure "addrinfo" is a key component in this program.  A total of X
*    different addrinfos are used with "getaddrinfo" to identify an internet socket.
*    See the code for details.
*  - Here is what an addrinfo structure looks like:
*         struct addrinfo {
*               int              ai_flags;  	This is where you set certain flags.
*               int              ai_family;	Address family (usually AF_INET).
*               int              ai_socktype;	Stream (TCP) and Datagram (UDP).
*               int              ai_protocol;	Protocol.
*               socklen_t        ai_addrlen;	Length of the addrinfo (will be different 
*						for IP4 versus IP6).
*               struct sockaddr *ai_addr;	The socket address.
*               char            *ai_canonname;	Hostname.
*               struct addrinfo *ai_next;	Pointer to next item in linked list.  Use
*						this to iterate over all the addrinfos in
*						the list.
*           };
*      
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

  // Define the addrinfo structures.  You'll need 3 of them:  hints contains
  // the parameters you specify to identify the type of socket you want to open. 
  // result is the structure that contains the addrinfo(s) identified by "getaddrinfo".
  // Remember, result will probably just contain 1 addrinfo, but it could contain more; 
  // so it is actually a linked list of addrinfo(s)!
  // Finally, rp is used to iterate over the linked list to find the identified socket. 
  // Again, there is probably just 1 item in the linked list, but you have to iterate 
  // due to the possibility of more than 1 item. 
  struct addrinfo hints;
  struct addrinfo *result, *rp;

  int sfd, s;

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

  // Use "memset" to fill the hints struct with a bunch of 0s.  This
  // will "initialize" the empty struct and make sure there is no left
  // over data in the memory address associated with the struct.  In other
  // words, zero out this struct.
  memset(&hints, 0, sizeof(struct addrinfo));
 
  // Now setup the type of socket you want to identify.
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM; /* Datagram socket (aka UDP)*/
  hints.ai_flags = AI_PASSIVE;    /* Listen on all NICs (even localhost)*/
  hints.ai_protocol = 0;          /* Any protocol */
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  // Pass the hints structure to "getaddrinfo" and let it identify 1 (or more)
  // sockets.  Arguments are as follows:  
  //   host = NULL (identify a socket on the localhost)
  //   service = port number of our socket (from the command line)
  //   hints = Structure we defined above ... contains the type of socket we want to identify
  //   result = Contains the linked list of addrinfo structs with 1 or more identified sockets
  // The int s contains the return from "getaddrinfo".  Either a 0 (success) or an error.
  s = getaddrinfo(NULL, argv[1], &hints, &result);
  if (s != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
      exit(EXIT_FAILURE);
  }

  // Now that you've got a linked list of potential sockets, you need to iterate through the
  // list, open a socket with "socket" and bind to the socket with "bind".  The list probably
  // only contains 1 item, but can contain more than 1 item.  See above discussion in Notes.

  /* getaddrinfo() returns a list of address structures.
     Try each address until we successfully bind(2).
     If socket(2) (or bind(2)) fails, we (close the socket
     and) try the next address. */

  // Set rp to the result.  Remember, result contains a linked list of addrinfo structures.
  // Then iterate through all the items in the rp linked list.  I don't know why they do it
  // this way; maybe to preserve the original linked list in result?
  for (rp = result; rp != NULL; rp = rp->ai_next) {

      // First, try to open the socket with a call to "socket".  It'll either return an 
      // integer file descriptor or an error.  The parameters for "socket" come directly 
      // from the structure that is the current item.
      sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sfd == -1)
          continue; // If you get an error when opening the socket, just go to the next item.
  
      // Next, if you open the socket, go ahead and try to bind to the socket.  Again the
      // parameters for "bind" come directly from the structure that is the current item. The
      // "bind" call returns an integer, 0 means success.
      // A little more info on "bind":  It actually assigns our identified address to the socket.  See "bind"
      // man page for more details.
      if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
          break;                  /* Success */

      // If you get this far in the for loop, the "bind" call failed.  So go ahead and close out the socket
      // (opened with the above call to "socket") before you iterate and try again.  Remember, if "bind"
      // completes successfully, you won't hit the section of the loop (see above break).
      close(sfd);
  }

  // Check to make sure the linked list is not empty.  If the linked list is empty, then "getaddrinfo"
  // failed to identify an address for us.
  if (rp == NULL) {               /* No address succeeded */
      fprintf(stderr, "Could not bind\n");
      exit(EXIT_FAILURE);
  }

  // Now free the memory associated with our addrinfo struct result.  Don't worry, this doesn't do anything to
  // our bound socket.  We've used result to open and bind the socket, so we don't need it anymore.  Also, since
  // result is a pointer and rp points to result, we simoultaneously free the memory for rp (I think).
  freeaddrinfo(result);           /* No longer needed */

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
      //  peer_addr = the address of our client ... note this is case as a sockaddr 
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
