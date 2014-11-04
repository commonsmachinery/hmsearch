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
#include <map>
#include <iostream>

#include <pqxx/pqxx>

#include "hmsearch.h"

/** The actual implementation of the HmSearch database.
 *
 * A difference between this implementation and the HmSearch algorithm
 * in the paper is that only exact-matches are stored in the database,
 * not the 1-matches.  The 1-var partitions are instead generated
 * during lookup.  This drastically reduces database size, which
 * speeds up insertion and probably lookups too.
 *
 * The database contains some setting records controlling the
 * operation:
 *
 * _hb: hash bits
 * _me: max errors
 * _se: global sequence when adding new keys
 *
 * These can't be changed once the database has been initialised.
 *
 * Each partition is stored as a key on the following format:
 *  Byte 0: 'P'
 *  Byte 1: Partition number (thus limiting to max error 518)
 *  Bytes 2-N: Partition bits.
 *  Bytes N-M: Sequence number (globally incremented with each addition)
 */
class HmSearchImpl : public HmSearch
{
public:
    HmSearchImpl(std::string connstr, int hash_bits, int max_error)
        : _hash_bits(hash_bits)
        , _max_error(max_error)
        , _hash_bytes((hash_bits + 7) / 8)
        , _partitions((max_error + 3) / 2)
        , _partition_bits(ceil((double)hash_bits / _partitions))
        , _partition_bytes((_partition_bits + 7) / 8 + 1)
    {
      _db = new pqxx::connection(connstr);
    }

    ~HmSearchImpl() {
        close();
    }
    
    bool insert(const hash_string& hash,
                std::string* error_msg = NULL);

    bool print_copystring(const hash_string& hash,
                std::string* error_msg = NULL);

    bool lookup(const hash_string& query,
                LookupResultList& result,
                int max_error = -1,
                std::string* error_msg = NULL);

    bool close(std::string* error_msg = NULL);

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
                             const uint8_t* hashes, size_t length);
    hash_string get_multiple_keys(uint8_t *key);
    bool valid_candidate(const Candidate& candidate);
    int hamming_distance(const hash_string& query, const hash_string& hash);
    
    int get_partition_key(const hash_string& hash, int partition, uint8_t *key);

    pqxx::connection *_db;
    int _hash_bits;
    int _max_error;
    int _hash_bytes;
    int _partitions;
    int _partition_bits;
    int _partition_bytes;

    static int one_bits[256];
};


bool HmSearch::init(const std::string& path,
                    unsigned hash_bits, unsigned max_error,
                    uint64_t num_hashes,
                    std::string* error_msg)
{
    std::string dummy;
    if (!error_msg) {
        error_msg = &dummy;
    }
    *error_msg = "";

    if (hash_bits == 0 || (hash_bits & 7)) {
        *error_msg = "invalid hash_bits value";
        return false;
    }

    if (max_error == 0 || max_error >= hash_bits || max_error > 518) {
        *error_msg = "invalid max_error value";
        return false;
    }

    pqxx::connection db(path);
    if (!db.is_open()) {
       *error_msg = "Can't open database";
       return false;
    }

    std::string sql;
    sql = "INSERT INTO config VALUES ($1, $2)";
    db.prepare("hash_max", sql);

    pqxx::work W(db);
    sql = "CREATE TABLE config ("\
          "       hash_bits int,"\
          "       max_error int)";
    W.exec(sql);

    W.prepared("hash_max")(hash_bits)(max_error).exec();

    sql = "CREATE TABLE hashes ("\
          "       hash bytea,"\
          "       partition int,"\
          "       key bytea)";

    W.exec(sql);

    sql = "CREATE INDEX ix_key ON hashes(key)";

    W.exec(sql);

    W.commit();

    db.disconnect();

    return true;
}


HmSearch* HmSearch::open(const std::string& path,
                         std::string* error_msg)
{
    std::string dummy;
    if (!error_msg) {
        error_msg = &dummy;
    }
    *error_msg = "";

    pqxx::connection db(path);
    if (!db.is_open()) {
        *error_msg = "Can't open database";
        return NULL;
    }

    std::string sql;

    sql = "SELECT max_error, hash_bits FROM config";
    pqxx::nontransaction n(db);
    pqxx::result res(n.exec(sql));

    pqxx::result::const_iterator c = res.begin(); // We retrieve just one row

    unsigned long hash_bits, max_error;
    max_error = c[0].as<long>();
    hash_bits = c[1].as<long>();

    db.disconnect();

    HmSearch* hm = new HmSearchImpl(path, hash_bits, max_error);
    if (!hm) {
        *error_msg = "out of memory";
        return NULL;
    }

    return hm;
}


HmSearch::hash_string HmSearch::parse_hexhash(const std::string& hexhash)
{
    int len = hexhash.length() / 2;
    uint8_t hash[len];
    
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


bool HmSearchImpl::print_copystring(const hash_string& hash,
                          std::string* error_msg)
{
    std::string dummy;
    if (!error_msg) {
        error_msg = &dummy;
    }
    *error_msg = "";

    if (hash.length() != (size_t) _hash_bytes) {
        *error_msg = "incorrect hash length";
        return false;
    }

    for (int i = 0; i < _partitions; i++) {
        uint8_t key[_partition_bytes + 2];

        get_partition_key(hash, i, key);

        std::cout << "\\\\x" << format_hexhash(hash)
                  << " "
                  << int(i)
                  << " "
                  << "\\\\x" << format_hexhash(hash_string(key, _partition_bytes + 2))
                  << std::endl;

    }

    return true;
}

bool HmSearchImpl::insert(const hash_string& hash,
                          std::string* error_msg)
{
    std::string dummy;
    if (!error_msg) {
        error_msg = &dummy;
    }
    *error_msg = "";

    if (hash.length() != (size_t) _hash_bytes) {
        *error_msg = "incorrect hash length";
        return false;
    }

    if (!_db->is_open()) {
        *error_msg = "database is closed";
        return false;
    }

    _db->prepare("insert", "INSERT INTO hashes VALUES ($1, $2, $3)");
    pqxx::work W(*_db);
    for (int i = 0; i < _partitions; i++) {
        uint8_t key[_partition_bytes + 2];

        get_partition_key(hash, i, key);

        pqxx::binarystring key_blob(key, _partition_bytes + 2);
        pqxx::binarystring hash_blob(hash.data(), hash.length());

        W.prepared("insert")(hash_blob)(i)(key_blob).exec();
    }

    W.commit();

    return true;
}


bool HmSearchImpl::lookup(const hash_string& query,
                          LookupResultList& result,
                          int reduced_error,
                          std::string* error_msg)
{
    std::string dummy;
    if (!error_msg) {
        error_msg = &dummy;
    }
    *error_msg = "";

    if (query.length() != (size_t) _hash_bytes) {
        *error_msg = "incorrect hash length";
        return false;
    }

    if (!_db->is_open()) {
        *error_msg = "database is closed";
        return false;
    }

    CandidateMap candidates;
    get_candidates(query, candidates);

    for (CandidateMap::const_iterator i = candidates.begin(); i != candidates.end(); ++i) {
        if (valid_candidate(i->second)) {
            int distance = hamming_distance(query, i->first);

            if (distance <= _max_error
                && (reduced_error < 0 || distance <= reduced_error)) {
                result.push_back(LookupResult(i->first, distance));
            }
        }
    }

    return true;
}


bool HmSearchImpl::close(std::string* error_msg)
{
    std::string dummy;
    if (!error_msg) {
        error_msg = &dummy;
    }
    *error_msg = "";

    if (!_db->is_open()) {
        // Already closed
        return true;
    }

    _db->disconnect();

    return true;
}


HmSearchImpl::hash_string HmSearchImpl::get_multiple_keys(
    uint8_t *key)
{
    hash_string hashes;

    pqxx::nontransaction n(*_db);
    pqxx::binarystring key_blob(key, _partition_bytes + 2);
    pqxx::result res = n.prepared("select")(key_blob).exec();

    for (pqxx::result::const_iterator c = res.begin(); c != res.end(); ++c) {
       pqxx::binarystring hash_result(c[0]);
       hashes.append(hash_string(hash_result.data(), hash_result.size()));
    }

    return hashes;
}

void HmSearchImpl::get_candidates(
    const HmSearchImpl::hash_string& query,
    HmSearchImpl::CandidateMap& candidates)
{
    uint8_t key[_partition_bytes + 2];

    std::string sql;
    sql = "SELECT hash FROM hashes WHERE key=$1";
    _db->prepare("select", sql);
    
    for (int i = 0; i < _partitions; i++) {
        hash_string hashes;
        
        int bits = get_partition_key(query, i, key);

        // Get exact matches

        hashes = get_multiple_keys(key);

        if (hashes.length() > 0) {
            add_hash_candidates(candidates, 0, (const uint8_t*)hashes.data(), hashes.length());
        }

        // Get 1-variant matches

        int pbyte = (i * _partition_bits) / 8;
        for (int pbit = i * _partition_bits; bits > 0; pbit++, bits--) {
            uint8_t flip = 1 << (7 - (pbit % 8));

            key[pbit / 8 - pbyte + 2] ^= flip;

            hash_string hashes;
            hashes = get_multiple_keys(key);
            if (hashes.length() > 0) {
                add_hash_candidates(candidates, 1, (const uint8_t*)hashes.data(), hashes.length());
            }
            
            key[pbit / 8 - pbyte + 2] ^= flip;
        }
    }
}


void HmSearchImpl::add_hash_candidates(
    HmSearchImpl::CandidateMap& candidates, int match,
    const uint8_t* hashes, size_t length)
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


int HmSearchImpl::get_partition_key(const hash_string& hash, int partition, uint8_t *key)
{
    int psize, hash_bit, bits_left;

    psize = _hash_bits - partition * _partition_bits;
    if (psize > _partition_bits) {
        psize = _partition_bits;
    }

    // Store key partition number first
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
