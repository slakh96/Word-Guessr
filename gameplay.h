#include <netinet/in.h>

#define MAX_NAME 30  
#define MAX_MSG 128
#define MAX_WORD 20
#define MAX_BUF 256
#define MAX_GUESSES 4
#define NUM_LETTERS 26
#define WELCOME_MSG "Welcome to our word game. What is your name? "

struct client {
    int fd;	//The integer representing the file descriptor
    struct in_addr ipaddr; //
    struct client *next; //The next person playing this game
    char name[MAX_NAME];	//Name of this client
    char inbuf[MAX_BUF];  // Used to hold input from the client
    char *in_ptr;         // A pointer into inbuf to help with partial reads
};

// Information about the dictionary used to pick random word
struct dictionary {
    FILE *fp;
    int size;
};

struct game_state {
    char word[MAX_WORD];      // The word to guess
    char guess[MAX_WORD];     // The current guess (for example '-o-d')
    int letters_guessed[NUM_LETTERS]; // Index i will be 1 if the corresponding
                                      // letter has been guessed; 0 otherwise
    int guesses_left;         // Number of guesses remaining
    struct dictionary dict;
    
    struct client *head;
    struct client *has_next_turn;
};
  
void init_game(struct game_state *game, char *dict_name);
int get_file_length(char *filename);
char *status_message(char *msg, struct game_state *game);
void add_player(struct client **top, int fd, struct in_addr addr);
void remove_player(struct client **top, int fd);
int check_name_valid(char *buf, struct game_state state);
void Write(int fd, char *message, struct game_state *game, 
		   struct client **new_players);
void add_active_player(struct client **top, int fd, 
					   struct in_addr addr, char *name);
int find_network_newline(const char *buf, int n);
void broadcast(struct game_state *game, char *outbuf);
void remove_from_new(struct client **top, int fd);
void print_players(struct client *head);
int is_game_over(struct game_state *game);
int has_winner(struct game_state *game);
int check_move(struct game_state *game, char guess, int p_id);
int make_move(struct game_state *game, char guess, int p_id);
int valid_guess(struct game_state *game, char guess);
int find_char_array_length(char *char_array);
void update_guess_array(struct game_state *game, char guess);
void handle_move_attempt(struct game_state *game, int move_attempt, int cur_fd,
					     char guess, char *guesser, struct client *new_players);
char *status_message(char *msg, struct game_state *game);
void safe_remove(struct game_state *game, struct client **new_players, int fd);
struct client *find_player(struct client *head, int fd);
struct client *find_previous_player(struct client *head, int fd);
int linked_list_size(struct client *head);
void broadcast(struct game_state *game, char *outbuf);
void announce_turn(struct game_state *game);
void announce_winner(struct game_state *game, struct client *winner);
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game);
