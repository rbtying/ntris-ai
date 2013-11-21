#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <utility>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <sys/time.h>
#include <ctime>

#include "choose_move.h"
#include "dropblox_ai.h"

/*!
 * Loads weights from a file, in
 *
 * key=val
 *
 * format.
 */
void ScoreVector::LoadWeightsFromFile(std::string fname) {
    std::ifstream f;
    std::string line;

    f.open(fname.c_str());

    while (std::getline(f, line)) {
        std::stringstream parse(line);
        double value;
        std::string key;
        parse >> key >> value;

        weights[key] = value;
    }

    f.close();
}

/*!
 * Writes weights to a file, in
 *
 * key=val
 *
 * format.
 */
void ScoreVector::WriteWeightsToFile(std::string fname) {
    std::ofstream f;

    f.open(fname.c_str());

    // write all items to file
    for (std::map<std::string, double>::iterator iter = weights.begin(); iter != weights.end(); ++iter) {
        std::string key = iter->first;
        double weight = iter->second;
        f << key << " " << weight << std::endl;
    }

    f.close();
}

/*!
 * Converts a move_t into its string form
 *
 * @param m the move to convert
 *
 * @return the string form of the move
 */
std::string StringifyMove(const move_t& m) {
    switch(m) {
        case LEFT:
            return "left";
        case RIGHT:
            return "right";
        case UP:
            return "up";
        case DOWN:
            return "down";
        case ROTATE:
            return "rotate";
    }
    return "INVALID";
}


/*!
 * Generates the valid moves in this board
 *
 * @param board the initial starting board position
 *
 * @return a list of lists of possible moves
 */
std::vector<std::vector<move_t> > GenerateValidMoves(Board* board) {
    std::vector<std::vector<move_t> > permutations(48);

    // use naive drop approach for now
    // go from rotate, then go from left to right
    Block *block = board->block;

    Board b(*board);

    for (int rot = 0; rot < 4; rot++) {
        for (int j = 0; j < board->cols; j++) {
            vector<move_t> moves;
            block->reset_position();
            move_t dx_move;
            int dx = 0;

            if (block->center.j > j) {
                dx_move = LEFT;
                dx = block->center.j - j;
            } else {
                dx_move = RIGHT;
                dx = j - block->center.j;
            }

            for (int x = 0; x < dx; x++) {
                bool moved = false;

                // attempt rotation if not already there
                int dr = rot - (block->rotation % 4);
                for (int r = 0; r < dr; r++) {
                    if (block->checked_rotate(b)) {
                        moves.push_back(ROTATE);
                        moved = true;
                    }
                }

                if (dx_move == LEFT) {
                    if (block->checked_left(b)) {
                        moves.push_back(LEFT);
                        moved = true;
                    }
                } else if (dx_move == RIGHT) {
                    if (block->checked_right(b)) {
                        moves.push_back(RIGHT);
                        moved = true;
                    }
                }

                if (!moved) {
                    // try going down?
                    if (block->checked_down(b)) {
                        moves.push_back(DOWN);
                    }
                }
            }

            // if (block->center.j > j) {
            //     int dx = block->center.j - j;
            //     for (int left = 0; left < dx; left++) {
            //         block->left();
            //         if (board->check(*block)) {
            //             moves.push_back(LEFT);
            //         } else {
            //             block->right();
            //         }
            //     }
            // } else if (block->center.j < j) {
            //     int dx = j - block->center.j;
            //     for (int right = 0; right < dx; right++) {
            //         block->right();
            //         if (board->check(*block)) {
            //             moves.push_back(RIGHT);
            //         } else {
            //             block->left();
            //         }
            //     }
            // }

            if (board->check(*block)) {
                permutations.push_back(moves);
            }
        }
    }
    block->reset_position();

    return permutations;
}

/*!
 * Attempts to find the best move given a board
 *
 * @param a board to find the best move for
 * @return the best sequence of moves
 */
std::vector<move_t> FindBestMove(Board* board, std::string config) {
    ScoreVector sv;
    sv.LoadWeightsFromFile(config);

    timeval a, b;

    gettimeofday(&a, 0);

    std::vector<vector<move_t> > permutations = GenerateValidMoves(board);

    int best = 0;

    double scores[permutations.size()];

// gcc parallelization
#pragma omp parallel for
    for (int i1 = 0; i1 < permutations.size(); i1++) {
        scores[i1] = -10e6;
        Board * ptr1 = board->do_commands(permutations[i1]);
        if (ptr1->check(*ptr1->block)) {
#define DO_LOOKAHEAD
#ifdef DO_LOOKAHEAD
            try {
                std::vector<vector<move_t> > perm2 = GenerateValidMoves(board->do_commands(permutations[i1]));
                for (int i2 = 0; i2 < perm2.size(); i2++) {
                    Board * ptr2 = ptr1->do_commands(perm2[i2]);
                    double score = sv.Score(ptr2);
                    if (score > scores[i1]) {
                        scores[i1] = score;
                    }
                }
            } catch (Exception& e) {
#endif
                double score = sv.Score(ptr1);
                if (score > scores[i1]) {
                    scores[i1] = score;
                }
#ifdef DO_LOOKAHEAD
            }
#endif
        }
    }

    int best_idx = 0;
    for (int i = 0; i < permutations.size(); i++) {
        if (scores[i] > scores[best_idx]) {
            best_idx = i;
        }
    }
    std::cerr << "best_score: " << scores[best_idx] << std::endl;

    gettimeofday(&b, 0);

    time_t mtime = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_usec - a.tv_usec) / 1000;
    std::cerr << "diff: " << mtime << std::endl;


    return permutations[best_idx];
}

/*!
 * Scores a board based on the weights we have.
 *
 * @param board the starting board
 * @param moves the moves to make
 */
double ScoreVector::Score(Board* board) {
    std::map<std::string, double> values;

    int max_height = 0;

    int heights[board->cols];
    int holes = 0;
    int edges_touching_block = 0;
    int edges_touching_wall = 0;
    int external_edges = 0;
    int gaps = 0;
    int points = 0;
    int block_height = 0;
    int covers = 0;
    int bumpiness = 0;

    Block *block = board->block;

    std::pair<int, int> edges = board->countedges(*board->block);

    if (edges.first < 0 && edges.second < 0) {
        return -10e6;
    }
   
    // initialize heights
    for (int j = 0; j < board->cols; j++) {
        heights[j] = board->cols;
    }

    int prev_height = -1;
    for (int j = 0; j < board->cols; j++) {
        bool found_height = false;
        int possible_covers = 0;
        int holes_in_this_col = 0;

        for (int i = 0; i < board->rows; i++) {
            if (!found_height) {
                // find max height of column
                if (board->bitmap[i][j]) {
                    heights[j] = board->rows - i;


                    if (heights[j] > max_height) {
                        max_height = heights[j];
                    }
                    found_height = true;
                }
            } else {
                // we are now below the max height of this column
                if (!board->bitmap[i][j]) {
                    // this must be a 'hole'
                    holes_in_this_col++;
                    /* holes += holes_in_this_col; */
                    holes++;

                    // so any blocks before must have covered it up
                    covers += possible_covers;
                } else {
                    possible_covers++;
                }
            }

            if (board->bitmap[i][j]) {
                block_height += board->rows - i;
            }
        }

        // we can get the number of gaps by checking decreasing height
        // from left to right
        if (heights[j] > prev_height) {
            gaps += heights[j] - prev_height;

            if (prev_height != -1) {
                bumpiness += heights[j] - prev_height;
            }
        } else {
            if (prev_height != -1) {
                bumpiness += prev_height - heights[j];
            }
        }
        prev_height = heights[j];
    }

    double score = 0.0;

    values["BLOCK_EDGES"] = max(edges.first, 0);
    values["WALL_EDGES"] = max(edges.second, 0);
    values["EXTERNAL_EDGES"] = max(edges.first + edges.second, 0);
    values["GAPS"] = gaps;
    values["HOLES"] = holes;
    values["MAX_HEIGHT"] = max_height;
    values["BLOCK_HEIGHT"] = block_height;
    points = (1 << board->rows_cleared) - 1;
    values["POINTS_EARNED"] = points;
    values["COVERS"] = covers;
    values["BUMPINESS"] = bumpiness;

    for (std::map<std::string, double>::iterator iter = values.begin(); iter != values.end(); ++iter) {
        std::string key = iter->first;
        double val = iter->second;
        if (weights.count(key) > 0) {
            score += val * weights[key];
        }
    }

    return score;
}
