//
// Created by xinyang on 2020/9/22.
//

#ifndef PLUGIN_CHESS_HPP
#define PLUGIN_CHESS_HPP

#include <vector>
#include <limits>

constexpr int DEFAULT_MAX_DEPTH = 2;
constexpr int value_min = -(1 << 30);
constexpr int value_max = (1 << 30);

struct point_t {
    int x, y;
};

struct action_t {
    point_t begin, end;
};

using chess_t = int (*)[10];
using chess_ct = const int (*)[10];

inline point_t operator+(const point_t &p1, const point_t &p2) {
    return {p1.x + p2.x, p1.y + p2.y};
}

inline bool operator==(const point_t &p1, const point_t &p2) {
    return p1.x == p2.x && p1.y == p2.y;
}

bool is_finish(int player, chess_ct chess);

std::vector<action_t> get_legal_action(int player, chess_t chess);


class MinMaxAgent {
public:
    std::size_t max_search_actions_cnt{std::numeric_limits<std::size_t>::max()};
    std::size_t max_search_depth{DEFAULT_MAX_DEPTH};
    std::size_t max_search_depth_without_opponent{DEFAULT_MAX_DEPTH - 1};
    bool enable_sort_actions{false};
    bool enable_without_opponent{false};

private:
    int player;

    std::tuple<int, action_t>
    minmax(int current_player, chess_t chess, int alpha, int beta, int depth, int without_opponent);

public:
    explicit MinMaxAgent(int p) : player(p) {};

    action_t run_normal(chess_t chess);

    action_t run_parallel(chess_t chess);
};

#endif //PLUGIN_CHESS_HPP
