hmsearch
========

C++ implementation of hamming distance algorithm HmSearch using Kyoto
Cabinet.

For a description of the algorithm, see the original paper in the
`doc` directory.


Installation
------------

Ensure that Kyoto Cabinet is installed. On Ubuntu:

    apt-get install libkyotocabinet-dev kyotocabinet-util

Then there should be a configure script, but for now just hit `make`.


Library usage
-------------

See the documentation in `hmsearch.h`


Tool usage
----------

Create a new database, providing hash size in bits, the max error
(hamming distance), and the expected number of hashes in the
database.  E.g.:

    ./hm_initdb hashes.kch 256 10 100000000


Add hashes with `hm_insert`, either providing them on the command line
or on stdin:

    ./hm_insert hashes.kch 6E6FB315FA8C43FE9C2687D5BE14575ABB7252104236747D571B97E003563DF0
    ./hm_insert hashes.kch < list-of-hashes


Lookup hashes with `hm_insert`, again providing a list of hashes on
the command line or on stdin:
    
    ./hm_lookup hashes.kch 6F6FB315FA8C43FE9C2687D5BE14575ABB7252104236747D571B97E003563DF0
    ./hm_lookup hashes.kch < list-of-query-hashes

It will output all found hashes together with the hamming distance.

`hm_dump` outputs the internal structure of the database, and is only
useful for debugging.  `kchashmgr inform -st` can be used to get
further information about the underlying database.

To help testing and tuning, there are a few Python tools:

    ./gen_hashes.py HASH_SIZE NUM_HASHES | ./hm_insert hashes.kch
    ./select.py NUM_LINES < list-of-hashes | ./hm_lookup hashes.kch
    ./select.py NUM_LINES < list-of-hashes | ./flip.py BITS_TO_FLIP | ./hm_lookup hashes.kch


Limitations
-----------

The code only supports a binary value space (i.e. 0 and 1), not the
larger spaces of full HmSearch.

Hashes must be an even number of bytes.

This code will degrade once the probability of several hashes sharing
the same partition value goes above perhaps 0.1.  To handle that case,
the HmSearch::init() need to be extended to tune the database to align
records so that each append of a hash doesn't always require moving
the whole record.

There's also other changes that can be done to optimise this, but the
code works pretty well at least for 25M 256-bit hashes on a regular
laptop with SSD.


License
-------

Copyright 2014 Commons Machinery http://commonsmachinery.se/

Distributed under an MIT license, please see LICENSE in the top dir.

Contact: dev@commonsmachinery.se


Credits
-------

HmSearch was created by Xiaoyang Zhang, Jianbin Qin, Wei Wang, Yifang
Sun and Jiaheng Lu.  For details on the algorithm and contact emails,
please see the copy of their paper in the `doc` directory.
