#include <string>
#include <vector>
#include <queue>
#include <map>
#include <set>
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
        case NO_MOVE:
            return "no_move";
    }
    return "INVALID";
}

pose_t apply_move(const pose_t& initial, const move_t& m) {
    pose_t end;
    end.i = initial.i;
    end.j = initial.j;
    end.rot = initial.rot;

    switch(m) {
        case LEFT:
            end.j -= 1;
            break;
        case RIGHT:
            end.j += 1;
            break;
        case UP:
            end.i -= 1;
            break;
        case DOWN:
            end.i += 1;
            break;
        case ROTATE:
            end.rot = (end.rot + 1) % 4;
            break;
        case NO_MOVE:
        default:
            break;
    }
    return end;
}

/*!
 * Generates the valid moves in this board
 *
 * @param board the initial starting board position
 *
 * @return a list of lists of possible moves
 */
std::vector<std::vector<move_t> > GenerateValidMoves(Board* board) {
    std::vector<std::vector<move_t> > permutations;

    // use naive drop approach for now
    // go from rotate, then go from left to right
    Block *block = board->block;

    Board b(*board);

    // this cache holds a mapping from
    // current_pose -> previous_pose, move
    // such that current_pose = apply_move(previous_pose, move)
    //
    // we can thus walk up current_pose->previous_pose to the initial pose,
    // and generate the move list by pushing the move to the front of the list.
    std::map<pose_t, std::pair<pose_t, move_t> > path_cache;

    // try left, right, down, rotate on each step
    move_t possible_moves[] = { ROTATE, LEFT, RIGHT, DOWN };
    int num_moves = 4;

    pose_t initial;
    initial.i = block->center.i;
    initial.j = block->center.j;
    initial.rot = block->rotation;

    std::queue<pose_t> toprocess;
    toprocess.push(initial);

    std::set<pose_t> end_positions;

    while (!toprocess.empty()) {
        pose_t p = toprocess.front();
        toprocess.pop();

        // set up next node
        for (int moveidx = 0; moveidx < num_moves; moveidx++) {
            move_t move = possible_moves[moveidx];
            pose_t next = apply_move(p, move);
            if (board->check(*block, next)) {
                // this is a valid position
                // memoize the paths. 
                if (!path_cache.count(next)) {
                    std::pair<pose_t, move_t> transition;
                    transition.first = p;
                    transition.second = move;
                    path_cache[next] = transition;

                    toprocess.push(next);
                }
            } else {
                if (move == DOWN && !end_positions.count(p)) {
                    // this block can't be moved down any further, so it must be
                    // a block of interest, i.e. a potential end state
                    end_positions.insert(p);
                }
            }
        }
    }
    for (std::set<pose_t>::iterator it = end_positions.begin(); it != end_positions.end(); ++it) {
        std::vector<move_t> moves;

        pose_t ptr = *it;
        bool successful = true;
        while (ptr != initial) {
            if (path_cache.count(ptr)) {
                std::pair<pose_t, move_t> transition = path_cache[ptr];
                pose_t parent = transition.first;
                move_t m = transition.second;
                moves.push_back(m);
                ptr = parent;
            } else {
                successful = false;
                break;
            }
        }
        if (successful) {
            std::reverse(moves.begin(), moves.end());
            // get rid of trailing DOWN
            while (moves.back() == DOWN) {
                moves.pop_back();
            }

            permutations.push_back(moves);
        } else {
            std::cerr << "Could not find path" << std::endl;
        }
    }

    std::cerr << "Permutations: " << permutations.size() << std::endl << std::flush;

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

        Board * ptr1;
        int landing_height;
        try {
            ptr1 = board->do_commands(permutations[i1]);

            std::pair<int, int> dim = board->block->dimensions();
            landing_height = board->block->center.j + dim.second / 2;

            double score = sv.Score(ptr1, landing_height);
            if (score > scores[i1]) {
                scores[i1] = score;
            }

        } catch (Exception& e) {
            // std::cerr << "Could not complete commands: " << std::endl;
            // for (int i = 0; i < permutations[i].size(); i++) {
            //     std::cerr << StringifyMove(permutations[i1][i]) << ", ";
            // }
            // std::cerr << std::endl;
            continue;
        }
/* #define DO_LOOKAHEAD */
#ifdef DO_LOOKAHEAD
        if (ptr1->check(*ptr1->block)) {
            scores[i1] = -1; // reset
                std::vector<vector<move_t> > perm2 = GenerateValidMoves(ptr1);
                for (int i2 = 0; i2 < perm2.size(); i2++) {
                    try {
                        Board * ptr2 = ptr1->do_commands(perm2[i2]);
                        double score = sv.Score(ptr2, landing_height);
                        if (score > scores[i1]) {
                            scores[i1] = score;
                        }
                    } catch (Exception& e) {
                        continue;
                    }
                }
        }
#endif
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
 * @param landing_height, set to <0 to recalculate.
 */
double ScoreVector::Score(Board* board, int landing_height) {
    std::map<std::string, double> values;

    int max_height = 0;

    int heights[board->cols];
    int holes = 0;
    int external_edges = 0;
    int gaps = 0;
    int points = 0;
    int block_height = 0;
    int covers = 0;
    int bumpiness = 0;
    bool find_landing_height = landing_height < 0;

    Block *block = board->block;

    std::pair<int, int> edges = board->countedges(*board->block);

    if (edges.first < 0 && edges.second < 0) {
        return -10e6;
    }

    if (find_landing_height) {
        std::pair<int, int> dim = block->dimensions();
        landing_height = block->center.j + dim.second / 2;
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

    int row_transitions = 0;
    int col_transitions = 0;
    int well_sums = 0;

    for (int j = 0; j < board->cols; j++) {
        bool has_a_roof = false;
        bool found_well = false;
        for (int i = 0; i < board->rows; i++) {
            if (j > 0) {
                if ((!board->bitmap[i][j]) != (!board->bitmap[i][j-1])) {
                    // column transition
                    col_transitions++;
                }
            }

            if (i > 0) {
                if ((!board->bitmap[i][j]) != (!board->bitmap[i-1][j])) {
                    row_transitions++;
                }
            }

            if (board->bitmap[i][j]) {
                has_a_roof = true;
            }

            if (!has_a_roof) {
                bool leftcol = (j == 0) || board->bitmap[i][j-1];
                bool rightcol = (j == board->cols - 1) || board->bitmap[i][j+1];
                if (!board->bitmap[i][j] && leftcol && rightcol) {
                    if (!found_well) {
                        found_well = true;
                        // this is a well, ie
                        // X_X
                        // X_X
                        for (int row = i; row < board->rows; row++) {
                            if (!board->bitmap[row][j]) {
                                well_sums++;
                            }
                        }
                    }
                }
            }
        }
    }

    double score = 0.0;

    values["BLOCK_EDGES"] = max(edges.first, 0);
    values["WALL_EDGES"] = max(edges.second, 0);
    values["EXTERNAL_EDGES"] = max(edges.first + edges.second, 0);
    values["GAPS"] = gaps;
    values["MAX_HEIGHT"] = max_height;
    values["BLOCK_HEIGHT"] = block_height;
    points = (1 << board->rows_cleared) - 1;
    values["POINTS_EARNED"] = points;
    values["COVERS"] = covers;
    values["BUMPINESS"] = bumpiness;

    values["ROW_TRANSITIONS"] = row_transitions;
    values["COL_TRANSITIONS"] = col_transitions;
    values["ROWS_CLEARED"] = board->rows_cleared;
    values["LANDING_HEIGHT"] = landing_height;
    values["WELL_SUMS"] = well_sums;
    values["HOLES"] = holes;

    for (std::map<std::string, double>::iterator iter = values.begin(); iter != values.end(); ++iter) {
        std::string key = iter->first;
        double val = iter->second;
        if (weights.count(key) > 0) {
            score += val * weights[key];
        }
    }

    return score;
}
