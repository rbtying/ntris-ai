#ifndef CHOOSE_MOVE_H_
#define CHOOSE_MOVE_H_

#include <string>
#include <vector>
#include <map>
#include <cmath>

class Board;

typedef enum {
    LEFT = 0,
    RIGHT = 1,
    UP = 2,
    DOWN = 3,
    ROTATE = 4,
    DROP = 5
} move_t;

typedef struct {
    int i;
    int j;
    int rot;
} pose_t;

/*!
 * Determines how to score a given outcome
 */
class ScoreVector {
    public:
        void LoadWeightsFromFile(std::string fname);
        void WriteWeightsToFile(std::string fname);

        double Score(Board* board);

    private:
        std::map<std::string, double> weights;
};

std::string StringifyMove(const move_t& m);
std::vector<move_t> FindBestMove(Board* board, std::string config);
std::vector<std::vector<move_t> > GenerateValidMoves(Board* board);

#endif /* CHOOSE_MOVE_H_ */
