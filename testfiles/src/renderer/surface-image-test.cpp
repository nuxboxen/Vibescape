// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "inkscape.h"

#include "color-testbase.h"
#include "surface-testbase.h"

#include "renderer/surface-image.h"

static std::string base64of(const std::string &s)
{
    gchar *encoded = g_base64_encode(reinterpret_cast<guchar const *>(s.c_str()), s.size());
    std::string r(encoded);
    g_free(encoded);
    return r;
}

TEST(SurfaceImage, LoadPngFile)
{
    Glib::init();
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(INKSCAPE_TESTS_DIR "/data/renderer/transform-source-16.png");
    auto image = Renderer::Image(file);

    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(image,
                    "        "
                    "   :*   "
                    "  :$&*  "
                    " :$&&&* "
                    " *&&&&$."
                    "  *&&$. "
                    "   *$.  "
                    "    .   "
    , 50);
}

TEST(SurfaceImage, LoadPngSaveJpegBase64)
{
    Glib::init();
    std::string img = "image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAYAAABWKLW/AAAAJUlEQVQI1yXJoREAIAwEsPQQxWDZf86aRxAbYV8SVh0CA6H6Tz+kLwgyOkrwGwAAAABJRU5ErkJggg==";
    auto image = Renderer::Image(img);
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(image,
        " & "
        "&&&"
        " & "
    , 1);

    auto output = image.encode_as_base64("image/jpeg");
    EXPECT_EQ(output, "data:image/jpeg;base64,/9j/4AAQSkZJRgABAgAAAQABAAD/wAALCAADAAMBABEA/9sAQwADAgIDAgIDAwMDBAMDBAUIBQUEBAUKBwcGCAwKDAwLCgsLDQ4SEA0OEQ4LCxAWEBETFBUVFQwPFxgWFBgSFBUU/9sAQwEDBAQFBAUJBQUJFA0LDRQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQU/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/9oACAEAAAA/APlnwr4VsrjTJ3efUgRfXifJqlyowtzKBwJBzgcnqTknJJNf/9k=");
}

TEST(SurfaceImage, LoadPngWithICCProfile)
{
    Glib::init();
    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(INKSCAPE_TESTS_DIR "/data/renderer/red-green-icc.png");
    auto image = Renderer::Image(file);

    auto space = image.getColorSpace();
    EXPECT_EQ(space->getName(), "Swapped-Red-and-Green");

    // Original data is loaded, the edge is Red according to the icc, but the red
    // channel is now the second channel not the first. The middle is green likewise.
    EXPECT_COLOR_IS(image, 1, 1, {0.0, 1.0, 0.0, 1.0});
    EXPECT_COLOR_IS(image, 40, 40, {1.0, 0.0, 0.0, 1.0});

    EXPECT_IMAGE_IS(image,
         "88888888"
         "86222225"
         "82222225"
         "82222225"
         "82222225"
         "82222225"
         "82222225"
         "85555559"
    , 9);
    image.write_to_png("/tmp/foo");
}

TEST(SurfaceImage, LoadSVGFile)
{
}

TEST(SurfaceImage, LoadSVGBase64)
{
    /*
    Inkscape::Application::create(false);
    std::string img = "image/svg+xml;base64," + base64of("<svg><path d=\"M 71.527648,186.14229 A 740.48715,740.48715 0 0 0 696.31258,625.8041 Z\"/></svg>");
    auto image = Renderer::Image(img);
    EXPECT_IMAGE_IS<PixelPatch::Method::ALPHA>(image,
        " & "
        "&&&"
        " & "
    , 1);
    */
}

TEST(SurfaceImage, LoadSVGWithICCProfile)
{
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
