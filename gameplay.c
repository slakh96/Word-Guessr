#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gameplay.h"

/* Return a status message that shows the current state of the game.
 * Assumes that the caller has allocated MAX_MSG bytes for msg.
 */
char *status_message(char *msg, struct game_state *game) {
    sprintf(msg, "***************\r\n"
           "Word to guess: %s\r\nGuesses remaining: %d\r\n"
           "Letters guessed: \r\n", game->guess, game->guesses_left);
    for(int i = 0; i < 26; i++){
        if(game->letters_guessed[i]) {
            int len = strlen(msg);
            msg[len] = (char)('a' + i);
            msg[len + 1] = ' ';
            msg[len + 2] = '\0';
        }
    }
    strncat(msg, "\r\n***************\r\n", MAX_MSG);
    return msg;
}


/* Initialize the gameboard: 
 *    - initialize dictionary
 *    - select a random word to guess from the dictionary file
 *    - set guess to all dashes ('-')
 *    - initialize the other fields
 * We can't initialize head and has_next_turn because these will have
 * different values when we use init_game to create a new game after one
 * has already been played
 */
void init_game(struct game_state *game, char *dict_name) {
    char buf[MAX_WORD];
    if(game->dict.fp != NULL) {
        rewind(game->dict.fp);
    } else {
        game->dict.fp = fopen(dict_name, "r");
        if(game->dict.fp == NULL) {
            perror("Opening dictionary");
            exit(1);
        }
    } 

    int index = random() % game->dict.size;
    printf("Looking for word at index %d\n", index);
    for(int i = 0; i <= index; i++) {
        if(!fgets(buf, MAX_WORD, game->dict.fp)){
            fprintf(stderr,"File ended before we found the entry index %d",
            index);
            exit(1);
        }
    } 

    // Found word
    if(buf[strlen(buf) - 1] == '\n') {  // from a unix file
        buf[strlen(buf) - 1] = '\0';
    } else {
        fprintf(stderr, "The dictionary file does not appear "
                "to have Unix line endings\n");
    }
    strncpy(game->word, buf, MAX_WORD);
    game->word[MAX_WORD-1] = '\0';
    for(int j = 0; j < strlen(game->word); j++) {
        game->guess[j] = '-';
    }
    game->guess[strlen(game->word)] = '\0';

    for(int i = 0; i < NUM_LETTERS; i++) {
        game->letters_guessed[i] = 0;
    }
    game->guesses_left = MAX_GUESSES;
	fprintf(stdout, "A new game has begun\n");
}


/* Return the number of lines in the file
 */
int get_file_length(char *filename) {
    char buf[MAX_MSG];
    int count = 0;
    FILE *fp;
    if((fp = fopen(filename, "r")) == NULL) {
        perror("open");
        exit(1);
    }
    
    while(fgets(buf, MAX_MSG, fp) != NULL) {
        count++;
    }
    
    fclose(fp);
    return count;
}

//Tells us the length of a char array. Assume null terminated and at most 20 
//chars else returns -1
int find_char_array_length(char *char_array){
	int index = 0;
	while(index < MAX_WORD && char_array[index] != '\0'){
		index++;
	}
	if (index == MAX_WORD){
		return -1;
	}
	else
		return index;
}

//Write a message, usually to a client using error checking.
void Write(int fd, char *message, struct game_state *game, 
			struct client **new_players){
	int write_status = write(fd, message, strlen(message));
	if (write_status == -1){
		safe_remove(game, new_players, fd);
	}
	else if ( write_status != strlen(message)){
		fprintf(stderr, "The message '%s' was not "
				"written to client %d properly", message, fd);
	}
}
//Check whether name is valid by comparing it to the empty string and to
// each name of current players
int check_name_valid(char *buf, struct game_state state){
	if(strcmp(buf, "") == 0){
		return 1;	
	}
	struct client *cur_client = state.head;
	while(cur_client != NULL){
		if(strcmp(cur_client->name, buf) == 0){
			return 1;		
		}
		cur_client = cur_client->next;	
	}
	fprintf(stdout, "Name was valid\n");
	return 0;
	
}
void broadcast(struct game_state *game, char *outbuf){
	struct client *cur_client = game->head;
	while(cur_client){
		if(write(cur_client->fd, outbuf, strlen(outbuf)) != strlen(outbuf)){
			perror("Write to client");
			fprintf(stderr, "Write to client failed in broadcast function");
		}
		cur_client = cur_client->next;
	}
}

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
 */
int find_network_newline(const char *buf, int n) {
	int cur_pos = 0;
	while (cur_pos < n - 1){
		if (buf[cur_pos] == '\r'){
			if(buf[cur_pos + 1] == '\n'){
				return cur_pos + 1 + 1;
			}		
		}
		cur_pos++;	
	}
	return -1;
}
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game){
	if (game->has_next_turn == NULL){//If has next turn not set yet
		game->has_next_turn = game->head;
	}
	else if (game->has_next_turn->next == NULL){
		game->has_next_turn = game->head;
	}
	else {
		game->has_next_turn = game->has_next_turn->next;
	}
}
//Output a list of all players who are in the game
void print_players(struct client *head){
	
	for(struct client *current_pointer = head;
	current_pointer != NULL; current_pointer = current_pointer->next){
		printf("%s\n", current_pointer->name);
	}
}

//Prints out the identity of the player whose turn it is
void announce_turn(struct game_state *game){
	broadcast(game, strcat("Current turn: %s\r\n", game->has_next_turn->name));
}
//Determines whether the game is over or not; 1 for game over, 0 for not
int is_game_over(struct game_state *game){
	if (game->guesses_left == 0){
		return 1;
	}
	
	int word_length = find_char_array_length(game->word);
	for(int i = 0; i < word_length; i++){
		if (game->guess[i] == '-'){
			return 0; //At least one more letter to guess
		}
	}
	fprintf(stdout, "The game is over\n");
	return 1;
}
//Tells us whether the game has a winner or everyone lost; ASSUMES GAME IS 
//ALREADY OVER -2 if not over, -1 if all lost, else gives the 
//file descriptor of the current player(Assumed to be the winner)
int has_winner(struct game_state *game){
	//printf("Calling is_game_over from has_winner\n");
	if(is_game_over(game)){
		if (game->guesses_left == 0){
			return -1;
			fprintf(stdout, "There is no winner\n");
		}
		fprintf(stdout, "The winner is %s\n", game->has_next_turn->name);
		return game->has_next_turn->fd;
	}
	fprintf(stderr, "Game is not over and thus no winner..yet\n");
	return -2;
}
//Tells us whether the guess was valid or not by seeing if the guess is a letter
//and if so, if it hadn't been guessed before. 1 if invalid, 0 if valid
int valid_guess(struct game_state *game, char guess){
	if (!(guess >= 'a' && guess <= 'z')){
		fprintf(stdout, "%s made an invalid guess\n", 
				game->has_next_turn->name);
		return 1;
	}
	else{
		if(game->letters_guessed[guess - 97] == 1){
			fprintf(stdout, "%s made an invalid guess\n", 
					game->has_next_turn->name);
		}
		return game->letters_guessed[guess - 97]; 
		//Returns index, if index is 1 then 
		//already guessed, if index 0 letter not already guessed and thus valid
	}
}
//Returns whether the guess was a correct one or not - if the char is in the ans
//0 if correct guess, 1 if incorrect guess
int correct_guess(struct game_state *game, char guess){
	int word_length = find_char_array_length(game->word);
	for(int i = 0; i < word_length; i++){
		if (game->word[i] == guess){
			fprintf(stdout, "%s made a correct guess\n", 
					game->has_next_turn->name);
			return 0; 
		}
	}
	fprintf(stdout, "%s made an incorrect guess\n", game->has_next_turn->name);
	return 1;
}
//Checks a move, and tells us whether the guess was correct. 
//-3 if game_over, -2 if wrong player, -1 if invalid guess, 0 if right guess, 
//and 1 if wrong guess
int check_move(struct game_state *game, char guess, int p_id){
	if (is_game_over(game)){
		return -3;
	}
	if (p_id != game->has_next_turn->fd){
		return -2;//Someone is trying to play out of turn
	}
	if (valid_guess(game, guess) == 1){//If invalid guess
		return -1;
	}
	if (correct_guess(game, guess) == 0){
		return 0;
	}
	else {
		return 1; //Guess was wrong
	}
}
//Updates the guess array with the new correct letters
void update_guess_array(struct game_state *game, char guess){
	int word_length = find_char_array_length(game->word);
	for(int i = 0; i < word_length; i++){
		if (game->word[i] == guess){
			if (game->guess[i] == guess){
				fprintf(stderr, "This is weird...update_guess_array\n");
			}
			else if (game->guess[i] >= 'a' && game->guess[i] <= 'z'){
				fprintf(stderr, "This is weirder...update_guess_array\n");
			}
			game->guess[i] = guess;
		}
	}
}
//Updates the letter guessed array e.g. [10010101010] with the new letter
void update_letters_guessed(struct game_state *game, char guess){
	if (game->letters_guessed[guess - 97] == 1){
		fprintf(stderr, "This is weird...update_letters_guessed\n");
	}
	game->letters_guessed[guess - 97] = 1;
}

//Tries to perform a move, and tells us whether the guess was correct. 
//-3 if game_over, -2 if wrong player, -1 if invalid guess, 0 if right guess, 
//and 1 if wrong guess
int make_move(struct game_state *game, char guess, int p_id){
	int move_status = check_move(game, guess, p_id);
	if (move_status < 0){
		
		return move_status;
	}
	
	else if (move_status == 1){
		game->guesses_left -= 1; //Guess only decreases if incorrect and valid
		update_letters_guessed(game, guess);
		advance_turn(game);
		return 1;
	}
	else if (move_status == 0){//Guess was both valid and correct
		update_guess_array(game, guess);
		update_letters_guessed(game, guess);
		return 0;
	}
	else {
		fprintf(stderr, "This is weird...make_move function\n");
		return -1; //Note: For error checking only
	}
}
//Prints out the correct strings to the given clients
void handle_move_attempt(struct game_state *game, int move_attempt, int cur_fd, 
						 char guess, char *guesser, struct client *new_players){
	printf("The word is %s\n", game->word);
	char buffer[150];
	if (move_attempt == -3){
		Write(cur_fd, "Game is over! No moves allowed!\r\n", 
			  game, &new_players);
	}
	else if (move_attempt == -2){
		Write(cur_fd, "You must not play out of turn!\r\n", game, &new_players);
	}
	else if (move_attempt == -1){
		Write(cur_fd, "The guess was invalid! Please try "
	   		  "again with a single lowercase letter\r\n", game, &new_players);
	}
	else if (move_attempt == 1){
		sprintf(buffer, "%s guessed %s, which was incorrect.\r\n", 
				guesser, &guess);
		broadcast(game, buffer);
		char msg[MAX_BUF];
		broadcast(game, status_message(msg, game));
		sprintf(buffer, "It is now %s's turn!\r\n", game->has_next_turn->name);
		broadcast(game, buffer);
		fprintf(stdout, "It is now %s's turn!\r\n", game->has_next_turn->name);
		Write(game->has_next_turn->fd, "It is your turn! "
			  "Please provide a guess\r\n", game, &new_players);
	}
	else if (move_attempt == 0){
		char msg[MAX_BUF];
		sprintf(buffer, "%s guessed %s, which was correct!\r\n",
				guesser, &guess);
		broadcast(game, buffer);
		char *cur_state = status_message(msg, game);
		broadcast(game, cur_state);
		sprintf(buffer, "It is now %s's turn!\r\n", game->has_next_turn->name);
		broadcast(game, buffer);
		fprintf(stdout, "It is now %s's turn!\r\n", game->has_next_turn->name);
		Write(game->has_next_turn->fd, 
			  "It is your turn! Please provide a guess\r\n", game, 
			  &new_players);
	}
	else {
		fprintf(stderr, "This is weird...handle_move_attempt\n");
	}	
}

//Removes a player safely when they disconnect by checking if they are a new
//player, if it is currently their turn, and if they are the last player left
//in the game
void safe_remove(struct game_state *game, struct client **new_players, int fd){
	char *removed_player = NULL;
	struct client *found_new_player = find_player(*new_players, fd);
	struct client *found_active_player = find_player(game->head, fd);
	if (found_new_player != NULL){//If the guy was still in new
		removed_player = found_new_player->name;
		remove_player(new_players, fd);
	}
	else if (found_active_player != NULL){//If active player
		removed_player = found_active_player->name;
		if (linked_list_size(game->head) == 1){//Last player leaving!
			game->has_next_turn = NULL;
			char buf[150];
			sprintf(buf, "%s has left the game\r\n", removed_player);
			broadcast(game, buf);
			remove_player(&(game->head), fd);
		}
		else {//Still other players
			if (game->has_next_turn->fd == fd){//It's this guy's turn!
				advance_turn(game);
				remove_player(&(game->head), fd);
				char buf[150];
				sprintf(buf, "%s has left the game\r\n", removed_player);
				broadcast(game, buf);
				char buffer[150];
				sprintf(buffer, "It is now %s's turn!\r\n", 
					    game->has_next_turn->name);
				broadcast(game, buffer);
				Write(game->has_next_turn->fd, 
					  "It is your turn! Please provide a guess\r\n", 
					  game, new_players);
				
			}
			else {//It's not this guy's turn
				remove_player(&(game->head), fd);
				char buf[150];
				sprintf(buf, "%s has left the game\r\n", removed_player);
				broadcast(game, buf);
			}
		}
	}
	else{
		fprintf(stderr, "This is weird...safe_remove\n");
	}
	
}
//Finds a player in the given linked list of players. Returns the 
//client struct if found, else NULL
struct client *find_player(struct client *head, int fd){
	struct client *cur_client = head;
	while(cur_client != NULL && cur_client->fd != fd){
		cur_client = cur_client->next;
	}
	return cur_client;
}
//Finds the previous player to the player we input, and returns that client. 
//Assumes that player we input exists, and if the player has no previous,
//then returns null. If error, also returns NULL so BE CAREFUL
struct client *find_previous_player(struct client *head, int fd){
	struct client *cur_client = head;
	if (cur_client == NULL || cur_client->fd == fd){
		return NULL;
	}
	while (cur_client->next != NULL && cur_client->next->fd != fd){
		cur_client = cur_client->next;
	}
	return cur_client;
}
//Returns the length of the linked list we put in by iterating through it, or
//zero if it is NULL.
int linked_list_size(struct client *head){
	struct client *cur_client = head;
	int length = 0;
	while (cur_client != NULL){
		length += 1;
		cur_client = cur_client->next;
	}
	return length;
}


