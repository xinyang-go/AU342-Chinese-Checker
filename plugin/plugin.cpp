//
// Created by xinyang on 2020/9/22.
//

#include "chess.hpp"

static MinMaxAgent agents[2] = {MinMaxAgent{1}, MinMaxAgent{2}};

extern "C" void init_agent(int player, int max_search_depth, int max_search_depth_without_opponent,
                           int max_search_actions_cnt, bool enable_sort_actions, bool enable_without_opponent) {
    agents[player - 1].max_search_depth = max_search_depth;
    agents[player - 1].max_search_actions_cnt = max_search_actions_cnt;
    agents[player - 1].max_search_depth_without_opponent = max_search_depth_without_opponent;
    agents[player - 1].enable_sort_actions = enable_sort_actions;
    agents[player - 1].enable_without_opponent = enable_without_opponent;
}

extern "C" void alpha_beta_minmax(int player, int chess[10][10], int best_actions[2][2]) {
    auto action = agents[player - 1].run_normal(chess);
    best_actions[0][0] = action.begin.x;
    best_actions[0][1] = action.begin.y;
    best_actions[1][0] = action.end.x;
    best_actions[1][1] = action.end.y;
}

extern "C" void get_actions(int player, int chess[10][10], int actions[200][2][2], int *actions_cnt) {
    *actions_cnt = 0;
    for (const auto &a: get_legal_action(player, chess)) {
        actions[*actions_cnt][0][0] = a.begin.x;
        actions[*actions_cnt][0][1] = a.begin.y;
        actions[*actions_cnt][1][0] = a.end.x;
        actions[*actions_cnt][1][1] = a.end.y;
        (*actions_cnt)++;
    }
}
