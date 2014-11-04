/* HmSearch hash library - insert tool
 *
 * Copyright 2014 Commons Machinery http://commonsmachinery.se/
 * Distributed under an MIT license, please see LICENSE in the top dir.
 */


#include <stdio.h>

#include <iostream>
#include <memory>

#include "hmsearch.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s path [hexhash...]\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    std::string error_msg;
    
    std::auto_ptr<HmSearch> db(HmSearch::open(path, &error_msg));
    if (!db.get()) {
        fprintf(stderr, "%s: error opening %s: %s\n", argv[0], path, error_msg.c_str());
        return 1;
    }

    if (argc > 2) {
        // Insert hashes from command line
        for (int i = 2; i < argc; i++) {
            const char *hexhash = argv[i];
            if (!db->print_copystring(HmSearch::parse_hexhash(hexhash), &error_msg)) {
                fprintf(stderr, "%s: cannot insert hash: %s (%s)\n",
                        argv[0], error_msg.c_str(), hexhash);
            }
        }
    }
    else {
        // Read hashes from stdin
        std::string hexhash;
        while (std::cin >> hexhash) {
            if (!db->print_copystring(HmSearch::parse_hexhash(hexhash), &error_msg)) {
                fprintf(stderr, "%s: cannot insert hash: %s (%s)\n",
                        argv[0], error_msg.c_str(), hexhash.c_str());
            }
        }
    }

    if (!db->close(&error_msg)) {
        fprintf(stderr, "%s: error closing database: %s\n",
                argv[0], error_msg.c_str());
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
