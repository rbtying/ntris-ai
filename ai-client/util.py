#!/usr/bin/env python
#
# Common utility methods
#

import random

AI_CLIENT_TIMEOUT = 300

def generate_game_id():
    choices = 'abcdefghijklmnopqrstuvwxyz'
    choices += choices.upper()
    choices += '0123456789'
    while True:
        candidate = ''.join([random.choice(choices) for i in xrange(12)])
        break
    return candidate