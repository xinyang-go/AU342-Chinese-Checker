from ctypes import *
from util import *
import numpy as np
import numpy.ctypeslib as ct
import numba as nb
import random


class Agent(object):
    def __init__(self, game):
        self.game = game
        self.action = None

    def getAction(self, state):
        raise Exception("Not implemented yet")


class RandomAgent(Agent):
    def getAction(self, state):
        legal_actions = self.game.actions(state)
        self.action = random.choice(legal_actions)


class SimpleGreedyAgent(Agent):
    # a one-step-lookahead greedy agent that returns action with max vertical advance
    def getAction(self, state):
        legal_actions = self.game.actions(state)

        self.action = random.choice(legal_actions)

        player = self.game.player(state)
        if player == 1:
            max_vertical_advance_one_step = max([action[0][0] - action[1][0] for action in legal_actions])
            max_actions = [action for action in legal_actions if
                           action[0][0] - action[1][0] == max_vertical_advance_one_step]
        else:
            max_vertical_advance_one_step = max([action[1][0] - action[0][0] for action in legal_actions])
            max_actions = [action for action in legal_actions if
                           action[1][0] - action[0][0] == max_vertical_advance_one_step]
        self.action = random.choice(max_actions)


class XinMinimaxAgent(Agent):
    def __init__(self, game, player,
                 max_search_depth=4,
                 max_search_depth_without_opponent=3,
                 max_search_actions_cnt=32,
                 enable_sort_actions=True,
                 enable_without_opponent=True):
        super(XinMinimaxAgent, self).__init__(game)
        self.player = player
        self.plugin = ct.load_library("libplugin", "./plugin/lib")
        self.plugin.alpha_beta_minmax.argtypes = [
            c_int32,
            ct.ndpointer(np.int32, 2, (10, 10), "C_CONTIGUOUS"),
            ct.ndpointer(np.int32, 2, (2, 2), "C_CONTIGUOUS"),
        ]
        self.plugin.init_agent.argtypes = [
            c_int32, c_int32, c_int32, c_int32, c_bool, c_bool
        ]
        self.plugin.get_actions.argtypes = [
            c_int32,
            ct.ndpointer(np.int32, 2, (10, 10), "C_CONTIGUOUS"),
            ct.ndpointer(np.int32, 3, (200, 2, 2), "C_CONTIGUOUS"),
            POINTER(c_int32)
        ]
        self.plugin.init_agent(
            c_int32(player), c_int32(max_search_depth),
            c_int32(max_search_depth_without_opponent),
            c_int32(max_search_actions_cnt),
            c_bool(enable_sort_actions),
            c_bool(enable_without_opponent))
        self.getAction((player, self.game.startState()[1]))

    @nb.jit(forceobj=True)
    def getAction(self, state):
        assert self.player == state[0], "player id does not match agent's id"
        chess = np.zeros((10, 10), dtype=np.int32)
        for pos, v in state[1].board_status.items():
            x, y = pos2idx[pos[0], pos[1]]
            chess[x, y] = v

        # for debug
        # my_actions = np.zeros([200, 2, 2], dtype=np.int32)
        # my_actions_cnt = c_int32(0)
        # self.plugin.get_actions(c_int32(state[0]), chess, my_actions, pointer(my_actions_cnt))
        # assert my_actions_cnt.value == len(self.game.actions(state))

        best_action = np.zeros((2, 2), dtype=np.int32)
        self.plugin.alpha_beta_minmax(c_int32(state[0]), chess, best_action, )
        begin, end = best_action
        begin = tuple(idx2pos[begin[0], begin[1]].tolist())
        end = tuple(idx2pos[end[0], end[1]].tolist())
        self.action = (begin, end)
