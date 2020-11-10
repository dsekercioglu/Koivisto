
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

#include "Tuning.h"

#include "Bitboard.h"

#include <iomanip>

#ifdef TUNE

using namespace std;
using namespace tuning;

std::vector<TrainingEntry> tuning::training_entries {};

void tuning::loadPositionFile(const std::string& path, int count, int start) {

    fstream newfile;
    newfile.open(path, ios::in);
    if (newfile.is_open()) {
        string tp;
        int    lineCount = 0;
        int    posCount  = 0;
        while (getline(newfile, tp)) {

            if (lineCount < start) {
                lineCount++;
                continue;
            }

            // finding the first "c" to check where the fen ended
            auto firstC = tp.find_first_of('c');
            auto lastC  = tp.find_last_of('c');
            if (firstC == string::npos || lastC == string::npos) {
                continue;
            }

            // extracting the fen and result and removing bad characters.
            string fen = tp.substr(0, firstC);
            string res = tp.substr(lastC + 2, string::npos);

            fen = trim(fen);
            res = findAndReplaceAll(res, "\"", "");
            res = findAndReplaceAll(res, ";", "");
            res = trim(res);

            TrainingEntry new_entry {Board {fen}, 0};

            // parsing the result to a usable value:
            // assuming that the result is given as : a-b
            if (res.find('-') != string::npos) {
                if (res == "1/2-1/2") {
                    new_entry.target = 0.5;
                } else if (res == "1-0") {
                    new_entry.target = 1;
                } else if (res == "0-1") {
                    new_entry.target = 0;
                } else {
                    continue;
                }
            }
            // trying to read the result as a decimal
            else {
                try {
                    double actualResult = stod(res);
                    new_entry.target    = actualResult;
                } catch (std::invalid_argument& e) { continue; }
            }
            
            training_entries.push_back(new_entry);
            
            lineCount++;
            posCount++;

            if (posCount % 10000 == 0) {

                std::cout << "\r" << loadingBar(posCount, count, "Loading data") << std::flush;
            }

            if (posCount >= count)
                break;
        }

        std::cout << std::endl;
        newfile.close();
    }
}

void tuning::findWorstPositions(Evaluator* evaluator, double K, int count) {
    double max = 2;

    for (int i = 0; i < count; i++) {
        int    indexOfHighest = 0;
        double highestScore   = 0;

        for (int n = 0; n < dataCount; n++) {
            Score  q_i      = evaluator->evaluate(boards[n]);
            double expected = results[n];
            double sig      = sigmoid(q_i, K);

            double difference = abs(expected - sig);

            if (difference > highestScore && difference < max) {
                highestScore   = difference;
                indexOfHighest = n;
            }
        }

        max = highestScore;

        std::cout << boards[indexOfHighest]->fen() << " " << evaluator->evaluate(boards[indexOfHighest]) << " "
                  << results[indexOfHighest] << std::endl;
    }
}

void tuning::generateHeatMap(Piece piece, bool earlyAndLate, bool asymmetric) {

    auto addToTable = [](double* table, double* count, U64 bitboard, double factor, Color color) {
        while (bitboard) {
            Square s = bitscanForward(bitboard);

            int index = color == WHITE ? s : (squareIndex(7 - rankIndex(s), fileIndex(s)));

            table[index] += factor;
            count[index]++;
            bitboard = lsbReset(bitboard);
        }
    };

    auto printTable = [](double* table) {
        std::cout << " +--------+--------+--------+--------+--------+--------+--------+--------+\n";

        for (Rank r = 7; r >= 0; r--) {

            std::cout << " |        |        |        |        |        |        |        |        |\n";
            for (File f = 0; f <= 7; ++f) {
                Square sq = bb::squareIndex(r, f);

                std::cout << std::fixed << std::setprecision(1);
                std::cout << " | " << std::setw(6) << table[sq];
            }

            std::cout << " |\n |        |        |        |        |        |        |        |        |\n";
            std::cout << " +--------+--------+--------+--------+--------+--------+--------+--------+\n";
        }
    };

    auto trimTable = [](double* table, double* count) {
        double max = 0;
        double min = 0;

        for (int i = 0; i < 64; i++) {
            table[i] /= count[i];
        }

        for (int i = 0; i < 64; i++) {
            if (table[i] > max)
                max = table[i];
            if (table[i] < min)
                min = table[i];
        }
        for (int i = 0; i < 64; i++) {
            //            table[i] -= min;
            table[i] /= ((max - min) / 100);
        }
    };

    if (earlyAndLate && asymmetric) {
        double* earlyWhite = new double[64] {0};
        double* lateWhite  = new double[64] {0};
        double* earlyBlack = new double[64] {0};
        double* lateBlack  = new double[64] {0};

        double* earlyWhiteCount = new double[64] {0};
        double* lateWhiteCount  = new double[64] {0};
        double* earlyBlackCount = new double[64] {0};
        double* lateBlackCount  = new double[64] {0};

        for (int i = 0; i < dataCount; i++) {

            // dont look at equal positions
            if (results[i] == 0.5)
                continue;

            Board* b = boards[i];

            // calculate the phase
            double phase =
                (18
                 - bitCount(b->getPieces()[WHITE_BISHOP] | b->getPieces()[BLACK_BISHOP] | b->getPieces()[WHITE_KNIGHT]
                            | b->getPieces()[BLACK_KNIGHT] | b->getPieces()[WHITE_ROOK] | b->getPieces()[BLACK_ROOK])
                 - 3 * bitCount(b->getPieces()[WHITE_QUEEN] | b->getPieces()[BLACK_QUEEN]))
                / 18.0;

            Color winner      = results[i] > 0.5 ? WHITE : BLACK;
            int   whiteFactor = winner == WHITE ? 1 : -1;
            //            double earlyChange = phase < 0.3 ? 1:0;
            //            double lateChange = phase > 0.8 ? 1:0;

            double earlyChange = 1 - phase;
            double lateChange  = phase;

            //            std::cout << *b << std::endl;
            //            std::cout << phase << std::endl;

            addToTable(earlyWhite, earlyWhiteCount, b->getPieces()[piece % 6], whiteFactor * earlyChange, WHITE);
            addToTable(lateWhite, lateWhiteCount, b->getPieces()[piece % 6], whiteFactor * lateChange, WHITE);
            addToTable(earlyBlack, earlyBlackCount, b->getPieces()[(piece % 6) + 6], -whiteFactor * earlyChange, WHITE);
            addToTable(lateBlack, lateBlackCount, b->getPieces()[(piece % 6) + 6], -whiteFactor * lateChange, WHITE);
        }

        for (int i = 0; i < 64; i++) {
            std::cout << int(earlyWhiteCount[i]) << ",";
        }
        std::cout << std::endl;
        for (int i = 0; i < 64; i++) {
            std::cout << int(lateWhiteCount[i]) << ",";
        }
        std::cout << std::endl;
        for (int i = 0; i < 64; i++) {
            std::cout << int(earlyBlack[i]) << ",";
        }
        std::cout << std::endl;
        for (int i = 0; i < 64; i++) {
            std::cout << int(lateBlack[i]) << ",";
        }
        std::cout << std::endl;

        trimTable(earlyWhite, earlyWhiteCount);
        trimTable(lateWhite, lateWhiteCount);
        trimTable(earlyBlack, earlyBlackCount);
        trimTable(lateBlack, lateBlackCount);

        printTable(earlyWhite);
        printTable(lateWhite);
        printTable(earlyBlack);
        printTable(lateBlack);

        delete[] earlyWhite;
        delete[] lateWhite;
        delete[] earlyBlack;
        delete[] lateBlack;

        delete[] earlyWhiteCount;
        delete[] lateWhiteCount;
        delete[] earlyBlackCount;
        delete[] lateBlackCount;
    }

    if (earlyAndLate && !asymmetric) {
        double* earlyWhite = new double[64] {0};
        double* lateWhite  = new double[64] {0};

        double* earlyWhiteCount = new double[64] {0};
        double* lateWhiteCount  = new double[64] {0};

        for (int i = 0; i < dataCount; i++) {

            // dont look at equal positions
            if (results[i] == 0.5)
                continue;

            Board* b = boards[i];

            // calculate the phase
            double phase =
                (18
                 - bitCount(b->getPieces()[WHITE_BISHOP] | b->getPieces()[BLACK_BISHOP] | b->getPieces()[WHITE_KNIGHT]
                            | b->getPieces()[BLACK_KNIGHT] | b->getPieces()[WHITE_ROOK] | b->getPieces()[BLACK_ROOK])
                 - 3 * bitCount(b->getPieces()[WHITE_QUEEN] | b->getPieces()[BLACK_QUEEN]))
                / 18.0;

            Color winner      = results[i] > 0.5 ? WHITE : BLACK;
            int   whiteFactor = winner == WHITE ? 1 : -1;
            //            double earlyChange = phase < 0.3 ? 1:0;
            //            double lateChange = phase > 0.8 ? 1:0;

            double earlyChange = 1 - phase;
            double lateChange  = phase;

            //            std::cout << *b << std::endl;
            //            std::cout << phase << std::endl;

            addToTable(earlyWhite, earlyWhiteCount, b->getPieces()[piece % 6], whiteFactor * earlyChange, WHITE);
            addToTable(lateWhite, lateWhiteCount, b->getPieces()[piece % 6], whiteFactor * lateChange, WHITE);
            addToTable(earlyWhite, earlyWhiteCount, b->getPieces()[(piece % 6) + 6], -whiteFactor * earlyChange, BLACK);
            addToTable(lateWhite, lateWhiteCount, b->getPieces()[(piece % 6) + 6], -whiteFactor * lateChange, BLACK);
        }

        trimTable(earlyWhite, earlyWhiteCount);
        trimTable(lateWhite, lateWhiteCount);

        for (int i = 0; i < 64; i++) {
            std::cout << int(earlyWhite[i]) << ",";
        }
        std::cout << std::endl;
        for (int i = 0; i < 64; i++) {
            std::cout << int(lateWhite[i]) << ",";
        }
        std::cout << std::endl;

        printTable(earlyWhite);
        printTable(lateWhite);

        delete[] earlyWhite;
        delete[] lateWhite;

        delete[] earlyWhiteCount;
        delete[] lateWhiteCount;
    }
}

void tuning::clearLoadedData() {
    if (boards != nullptr) {

        for (int i = 0; i < dataCount; i++) {
            delete boards[i];
        }

        delete[] boards;
        delete[] results;
    }
}

double tuning::optimiseGD(Evaluator* evaluator, double K, double learningRate) {
    int paramCount = evaluator->paramCount();

    auto* earlyGrads   = new double[paramCount] {0};
    auto* lateGrads    = new double[paramCount] {0};
    auto* gradCounters = new int[paramCount] {0};

    double score = 0;

    for (int i = 0; i < dataCount; i++) {
        Score  q_i      = evaluator->evaluate(boards[i]);
        double expected = results[i];

        double sig       = sigmoid(q_i, K);
        double sigPrime  = sigmoidPrime(q_i, K);
        double lossPrime = -2 * (expected - sig);

        float* features = evaluator->getFeatures();
        float  phase    = evaluator->getPhase();

        for (int p = 0; p < paramCount; p++) {
            earlyGrads[p] += features[p] * (1 - phase) * sigPrime * lossPrime;
            lateGrads[p] += features[p] * phase * sigPrime * lossPrime;

            if (features[p] != 0) {
                gradCounters[p] += 1;
            }
        }

        score += (expected - sig) * (expected - sig);
    }

    for (int p = 0; p < paramCount; p++) {

        evaluator->getEarlyGameParams()[p] -= earlyGrads[p] * learningRate / -min(-1, -gradCounters[p]);
        evaluator->getLateGameParams()[p] -= lateGrads[p] * learningRate / -min(-1, -gradCounters[p]);
    }

    return score / dataCount;
}

double tuning::optimiseBlackBox(Evaluator* evaluator, double K, float* params, int paramCount, float lr) {

    for (int p = 0; p < paramCount; p++) {

        std::cout << "\r  param: " << p << std::flush;

        double er = computeError(evaluator, K);

        params[p] += lr;
        double erUpper = computeError(evaluator, K);

        if (erUpper < er)
            continue;

        params[p] -= 2 * lr;
        double erLower = computeError(evaluator, K);

        if (erLower < er)
            continue;

        params[p] += lr;
    }
    std::cout << std::endl;

    return computeError(evaluator, K);
}

double tuning::optimisePSTBlackBox(Evaluator* evaluator, double K, EvalScore* evalScore, int count, int lr) {
    double er;

    for (int p = 0; p < count; p++) {

        std::cout << "\r  param: " << p << std::flush;

        er = computeError(evaluator, K);
        //        std::cout << er << std::endl;
        evalScore[p] += M(+lr, 0);
        eval_init();
        //        showScore(M(+lr,0));

        double upper = computeError(evaluator, K);
        //        std::cout << upper << std::endl;
        if (upper >= er) {
            evalScore[p] += M(-2 * lr, 0);
            eval_init();
            //            showScore(evalScore[p]);

            double lower = computeError(evaluator, K);

            if (lower >= er) {
                evalScore[p] += M(+lr, 0);
                //                showScore(evalScore[p]);
                eval_init();
            }
        }

        er = computeError(evaluator, K);
        evalScore[p] += M(0, +lr);
        eval_init();

        upper = computeError(evaluator, K);
        if (upper >= er) {
            evalScore[p] += M(0, -2 * lr);
            eval_init();

            double lower = computeError(evaluator, K);

            if (lower >= er) {
                evalScore[p] += M(0, +lr);
                eval_init();
            }
        }
    }
    std::cout << std::endl;
    return er;
}

double tuning::optimisePSTBlackBox(Evaluator* evaluator, double K, EvalScore** evalScore, int count, int lr) {
    double er;

    for (int p = 0; p < count; p++) {

        std::cout << "\r  param: " << p << std::flush;

        er = computeError(evaluator, K);
        *evalScore[p] += M(+lr, 0);
        eval_init();

        double upper = computeError(evaluator, K);
        if (upper >= er) {
            *evalScore[p] += M(-2 * lr, 0);
            eval_init();

            double lower = computeError(evaluator, K);

            if (lower >= er) {
                *evalScore[p] += M(+lr, 0);
                eval_init();
            }
        }

        er = computeError(evaluator, K);
        *evalScore[p] += M(0, +lr);
        eval_init();

        upper = computeError(evaluator, K);
        if (upper >= er) {
            *evalScore[p] += M(0, -2 * lr);
            eval_init();

            double lower = computeError(evaluator, K);

            if (lower >= er) {
                *evalScore[p] += M(0, +lr);
                eval_init();
            }
        }
    }
    std::cout << std::endl;
    return er;
}

double tuning::computeError(Evaluator* evaluator, double K) {
    double score = 0;
    for (TrainingEntry& entry : training_entries) {
        
        Score  q_i      = evaluator->evaluate(&entry.board);
        double expected = entry.target;

        double sig = sigmoid(q_i, K);

        //        std::cout << sig << std::endl;

        score += (expected - sig) * (expected - sig);
    }
    return score / training_entries.size();
}

double tuning::computeK(Evaluator* evaluator, double initK, double rate, double deviation) {

    double K    = initK;
    double dK   = 0.01;
    double dEdK = 1;

    while (abs(dEdK) > deviation) {

        double Epdk = computeError(evaluator, K + dK);
        double Emdk = computeError(evaluator, K - dK);

        dEdK = (Epdk - Emdk) / (2 * dK);

        std::cout << "K:" << K << " Error: " << (Epdk + Emdk) / 2 << " dev: " << abs(dEdK) << std::endl;

        K -= dEdK * rate;
    }

    return K;
}

void tuning::evalSpeed() {

    Evaluator evaluator {};

    startMeasure();
    U64 sum = 0;

    for (TrainingEntry& en : training_entries) {
        sum += evaluator.evaluate(&en.board);
    }

    int ms = stopMeasure();
    std::cout << ms << "ms for " << training_entries.size()
              << " positions = " << (1000 * training_entries.size() / ms) / 1e6 << "Mnps"
              << " Checksum = " << sum << std::endl;
}



void tuning::displayTunedValues() {
    
    const static string psqt_names[] = {"psqt_pawn_same_side_castle",
                                        "psqt_pawn_opposite_side_castle",
                                        "psqt_knight_same_side_castle",
                                        "psqt_knight_opposite_side_castle",
                                        "psqt_bishop_same_side_castle",
                                        "psqt_bishop_opposite_side_castle",
                                        "psqt_rook_same_side_castle",
                                        "psqt_rook_opposite_side_castle",
                                        "psqt_queen_same_side_castle",
                                        "psqt_queen_opposite_side_castle",
                                        "psqt_king"};
    const static string mob_names[] = { "","mobilityKnight", "mobilityBishop", "mobilityRook", "mobilityQueen"};
    const static string feat_names[]    = {"SIDE_TO_MOVE",
                                        "PAWN_STRUCTURE",
                                        "PAWN_PASSED",
                                        "PAWN_ISOLATED",
                                        "PAWN_DOUBLED",
                                        "PAWN_DOUBLED_AND_ISOLATED",
                                        "PAWN_BACKWARD",
                                        "PAWN_OPEN",
                                        "PAWN_BLOCKED",
                                        "KNIGHT_OUTPOST",
                                        "KNIGHT_DISTANCE_ENEMY_KING",
                                        "ROOK_OPEN_FILE",
                                        "ROOK_HALF_OPEN_FILE",
                                        "ROOK_KING_LINE",
                                        "BISHOP_DOUBLED",
                                        "BISHOP_FIANCHETTO",
                                        "BISHOP_PIECE_SAME_SQUARE_E",
                                        "QUEEN_DISTANCE_ENEMY_KING",
                                        "KING_CLOSE_OPPONENT",
                                        "KING_PAWN_SHIELD",
                                        "CASTLING_RIGHTS"};
    
    for(int i = 0; i < 11; i++){
        std::cout << "EvalScore " << psqt_names[i] << "[64] = {" << right << std::endl;
        for(Square s = 0; s < 64; s++){
            if(s % 8 == 0){
                std::cout << "    ";
            }
            std::cout << "M(" << setw(4) << MgScore(psqt[i][s]) << "," << setw(4) << EgScore(psqt[i][s]) << "), ";
            if(s % 8 == 7){
                std::cout << std::endl;
            }
        }
        std::cout << "};" << std::endl;
        std::cout << std::endl;
    }
    
    for(Piece p = KNIGHT; p <= QUEEN; p++){
        std::cout << "EvalScore " << mob_names[p] << "[" << mobEntryCount[p] <<"] = {" << right << std::endl;
        for(Square s = 0; s < mobEntryCount[p]; s++){
            if(s % 8 == 0){
                std::cout << "    ";
            }
            std::cout << "M(" << setw(5) << MgScore(mobilities[p][s]) << "," << setw(5) << EgScore(mobilities[p][s]) << "), ";
            if(s % 8 == 7){
                std::cout << std::endl;
            }
        }
        std::cout << "};" << std::endl;
        std::cout << std::endl;
    }
    
    // hanging eval
    std::cout << "EvalScore hangingEval[5] {" << std::endl << "    ";
    for(int i = 0; i < 5; i++){
        std::cout << "M(" << setw(5) << MgScore(hangingEval[i]) << "," << setw(5) << EgScore(hangingEval[i]) << "), ";
        if(i % 5 == 4){
            std::cout << std::endl;
        }
    }
    std::cout << "};" << std::endl;
    std::cout << std::endl;
    
    // pinned eval
    std::cout << "EvalScore pinnedEval[15] {" << std::endl;
    for(int i = 0; i < 15; i++){
        if(i % 5 == 0){
            std::cout << "    ";
        }
        std::cout << "M(" << setw(5) << MgScore(pinnedEval[i]) << "," << setw(5) << EgScore(pinnedEval[i]) << "), ";
        if(i % 5 == 4){
            std::cout << std::endl;
        }
    }
    std::cout << "};" << std::endl;
    std::cout << std::endl;
    
    
    // passer eval
    std::cout << "EvalScore passer_rank_n[16] {" << std::endl;
    for(int i = 0; i < 16; i++){
        if(i % 8 == 0){
            std::cout << "    ";
        }
        std::cout << "M(" << setw(5) << MgScore(passer_rank_n[i]) << "," << setw(5) << EgScore(passer_rank_n[i]) << "), ";
        if(i % 8 == 7){
            std::cout << std::endl;
        }
    }
    std::cout << "};" << std::endl;
    std::cout << std::endl;

    // features
    for (int i = 0; i < 22; i++) {
        std::cout << "EvalScore " << setw(30) << left << feat_names[i] << right << "= M(" << setw(5) << MgScore(*evfeatures[i])
                  << "," <<setw(5) << EgScore(*evfeatures[i]) << ");" << std::endl;
    }
    std::cout << left;

    
    // king safety
    std::cout << "EvalScore kingSafetyTable[100] {" << std::endl;
    for (int i = 0; i < 100; i++) {
        if (i % 8 == 0) {
            std::cout << "    ";
        }
        std::cout << "M(" << setw(5) << right << MgScore(kingSafetyTable[i]) << "," << setw(5)
                  << EgScore(kingSafetyTable[i]) << "), ";
        if (i % 8 == 7) {
            std::cout << std::endl;
        }
    }
    std::cout << "};" << std::endl;
    std::cout << std::endl;
}

#endif