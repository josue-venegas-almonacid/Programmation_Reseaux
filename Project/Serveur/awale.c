#include <stdio.h>
#include "awale.h"

#define CLEAR_SCREEN_ANSI "\033[H\033[J"
#define BUF_SIZE    1024

// Display the Awale board on the screen
//*************************************
char* display(Awale game) {
    static char buffer[BUF_SIZE];  // Static buffer to hold the message

    // Clear the screen ANSI code
    snprintf(buffer, BUF_SIZE, "%s", CLEAR_SCREEN_ANSI);

    // Append the rest of the message
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "\n\n\nTurn: %s\n\n", (game.turn == 1 ? "Player One" : "Player Two"));
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "   Player Two Score: %d\n", game.score_two);
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "   Player One Score: %d\n", game.score_one);
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "   ┌─────┬─────┬─────┬─────┬─────┬─────┐\n");
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "   │ %3d │ %3d │ %3d │ %3d │ %3d │ %3d │\n",
             game.player_two[5], game.player_two[4], game.player_two[3], game.player_two[2], game.player_two[1], game.player_two[0]);
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "   ├─────┼─────┼─────┼─────┼─────┼─────┤\n");
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "   │ %3d │ %3d │ %3d │ %3d │ %3d │ %3d │\n",
             game.player_one[0], game.player_one[1], game.player_one[2], game.player_one[3], game.player_one[4], game.player_one[5]);
    snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "   └─────┴─────┴─────┴─────┴─────┴─────┘\n");

    return buffer;
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
void check_move(Awale* game, int player, int move)
//*****************************
{
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

int is_game_over(Awale* game) {
    int i;
    // Check if player one has any seeds left
    for (i = 0; i < 6; i++) {
        if (game->player_one[i] > 0) {
            break;
        }
    }
    if (i == 6) {
        // Player one has no seeds left, game is over
        return 1;
    }

    // Check if player two has any seeds left
    for (i = 0; i < 6; i++) {
        if (game->player_two[i] > 0) {
            break;
        }
    }
    if (i == 6) {
        // Player two has no seeds left, game is over
        return 1;
    }

    // Both players have seeds left, game is not over
    return 0;
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
char* display_winner(Awale game) {
    static char buffer[BUF_SIZE];  // Static buffer to hold the message

    // Clear the screen ANSI code
    snprintf(buffer, BUF_SIZE, "%s", CLEAR_SCREEN_ANSI);

    // Append the winner message
    if (game.score_one > game.score_two)
        snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "The winner is Player One!\n Congratulations!\n\n");
    else if (game.score_one < game.score_two)
        snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "The winner is Player Two!\n Congratulations.\n\n");
    else
        snprintf(buffer + strlen(buffer), BUF_SIZE - strlen(buffer), "It's a draw!\n Good job to both players.\n\n");

    return buffer;
}


void run_game(Awale* game)
//*********************************
{
    int i;
    int move;

    //display(*game);

    char* displayMessage = display_winner(*game);
    // Print the message ( change with send to client)
    printf("%s", displayMessage);

    while (game->finished == 0)
    {
        if (is_game_over(game)) {
            game->finished = 1;
            break;
        }
        printf("Your move: ");
        scanf("%d", &move);
        check_move(game, game->turn, move);
        //display(*game);

        char* displayMessage = display(*game);
        // Print the message
        printf("%s", displayMessage);
    }
    finish_game(game);
    displayMessage = display_winner(*game);
    printf("%s", displayMessage);
}

/*int main()
{
    Awale awale = { {4, 4, 4, 4, 4, 4}, {4, 4, 4, 4, 4, 4}, 0, 0, 1, 0 };
    run_game(&awale);
    return 0;
}*/
