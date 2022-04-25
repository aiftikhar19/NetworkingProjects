/*
  Filename: tictactoeClient.c
  Created by: Aisha Iftikhar
  Creation date: 1/29/20
  Synopsis: This project is a client side code for a version of tictactoe played on different machines. 
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
#include <ctype.h>

/*Global variables*/
#define ROWS  3
#define COLUMNS  3

/*Function declarations*/
void error_check(char board[ROWS][COLUMNS], char msg[4], int player, int i);
int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS]);
int tictactoe(char board[ROWS][COLUMNS], int sock);
int initSharedState(char board[ROWS][COLUMNS]);

int main(int argc, char *argv[])
{
/*Error check input*/
  if (argc != 3) {
	printf("ERROR: Incorrect number of arguments.\n");
	printf("Use the format: tictactoeClient <port_number> <ip addr> \n");
	exit(0);
  }
  /*variable declarations*/
  char board[ROWS][COLUMNS]; 
  int server_port = atoi(argv[1]);
  char *server_ip = argv[2];
  int sock, rc;
  struct sockaddr_in serv_addr;		/*structure for socket name setup */
  struct hostent *server;			/*used to store host address*/
  
  /*Initialize socket connection*/
  if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
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
  
  /*connect to server*/
  if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
  	close(sock);
  	perror("ERROR: Connection failed");
  	exit(0);
  }
  	
  /*print status*/
  printf("Connected.\n");
  
  // check return codes
  rc = initSharedState(board); 
  if (rc != 0) {
  	printf("ERROR\n");
  }
  rc = tictactoe(board, sock);
  if (rc != 0) {
  	printf("ERROR\n");
  }
  return 0; 
}

void error_check(char board[ROWS][COLUMNS], char msg[4], int player, int i){
  switch((int)msg[1]) {
  	case 1:
  		printf("Invalid Move\n");
  		exit(1);
  	case 2:
  		printf("ERROR: Game out of sync\n");
  		exit(1);
  	case 3:
  		printf("ERROR: Invalid Request\n");
  		exit(1);
  	case 4:
  		if (i == 1) {
  			if(player == 1) {
  				print_board(board);
  			}
  			printf("Game Over\n");
      		printf("==>\aPlayer %d wins\n ", player);
      	} else if (i == 0) {
      		if(player == 1) {
  				print_board(board);
  			}
      		printf("Game Over\n");
      		printf("==>\aGame draw\n ");
      	}
      	exit(0);
  	case 5:
  		printf("Game Over Acknowledge\n");
  		exit(1);
  	case 6:
  		printf("ERROR: Incompatible Version Number\n");
  		exit(1);
  	default:
  		return;
  }
}

int tictactoe(char board[ROWS][COLUMNS], int sock)
{
  char msg[4] = {'0', '0', '0', '0'};
  int i, choice;  // used for keeping track of choice user makes
  int player = 1;
  int row, column;
  char mark;      // either an 'x' or an 'o'
  print_board(board);
  
  do{
    player = (player % 2) ? 1 : 2;  // Mod math to figure out who the player is
    printf("Awaiting player 1...\n");
    // read message from player 1 (server)
    int n = read(sock, msg, 4);
    // error check input message
    if (n != 4) {
      printf("ERROR: wrong number bytes read\n");
	 exit(1);
    }
        
    choice = (int)(msg[2]);
    printf("Player 1 chose: %d\n", choice);
    // set mark to X for player 1, O for player 2
    mark = (player == 1) ? 'X' : 'O';
    //math to figure out where to place each mark
    row = (int)((choice-1) / ROWS); 
    column = (choice-1) % COLUMNS;
    if (board[row][column] == (choice+'0')) {
      board[row][column] = mark;
    }
    
    i = checkwin(board);
    error_check(board, msg, player, i);	// use to check for game over
    
    // player 2
    player++;
    print_board(board); // call function to print the board on the screen
    printf("Player %d, enter a number:  ", player); // print out player so you can pass game
    scanf("%d", &choice); //using scanf to get the choice
    
    mark = (player == 1) ? 'X' : 'O';
    row = (int)((choice-1) / ROWS); 
    column = (choice-1) % COLUMNS;
    // check for invalid input
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
    
    i = checkwin(board);
    if (i != -1) {
      msg[1] = 4;
    }
    print_board(board); // call function to print the board on the screen
    
    msg[2] = choice;
    msg[3]++;
    n = write(sock, &msg, 4);
    if (n != 4) {
      printf("ERROR: wrong number bytes read\n");
	 exit(1);
    }
    error_check(board, msg, player, i);
    player++;
    
  } while (i ==  - 1); // -1 means no one won

  return 0;
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
