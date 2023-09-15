#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


// additional file inclusions
#include "msg.h"
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <inttypes.h>

// file to store records
#define DB "entry.dat"

void Usage(char *progname);
void PrintOut(int fd, struct sockaddr *addr, size_t addrlen);
void PrintReverseDNS(struct sockaddr *addr, size_t addrlen);
void PrintServerSide(int client_fd, int sock_family);
int  Listen(char *portnum, int *sock_family);
void* HandleClient(void* arg);

// introduced a struct to circumvent arg passing limitations of pthread_create
struct handlerParam{
  struct sockaddr_storage caddr;
  socklen_t caddr_len;
  int client_fd;
};

int main(int argc, char **argv) {
  // Expect the port number as a command line argument.
  if (argc != 2) {
    Usage(argv[0]);
  }

  int sock_family;
  int listen_fd = Listen(argv[1], &sock_family);
  if (listen_fd <= 0) {
    // We failed to bind/listen to a socket.  Quit with failure.
    printf("Couldn't bind to any addresses.\n");
    return EXIT_FAILURE;
  }

  // Loop forever, accepting a connection from a client and doing
  // an echo trick to it.
  while (1) {
    // initialize parameters like you would with client.c
    pthread_t handlerThread;
    struct handlerParam clientParam;
    clientParam.caddr_len = sizeof(clientParam.caddr);
    clientParam.client_fd = accept(listen_fd, (struct sockaddr *)(&clientParam.caddr), &clientParam.caddr_len);
    if (clientParam.client_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
        continue;
      fprintf(stderr, "Failure on accept:%s \n ", strerror(errno));
      break;
    }

    // now create it the thread and handle client request, terminates on user request
    pthread_create(&handlerThread, NULL, HandleClient, &clientParam);
  }

  // Close socket  
  close(listen_fd);
  return EXIT_SUCCESS;
}

// from driver code, shows the correct command line usage for program
void Usage(char *progname) {
  printf("usage: %s port \n", progname);
  exit(EXIT_FAILURE);
}

// from driver code, prints out details
void PrintOut(int fd, struct sockaddr *addr, size_t addrlen) {
  printf("Socket [%d] is bound to: \n", fd);
  if (addr->sa_family == AF_INET) {
    // Print out the IPV4 address and port

    char astring[INET_ADDRSTRLEN];
    struct sockaddr_in *in4 = (struct sockaddr_in *)(addr);
    inet_ntop(AF_INET, &(in4->sin_addr), astring, INET_ADDRSTRLEN);
    printf(" IPv4 address %s", astring);
    printf(" and port %d\n", ntohs(in4->sin_port));

  } else if (addr->sa_family == AF_INET6) {
    // Print out the IPV6 address and port

    char astring[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)(addr);
    inet_ntop(AF_INET6, &(in6->sin6_addr), astring, INET6_ADDRSTRLEN);
    printf("IPv6 address %s", astring);
    printf(" and port %d\n", ntohs(in6->sin6_port));

  } else {
    printf(" ???? address and port ???? \n");
  }
}

// from driver code
void PrintReverseDNS(struct sockaddr *addr, size_t addrlen) {
  char hostname[1024];  // ought to be big enough.
  if (getnameinfo(addr, addrlen, hostname, 1024, NULL, 0, 0) != 0) {
    sprintf(hostname, "[reverse DNS failed]");
  }
  printf("DNS name: %s \n", hostname);
}

// from driver code
void PrintServerSide(int client_fd, int sock_family) {
  char hname[1024];
  hname[0] = '\0';

  printf("Server side interface is ");
  if (sock_family == AF_INET) {
    // The server is using an IPv4 address.
    struct sockaddr_in srvr;
    socklen_t srvrlen = sizeof(srvr);
    char addrbuf[INET_ADDRSTRLEN];
    getsockname(client_fd, (struct sockaddr *) &srvr, &srvrlen);
    inet_ntop(AF_INET, &srvr.sin_addr, addrbuf, INET_ADDRSTRLEN);
    printf("%s", addrbuf);
    // Get the server's dns name, or return it's IP address as
    // a substitute if the dns lookup fails.
    getnameinfo((const struct sockaddr *) &srvr,
                srvrlen, hname, 1024, NULL, 0, 0);
    printf(" [%s]\n", hname);
  } else {
    // The server is using an IPv6 address.
    struct sockaddr_in6 srvr;
    socklen_t srvrlen = sizeof(srvr);
    char addrbuf[INET6_ADDRSTRLEN];
    getsockname(client_fd, (struct sockaddr *) &srvr, &srvrlen);
    inet_ntop(AF_INET6, &srvr.sin6_addr, addrbuf, INET6_ADDRSTRLEN);
    printf("%s", addrbuf);
    // Get the server's dns name, or return it's IP address as
    // a substitute if the dns lookup fails.
    getnameinfo((const struct sockaddr *) &srvr,
                srvrlen, hname, 1024, NULL, 0, 0);
    printf(" [%s]\n", hname);
  }
}

// from driver code, waits for connection from client
int Listen(char *portnum, int *sock_family) {

  // Populate the "hints" addrinfo structure for getaddrinfo().
  // ("man addrinfo")
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;       // IPv6 (also handles IPv4 clients)
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  // Use argv[1] as the string representation of our portnumber to
  // pass in to getaddrinfo().  getaddrinfo() returns a list of
  // address structures via the output parameter "result".
  struct addrinfo *result;
  int res = getaddrinfo(NULL, portnum, &hints, &result);

  // Did addrinfo() fail?
  if (res != 0) {
	printf( "getaddrinfo failed: %s", gai_strerror(res));
    return -1;
  }

  // Loop through the returned address structures until we are able
  // to create a socket and bind to one.  The address structures are
  // linked in a list through the "ai_next" field of result.
  int listen_fd = -1;
  struct addrinfo *rp;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    listen_fd = socket(rp->ai_family,
                       rp->ai_socktype,
                       rp->ai_protocol);
    if (listen_fd == -1) {
      // Creating this socket failed.  So, loop to the next returned
      // result and try again.
      fprintf(stderr, "socket() failed:%s \n ", strerror(errno));
      listen_fd = -1;
      continue;
    }

    // Configure the socket; we're setting a socket "option."  In
    // particular, we set "SO_REUSEADDR", which tells the TCP stack
    // so make the port we bind to available again as soon as we
    // exit, rather than waiting for a few tens of seconds to recycle it.
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    // Try binding the socket to the address and port number returned
    // by getaddrinfo().
    if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      // Bind worked!  Print out the information about what
      // we bound to.
      PrintOut(listen_fd, rp->ai_addr, rp->ai_addrlen);

      // Return to the caller the address family.
      *sock_family = rp->ai_family;
      break;
    }

    // The bind failed.  Close the socket, then loop back around and
    // try the next address/port returned by getaddrinfo().
    close(listen_fd);
    listen_fd = -1;
  }

  // Free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // If we failed to bind, return failure.
  if (listen_fd == -1)
    return listen_fd;

  // Success. Tell the OS that we want this to be a listening socket.
  if (listen(listen_fd, SOMAXCONN) != 0) {
    fprintf(stderr, "Failed to mark socket as listening:%s \n ", strerror(errno));
    close(listen_fd);
    return -1;
  }

  // Return to the client the listening file descriptor.
  return listen_fd;
}

// determines what to do with client request
void* HandleClient(void* arg) {
  // recast arg into clientParam
  struct handlerParam* clientParam = (struct handlerParam*) arg;
  int c_fd = clientParam->client_fd;
 
  // Print out information about the client.
  printf("\nNew client connection \n" );
  // Reads data and echoes it back, until the client terminates connection.
  while (1) {
    // read from client
    char clientbuf[1024]; 
    ssize_t res = read(c_fd, clientbuf, 1023);
    clientbuf[res] = '\0';    // make sure data is correctly null terminated

    // 0 byte read == connection terminated
    if (res == 0) {
      printf("[The client disconnected.] \n");
      break;
    }

    // error with reading byte from client
    if (res == -1) {
      if ((errno == EAGAIN) || (errno == EINTR)){
        continue;
      }
      else{
        fprintf(stderr, "Error on client socket:%s \n ", strerror(errno));
     	  break;
      }
    }

    //recast read bytes into the form of msg struct
    struct msg* message = (struct msg*) clientbuf;
    struct msg response;    // response for client

    // indicates what the client requested
    printf("The client sent: %d \n", message->type);

    // if client asks to store data into server,
    if(message->type == PUT){
      // create/open the data file
    	FILE* file = fopen(DB, "a");

      // File open error
	    if(file == NULL){
		    printf("File not found");  
		    response.type = FAIL;
		    break;
	    }

      // write the given record into entry.dat and close the file
      fwrite(&(message->rd), sizeof(struct record), 1, file);
	    fclose(file);

      // tells client record is successfully stored into file
	    response.type = SUCCESS;
    }

    // if client asks to retrieve data from server,
    else if(message->type == GET){
      struct record record;    // to store data from file

      // open record file
    	FILE *file = fopen(DB, "r");

      // file opening error
	    if(file == NULL){
		    printf("File not found");
        response.type = FAIL;
        break;
	    }

      response.type = FAIL;  // assumes that record cannot be found
      // while data is still avail., keep reading
	    while(fread(&record, sizeof(struct record), 1, file) == 1){
        // check if stored record's id matches with the id that client requested
        // if it matches,
  	    if(record.id == message->rd.id){ 
          // tells client record is successfully found and append the record details to the response
  		    response.type = SUCCESS;
  		    response.rd = record; 
  		    break;
  	    }
      }

      // close the data file
	    fclose(file);
    }

    // return the response to client
    write(c_fd, &response, sizeof(response));
  }

  // connection terminated
  close(c_fd);
  return NULL;
}
