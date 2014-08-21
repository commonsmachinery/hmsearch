/* HmSearch hash library - lookup tool
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

    std::auto_ptr<HmSearch> db(HmSearch::open(path, HmSearch::READ));
    if (!db.get()) {
        return 1;
    }

    if (argc > 2) {
        // Lookup hashes from command line
        for (int i = 2; i < argc; i++) {
            const char *hexhash = argv[i];

            HmSearch::ResultList matches;
            if (!db->lookup(HmSearch::parse_hexhash(hexhash), matches)) {
                fprintf(stderr, "%s: bad hash: %s\n", argv[0], hexhash);
                return 1;
            }

            for (HmSearch::ResultList::const_iterator i = matches.begin();
                 i != matches.end();
                 ++i) {
                std::cout << HmSearch::format_hexhash(i->hash) << " " << i->distance << std::endl;
            }
        }
    }
    else {
        // Read hashes from stdin
        std::string hexhash;
        while (std::cin >> hexhash) {
            HmSearch::ResultList matches;
            if (!db->lookup(HmSearch::parse_hexhash(hexhash), matches)) {
                fprintf(stderr, "%s: bad hash: %s\n", argv[0], hexhash.c_str());
                return 1;
            }

            for (HmSearch::ResultList::const_iterator i = matches.begin();
                 i != matches.end();
                 ++i) {
                std::cout << HmSearch::format_hexhash(i->hash) << " " << i->distance << std::endl;
            }
        }
    }

    return 0;
}

/*
  Local Variables:
  c-file-style: "stroustrup"
  indent-tabs-mode:nil
  End:
*/
