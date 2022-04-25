/*
  Filename: tictactoeClient.c
  Created by: Aisha Iftikhar
  Creation date: 2/11/20
  Synopsis: This project is a client side code for a version of tictactoe played on different machines. 
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

/*Global variables*/
#define ROWS  3
#define COLUMNS  3
#define sendrecvflag 0 

/*Function declarations*/
int tictactoe(char board[ROWS][COLUMNS], int sock, struct sockaddr_in serv_addr);
void error_check_response(char board[ROWS][COLUMNS], char msg[5], int player, int i, int sock, struct sockaddr_in serv_addr);
void error_check_connection(char board[ROWS][COLUMNS], char msg[5], int player, int i);
int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS]);
int initSharedState(char board[ROWS][COLUMNS]);

int main(int argc, char *argv[])
{
  /*Error check input*/
  if (argc != 3) {
	printf("ERROR: Incorrect number of arguments.\n");
	printf("Use the format: tictactoeClient <port_number> <ip addr> \n");
	exit(1);
  }
  
  /*variable declarations*/
  char board[ROWS][COLUMNS]; 
  int server_port = atoi(argv[1]);
  char *server_ip = argv[2];
  int sock, rc;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  
  /*Initialize socket connection*/
  if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
  	perror("ERROR: Cannot open datagram socket");
	exit(0); 
  }
	
  /*print status*/
  printf("Initializing connection...\n");
	
  /*Connect to host*/
  server = gethostbyname(server_ip);	
  if(server == NULL) {
	printf("%s: unknown host\n", server_ip);
	exit(0);
  }
  
  /*construct name of socket*/
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(server_port);
  	
  /*print status*/
  printf("Connected.\n");
  
  // check return codes
  rc = initSharedState(board); 
  if (rc != 0) {
  	printf("Something went wrong. Exiting...\n");
     exit(1);
  }
  rc = tictactoe(board, sock, serv_addr);
  if (rc != 0) {
  	printf("Something went wrong. Exiting...\n");
     exit(1);
  }
  return 0; 
}

int tictactoe(char board[ROWS][COLUMNS], int sock, struct sockaddr_in serv_addr)
{
  char msg[5] = {1, 1, 0, 0, 0};
  int i = 0, n, choice, row, column, player = 1; 
  char mark;
  socklen_t addrlen = sizeof(serv_addr);
  
  /*send game request to server*/
  n = sendto(sock, msg, 5, sendrecvflag, (struct sockaddr*)&serv_addr, addrlen);
  printf("Game Request Sent.\n");
  if (n != 5) {
    printf("ERROR: wrong number bytes read\n");
    exit(1);
  }
  
  /*response from server*/
  n = recvfrom(sock, msg, 5, sendrecvflag, (struct sockaddr*)&serv_addr, (socklen_t *)&addrlen);
  /*check if game is accepted*/
  error_check_connection(board, msg, player, i);
  error_check_response(board, msg, player, i, sock, serv_addr);

  do{
    /*Mod math to figure out who the player is*/
    player = (player % 2) ? 1 : 2;
    
    /*get player choice*/
    if(player == 1) {
      if((msg[4]+0) > 0) {
        printf("Awaiting Player 1...\n");
        n = recvfrom(sock, msg, 5, sendrecvflag, (struct sockaddr*)&serv_addr, (socklen_t *)&addrlen);
      }
      choice = msg[3]+0;
    } else if(player == 2) {
      printf("Player 1 chose: %d\n", msg[3]+0);
      printf("Player %d, enter a number:  ", player); // print out player so you can pass game
      scanf("%d", &choice); //using scanf to get the choice
      while(choice<0 && choice>10) {
        printf("Invalid move. Try again: ");
        choice = getchar();
		if(!(isdigit(choice))) {
			scanf("%d", &choice);
		}
	 }
    } else {
      printf("Something went wrong. Exiting...\n");
      exit(1);
    }
       
    error_check_response(board, msg, player, i, sock, serv_addr);	// use to check for game over
    
    /*set mark to X for player 1, O for player 2*/
    mark = (player == 1) ? 'X' : 'O';
    /*math to figure out what move*/
    row = (int)((choice-1) / ROWS); 
    column = (choice-1) % COLUMNS;
    if (board[row][column] == (choice+'0')) {
      board[row][column] = mark;
    } else {
      while(board[row][column] != (choice+'0') || ((choice+'0')<0 && (choice+'0')>10)) {
        printf("Invalid move. Try again: ");
	   choice = getchar();
		if(!(isdigit(choice))) {
			scanf("%d", &choice);
		}
	   row = (int)((choice-1) / ROWS); 
        column = (choice-1) % COLUMNS;
      }
      board[row][column] = mark;
    }
    
    i = checkwin(board);    
    
    if(player == 2) {
      if (i != -1) {
        msg[2] = 4;
      }
      msg[3] = choice;
      msg[4]++;
      n = sendto(sock, msg, 5, sendrecvflag, (struct sockaddr*)&serv_addr, addrlen);
      if (n != 5) {
        printf("ERROR: wrong number bytes read\n");
	   exit(1);
      }
    }
    
    system("clear");
    /*debug statements
    printf("Version: %d\n", msg[0]+0);
    printf("Connection: %d\n", msg[1]+0);
    printf("Response: %d\n", msg[2]+0);
    printf("Move: %d\n", msg[3]+0);
    printf("Turn: %d\n", msg[4]+0);*/
    
    error_check_response(board, msg, player, i, sock, serv_addr);	// use to check for game over
    
    print_board(board); // call function to print the board on the screen
    
    player++;  
    
  } while (i ==  - 1); // -1 means no one won

  return 0;
}

void error_check_response(char board[ROWS][COLUMNS], char msg[5], int player, int i, int sock, struct sockaddr_in serv_addr){
  int n;
  switch(msg[2]+0) {
  	case 1:
  		printf("ERROR: Invalid Move. The requested move can't be performed.\n");
  		exit(1);
  	case 2:
  		printf("ERROR: Game out of sync; invalid turn number.\n");
  		exit(1);
  	case 3:
  		printf("ERROR: Invalid Request\n");
  		exit(1);
  	case 4:
  		if (i == 1) {
  		     system("clear");
  			print_board(board);
  			printf("Game Over\n");
      		printf("==>\aPlayer %d wins\n ", player);
      		if(player == 1) {
      			msg[2] = 5;
      			n = sendto(sock, msg, 5, sendrecvflag, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
      			if (n != 5) {
        				printf("ERROR: wrong number bytes read\n");
	   				exit(1);
      			}
      		}
      	} else if (i == 0) {
      	     system("clear");
  			print_board(board);
      		printf("Game Over\n");
      		printf("==>\aGame draw\n ");
      		if(player == 1) {
      			msg[2] = 5;
      			n = sendto(sock, msg, 5, sendrecvflag, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
      			if (n != 5) {
        				printf("ERROR: wrong number bytes read\n");
	   				exit(1);
      			}
      		}
      	} else {
      		return;
      	}
      	exit(0);
  	case 5:
  		printf("Game Over Acknowledged\n");
  		exit(1);
  	case 6:
  		printf("ERROR: Incompatible Version Number\n");
  		exit(1);
  	default:
  		return;
  }
}

void error_check_connection(char board[ROWS][COLUMNS], char msg[5], int player, int i){
  switch(msg[1]+0) {
  	case 0:
  		printf("Game accepted.\n");
  		return;
  	case 1:
  		printf("ERROR: New game request...This is a client.\n");
  		exit(1);
  	case 2:
  		printf("Game refused. Exiting...\n");
  		exit(1);
  	default:
  		return;
  }
}

int checkwin(char board[ROWS][COLUMNS])
{
  /************************************************************************/
  /* brute force check to see if someone won, or if there is a draw       */
  /* return a 0 if the game is 'over' and return -1 if game should go on  */
  /************************************************************************/
  if (board[0][0] == board[0][1] && board[0][1] == board[0][2] ) // row matches
    return 1;
        
  else if (board[1][0] == board[1][1] && board[1][1] == board[1][2] ) // row matches
    return 1;
        
  else if (board[2][0] == board[2][1] && board[2][1] == board[2][2] ) // row matches
    return 1;
        
  else if (board[0][0] == board[1][0] && board[1][0] == board[2][0] ) // column
    return 1;
        
  else if (board[0][1] == board[1][1] && board[1][1] == board[2][1] ) // column
    return 1;
        
  else if (board[0][2] == board[1][2] && board[1][2] == board[2][2] ) // column
    return 1;
        
  else if (board[0][0] == board[1][1] && board[1][1] == board[2][2] ) // diagonal
    return 1;
        
  else if (board[2][0] == board[1][1] && board[1][1] == board[0][2] ) // diagonal
    return 1;
        
  else if (board[0][0] != '1' && board[0][1] != '2' && board[0][2] != '3' &&
	   board[1][0] != '4' && board[1][1] != '5' && board[1][2] != '6' && 
	   board[2][0] != '7' && board[2][1] != '8' && board[2][2] != '9')

    return 0; // Return of 0 means game over
  else
    return  - 1; // return of -1 means keep playing
}

void print_board(char board[ROWS][COLUMNS])
{
  /*****************************************************************/
  /* brute force print out the board and all the squares/values    */
  /*****************************************************************/

  printf("\n\tCurrent TicTacToe Game\n\n");

  printf("Player 1 (X)  -  Player 2 (O)\n\n");


  printf("     |     |     \n");
  printf("  %c  |  %c  |  %c \n", board[0][0], board[0][1], board[0][2]);

  printf("_____|_____|_____\n");
  printf("     |     |     \n");

  printf("  %c  |  %c  |  %c \n", board[1][0], board[1][1], board[1][2]);

  printf("_____|_____|_____\n");
  printf("     |     |     \n");

  printf("  %c  |  %c  |  %c \n", board[2][0], board[2][1], board[2][2]);

  printf("     |     |     \n\n");
}



int initSharedState(char board[ROWS][COLUMNS]){    
  /* this just initializing the shared state aka the board */
  int i, j, count = 1;
  //printf ("in sharedstate area\n");
  for (i=0;i<3;i++)
    for (j=0;j<3;j++){
      board[i][j] = count + '0';
      count++;
    }
  return 0;
}
