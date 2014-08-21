/* HmSearch hash library - database init tool
 *
 * Copyright 2014 Commons Machinery http://commonsmachinery.se/
 * Distributed under an MIT license, please see LICENSE in the top dir.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hmsearch.h"

int main(int argc, char **argv)
{
    const char *path;
    unsigned hash_bits;
    unsigned max_error;
    unsigned long num_hashes;
    
    if (argc != 5) {
        fprintf(stderr, "Usage: %s path hash_bits max_error num_hashes\n", argv[0]);
        return 1;
    }

    path = argv[1];
    hash_bits = strtoul(argv[2], NULL, 10);
    max_error = strtoul(argv[3], NULL, 10);
    num_hashes = strtoul(argv[4], NULL, 10);

    if (!HmSearch::init(path, hash_bits, max_error, num_hashes)) {
        return 1;
    }

    return 0;
}

/*
  Local Variables:
  c-file-style: "stroustrup"
  indent-tabs-mode:nil
  End:
*/
