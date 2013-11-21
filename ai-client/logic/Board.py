from copy import deepcopy
import math
import random

from Block import (
  COLS,
  ROWS,
  types,
  blocks,
)

LEVEL_INTERVAL = 60
MIN_R = 0.1
MAX_R = 0.9
R_INTERVAL = 480

PREVIEW = 5

class Board(object):
  def __init__(self, seed):
    self.seed = seed
    self.random = random.Random()
    self.random.seed(seed)

    self.score = 0
    self.state = 'playing'

    self.bitmap = [[0 for j in range(COLS)] for i in range(ROWS)]
    self.block = self.get_block()
    self.held_block = self.get_block()
    self.preview = [self.get_block() for i in range(PREVIEW)]

  def __repr__(self):
    return self.__str__()

  def __str__(self):
    bitmap = [['X' if elt else '.' for elt in row] for row in self.bitmap]
    if self.block:
      for point in self.block['offsets']:
        i = point['i'] + self.block['center']['i']
        j = point['j'] + self.block['center']['j']
        bitmap[i][j] = 'O'
    board_str = '\n'.join(''.join(row) for row in bitmap)
    state = '%s%s' % (self.state[0].upper(), self.state[1:])
    board_str += '\n%s! Score = %s' % (state, self.score)
    return board_str

  def to_dict(self):
    return {
      'state': self.state,
      'bitmap': self.bitmap,
      'block': self.block,
      'preview': self.preview,
      'score': self.score,
    }

  def check(self, block):
    for point in block['offsets']:
      i = point['i'] + block['center']['i']
      j = point['j'] + block['center']['j']
      if i < 0 or i >= ROWS or j < 0 or j >= COLS or self.bitmap[i][j]:
        return False
    return True

  def get_block(self):
    level = len(types) - 1

    # Calculate the ratio r between the probability of different levels.
    p = self.random.random()
    x = 2.0*(self.score - R_INTERVAL)/R_INTERVAL
    r = (MAX_R - MIN_R)*(x/math.sqrt(x*x + 1) + 1)/2 + MIN_R

    # Run through the levels and compare p to a sigmoid for each level.
    for i in range(1, len(types)):
      x = 2.0 * (self.score - i*LEVEL_INTERVAL)/LEVEL_INTERVAL
      if p > (r**i)*(x/math.sqrt(x*x + 1) + 1)/2:
        level = i - 1
        break
    
    # Return a block of the appropriate difficuly level.
    type = int(self.random.random()*types[level])
    return deepcopy(blocks[type])

  def send_commands(self, commands):
    if self.state != 'playing':
      return

    commands_dict = {
      'rotate': Board.rotate,
      'left': Board.left,
      'right': Board.right,
      'up': Board.up,
      'down': Board.down,
    }
    commands.append('drop')
    for command in commands:
      if command in commands_dict:
        block = commands_dict[command](self.block)
        if self.check(block):
          self.block = block
      elif command == 'hold':
        continue
        #block = deepcopy(self.held_block)
        #block['center']['i'] = self.block['center']['i']
        #block['center']['j'] = self.block['center']['j']
        #if self.check(block):
        #  self.held_block = self.block
        #  self.block = block
      elif command == 'drop':
        self.place()
        break

  def place(self):
    rows_free = self.rows_free(self.block)
    for point in self.block['offsets']:
      i = point['i'] + self.block['center']['i'] + rows_free
      j = point['j'] + self.block['center']['j']
      self.bitmap[i][j] = self.block['type'] + 1
    self.remove_rows()

    self.block = self.preview.pop(0)
    self.preview.append(self.get_block())
    if self.rows_free(self.block) < 0:
      self.state = 'failed'

  def rows_free(self, block):
    temp_block = deepcopy(block)
    while self.check(temp_block):
      temp_block['center']['i'] += 1
    return temp_block['center']['i'] - block['center']['i'] - 1

  def remove_rows(self):
    self.bitmap = [row for row in self.bitmap if not all(row)]
    num_rows_cleared = ROWS - len(self.bitmap)
    self.score += 2**num_rows_cleared - 1
    self.bitmap = [[0 for j in range(COLS)] for i in range(num_rows_cleared)] + self.bitmap

  @staticmethod
  def rotate(block):
    result = deepcopy(block)
    result['offsets'] = [{
      'i': offset['j'],
      'j': -offset['i'],
    } for offset in result['offsets']]
    return result

  @staticmethod
  def translate(block, i=0, j=0):
    result = deepcopy(block)
    result['center'] = {
      'i': result['center']['i'] + i,
      'j': result['center']['j'] + j,
    }
    return result

  @staticmethod
  def left(block):
    return Board.translate(block, j=-1)

  @staticmethod
  def right(block):
    return Board.translate(block, j=1)

  @staticmethod
  def up(block):
    return Board.translate(block, i=-1)

  @staticmethod
  def down(block):
    return Board.translate(block, i=1)
