/*
 * dw_util.cpp - files, zlib and resources for the offline Demonware core.
 * The only file under offline/ besides dw_net.cpp that touches the operating system.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif
#include "dw.h"
#include "dw_state.h"

namespace dw {

std::string PathJoin(const std::string &a, const std::string &b)
{
    if (a.empty()) return b;
    char last = a[a.size() - 1];
    return (last == '/' || last == '\\') ? a + b : a + "/" + b;
}

bool ReadFile(const std::string &path, Bytes &out)
{
    out.clear();
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;
    char chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) out.append(chunk, n);
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

bool FileExists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFREG);
}

int64_t FileMtime(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 ? (int64_t)st.st_mtime : 0;
}

void MakeDirs(const std::string &path)
{
    for (size_t i = 1; i <= path.size(); i++) {
        if (i != path.size() && path[i] != '/' && path[i] != '\\') continue;
        std::string part = path.substr(0, i);
        if (part.size() == 2 && part[1] == ':') continue;
#ifdef _WIN32
        _mkdir(part.c_str());
#else
        mkdir(part.c_str(), 0755);
#endif
    }
}

bool RemoveFile(const std::string &path) { return remove(path.c_str()) == 0; }

/* Temp file in the same folder, then an atomic replace: a half-written profile is never visible. */
bool WriteFileAtomic(const std::string &path, const Bytes &data)
{
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) MakeDirs(path.substr(0, slash));
#ifdef _WIN32
    std::string tmp = path + Fmt(".tmp.%lu", (unsigned long)GetCurrentProcessId());
#else
    std::string tmp = path + Fmt(".tmp.%ld", (long)getpid());
#endif
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size();
    if (fflush(f)) ok = false;
    if (fclose(f)) ok = false;
#ifdef _WIN32
    if (ok) ok = MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (ok) ok = rename(tmp.c_str(), path.c_str()) == 0;
#endif
    if (!ok) remove(tmp.c_str());
    return ok;
}

std::vector<std::string> ListDir(const std::string &dir)
{
    std::vector<std::string> out;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(PathJoin(dir, "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, "..")) out.push_back(fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir.c_str());
    if (!d) return out;
    while (struct dirent *e = readdir(d))
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) out.push_back(e->d_name);
    closedir(d);
#endif
    return out;
}

bool Inflate(const Bytes &in, Bytes &out)
{
    z_stream z;
    memset(&z, 0, sizeof(z));
    out.clear();
    if (inflateInit(&z) != Z_OK) return false;
    z.next_in = (Bytef *)in.data();
    z.avail_in = (uInt)in.size();
    unsigned char chunk[65536];
    int r;
    do {
        z.next_out = chunk;
        z.avail_out = sizeof(chunk);
        r = inflate(&z, Z_NO_FLUSH);
        if (r != Z_OK && r != Z_STREAM_END) { inflateEnd(&z); return false; }
        out.append((const char *)chunk, sizeof(chunk) - z.avail_out);
    } while (r != Z_STREAM_END && (z.avail_in || z.avail_out == 0));
    inflateEnd(&z);
    return r == Z_STREAM_END;
}

bool Deflate(const Bytes &in, Bytes &out, int level)
{
    uLongf cap = compressBound((uLong)in.size());
    out.assign(cap, '\0');
    if (compress2((Bytef *)&out[0], &cap, (const Bytef *)in.data(), (uLong)in.size(), level) != Z_OK) return false;
    out.resize(cap);
    return true;
}

/* ---- resources: the override folder first, then the copy embedded in the binary ---- */

static const EmbeddedFile *FindEmbedded(const std::string &name)
{
    for (int i = 0; i < kEmbeddedCount; i++)
        if (name == kEmbedded[i].name) return &kEmbedded[i];
    return NULL;
}

bool Server::Resource(const std::string &name, Bytes &out) const
{
    if (!cfg.resources.empty() && ReadFile(PathJoin(cfg.resources, name), out)) return true;
    const EmbeddedFile *e = FindEmbedded(name);
    if (!e) { out.clear(); return false; }
    if (!e->packed) { out.assign((const char *)e->data, e->size); return true; }
    return Inflate(Bytes((const char *)e->data, e->packedSize), out) && out.size() == e->size;
}

bool Server::ResourceExists(const std::string &name) const
{
    if (!cfg.resources.empty() && FileExists(PathJoin(cfg.resources, name))) return true;
    return FindEmbedded(name) != NULL;
}

int64_t Server::ResourceMtime(const std::string &name) const
{
    if (!cfg.resources.empty() && FileExists(PathJoin(cfg.resources, name))) return FileMtime(PathJoin(cfg.resources, name));
    const EmbeddedFile *e = FindEmbedded(name);
    return e ? e->mtime : 0;
}

} // namespace dw
