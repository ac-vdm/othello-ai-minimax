# othello-ai-minimax
AI player, for Othello boardgame, employs the Minimax algorithm with alpha-beta pruning to make strategic decisions. It is designed to run against a random player, with random moves. The AI utilizes MPI for parallelizing the decision-making process, which significantly enhances performance by distributing computations across multiple processors.

NB The IngeniousFramework.jar and the `Logs/` directory should be inside the `IngeniousFrame/` directory 
--------------------------------------------------------------------------------------------------------
The IngeniousFrame Jar uses relative paths to find the `Logs/` directory inside the `IngeniousFrame/` directory, i.e. the `Logs/` directory and the IngeniousFramework.jar file should reside inside the `IngeniousFrame/` directory.

Building players and running matches
------------------------------------ 
Executing: `python3 run_rr.py` 
1. builds the players by calling make inside the `src_random_player/` and `src_my_player/` directories respectively
 - Note that the executables are stored in the `players/` directory 
 - and the output files in `src_random_player/obj/` and `player_min/obj/` are deleted
2. and runs a tournament where `my_player` plays two matches against every other player (one where it makes the first move and one where it makes the second move) 
