#include <sstream>
#include <sys/ioctl.h>

#include "grass/filesystem.h"
#include "grass/grass.h"

namespace grass {

Directory::Directory(const std::string &root) : _root(root) {}

void Directory::Update(std::vector<std::string> new_path) {
    _path.swap(new_path);
}

std::string Directory::PathFromRoot(const std::string &relative_path) const {
    // Absolute paths are not allowed
    if (relative_path.length() >= 1 && relative_path[0] == '/') {
        throw GrassException("Access denied");
    }

    return PathFromVector(TokenizePath(relative_path));
}

std::vector<std::string> Directory::TokenizePath(
    const std::string &relative_path) const {

    // Absolute paths are not allowed
    if (relative_path.length() >= 1 && relative_path[0] == '/') {
        throw GrassException("Access denied");
    }

    std::vector<std::string> tokenized_path(_path);
    std::stringstream stream(relative_path);
    std::string token;

    while (std::getline(stream, token, '/')) {
        if (token == "" || token == ".") {
            // Nothing to do
            continue;
        }

        if (token == "..") {
            // Remove one directory from path
            if (tokenized_path.size() > 0) {
                tokenized_path.pop_back();
            } else {
                throw GrassException("Access denied");
            }
        } else {
            // Add directory to path
            tokenized_path.push_back(token);
        }
    }

    return tokenized_path;
}

std::string Directory::PathFromVector(
    const std::vector<std::string> &path_vector) const {

    // Build path
    std::string path = _root;
    for (auto const &dir : path_vector) {
        path += "/" + dir;
    }

    if (path.length() > 128) {
        throw GrassException("The path is too long");
    }

    return path;
}

DirectoryTraverser::DirectoryTraverser(const std::string &base_dir) {
    const char *paths[] = {base_dir.c_str(), nullptr};

    _fts = fts_open(const_cast<char *const *>(paths),
        FTS_XDEV | FTS_NOCHDIR | FTS_PHYSICAL,
        nullptr);
    if (_fts == nullptr) {
        throw GrassException("fts_open() failed");
    }
}

DirectoryTraverser::~DirectoryTraverser() { fts_close(_fts); }

void DirectoryTraverser::traverse(std::function<void(FTSENT *)> callback) {
    FTSENT *ent;
    while ((ent = fts_read(_fts)) != nullptr) {
        callback(ent);
    }
}

std::string ExtractFilename(const std::string &path) {
    size_t index = path.find_last_of('/');
    return path.substr(index + 1);
}

void RemoveDirectoryRecursively(const std::string &dir) {
    DirectoryTraverser traverser(dir);

    traverser.traverse([](FTSENT *ent) {
        switch (ent->fts_info) {
        case FTS_NS:
        case FTS_DNR:
        case FTS_ERR:
            // Error, ignore it
            break;

        case FTS_DP:
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            remove(ent->fts_accpath);
            break;

        default:
            break;
        }
    });
}

bool IsClosed(const int fd) {
    int n = 0;
    ioctl(fd, FIONREAD, &n);
    return n == 0;
}

struct stat Stat(const std::string &path) {
    struct stat sb;
    int stat_ret = stat(path.c_str(), &sb);
    if (stat_ret < 0) {
        switch (errno) {
        case ENOENT:
            throw GrassException("No such file or directory");
        default:
            throw GrassException("stat() failed");
        }
    }

    return sb;
}

} // namespace grass
