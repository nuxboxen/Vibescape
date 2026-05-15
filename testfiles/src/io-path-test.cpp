// SPDX-License-Identifier: GPL-2.0-or-later

#include "io/path.h"

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace Inkscape {

TEST(IoPathTest, split_path)
{
#ifdef _WIN32
    {
        auto path = "";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::Empty);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{}));
        EXPECT_EQ(parts.join(), "");
    }
    {
        auto path = ".\\";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::RelativeCWD);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{}));
        EXPECT_EQ(parts.join(), ".");
    }
    {
        auto path = "\\files\\test.txt";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::RelativeCurrentDriveRoot);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"files", "test.txt"}));
        EXPECT_EQ(parts.join(), "\\files\\test.txt");
    }
    {
        auto path = "D:..\\stuff\\../..\\file.txt";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::RelativeDriveCWD);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"D:", "..", "..", "file.txt"}));
        EXPECT_EQ(parts.join(), "D:..\\..\\file.txt");
    }
    {
        auto path = "LPT2";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::LegacyDevice);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"LPT2"}));
        EXPECT_EQ(parts.join(), "LPT2");
    }
    {
        auto path = "LPT22.txt";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::RelativeCWD);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"LPT22.txt"}));
        EXPECT_EQ(parts.join(), ".\\LPT22.txt");
    }
    {
        auto path = "C:\\images\\..\\こんにちは/\\.\\\\file.svg";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::FullyQualified);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"C:", "こんにちは", "file.svg"}));
        EXPECT_EQ(parts.join(), "C:\\こんにちは\\file.svg");
    }
    {
        auto path = "\\\\192.168.0.100\\Files\\..\\hello.txt";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::UNC);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"192.168.0.100", "Files", "hello.txt"}));
        EXPECT_EQ(parts.join(), "\\\\192.168.0.100\\Files\\hello.txt");
    }
    {
        auto path = "\\\\.\\C:\\test.txt";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::Device);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"C:", "test.txt"}));
        EXPECT_EQ(parts.join(), "\\\\.\\C:\\test.txt");
    }
    {
        auto path = "\\\\.\\UNC\\127.0.0.1\\Misc/Items\\README.md";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::DeviceUNC);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"127.0.0.1", "Misc", "Items", "README.md"}));
        EXPECT_EQ(parts.join(), "\\\\.\\UNC\\127.0.0.1\\Misc\\Items\\README.md");
    }
    {
        auto path = "\\\\?\\C:\\..\\foo/test.txt";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::DeviceNormalized);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"C:", "..", "foo/test.txt"}));
        EXPECT_EQ(parts.join(), "\\\\?\\C:\\..\\foo/test.txt");
    }
    {
        auto path = "\\\\?\\UNC\\10.0.0.1\\MyShare\\asdf.svg";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::DeviceNormalizedUNC);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"10.0.0.1", "MyShare", "asdf.svg"}));
        EXPECT_EQ(parts.join(), "\\\\?\\UNC\\10.0.0.1\\MyShare\\asdf.svg");
    }
#else // no _WIN32
    {
        auto path = "";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::Empty);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{}));
        EXPECT_EQ(parts.join(), "");
    }
    {
        auto path = "/";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::Absolute);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{}));
        EXPECT_EQ(parts.join(), "/");
    }
    {
        auto path = "./";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::RelativeCWD);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{}));
        EXPECT_EQ(parts.join(), ".");
    }
    {
        auto path = "my file";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::RelativeCWD);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"my file"}));
        EXPECT_EQ(parts.join(), "./my file");
    }
    {
        auto path = "/home/user/../こんにちは/.//file.svg";
        auto parts = Inkscape::IO::split_path(path);
        EXPECT_EQ(parts.type, Inkscape::IO::PathType::Absolute);
        EXPECT_EQ(parts.data, (std::vector<std::string_view>{"home", "こんにちは", "file.svg"}));
        EXPECT_EQ(parts.join(), "/home/こんにちは/file.svg");
    }
#endif
}

TEST(IoPathTest, shorten_paths)
{
    {
        std::vector<std::string_view> paths{};
        auto shortened_paths = Inkscape::IO::shorten_paths(paths, " > ");
        EXPECT_EQ(shortened_paths, (std::vector<std::string>{}));
    }
    {
        std::vector<std::string_view> paths{ "/home/user/files/drawing.svg" };
        auto shortened_paths = Inkscape::IO::shorten_paths(paths, " > ");
        EXPECT_EQ(shortened_paths, (std::vector<std::string>{ "drawing.svg" }));
    }
    {
        std::vector<std::string_view> paths{
            "/home/other/files/drawing.svg",
            "/home/user/stuff/temp/drawing.svg",
            "/home/user/files/temp/todo/drawing.svg",
            "/home/user/files/drawing.svg",
            "/home/user/files/drawing-test.svg",
            "/tmp/user/files/drawing.svg",
        };
        auto shortened_paths = Inkscape::IO::shorten_paths(paths, " > ");
        EXPECT_EQ(shortened_paths, (std::vector<std::string>{
            "other > files > drawing.svg",
            "temp > drawing.svg",
            "todo > drawing.svg",
            "home > user > files > drawing.svg",
            "drawing-test.svg",
            "tmp > user > files > drawing.svg",
        }));
    }
}

TEST(IoPathTest, optimize_path)
{
    {
        auto path = "";
        auto base = "/a/e/f";
        auto [optimized, success] = Inkscape::IO::optimize_path(path, base);
        EXPECT_EQ(success, false);
        EXPECT_EQ(optimized, "");
    }
    {
        auto path = "/a/e/f";
        auto base = "";
        auto [optimized, success] = Inkscape::IO::optimize_path(path, base);
        EXPECT_EQ(success, false);
#ifdef _WIN32
        EXPECT_EQ(optimized, "\\a\\e\\f");
#else // no _WIN32
        EXPECT_EQ(optimized, "/a/e/f");
#endif
    }
    {
        auto path = "./s/t/w";
        auto base = "/a/e/f";
        auto [optimized, success] = Inkscape::IO::optimize_path(path, base);
        EXPECT_EQ(success, false);
#ifdef _WIN32
        EXPECT_EQ(optimized, ".\\s\\t\\w");
#else // no _WIN32
        EXPECT_EQ(optimized, "./s/t/w");
#endif
    }
    {
        auto path = "/s/t/w";
        auto base = "./a/e/f";
        auto [optimized, success] = Inkscape::IO::optimize_path(path, base);
        EXPECT_EQ(success, false);
#ifdef _WIN32
        EXPECT_EQ(optimized, "\\s\\t\\w");
#else // no _WIN32
        EXPECT_EQ(optimized, "/s/t/w");
#endif
    }
    {
        auto path = "/a/b/c/d";
        auto base = "/g/m/n";
        auto [optimized, success] = Inkscape::IO::optimize_path(path, base);
        EXPECT_EQ(success, false);
#ifdef _WIN32
        EXPECT_EQ(optimized, "\\a\\b\\c\\d");
#else // no _WIN32
        EXPECT_EQ(optimized, "/a/b/c/d");
#endif
    }
    {
        auto path = "/a/b/c/d";
        auto base = "/a/e/f";
        auto [optimized, success] = Inkscape::IO::optimize_path(path, base);
#ifdef _WIN32
        EXPECT_EQ(success, false);
        EXPECT_EQ(optimized, "\\a\\b\\c\\d");
#else // no _WIN32
        EXPECT_EQ(success, true);
        EXPECT_EQ(optimized, "./../../b/c/d");
#endif
    }
    {
        auto path = "C:/a/b/c";
        auto base = "C:/a/e";
        auto [optimized, success] = Inkscape::IO::optimize_path(path, base);
#ifdef _WIN32
        EXPECT_EQ(success, true);
        EXPECT_EQ(optimized, ".\\..\\b\\c");
#else // no _WIN32
        EXPECT_EQ(success, false);
        EXPECT_EQ(optimized, "./C:/a/b/c");
#endif
    }
}

} // namespace Inkscape
