// SPDX-License-Identifier: GPL-2.0-or-later

#include "io/fix-broken-links.h"

#include <filesystem>
#include <fstream>

#include <glib.h>
#include <gtest/gtest.h>

namespace Inkscape {

TEST(IoFixBrokenLinksTest, search_upwards_and_concat_paths)
{
    std::filesystem::path tmp_dir = std::filesystem::temp_directory_path() / "inkscape-search-upwards-and-concat-test-XXXXXXXXXX";
    auto tmp_dir_str = tmp_dir.string();
    g_mkdtemp((char *)tmp_dir_str.c_str());
    tmp_dir = tmp_dir_str;
    {
        auto empty = "";
        auto output = Inkscape::IO::search_upwards_and_concat_paths(tmp_dir_str, empty);
        EXPECT_EQ(output, "");
    }
    {
        std::filesystem::path subpath = std::filesystem::path("c") / "d" / "non_existent.svg";
        auto output = Inkscape::IO::search_upwards_and_concat_paths(tmp_dir_str, subpath.string());
        EXPECT_EQ(output, "");
    }
    {
        std::filesystem::path file_path = tmp_dir / "a" / "d" / "test.svg";
        std::filesystem::create_directories(file_path.parent_path());
        std::ofstream file(file_path); // create empty file
        file.close();
        std::filesystem::path subpath = std::filesystem::path("c") / "d" / "test.svg";
        auto output = Inkscape::IO::search_upwards_and_concat_paths(file_path.parent_path().string(), subpath.string());
        EXPECT_EQ(output, file_path.string());
    }
    std::filesystem::remove_all(tmp_dir);
}

} // namespace Inkscape
