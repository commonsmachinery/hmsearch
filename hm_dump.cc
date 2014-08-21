/* HmSearch hash library - database debugging tool
 *
 * Copyright 2014 Commons Machinery http://commonsmachinery.se/
 * Distributed under an MIT license, please see LICENSE in the top dir.
 */

#include <stdlib.h>
#include <stdio.h>

#include <memory>

#include "hmsearch.h"

int main(int argc, char **argv)
{
    const char *path;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s path\n", argv[0]);
        return 1;
    }

    path = argv[1];

    std::auto_ptr<HmSearch> db(HmSearch::open(path, HmSearch::READ));
    if (!db.get()) {
        return 1;
    }

    db->dump();
    return 0;
}

/*
  Local Variables:
  c-file-style: "stroustrup"
  indent-tabs-mode:nil
  End:
*/
