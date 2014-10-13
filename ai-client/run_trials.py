import subprocess
import random
import re
import tempfile
import os
import threading
import sys

def run_trial(seed=random.randint(0, 1024)):
    print 'running trial with seed ' + str(seed)

    process = subprocess.Popen(['./unmodified_client.py', 'local', str(seed)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()

    print stdout

    m = re.search('RESULTS: (\d+)', stdout)

    try:
        print 'trial with seed ' + seed + ' finished with result ' + m.group(1)
        return int(m.group(1))
    except:
        return 0

if __name__ == '__main__':
    score = 0

    while score < 400:
        score = run_trial(random.randint(0, 1024))
