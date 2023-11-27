#include <stdio.h>

// Definition of the Awale structure
typedef struct
{
    int player_one[6];
    int player_two[6];
    int score_one;
    int score_two;
    int turn;
    int finished;
    int player1Socket;
    int player2Socket;
    int observersSockets[10];
} Awale;


#define CLEAR_SCREEN_ANSI "\033[H\033[J"

// Display the Awale board on the screen
//*************************************
void display(Awale game)
//*************************************
{
    printf(CLEAR_SCREEN_ANSI);
    printf("\n\n\nTurn: %s\n\n", (game.turn == 1 ? "Player One" : "Player Two"));

    printf("   Player Two Score: %d\n", game.score_two);
    printf("   Player One Score: %d\n", game.score_one);
    printf("   ┌─────┬─────┬─────┬─────┬─────┬─────┐\n");
    printf("   │ %3d │ %3d │ %3d │ %3d │ %3d │ %3d │\n",
           game.player_two[5], game.player_two[4], game.player_two[3], game.player_two[2], game.player_two[1], game.player_two[0]);
    printf("   ├─────┼─────┼─────┼─────┼─────┼─────┤\n");
    printf("   │ %3d │ %3d │ %3d │ %3d │ %3d │ %3d │\n",
           game.player_one[0], game.player_one[1], game.player_one[2], game.player_one[3], game.player_one[4], game.player_one[5]);
    printf("   └─────┴─────┴─────┴─────┴─────┴─────┘\n");   
}

// Player makes a move between 0 and 5
// Moves the seeds, updates the score
//***************************************
void make_move(Awale* game, int chosen_case, int player)
//***************************************
{
    int current_case = chosen_case;
    int seed_count = (player == 1) ? game->player_one[chosen_case] : game->player_two[chosen_case];
    int opponent_case = 11; // Used for the rule: take all or take none
    int take_all = 0;

    if (player == 1)
        game->player_one[chosen_case] = 0;
    else
        game->player_two[chosen_case] = 0;

    // Place the seeds
    while (seed_count != 0)
    {
        current_case++;
        if (current_case == 12) current_case = 0;
        if (current_case == chosen_case) current_case++;
        if (current_case > 5)
        {
            if (player == 1)
                game->player_two[current_case - 6]++;
            else
                game->player_one[current_case - 6]++;
        }
        else
        {
            if (player == 1)
                game->player_one[current_case]++;
            else
                game->player_two[current_case]++;
        }
        seed_count--;
    }

    // Take the gained seeds and update the score
    if ((current_case > 5) && ((player == 1 && game->player_two[current_case - 6] == 2) || (player == 2 && game->player_one[current_case - 6] == 2) || (player == 1 && game->player_two[current_case - 6] == 3) || (player == 2 && game->player_one[current_case - 6] == 3)))
    {
        while ((opponent_case != current_case) && (take_all == 0))
        {
            if ((player == 1 && game->player_two[opponent_case - 6] != 0) || (player == 2 && game->player_one[opponent_case - 6] != 0))
                take_all = 1;
            opponent_case--;
        }
        opponent_case--;
        while ((opponent_case > 5) && (take_all == 0))
        {
            if ((player == 1 && (game->player_two[opponent_case - 6] != 2) && (game->player_two[opponent_case - 6] != 3)) || (player == 2 && (game->player_one[opponent_case - 6] != 2) && (game->player_one[opponent_case - 6] != 3)))
                take_all = 1;
            opponent_case--;
        }

        // if take_all==1, then we can take the gained seeds
        if (take_all == 1)
        {
            while ((current_case > 5) && ((player == 1 && (game->player_two[current_case - 6] == 2 || game->player_two[current_case - 6] == 3)) || (player == 2 && (game->player_one[current_case - 6] == 2 || game->player_one[current_case - 6] == 3))))
            {
                if (player == 1)
                {
                    game->score_one += game->player_two[current_case - 6];
                    game->player_two[current_case - 6] = 0;
                }
                else
                {
                    game->score_two += game->player_one[current_case - 6];
                    game->player_one[current_case - 6] = 0;
                }
                current_case--;
            }
        }
    }
    game->turn = (player == 1) ? 2 : 1;
}

// Ask Player for a move and play it
//*****************************
void ask_player(Awale* game, int player)
//*****************************
{
    int i = 0;
    int move;

    // Check if the game is not finished
    if (player == 1 && game->player_two[0] + game->player_two[1] + game->player_two[2] + game->player_two[3] + game->player_two[4] + game->player_two[5] == 0)
    {
        while ((i + game->player_one[i] < 6) && (i < 6)) i++; // Check if we can give seeds
        if (i == 6)
        {
            game->finished = 1;
            return; // Game over
        }
    }
    else if (player == 2 && game->player_one[0] + game->player_one[1] + game->player_one[2] + game->player_one[3] + game->player_one[4] + game->player_one[5] == 0)
    {
        while ((i + game->player_two[i] < 6) && (i < 6)) i++; // Check if we can give seeds
        if (i == 6)
        {
            game->finished = 1;
            return; // Game over
        }
    }

    printf("Your move: ");
    scanf("%d", &move);

    // Move between 1 and 6
    while ((move < 1) || (move > 6))
    {
        printf("Enter a number between 1 and 6: ");
        scanf("%d", &move);
    }

    // Play a non-empty case
    while ((player == 1 && game->player_one[move - 1] == 0) || (player == 2 && game->player_two[move - 1] == 0))
    {
        printf("You must play a non-empty case: ");
        scanf("%d", &move);
    }

    // Check if the opponent has no seeds
    if (player == 1 && game->player_two[0] + game->player_two[1] + game->player_two[2] + game->player_two[3] + game->player_two[4] + game->player_two[5] == 0)
        while (move + game->player_one[move - 1] < 7)
        {
            printf("You must give seeds! (stingy): ");
            scanf("%d", &move);
        }
    else if (player == 2 && game->player_one[0] + game->player_one[1] + game->player_one[2] + game->player_one[3] + game->player_one[4] + game->player_one[5] == 0)
        while (move - 1 + game->player_two[move - 1] < 6)
        {
            printf("You must give seeds! (stingy): ");
            scanf("%d", &move);
        }

    make_move(game, move - 1, player);
}

// Assign the remaining seeds when the game is over
//******************************
void finish_game(Awale* game)
//******************************
{
    int i;
    game->score_one += game->player_one[0] + game->player_one[1] + game->player_one[2] + game->player_one[3] + game->player_one[4] + game->player_one[5];
    game->score_two += game->player_two[0] + game->player_two[1] + game->player_two[2] + game->player_two[3] + game->player_two[4] + game->player_two[5];
    for (i = 0; i < 6; i++)
    {
        game->player_one[i] = 0;
        game->player_two[i] = 0;
    }
}

// Display the winner
//************************************
void display_winner(Awale game)
//************************************
{
    if (game.score_one > game.score_two)
        printf("The winner is Player One!\n Congratulations!\n\n");
    else if (game.score_one < game.score_two)
        printf("The winner is Player Two!\n Congratulations.\n\n");
    else
        printf("It's a draw!\n Good job to both players.\n\n");
}


void run_game(Awale* game)
//*********************************
{
    int i;

    display(*game);
    while (game->finished == 0)
    {
        ask_player(game, game->turn);
        display(*game);
    }
    finish_game(game);
    display_winner(*game);
}

int main()
{
    Awale awale = { {4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4}, 0, 0, 1, 0 };
    run_game(&awale);
    return 0;
}
