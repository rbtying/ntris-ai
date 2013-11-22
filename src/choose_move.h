#ifndef CHOOSE_MOVE_H_
#define CHOOSE_MOVE_H_

#include <string>
#include <vector>
#include <map>
#include <cmath>

class Board;

typedef enum {
    NO_MOVE = -1,
    LEFT = 0,
    RIGHT = 1,
    UP = 2,
    DOWN = 3,
    ROTATE = 4,
    DROP = 5
} move_t;

struct pose_t {
    int i;
    int j;
    int rot;
};
inline bool operator==(const pose_t& lhs, const pose_t& rhs) {
    return lhs.i == rhs.i && lhs.j == rhs.j && (lhs.rot % 4) == (rhs.rot % 4);
};
inline bool operator!=(const pose_t& lhs, const pose_t& rhs) {
    return !operator==(lhs,rhs);
};
inline bool operator< (const pose_t& lhs, const pose_t& rhs) {
    if (lhs.i < rhs.i) {
        return true;
    } else if (lhs.i == rhs.i) {
        if (lhs.j < rhs.j) {
            return true;
        } else if (lhs.j == rhs.j) {
            if ((lhs.rot % 4) < (rhs.rot % 4)) {
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
inline bool operator> (const pose_t& lhs, const pose_t& rhs) {
    return operator< (rhs,lhs);
};
inline bool operator<=(const pose_t& lhs, const pose_t& rhs) {
    return !operator> (lhs,rhs);
};
inline bool operator>=(const pose_t& lhs, const pose_t& rhs) {
    return !operator< (lhs,rhs);
}

/*!
 * Determines how to score a given outcome
 */
class ScoreVector {
    public:
        void LoadWeightsFromFile(std::string fname);
        void WriteWeightsToFile(std::string fname);

        double Score(Board* board, int landing_height=-1);

    private:
        std::map<std::string, double> weights;
};

std::string StringifyMove(const move_t& m);
std::vector<move_t> FindBestMove(Board* board, std::string config);
std::vector<std::vector<move_t> > GenerateValidMoves(Board* board);

#endif /* CHOOSE_MOVE_H_ */
