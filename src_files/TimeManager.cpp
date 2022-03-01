
/****************************************************************************************************
 *                                                                                                  *
 *                                     Koivisto UCI Chess engine                                    *
 *                           by. Kim Kahre, Finn Eggers and Eugenio Bruno                           *
 *                                                                                                  *
 *                 Koivisto is free software: you can redistribute it and/or modify                 *
 *               it under the terms of the GNU General Public License as published by               *
 *                 the Free Software Foundation, either version 3 of the License, or                *
 *                                (at your option) any later version.                               *
 *                    Koivisto is distributed in the hope that it will be useful,                   *
 *                  but WITHOUT ANY WARRANTY; without even the implied warranty of                  *
 *                   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                  *
 *                           GNU General Public License for more details.                           *
 *                 You should have received a copy of the GNU General Public License                *
 *                 along with Koivisto.  If not, see <http://www.gnu.org/licenses/>.                *
 *                                                                                                  *
 ****************************************************************************************************/

#include "TimeManager.h"
#include "UCIAssert.h"

TimeManager::TimeManager() {
    this->setStartTime();
}

void TimeManager::setDepthLimit     (Depth depth) {
    UCI_ASSERT(depth >= 0);
    
    this->depth_limit.depth   = depth;
    this->depth_limit.enabled = true;
}
void TimeManager::setNodeLimit      (U64 nodes) {
    UCI_ASSERT(nodes >= 0);
    
    this->node_limit.nodes   = nodes;
    this->node_limit.enabled = true;
}
void TimeManager::setMoveTimeLimit  (U64 move_time) {
    UCI_ASSERT(move_time >= 0);
    
    this->move_time_limit.upper_time_bound = move_time;
    this->move_time_limit.enabled          = true;
}
void TimeManager::setMatchTimeLimit (U64 time, U64 inc, int moves_to_go) {
    UCI_ASSERT(time >= 0);
    UCI_ASSERT(inc >= 0);
    UCI_ASSERT(moves_to_go >= 0);
    
    constexpr U64 overhead = 50;
    const double  division = moves_to_go + 1;
    
    U64 upperTimeBound = (int(time / division) * 3 + std::min(time * 0.9 + inc, inc * 3.0) - 25);
    U64 timeToUse      = time / 40;
    
    timeToUse          = std::min(time - inc, timeToUse);
    upperTimeBound     = std::min(time - inc, upperTimeBound);
    
    this->setMoveTimeLimit(upperTimeBound);
    this->match_time_limit.time_to_use      = timeToUse;
    this->match_time_limit.enabled          = true;
}

void TimeManager::setStartTime() {
    start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TimeManager::reset() {
    this->last_eval = 0;
    this->eval_factor = 1.0;
    this->move_factor = 1.0;
    this->prev_move = 0;
    this->same_move_depth = 0;
}

int  TimeManager::elapsedTime() const {
    auto end   = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now().time_since_epoch()).count();
    auto diff = end - start_time;
    return diff;
}

void TimeManager::stopSearch() {
    force_stop = true;
}

void TimeManager::update(int depth, int eval, Move move) {
    if (depth < 6) {
        this->last_eval = eval;
        this->prev_move = move;
        return;
    } else {
        if (move != this->prev_move) {
            this->same_move_depth = 0;
        } else {
            this->same_move_depth += 1;
        }

        this->move_factor = std::max((float)std::pow(1.05, (float)(9 - this->same_move_depth)), 0.4f);

        float diff = std::min((float)std::abs(eval - this->last_eval) / 25.0f, 1.0f);
        this->eval_factor *= (float)std::pow(1.05, diff);

        this->prev_move = move;
        this->last_eval = eval;
    }
}

bool TimeManager::isTimeLeft(SearchData* sd) {
    // stop the search if requested
    if (force_stop)
        return false;
    
    int elapsed = elapsedTime();
    
    if (sd != nullptr && this->match_time_limit.enabled) {
        if (elapsed < this->match_time_limit.time_to_use) {
            sd->targetReached = false;
        } else {
            sd->targetReached = true;
        }
    }
    
    // if we are above the maximum allowed time, stope
    if (    this->move_time_limit.enabled
         && this->move_time_limit.upper_time_bound < elapsed)
        return false;
    
    return true;
}

bool TimeManager::rootTimeLeft(int score) {
    // stop the search if requested
    if (force_stop)
        return false;
    
    int elapsed = elapsedTime();
    
    if(    move_time_limit.enabled
        && move_time_limit.upper_time_bound < elapsed)
        return false;
    
    // the score is a value between 0 and 100 where 100 means that
    // the entire time has been spent looking at the best move.
    // this indicates that there is most likely just a single best move
    // which means we could spend less time searching. In case of the score being
    // 100, we half the time to use. If it's lower than 30, it reaches a maximum of 1.4 times the
    // original time to use.
    if(    match_time_limit.enabled
        && (int)(match_time_limit.time_to_use * this->eval_factor * this->move_factor * 0.8) < elapsed)
        return false;

    return true;
}
