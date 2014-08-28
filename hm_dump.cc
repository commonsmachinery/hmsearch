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
    if (argc != 2) {
        fprintf(stderr, "Usage: %s path\n", argv[0]);
        return 1;
    }

    const char* path = argv[1];
    std::string error_msg;

    std::auto_ptr<HmSearch> db(HmSearch::open(path, HmSearch::READONLY, &error_msg));
    if (!db.get()) {
        fprintf(stderr, "%s: error opening %s: %s\n", argv[0], path, error_msg.c_str());
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
