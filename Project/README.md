# Awale Game
This code implements the game of Awale.

Awale is a traditional African board game played with a wooden board and seeds. The board consists of 12 holes, with 6 holes on each side. Each hole initially contains a certain number of seeds.

The rules of Awale are as follows:
1. Players take turns to pick up all the seeds from one of their holes and distribute them in a counter-clockwise direction, one seed per hole.
2. If the last seed is dropped into an empty hole on the player's side, and the opposite hole on the opponent's side contains seeds, the player captures all the seeds in the opponent's hole and the last seed dropped.
3. The game ends when one player has no more seeds on their side of the board.
4. The player with the most seeds in their store (the two holes at the ends of the board) wins the game.


## Creators
- YAGOUBI Hichem
- VENEGAS Josue


## Usage
1. In the root folder, open a terminal and run `make`.
2. Run `./serverfile` for the server. ( the server is also running on a VPS 165.22.66.251) there is for sure a lot of buffer overflows so don't hack me please :)
3. Run `./clientfile 0.0.0.0 <<username>>` for each client.
   Run `./clientfile 165.22.66.251 <<username>>` for each client.
4. In the program, you can run `/help_1` and `/help_2` to see the list of available commands.
5. After finishing, clean the executable files with `make clean`.
