/*
  Filename: tictactoeServer.c
  Created by: Aisha Iftikhar
  Creation date: 4/7/20
  Synopsis: This program is a UDP server side tictactoe game implementation. It allows for two players on different networks to play; this version automatically generates moves. Further, the server can send and recieve multicast requests from a client as well as resume a game provided a game board from a client.
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
#include <errno.h>

/*Global variables*/
#define ROWS 3
#define COLUMNS 3
#define sendrecvflag 0
#define bytes 15
#define totalGames 3
#define TIMEOUT 10
/*predefined multicast port and IP*/
#define MC_PORT 1818
#define MC_GROUP "239.0.0.1"

/*Function Declarations*/
int game_check(int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr, struct timeval tv);
int setBoard(char msg[bytes], char board[ROWS][COLUMNS]);
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

/*multicast structure*/
struct ip_mreq mreq;



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
  int sock, rc, mc_sock;
  struct sockaddr_in serv_addr, cli_addr;
  struct timeval tv;
  fd_set socketFDS;
  int maxSD = 0; // highest socket descriptor we've seen 

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
  if (bind(sock, (struct sockaddr * ) & serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: bind failed\n");
    exit(1);
  }
  
  /*set up multicast socket*/
  if ((mc_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("ERROR: Cannot open datagram socket\n");
    exit(1);
  }
  
  /*format multicast port*/
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(MC_PORT);
  
  /*Bind the multicast socket with the server address*/
  if (bind(mc_sock, (struct sockaddr * ) & serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: bind failed\n");
    exit(1);
  }

  mreq.imr_multiaddr.s_addr = inet_addr(MC_GROUP);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, & mreq, sizeof(mreq)) < 0) {
    perror("ERROR: setsockopt failed\n");
    exit(1);
  }
  
  printf("Connected. Awaiting game request...\n");

  /*necessary to generate random number for move*/
  srand(time(0));

  /*CONTINUALLY CHECK FOR INCOMING GAME REQUESTS*/
  while (1) {
    /*zero socket descriptors*/
    FD_ZERO(&socketFDS);
    /*add regular socket and multicast socket to list of socket descriptors*/
    FD_SET(sock, &socketFDS);
    FD_SET(mc_sock, &socketFDS);
    /*figure out which socket is largest for select*/
    maxSD = sock;
    if(mc_sock > maxSD) {
      maxSD = mc_sock;
    }
    
    /*blocks until something arrives*/
    rc = select(maxSD + 1, &socketFDS, NULL, NULL, NULL);	
    
    /*check which socket recieved something and send to game_check to read command code*/
    if (FD_ISSET(sock, &socketFDS)) { 
      rc = game_check(sock, serv_addr, cli_addr, tv);
      if(rc == 0) { // client disconnected
      	printf("Time out...No messages received\n");
      } else if (rc == -1) {
      	printf("Something went wrong. Exiting...\n");
      	exit(1);
      }                
    } else if (FD_ISSET(mc_sock, &socketFDS)){
      rc = game_check(mc_sock, serv_addr, cli_addr, tv);
      if(rc == 0) { // client disconnected
      	printf("Time out...No messages received\n");
      } else if (rc == -1) {
      	printf("Something went wrong. Exiting...\n");
      	exit(1);
      }    
    }
  }
  return 0;
}


/*game_check reads from provided socket and determines path based on command code*/
int game_check(int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr, struct timeval tv) {
  /*variable declarations*/
  char board[ROWS][COLUMNS];
  char msg[bytes];
  memset(msg, 0, bytes);
  int n, i, addrlen = sizeof(cli_addr), rc;
  int game_check = 0;

  /*set timeout- NOT USED*/
  tv.tv_sec = TIMEOUT;
  tv.tv_usec = 0;


  /*loop used for timeout purposes*/
  do {
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, & tv, sizeof(tv)))
       perror("ERROR - setsocketopt");

    rc = recvfrom(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, (socklen_t * ) & addrlen);

    if (rc <= 0) {
      printf("ERROR: haven't received anything. RC is %d\nError code %d: %s.\n\n", rc, errno, strerror(errno));
      return 0;
    } else {
      printf("\nOk, got something...\n");
      break;
    }
  }while (1);

  /*check command code*/
  switch (msg[1] + 0) {
  case 0:
    /*if initial incoming message has 0 for command, first check if game exists*/
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
    if (game_check == 0) {
      printf("Game %d not found. Something's gone wrong. Exiting...\n", msg[5] + 0);
      msg[2] = 8;
      n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
      }
    }
    return 1;
  case 1:
    /*check if there is space for another game*/
    if (current_game_count == (totalGames)) {
      printf("ERROR: Not enough space for a new game. Please wait and try again later...\n");
      msg[2] = 7;
      n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
      }
      return 1;
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
    printf("Game number will be %d\n", current_game_count);
 
    /*send board to play for move generation*/
    rc = play(board, msg, sock, serv_addr, cli_addr, current_game_count);
    if (rc != 0) {
      printf("Something went wrong. Exiting...\n");
      exit(1);
    }

    /*copy game_num and board to games array*/
    games[current_game_count].game_num = current_game_count;
    memcpy(games[current_game_count++].board, board, sizeof(board));
    return 1;
  case 2:
    /*Resume game request from client*/
    /*client's move already on board; client also sent turn number of move it just made; server sends new game number*/
    if (current_game_count == totalGames) {
      msg[2] = 7;
      n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
      }
    } else {
      printf("\nResume game requested...Setting up game\n");
      printf("Msg rcvd: %d, %d, %d, %d, %d, %d\n", msg[0] + 0, msg[1] + 0, msg[2] + 0, msg[3] + 0, msg[4] + 0, msg[5] + 0);
      printf("Board rcvd: %d, %d, %d, %d, %d, %d, %d, %d, %d\n", msg[6]+0, msg[7]+0, msg[8]+0, msg[9]+0, msg[10]+0, msg[11]+0, msg[12]+0, msg[13]+0, msg[14]+0);
      
      /*assign game number to client*/
      printf("Game number will be %d\n", current_game_count);
      
      /*initialize a new baord and recreate given client message*/
      rc = initSharedState(board);
      if (rc != 0) {
        printf("Something went wrong. Exiting...\n");
        exit(1);
      }

      rc = setBoard(msg, board);
      if (rc != 0) {
        printf("Something went wrong. Exiting...\n");
        exit(1);
      } 
      
      /*send board to play function to generate moves*/
      rc = play(board, msg, sock, serv_addr, cli_addr, current_game_count);
      if (rc != 0) {
        printf("Something went wrong. Exiting...\n");
        exit(1);
      }

      /*copy game_num and board to games array*/
      games[current_game_count].game_num = current_game_count;
      memcpy(games[current_game_count++].board, board, sizeof(board));
    }
    return 1;
  case 3:
    /*new server request from client*/
    printf("Received: %d, %d, %d, %d, %d, %d\n", msg[0] + 0, msg[1] + 0, msg[2] + 0, msg[3] + 0, msg[4] + 0, msg[5] + 0);
    if (current_game_count != totalGames) {
      printf("I have space! Sent response to multicast request.\n");
      msg[2] = 9;
      n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
      }
    } else {
      printf("Unfortunately, I can't accept a new game right now\n");
    }
    return 1;
  default:
    return 1;
  }
  return -1;
}


/*setBoard used to recreate board in resume game request*/
int setBoard(char msg[bytes], char board[ROWS][COLUMNS]) {
  int i, mark, copy[10];
  int row, column;
  
  for(i = 1; i < 10; i++) {
    copy[i] = msg[i+5];
  }
  //printf("Copied array: %d, %d, %d, %d, %d, %d, %d, %d, %d\n", copy[1], copy[2], copy[3], copy[4], copy[5], copy[6], copy[7], copy[8], copy[9]);
  
  for(i = 1; i < 10; i++) {
    mark = copy[i];
    row = (int)((i- 1) / ROWS);
    column = (i - 1) % COLUMNS;
    if (mark == 0) {
      //nothing - no move has been made here
    } else if (mark == 1) {
      //1 means player 1 (server) move
      board[row][column] = 'X';
    } else if (mark == 2) {
      //2 means player 2 (client) move
      board[row][column] = 'O';
    } else {
      printf("That's not right; this board is not correct.\n");
      return -1;
    }
  }
  
  print_board(board);
  return 0;
}


/*play used to generate game moves*/
int play(char board[ROWS][COLUMNS], char msg[bytes], int sock, struct sockaddr_in serv_addr, struct sockaddr_in cli_addr, int game_num) {
  /*variable declarations*/
  int i = 0, j = 0, n, choice, row, column, player = 0, count = 0, rc;
  char mark;
  socklen_t addrlen = sizeof(cli_addr);

  /*debug statements*/
  printf("Received: %d, %d, %d, %d, %d, %d\n", msg[0]+0, msg[1]+0, msg[2]+0, msg[3]+0, msg[4]+0, msg[5]+0);
  
  /*check if client sent game winning move in resume game request*/
  i = checkwin(board);
  if(i==0 || i==1) 
    player = 2;
  rc = check_response(board, msg, player, i, sock, cli_addr);
    if(rc != 0) {
      return 0;
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
 
  /*loop once if generating first move; loop twice for receiving and then sending*/
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
      /*regenerate if invalid move*/
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
      
      /*update games in struct for current plays*/
      games[msg[5]+0].turn_num = msg[4]+0;
      memcpy(games[msg[5]+0].prev_msg, msg, sizeof(*msg));
      
      n = sendto(sock, msg, bytes, sendrecvflag, (struct sockaddr * ) & cli_addr, addrlen);
      if (n != bytes) {
        printf("ERROR: wrong number bytes read\n");
        exit(1);
      }
      printf("Sent: %d, %d, %d, %d, %d, %d\n\n", msg[0]+0, msg[1]+0, msg[2]+0, msg[3]+0, msg[4]+0, msg[5]+0);
    }
 
    /*check win*/
    rc = check_response(board, msg, player, i, sock, cli_addr);
    if(rc != 0) {
      return 0;
    }

    player++;
    j++;
  }

  return 0;
}


/*used to delete completed games from games structure*/
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


/*check_response used to error check response codes*/
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
      //deleteGame(msg[5]+0);
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


/*checkwin used to determine whether game is won*/
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


/*print_board used to print current variation of game board*/
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


/*initSharedState used to set up board*/
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
