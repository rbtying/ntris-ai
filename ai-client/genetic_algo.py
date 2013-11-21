import subprocess
import random
import re
import tempfile
import os
import threading
import sys

chromosome_seed = {
        'BLOCK_EDGES': 0.7277,
        'WALL_EDGES': 1.42234,
        # 'EXTERNAL_EDGES': 0.4,
        'GAPS': 0.182809,
        'HOLES': -1.6656,
        'MAX_HEIGHT': -0.1,
        'BLOCK_HEIGHT': -0.7377,
        'POINTS_EARNED': 0.9297,
        'COVERS': 0.05707,
        'BUMPINESS': -0.4219
    }

def generate_random_chromosome(seed=chromosome_seed, sd=1):
    new_chromosome = dict()
    for key in seed.iterkeys():
        # random values clustered around seed
        new_chromosome[key] = random.gauss(seed[key], sd)

    return Chromosome(new_chromosome)

def run_trial(chromosome, seed=random.randint(0, 1024)):
    # print 'running trial with seed ' + str(seed)
    tf = tempfile.NamedTemporaryFile(delete=False)

    for key, val in chromosome.iteritems():
        tf.write(key + ' ' + str(val) + '\n')
    tf.close()

    process = subprocess.Popen(['./client.py', 'local', str(seed), tf.name],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()

    os.unlink(tf.name)

    m = re.search('RESULTS: (\d+)', stdout)

    try:
        # print 'trial finished with result ' + m.group(1)
        return int(m.group(1))
    except:
        return 0

def randompair(collection):
    random.shuffle(collection)
    return zip(collection[0::2], collection[1::2])

def crossover(ch1, ch2):
    child = dict()
    for key in ch1.iterkeys():
        randval = random.random()
        if randval <= 0.4:
            child[key] = ch1[key]
        elif randval <= 0.8:
            child[key] = ch2[key]
        else:
            # mutation
            child[key] = random.gauss(0, 1)
    return Chromosome(child)

class Chromosome(dict):
    def __hash__(self):
        return hash((frozenset(self), frozenset(self.itervalues())))

def run_generation(population):
    print 'running generation'

    # 4 trials
    seeds = [random.randint(0, 1024) for i in range(4)]

    class RunThread(threading.Thread):
        def __init__(self, chromosome, seed):
            threading.Thread.__init__(self)
            self.chromosome = chromosome;
            self.seed = seed

        def run(self):
            self.score = run_trial(self.chromosome, self.seed)

    threads = [RunThread(c, s) for c in population for s in seeds]
    for thread in threads:
        thread.start()

    scoremap = dict()
    for thread in threads:
        thread.join()
        if not thread.chromosome in scoremap:
            scoremap[thread.chromosome] = 0
        scoremap[thread.chromosome] += thread.score

    table = [(c, s) for c, s in scoremap.iteritems()]

    for row in sorted(table, key=lambda v: v[1]):
        print row

    sys.stdout.flush()

    survivors = list()
    pairs = randompair(table)
    for pair in pairs:
        # compare scores
        if pair[0][1] > pair[1][1]:
            survivors.append(pair[0][0])
        else:
            survivors.append(pair[1][0])

    # # only look at the best ones
    # for row in table[len(table)/2:]:
    #     survivors.append(row[0])

    parents = randompair(survivors)
    children = list()
    newpopl = list()
    for parentpair in parents:
        # newpopl.append(parentpair[0][0])
        children.append(crossover(parentpair[0], parentpair[1]))
        children.append(crossover(parentpair[0], parentpair[1]))
        newpopl.append(generate_random_chromosome(parentpair[0], 0.01))
        # newpopl.append(parentpair[1][0])
        newpopl.append(generate_random_chromosome(parentpair[1], 0.01))
    newpopl = newpopl + children

    for row in sorted(table, key=lambda v: v[1], reverse=True):
        return newpopl, row[1]

if __name__ == '__main__':
    population = [generate_random_chromosome() for i in range(16)]

    while True:
        population, topscore = run_generation(population)
        if topscore / 4 > 100:
            break
