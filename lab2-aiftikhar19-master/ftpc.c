/*
  Filename: ftpc.c
  Created by: Aisha Iftikhar
  Creation date: 1/16/20
  Synopsis: This project is a client side code for a file transfer protocol. 
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#define MAX 100

int main(int argc, char *argv[]) {

	/*Variable declarations*/
	int sock, size, return_code, return_size, n, bytes_read;	/*vars used to store socket, file size, return codes (from read and write), and bytes_read from fread*/
	struct sockaddr_in serv_addr;		/* structure for socket name setup */
	struct hostent *server;			/*used to store host address*/
	FILE *file;				/*used for file to transfer*/
	char buff[MAX];				/*buffer to store file contents*/
	char blank[1];				/*used to store blank for syntax*/
	blank[0] = ' ';
	
	/*variables to store input*/
	char *server_ip = argv[1];
	int server_port = atoi(argv[2]);
	char *file_to_transfer = argv[3];
	
	/*Error check input*/
	if (argc != 4) {
		printf("ERROR: Incorrect number of arguments.\n");
		printf("Use the format: ftpc <remote-IP> <remote-port> <local-file-to-transfer> \n");
		exit(0);
	}
	
	/*Initialize socket connection*/
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("ERROR: Cannot open datagram socket");
		exit(0);
	}
	
	/*print status*/
	printf("Initializing connection...\n");
	
	/*Connect to host*/
	/*get host - takes IP address and returns pointer to hostent containing info about host*/
	server = gethostbyname(server_ip);	
	if(server == NULL) {
		printf("%s: unknown host\n", argv[1]);
		exit(0);
	}
	
	/*construct name of socket*/
	/*bzero() sets all values in a buffer to zero*/
	bzero((char *) &serv_addr, sizeof(serv_addr));
	/*variable serv_addr is a structure of type struct sockaddr_in. This structure has four fields. The first field is short sin_family, which contains a code for the address family. It should always be set to the symbolic constant AF_INET*/
	serv_addr.sin_family = AF_INET;
  	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  	/*second field of serv_addr is unsigned short sin_port , which contain the port number. However, instead of simply copying the port number to this field, it is necessary to convert this to network byte order using the function htons() which converts a port number in host byte order to a port number in network byte order*/
  	serv_addr.sin_port = htons(server_port);
  
  	/*connect to server*/
  	/*connect takes three arguments: the socket file descriptor, the address of the host to which it wants to connect (including the port number), and the size of this address. This function returns 0 on success and -1 if it fails*/
  	if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    		close(sock);
    		perror("ERROR: Connection failed");
    		exit(0);
  	}
  	
  	/*print status*/
  	printf("Connected.\n");
	
	/*Error check file open */
	if ((file = fopen(file_to_transfer, "rb")) == NULL) {
		printf ("ERROR: Cannot open the input file %s\n", file_to_transfer);
		exit(0);
	}
	
	/*Get file size*/
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	/*send size of the file in bytes; convert from host to network long*/
	size = htonl(size);
	n = write(sock, &size, 4);
	if (n != 4) {
		printf("ERROR: wrong number bytes sent\n");
		exit(1);
	}
	
	/*print status*/
	printf("File Size Sent: %d\n", htonl(size));
	
	/*add single ascii space*/
	n = write(sock, &blank, 1);
	if (n != 1) {
		printf("ERROR: wrong number bytes sent\n");
		exit(1);
	}
	
	/*send name of file in bytes*/
	n = write(sock, file_to_transfer, 20);
	if (n != 20) {
		printf("ERROR: wrong number bytes sent\n");
		exit(1);
	}
	
	/*print status*/
	printf("File Name Sent: %s\n", file_to_transfer);
	
	/*add single ascii space*/
	n = write(sock, &blank, 1);
	if (n != 1) {
		printf("ERROR: wrong number bytes sent\n");
		exit(1);
	}
	
	/*begin reading data*/
	while (1) {
		/*fread returns total number of elements successfully read as a size_t object. If this number differs from the nmemb parameter, then either an error had occurred or the End Of File was reached*/
		bytes_read = fread(buff, 1, MAX, file);
		if(bytes_read > 0) {
			/*while there is still data, send to stream*/
			write(sock, buff, bytes_read); 
		}
		/*as you're reading data, check for end of file*/
		if (feof(file)) {
			break;
		}
	}
	/*print status*/
	printf("File Sent.\n");
	
	/*read return code and returned file size*/
     	n = read(sock, &return_code, 4);
    	if (n != 4) {
		printf("ERROR: wrong number bytes read\n");
		exit(1);
	}
     	n = read(sock, &blank, 1);
     	if (n != 1) {
		printf("ERROR: wrong number bytes read\n");
		exit(1);
	}
     	n = read(sock, &return_size, 4);
     	if (n != 4) {
		printf("ERROR: wrong number bytes read\n");
		exit(1);
	}
     
     	/*convert return codes from network to host long*/
     	return_code = ntohl(return_code);
     	return_size = ntohl(return_size);
     
     	/*print status*/
     	printf("Returned code: %d\n", return_code);
     	printf("Returned file size: %d\n", return_size);
	
	/*close file streams*/
	fclose (file);
	close(sock);
	
	/*print status*/
	printf("Transfer complete. Socket closed.\n");

return 0;
}
