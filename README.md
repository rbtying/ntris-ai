NTRIS-AI
========

AUTHOR
------
Robert Ying

STRATEGY OVERVIEW
-----------------

The ntris AI implements a fairly basic heuristic for determining the sequence of
moves to play in the next turn:

- GenerateValidMoves()
- for each moveseq in validmoves
    - calculate the score (see SCORING ALGORITHM)
- find the average score
- for each moveseq in validmoves
    - if the score of this moveseq is greater than the average, recursively
      calculate scores for one level deeper.

The AI makes decisions based only on 2-piece look-ahead (see TRADEOFFS), so it
will at most recurse three levels deep. This leads to a search space on the
order of `48 ** 3` possible move sequences, which can be computed within a
reasonable period of time (~500ms) on a modern multicore machine. The search
space is significantly reduced by recursing only on the branches which
outperform the average score; otherwise, the computation takes too long to be
useful.

The weights for the scoring algorithm were determined experimentally (see
DETERMINING WEIGHTS)

DESIGN CHOICES
==============

SCORING ALGORITHM
-----------------

Tetris AIs have been researched in the past, and so it seemed prudent to look at
the metrics the most [successful
projects](http://www.colinfahey.com/tetris/tetris.html) used to evaluate tetris
game boards. The ones that are calculated by the scoring algorithm are as
follows:

- `BLOCK_EDGES`: The number of edges a given block has in contact with the existing
  board

- `WALL_EDGES`: The number of edges a given block has in contact with the
  wall

- `EXTERNAL_EDGES`: The sum of `BLOCK_EDGES` and `WALL_EDGES`

- `GAPS`: The number of transitions from high columns to low columns,
  weighted by depth.

- `MAX_HEIGHT`: The height of the highest part of the highest block in the
  board.

- `BLOCK_HEIGHT`: The sum of the height of each part of each block, weighted
  by its vertical position.

- `POINTS_EARNED`: The number of points earned by the board

- `COVERS`: The number of squares which cover holes, weighted by the number
  of holes covered.

- `ROW_TRANSITIONS`: The number of transitions from filled to unfilled
  squares and vice versa in all rows. A measure of the uniformity of a row
  
- `COL_TRANSITIONS`: The number of transitions from filled to unfilled
  squares and vice versa in all columns. A measure of the uniformity of a
  column.

- `ROWS_CLEARED`: The number of rows cleared by the board.

- `LANDING_HEIGHT`: The height of center the block being placed.

- `WELL_SUMS`: The sum of all squares that are in a "well", i.e. are 1
  column wide with both the left and right columns occupied and higher.

- `HOLES`: The number of holes (empty squares with at least one filled
  square above them) on the board.

Not all of these metrics are used; in fact, many of them measure the same
things. Nevertheless, all of them are calculated in each score evaluation, and
then used as a linear input to the weighting function.

The weighting function is a simple inner product between the weights and the
components of the scores, represented by a map of string -> weight read from a
file. Reading from a file was chosen because it makes it simpler to simulate the
program with different parameters.

At the time of writing, only `POINTS_EARNED`, `ROW_TRANSITIONS`,
`LANDING_HEIGHT`, `HOLES`, `BLOCK_HEIGHT`, `WELL_SUMS`, and `COL_TRANSITIONS`
are being used to determine what the next best step would be.

FINDING VALID MOVES
-------------------

Initially, I used a naive brute-force approach to determine the 48 locations
that a block would drop normally. This proved difficult to scale, and so it was
clear that a better method would be necessary. It also unnecessarily precluded
search paths, such as those which attempted to "spin" a piece into place on the
board. 

Because of this, I reimplemented this search using a fairly standard BFS. The
main change was the addition of a path cache, which helped reduce the number of
redundant calculations performed when searching for a valid path. Though this
remains more computationally expensive than the naive method, it significantly
improves results and so is most likely useful.

DETERMINING WEIGHTS
-------------------

I started by trying to find useful weights by hand, but I quickly realized that
the number of variables meant that manual optimization was not likely to work
very well.

From my research online, standard tetris lead to scoring algorithms that had
many local maxima (when looking at score as a function of weights). Thus, I
decided to use a genetic algorithm to try and refine my scoring weights.

Advantages of genetic algorithms:
    - increased randomness makes it more possible to escape the bounds of a
      local maximum
    - simple implementation leads to relatively few bugs
    - lack of interdependency within a given generation makes it easy to
      parallelize
    - can be seeded to bias the initial population in one direction or another

Disadvantages of genetic algorithms:
    - slow to converge
    - may never converge
    - no guarantee of success, and difficult to measure probability of success
    - depends heavily on the ability to compute trials efficiently

I also considered using statistical machine learning to build a classifier for
the AI, but decided that the additional complexity would make it less practical
for the time given. In the future it may be worthwhile to investigate this
route.

Overview of genetic algorithm:

    initial population = 16 random sets of weights ("chromosome")

    seeds = 4 random integer seeds for the pRNG

    for each chromosome in population
        run the client program and find the total score for all 4 trials

    randomly pair the chromosomes in the population, and drop the one of the
    lower pair (there are now 8 chromosomes)

    randomly pair the chromesomes again, and create children by crossing over
    their traits and/or introducing mutation (40% chance of parent 1, 40% chance
    of parent 2, 20% chance of mutation for each trait)

    create another child with the same parents, due to high mutation rate
    (attempt to make the algorithm converge faster)

    apply a gaussian random offset to each of the original chromosomes in the
    population

    create a new population with the 8 children and the 8 offset parents

This process was run indefinitely, and the log files were inspected for the sets
of weights which performed the best. These were then tested experimentally by
hand.

OPTIMIZATION
------------

- Early-stage optimization:
    - Turned on GCC optimization level 3. Though this occasionally introduces
      unexpected bugs and crashes, I felt that my implementation of this AI was
      not sufficiently complex that it would be difficult to correct for. This
      led to a noticeable speedup in computation, and made large-scale
      simulation to find weights possible

    - Cached intermediate notes during BFS for valid moves. This is a simple
      application of dynamic programming; if a given pose has been previously
      determined reachable by some move sequence, there's no need to recreate
      that part of the complete move sequence. This is implemented as a
      backtrack map from result pose -> (parent pose, move), so that it also
      serves as a graph-based store of the move sequence itself.

    - Represented move states as `enum` to avoid excessive memory usage from
      strings. This optimization may have been premature, but it made the
      code type-safe and compiler-checked, so it was probably a net benefit.

- Search space optimization:
    - At 2-piece lookahead it is very impractical to consider the entire search
      space of move sequences. A simple average-score heuristic is used to
      eliminate half of the possibility trees at each level of recursion, which
      makes the runtime fairly reasonable.

- OpenMP
    - Initially tried to get low-hanging fruit by enabling OpenMP. Never having
      used OpenMP before, I forgot to set the `-fopenmp` flag during
      compilation, and so saw no distinct performance improvement.

    - When implementing 2-piece lookahead performance again became a bottleneck,
      this was solved on multicore machines by separating pure functions from
      those with side effects. This massively improved CPU utilization and
      helped make lookahead practical. 
    
    - Use of OpenMP was considered because of built-in support in GCC, and
      because it was trivial to use OpenMP on pure functions. Moreover, OpenMP
      implements an internal thread pool / worker model, which is proven and
      well-tested, and scales well to the number of available cores.

- Debug flags
    - Compiled without `-g` flag allow further optimization and to keep the
      debug hooks from slowing down computation. This has a noticeable effect,
      going from ~400ms/move to ~1000ms/move.

- Genetic Algorithm
    - Parallelized trial computations
    - Used single-lookahead as proxy for double-lookahead to reduce
      computational time
    - minimized IO to attempt to reduce the number of slow operations

LANGUAGE
--------

The samples given were written in Java, C++, and Python. I decided to implement
the AI in C++ because of the need for computational speed in the algorithm. This
choice was somewhat arbitrary but turned out to be useful later (see
OPTIMIZATION).

The genetic weight optimizer was written in python for ease of implementation,
since it was not a bottleneck in the process. This allowed for quick
improvements without the need for recompilation and significant testing.

COMPILATION
-----------

On machines with modern versions of gcc (tested on Ubuntu machines with gcc
4.8.1), this should build with a simple call to `make` in the root directory. If
there are compile errors relating to `#pragma omp parallel for` not being
available, you may need to determine how to install OpenMP libraries on your
system. In most cases, these will be installed already.

There are no other dependencies.

This AI conforms to the Dropblox specification; you can run it as you would
normally run Dropblox. The original unmodified client is available as
`unmodified_client.py` in `ai-client`, and the Makefile will put the
`dropbox_ai` executable in the correct location.
