from agent import *
from game import ChineseChecker
import datetime
import tkinter as tk
from UI import GameBoard
import time


# 1 seconds
def timeout(func, param, timeout_duration=1, default=None):
    import signal

    class TimeoutError(Exception):
        pass

    def handler(signum, frame):
        raise TimeoutError()

    # set the timeout handler
    signal.signal(signal.SIGALRM, handler)
    signal.alarm(timeout_duration)
    try:
        result = func(param)
    except TimeoutError as exc:
        result = default
        print("[WARNING]: timeout!")
    finally:
        signal.alarm(0)


def runGame(ccgame, agents):
    state = ccgame.startState()
    # print(state)
    max_iter = 200  # deal with some stuck situations
    iter = 0
    start = datetime.datetime.now()
    while (not ccgame.isEnd(state, iter)) and iter < max_iter:
        iter += 1
        board.board = state[1]
        board.draw()
        board.update_idletasks()
        board.update()

        player = ccgame.player(state)
        agent = agents[player]
        # function agent.getAction() modify class member action
        # 1 s return
        timeout(agent.getAction, state)
        legal_actions = ccgame.actions(state)
        if agent.action not in legal_actions:
            print("invalid action: ", agent.action)
            agent.action = random.choice(legal_actions)
        state = ccgame.succ(state, agent.action)
        # input()
    board.board = state[1]
    board.draw()
    board.update_idletasks()
    board.update()
    time.sleep(0.1)
    end = datetime.datetime.now()
    if ccgame.isEnd(state, iter):
        return state[1].isEnd(iter)[1]  # return winner
    else:  # stuck situation
        print('stuck!')
        return 0


def simulateMultipleGames(agents_dict, simulation_times, ccgame):
    win_times_P1 = 0
    win_times_P2 = 0
    tie_times = 0
    utility_sum = 0
    for i in range(simulation_times):
        run_result = runGame(ccgame, agents_dict)
        print(run_result)
        if run_result == 1:
            win_times_P1 += 1
        elif run_result == 2:
            win_times_P2 += 1
        elif run_result == 0:
            tie_times += 1
        print('game', i + 1, 'finished', 'winner is player ', run_result)
    print('In', simulation_times, 'simulations:')
    print('winning times: for player 1 is ', win_times_P1)
    print('winning times: for player 2 is ', win_times_P2)
    print('Tie times:', tie_times)


def callback(ccgame):
    B.destroy()
    simpleGreedyAgent1 = SimpleGreedyAgent(ccgame)
    simpleGreedyAgent2 = SimpleGreedyAgent(ccgame)
    teamAgent1 = TeamNameMinimaxAgent(ccgame, player=1,
                                      max_search_depth=4,
                                      max_search_depth_without_opponent=3,
                                      max_search_actions_cnt=32,
                                      enable_sort_actions=True,
                                      enable_without_opponent=True)
    teamAgent2 = TeamNameMinimaxAgent(ccgame, player=2,
                                      max_search_depth=2,
                                      max_search_depth_without_opponent=2,
                                      max_search_actions_cnt=200,
                                      enable_sort_actions=True,
                                      enable_without_opponent=False)
    simulateMultipleGames({1: teamAgent1, 2: simpleGreedyAgent2}, 10, ccgame)


if __name__ == '__main__':
    ccgame = ChineseChecker(10, 4)
    root = tk.Tk()
    board = GameBoard(root, ccgame.size, ccgame.size * 2 - 1, ccgame.board)
    board.pack(side="top", fill="both", expand="true", padx=4, pady=4)
    B = tk.Button(board, text="Start", command=lambda: callback(ccgame=ccgame))
    B.pack()
    root.mainloop()
