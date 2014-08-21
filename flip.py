#!/usr/bin/python3

import sys
import base64
import random

bits = int(sys.argv[1])

for line in sys.stdin:
    hash = bytearray(base64.b16decode(line.strip()))
    for bit in range(bits):
        hash[random.randrange(len(hash))] ^= 1 << random.randint(0, 7)

    sys.stdout.buffer.write(base64.b16encode(hash) + b'\n')

