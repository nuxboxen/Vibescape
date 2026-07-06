// SPDX-License-Identifier: GPL-2.0-or-later
//

#include "string-convert.h"

#include <cassert>
#include <string>
#include <glib.h>
#include <glibmm/ustring.h>

namespace Inkscape {

#if defined(__linux__) && defined(__GLIBCXX__)
// wchar_t is 32-bit on Linux, same as gunichar, so use Glib::ustring.
std::wstring utf8_to_wstring(const std::string& str) {
    auto ustr = Glib::ustring(str);
    return {ustr.data(), ustr.data() + ustr.size()};
}

std::string wstring_to_utf8(const wchar_t* wstr) {
    auto len = std::char_traits<wchar_t>::length(wstr);
    auto ustr = Glib::ustring(reinterpret_cast<const gunichar*>(wstr),
                               reinterpret_cast<const gunichar*>(wstr) + len);
    return ustr.raw();
}
#else
// Fallback using GLib conversion for platforms where wchar_t != 32-bit.
std::wstring utf8_to_wstring(const std::string& str) {
    glong len = 0;
    auto utf16 = g_utf8_to_utf16(str.c_str(), -1, nullptr, &len, nullptr);
    std::wstring result;
    if (utf16) {
        result.assign(reinterpret_cast<const wchar_t*>(utf16), len);
        g_free(utf16);
    }
    return result;
}

std::string wstring_to_utf8(const wchar_t* wstr) {
    auto len = std::char_traits<wchar_t>::length(wstr);
    auto utf8 = g_utf16_to_utf8(reinterpret_cast<const gunichar2*>(wstr), len, nullptr, nullptr, nullptr);
    std::string result;
    if (utf8) {
        result = utf8;
        g_free(utf8);
    }
    return result;
}
#endif

std::string wstring_to_utf8(const gunichar* wstr, unsigned int count) {
    assert(wstr);
    Glib::ustring str{wstr, wstr + count};
    return str.raw();
}

std::string unicode_char_to_utf8(unsigned int unicode) {
    gunichar chr = unicode;
    Glib::ustring str{&chr, &chr + 1};
    return str.raw();
}

} // Inkscape
