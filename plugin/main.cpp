//
// Created by xinyang on 2020/9/22.
//

#include "chess.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <cstring>
#include <tuple>
#include <algorithm>
#include <iomanip>
#include <map>

static void print_chess(int chess[10][10]) {
    for (int x = 0; x < 10; x++) {
        for (int y = 0; y < 10; y++) {
            std::cout << chess[x][y] << " ";
        }
        std::cout << std::endl;
    }
}

static inline std::ostream &operator<<(std::ostream &out, point_t p) {
    return out << "[" << p.x << ", " << p.y << "]";
}

static inline std::ostream &operator<<(std::ostream &out, action_t a) {
    return out << a.begin << "-->" << a.end;
}

static bool is_max_only(int chess[10][10]) {
    int p1_min = std::numeric_limits<int>::max();
    int p1_max = std::numeric_limits<int>::min();
    int p2_min = std::numeric_limits<int>::max();
    int p2_max = std::numeric_limits<int>::min();
    for (int x = 0; x < 10; x++) {
        for (int y = 0; y < 10; y++) {
            if (chess[x][y] == 1 || chess[x][y] == 3) {
                p1_min = std::min(x + y, p1_min);
                p1_max = std::max(x + y, p1_max);
            } else if (chess[x][y] == 2 || chess[x][y] == 4) {
                p2_min = std::min(x + y, p2_min);
                p2_max = std::max(x + y, p2_max);
            }
        }
    }
    return p1_min >= p2_max || p1_max < p2_min;
}

std::map<int, int> jumps_cnt;

int conflict[65536] = {0};

template<int MOD>
class HashChess {
public:
    std::size_t operator()(int chess[10][10]) {
        int *ptr = (int *) chess;
        std::size_t val = 0;
        for (int x = 0; x < 100; x++) {
            val = val * 31 + ptr[x] + 7;
        }
        return val % MOD;
    }
};

int main(int argc, char *argv[]) {
    bool normal_mode = false;
    if (argc > 1) {
        if (strcmp("parallel", argv[1]) == 0) {
            normal_mode = false;
        } else if (strcmp("normal", argv[1]) == 0) {
            normal_mode = true;
        } else {
            std::cout << "usage: " << argv[0] << " [mode=parallel|normal, default=parallel]" << std::endl;
            return -1;
        }
    }

    MinMaxAgent agent1(1), agent2(2);
    agent1.max_search_actions_cnt = 100;
    agent1.max_search_depth = 2;
    agent1.max_search_depth_without_opponent = 2;
    agent1.enable_sort_actions = true;
    agent1.enable_without_opponent = false;

    agent2.max_search_actions_cnt = 36;
    agent2.max_search_depth = 4;
    agent1.max_search_depth_without_opponent = 3;
    agent2.enable_sort_actions = true;
    agent2.enable_without_opponent = true;

    int chess[10][10] = {
            //0,1, 2, 3, 4, 5, 6, 7, 8, 9
            {2, 4, 2, 2, 0, 0, 0, 0, 0, 0}, // 0
            {4, 4, 2, 0, 0, 0, 0, 0, 0, 0}, // 1
            {2, 2, 0, 0, 0, 0, 0, 0, 0, 0}, // 2
            {2, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 3
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 4
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 5
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // 6
            {0, 0, 0, 0, 0, 0, 0, 0, 1, 1}, // 7
            {0, 0, 0, 0, 0, 0, 0, 1, 3, 3}, // 8
            {0, 0, 0, 0, 0, 0, 1, 1, 3, 1}, // 9
    };

    int player = 1;
    int step = 0;
    action_t best_action{};
    while (!is_finish(1, chess) && !is_finish(2, chess)) {
        step += 1;
        if (step > 200) return -1;
        auto t1 = std::chrono::system_clock::now();
        if (player == 1) {
            best_action = agent1.run_normal(chess);
        } else {
            best_action = agent2.run_normal(chess);
        }
        auto t2 = std::chrono::system_clock::now();

        auto action = best_action;
        auto[begin, end] = action;
        chess[end.x][end.y] = chess[begin.x][begin.y];
        chess[begin.x][begin.y] = 0;

        if (t2 - t1 >= std::chrono::milliseconds(950)) {
            std::cout << "[WARNING]: timeout! "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "ms"
                      << std::endl;
        }

//        std::cout << "step: " << step << std::endl;
//        std::cout << (normal_mode ? "normal: " : "parallel: ")
//                  << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "ms"
//                  << std::endl;
//        std::cout << "player-" << player << ": " << action << std::endl;
//        print_chess(chess);
//        std::cout << "============================" << std::endl;

        player = 3 - player;
    }
    if (is_finish(1, chess)) {
        std::cout << "player-1 win in " << step << " steps." << std::endl;
    } else {
        std::cout << "player-2 win in " << step << " steps." << std::endl;
    }
    return 0;
}
