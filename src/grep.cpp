#include <fstream>
#include <regex.h>

#include "grass/filesystem.h"
#include "grass/grass.h"
#include "grass/grep.h"

namespace grass {

// Simple RAII wrapper around POSIX regular expressions
class GrassRegex {
public:
    GrassRegex(const std::string &pattern) {
        if (regcomp(&_regex, pattern.c_str(), REG_EXTENDED | REG_NOSUB) != 0) {
            throw GrassException("Failed to create regex");
        }
    }

    ~GrassRegex() { regfree(&_regex); }

    bool testMatch(const std::string &input) const {
        return regexec(&_regex, input.c_str(), 0, nullptr, 0) == 0;
    }

private:
    regex_t _regex;
};

/*
 * Checks if a file matches a given pattern. The file is searched for matches
 * one line at a time, as specified by POSIX
 */
static bool SearchFile(const GrassRegex &regex, const std::string &filename) {
    std::ifstream infile(filename);
    std::string line;

    while (std::getline(infile, line)) {
        if (regex.testMatch(line)) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> SearchDirectory(const std::string &dir,
    const std::string &pattern) {
    GrassRegex regex(pattern);
    std::vector<std::string> results;
    DirectoryTraverser traverser(dir);

    traverser.traverse([&](FTSENT *ent) {
        switch (ent->fts_info) {
        /*
             * Ignore errors (due for example to inaccessible files)
             * and directories
             */
        case FTS_DNR:
        case FTS_ERR:
        case FTS_D:
        case FTS_DP:
        case FTS_DC:
            break;

        default: {
            /*
                 * The recursive traversal has found a file, check its contents
                 * for a match
                 */
            std::string file_path(ent->fts_path);
            if (SearchFile(regex, file_path)) {
                /*
                     * fts_path contains the path of the current file relative
                     * to the root of the traversal (including the root itself).
                     * However we don't want the root of the traversal to appear
                     * in the results, so we have to remove it.
                     */
                results.push_back(file_path.substr(dir.length() + 1));
            }
            break;
        }
        }
    });

    return results;
}

} // namespace grass
