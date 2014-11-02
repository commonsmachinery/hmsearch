hmsearch
========

C++ implementation of hamming distance algorithm HmSearch using LevelDB.

The algorithm is described in the paper "HmSearch: An Efficient
Hamming Distance Query Processing Algorithm" by Xiaoyang Zhang,
Jianbin Qin, Wei Wang, Yifang Sun and Jiaheng Lu.


Installation
------------

Ensure that LevelDB is installed. On Ubuntu:

    apt-get install libleveldb-dev

Then there should be a configure script, but for now just hit `make`.


Library usage
-------------

See the documentation in `hmsearch.h`


Tool usage
----------

Create a new database, providing hash size in bits, the max error
(hamming distance), and the expected number of hashes in the
database.  E.g.:

    ./hm_initdb hashes.ldb 256 10 100000000

Please note that the path indicated is a directory, under which the
database files will be housed. The above would create the directory
hashes.ldb/ with files hashes.ldb/LOG, hashes.ldb/CURRENT and so on.

Add hashes with `hm_insert`, either providing them on the command line
or on stdin:

    ./hm_insert hashes.ldb 6E6FB315FA8C43FE9C2687D5BE14575ABB7252104236747D571B97E003563DF0
    ./hm_insert hashes.ldb < list-of-hashes


Lookup hashes with `hm_insert`, again providing a list of hashes on
the command line or on stdin:
    
    ./hm_lookup hashes.ldb 6F6FB315FA8C43FE9C2687D5BE14575ABB7252104236747D571B97E003563DF0
    ./hm_lookup hashes.ldb < list-of-query-hashes

It will output all found hashes together with the hamming distance.

`hm_dump` outputs the internal structure of the database, and is only
useful for debugging.

To help testing and tuning, there are a few Python tools:

    ./gen_hashes.py HASH_SIZE NUM_HASHES | ./hm_insert hashes.ldb
    ./select.py NUM_LINES < list-of-hashes | ./hm_lookup hashes.ldb
    ./select.py NUM_LINES < list-of-hashes | ./flip.py BITS_TO_FLIP | ./hm_lookup hashes.ldb


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

