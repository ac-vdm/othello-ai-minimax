/* vim: :se ai :se sw=4 :se ts=4 :se sts :se et */

/*H**********************************************************************
 *
 *    This is a skeleton to guide development of Othello engines that can be used
 *    with the Ingenious Framework and a Tournament Engine.
 *
 *    The communication with the referee is handled by an implementaiton of comms.h,
 *    All communication is performed at rank 0.
 *
 *    Board co-ordinates for moves start at the top left corner of the board i.e.
 *    if your engine wishes to place a piece at the top left corner,
 *    the "gen_move_master" function must return "00".
 *
 *    The match is played by making alternating calls to each engine's
 *    "gen_move_master" and "apply_opp_move" functions.
 *    The progression of a match is as follows:
 *        1. Call gen_move_master for black player
 *        2. Call apply_opp_move for white player, providing the black player's move
 *        3. Call gen move for white player
 *        4. Call apply_opp_move for black player, providing the white player's move
 *        .
 *        .
 *        .
 *        N. A player makes the final move and "game_over" is called for both players
 *
 *    IMPORTANT NOTE:
 *        Write any (debugging) output you would like to see to a file.
 *        	- This can be done using file fp, and fprintf()
 *        	- Don't forget to flush the stream
 *        	- Write a method to make this easier
 *        In a multiprocessor version
 *        	- each process should write debug info to its own file
 *H***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <mpi.h>
#include <time.h>
#include <assert.h>
#include "comms.h"
#include <limits.h>

const int EMPTY = 0;
const int BLACK = 1;
const int WHITE = 2;
const int OUTER = 3;

const int ALLDIRECTIONS[8] = {-11, -10, -9, -1, 1, 9, 10, 11};
const int BOARDSIZE = 100;

const int LEGALMOVSBUFSIZE = 65;
const char piecenames[4] = {'.', 'b', 'w', '?'};

const double TIME_OFFSET = 0.3; // variable used in time calculation
const int DEPTH = 5;			// Depth of the minimax algorithm

/////////////////////constants used in the stability evaluation of the minmax algorithm
const int CORNER_WEIGHT = 4;
const int EDGE_WEIGHT = 2;
const int INTERIOR_WEIGHT = 1;
/////////////////////

void run_master(int argc, char *argv[]);
int initialise_master(int argc, char *argv[], int *time_limit, int *my_colour, FILE **fp);
void gen_move_master(char *move, int my_colour, FILE *fp);
void apply_opp_move(char *move, int my_colour, FILE *fp);
void game_over(void);
void run_worker(int rank);
void initialise_board(void);
void free_board(void);

void legal_moves(int player, int *moves, FILE *fp);
int legalp(int move, int player, FILE *fp);
int validp(int move);
int would_flip(int move, int dir, int player, FILE *fp);
int opponent(int player, FILE *fp);
int find_bracket_piece(int square, int dir, int player, FILE *fp);
void make_move(int move, int player, FILE *fp);
void make_flips(int move, int dir, int player, FILE *fp);
int get_loc(char *movestring);
void get_move_string(int loc, char *ms);
void print_board(FILE *fp);
char nameof(int piece);
int count(int player, int *board);

int minimax(int loc, int my_colour, int depth, int alpha, int beta, int MaximisingPlayer);
int updated_evaluation(int my_colour); // updated version of evaluate_board function for better decision making in the minimax algorithm
int min(int x, int y);
int max(int x, int y);

int *board;
/////////////////////used in debugging the program
FILE *fptr_debug0;
FILE *fptr_debug1;
FILE *fptr_debug2;
FILE *fptr_debug3;
/////////////////////

int nr_of_procs; // global variable to store number of || processes
int time_limit;
double start_time; // variable used in time calculation

int main(int argc, char *argv[])
{
	int rank;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nr_of_procs); // get the number of parallel processes
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);		 // get the id of each || process

	fptr_debug0 = fopen("debug0.txt", "w");
	fptr_debug1 = fopen("debug1.txt", "w");
	fptr_debug2 = fopen("debug2.txt", "w");
	fptr_debug3 = fopen("debug3.txt", "w");

	initialise_board(); // one for each process

	if (rank == 0)
	{
		run_master(argc, argv);
	}
	else
	{
		run_worker(rank);
	}
	game_over();
}

void run_master(int argc, char *argv[])
{
	// fprintf(fptr_debug0, "Hello from Proc 0\n");

	char cmd[CMDBUFSIZE];
	char my_move[MOVEBUFSIZE];
	char opponent_move[MOVEBUFSIZE];
	int my_colour;
	int running = 0;
	FILE *fp = NULL;

	if (initialise_master(argc, argv, &time_limit, &my_colour, &fp) != FAILURE)
	{
		running = 1;
	}
	if (my_colour == EMPTY)
		my_colour = BLACK;

	/*Broadcast my_colour to every process*/
	MPI_Bcast(&my_colour, 1, MPI_INT, 0, MPI_COMM_WORLD);

	while (running == 1)
	{
		/* Receive next command from referee */
		if (comms_get_cmd(cmd, opponent_move) == FAILURE)
		{
			fprintf(fp, "Error getting cmd\n");
			fflush(fp);
			running = 0;
			break;
		}

		/* Received game_over message */
		if (strcmp(cmd, "game_over") == 0)
		{
			running = 0;
			fprintf(fp, "Game over\n");
			fflush(fp);
			break;

			/* Received gen_move message */
		}
		else if (strcmp(cmd, "gen_move") == 0)
		{
			//*Broadcast running to every process*/
			MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);

			//*Broadcast board to every process*/
			MPI_Bcast(board, BOARDSIZE, MPI_INT, 0, MPI_COMM_WORLD);

			gen_move_master(my_move, my_colour, fp);

			print_board(fp);

			if (comms_send_move(my_move) == FAILURE)
			{
				running = 0;
				fprintf(fp, "Move send failed\n");
				fflush(fp);
				break;
			}

			/* Received opponent's move (play_move mesage) */
		}
		else if (strcmp(cmd, "play_move") == 0)
		{
			apply_opp_move(opponent_move, my_colour, fp);
			print_board(fp);

			/* Received unknown message */
		}
		else
		{
			fprintf(fp, "Received unknown command from referee\n");
		}
	}

	if (running == 0)
	{
		fclose(fptr_debug0);
		fclose(fptr_debug1);
		fclose(fptr_debug2);
		fclose(fptr_debug3);
	}

	//*Broadcast running to every process*/
	MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);
}

int initialise_master(int argc, char *argv[], int *time_limit, int *my_colour, FILE **fp)
{
	int result = FAILURE;

	if (argc == 5)
	{
		unsigned long ip = inet_addr(argv[1]);
		int port = atoi(argv[2]);
		*time_limit = atoi(argv[3]);

		*fp = fopen(argv[4], "w");
		if (*fp != NULL)
		{
			fprintf(*fp, "Initialise communication and get player colour \n");
			if (comms_init_network(my_colour, ip, port) != FAILURE)
			{
				result = SUCCESS;
			}
			fflush(*fp);
		}
		else
		{
			fprintf(stderr, "File %s could not be opened", argv[4]);
		}
	}
	else
	{
		fprintf(*fp, "Arguments: <ip> <port> <time_limit> <filename> \n");
	}

	return result;
}

void initialise_board(void)
{
	int i;
	board = (int *)malloc(BOARDSIZE * sizeof(int));
	for (i = 0; i <= 9; i++)
		board[i] = OUTER;
	for (i = 10; i <= 89; i++)
	{
		if (i % 10 >= 1 && i % 10 <= 8)
			board[i] = EMPTY;
		else
			board[i] = OUTER;
	}
	for (i = 90; i <= 99; i++)
		board[i] = OUTER;
	board[44] = WHITE;
	board[45] = BLACK;
	board[54] = BLACK;
	board[55] = WHITE;
}

void free_board(void)
{
	free(board);
}

/**
 *   Rank i (i != 0) executes this code
 *   ----------------------------------
 *   Called at the start of execution on all ranks except for rank 0.
 *   - run_worker should play minimax from its move(s)
 *   - results should be send to Rank 0 for final selection of a move
 */
void run_worker(int rank)
{
	/////////////////////used in debugging the program
	switch (rank)
	{
	case 1:
		fprintf(fptr_debug1, "Hello from Proc 1\n");
		break;
	case 2:
		fprintf(fptr_debug2, "Hello from Proc 2\n");
		break;
	case 3:
		fprintf(fptr_debug3, "Hello from Proc 3\n");
		break;
	}
	/////////////////////

	int running = 0;
	int my_colour;
	FILE *fp = NULL;
	int my_loc = -1;
	int my_score = -10000000;
	int max_score = -10000000;
	int max_loc = -1;
	int *legalmoves = (int *)malloc(LEGALMOVSBUFSIZE * sizeof(int));
	memset(legalmoves, 0, LEGALMOVSBUFSIZE);
	int *best_scores = (int *)malloc(nr_of_procs * sizeof(int));
	int *best_locs = (int *)malloc(nr_of_procs * sizeof(int));
	int *prev_board;

	/*broadcast colour*/
	MPI_Bcast(&my_colour, 1, MPI_INT, 0, MPI_COMM_WORLD);
	/*broadcast running*/
	MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);

	while (running == 1)
	{
		/*broadcast the board*/
		MPI_Bcast(board, BOARDSIZE, MPI_INT, 0, MPI_COMM_WORLD);

		/*generate a move*/
		legal_moves(my_colour, legalmoves, fp);

		if (legalmoves[0] > 0)
		{
			for (int i = 1; i <= legalmoves[0]; i++)
			{
				if ((i % nr_of_procs) == rank)
				{
					my_loc = legalmoves[i];

					/////////////////////used in debugging the program
					// switch (rank)
					// {
					// case 1:
					// 	printf("Proc 1 has received loc: %d\n", my_loc);
					// 	break;
					// case 2:
					// 	printf("Proc 2 has received loc: %d\n", my_loc);
					// 	break;
					// case 3:
					// 	printf("Proc 3 has received loc: %d\n", my_loc);
					// 	break;
					// }
					/////////////////////

					start_time = MPI_Wtime();

					prev_board = (int *)malloc(BOARDSIZE * sizeof(int));
					memcpy(prev_board, board, BOARDSIZE * sizeof(int));

					make_move(my_loc, my_colour, fp);

					my_score = minimax(my_loc, my_colour, DEPTH, INT_MIN, INT_MAX, 1);

					memcpy(board, prev_board, BOARDSIZE * sizeof(int));

					if (my_score > max_score)
					{
						max_score = my_score;
						max_loc = my_loc;
					}
				}
				MPI_Bcast(board, BOARDSIZE, MPI_INT, 0, MPI_COMM_WORLD);
			}
		}
		else
		{
			max_score = -1;
			max_loc = -1;
		}

		/////////////////////used in debugging the program
		// switch (rank)
		// {
		// case 1:
		// 	printf("Proc 1 has max score %d at loc %d\n", max_score, max_loc);
		// 	break;
		// case 2:
		// 	printf("Proc 2 has max score %d at loc %d\n", max_score, max_loc);
		// 	break;
		// case 3:
		// 	printf("Proc 3 has max score %d at loc %d\n", max_score, max_loc);
		// 	break;
		// }
		/////////////////////

		/*gather all options for best score at master process*/
		MPI_Gather(&max_score, 1, MPI_INT, best_scores, 1, MPI_INT, 0, MPI_COMM_WORLD);
		/*gather all locations for the best score options at master process*/
		MPI_Gather(&max_loc, 1, MPI_INT, best_locs, 1, MPI_INT, 0, MPI_COMM_WORLD);

		/////////////////////reinitialise variables that will be reused when the while loop continues, and the historic value should NOT be remembered*/
		max_loc = -1;
		max_score = -100000;
		/////////////////////

		/*Broadcast running*/
		MPI_Bcast(&running, 1, MPI_INT, 0, MPI_COMM_WORLD);
	}
}

/**
 *  Rank 0 executes this code:
 *  --------------------------
 *  Called when the next move should be generated
 *  - gen_move_master should play minimax from its move(s)
 *  - the ranks may communicate during execution
 *  - final results should be gathered at rank 0 for final selection of a move
 */
void gen_move_master(char *move, int my_colour, FILE *fp)
{
	int my_score;
	int my_loc;
	int max_score = -1;
	int max_loc = -1;
	int overall_best_score = -100000;
	int overall_best_loc = -1;
	int *legalmoves = (int *)malloc(LEGALMOVSBUFSIZE * sizeof(int));
	memset(legalmoves, 0, LEGALMOVSBUFSIZE);
	int *best_scores = (int *)malloc(nr_of_procs * sizeof(int));
	int *best_locs = (int *)malloc(nr_of_procs * sizeof(int));
	int *prev_board;

	/* generate move */
	legal_moves(my_colour, legalmoves, fp);

	if (legalmoves[0] > 0)
	{
		for (int i = 1; i <= legalmoves[0]; i++)
		{
			// printf("legal move %d: %d\n", i, legalmoves[i]);
			if ((i % nr_of_procs) == 0)
			{
				my_loc = legalmoves[i];
				// printf("Process 0 received legalmove %d\n", my_loc);

				start_time = MPI_Wtime();

				prev_board = (int *)malloc(BOARDSIZE * sizeof(int));
				memcpy(prev_board, board, BOARDSIZE * sizeof(int));

				make_move(my_loc, my_colour, fp);

				my_score = minimax(my_loc, my_colour, DEPTH, INT_MIN, INT_MAX, 1);

				memcpy(board, prev_board, BOARDSIZE * sizeof(int));

				if (my_score > max_score)
				{
					max_score = my_score;
					max_loc = my_loc;
				}
			}
			MPI_Bcast(board, BOARDSIZE, MPI_INT, 0, MPI_COMM_WORLD);
		}
	}
	else
	{
		max_score = -1;
		max_loc = -1;
	}

	// printf("Proc 0 has max score %d at loc %d\n", max_score, max_loc);

	/*gather all options for best score at master process*/
	MPI_Gather(&max_score, 1, MPI_INT, best_scores, 1, MPI_INT, 0, MPI_COMM_WORLD);

	/*gather all locations for the best score options at master process*/
	MPI_Gather(&max_loc, 1, MPI_INT, best_locs, 1, MPI_INT, 0, MPI_COMM_WORLD);

	// for (int i = 0; i < nr_of_procs; i++)
	// {
	// 	printf("Proc 0 received max score %d at loc %d from rank %d\n", best_scores[i], best_locs[i], i);
	// }

	for (int j = 0; j < nr_of_procs; j++)
	{
		if (best_locs[j] != -1) /////////////////////used in debugging the program first check if is  legal move
		{
			if (best_scores[j] > overall_best_score)
			{
				overall_best_score = best_scores[j];
				overall_best_loc = best_locs[j];
			}
		}
	}

	// printf("move to make : %d\n", overall_best_loc);
	// printf("i am here =================================================\n");

	if (overall_best_loc == -1)
	{
		// printf("only option is to pass\n");
		overall_best_score = -100000;
		overall_best_loc = -1;
		strncpy(move, "pass\n", MOVEBUFSIZE);
	}
	else if (overall_best_loc != -1)
	{
		/* apply move */
		get_move_string(overall_best_loc, move);
		// printf("MOVE STRING %s", move);

		make_move(overall_best_loc, my_colour, fp);

		overall_best_score = -100000;
		overall_best_loc = -1;
	}
}

void apply_opp_move(char *move, int my_colour, FILE *fp)
{
	int loc;
	if (strcmp(move, "pass\n") == 0)
	{
		return;
	}
	loc = get_loc(move);
	make_move(loc, opponent(my_colour, fp), fp);
}

void game_over(void)
{
	free_board();
	MPI_Finalize();
}

void get_move_string(int loc, char *ms)
{
	int row, col, new_loc;
	new_loc = loc - (9 + 2 * (loc / 10));
	row = new_loc / 8;
	col = new_loc % 8;
	ms[0] = row + '0';
	ms[1] = col + '0';
	ms[2] = '\n';
	ms[3] = 0;
}

int get_loc(char *movestring)
{
	int row, col;
	/* movestring of form "xy", x = row and y = column */
	row = movestring[0] - '0';
	col = movestring[1] - '0';
	return (10 * (row + 1)) + col + 1;
}

void legal_moves(int player, int *moves, FILE *fp)
{
	int move, i;
	moves[0] = 0;
	i = 0;
	for (move = 11; move <= 88; move++)
	{
		if (legalp(move, player, fp))
		{
			i++;
			moves[i] = move;
		}
	}
	moves[0] = i;
}

int legalp(int move, int player, FILE *fp)
{
	int i;
	if (!validp(move))
		return 0;
	if (board[move] == EMPTY)
	{
		i = 0;
		while (i <= 7 && !would_flip(move, ALLDIRECTIONS[i], player, fp))
			i++;
		if (i == 8)
			return 0;
		else
			return 1;
	}
	else
		return 0;
}

int validp(int move)
{
	if ((move >= 11) && (move <= 88) && (move % 10 >= 1) && (move % 10 <= 8))
		return 1;
	else
		return 0;
}

int would_flip(int move, int dir, int player, FILE *fp)
{
	int c;
	c = move + dir;
	if (board[c] == opponent(player, fp))
		return find_bracket_piece(c + dir, dir, player, fp);
	else
		return 0;
}

int find_bracket_piece(int square, int dir, int player, FILE *fp)
{
	while (validp(square) && board[square] == opponent(player, fp))
		square = square + dir;
	if (validp(square) && board[square] == player)
		return square;
	else
		return 0;
}

int opponent(int player, FILE *fp)
{
	if (player == BLACK)
		return WHITE;
	if (player == WHITE)
		return BLACK;
	fprintf(fp, "illegal player\n");
	return EMPTY;
}

void make_move(int move, int player, FILE *fp)
{
	int i;
	board[move] = player;
	for (i = 0; i <= 7; i++)
		make_flips(move, ALLDIRECTIONS[i], player, fp);
}

void make_flips(int move, int dir, int player, FILE *fp)
{
	int bracketer, c;
	bracketer = would_flip(move, dir, player, fp);
	if (bracketer)
	{
		c = move + dir;
		do
		{
			board[c] = player;
			c = c + dir;
		} while (c != bracketer);
	}
}

void print_board(FILE *fp)
{
	int row, col;
	fprintf(fp, "   1 2 3 4 5 6 7 8 [%c=%d %c=%d]\n",
			nameof(BLACK), count(BLACK, board), nameof(WHITE), count(WHITE, board));
	for (row = 1; row <= 8; row++)
	{
		fprintf(fp, "%d  ", row);
		for (col = 1; col <= 8; col++)
			fprintf(fp, "%c ", nameof(board[col + (10 * row)]));
		fprintf(fp, "\n");
	}
	fflush(fp);
}

char nameof(int piece)
{
	assert(0 <= piece && piece < 5);
	return (piecenames[piece]);
}

int count(int player, int *board)
{
	int i, cnt;
	cnt = 0;
	for (i = 1; i <= 88; i++)
		if (board[i] == player)
			cnt++;
	return cnt;
}

/*
	Function runs the recursive minimax algorithm with alpha beta pruning.
	Parameters:
		loc - the current move that is being played.
		depth - depth of the search into the tree.
		alpha - the alpha value used in the pruning.
		beta - the beta value used in the pruning
	Returns:
		Result - Score based on the evalaution function and minimax algorithm.
*/
int minimax(int loc, int my_colour, int depth, int alpha, int beta, int MaximisingPlayer)
{
	FILE *fp = NULL;
	int best_score = -1;
	int child_score;
	int *childMoves = (int *)malloc(LEGALMOVSBUFSIZE * sizeof(int));
	int *original_board;
	int time_elapsed = 0;
	int result;

	time_elapsed = MPI_Wtime() - start_time;

	if (depth == 0 || loc == -1 || time_elapsed >= (time_limit - TIME_OFFSET))
	{
		result = updated_evaluation(my_colour);
		// memcpy(board, original_board, BOARDSIZE * sizeof(int));
		return result;
	}

	if (MaximisingPlayer)
	{
		best_score = INT_MIN;
		legal_moves(my_colour, childMoves, fp);
		if (childMoves[0] == 0)
		{
			result = updated_evaluation(my_colour);
			return result;
		}
		else
		{
			for (int i = 1; i <= childMoves[0]; i++)
			{
				original_board = (int *)malloc(BOARDSIZE * sizeof(int));
				memcpy(original_board, board, BOARDSIZE * sizeof(int));
				make_move(childMoves[i], my_colour, fp);

				child_score = minimax(childMoves[i], my_colour, depth - 1, alpha, beta, 0);
				best_score = max(child_score, best_score);
				alpha = max(alpha, child_score);
				memcpy(board, original_board, BOARDSIZE * sizeof(int));
				if (beta <= alpha)
				{
					break;
				}
			}
			free(childMoves);
			free(original_board);

			return best_score;
		}
	}
	else if (!MaximisingPlayer)
	{
		best_score = INT_MAX;
		legal_moves(opponent(my_colour, fp), childMoves, fp);
		if (childMoves[0] == 0)
		{
			result = updated_evaluation(my_colour);
			return result;
		}
		else
		{
			for (int i = 1; i <= childMoves[0]; i++)
			{
				original_board = (int *)malloc(BOARDSIZE * sizeof(int));
				memcpy(original_board, board, BOARDSIZE * sizeof(int));
				make_move(childMoves[i], opponent(my_colour, fp), fp);

				child_score = minimax(childMoves[i], my_colour, depth - 1, alpha, beta, 1);
				best_score = min(child_score, best_score);
				beta = min(beta, child_score);
				memcpy(board, original_board, BOARDSIZE * sizeof(int));
				if (beta <= alpha)
				{
					break;
				}
			}
			free(original_board);
			free(childMoves);

			return best_score;
		}
	}

	return -1;
}

/*
   Functions takes in 2 numbers and returns the maximum between them.
*/
int max(int x, int y)
{
	if (x >= y)
	{
		return x;
	}
	else
	{
		return y;
	}
}

/*
   Functions takes in 2 numbers and returns the minimum between them.
*/
int min(int x, int y)
{
	if (x <= y)
	{
		return x;
	}
	else
	{
		return y;
	}
}

int updated_evaluation(int my_colour)
{
	FILE *fp = NULL;
	int my_count;
	int opp_count;
	int coin_parity = 0;
	int my_moves;
	int opp_moves;
	int mobility_heuristic = 0;
	int my_stability = 0;
	int opp_stability = 0;
	int stability_heuristic = 0;
	int my_corners = 0;
	int opp_corners = 0;
	int corner_heuristic = 0;
	int my_edges = 0;
	int opp_edges = 0;
	int edges_heuristic = 0;
	int i;
	int *moves = (int *)malloc(LEGALMOVSBUFSIZE * sizeof(int));
	memset(moves, 0, LEGALMOVSBUFSIZE);

	//////////////////////////*Coin parity*/
	my_count = count(BLACK, board);
	opp_count = count(WHITE, board);

	coin_parity = 100 * (my_count - opp_count) / (my_count + opp_count);
	//////////////////////////

	//////////////////////////*Mobility heuristic*/
	legal_moves(my_colour, moves, fp);
	my_moves = moves[0];
	legal_moves(opponent(my_colour, fp), moves, fp);
	opp_moves = moves[0];
	if (my_moves > opp_moves)
		mobility_heuristic = (100.0 * my_moves) / (my_moves + opp_moves);
	else if (my_moves < opp_moves)
		mobility_heuristic = -(100.0 * opp_moves) / (my_moves + opp_moves);
	free(moves);
	//////////////////////////

	int s_weights[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 4, -3, 2, 2, 2, 2, -3, 4, 0,
		0, -3, -4, -1, -1, -1, -1, -4, -3, 0,
		0, 2, -1, 1, 0, 0, 1, -1, 2, 0,
		0, 2, -1, 0, 1, 1, 0, -1, 2, 0,
		0, 2, -1, 0, 1, 1, 0, -1, 2, 0,
		0, 2, -1, 1, 0, 0, 1, -1, 2, 0,
		0, -3, -4, -1, -1, -1, -1, -4, -3, 0,
		0, 4, -3, 2, 2, 2, 2, -3, 4, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	for (i = 0; i < BOARDSIZE; i++)
	{
		if (i == 11 || i == 18 || i == 81 || i == 88)
		{
			if (board[i] == my_colour)
			{
				my_corners = my_corners + 11;
				my_stability += (s_weights[i]) * CORNER_WEIGHT;
			}
			else if (board[i] == opponent(my_colour, fp))
			{
				opp_corners = opp_corners + 11;
				opp_stability += (s_weights[i]) * CORNER_WEIGHT;
			}
		}
		else if (i % 10 == 1 || i % 10 == 8)
		{
			if (board[i] == my_colour)
			{
				my_edges = my_edges + 6;
				my_stability += (s_weights[i]) * EDGE_WEIGHT;
			}
			else if (board[i] == opponent(my_colour, fp))
			{
				opp_edges = opp_edges + 6;
				opp_stability += (s_weights[i]) * EDGE_WEIGHT;
			}
		}
		else if ((i > 11 && i < 18) || (i > 81 && i < 88))
		{
			if (board[i] == my_colour)
			{
				my_edges = my_edges + 6;
				my_stability += (s_weights[i]) * EDGE_WEIGHT;
			}
			else if (board[i] == opponent(my_colour, fp))
			{
				opp_edges = opp_edges + 6;
				opp_stability += (s_weights[i]) * EDGE_WEIGHT;
			}
		}
		else
		{
			if (board[i] == my_colour)
			{
				my_stability += (s_weights[i]) * INTERIOR_WEIGHT;
			}
			else if (board[i] == opponent(my_colour, fp))
			{
				opp_stability += (s_weights[i]) * INTERIOR_WEIGHT;
			}
		}
	}

	if ((my_stability + opp_stability) != 0)
	{
		stability_heuristic = 100 * (my_stability - opp_stability) / (my_stability + opp_stability);
	}

	if ((my_corners + opp_corners) != 0)
	{
		corner_heuristic = 100 * (my_corners - opp_corners) / (my_corners + opp_corners);
	}

	if ((my_edges + opp_edges) != 0)
	{
		edges_heuristic = 100 * (my_edges - opp_edges) / (my_edges + opp_edges);
	}

	int heuristic_eval = coin_parity + mobility_heuristic + stability_heuristic + corner_heuristic + edges_heuristic;
	return heuristic_eval;
}
