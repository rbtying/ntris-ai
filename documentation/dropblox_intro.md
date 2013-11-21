Welcome to Dropblox!
====================

We're asking you to write an AI to play a Tetris variant called
Dropblox! We'll provide you with tools to test your program and
visualize its performance. We think this contest poses a number of
interesting programming challenges, both conceptual and practical.

What's in this Package
----------------------

If you're reading this, you've already downloaded the dropblox
package.  Here's what you'll find inside:

* `documentation/` contains this document.
* `ai-client/` contains the framework you need to run your AI.  The
  framework is written in Python.
* `samples/` contains sample AI programs written in C++, Java, and
  Python.
* Other Files: The other files ini this package are suppoting files
  for the history viewer.

Getting Started
---------------

### Pre-requisites

The game framework requires Python 2.7.  It also requires some Python
packages which will be fetched by this setup script:

    bash ai-client/bootstrap.sh

This package is configured to run on OSX or Linux.  Talk to us if you
need a Windows setup.

### Running games locally

Dropblox is intended to be able to be used in a contest with a remote
server, but this package is set up so you can run with a local server.
The `ai-client/` directory is already set up to run the Python sample
AI in like this:

    cd ai-client
    python client.py local

The client will run a random game by default, but for testing you
might want to have repeatable results.  You can provide a random seed
on the command-line like this:

    python client.py local 1234

### Viewing results

Once you've run a game, you can view the results visually.  To do so,
you need to launch a local server to provide the web view.  Run this
in a separate terminal window:

    python viewer_server.py

Then you can review the results of the most recent game like this:

    python viewer.py

This will launch a browser where you can see you game results.  All
the games you've ever run are saved in the `history/` subdirectory
using the naming convention `<game id>_<timestamp>`. All games run
locally have a game_id of 0.  You can provide the path to one of the
`history/` subdirectories on the viewer.py command-line to view it
instead of hte most recent.

The Rules of Dropblox
---------------------

Dropblox plays much like Tetris. You control one block at a time, and
you can translate and rotate it to position it on the board. When you
have made your moves, the block will drop as far as it can fall
unobstructed, then lock in place. Any rows that are then full are
removed from the board.

The main twist introduced in Dropblox are the types of blocks in the
game. In addition to tetrominoes, Dropblox includes blocks of
arbitrary sizes. The higher your score, the larger the expected size
of the blocks that you'll get! Dropblox also gives you a preview of
the next 5 blocks which will drop after the current one, so you can
plan your moves.

The goal of the game is to score as many points as possible within a 5
minute time limit.

We’ve simplified the rules of the game to make it easier to write an
AI. The following is a complete description of all the rules, other
than the block generation algorithm.

### Definitions

The board is the grid on which you move the active block. This grid
has 33 rows and 12 columns, although only the bottom 24 rows are
visualized. When we refer to coordinates on this board, we will always
use `i` to refer to the rows and `j` to the columns. The top-left
corner of the board is where `i` and `j` equal 0.

The `bitmap` is the current state of the board. `bitmap[i][j]` is
non-zero if and only if that square is occupied.

A `block` is defined by the following properties:

1. A `center`, a square on the board, given by an `i` and a `j`. The
center is the square about which the block rotates. It is _not_
necessarily a square occupied by the block.
2. A list of offsets, also given by `i` and `j` coordinates. These are
the positions of the block’s component squares, relative to its
center.

At all times, the board state includes six blocks:

1. One active block, the `block`, which can be moved.
2. A list of five `preview` blocks, which will be the next five active
blocks, in order.

### Commands

There are five commands that you can issue to move the active block:
`left`, `right`, `up`, `down`, and `rotate`. We define their behavior
here. First, we specify what it means for a block to be in a legal
position, given by the `check` function.

	boolean check(bitmap, block)
		for (offset in block.offsets)
			(i, j) = (block.center.i + offset.i, block.center.j + offset.j);
			if (i < 0 or i >= 33 or j < 0 or j >= 12 or bitmap[i][j])
				return false;
			return true;
			
Basically, a block is in legal position whenever all of its the
component squares are within the bounds of the board and none of them
overlap with already occupied cells!

All of the following movement functions assume that the block begins
and ends in a valid position! If you are implementing this logic in
your AI, you may want to check before and after each command that your
block is in a valid position. Alternatively, you may be able to
optimize away many of the validity checks.

The `left`, `right`, `up`, and `down` commands are translations with
`(i, j)` offsets of `(0, -1)`, `(0, 1)`, `(-1, 0)`, `(1, 0)`,
respectively. The general code for a translation is:

	void translate(block, i_offset, j_offset)
		block.center.i += i_offset;
		block.center.j += j_offset;

The `rotate` command rotates a block 90 degrees clockwise around its
center. Code for rotate is as follows:

	void rotate(block)
		for (offset in block.offsets) 
			(offset.i, offset.j) = (offset.j, -offset.i);

When you wish to end your turn, your AI process should terminate. A
`drop` command will be append to the end of your list of moves.

Code for the `drop` command is as follows. Note that this code assumes
that the block is initially in a valid position, and that it mutates
both the bitmap and the block.

	void drop(bitmap, block)
		while (check(bitmap, block))
			block.center.i += 1
		block.center.i -= 1
		for (offset in block.offsets)
			// We could set this cell to be any non-zero number. In the actual 
			// implementation, the bitmap contains color information.
			bitmap[block.center.i + offset.i][block.center.j + offset.j] = 1
		remove_full_rows(bitmap)

As shown in this snippet, after a block is placed, any full rows are
removed from the board. It is at this stage that you score points! You
will get `2^n - 1` points for clearing `n` rows with one drop. Code
for this procedure is as follows:

	void remove_full_rows(bitmap)
		bitmap = [row for row in bitmap if not all(row)]
		num_rows_removed = 33 - len(bitmap)
		bitmap = num_rows_removed*[12*[0]] + bitmap
		score += (1 << num_rows_removed) - 1

And that’s (nearly) the full rule-set of Dropblox! Feel free to ask
questions.

Building Your AI
----------------

### dropblox_ai Program Specification

For each turn in the game, the client process will spawn your
`dropblox_ai` executable, passing in the state of the game as a
command line argument. Your executable should print a list of commands
to stdout, each one on a new line. When the `dropblox_ai` executable
terminates, the turn will be over. The game server will execute the
given commands to move the block, then drop it into place on the
board. The server will then request the next move, and the client will
spawn a new AI process with the new game state.

This loop will continue until you run out of space on the board or the
5 minute time period for the game is over. When the game ends on time,
any commands your AI has printed to stdout for the current move will
be ignored.

#### Input

Your `dropblox_ai` program should accept two command-line arguments:

1. A JSON-encoded string modeling the game state. Here is an example
of the input:

<pre>{
 "state": "playing",
 "score": 0,
 "bitmap": [
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8],
            [0, 0, 0, 4, 0, 2, 0, 0, 0, 0, 0, 9],
            [5, 5, 5, 3, 3, 2, 2, 2, 2, 1, 8, 0]
           ],
 "block": {
           "type": 4,
           "center": {"i": 7, "j": 6},
           "offsets": [
                       {"i": 0, "j": 0},
                       {"i": -1, "j": 0},
                       {"i": 1, "j": 0},
                       {"i": 2, "j": 0}
                      ]
           }
 "preview": [
             {"type": 0, "center": {"i": 8, "j": 6}, "offsets": [{"i": 0, "j": 0}]},
             {"type": 0, "center": {"i": 8, "j": 6}, "offsets": [{"i": 0, "j": 0}]},
             {"type": 0, "center": {"i": 8, "j": 6}, "offsets": [{"i": 0, "j": 0}]},
             {"type": 0, "center": {"i": 8, "j": 6}, "offsets": [{"i": 0, "j": 0}]},
             {"type": 0, "center": {"i": 8, "j": 6}, "offsets": [{"i": 0, "j": 0}]},
            ],
}</pre>

2. The number of seconds remaining in the game, a float.

#### Output

We are expecting your AI program to print its moves to standard
out. The following are considered valid move strings:

1. `left`
2. `right`
3. `up`
4. `down`
5. `rotate`

Your AI must print one of these strings, immediately followed by a
newline character, in order to be sent to our server. We recommend you
flush stdout after printing, to ensure the move is sent to the server
immediately. This will allow you to submit moves in a streaming
fashion, so that if you hit the timeout, you'll at least have made
some move with the current block.

If you print anything else to stdout, our `client` program will simply
print it to stdout itself.

Sample AIs
----------

We have provided reference implementations in 3 different languages to
help you get started quickly. You will find these implementations
inside the `samples/` directory of the getting-started materials. Each
sample comes with a `README` file explaining how to compile (if
necessary) and run the code.

These reference implementations take care of a lot of the details of
setup, including parsing the JSON and generating reasonable objects to
represent the current game state. They also provide helper methods for
handling pieces, making moves, and computing the new board after a
drop. We recommend that you start with them! However, these AIs won't
score many points. The reference implementations simply move the piece
as far left as it can go -- that's it!
