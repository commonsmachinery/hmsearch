/* HmSearch hash lookup library
 *
 * Copyright 2014 Commons Machinery http://commonsmachinery.se/
 * Distributed under an MIT license, please see LICENSE in the top dir.
 */

#ifndef __HMSEARCH_H_INCLUDED__
#define __HMSEARCH_H_INCLUDED__

#include <string>
#include <list>

class HmSearch
{
public:
    typedef unsigned char hash_char_t;
    typedef std::basic_string<hash_char_t> hash_string;

    struct Result {
        Result(const hash_string& h, int d) : hash(h), distance(d) {}
        hash_string hash;
        int distance;
    };

    typedef std::list<Result> ResultList;

    enum Mode {
        READ,
        WRITE
    };

    static bool init(const char *path, 
                     unsigned hash_bits, unsigned max_error,
                     double num_hashes);

    static HmSearch* open(const char *path, Mode mode);

    static hash_string parse_hexhash(const std::string& hexhash);
    static std::string format_hexhash(const hash_string& hash);

    virtual bool insert(const hash_string& hash) = 0;
    virtual bool lookup(const hash_string& query,
                        ResultList& result,
                        int max_error = -1) = 0;
    virtual void dump() = 0;

    virtual ~HmSearch() {}

protected:
    HmSearch() {}
};


/*
  Local Variables:
  c-file-style: "stroustrup"
  indent-tabs-mode:nil
  End:
*/

#endif // __HMSEARCH_H_INCLUDED__

