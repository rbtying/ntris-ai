#!/usr/bin/env python
#
# This client connects to the centralized game server
# via http. After creating a new game on the game
# server, it spaws an AI subprocess called "dropblox_ai."
# For each turn, this client passes in the current game
# state to a new instance of dropblox_ai, waits ten seconds
# for a response, then kills the AI process and sends
# back the move list.
#

import contextlib
import hashlib
import os
import platform
import sys
import threading
import time
import urllib2
from subprocess import Popen, PIPE

import json

import messaging
import util
from logic.Board import Board

# Remote server to connect to:
PROD_HOST = 'playdropblox.com'
PROD_PORT = 443
PROD_SSL = True

is_windows = platform.system() == "Windows"

# Subprocess
LEFT_CMD = 'left'
RIGHT_CMD = 'right'
UP_CMD = 'up'
DOWN_CMD = 'down'
ROTATE_CMD = 'rotate'
VALID_CMDS = [LEFT_CMD, RIGHT_CMD, UP_CMD, DOWN_CMD, ROTATE_CMD]
AI_PROCESS_PATH = os.path.join(os.getcwd(), 'dropblox_ai' if not is_windows else 'dropblox_ai.bat')

# Printing utilities
colorred = "\033[01;31m{0}\033[00m" if not is_windows else "{0}"
colorgrn = "\033[1;36m{0}\033[00m" if not is_windows else "{0}"

# Logging AI actions for debug webserver
LOGGING_DIR = os.path.join(os.getcwd(), 'history')

class Command(object):
    def __init__(self, cmd, *args):
        self.cmd = cmd
        self.args = list(args)

    def run(self, timeout):
        cmds = []
        is_windows = platform.system() == "Windows"
        process = Popen([self.cmd] + self.args, stdout=PIPE, universal_newlines=True, shell=is_windows)
        def target():
            for line in iter(process.stdout.readline, ''):
                line = line.rstrip('\n')
                if line not in VALID_CMDS:
                    print 'INVALID COMMAND:', line # Forward debug output to terminal
                else:
                    cmds.append(line)

        thread = threading.Thread(target=target)
        thread.start()

        thread.join(timeout)
        print colorred.format('Terminating process')
        try:
            process.terminate()
            thread.join(60)
        except Exception:
            pass
        print colorgrn.format('commands received: %s' % cmds)
        return cmds

class GameStateLogger(object):
    log_dir = None
    turn_num = 0

    def __init__(self, game_id):
        self.log_dir = os.path.join(LOGGING_DIR, '%s_%s' % (game_id, int(time.time())))
        if not os.path.exists(self.log_dir):
            os.makedirs(self.log_dir)

    def log_game_state(self, game_state):
        fname = os.path.join(self.log_dir, 'state%s' % (self.turn_num,))
        with open(fname, 'w+') as f:
            f.write(game_state)

    def log_ai_move(self, move_list):
        fname = os.path.join(self.log_dir, 'move%s' % (self.turn_num,))
        with open(fname, 'w+') as f:
            f.write(move_list)
        self.turn_num += 1

class AuthException(Exception):
    pass

class GameOverError(Exception):
    def __init__(self, game_state_dict):
        self.game_state_dict = game_state_dict

class DropbloxServer(object):
    def __init__(self, team_name, team_password, host, port, ssl):
        # maybe support any transport
        # but whatever
        self.host = host
        self.port = port
        self.ssl = ssl

        self.team_name = team_name
        self.team_password = team_password
        
    def _request(self, path, tbd):
        schema = 'https' if self.ssl else 'http'
        url = '%s://%s:%d%s' % (schema, self.host, self.port, path)
        
        tbd = dict(tbd)
        tbd['team_name'] = self.team_name
        tbd['password'] = self.team_password
        data = json.dumps(tbd)

        req = urllib2.Request(url, data, {
                'Content-Type': 'application/json'
                })

        try:
            with contextlib.closing(urllib2.urlopen(req)) as resp:
                if resp.getcode() != 200:
                    raise Exception("Bad response: %r" % resp.getcode())
                return json.loads(resp.read())
        except urllib2.HTTPError, err:
           if err.code == 401:
               raise AuthException()
           else:
               raise

    def create_practice_game(self):
        return self._request("/create_practice_game", {})

    def get_compete_game(self):
        # return None if game is not ready to go yet
        resp = self._request("/get_compete_game", {})
        return resp

    def submit_game_move(self, game_id, move_list, moves_made):
        resp = self._request("/submit_game_move", {
                'game_id': game_id,
                'move_list': move_list,
                'moves_made': moves_made,
                })
        if resp['ret'] == 'ok':
            return resp

        if resp['ret'] == 'fail':
            if resp['code'] == messaging.CODE_GAME_OVER:
                raise GameOverError(resp['game_state'])
            else:
                raise Exception("Bad move: %r:%r",
                                resp['code'], resp['reason'])
        
        raise Exception("Bad response: %r" % (resp,))

class LocalServer(object):
    def __init__(self, seed):
      self.seed = seed
    def submit_game_move(self, game_id, move_list, moves_made):
        # update local state, return json object
        game_state = self.game
        made_move = False
        if game_state.state != 'failed':
            if time.time() - self.game.game_started_at > util.AI_CLIENT_TIMEOUT:
                game_state.state = 'failed'
            else:
                game_state.send_commands(move_list)
                made_move = True
        game_state.total_steps += int(bool(made_move))

        if game_state.state == 'failed':
            raise GameOverError(game_state.to_dict())
        return dict(
            ret='ok',
            game=dict(
                id=0,
                number_moves_made=game_state.total_steps,
                game_state=game_state.to_dict(),
            ),
            competition_seconds_remaining=util.AI_CLIENT_TIMEOUT - (time.time() - self.game.game_started_at) - 1,
            )

    def get_local_game(self):
        # return a json object
        self.game = Board(self.seed)
        self.game.game_started_at = time.time()
        self.game.game_id = 0
        self.game.total_steps = 0
        return dict(
            ret='ok',
            game=dict(
                id=0,
                number_moves_made=self.game.total_steps,
                game_state=self.game.to_dict(),
            ),
            competition_seconds_remaining=util.AI_CLIENT_TIMEOUT-1,
            )

def run_ai(game_state_dict, seconds_remaining, logger=None):
    ai_arg_one = json.dumps(game_state_dict)
    ai_arg_two = json.dumps(seconds_remaining)
    if logger is not None:
        logger.log_game_state(ai_arg_one)
    command = Command(AI_PROCESS_PATH, ai_arg_one, ai_arg_two)
    ai_cmds = command.run(timeout=float(ai_arg_two))
    if logger is not None:
        logger.log_ai_move(json.dumps(ai_cmds))
    return ai_cmds

def run_game(server, game, use_logger=True):
    game_id = game['game']['id']
    moves_made = game['game']['number_moves_made']

    logger = GameStateLogger(game_id) if use_logger else None

    while True:
        ai_cmds = run_ai(game['game']['game_state'],
                         game['competition_seconds_remaining'],
                         logger=logger)

        try:
            game = server.submit_game_move(game_id, ai_cmds, moves_made)
        except GameOverError, e:
            final_game_state_dict = e.game_state_dict
            break
        moves_made += 1
        print 'moves_made:', moves_made, 'score:', game['game']['game_state']['score'],
        print 'seconds_remaining:', game['competition_seconds_remaining']

    if logger is not None:
        logger.log_game_state(json.dumps(final_game_state_dict))

    print colorgrn.format("Game over! Your score was: %s" %
                          (final_game_state_dict['score'],))
    print "RESULTS: %s" % final_game_state_dict['score']
    print "RESULTS_TIME: %s" % moves_made

def run_compete(server):
    # TODO: it might be better for this to be an actual game object
    #       instead of the dictionary serialization of it
    new_game = server.get_compete_game()

    # HAX: didn't have time to clean up this abstraction
    if new_game['ret'] == 'wait':
        wait_time = float(new_game.get('wait_time', 0.5))
        print colorred.format("Waiting to compete...")

    while new_game['ret'] == 'wait':
        time.sleep(wait_time)

        new_game = server.get_compete_game()
        # HAX: didn't have time to clean up this abstraction
        if new_game['ret'] == 'wait':
            wait_time = float(new_game.get('wait_time', 0.5))

    print colorred.format("Fired up and ready to go!")
    run_game(server, new_game, use_logger=True)

def run_practice(server):
    # TODO: it might be better for this to be an actual game object
    #       instead of the dictionary serialization of it
    new_game = server.create_practice_game()
    run_game(server, new_game)

def run_local(seed):
    # create practice games, run game
    print "running local"
    server = LocalServer(seed)
    run_game(server, server.get_local_game())

def main(argv):
    if len(argv) < 2:
        print colorred.format("Usage: client.py [compete|practice|local] <seed>")
        sys.exit(0)

    seed = None
    if len(argv) == 3:
      seed = int(argv[2])
    entry_mode = argv[1]
    if entry_mode not in ('compete', 'practice', 'local'):
        print colorred.format("Usage: client.py [compete|practice|local]")     
        sys.exit(0)

    if entry_mode in ('compete', 'practice'):
        with open('config.txt', 'r') as f:
            team_name = f.readline().rstrip('\n')
            team_password = f.readline().rstrip('\n')
        if team_name == "TEAM_NAME_HERE" or team_password == "TEAM_PASSWORD_HERE":
            print colorred.format("Please specify a team name and password in config.txt")
            sys.exit(0)

        if (hashlib.md5(os.environ.get('DROPBLOX_DEBUG', '')).digest() ==
            '\x98w\x01\x0b%O\x08\xfa\x07\xe8\xa3\xe6]\xe9\xf0\xeb'):
            connect_details = ('localhost', 8080, False)
        else:
            connect_details = (PROD_HOST, PROD_PORT, PROD_SSL)
        server = DropbloxServer(team_name, team_password, *connect_details)

        try:
            if entry_mode == "practice":
                run_practice(server)
                return 0
            elif entry_mode == "compete":
                run_compete(server)
                return 0
            else:
                assert False, 'wtf? mode = %s' % entry_mode
        except AuthException:
            print colorred.format("Cannot authenticate, please check config.txt")
    elif entry_mode == 'local':
        run_local(seed)
        return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
