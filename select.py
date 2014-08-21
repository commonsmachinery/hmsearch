#!/usr/bin/python3

import sys
import random

count = int(sys.argv[1])

for line in random.sample(sys.stdin.readlines(), count):
    sys.stdout.write(line)


