// SPDX-License-Identifier: GPL-2.0-or-later

#include "path.h"

#include <algorithm>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace Inkscape::IO {

using std::operator""sv;
using Range = std::ranges::subrange<char const *>;

void action_relative(Range r, std::vector<std::string_view> &parts)
{
    if (parts.empty() || parts.back() == ".."sv) {
        parts.emplace_back(r.begin(), std::ranges::distance(r)); // preserve '..'
    } else {
        parts.pop_back(); // apply '..'
    }
}

void action_absolute(Range _r, std::vector<std::string_view> &parts)
{
    if (!parts.empty()) {
        parts.pop_back(); // apply '..'
    }
}

void action_noop(Range _r, std::vector<std::string_view> &parts) {}

#ifdef _WIN32

void action_fully_qualified(Range _r, std::vector<std::string_view> &parts)
{
    if (parts.size() > 1) { // preserve drive letter
        parts.pop_back();   // apply '..'
    }
}

void action_unc(Range _r, std::vector<std::string_view> &parts)
{
    if (parts.size() > 2) { // preserve server and share
        parts.pop_back();   // apply '..'
    }
}

void action_relative_drive_root(Range r, std::vector<std::string_view> &parts)
{
    if (parts.size() <= 1 || parts.back() == ".."sv) {
        parts.emplace_back(r.begin(), std::ranges::distance(r)); // preserve '..'
    } else {
        parts.pop_back(); // apply '..'
    }
}

void (*SPLIT_PATH_PARENT_ACTIONS[11])(Range, std::vector<std::string_view> &) = {
    action_noop,
    action_noop,
    action_noop,
    action_noop,
    action_unc,
    action_unc,
    action_fully_qualified,
    action_fully_qualified,
    action_relative_drive_root,
    action_absolute,
    action_relative,
};

bool starts_with_separator(std::string_view path)
{
    return path.starts_with('\\') || path.starts_with('/');
}

PathType get_path_type(std::string_view path)
{
    if (path.empty()) {
        return PathType::Empty;
    }
    // drive letter
    if (path.substr(1).starts_with(':')) {
        if (starts_with_separator(path.substr(2))) {
            return PathType::FullyQualified;
        }
        return PathType::RelativeDriveCWD;
    }
    // match without normalization
    if (path.starts_with('\\')) {
        if (path.substr(1).starts_with('\\')) {
            if (path.substr(2).starts_with("?\\"sv)) {
                if (path.substr(4).starts_with("UNC\\"sv)) {
                    return PathType::DeviceNormalizedUNC;
                }
                return PathType::DeviceNormalized;
            }
            return PathType::UNC;
        }
        return PathType::RelativeCurrentDriveRoot;
    }
    // match with normalization
    if (starts_with_separator(path)) {
        if (starts_with_separator(path.substr(1))) {
            if (path.substr(2).starts_with('.')) {
                if (starts_with_separator(path.substr(3))) {
                    if (path.substr(4).starts_with("UNC"sv)) {
                        if (starts_with_separator(path.substr(7))) {
                            return PathType::DeviceUNC;
                        }
                    }
                    return PathType::Device;
                }
            }
            return PathType::UNC;
        }
        return PathType::RelativeCurrentDriveRoot;
    }
    // legacy device
    if (path.size() == 4) {
        if (path.starts_with("COM"sv) || path.starts_with("LPT"sv)) {
            if (std::isdigit(path.back()) > 0) {
                return PathType::LegacyDevice;
            }
        }
    }
    if (path.size() == 3) {
        if (path == "CON"sv || path == "AUX"sv || path == "PRN"sv || path == "NUL"sv) {
            return PathType::LegacyDevice;
        }
    }
    return PathType::RelativeCWD;
}

size_t get_path_type_offset(PathType type)
{
    switch (type) {
        case PathType::FullyQualified:
            return 3;
        case PathType::RelativeCWD:
        case PathType::LegacyDevice:
        case PathType::Empty:
            return 0;
        case PathType::RelativeCurrentDriveRoot:
            return 1;
        case PathType::UNC:
        case PathType::RelativeDriveCWD:
            return 2;
        case PathType::Device:
        case PathType::DeviceNormalized:
            return 4;
        case PathType::DeviceUNC:
        case PathType::DeviceNormalizedUNC:
            return 8;
        default:
            return 0;
    }
}

bool PathParts::is_relative() const
{
    switch (type) {
        case PathType::RelativeCWD:
        case PathType::RelativeCurrentDriveRoot:
        case PathType::RelativeDriveCWD:
            return true;
        default:
            return false;
    }
}

/**
 * Return the prefix that is implicitely stored in the path type
 */
std::string_view PathParts::prefix() const
{
    switch (type) {
        case PathType::FullyQualified:
        case PathType::RelativeDriveCWD:
        case PathType::LegacyDevice:
            return data[0];
        case PathType::UNC:
            return R"(\)";
        case PathType::Device:
            return R"(\\.)";
        case PathType::DeviceNormalized:
            return R"(\\?)";
        case PathType::DeviceUNC:
            return R"(\\.\UNC)";
        case PathType::DeviceNormalizedUNC:
            return R"(\\?\UNC)";
        case PathType::RelativeCWD:
            return ".";
        default:
            return std::string_view{};
    }
}

size_t PathParts::get_join_size() const
{
    auto size = data.size();
    switch (type) {
        case PathType::FullyQualified:
            size -= 1;
            break;
        case PathType::RelativeDriveCWD:
            size -= 2;
            break;
        case PathType::Device:
        case PathType::DeviceNormalized:
            size += 3;
            break;
        case PathType::DeviceUNC:
        case PathType::DeviceNormalizedUNC:
            size += 7;
            break;
        case PathType::UNC:
        case PathType::RelativeCWD:
            size += 1;
            break;
        case PathType::LegacyDevice:
            size = 0;
            break;
        default:;
    }
    for (auto const sv : data) {
        size += sv.size();
    }
    return size;
}

std::string PathParts::join() const
{
    if (data.empty()) {
        switch (type) {
            case PathType::Empty:
                return "";
            case PathType::RelativeCWD:
                return ".";
            case PathType::RelativeCurrentDriveRoot:
                return R"(\)";
            case PathType::UNC:
                return R"(\\)";
            case PathType::Device:
                return R"(\\.\)";
            case PathType::DeviceUNC:
                return R"(\\.\UNC\)";
            case PathType::DeviceNormalized:
                return R"(\\?\)";
            case PathType::DeviceNormalizedUNC:
                return R"(\\?\UNC\)";
        }
    }
    std::string result;
    result.reserve(get_join_size());
    result.append(prefix());
    std::span<std::string_view const> items;
    switch (type) {
        case PathType::FullyQualified:
            items = std::span(data).subspan(1);
            break;
        case PathType::RelativeDriveCWD:
            if (data.size() >= 2) {
                result.append(data[1]);
            }
            items = std::span(data).subspan(2);
            break;
        case PathType::LegacyDevice:
            return result;
        default:
            items = std::span(data);
    }
    for (auto const sv : items) {
        result.push_back('\\');
        result.append(sv);
    }
    return result;
}

void split_path(std::string_view path, std::vector<std::string_view> &parts)
{
    auto it = path | std::views::split("\\"sv);
    for (auto r : it) {
        parts.emplace_back(r.begin(), std::ranges::distance(r));
    }
}

void split_path(std::string_view path, std::vector<std::string_view> &parts, PathType type)
{
    auto tran = [](auto r) { return std::views::split(r, "/"sv); };
    auto pred = [](auto r) { return !(r.empty() || std::ranges::equal(r, "."sv)); };
    auto it =
        path | std::views::split("\\"sv) | std::views::transform(tran) | std::views::join | std::views::filter(pred);
    auto parent_action = SPLIT_PATH_PARENT_ACTIONS[static_cast<int>(type)];
    for (auto r : it) {
        if (std::ranges::equal(r, ".."sv)) {
            parent_action(r, parts);
        } else {
            parts.emplace_back(r.begin(), std::ranges::distance(r));
        }
    }
}

PathParts split_path(std::string_view path)
{
    auto type = get_path_type(path);
    if (type == PathType::Empty) {
        return {};
    } else if (type == PathType::LegacyDevice) {
        return {.data = {path}, .type = type};
    }
    std::vector<std::string_view> parts;
    // store drive letter if present
    if (type == PathType::FullyQualified || type == PathType::RelativeDriveCWD) {
        parts.emplace_back(path.substr(0, 2));
    }
    path = path.substr(get_path_type_offset(type));
    if (type == PathType::DeviceNormalized || type == PathType::DeviceNormalizedUNC) {
        split_path(path, parts);
    } else {
        split_path(path, parts, type);
    }
    return {.data = parts, .type = type};
}

#else // no _WIN32

void (*SPLIT_PATH_PARENT_ACTIONS[3])(Range, std::vector<std::string_view> &) = {
    action_noop,
    action_absolute,
    action_relative,
};

PathType get_path_type(std::string_view path)
{
    if (path.empty()) {
        return PathType::Empty;
    }
    if (path.starts_with('/')) {
        return PathType::Absolute;
    }
    return PathType::RelativeCWD;
}

size_t get_path_type_offset(PathType type)
{
    if (type == PathType::Absolute) {
        return 1;
    }
    return 0;
}

bool PathParts::is_relative() const
{
    return type == PathType::RelativeCWD;
}

std::string_view PathParts::prefix() const
{
    if (type == PathType::RelativeCWD) {
        return "."sv;
    }
    return std::string_view{};
}

size_t PathParts::get_join_size() const
{
    auto size = data.size();
    switch (type) {
        case PathType::RelativeCWD:
            size += 1;
            break;
        default:;
    }
    for (auto const sv : data) {
        size += sv.size();
    }
    return size;
}

std::string PathParts::join() const
{
    if (data.empty()) {
        switch (type) {
            case PathType::Absolute:
                return "/";
            case PathType::RelativeCWD:
                return ".";
            case PathType::Empty:
                return "";
        }
    }
    std::string result;
    result.reserve(get_join_size());
    result.append(prefix());
    for (auto const sv : data) {
        result.push_back('/');
        result.append(sv);
    }
    return result;
}

void split_path(std::string_view path, std::vector<std::string_view> &parts, PathType type)
{
    auto pred = [](auto r) { return !(r.empty() || std::ranges::equal(r, "."sv)); };
    auto it = path | std::views::split("/"sv) | std::views::filter(pred);
    auto parent_action = SPLIT_PATH_PARENT_ACTIONS[static_cast<int>(type)];
    for (auto r : it) {
        if (std::ranges::equal(r, ".."sv)) {
            parent_action(r, parts);
        } else {
            parts.emplace_back(r.begin(), std::ranges::distance(r));
        }
    }
}

PathParts split_path(std::string_view path)
{
    auto type = get_path_type(path);
    if (type == PathType::Empty) {
        return {};
    }
    std::vector<std::string_view> parts;
    path = path.substr(get_path_type_offset(type));
    split_path(path, parts, type);
    return {.data = parts, .type = type};
}

#endif

std::string join_parts_with(std::span<std::string_view const> parts, std::string_view sep)
{
    if (parts.empty()) {
        return std::string{};
    }
    if (parts.size() == 1) {
        return std::string(parts[0]);
    }
    size_t size = sep.size() * (parts.size() - 1);
    for (auto part : parts) {
        size += part.size();
    }
    std::string result;
    result.reserve(size);
    result.append(parts[0]);
    for (auto part : parts.subspan(1)) {
        result.append(sep);
        result.append(part);
    }
    return result;
}

std::string PathParts::join_with(std::string_view sep) const
{
    return join_parts_with(data, sep);
}

std::string PathSortEntry::join_with(std::string_view sep) const
{
    auto data_parts = offset < parts.size() ? std::span(parts.data).subspan(parts.size() - offset) : std::span(parts.data);
    return join_parts_with(data_parts, sep);
}

bool compare_parts_at_offset(PathSortEntry *a, PathSortEntry *b)
{
    auto const size_a = a->parts.data.size();
    auto const size_b = b->parts.data.size();
    auto const offset_a = a->offset;
    auto const offset_b = b->offset;
    if (offset_a > size_a || offset_b > size_b) {
        return size_a > size_b; // place gaps at the end
    }
    return a->parts.data[size_a - offset_a] < b->parts.data[size_b - offset_b];
}

/*
    Recursively sort entries lexicographically by their parts.
    Parts are compared from back to front (basename first).
*/
void sort_path_entries(std::span<PathSortEntry*> entries)
{
    std::sort(entries.begin(), entries.end(), compare_parts_at_offset);
    auto start = 0;
    auto offset = entries[0]->offset;
    auto sv_start = entries[0]->parts.data[entries[0]->parts.size() - offset];
    auto i = 1;
    while (i < entries.size()) {
        if (offset > entries[i]->parts.size()) { // trailing gaps
            for (auto n = i; n < entries.size(); n++) {
                entries[n]->offset++;
            }
            break;
        }
        auto const sv = entries[i]->parts.data[entries[i]->parts.size() - offset];
        if (sv_start != sv) {
            if (start + 1 < i) { // at least 2 entries with same part
                entries[start]->offset++;
                sort_path_entries(std::span(entries).subspan(start, i - start));
            }
            sv_start = sv;
            start = i;
        } else {
            entries[i]->offset++;
        }
        i++;
    }
    if (start + 1 < i) {
        entries[start]->offset++;
        sort_path_entries(std::span(entries).subspan(start, i - start));
    }
}

/*
    Create shortened display strings for a span of file paths.

    If the input span contains unique normalized file paths,
    the output vector will contain unique shortened display strings.
*/
std::vector<std::string> shorten_paths(std::span<std::string_view const> paths, std::string_view sep)
{
    if (paths.empty()) {
        return {};
    }
    std::vector<PathSortEntry> sort_entries;
    sort_entries.reserve(paths.size());
    std::vector<PathSortEntry*> ptrs;
    ptrs.reserve(paths.size());
    // make sure sort_entries does not expand/reallocate during the next loop
    // as this will invalidate the stored references in ptrs
    for (auto path : paths) {
        sort_entries.emplace_back(Inkscape::IO::split_path(path), 1);
        ptrs.push_back(&sort_entries.back());
    }
    sort_path_entries(ptrs);
    std::vector<std::string> result;
    result.reserve(paths.size());
    for (auto const& entry : sort_entries) {
        result.emplace_back(entry.join_with(sep));
    }
    return result;
}

/**
 * Convert an absolute path into a relative one if possible to do in the given number of parent steps.
 *
 * @arg path - The absolute path to convert
 * @arg base - The base or reference absolute path to be relative to
 * @arg parents - The number of parents or .. segments to allow
 *
 * All input strings must have the same encoding,
 * either UTF8 or platform-native encoding (see Glib::filename_to_utf8).
 * The return value has the same encoding as the input.
 */
std::pair<std::string, bool> optimize_path(std::string_view path, std::string_view base, size_t parents)
{
    if (path.empty()) {
        return std::make_pair(std::string{}, false);
    }
    Inkscape::IO::PathParts path_parts = Inkscape::IO::split_path(path);
    if (path_parts.is_relative()) {
        return std::make_pair(path_parts.join(), false);
    }
    Inkscape::IO::PathParts base_parts = Inkscape::IO::split_path(base);
    if (base_parts.is_relative()) {
        return std::make_pair(path_parts.join(), false);
    }
    auto const size = std::min(path_parts.size(), base_parts.size());
    auto count = 0;
    // count the number of parts in the shared prefix
    while (count < size && path_parts.data[count] == base_parts.data[count]) {
        ++count;
    }
    if (count == 0) {
        return std::make_pair(path_parts.join(), false);
    }
    // "remove" the shared prefix
    auto const base_span = std::span(base_parts.data).subspan(count);
    if (base_span.size() > parents) {
        return std::make_pair(path_parts.join(), false);
    }
    auto const path_span = std::span(path_parts.data).subspan(count);
    // construct optimized path
    Inkscape::IO::PathParts p = { .data = {}, .type = Inkscape::IO::PathType::RelativeCWD };
    for (auto i = 0; i < base_span.size(); ++i) {
        p.data.emplace_back("..");
    }
    p.data.insert(p.data.end(), path_span.begin(), path_span.end());
    return std::make_pair(p.join(), true);
}

std::string normalize_path(std::string_view path)
{
    return Inkscape::IO::split_path(path).join();
}

} // namespace Inkscape::IO
