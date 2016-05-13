#!/usr/bin/python

import sys
import os
import base64

hash_size = int(int(sys.argv[1]) / 8)
count = int(sys.argv[2])
for i in range(count):
    print(base64.b16encode(os.urandom(hash_size)))


