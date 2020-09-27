#include "chess.hpp"
#include "thread_pools.hpp"
#include <algorithm>
#include <random>
#include <chrono>
#include <cstring>

static constexpr point_t steps[6] = {{0,  1},
                                     {0,  -1},
                                     {1,  0},
                                     {-1, 0},
                                     {1,  -1},
                                     {-1, 1},};

static constexpr point_t directions[] = {
        {0,  1},
        {0,  -1},
        {1,  0},
        {-1, 0},
        {1,  -1},
        {-1, 1}
};

static constexpr int score7[10][10] = {
        {400, 0,   317, 280, 245, 213, 184, 157, 132, 110},
        {0,   0,   286, 250, 218, 188, 160, 135, 112, 92},
        {317, 286, 256, 222, 192, 163, 138, 114, 94,  75},
        {280, 250, 222, 196, 167, 141, 117, 96,  76,  60},
        {245, 218, 192, 167, 144, 119, 98,  78,  61,  46},
        {213, 188, 163, 141, 119, 100, 80,  62,  47,  34},
        {184, 160, 138, 117, 98,  80,  64,  48,  35,  24},
        {157, 135, 114, 96,  78,  62,  48,  36,  24,  15},
        {132, 112, 94,  76,  61,  47,  35,  24,  16,  8},
        {110, 92,  75,  60,  46,  34,  24,  15,  8,   4},
};

static constexpr int score3[10][10] = {
        {0,   357, 100, 280, 245, 213, 184, 157, 132, 110},
        {357, 324, 286, 250, 218, 188, 160, 135, 112, 92},
        {100, 286, 256, 222, 192, 163, 138, 114, 94,  75},
        {280, 250, 222, 196, 167, 141, 117, 96,  76,  60},
        {245, 218, 192, 167, 144, 119, 98,  78,  61,  46},
        {213, 188, 163, 141, 119, 100, 80,  62,  47,  34},
        {184, 160, 138, 117, 98,  80,  64,  48,  35,  24},
        {157, 135, 114, 96,  78,  62,  48,  36,  24,  15},
        {132, 112, 94,  76,  61,  47,  35,  24,  16,  8},
        {110, 92,  75,  60,  46,  34,  24,  15,  8,   4},
};

static thread_pool::static_pool pool;

static std::default_random_engine e(std::chrono::system_clock::now().time_since_epoch().count());

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

void dfs_jumps(chess_t chess, const point_t &begin, const point_t &curr, std::vector<action_t> &actions) {
    // 遍历每个可以跳的方向
    for (const auto &direction : directions) {
        bool finding_bridge = true;
        int before_bridge_len = 0, after_bridge_len = 0;
        // 遍历这个方向上的棋盘
        for (point_t end = curr + direction; !is_out_of_range(end); end += direction) {
            if (finding_bridge) {
                if (chess[end.x][end.y] == 0) {
                    before_bridge_len++;
                } else {
                    finding_bridge = false;
                }
            } else {
                if (chess[end.x][end.y] == 0) {
                    if (after_bridge_len == before_bridge_len) {
                        if (begin != end && !is_same_action(actions, begin, end)) {
                            // 找到一个合法的跳法
                            actions.emplace_back(action_t{begin, end});
                            auto_action_applier applier(chess, curr, end);
                            dfs_jumps(chess, begin, end, actions);
                        }
                        break;
                    } else {
                        after_bridge_len++;
                    }
                } else {
                    break;
                }
            }
        }
    }
}

std::vector<action_t> get_legal_action(int player, chess_t chess) {
    std::vector<action_t> actions;
    actions.reserve(200);

    std::array<point_t, 10> curr_idx{};
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
        return value1 - value2;
    } else {
        return value2 - value1;
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

std::tuple<int, std::vector<action_t>>
MinMaxAgent::minmax_search(int current_player, chess_t chess,
                           std::vector<action_t>::const_iterator begin, std::vector<action_t>::const_iterator end,
                           int alpha, int beta, int depth, int without_opponent) {
    int val{0}, best_val{std::numeric_limits<int>::min()};
    std::vector<action_t> best_actions;
    best_actions.reserve(end - begin);

    for (auto iter = begin; iter != end; iter++) {
        const auto &action = *iter;
        // apply current action & resume automatically
        auto_action_applier applier(chess, action.begin, action.end);

        if (is_finish(current_player, chess)) {
            // finish game
            return {value_max + (int) max_search_depth, {action}};
        } else if (depth == 0) {
            // evaluate current status
            val = evaluate_chess(current_player, chess);
        } else if (without_opponent) {
            // step into myself
            auto[v, a] = minmax_normal(current_player, chess, alpha, beta, depth - 1, without_opponent);
            val = v;
        } else {
            // step into opponent
            auto[v, a] = minmax_normal(3 - current_player, chess, -beta, -alpha, depth - 1, without_opponent);
            val = -v;
        }
        // value decrease by depth
        val -= 1;

        // alpha-beta tuning
        if (val >= beta) {
            return {beta, {action}};
        }
        // update alpha
        if (val > alpha) {
            alpha = val;
        }
        // update best action
        if (val > best_val) {
            best_val = val;
        }
        if (val == best_val) {
            best_actions.emplace_back(action);
        }
    }
    return {best_val, best_actions};
}

std::tuple<int, std::vector<action_t>>
MinMaxAgent::minmax_normal(int current_player, chess_t chess, int alpha, int beta, int depth, int without_opponent) {
    auto legal_actions = get_legal_action(current_player, chess);
    int searching_cnt = std::min(max_search_actions_cnt, legal_actions.size());

    if (enable_sort_actions) sort_actions(current_player, legal_actions);

    auto begin = legal_actions.begin();
    auto end = legal_actions.begin() + searching_cnt;

    return minmax_search(current_player, chess, begin, end, alpha, beta, depth, without_opponent);
}

std::tuple<int, std::vector<action_t>>
MinMaxAgent::minmax_parallel(int current_player, chess_t chess, int alpha, int beta, int depth, int without_opponent) {
    auto legal_actions = get_legal_action(current_player, chess);
    int searching_cnt = std::min(max_search_actions_cnt, legal_actions.size());

    if (enable_sort_actions) sort_actions(current_player, legal_actions);

    size_t n_cpu = std::thread::hardware_concurrency();
    size_t part = searching_cnt / n_cpu;
    size_t remain = searching_cnt - part * n_cpu;

    size_t idx = 0;
    std::future<std::tuple<int, std::vector<action_t>>> futures[n_cpu];
    for (int i = 0; i < n_cpu; i++) {
        auto begin = legal_actions.begin() + idx;
        idx += part + (i < remain ? 1 : 0);
        auto end = legal_actions.begin() + idx;
        futures[i] = pool.enqueue([&](auto p, auto c, auto b, auto e, auto v1, auto v2, auto d, auto w) {
            int chess_copy[10][10];
            memcpy(chess_copy, c, sizeof(int[10][10]));
            return minmax_search(p, chess_copy, b, e, v1, v2, d, w);
        }, player, chess, begin, end, alpha, beta, depth, without_opponent);
    }

    int best_val{std::numeric_limits<int>::min()};
    std::vector<action_t> best_actions;
    best_actions.reserve(legal_actions.size());
    for (int i = 0; i < n_cpu; i++) {
        auto[val, actions] = futures[i].get();
        if (val > best_val) {
            best_val = val;
            best_actions.clear();
        }
        if (val == best_val) {
            best_actions.insert(best_actions.end(), actions.begin(), actions.end());
        }
    }

    return {best_val, best_actions};
}

std::tuple<int, action_t> MinMaxAgent::run_normal(chess_t chess) {
    bool without_opponent = enable_without_opponent && is_without_opponent(chess);
    int depth = without_opponent ? max_search_depth_without_opponent : max_search_depth;
    auto[val, best_actions] = minmax_normal(player, chess, value_min, value_max, depth, without_opponent);

    std::uniform_int_distribution<size_t> u(0, best_actions.size() - 1);
    return {val, best_actions[u(e)]};
}

std::tuple<int, action_t> MinMaxAgent::run_parallel(chess_t chess) {
    bool without_opponent = enable_without_opponent && is_without_opponent(chess);
    int depth = without_opponent ? max_search_depth_without_opponent : max_search_depth;
    auto[val, best_actions] = minmax_parallel(player, chess, value_min, value_max, depth, without_opponent);

    std::uniform_int_distribution<size_t> u(0, best_actions.size() - 1);
    return {val, best_actions[u(e)]};
}


