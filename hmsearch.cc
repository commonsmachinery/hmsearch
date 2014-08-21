/* HmSearch hash lookup library
 *
 * Copyright 2014 Commons Machinery http://commonsmachinery.se/
 * Distributed under an MIT license, please see LICENSE in the top dir.
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <memory>
#include <algorithm>

#include <kcdbext.h>

#include "hmsearch.h"

class HmSearchImpl : public HmSearch
{
public:
    HmSearchImpl(kyotocabinet::PolyDB* db, int hash_bits, int max_error)
        : _db(db)
        , _hash_bits(hash_bits)
        , _max_error(max_error)
        , _hash_bytes((hash_bits + 7) / 8)
        , _partitions((max_error + 3) / 2)
        , _partition_bits(ceil((double)hash_bits / _partitions))
        , _partition_bytes((_partition_bits + 7) / 8 + 1)
        { }

    ~HmSearchImpl() {
        _db->close();
        delete _db;
    }
    
    bool insert(const hash_string& hash);
    bool lookup(const hash_string& query,
                std::list<Result>& result,
                int max_error = -1);

    void dump();

private:
    struct Candidate {
        Candidate() : matches(0), first_match(0), second_match(0) {}
        int matches;
        int first_match;
        int second_match;
    };

    typedef std::map<hash_string, Candidate> CandidateMap;
    
    void get_candidates(const hash_string& query, CandidateMap& candidates);
    void add_hash_candidates(CandidateMap& candidates, int match,
                             const hash_char_t* hashes, size_t length);
    bool valid_candidate(const Candidate& candidate);
    int hamming_distance(const hash_string& query, const hash_string& hash);
    
    int get_partition_key(const hash_string& hash, int partition, hash_char_t *key);

    kyotocabinet::PolyDB* _db;
    int _hash_bits;
    int _max_error;
    int _hash_bytes;
    int _partitions;
    int _partition_bits;
    int _partition_bytes;

    static int one_bits[256];
};


bool HmSearch::init(const char *path,
                    unsigned hash_bits, unsigned max_error,
                    double num_hashes)
{
    if (hash_bits == 0 || (hash_bits & 7)) {
        fprintf(stderr, "invalid hash_bits: %u\n", hash_bits);
        return false;
    }

    if (max_error == 0 || max_error >= hash_bits) {
        fprintf(stderr, "invalid max_error: %u\n", max_error);
        return false;
    }

    std::auto_ptr<kyotocabinet::HashDB> db(new kyotocabinet::HashDB);
    if (!db.get()) {
        return false;
    }

    int partitions = (max_error + 3) / 2;
    int partition_bits = ceil((double)hash_bits / partitions);

    double hashes_per_partition = std::max(1.0, num_hashes / (int64_t(1) << partition_bits));

    // TODO: handle >1 hashes_per_partition by setting suitable
    // alignment to allow efficient append and perhaps even having a
    // metablock structure where the partition record just contains
    // references to blocks that actually holds the hashes.
    
    double keys = (num_hashes / hashes_per_partition) * partitions;

    // Limit to 0.5 GB index size, might make this configurable too
    int bucket_size = 6;
    int64_t buckets = std::max(int64_t(keys) / bucket_size, int64_t(512) << 20 / bucket_size);  

    fprintf(stderr, "tuning treedb %s to have %lu buckets\n", path, (unsigned long) buckets);
    db->tune_buckets(buckets);
    db->tune_options(kyotocabinet::HashDB::TLINEAR);
    
    if (!db->open(path, kyotocabinet::BasicDB::OWRITER | kyotocabinet::BasicDB::OCREATE)) {
        fprintf(stderr, "%s: cannot create or open database\n", path);
        return false;
    }

    if (db->count() > 0) {
        fprintf(stderr, "%s: cannot initialise non-empty database\n", path);
        return false;
    }
    
    char buf[20];
    snprintf(buf, sizeof(buf), "%u", hash_bits);
    if (!db->set("_hb", buf)) {
        fprintf(stderr, "%s: error storing hash bits\n", path);
        return false;
    }

    snprintf(buf, sizeof(buf), "%u", max_error);
    if (!db->set("_me", buf)) {
        fprintf(stderr, "%s: error storing max error\n", path);
        return false;
    }

    db->close();
    return true;
}


HmSearch* HmSearch::open(const char *path, Mode mode)
{
    // TODO: check that path ends in kct so we open a tree database.

    std::auto_ptr<kyotocabinet::PolyDB> db(new kyotocabinet::PolyDB);
    if (!db.get()) {
        return NULL;
    }

    // Increase mmap from 64M to 512M
    //db->tune_map(int64_t(512) << 20);

    if (!db->open(path, (mode == READ ?
                         kyotocabinet::BasicDB::OREADER :
                         kyotocabinet::BasicDB::OWRITER))) {
        fprintf(stderr, "%s: cannot open database\n", path);
        return NULL;
    }
    
    std::string v;
    unsigned long hash_bits, max_error;
    if (!db->get("_hb", &v) || !(hash_bits = strtoul(v.c_str(), NULL, 10))) {
        fprintf(stderr, "%s: cannot get hash bits\n", path);
        return NULL;
    }
        
    if (!db->get("_me", &v) || !(max_error = strtoul(v.c_str(), NULL, 10))) {
        fprintf(stderr, "%s: cannot get max error\n", path);
        return NULL;
    }

    HmSearch* hm = new HmSearchImpl(db.get(), hash_bits, max_error);
    if (!hm) {
        return NULL;
    }

    db.release();
    return hm;
}


HmSearch::hash_string HmSearch::parse_hexhash(const std::string& hexhash)
{
    int len = hexhash.length() / 2;
    hash_char_t hash[len];
    
    for (int i = 0; i < len; i++) {
        char buf[3];
        char* err;
        
        buf[0] = hexhash[i * 2];
        buf[1] = hexhash[i * 2 + 1];
        buf[2] = 0;

        hash[i] = strtoul(buf, &err, 16);
        
        if (*err != '\0') {
            return hash_string();
        }
    }

    return hash_string(hash, len);
}

std::string HmSearch::format_hexhash(const HmSearch::hash_string& hash)
{
    char hex[hash.length() * 2 + 1];

    for (size_t i = 0; i < hash.length(); i++) {
        sprintf(hex + 2 * i, "%02x", hash[i]);
    }

    return hex;
}




bool HmSearchImpl::insert(const hash_string& hash)
{
    hash_char_t key[_partition_bytes + 2];
    
    if (hash.length() != (size_t) _hash_bytes) {
        return false;
    }

    for (int i = 0; i < _partitions; i++) {
        get_partition_key(hash, i, key);

        if (!_db->append((const char*) key, _partition_bytes + 2,
                         (const char*) hash.data(), hash.length())) {
            return false;
        }
    }

    return true;
}


bool HmSearchImpl::lookup(const hash_string& query, std::list<Result>& result, int reduced_error)
{
    CandidateMap candidates;

    if (query.length() != (size_t) _hash_bytes) {
        return false;
    }

    get_candidates(query, candidates);

    for (CandidateMap::const_iterator i = candidates.begin(); i != candidates.end(); ++i) {
        if (valid_candidate(i->second)) {
            int distance = hamming_distance(query, i->first);

            if (distance <= _max_error
                && (reduced_error < 0 || distance <= reduced_error)) {
                result.push_back(Result(i->first, distance));
            }
        }
    }

    return true;
}


void HmSearchImpl::dump()
{
    kyotocabinet::BasicDB::Cursor *c = _db->cursor();

    std::string key_str, value_str;

    c->jump();
    while (c->get(&key_str, &value_str, true)) {
        hash_char_t* key = (hash_char_t*) key_str.data();
        hash_char_t* value = (hash_char_t*) value_str.data();
        
        if (key[0] == 'P') {
            std::cout << "Partition "
                      << int(key[1])
                      << format_hexhash(hash_string(key + 2, key_str.length() - 2))
                      << std::endl;

            for (long len = value_str.length(); len >= _hash_bytes;
                 len -= _hash_bytes, value += _hash_bytes) {
                std::cout << "    "
                          << format_hexhash(hash_string(value, _hash_bytes))
                          << std::endl;
            }
            std::cout << std::endl;
        }
    }

    delete c;
}


void HmSearchImpl::get_candidates(
    const HmSearchImpl::hash_string& query,
    HmSearchImpl::CandidateMap& candidates)
{
    hash_char_t key[_partition_bytes + 2];
    
    for (int i = 0; i < _partitions; i++) {
        std::string hashes;
        
        int bits = get_partition_key(query, i, key);

        // Get exact matches
        if (_db->get(std::string((const char*) key, _partition_bytes + 2), &hashes)) {
            add_hash_candidates(candidates, 0, (const hash_char_t*)hashes.data(), hashes.length());
        }

        // Get 1-variant matches

        int pbyte = (i * _partition_bits) / 8;
        for (int pbit = i * _partition_bits; bits > 0; pbit++, bits--) {
            hash_char_t flip = 1 << (7 - (pbit % 8));

            key[pbit / 8 - pbyte + 2] ^= flip;
            
            if (_db->get(std::string((const char*) key, _partition_bytes + 2), &hashes)) {
                add_hash_candidates(candidates, 1, (const hash_char_t*)hashes.data(), hashes.length());
            }
            
            key[pbit / 8 - pbyte + 2] ^= flip;
        }
    }
}


void HmSearchImpl::add_hash_candidates(
    HmSearchImpl::CandidateMap& candidates, int match,
    const HmSearch::hash_char_t* hashes, size_t length)
{
    for (size_t n = 0; n < length; n += _hash_bytes) {
        hash_string hash = hash_string(hashes + n, _hash_bytes);
        Candidate& cand = candidates[hash];

        ++cand.matches;
        if (cand.matches == 1) {
            cand.first_match = match;
        }
        else if (cand.matches == 2) {
            cand.second_match = match;
        }
    }
}


bool HmSearchImpl::valid_candidate(
    const HmSearchImpl::Candidate& candidate)
{
    if (_max_error & 1) {
        // Odd k
        if (candidate.matches < 3) {
            if (candidate.matches == 1 || (candidate.first_match && candidate.second_match)) {
                return false;
            }
        }
    }
    else {
        // Even k
        if (candidate.matches < 2) {
            if (candidate.first_match)
                return false;
        }
    }

    return true;
}


int HmSearchImpl::hamming_distance(
    const HmSearchImpl::hash_string& query,
    const HmSearchImpl::hash_string& hash)
{
    int distance = 0;

    for (size_t i = 0; i < query.length(); i++) {
        distance += one_bits[query[i] ^ hash[i]];
    }

    return distance;
}


int HmSearchImpl::get_partition_key(const hash_string& hash, int partition, hash_char_t *key)
{
    int psize, hash_bit, bits_left;

    psize = _hash_bits - partition * _partition_bits;
    if (psize > _partition_bits) {
        psize = _partition_bits;
    }

    // Store key identifier and partition number first
    key[0] = 'P';
    key[1] = partition;

    // Copy bytes, masking out some bits at the start and end
    bits_left = psize;
    hash_bit = partition * _partition_bits;

    for (int i = 0; i < _partition_bytes; i++) {
        int byte = hash_bit / 8;
        int bit = hash_bit % 8;
        int bits = 8 - bit;

        if (bits > bits_left) {
            bits = bits_left;
        }

        bits_left -= bits;
        hash_bit += bits;

        key[i + 2] = hash[byte] & (((1 << bits) - 1) << (8 - bit - bits));
    }

    return psize;
}


int HmSearchImpl::one_bits[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

/*
  Local Variables:
  c-file-style: "stroustrup"
  indent-tabs-mode:nil
  End:
*/
