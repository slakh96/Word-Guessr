#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "socket.h"
#include "gameplay.h"


#ifndef PORT
    #define PORT 56408
#endif
#define MAX_QUEUE 5
#define BUFSIZE 30

/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;

/* Add a client to the head of the linked list
 */
void add_player(struct client **top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));

    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->name[0] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *top;
    *top = p;
}

/* Removes client from the linked list and closes its socket.
 * Also removes socket descriptor from allset 
 */
void remove_player(struct client **top, int fd) {
    struct client **p;
	
    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
		printf("Name removed was %s\n", (*p)->name);
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}
void remove_from_new(struct client **top, int fd) {
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
		printf("Name removed was %s\n", (*p)->name);
        //FD_CLR((*p)->fd, &allset);
        //close((*p)->fd);
        //free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
}

//Add a new player to the active players list in the game
void add_active_player(struct client **top, int fd, struct in_addr addr, 
					   char *name){
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }
    int end;
    if(strlen(name) >= MAX_NAME){
	end = MAX_NAME - 1;
    }
    else{
		end = strlen(name);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    //p->name = {'\0'};
    strncpy(p->name, name, MAX_NAME);
    p->name[end] = '\0';
    p->in_ptr = p->inbuf;
    p->inbuf[0] = '\0';
    p->next = *top;
    *top = p;
    printf("Name added was %s\n", p->name);

}

int main(int argc, char **argv) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;
    
    
    // Add the following code to main in wordsrv.c:
  	struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGPIPE, &sa, NULL) == -1) {
    	perror("sigaction");
    	exit(1);
    }
    
    if(argc != 2){
        fprintf(stderr,"Usage: %s <dictionary filename>\n", argv[0]);
        exit(1);
    }
    
    // Create and initialize the game state
    struct game_state game;

    srandom((unsigned int)time(NULL));
    // Set up the file pointer outside of init_game because we want to 
    // just rewind the file when we need to pick a new word
    game.dict.fp = NULL;
    game.dict.size = get_file_length(argv[1]);

    init_game(&game, argv[1]);
    
    // head and has_next_turn also don't change when a subsequent game is
    // started so we initialize them here.
    game.head = NULL;
    game.has_next_turn = NULL;
    
    /* A list of client who have not yet entered their name.  This list is
     * kept separate from the list of active players in the game, because
     * until the new playrs have entered a name, they should not have a turn
     * or receive broadcast messages.  In other words, they can't play until
     * they have a name.
     */
    struct client *new_players = NULL;
    
    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, MAX_QUEUE);
    
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
		    // make a copy of the set before we pass it into select
		    rset = allset;
		    nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
		    if (nready == -1) {
		        perror("select");
		        continue;
		    }

		    if (FD_ISSET(listenfd, &rset)){
		        printf("A new client is connecting\n");
		        clientfd = accept_connection(listenfd);

		        FD_SET(clientfd, &allset);
		        if (clientfd > maxfd) {
		            maxfd = clientfd;
		        }
		        printf("Connection from %s\n", inet_ntoa(q.sin_addr));
		        add_player(&new_players, clientfd, q.sin_addr);
		        char *greeting = WELCOME_MSG;
		        if(write(clientfd, greeting, strlen(greeting)) == -1) {
		            fprintf(stderr, "Write to client %s failed\n", 
		            		inet_ntoa(q.sin_addr));
		            remove_player(&new_players, p->fd);
		        };
		    }
		    /* Check which other socket descriptors have something ready to read
		     * The reason we iterate over the rset descriptors at top level &
		     * search through the two lists of clients each time is that it is
		     * possible that a client will be removed in the middle of one
		     * operations. This is also why we call break after handling input.
		     * If a client has been removed the loop variables may not longer be 
		     * valid.
		     */
		    int cur_fd;
	
		    for(cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
		        if(FD_ISSET(cur_fd, &rset)) {
		            // Check if this socket descriptor is an active player
		            
		            for(p = game.head; p != NULL; p = p->next) {
		                if(cur_fd == p->fd) {
		                	
		                	//Read the player's input, check if it is just one 
		                	//char, and then call make move...make move will 
		                	//try to make a move, if it fails then output the 
		                	//correct message, also whenever a move is made then
		                	//print the state of the game. After each successful
		                	//move, check if the game is over, and if so output
		                	//the correct messages.
						    int nbytes;
							if ((nbytes = read(cur_fd, p->in_ptr, NUM_LETTERS))
								 > 0) {
								p->in_ptr += nbytes;
								//moves nbytes * sizeof(char) into the array
					    		int where;
								if ((where = 
									find_network_newline(p->inbuf, MAX_BUF))
									 > 0) {
									p->inbuf[where - 2] = '\0';
									p->in_ptr -= where;
									if (strlen(p->inbuf) == 1){
									//If the user entered
									//a char, then we can use the helpers
										char *whose_turn =
											 game.has_next_turn->name;
										int move_attempt = make_move(&game, 
														   p->inbuf[0], cur_fd);
										handle_move_attempt(&game, move_attempt,
										cur_fd, p->inbuf[0], whose_turn, 
										new_players);
										memmove(p->inbuf, &(p->inbuf[where]), 
												p->in_ptr - p->inbuf);
										if (is_game_over(&game) == 1){
											if (has_winner(&game) >= 0){
												char winning_message[100] = 
																		{'\0'};
												sprintf(winning_message,
												"Game over! %s won!\r\n", 
												whose_turn);
												broadcast(&game, 
														  winning_message);
												Write(has_winner(&game), 
													"You are the winner!\r\n", 
													&game, &new_players);
												advance_turn(&game);
												
											}
											else {
												char losing_message[100] = 
																		{'\0'};
												sprintf(losing_message, 
												   "Game over! No one won\r\n");
												broadcast(&game, 
														  losing_message);
											}
											
											init_game(&game, argv[1]);
											
											char msg[MAX_BUF];
											broadcast(&game, 
													  status_message(msg,
													  				 &game));
											char buffer[150];
											sprintf(buffer, 
													"It is now %s's turn!\r\n",
													game.has_next_turn->name);
											broadcast(&game, buffer);
											Write(game.has_next_turn->fd, 
													"It is your turn! Please "
													"provide a guess\r\n", 
													&game, &new_players);
										}
										
									}
									else {
										Write(cur_fd, 
											  "Your guess must be a "
											  "single character!\r\n", &game, 
											  &new_players);
									}
								}
							}
		                	else if (nbytes == 0){//Couldn't read anything
		                		safe_remove(&game, &new_players, cur_fd);
		                	}
		                	else {//Read call returned a negative, so system err
		                		fprintf(stderr, 
		                			   "Read called failed; removing player\n");
		                		safe_remove(&game, &new_players, cur_fd);
		                	}
		                }
		            }
		    
		            // Check if any new players are entering their names
					//Seeing if the file descriptor that got set is a new player
		            for(p = new_players; p != NULL; p = p->next) {
				
		                if(cur_fd == p->fd) {
				    		char *greeting = WELCOME_MSG;
						    int nbytes;
			
							if ((nbytes = read(cur_fd, p->in_ptr, MAX_NAME)) 
								> 0) {//Keep reading in a loop until all the 
								//bytes have been read
								p->in_ptr += nbytes;
					    		int where;
								if ((where = find_network_newline(p->inbuf, 
									MAX_BUF)) > 0) {
									p->inbuf[where - 2] = '\0';
									if(check_name_valid(p->inbuf, game) == 0){
										Write(cur_fd, "Correct name!\n", &game, 
											&new_players); //Write to current fd
										add_active_player(&(game.head), cur_fd, 
														  p->ipaddr, p->inbuf);
										if (game.has_next_turn == NULL){
											game.has_next_turn = game.head;
										}
										printf("The new player added has "
											   "name of %s\n", game.head->name);
										printf("The new player added has fd "
											   "of %d\n", game.head->fd);
										//broadcast(&game, game.head->name);
										char new_player_message[MAX_NAME] = 
																		{'\0'};
										sprintf(new_player_message, 
												"%s has just joined the "
												"game\r\n", p->inbuf);
										broadcast(&game, new_player_message);
										remove_from_new(&new_players, cur_fd);
										char msg[MAX_BUF];
										char *cur_state = status_message(msg, 
																		 &game);
										broadcast(&game, cur_state);
										Write(game.has_next_turn->fd, "It is "
										"your turn! Please provide a guess\r\n",
										&game, &new_players);
					
									}
									else{
										Write(cur_fd, "This nickname is "
										"already in use, or is a blank "
										"nickname! Please choose another one\n",
										&game, &new_players);
										Write(cur_fd, greeting, &game,
											  &new_players);			
									}
									p->inbuf[0] = '\0';
									//Don't need anything in there, since
									//the player was already added or has 
									//invalid name
								}
								p->in_ptr -= where;
						}
						else if (nbytes == 0){
							safe_remove(&game, &new_players, cur_fd);
							
						}
						else {
							safe_remove(&game, &new_players, cur_fd);
							fprintf(stderr, "Read call failed when reading "
							"from new players list...removed that player\n");
						}
				
				
				}
			}
		}
	}
	}
    return 0;
}


