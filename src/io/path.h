// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef IO_PATH_H
#define IO_PATH_H

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Inkscape::IO {

#ifdef _WIN32

enum class PathType
{
    Empty,
    LegacyDevice,
    DeviceNormalized,
    DeviceNormalizedUNC,
    UNC,
    DeviceUNC,
    Device,
    FullyQualified,
    RelativeDriveCWD,
    RelativeCurrentDriveRoot,
    RelativeCWD,
};

#else // no _WIN32

enum class PathType
{
    Empty,
    Absolute,
    RelativeCWD,
};

#endif

struct PathParts
{
    std::vector<std::string_view> data;
    PathType type;

    size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }
    std::string_view const operator[](size_t index) { return data[index]; }
    std::string_view basename() const { return data.back(); }
    std::span<std::string_view const> dirnames() const { return std::span(data).first(data.size() - 1); }
    std::vector<std::string> allocate_strings() const { return std::vector<std::string>(data.begin(), data.end()); }
    bool is_absolute() const { return !is_relative(); }
    bool is_relative() const;
    std::string_view prefix() const;
    size_t get_join_size() const;
    std::string join() const;
    std::string join_with(std::string_view sep) const;
};

struct PathSortEntry
{
    Inkscape::IO::PathParts parts;
    size_t offset;

    std::string join_with(std::string_view sep) const;
};

PathParts split_path(std::string_view path);
std::vector<std::string> shorten_paths(std::span<std::string_view const> paths, std::string_view sep);
std::pair<std::string, bool> optimize_path(std::string_view path, std::string_view base, size_t parents = 2);
std::string normalize_path(std::string_view path);

} // namespace Inkscape::IO

#endif // IO_PATH_H
