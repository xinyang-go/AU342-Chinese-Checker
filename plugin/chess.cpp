#include "chess.hpp"
#include <cstring>
#include <algorithm>
#include <random>
#include <thread>
#include <future>
#include <iostream>
#include <cassert>

static constexpr point_t steps[6] = {{0,  1},
                                     {0,  -1},
                                     {1,  0},
                                     {-1, 0},
                                     {1,  -1},
                                     {-1, 1},};

static constexpr point_t jumps[24] = {{0,  2},
                                      {0,  4},
                                      {0,  6},
                                      {0,  8},
                                      {0,  -2},
                                      {0,  -4},
                                      {0,  -6},
                                      {0,  -8},
                                      {2,  0},
                                      {4,  0},
                                      {6,  0},
                                      {8,  0},
                                      {-2, 0},
                                      {-4, 0},
                                      {-6, 0},
                                      {-8, 0},
                                      {2,  -2},
                                      {4,  -4},
                                      {6,  -6},
                                      {8,  -8},
                                      {-2, 2},
                                      {-4, 4},
                                      {-6, 6},
                                      {-8, 8},
};

static constexpr int jumps_cnt = sizeof(jumps) / sizeof(jumps[0]);

static constexpr int score7[10][10] = {
        {600, 220, 500, 429, 418, 403, 387, 367, 346, 321},
        {220, 220, 430, 419, 404, 388, 368, 347, 322, 296},
        {500, 430, 420, 405, 389, 369, 348, 323, 297, 267},
        {429, 419, 405, 390, 370, 349, 324, 298, 268, 237},
        {418, 404, 389, 370, 350, 325, 299, 269, 238, 203},
        {403, 388, 369, 349, 325, 300, 270, 239, 204, 168},
        {387, 368, 348, 324, 299, 270, 240, 205, 169, 129},
        {367, 347, 323, 298, 269, 239, 205, 170, 130, 89},
        {346, 322, 297, 268, 238, 204, 169, 130, 90,  45},
        {321, 296, 267, 237, 203, 168, 129, 89,  45,  0},
};

static constexpr int score3[10][10] = {
        {200, 600, 300, 429, 418, 403, 387, 367, 346, 321},
        {600, 600, 430, 419, 404, 388, 368, 347, 322, 296},
        {300, 430, 420, 405, 389, 369, 348, 323, 297, 267},
        {429, 419, 405, 390, 370, 349, 324, 298, 268, 237},
        {418, 404, 389, 370, 350, 325, 299, 269, 238, 203},
        {403, 388, 369, 349, 325, 300, 270, 239, 204, 168},
        {387, 368, 348, 324, 299, 270, 240, 205, 169, 129},
        {367, 347, 323, 298, 269, 239, 205, 170, 130, 89},
        {346, 322, 297, 268, 238, 204, 169, 130, 90,  45},
        {321, 296, 267, 237, 203, 168, 129, 89,  45,  0},
};

class auto_action_applier {
private:
    chess_t _chess;
    const point_t &_begin;
    const point_t &_end;
public:
    auto_action_applier(chess_t chess, const point_t &begin, const point_t &end)
            : _chess(chess), _begin(begin), _end(end) {
        // apply action
        _chess[_end.x][_end.y] = _chess[_begin.x][_begin.y];
        _chess[_begin.x][_begin.y] = 0;
    }

    ~auto_action_applier() {
        // resume action
        _chess[_begin.x][_begin.y] = _chess[_end.x][_end.y];
        _chess[_end.x][_end.y] = 0;
    }
};

static void get_curr_idx(int player, chess_ct chess, std::array<point_t, 10> &curr_idx) {
    int curr_idx_cnt = 0;
    for (int x = 0; x < 10; x++) {
        for (int y = 0; y < 10; y++) {
            if (chess[x][y] == player || chess[x][y] == player + 2) {
                curr_idx[curr_idx_cnt++] = {x, y};
            }
        }
    }
}

static inline bool is_out_of_range(const point_t &p) {
    return p.x < 0 || p.x >= 10 || p.y < 0 || p.y >= 10;
}

static inline bool is_empty_position(chess_ct chess, const point_t &p) {
    return chess[p.x][p.y] == 0;
}

static bool is_legal_jump(chess_ct chess, const point_t &curr, const point_t &jump, const point_t &end) {
    bool legal = true;
    if (jump.x == 0 && jump.y != 0) {
        int bias = jump.y > 0 ? 1 : -1;
        for (int y = curr.y + bias; y != end.y; y += bias) {
            if (y == (curr.y + end.y) / 2) {
                if (chess[end.x][y] == 0) {
                    legal = false;
                    break;
                }
            } else {
                if (chess[end.x][y] != 0) {
                    legal = false;
                    break;
                }
            }
        }
    } else if (jump.x != 0 && jump.y == 0) {
        int bias = jump.x > 0 ? 1 : -1;
        for (int x = curr.x + bias; x != end.x; x += bias) {
            if (x == (curr.x + end.x) / 2) {
                if (chess[x][end.y] == 0) {
                    legal = false;
                    break;
                }
            } else {
                if (chess[x][end.y] != 0) {
                    legal = false;
                    break;
                }
            }
        }
    } else {
        int bias_x = jump.x > 0 ? 1 : -1;
        int bias_y = jump.y > 0 ? 1 : -1;
        for (int x = curr.x + bias_x, y = curr.y + bias_y; x != end.x; x += bias_x, y += bias_y) {
            if (x == (curr.x + end.x) / 2) {
                if (chess[x][y] == 0) {
                    legal = false;
                    break;
                }
            } else {
                if (chess[x][y] != 0) {
                    legal = false;
                    break;
                }
            }
        }
    }
    return legal;
}

static bool is_same_action(const std::vector<action_t> &actions,
                           const point_t &begin, const point_t &end) {
    return std::any_of(actions.begin(), actions.end(), [&](auto &a) {
        return a.begin == begin && a.end == end;
    });
}

static void dfs_jumps(chess_t chess, const point_t &begin, const point_t &curr,
                      std::vector<action_t> &actions) {
    for (int i = 0; i < jumps_cnt; i++) {
        auto jump = jumps[i];
        auto end = curr + jump;
        if (is_out_of_range(end)) {
            i = i / 4 * 4 + 3;
            continue;
        }
        if (!is_empty_position(chess, end)) continue;
        if (begin == end) continue;
        if (!is_legal_jump(chess, curr, jump, end)) continue;
        if (is_same_action(actions, begin, end)) continue;
        actions.emplace_back(action_t{begin, end});
        auto_action_applier applier(chess, curr, end);
        dfs_jumps(chess, begin, end, actions);
        i = i / 4 * 4 + 3;
    }
}

std::vector<action_t> get_legal_action(int player, chess_t chess) {
    thread_local static std::vector<action_t> actions;
    actions.clear();
    actions.reserve(200);

    thread_local static std::array<point_t, 10> curr_idx{};
    get_curr_idx(player, chess, curr_idx);

    for (auto begin : curr_idx) {
        for (auto step : steps) {
            auto end = begin + step;
            if (is_out_of_range(end)) continue;
            if (!is_empty_position(chess, end)) continue;
            actions.emplace_back(action_t{begin, end});
        }
    }

    for (auto begin : curr_idx) {
        dfs_jumps(chess, begin, begin, actions);
    }

    return actions;
}

bool is_finish(int player, chess_ct chess) {
    if (player == 1) {
        if (chess[0][0] == 1 && chess[0][1] == 3 && chess[0][2] == 1 && chess[0][3] == 1 &&
            chess[1][0] == 3 && chess[1][1] == 3 && chess[1][2] == 1 &&
            chess[2][0] == 1 && chess[2][1] == 1 &&
            chess[3][0] == 1)
            return true;
    } else {
        if (chess[9][9] == 2 && chess[9][8] == 4 && chess[9][7] == 2 && chess[9][6] == 2 &&
            chess[8][9] == 4 && chess[8][8] == 4 && chess[8][7] == 2 &&
            chess[7][9] == 2 && chess[7][8] == 2 &&
            chess[6][9] == 2)
            return true;
    }
    return false;
}

static int evaluate_chess(int player, chess_ct chess) {
    int value1 = 0, value2 = 0;

    for (int i = 0; i < 100; i++) {
        int x = i / 10;
        int y = i % 10;
        if (chess[x][y] == 1) {
            value1 += score7[x][y];
        } else if (chess[x][y] == 3) {
            value1 += score3[x][y];
        } else if (chess[x][y] == 2) {
            value2 += score7[9 - x][9 - y];
        } else if (chess[x][y] == 4) {
            value2 += score3[9 - x][9 - y];
        }
    }

    if (player == 1) {
        return value1 - value2 / 2;
    } else {
        return value2 - value1 / 2;
    }
}

static bool is_without_opponent(chess_ct chess) {
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

void sort_actions(int player, std::vector<action_t> &actions) {
    if (player == 1) {
        std::sort(actions.begin(), actions.end(), [](const action_t &a1, const action_t &a2) {
            int dist1 = a1.end.x + a1.end.y - a1.begin.x - a1.begin.y;
            int dist2 = a2.end.x + a2.end.y - a2.begin.x - a2.begin.y;
            return dist1 < dist2;
        });
    } else {
        std::sort(actions.begin(), actions.end(), [](const action_t &a1, const action_t &a2) {
            int dist1 = a1.end.x + a1.end.y - a1.begin.x - a1.begin.y;
            int dist2 = a2.end.x + a2.end.y - a2.begin.x - a2.begin.y;
            return dist1 > dist2;
        });
    }
}

std::tuple<int, action_t>
MinMaxAgent::minmax(int current_player, chess_t chess, int alpha, int beta, int depth, int without_opponent) {
    auto legal_actions = get_legal_action(current_player, chess);
    int searching_cnt = std::min(max_search_actions_cnt, legal_actions.size());

    if (enable_sort_actions) sort_actions(current_player, legal_actions);

    int val{0}, best_val{std::numeric_limits<int>::min()};
    action_t best_action{};

    for (int i = 0; i < searching_cnt; i++) {
        const auto &action = legal_actions[i];

        // apply current action & resume automatically
        auto_action_applier applier(chess, action.begin, action.end);

        if (is_finish(current_player, chess)) {
            // finish game
            return {value_max + max_search_depth, action};
        } else if (depth == 0) {
            // evaluate current status
            val = evaluate_chess(current_player, chess);
        } else if (without_opponent) {
            // step into myself
            auto[v, a] = minmax(current_player, chess, alpha, beta, depth - 1, without_opponent);
            val = v;
        } else {
            // step into opponent
            auto[v, a] = minmax(3 - current_player, chess, -beta, -alpha, depth - 1, without_opponent);
            val = -v;
        }
        // value decrease by depth
        val -= 1;

        // alpha-beta tuning
        if (val >= beta) {
            return {beta, action};
        }
        // update alpha
        if (val > alpha) {
            alpha = val;
        }
        // update best action
        if (val > best_val) {
            best_val = val;
            best_action = action;
        }
    }
    return {best_val, best_action};
}

action_t MinMaxAgent::run_normal(chess_t chess) {
    bool without_opponent = enable_without_opponent && is_without_opponent(chess);
    auto[val, action] = minmax(player, chess, value_min, value_max,
                               without_opponent ? max_search_depth_without_opponent : max_search_depth,
                               without_opponent);
    return action;
}



