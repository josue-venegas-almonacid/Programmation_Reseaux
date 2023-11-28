// awale.h

#ifndef AWALE_H
#define AWALE_H

// Definition of the Awale structure
typedef struct
{
    int player_one[6];
    int player_two[6];
    int score_one;
    int score_two;
    int turn;
    int finished;
} Awale;

// Function declarations
char* display(Awale game);
void make_move(Awale* game, int chosen_case, int player);
int check_move(Awale* game, int player, int move, char* buffer);
int is_game_over(Awale* game);
void finish_game(Awale* game);
char* display_winner(Awale game);
void run_game(Awale* game);

#endif // AWALE_H
