
/****************************************************************************************************
 *                                                                                                  *
 *                                     Koivisto UCI Chess engine                                    *
 *                                   by. Kim Kahre and Finn Eggers                                  *
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

#include "attacks.h"
#include "uci.h"
#include <fstream>

int main(int argc, char *argv[]) {
    attacks::init();
    bb::init();
    nn::init();



    Search searchObject = {};
    searchObject.init(16);

    searchObject.disableInfoStrings();

    std::string fen;
    std::ifstream data("data.txt");
    while (std::getline(data, fen)) {
        Board board = new Board(fen);
        TimeManager timeManager {};
        timeManager.setDepthLimit(20);

        searchObject.bestMove(&board, &timeManager);
        searchObject.clearHash();
        searchObject.clearHistory();
    }
    // uci::mainloop(argc, argv);
    return 0;
}
