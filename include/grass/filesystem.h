#pragma once

#include <functional>
#include <string>

#include <fts.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace grass {

// This class represents a directory on the file system
class Directory {
public:
    Directory(const std::string &root);

    // Update _path
    void Update(std::vector<std::string> new_path);

    // Compute the absolute path from _root according to a relative path and the current one
    std::string PathFromRoot(const std::string &relative_path = "") const;

    // Given a path, split it on '\' and return the corresponding vector
    std::vector<std::string> TokenizePath(
        const std::string &relative_path = "") const;

    // Given a vector of strings, join them with a '/' and return the resulting path
    std::string PathFromVector(
        const std::vector<std::string> &path_vector) const;

private:
    std::string _root;
    std::vector<std::string> _path;
};

/*
 * Simple RAII wrapper around fts objects, which are used to traverse a file
 * system hierarchy
 */
class DirectoryTraverser {
public:
    /*
	 * Constructs a traverser. base_dir will serve as the root directory of the
	 * traversal
	 */
    DirectoryTraverser(const std::string &base_dir);

    // Destructor which releases the FTS resources
    ~DirectoryTraverser();

    /*
	 * Start traversing the file system. Every time a new entity is processed
	 * the given callback will be invoked with a FTSENT structure as argument.
	 */
    void traverse(std::function<void(FTSENT *)> callback);

private:
    FTS *_fts;
};

// Maximum path length
static const size_t MAX_PATH_LENGTH = 128;

// Extract filename from a path
std::string ExtractFilename(const std::string &path);

// Remove a directory tree recursively
void RemoveDirectoryRecursively(const std::string &dir);

// Verify if the given file descriptor is closed
bool IsClosed(const int fd);

// Return `stat` structure of the given file/directory
struct stat Stat(const std::string &path);

} // namespace grass
