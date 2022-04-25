/*
  Filename: tictactoeServer.c
  Created by: Aisha Iftikhar
  Creation date: 3/23/20
  Synopsis: This project is a server side UDP implementation for a version of tictactoe played on different machines. The server can play multiple games at once, and will autoplay. The server can handle lost and duplicate packets.
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
#include <sys/time.h>
#include <errno.h>

/*Global variables*/
# define ROWS 3
# define COLUMNS 3
# define sendrecvflag 0
# define bytes 6
/*total games needs to be whatever you want + 1 b/c bug in code makes games start at 1(not 0)*/
# define totalGames 3
# define TIMEOUT 5

/*Function Declarations*/
int game_check(int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr, struct timeval tv);
int play(char board[ROWS][COLUMNS], char msg[bytes], int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr, int game_num);
void deleteGame(int game_num);
int check_response(char board[ROWS][COLUMNS], char msg[bytes], int player, int i, int sock, struct sockaddr_in serv_addr);
int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS]);
int initSharedState(char board[ROWS][COLUMNS]);

/*structure used to keep track of different games*/
struct store_games {
  int game_num;
  char board[ROWS][COLUMNS];
  int turn_num;
  char prev_msg[bytes];
};
/*can change how many games can be held*/
struct store_games games[totalGames];
/*variable used to keep track of total number of games played since server started up*/
static int current_game_count = 0;

/*MAIN*/
int main(int argc, char * argv[]) {
  /*Error check input*/
  if (argc != 2) {
    printf("ERROR: Incorrect number of arguments.\n");
    printf("Use the format: tictactoeServer <port_number> \n");
    exit(1);
  }

  /*variable declarations*/
  int PORT = atoi(argv[1]);
  int sock, rc;
  struct sockaddr_in serv_addr, cli_addr;
  struct timeval tv;
  
  /*check if the port number is between 1 and 64000*/
  if (PORT < 1 || PORT > 64000) {
    printf("ERROR: Invalid port number\n");
    exit(1);
  }

  /*Initialize socket connection*/
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("ERROR: Cannot open datagram socket\n");
    exit(1);
  }
  
  /*format server*/
  memset( & serv_addr, 0, sizeof(serv_addr));
  memset( & cli_addr, 0, sizeof(cli_addr));

  /*Filling server information*/
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(PORT);

  /*Bind the socket with the server address*/
  if (bind(sock, (const struct sockaddr * ) & serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: bind failed\n");
    exit(1);
  }

  printf("Connected. Awaiting game request...\n");
  
  /*necessary to generate random number for move*/
  srand(time(0));

  /*CONTINUALLY CHECK FOR INCOMING GAME REQUESTS*/
  while (1) {
    rc = game_check(sock, serv_addr, cli_addr, tv);
    if (rc != 0) {
      printf("ERROR: Something went wrong. Exiting...\n");
      exit(1);
    }
  }

  return 0;
}

int game_check(int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr, struct timeval tv) {
  /*create a new board*/
  char board[ROWS][COLUMNS];
  char msg[bytes]; 
  int n, i, addrlen = sizeof(cli_addr), rc, count = 0;
  int game_check = 0;

  tv.tv_sec = TIMEOUT;
  tv.tv_usec = 0;

  do {
  	if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)))
  		perror("ERROR - setsocketopt");
  		
  	/*check for any incoming messages*/
  	rc = recvfrom(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, (socklen_t * ) & addrlen);
  	if (rc <= 0) { 
  	  	count++;
  		printf("ERROR: haven't received anything. RC is %d\nError code %d: %s.\nAttempt number %d...\n", rc, errno, strerror(errno), count); 
  	}else {
  		printf("Ok, got something...Let's play\n"); 
  		break;
  	}
  		
  	if(count > 4) {
  		printf("Right, well, I'm not receiving anything. Let's figure this out.\n");
 		if (current_game_count == 0 && games[current_game_count].turn_num == 0) {
 			printf("I've received no game requests. I'm shutting down...\n");
 			exit(1);
 		} else{
 			printf("Let me resend what my last move was on my current game\n");
 			memcpy(msg, games[current_game_count].prev_msg, sizeof(games[current_game_count].prev_msg));
 			n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      		if (n != bytes) {
        			printf("ERROR: wrong number bytes read\n");
        			exit(1);
      		}
      		printf("Sent: %d, %d, %d, %d, %d, %d\n\n", msg[0]+0, msg[1]+0, msg[2]+0, msg[3]+0, msg[4]+0, msg[5]+0);
      		return 0;
 		}
  	}
    		
  }while(1);

  /*check command code*/
  switch (msg[1] + 0) {
  case 0:
    /*if initial incoming message has 0 for command, first check if game exists*/
      /*look throw games array to find matching game_num to incoming msg*/
      for (i = 0; i < current_game_count; i++) {
        /*if game is found, get game_num and board, and play*/
        if ((games[i].game_num) == (msg[5] + 0)) {
          game_check = 1;
          printf("Found the game: %d\n", games[i].game_num);
          rc = play(games[i].board, msg, sock, serv_addr, cli_addr, games[i].game_num);
          if (rc != 0) {
            printf("Something went wrong. Exiting...\n");
            exit(1);
          }
        }
      }
      /*if game_num is not found, then print error*/
      if(game_check == 0) {
      	printf("Game %d not found. Something's gone wrong. Exiting...\n", msg[5]+0);
      	msg[2] = 8;
      	n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      	if (n != bytes) {
        	    printf("ERROR: wrong number bytes read\n");
       	    exit(1);
      	}
      }
    return 0;
  case 1:
    /*check if there is space for another game*/
    if(current_game_count == (totalGames - 1)) {
    	 printf("ERROR: Not enough space for a new game. Please wait and try again later...\n");
    	 msg[2] = 7;
    	 n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
      }
      return 0;
    }

    /*command code of 1 means new game has been requested*/
    printf("New game requested. Initializing...\n");

    /*initialize game board*/
    rc = initSharedState(board);
    if (rc != 0) {
      printf("Something went wrong. Exiting...\n");
      exit(1);
    }

    /*assign game number to client*/
    printf("Game number %d\n", current_game_count);

    rc = play(board, msg, sock, serv_addr, cli_addr, current_game_count);
    if (rc != 0) {
      printf("Something went wrong. Exiting...\n");
      exit(1);
    }
    
    /*copy game_num and board to games array*/
    games[current_game_count].game_num = current_game_count;
    memcpy(games[current_game_count++].board, board, sizeof(board));
    return 0;
    
  default:
    return 0;
  }
  return -1;
}

int play(char board[ROWS][COLUMNS], char msg[bytes], int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr, int game_num) {
  int i = 0, j = 0, n, choice, row, column, player = 0, count = 0, rc;
  char mark;
  socklen_t addrlen = sizeof(cli_addr);

  /*debug statements*/
  printf("Received: %d, %d, %d, %d, %d, %d\n", msg[0]+0, msg[1]+0, msg[2]+0, msg[3]+0, msg[4]+0, msg[5]+0);
  
  rc = check_response(board, msg, player, i, sock, cli_addr);
    if(rc != 0) {
      return 0;
    }
  
  /* check the turn number to ensure if the data received is a duplicate*/
  if(msg[1]+0 == 0) {
  if(msg[4]+0 != (games[msg[5]+0].turn_num + 1)) {
  	printf("Invalid turn number. Let's see what we need to do.\n");
  	/*if turn number received is 1 less than what I have stored, then I have to resend*/
  	if(msg[4]+0 == (games[msg[5]+0].turn_num - 1)) {
  	  printf("Resending previous game move...\n");
  	  memcpy(msg, games[msg[5]+0].prev_msg, sizeof(games[msg[5]+0].prev_msg));
 	  n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
       if (n != bytes) {
       	printf("ERROR: wrong number bytes read\n");
       	exit(1);
       }
       printf("Sent: %d, %d, %d, %d, %d, %d\n\n", msg[0]+0, msg[1]+0, msg[2]+0, msg[3]+0, msg[4]+0, msg[5]+0);
       return 0;
  	/*if turn number is even less than my current stored turn number, sending error*/
  	} else if (msg[4]+0 < (games[msg[5]+0].turn_num - 1)) {
  	  printf("Pretty sure the games's out of sync. I'm gonna have to kick you out. Sorry, pal.\n");
  	  msg[2] = 2;
	  n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
       if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
       }
       deleteGame(msg[5]+0);
  	}
  }
  }

  /* if command code was 1, means new game, so generate a move and sent first
     otherwise, recv move from client, check win, generate move, check win, send move */
  if (msg[1] != 0) {
    count = 1;
    player = 1;
    msg[1] = 0;
  } else {
    count = 2;
    player = 2;
  }

  while (j < count) {
    /*figure out who the player is*/
    player = (player % 2) ? 1 : 2;

    if (player == 1) {
      /*my turn*/
      choice = (rand() % (9 - 1 + 1)) + 1;
    } else if (player == 2) {
      /*client's turn*/
      choice = msg[3] + 0;
    } else {
      printf("Something went wrong. Exiting...\n");
      exit(1);
    }

    /*check for win or game over*/
    i = checkwin(board);
    rc = check_response(board, msg, player, i, sock, cli_addr);
    if(rc != 0) {
      return 0;
    }

    /*set mark to X for player 1, O for player 2*/
    mark = (player == 1) ? 'X' : 'O';
    
    /*math to figure out what move*/
    row = (int)((choice - 1) / ROWS);
    column = (choice - 1) % COLUMNS;
    if (board[row][column] == (choice + '0')) {
      board[row][column] = mark;
    } else {
      while (board[row][column] != (choice + '0') || ((choice + '0') < 0 && (choice + '0') > 10)) {
        choice = (rand() % (9 - 1 + 1)) + 1;
        row = (int)((choice - 1) / ROWS);
        column = (choice - 1) % COLUMNS;
      }
      board[row][column] = mark;
    }
    
    /*check for win again*/
    i = checkwin(board);
    
    /*format message to send client*/    
    if (player == 1) {
      printf("My choice: %d\n", choice);
      if (i != -1) {
        msg[2] = 4;
      }
      msg[3] = choice;
      if(msg[4] != 0) 
      	msg[4]++;
      msg[5] = game_num;
      games[msg[5]+0].turn_num = msg[4]+0;
      memcpy(games[msg[5]+0].prev_msg, msg, sizeof(msg));
      n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
      }
      printf("Sent: %d, %d, %d, %d, %d, %d\n\n", msg[0]+0, msg[1]+0, msg[2]+0, msg[3]+0, msg[4]+0, msg[5]+0);
      printf("My turn number was %d\n", msg[4]+0);
      printf("Message stored %d, %d, %d, %d, %d, %d\n\n", games[msg[5]+0].prev_msg[0]+0, games[msg[5]+0].prev_msg[1]+0, games[msg[5]+0].prev_msg[2]+0, games[msg[5]+0].prev_msg[3]+0, games[msg[5]+0].prev_msg[4]+0, games[msg[5]+0].prev_msg[5]+0);
    }

    rc = check_response(board, msg, player, i, sock, cli_addr); // use to check for game over
    if(rc != 0) {
      return 0;
    }

    player++;
    j++;
  }

  return 0;
}

void deleteGame(int num){
  int index = 0;
  while (index < current_game_count) {
  	if (num == games[index].game_num) break;
  	++index;
  }
  if (index == current_game_count) return;
  
  printf("Deleting game %d...\n", num);
  if(index != current_game_count) {
     games[index].turn_num = games[current_game_count - 1].turn_num;
  	games[index].game_num = games[current_game_count - 1].game_num;
  	memcpy(games[current_game_count - 1].board, games[index].board, sizeof(games[index].board));
  	memcpy(games[current_game_count - 1].prev_msg, games[index].prev_msg, sizeof(games[index].prev_msg));
  }
  --current_game_count;
}

int check_response(char board[ROWS][COLUMNS], char msg[bytes], int player, int i, int sock, struct sockaddr_in cli_addr) {
  int n;
  /*check response code from msg*/
  switch (msg[2] + 0) {
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
      printf("Game %d Over\n", msg[5]+0);
      printf("==>\aPlayer %d wins\n\n", player);
      if (player == 2) {
        msg[2] = 5;
        n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, sizeof(cli_addr));
        if (n != bytes) {
          printf("ERROR: wrong number bytes read\n");
          exit(1);
        }
      }
    } else if (i == 0) {
      system("clear");
      print_board(board);
      printf("Game %d Over\n", msg[5]+0);
      printf("==>\aGame draw\n\n");
      if (player == 2) {
        msg[2] = 5;
        n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, sizeof(cli_addr));
        if (n != bytes) {
          printf("ERROR: wrong number bytes read\n");
          exit(1);
        }
      }
    }
    return 0;
  case 5:
    printf("Game Over Acknowledged\n\n");
    deleteGame(msg[5]+0);
    return 1;
  case 6:
    printf("ERROR: Incompatible Version Number\n");
    exit(1);
  case 7:
    printf("That's weird: I'm the server. Let's try again.\n");
    exit(1);
  case 8:
    printf("ERROR: Game number mismatch. Tha game number received is not valid\n");
    return 1;
  default:
    return 0;
  }
}

int checkwin(char board[ROWS][COLUMNS]) {
  if (board[0][0] == board[0][1] && board[0][1] == board[0][2]) // row matches
    return 1;

  else if (board[1][0] == board[1][1] && board[1][1] == board[1][2]) // row matches
    return 1;

  else if (board[2][0] == board[2][1] && board[2][1] == board[2][2]) // row matches
    return 1;

  else if (board[0][0] == board[1][0] && board[1][0] == board[2][0]) // column
    return 1;

  else if (board[0][1] == board[1][1] && board[1][1] == board[2][1]) // column
    return 1;

  else if (board[0][2] == board[1][2] && board[1][2] == board[2][2]) // column
    return 1;

  else if (board[0][0] == board[1][1] && board[1][1] == board[2][2]) // diagonal
    return 1;

  else if (board[2][0] == board[1][1] && board[1][1] == board[0][2]) // diagonal
    return 1;

  else if (board[0][0] != '1' && board[0][1] != '2' && board[0][2] != '3' &&
    board[1][0] != '4' && board[1][1] != '5' && board[1][2] != '6' &&
    board[2][0] != '7' && board[2][1] != '8' && board[2][2] != '9')

    return 0; // Return of 0 means game over
  else
    return -1; // return of -1 means keep playing
}

void print_board(char board[ROWS][COLUMNS]) {
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

int initSharedState(char board[ROWS][COLUMNS]) {
  /* this just initializing the shared state aka the board */
  int i, j, count = 1;
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++) {
      board[i][j] = count + '0';
      count++;
    }
  return 0;
}
