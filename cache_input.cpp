#include "cache.h"

using namespace std;

static const vector<string> cache_names = { "ARC", "2Q", "LFU", "LIRS" };
    
pair<int, vector<CacheType>> ReadCacheLevelsInfo(const char *filename)
{
    int levels_num {};
    ifstream infile(filename);

    if (!infile.is_open()) {
        throw runtime_error("Couldn't open the file with cache levels");
    }
    if (!(infile >> levels_num) || levels_num < 0) {
        throw std::runtime_error("Incorrect data format in the file with cache levels");
    }

    vector<CacheType> active_caches;
    active_caches.reserve(levels_num); 

    // Read levels_num caches strings 
    string cache_name;
    for (int i = 0; i < levels_num; ++i) 
    {
        if (!(infile >> cache_name)) {
            throw runtime_error(format("A file with cache levels info contains less then {} caches names", levels_num));
        }

        // Looking for a string in the vector of caches names
        auto it = std::find(cache_names.begin(), cache_names.end(), cache_name);
        
        if (it != cache_names.end()) {
            // find the index of the match
            auto index = std::distance(cache_names.begin(), it);
            // set the index to the CacheType type and save
            active_caches.push_back(static_cast<CacheType>(index));
        } else {
            throw runtime_error("Unknown cache type was found in the file");
        }
    }
    return {levels_num, active_caches};
}