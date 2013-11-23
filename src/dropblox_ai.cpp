#include "dropblox_ai.h"

using namespace json;
using namespace std;

//----------------------------------
// Block implementation starts here!
//----------------------------------

Block::Block(Object& raw_block) {
  center.i = (int)(Number&)raw_block["center"]["i"];
  center.j = (int)(Number&)raw_block["center"]["j"];
  size = 0;

  Array& raw_offsets = raw_block["offsets"];
  for (Array::const_iterator it = raw_offsets.Begin(); it < raw_offsets.End(); it++) {
    size += 1;
  }
  for (int i = 0; i < size; i++) {
    offsets[i].i = (Number&)raw_offsets[i]["i"];
    offsets[i].j = (Number&)raw_offsets[i]["j"];
  }

  translation.i = 0;
  translation.j = 0;
  rotation = 0;
}

inline bool operator==(const Block& lhs, const Block& rhs) {
    return lhs.center.i == rhs.center.i && lhs.center.j == rhs.center.j && (lhs.rotation % 4) == (rhs.rotation % 4);
};
inline bool operator!=(const Block& lhs, const Block& rhs) {
    return !operator==(lhs,rhs);
};
inline bool operator< (const Block& lhs, const Block& rhs) {
    if (lhs.center.i < rhs.center.i) {
        return true;
    } else if (lhs.center.i == rhs.center.i) {
        if (lhs.center.j < rhs.center.j) {
            return true;
        } else if (lhs.center.j == rhs.center.j) {
            if ((lhs.rotation % 4) < (rhs.rotation % 4)) {
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    } else {
        return false;
    }
};
inline bool operator> (const Block& lhs, const Block& rhs) {
    return operator< (rhs,lhs);
};
inline bool operator<=(const Block& lhs, const Block& rhs) {
    return !operator> (lhs,rhs);
};
inline bool operator>=(const Block& lhs, const Block& rhs) {
    return !operator< (lhs,rhs);
}

void Block::left() {
  translation.j -= 1;
}

void Block::right() {
  translation.j += 1;
}

void Block::up() {
  translation.i -= 1;
}

void Block::down() {
  translation.i += 1;
}

void Block::rotate() {
  rotation += 1;
}

void Block::unrotate() {
  rotation -= 1;
}

// The checked_* methods below perform an operation on the block
// only if it's a legal move on the passed in board.  They
// return true if the move succeeded.
//
// The block is still assumed to start in a legal position.
bool Block::checked_left(const Board& board) {
  left();
  if (board.check(*this)) {
    return true;
  }
  right();
  return false;
}

bool Block::checked_right(const Board& board) {
  right();
  if (board.check(*this)) {
    return true;
  }
  left();
  return false;
}

bool Block::checked_up(const Board& board) {
  up();
  if (board.check(*this)) {
    return true;
  }
  down();
  return false;
}

bool Block::checked_down(const Board& board) {
  down();
  if (board.check(*this)) {
    return true;
  }
  up();
  return false;
}

bool Block::checked_rotate(const Board& board) {
  rotate();
  if (board.check(*this)) {
    return true;
  }
  unrotate();
  return false;
}

/*!
 * Attempts a move and returns true on success, false on failure.
 *
 * @param board the board to check against
 * @param move the move to try
 * @return true on move success
 */
bool Block::checked_move(const Board& board, const move_t& move) {
  switch(move) {
    case LEFT:
      return checked_left(board);
    case RIGHT:
      return checked_right(board);
    case DOWN:
      return checked_down(board);
    case ROTATE:
      return checked_rotate(board);
    case UP:
      return checked_up(board);
    default:
      return false;
  }
  return false;
}

void Block::do_command(const string& command) {
  if (command == "left") {
    left();
  } else if (command == "right") {
    right();
  } else if (command == "up") {
    up();
  } else if (command == "down") {
    down();
  } else if (command == "rotate") {
    rotate();
  } else {
    throw Exception("Invalid command " + command);
  }
}

/*!
 * Gets the width, height of this block
 *
 * @return pair of width, height.
 */
std::pair<int, int> Block::dimensions() {
  Point point;
  std::pair<int, int> dim;

  int left_bound = COLS;
  int right_bound = 0;
  int bottom_bound = ROWS;
  int top_bound = 0;
  
  for (int i = 0; i < size; i++) {
    point.i = center.i + translation.i;
    point.j = center.j + translation.j;
    if (rotation % 2) {
      point.i += (2 - rotation)*offsets[i].j;
      point.j +=  -(2 - rotation)*offsets[i].i;
    } else {
      point.i += (1 - rotation)*offsets[i].i;
      point.j += (1 - rotation)*offsets[i].j;
    }
    if (point.j < left_bound) {
        left_bound = point.j;
    }
    if (point.j > right_bound) {
        right_bound = point.j;
    }

    if (point.i < bottom_bound) {
        bottom_bound = point.i;
    }
    if (point.i > top_bound) {
        top_bound = point.i;
    }
  }

  dim.first = right_bound - left_bound;
  dim.second = top_bound - bottom_bound;
  return dim;
}

void Block::do_commands(const vector<string>& commands) {
  for (unsigned int i = 0; i < commands.size(); i++) {
    do_command(commands[i]);
  }
}

void Block::reset_position() {
  translation.i = 0;
  translation.j = 0;
  rotation = 0;
}

//----------------------------------
// Board implementation starts here!
//----------------------------------

Board::Board() {
  rows = ROWS;
  cols = COLS;
  rows_cleared = 0;
}

Board::Board(Object& state) {
  rows = ROWS;
  cols = COLS;

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      bitmap[i][j] = ((int)(Number&)state["bitmap"][i][j] ? 1 : 0);
    }
  }

  // Note that these blocks are NEVER destructed! This is because calling
  // place() on a board will create new boards which share these objects.
  //
  // There's a memory leak here, but it's okay: blocks are only constructed
  // when you construct a board from a JSON Object, which should only happen
  // for the very first board. The total memory leaked will only be ~10 kb.
  block = new Block(state["block"]);
  for (int i = 0; i < PREVIEW_SIZE; i++) {
    preview.push_back(new Block(state["preview"][i]));
  }
}

// Returns true if the `query` block is in valid position - that is, if all of
// its squares are in bounds and are currently unoccupied.
bool Board::check(const Block& query) const {
  Point point;
  for (int i = 0; i < query.size; i++) {
    point.i = query.center.i + query.translation.i;
    point.j = query.center.j + query.translation.j;
    if (query.rotation % 2) {
      point.i += (2 - query.rotation)*query.offsets[i].j;
      point.j +=  -(2 - query.rotation)*query.offsets[i].i;
    } else {
      point.i += (1 - query.rotation)*query.offsets[i].i;
      point.j += (1 - query.rotation)*query.offsets[i].j;
    }
    if (point.i < 0 || point.i >= ROWS ||
        point.j < 0 || point.j >= COLS || bitmap[point.i][point.j]) {
      return false;
    }
  }
  return true;
}

bool Board::check(const Block& query, const pose_t& pose) {
  Point point;
  for (int i = 0; i < query.size; i++) {
    point.i = pose.i;
    point.j = pose.j;
    if (query.rotation % 2) {
      point.i += (2 - pose.rot)*query.offsets[i].j;
      point.j +=  -(2 - pose.rot)*query.offsets[i].i;
    } else {
      point.i += (1 - pose.rot)*query.offsets[i].i;
      point.j += (1 - pose.rot)*query.offsets[i].j;
    }
    if (point.i < 0 || point.i >= ROWS ||
        point.j < 0 || point.j >= COLS || bitmap[point.i][point.j]) {
      return false;
    }
  }
  return true;
}

std::pair<int, int> Board::countedges(const Block& query) const {
    bool bmp[rows][cols];
    memset(bmp, false, sizeof(bool) * rows * cols);
    int edges_touching_block = 0;
    int edges_touching_wall = 0;
    std::pair<int, int> result;
    result.first = -1;
    result.second = -1;

    // set the points of the block in bmp
    for (int idx = 0; idx < block->size; idx++) {
        Point point;
        point.i = block->center.i + block->translation.i;
        point.j = block->center.j + block->translation.j;

        if (block->rotation % 2) {
            point.i += (2 - block->rotation) * block->offsets[idx].j;
            point.j += -(2 - block->rotation) * block->offsets[idx].i;
        } else {
            point.i += (1 - block->rotation) * block->offsets[idx].i;
            point.j += (1 - block->rotation) * block->offsets[idx].j;
        }

        if (point.i < 0 || point.i >= ROWS ||
            point.j < 0 || point.j >= COLS || bitmap[point.i][point.j]) {
            return result;
        }

        bmp[point.i][point.j] = true;
    }

    // check for edges
    for (int idx = 0; idx < block->size; idx++) {
        Point point;
        point.i = block->center.i + block->translation.i;
        point.j = block->center.j + block->translation.j;

        if (block->rotation % 2) {
            point.i += (2 - block->rotation) * block->offsets[idx].j;
            point.j += -(2 - block->rotation) * block->offsets[idx].i;
        } else {
            point.i += (1 - block->rotation) * block->offsets[idx].i;
            point.j += (1 - block->rotation) * block->offsets[idx].j;
        }

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                int r = point.i + dy;
                int c = point.j + dx;
                if (r < 0 || c < 0 || r >= rows || c >= cols) {
                    // this is a wall edge
                    edges_touching_wall++;
                } else if (bmp[r][c]) {
                    // this is in the block, so it's not an edge.
                    continue;
                } else if (bitmap[r][c]) {
                    // this is set on the board, so this is an edge touching
                    // another block.
                    edges_touching_block++;
                }
            }
        }

    }
    result.first = edges_touching_block;
    result.second = edges_touching_wall;
    return result;
}

// Resets the block's position, moves it according to the given commands, then
// drops it onto the board. Returns a pointer to the new board state object.
//
// Throws an exception if the block is ever in an invalid position.
Board* Board::do_commands(const vector<move_t>& commands) {
  block->reset_position();
  if (!check(*block)) {
    throw Exception("Block started in an invalid position");
  }
  for (unsigned int i = 0; i < commands.size(); i++) {
    if (commands[i] == DROP) {
      return place();
    } else {
      block->do_command(StringifyMove(commands[i]));
      if (!check(*block)) {
        throw Exception("Block reached in an invalid position");
      }
    }
  }
  // If we've gotten here, there was no "drop" command. Drop anyway.
  return place();
}

// Drops the block from whatever position it is currently at. Returns a
// pointer to the new board state object, with the next block drawn from the
// preview list.
//
// Assumes the block starts out in valid position.
// This method translates the current block downwards.
//
// If there are no blocks left in the preview list, this method will fail badly!
// This is okay because we don't expect to look ahead that far.
Board* Board::place() {
  Board* new_board = new Board();

  while (check(*block)) {
    block->down();
  }
  block->up();

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      new_board->bitmap[i][j] = bitmap[i][j];
    }
  }

  Point point;
  for (int i = 0; i < block->size; i++) {
    point.i = block->center.i + block->translation.i;
    point.j = block->center.j + block->translation.j;
    if (block->rotation % 2) {
      point.i += (2 - block->rotation)*block->offsets[i].j;
      point.j +=  -(2 - block->rotation)*block->offsets[i].i;
    } else {
      point.i += (1 - block->rotation)*block->offsets[i].i;
      point.j += (1 - block->rotation)*block->offsets[i].j;
    }
    new_board->bitmap[point.i][point.j] = 1;
  }
  new_board->rows_cleared = rows_cleared + Board::remove_rows(&(new_board->bitmap));

  new_board->block = preview[0];
  for (unsigned int i = 1; i < preview.size(); i++) {
    new_board->preview.push_back(preview[i]);
  }

  return new_board;
}

// A static method that takes in a new_bitmap and removes any full rows from it.
// Mutates the new_bitmap in place.
int Board::remove_rows(Bitmap* new_bitmap) {
  int rows_removed = 0;
  for (int i = ROWS - 1; i >= 0; i--) {
    bool full = true;
    for (int j = 0; j < COLS; j++) {
      if (!(*new_bitmap)[i][j]) {
        full = false;
        break;
      }
    }
    if (full) {
      rows_removed += 1;
    } else if (rows_removed) {
      for (int j = 0; j < COLS; j++) {
        (*new_bitmap)[i + rows_removed][j] = (*new_bitmap)[i][j];
      }
    }
  }
  for (int i = 0; i < rows_removed; i++) {
    for (int j = 0; j < COLS; j++) {
      (*new_bitmap)[i][j] = 0;
    }
  }
  return rows_removed;
}

int main(int argc, char** argv) {
  // Construct a JSON Object with the given game state.
  istringstream raw_state(argv[1]);
  Object state;
  Reader::Read(state, raw_state);

  // Construct a board from this Object.
  Board board(state);

  // Make some moves!
  // vector<string> moves;
  // while (board.check(*board.block)) {
  //   board.block->left();
  //   moves.push_back("left");
  // }
  // // Ignore the last move, because it moved the block into invalid
  // // position. Make all the rest.
  // for (int i = 0; i < moves.size() - 1; i++) {
  //   cout << moves[i] << endl;
  // }
  //
  const char * fpath = "weights.txt";
  if (argc >= 3) {
      fpath = argv[3];
  }
  std::vector<move_t> bestmove = FindBestMove(&board, fpath);
  for (unsigned int i = 0; i < bestmove.size(); i++) {
      cout << StringifyMove(bestmove[i]) << endl;
  }
}
