/*
  Filename: tictactoeServer.c
  Created by: Aisha Iftikhar
  Creation date: 2/20/20
  Synopsis: This project is a server side UDP implementation for a version of tictactoe played on different machines. The server can play multiple games at once, and will autoplay. 
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
#include <time.h>

/*Global variables*/
#define ROWS 3
# define COLUMNS 3
# define sendrecvflag 0
# define bytes 6

/*Function Declarations*/
int game_check(int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr);
int play(char board[ROWS][COLUMNS], char msg[bytes], int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr, int game_num);
int check_response(char board[ROWS][COLUMNS], char msg[bytes], int player, int i, int sock, struct sockaddr_in serv_addr);
int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS]);
int initSharedState(char board[ROWS][COLUMNS]);

/*structure used to keep track of different games*/
struct store_games {
  int game_num;
  char board[ROWS][COLUMNS];
};
/*currently holds a total of 100 games*/
struct store_games games[100];
/*variable used to keep track of total number of games played since server started up*/
static int current_game_count = 1;

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
    rc = game_check(sock, serv_addr, cli_addr);
    if (rc != 0) {
      printf("ERROR: Something went wrong. Exiting...\n");
      exit(1);
    }
  }

  return 0;
}

int game_check(int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr) {
  /*create a new board*/
  char board[ROWS][COLUMNS];
  char msg[bytes]; 
  int n, i, addrlen = sizeof(cli_addr), rc;
  int game_check = 0;

  /*check for any incoming messages*/
  n = recvfrom(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, (socklen_t * ) & addrlen);
  if (n != bytes) {
    printf("ERROR: wrong number of bytes received\n");
    exit(1);
  }

  /*check command code*/
  switch (msg[1] + 0) {
  case 0:
    /*if initial incoming message has 0 for command, first check if game exists*/
    if ((msg[5] + 0) != 0) {
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
      	exit(1);
      }
    } else {
      /*if incoming command if 0 and no game_num received, then error*/
      printf("ERROR: No game request received.\n");
      msg[2] = 8;
      n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
      }
    }
    return 0;
  case 1:
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
  int i = 0, j = 0, n, choice, row, column, player, count = 0, rc;
  char mark;
  socklen_t addrlen = sizeof(cli_addr);

  /*debug statements*/
  printf("Received: %d, %d, %d, %d, %d, %d\n", msg[0]+0, msg[1]+0, msg[2]+0, msg[3]+0, msg[4]+0, msg[5]+0);

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
      msg[4]++;
      msg[5] = game_num;
      n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
      }
      printf("Sent: %d, %d, %d, %d, %d, %d\n\n", msg[0]+0, msg[1]+0, msg[2]+0, msg[3]+0, msg[4]+0, msg[5]+0);
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
