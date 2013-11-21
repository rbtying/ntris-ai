import sys

if __name__ == '__main__':
    fname = sys.argv[1]

    scoretuples = list()
    with open(fname) as f:
        for line in f:
            if not 'running generation' in line:
                scoretuple = eval(line)
                scoretuples.append(scoretuple)

    for tup in sorted(scoretuples, key=lambda v: v[1], reverse=True):
        chromosome, score = tup
        for key, val in chromosome.iteritems():
            print key + ' ' + str(val)
        print '---------'
        print 'AVG SCORE: ' + str(score / 4.0)
        break
