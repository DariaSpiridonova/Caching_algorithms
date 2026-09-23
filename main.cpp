#include "cache.h"

using namespace std;

int main()
{
    try {
        MultiCache multi_level_cache {ReadCacheLevelsInfo(MUL_CACHE_INFO_FILENAME)};
    } 
    catch (const std::exception& e) {
        cerr << e.what() << std::endl;
        return EXIT_FAILURE; 
    }

    return 0;
}