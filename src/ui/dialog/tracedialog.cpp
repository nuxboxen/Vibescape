// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Bitmap tracing settings dialog - second implementation.
 */
/* Authors:
 *   Bob Jamison
 *   Marc Jeanmougin <marc.jeanmougin@telecom-paristech.fr>
 *   PBS <pbs3141@gmail.com>
 *   Others - see git history.
 *
 * Copyright (C) 2019-2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "tracedialog.h"

#include <map>
#include <string>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/eventcontrollerfocus.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/frame.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/grid.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/image.h>
#include <gtkmm/picture.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/snapshot.h>
#include <gtkmm/stack.h>

#include "colors/color.h"
#include "colors/color-set.h"
#include "desktop.h"
#include "display/cairo-utils.h"
#include "preferences.h"
#include "object/sp-image.h"
#include "selection.h"
#include "ui/tools/dropper-tool.h"
#include "ui/tools/tool-base.h"
#include "trace/autotrace/inkscape-autotrace.h"
#include "trace/depixelize/inkscape-depixelize.h"
#include "trace/imagemap-gdk.h"
#include "trace/potrace/inkscape-potrace.h"
#include "trace/quantize.h"
#include "ui/widget/color-picker-panel.h"
#include "ui/builder-utils.h"
#include "ui/util.h"
#include "ui/widget/generic/bin.h"

namespace Inkscape::UI::Dialog {
namespace {

constexpr auto cbt_ss_map = std::to_array({
    Trace::Potrace::TraceType::BRIGHTNESS,
    Trace::Potrace::TraceType::CANNY,
    Trace::Potrace::TraceType::QUANT,
    Trace::Potrace::TraceType::AUTOTRACE_SINGLE,
    Trace::Potrace::TraceType::AUTOTRACE_CENTERLINE
});

constexpr auto cbt_ms_map = std::to_array({
    Trace::Potrace::TraceType::BRIGHTNESS_MULTI,
    Trace::Potrace::TraceType::QUANT_COLOR,
    Trace::Potrace::TraceType::QUANT_MONO,
    Trace::Potrace::TraceType::AUTOTRACE_MULTI
});

enum class EngineType
{
    Potrace,
    Autotrace,
    Depixelize
};

// CBT_MS index 1 = "Colors" mode
bool isCustomPaletteMode(int ms_selection)
{
    return ms_selection == 1;
}

/// Colored rectangle widget with an eyedropper icon overlay on hover.
class ColorSwatchRect : public Gtk::Widget
{
public:
    ColorSwatchRect()
    {
        set_size_request(32, 24);

        auto motion = Gtk::EventControllerMotion::create();
        motion->signal_enter().connect([this] (double, double) { hovered = true; queue_draw(); });
        motion->signal_leave().connect([this] { hovered = false; queue_draw(); });
        add_controller(motion);
    }

    void set_color(Gdk::RGBA const &c) { color = c; queue_draw(); }
    Gdk::RGBA get_color() const { return color; }

protected:
    void snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot) override
    {
        int w = get_width();
        int h = get_height();
        if (w <= 0 || h <= 0) return;

        auto fill = GdkRGBA{
            static_cast<float>(color.get_red()),
            static_cast<float>(color.get_green()),
            static_cast<float>(color.get_blue()),
            1.0f
        };
        auto rect = GRAPHENE_RECT_INIT(0, 0, static_cast<float>(w), static_cast<float>(h));
        gtk_snapshot_append_color(snapshot->gobj(), &fill, &rect);

        // 1px border.
        auto bc = GdkRGBA{0.3f, 0.3f, 0.3f, 1.0f};
        auto top    = GRAPHENE_RECT_INIT(0, 0, (float)w, 1);
        auto bottom = GRAPHENE_RECT_INIT(0, (float)(h-1), (float)w, 1);
        auto left   = GRAPHENE_RECT_INIT(0, 0, 1, (float)h);
        auto right  = GRAPHENE_RECT_INIT((float)(w-1), 0, 1, (float)h);
        gtk_snapshot_append_color(snapshot->gobj(), &bc, &top);
        gtk_snapshot_append_color(snapshot->gobj(), &bc, &bottom);
        gtk_snapshot_append_color(snapshot->gobj(), &bc, &left);
        gtk_snapshot_append_color(snapshot->gobj(), &bc, &right);

        // Eyedropper icon on hover.
        if (hovered) {
            auto theme = Gtk::IconTheme::get_for_display(get_display());
            auto icon = theme->lookup_icon("color-picker-symbolic", 16);
            if (icon) {
                auto overlay_bg = GdkRGBA{0.0f, 0.0f, 0.0f, 0.4f};
                gtk_snapshot_append_color(snapshot->gobj(), &overlay_bg, &rect);
                float ix = (w - 16) / 2.0f;
                float iy = (h - 16) / 2.0f;
                auto pt = GRAPHENE_POINT_INIT(ix, iy);
                gtk_snapshot_save(snapshot->gobj());
                gtk_snapshot_translate(snapshot->gobj(), &pt);
                icon->snapshot(snapshot, 16, 16);
                gtk_snapshot_restore(snapshot->gobj());
            }
        }
    }

private:
    Gdk::RGBA color;
    bool hovered = false;
};

/// A palette row: [color swatch] [#RRGGBB label] [delete button].
/// Clicking the hex label or swatch opens/closes a shared color picker below.
class PaletteRowWidget : public Gtk::Box
{
public:
    PaletteRowWidget()
        : Gtk::Box(Gtk::Orientation::HORIZONTAL, 4)
    {
        set_hexpand(true);
        swatch.set_valign(Gtk::Align::CENTER);
        hex_label.set_xalign(0);
        hex_label.set_hexpand(true);
        hex_label.set_valign(Gtk::Align::CENTER);
        delete_btn.set_icon_name("edit-delete-symbolic");
        delete_btn.set_valign(Gtk::Align::CENTER);
        delete_btn.set_tooltip_text("Remove this color");
        delete_btn.add_css_class("flat");

        expand_arrow.set_from_icon_name("pan-end-symbolic");
        expand_arrow.set_valign(Gtk::Align::CENTER);

        append(swatch);
        append(expand_arrow);
        append(hex_label);
        append(delete_btn);

        // Initialize the ColorSet for this row's color.
        color_set = std::make_shared<Colors::ColorSet>();
        color_set->signal_changed.connect([this] {
            if (updating) return;
            auto opt = color_set->get();
            if (!opt) return;
            uint32_t rgba = opt->toRGBA();
            Gdk::RGBA c;
            c.set_red(  ((rgba >> 24) & 0xFF) / 255.0);
            c.set_green(((rgba >> 16) & 0xFF) / 255.0);
            c.set_blue( ((rgba >>  8) & 0xFF) / 255.0);
            c.set_alpha(1.0);
            swatch.set_color(c);
            update_hex_label();
            if (on_color_changed) on_color_changed();
        });
    }

    void set_color(Gdk::RGBA const &c)
    {
        updating = true;
        swatch.set_color(c);
        // Push into the ColorSet.
        color_set->set(Colors::Color(
            static_cast<uint32_t>(
                (static_cast<int>(c.get_red()   * 255) << 24) |
                (static_cast<int>(c.get_green() * 255) << 16) |
                (static_cast<int>(c.get_blue()  * 255) <<  8) |
                0xFF
            )
        ));
        update_hex_label();
        updating = false;
    }

    Gdk::RGBA get_color() const { return swatch.get_color(); }

    void set_expanded(bool expanded) {
        expand_arrow.set_from_icon_name(expanded ? "pan-down-symbolic" : "pan-end-symbolic");
    }

    void update_hex_label()
    {
        auto c = swatch.get_color();
        char buf[8];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X",
            static_cast<int>(c.get_red()   * 255),
            static_cast<int>(c.get_green() * 255),
            static_cast<int>(c.get_blue()  * 255));
        hex_label.set_label(buf);
    }

    ColorSwatchRect swatch;
    Gtk::Image expand_arrow;
    Gtk::Label hex_label;
    Gtk::Button delete_btn;
    std::shared_ptr<Colors::ColorSet> color_set;
    std::function<void()> on_color_changed;
    std::function<void()> on_delete;
    std::function<void()> on_expand;
    bool updating = false;
};

struct TraceData
{
    std::unique_ptr<Trace::TracingEngine> engine;
    bool sioxEnabled;
};

class TraceDialogImpl : public TraceDialog
{
public:
    TraceDialogImpl();
    ~TraceDialogImpl() override;

protected:
    void selectionModified(Selection *selection, unsigned flags) override;
    void selectionChanged(Selection *selection) override;

private:
    TraceData getTraceData() const;
    void setDefaults();
    void adjustParamsVisible();
    void onTraceClicked();
    void onAbortClicked();
    bool previewsEnabled() const;
    void schedulePreviewUpdate(int msecs, bool force = false);
    void updatePreview(bool force = false);
    void launchPreviewGeneration();

    // Handles to ongoing asynchronous computations.
    Trace::TraceFuture trace_future;
    Trace::TraceFuture preview_future;

    // Delayed preview generation.
    sigc::connection preview_timeout_conn;
    bool preview_pending_recompute = false;

    Glib::RefPtr<Gtk::Builder> builder;
    UI::Widget::Bin bin;
    Glib::RefPtr<Gtk::Adjustment> MS_scans, PA_curves, PA_islands, PA_sparse1, PA_sparse2;
    Glib::RefPtr<Gtk::Adjustment> SS_AT_ET_T, SS_AT_FI_T, SS_BC_T, SS_CQ_T, SS_ED_T;
    Glib::RefPtr<Gtk::Adjustment> optimize, smooth, speckles;
    Gtk::DropDown &CBT_SS, &CBT_MS;
    Gtk::CheckButton &CB_invert, &CB_MS_smooth, &CB_MS_stack, &CB_MS_rb;
    Gtk::CheckButton &CB_speckles,  &CB_smooth,  &CB_optimize,  &CB_SIOX;
    Gtk::CheckButton &CB_speckles1, &CB_smooth1, &CB_optimize1, &CB_SIOX1, &CB_PA_optimize;
    Gtk::CheckButton &RB_PA_voronoi;
    Gtk::Button &B_RESET, &B_STOP, &B_OK, &B_Update;
    Gtk::Box &mainBox;
    Gtk::Notebook &choice_tab;
    Gtk::Picture &previewArea;
    Gtk::Box &orient_box;
    Gtk::Frame &_preview_frame;
    Gtk::Grid &_param_grid;
    Gtk::CheckButton &_live_preview;
    Gtk::Stack &stack;
    Gtk::ProgressBar &progressbar;
    Gtk::Box &boxchild1, &boxchild2;
    // Palette editor widgets
    Gtk::Box &palette_box;
    Gtk::Box &palette_list;
    Gtk::Button &B_palette_add, &B_palette_reset;
    Gtk::Label &L_palette_count;
    std::vector<PaletteRowWidget*> palette_rows;
    sigc::scoped_connection _dropper_connection;
    // Shared color picker panel (expanded below the active row).
    std::unique_ptr<UI::Widget::ColorPickerPanel> color_panel;
    int active_panel_index = -1; // which row has the panel open, or -1
    void toggleColorPanel(int index);

    void addPaletteColor(Gdk::RGBA const &color);
    void removePaletteColor(int index);
    void clearPalette();
    void extractPaletteFromImage();
    void updatePaletteVisibility();
    void updatePaletteCountLabel();
    void pickColorForSwatch(int index);
    std::vector<Trace::RGB> getCustomPalette() const;

    // Per-image palette cache: preserves user edits across deselect/reselect cycles.
    std::map<std::string, std::vector<Trace::RGB>> _palette_cache;
    std::string _current_image_id;
    void savePaletteToCache();
    void restorePaletteFromCache(std::string const &image_id);

    sigc::scoped_connection _page_switched;
};

enum class Page
{
    SingleScan,
    MultiScan,
    PixelArt
};

TraceData TraceDialogImpl::getTraceData() const
{
    auto current_page = static_cast<Page>(choice_tab.get_current_page());

    auto &cb_siox = current_page == Page::SingleScan ? CB_SIOX : CB_SIOX1;
    bool enable_siox = cb_siox.get_active();

    auto trace_type = current_page == Page::SingleScan
                    ? cbt_ss_map.at(CBT_SS.get_selected())
                    : cbt_ms_map.at(CBT_MS.get_selected());

    EngineType engine_type;
    if (current_page == Page::PixelArt) {
        engine_type = EngineType::Depixelize;
    } else {
        switch (trace_type) {
            case Inkscape::Trace::Potrace::TraceType::AUTOTRACE_SINGLE:
            case Inkscape::Trace::Potrace::TraceType::AUTOTRACE_CENTERLINE:
            case Inkscape::Trace::Potrace::TraceType::AUTOTRACE_MULTI:
                engine_type = EngineType::Autotrace;
                break;
            default:
                engine_type = EngineType::Potrace;
                break;
        }
    }

    auto setup_potrace = [&, this] {
        auto palette = getCustomPalette();
        int nrColors = palette.empty() ? (int)MS_scans->get_value() : (int)palette.size();

        auto eng = std::make_unique<Trace::Potrace::PotraceTracingEngine>(
            trace_type, CB_invert.get_active(), (int)SS_CQ_T->get_value(), SS_BC_T->get_value(),
            0, // Brightness floor
            SS_ED_T->get_value(), nrColors, CB_MS_stack.get_active(), CB_MS_smooth.get_active(),
            CB_MS_rb.get_active());

        // Set custom palette if in Colors mode with user-defined colors.
        if (current_page == Page::MultiScan && isCustomPaletteMode(CBT_MS.get_selected()) && !palette.empty()) {
            eng->setCustomPalette(std::move(palette));
        }

        auto &cb_optimize = current_page == Page::SingleScan ? CB_optimize : CB_optimize1;
        eng->setOptiCurve(cb_optimize.get_active());
        eng->setOptTolerance(optimize->get_value());

        auto &cb_smooth = current_page == Page::SingleScan ? CB_smooth : CB_smooth1;
        eng->setAlphaMax(cb_smooth.get_active() ? smooth->get_value() : 0);

        auto &cb_speckles = current_page == Page::SingleScan ? CB_speckles : CB_speckles1;
        eng->setTurdSize(cb_speckles.get_active() ? (int)speckles->get_value() : 0);

        return eng;
    };

    auto setup_autotrace = [&, this] {
        auto eng = std::make_unique<Trace::Autotrace::AutotraceTracingEngine>();

        switch (trace_type) {
            case Inkscape::Trace::Potrace::TraceType::AUTOTRACE_SINGLE:
                eng->setColorCount(2);
                break;
            case Inkscape::Trace::Potrace::TraceType::AUTOTRACE_CENTERLINE:
                eng->setColorCount(2);
                eng->setCenterLine(true);
                eng->setPreserveWidth(true);
                break;
            case Inkscape::Trace::Potrace::TraceType::AUTOTRACE_MULTI:
                // Setting color count to 1 produces nothing, setting it to 2 gives a single scan.
                // It seems that the number of scans being done is 1 fewer than the color count we
                // set. Thus, incrementing the value by 1 gives the desired number of scans.
                // According to autotrace documentation, the domain of the color-count variable is
                // [1, 256]. Since, we add an extra 1 to get rid of the fewer scans problem, we must
                // limit the UI input values to [1, 255] to not override the original domain.
                eng->setColorCount((int)MS_scans->get_value() + 1);
                break;
            default:
                assert(false);
                break;
        }

        eng->setFilterIterations((int)SS_AT_FI_T->get_value());
        eng->setErrorThreshold(SS_AT_ET_T->get_value());

        return eng;
    };

    auto setup_depixelize = [this] {
        return std::make_unique<Trace::Depixelize::DepixelizeTracingEngine>(
            RB_PA_voronoi.get_active() ? Inkscape::Trace::Depixelize::TraceType::VORONOI : Inkscape::Trace::Depixelize::TraceType::BSPLINES,
            PA_curves->get_value(), (int) PA_islands->get_value(),
            (int) PA_sparse1->get_value(), PA_sparse2->get_value(),
            CB_PA_optimize.get_active());
    };

    TraceData data;
    switch (engine_type) {
        case EngineType::Potrace:    data.engine = setup_potrace();    break;
        case EngineType::Autotrace:  data.engine = setup_autotrace();  break;
        case EngineType::Depixelize: data.engine = setup_depixelize(); break;
        default: assert(false); break;
    }
    data.sioxEnabled = enable_siox;

    return data;
}

void TraceDialogImpl::selectionChanged(Inkscape::Selection *selection)
{
    if (isCustomPaletteMode(CBT_MS.get_selected())) {
        // Save current palette for the previous image before switching.
        savePaletteToCache();

        // Determine the newly selected image's id.
        std::string new_id;
        if (selection) {
            if (auto img = cast<SPImage>(selection->singleItem())) {
                if (auto id = img->getId()) {
                    new_id = id;
                }
            }
        }
        _current_image_id = new_id;

        if (!new_id.empty()) {
            // Restore cached palette if available, otherwise extract from image.
            auto it = _palette_cache.find(new_id);
            if (it != _palette_cache.end()) {
                restorePaletteFromCache(new_id);
            } else {
                extractPaletteFromImage();
            }
        } else {
            clearPalette();
        }
    }
    updatePreview();
}

void TraceDialogImpl::selectionModified(Selection *selection, unsigned flags)
{
    auto mask = SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_PARENT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG;
    if ((flags & mask) == mask) {
        // All flags set - preview instantly.
        updatePreview();
    } else if (flags & mask) {
        // At least one flag set - preview after a long delay.
        schedulePreviewUpdate(1000);
    }
}

void TraceDialogImpl::setDefaults()
{
    MS_scans->set_value(8);
    PA_curves->set_value(1);
    PA_islands->set_value(5);
    PA_sparse1->set_value(4);
    PA_sparse2->set_value(1);
    SS_AT_FI_T->set_value(4);
    SS_AT_ET_T->set_value(2);
    SS_BC_T->set_value(0.45);
    SS_CQ_T->set_value(64);
    SS_ED_T->set_value(.65);
    optimize->set_value(0.2);
    smooth->set_value(1);
    speckles->set_value(2);
    CB_invert.set_active(false);
    CB_MS_smooth.set_active(true);
    CB_MS_stack.set_active(true);
    CB_MS_rb.set_active(false);
    CB_speckles.set_active(true);
    CB_smooth.set_active(true);
    CB_optimize.set_active(true);
    CB_speckles1.set_active(true);
    CB_smooth1.set_active(true);
    CB_optimize1.set_active(true);
    CB_PA_optimize.set_active(false);
    CB_SIOX.set_active(false);
    CB_SIOX1.set_active(false);
}

void TraceDialogImpl::onAbortClicked()
{
    if (!trace_future) {
        // Not tracing; nothing to cancel.
        return;
    }

    stack.set_visible_child(boxchild1);
    if (auto desktop = getDesktop()) desktop->clearWaitingCursor();
    trace_future.cancel();
}

void TraceDialogImpl::onTraceClicked()
{
    if (trace_future) {
        // Still tracing; wait for either finished or cancelled.
        return;
    }

    // Attempt to fire off the tracer.
    auto data = getTraceData();
    trace_future = Trace::trace(std::move(data.engine), data.sioxEnabled,
        // On progress:
        [this] (double progress) {
            progressbar.set_fraction(progress);
        },
        // On completion without cancelling:
        [this] {
            progressbar.set_fraction(1.0);
            stack.set_visible_child(boxchild1);
            if (auto desktop = getDesktop()) desktop->clearWaitingCursor();
            trace_future.cancel();
        }
    );

    if (trace_future) {
        // Put the UI into the tracing state.
        if (auto desktop = getDesktop()) desktop->setWaitingCursor();
        stack.set_visible_child(boxchild2);
        progressbar.set_fraction(0.0);
    }
}

using Inkscape::UI::create_builder;
using Inkscape::UI::get_widget;

TraceDialogImpl::TraceDialogImpl()
  : builder(create_builder("dialog-trace.glade"))
    // Adjustment
  , MS_scans       (get_object<Gtk::Adjustment>(builder,              "MS_scans"))
  , PA_curves      (get_object<Gtk::Adjustment>(builder,             "PA_curves"))
  , PA_islands     (get_object<Gtk::Adjustment>(builder,            "PA_islands"))
  , PA_sparse1     (get_object<Gtk::Adjustment>(builder,            "PA_sparse1"))
  , PA_sparse2     (get_object<Gtk::Adjustment>(builder,            "PA_sparse2"))
  , SS_AT_FI_T     (get_object<Gtk::Adjustment>(builder,            "SS_AT_FI_T"))
  , SS_AT_ET_T     (get_object<Gtk::Adjustment>(builder,            "SS_AT_ET_T"))
  , SS_BC_T        (get_object<Gtk::Adjustment>(builder,               "SS_BC_T"))
  , SS_CQ_T        (get_object<Gtk::Adjustment>(builder,               "SS_CQ_T"))
  , SS_ED_T        (get_object<Gtk::Adjustment>(builder,               "SS_ED_T"))
  , optimize       (get_object<Gtk::Adjustment>(builder,              "optimize"))
  , smooth         (get_object<Gtk::Adjustment>(builder,                "smooth"))
  , speckles       (get_object<Gtk::Adjustment>(builder,              "speckles"))
    // ComboBoxText
  , CBT_SS         (get_widget<Gtk::DropDown>    (builder,              "CBT_SS"))
  , CBT_MS         (get_widget<Gtk::DropDown>    (builder,              "CBT_MS"))
    // CheckButton
  , CB_invert      (get_widget<Gtk::CheckButton> (builder,           "CB_invert"))
  , CB_MS_smooth   (get_widget<Gtk::CheckButton> (builder,        "CB_MS_smooth"))
  , CB_MS_stack    (get_widget<Gtk::CheckButton> (builder,         "CB_MS_stack"))
  , CB_MS_rb       (get_widget<Gtk::CheckButton> (builder,            "CB_MS_rb"))
  , CB_speckles    (get_widget<Gtk::CheckButton> (builder,         "CB_speckles"))
  , CB_smooth      (get_widget<Gtk::CheckButton> (builder,           "CB_smooth"))
  , CB_optimize    (get_widget<Gtk::CheckButton> (builder,         "CB_optimize"))
  , CB_SIOX        (get_widget<Gtk::CheckButton> (builder,             "CB_SIOX"))
  , CB_speckles1   (get_widget<Gtk::CheckButton> (builder,        "CB_speckles1"))
  , CB_smooth1     (get_widget<Gtk::CheckButton> (builder,          "CB_smooth1"))
  , CB_optimize1   (get_widget<Gtk::CheckButton> (builder,        "CB_optimize1"))
  , CB_SIOX1       (get_widget<Gtk::CheckButton> (builder,            "CB_SIOX1"))
  , CB_PA_optimize (get_widget<Gtk::CheckButton> (builder,      "CB_PA_optimize"))
    // RadioButton
  , RB_PA_voronoi  (get_widget<Gtk::CheckButton> (builder,       "RB_PA_voronoi"))
    // Button
  , B_RESET        (get_widget<Gtk::Button>      (builder,             "B_RESET"))
  , B_STOP         (get_widget<Gtk::Button>      (builder,              "B_STOP"))
  , B_OK           (get_widget<Gtk::Button>      (builder,                "B_OK"))
  , B_Update       (get_widget<Gtk::Button>      (builder,            "B_Update"))
    // Box
  , mainBox        (get_widget<Gtk::Box>         (builder,             "mainBox"))
  , choice_tab     (get_widget<Gtk::Notebook>    (builder,          "choice_tab"))
  , previewArea    (get_widget<Gtk::Picture>     (builder,         "previewArea"))
  , orient_box     (get_widget<Gtk::Box>         (builder,          "orient_box"))
  , _preview_frame (get_widget<Gtk::Frame>       (builder,      "_preview_frame"))
  , _param_grid    (get_widget<Gtk::Grid>        (builder,         "_param_grid"))
  , _live_preview  (get_widget<Gtk::CheckButton> (builder,       "_live_preview"))
  , stack          (get_widget<Gtk::Stack>       (builder,               "stack"))
  , progressbar    (get_widget<Gtk::ProgressBar> (builder,         "progressbar"))
  , boxchild1      (get_widget<Gtk::Box>         (builder,           "boxchild1"))
  , boxchild2      (get_widget<Gtk::Box>         (builder,           "boxchild2"))
    // Palette editor
  , palette_box    (get_widget<Gtk::Box>         (builder,        "palette_box"))
  , palette_list   (get_widget<Gtk::Box>         (builder,     "palette_list"))
  , B_palette_add  (get_widget<Gtk::Button>      (builder,     "B_palette_add"))
  , B_palette_reset(get_widget<Gtk::Button>      (builder,  "B_palette_reset"))
  , L_palette_count(get_widget<Gtk::Label>       (builder, "L_palette_count"))
{
    builder->get_objects(); // instantiate all InkSpinButton instances
    append(bin);
    bin.set_child(mainBox);
    bin.set_expand(true);

    auto prefs = Preferences::get();

    _live_preview.set_active(prefs->getBool(getPrefsPath() + "liveUpdate", true));

    B_Update.signal_clicked().connect([this] { updatePreview(true); });
    B_OK.signal_clicked().connect(sigc::mem_fun(*this, &TraceDialogImpl::onTraceClicked));
    B_STOP.signal_clicked().connect(sigc::mem_fun(*this, &TraceDialogImpl::onAbortClicked));
    B_RESET.signal_clicked().connect(sigc::mem_fun(*this, &TraceDialogImpl::setDefaults));

    // attempt at making UI responsive: relocate preview to the right or bottom of dialog depending on dialog size
    bin.connectBeforeResize([this] (int width, int height, int baseline) {
        // skip bogus sizes
        if (width >= 10 && height >= 10) {
            // ratio: is dialog wide or is it tall?
            double const ratio = width / static_cast<double>(height);
            // g_warning("size alloc: %d x %d - %f", alloc.get_width(), alloc.get_height(), ratio);
            constexpr double hysteresis = 0.01;
            if (ratio < 1 - hysteresis) {
                // narrow/tall
                choice_tab.set_valign(Gtk::Align::START);
                orient_box.set_orientation(Gtk::Orientation::VERTICAL);
            } else if (ratio > 1 + hysteresis) {
                // wide/short
                orient_box.set_orientation(Gtk::Orientation::HORIZONTAL);
                choice_tab.set_valign(Gtk::Align::FILL);
            }
        }
    });

    CBT_SS.property_selected().signal_changed().connect([this] { adjustParamsVisible(); });
    adjustParamsVisible();

    // Palette editor signals
    B_palette_add.signal_clicked().connect([this] {
        // Find the best next color using constrained k-means on the image.
        auto desktop = getDesktop();
        if (desktop) {
            auto selection = desktop->getSelection();
            if (selection) {
                auto item = selection->singleItem();
                auto img = cast<SPImage>(item);
                if (img && img->pixbuf) {
                    auto copy = Inkscape::Pixbuf(*img->pixbuf);
                    auto gdkpixbuf = Glib::wrap(copy.getPixbufRaw(), true);
                    auto rgbmap = Trace::gdkPixbufToRgbMap(gdkpixbuf);
                    auto currentPalette = getCustomPalette();
                    auto next = Trace::findNextPaletteColor(rgbmap, currentPalette);
                    Gdk::RGBA color;
                    color.set_red(next.r / 255.0);
                    color.set_green(next.g / 255.0);
                    color.set_blue(next.b / 255.0);
                    color.set_alpha(1.0);
                    addPaletteColor(color);
                    schedulePreviewUpdate(200, true);
                    return;
                }
            }
        }
        // Fallback: add white if no image is selected.
        Gdk::RGBA white;
        white.set_red(1.0); white.set_green(1.0); white.set_blue(1.0); white.set_alpha(1.0);
        addPaletteColor(white);
        schedulePreviewUpdate(200, true);
    });
    B_palette_reset.signal_clicked().connect([this] { extractPaletteFromImage(); });
    CBT_MS.property_selected().signal_changed().connect([this] {
        updatePaletteVisibility();
        // Auto-extract palette when switching to Colors mode for the first time.
        if (isCustomPaletteMode(CBT_MS.get_selected()) && palette_rows.empty()) {
            extractPaletteFromImage();
        }
    });
    updatePaletteVisibility();

    // watch for changes, but only in params that can impact preview bitmap
    for (auto adj : {SS_BC_T, SS_ED_T, SS_CQ_T, SS_AT_FI_T, SS_AT_ET_T, /* optimize, smooth, speckles,*/ MS_scans, PA_curves, PA_islands, PA_sparse1, PA_sparse2 }) {
        adj->signal_value_changed().connect([this] { updatePreview(); });
    }
    for (auto checkbtn : {&CB_invert, &CB_MS_rb, /* CB_MS_smooth, CB_MS_stack, CB_optimize1, CB_optimize, */ &CB_PA_optimize, &CB_SIOX1, &CB_SIOX, /* CB_smooth1, CB_smooth, CB_speckles1, CB_speckles, */ &_live_preview}) {
        checkbtn->signal_toggled().connect([this] { updatePreview(); });
    }
    for (auto combo : {&CBT_SS, &CBT_MS}) {
        combo->property_selected().signal_changed().connect([this] { updatePreview(); });
    }
    _page_switched = choice_tab.signal_switch_page().connect([this] (Gtk::Widget *, unsigned) { updatePreview(); });

    auto focus = Gtk::EventControllerFocus::create();
    focus->set_propagation_phase(Gtk::PropagationPhase::BUBBLE);
    add_controller(focus);
    focus->signal_enter().connect([this] { updatePreview(); });
}

TraceDialogImpl::~TraceDialogImpl()
{
    Inkscape::Preferences* prefs = Inkscape::Preferences::get();
    prefs->setBool(getPrefsPath() + "liveUpdate", _live_preview.get_active());
    preview_timeout_conn.disconnect();
}

bool TraceDialogImpl::previewsEnabled() const
{
    return _live_preview.get_active() && is_widget_effectively_visible(this);
}

void TraceDialogImpl::schedulePreviewUpdate(int msecs, bool force)
{
    if (!previewsEnabled() && !force) {
        return;
    }

    // Restart timeout.
    preview_timeout_conn.disconnect();
    preview_timeout_conn = Glib::signal_timeout().connect([this] {
        updatePreview(true);
        return false;
    }, msecs);
}

void TraceDialogImpl::updatePreview(bool force)
{
    if (!previewsEnabled() && !force) {
        return;
    }

    preview_timeout_conn.disconnect();

    if (preview_future) {
        // Preview generation already running - flag for recomputation when finished.
        preview_pending_recompute = true;
        return;
    }

    preview_pending_recompute = false;

    auto data = getTraceData();
    preview_future = Trace::preview(std::move(data.engine), data.sioxEnabled,
        // On completion:
        [this] (Glib::RefPtr<Gdk::Pixbuf> result) {
            previewArea.set_paintable(Gdk::Texture::create_for_pixbuf(result));
            preview_future.cancel();

            // Recompute if invalidated during computation.
            if (preview_pending_recompute) {
                updatePreview();
            }
        }
    );

    if (!preview_future) {
        // On instant failure:
        previewArea.set_paintable({});
    }
}

void TraceDialogImpl::addPaletteColor(Gdk::RGBA const &color)
{
    int index = palette_rows.size();

    auto row = Gtk::make_managed<PaletteRowWidget>();
    row->set_color(color);

    // Click on the color swatch: pick from canvas with eyedropper.
    auto swatch_click = Gtk::GestureClick::create();
    swatch_click->set_button(GDK_BUTTON_PRIMARY);
    swatch_click->signal_released().connect([this, index] (int, double, double) {
        pickColorForSwatch(index);
    });
    row->swatch.add_controller(swatch_click);

    // Click on the hex label: toggle inline color picker.
    auto label_click = Gtk::GestureClick::create();
    label_click->set_button(GDK_BUTTON_PRIMARY);
    label_click->signal_released().connect([this, index] (int, double, double) {
        toggleColorPanel(index);
    });
    row->hex_label.add_controller(label_click);

    // Delete button: remove this row.
    row->on_delete = [this, index] { removePaletteColor(index); };
    row->delete_btn.signal_clicked().connect([row] { if (row->on_delete) row->on_delete(); });

    // Color changed (from ColorSet via panel): update preview.
    row->on_color_changed = [this] { schedulePreviewUpdate(500); };

    palette_list.append(*row);
    palette_rows.push_back(row);
    updatePaletteCountLabel();
}

void TraceDialogImpl::removePaletteColor(int index)
{
    if (index < 0 || index >= (int)palette_rows.size()) return;

    // Close the color panel if it's showing for this or a later row.
    if (color_panel && active_panel_index >= index) {
        palette_list.remove(*color_panel);
        color_panel.reset();
        active_panel_index = -1;
    }

    auto *row = palette_rows[index];
    palette_list.remove(*row);
    palette_rows.erase(palette_rows.begin() + index);

    // Re-bind indices for remaining rows (closures capture index by value).
    for (int i = index; i < (int)palette_rows.size(); i++) {
        palette_rows[i]->on_delete = [this, i] { removePaletteColor(i); };
    }

    updatePaletteCountLabel();
    schedulePreviewUpdate(200, true);
}

void TraceDialogImpl::clearPalette()
{
    // Close any open color panel first.
    if (color_panel) {
        palette_list.remove(*color_panel);
        color_panel.reset();
        active_panel_index = -1;
    }
    while (!palette_rows.empty()) {
        auto *row = palette_rows.back();
        palette_rows.pop_back();
        palette_list.remove(*row);
    }
    updatePaletteCountLabel();
}

std::vector<Trace::RGB> TraceDialogImpl::getCustomPalette() const
{
    std::vector<Trace::RGB> palette;
    palette.reserve(palette_rows.size());
    for (auto *row : palette_rows) {
        auto c = row->get_color();
        palette.push_back({
            static_cast<unsigned char>(c.get_red()   * 255),
            static_cast<unsigned char>(c.get_green() * 255),
            static_cast<unsigned char>(c.get_blue()  * 255)
        });
    }
    return palette;
}

void TraceDialogImpl::pickColorForSwatch(int index)
{
    if (index < 0 || index >= (int)palette_rows.size()) return;

    auto desktop = getDesktop();
    if (!desktop) return;

    _dropper_connection.disconnect();

    Tools::sp_toggle_dropper(desktop);
    if (auto tool = dynamic_cast<Tools::DropperTool*>(desktop->getTool())) {
        _dropper_connection = tool->onetimepick_signal.connect([this, index] (Colors::Color const &picked) {
            if (index >= (int)palette_rows.size()) return;
            uint32_t rgba = picked.toRGBA();
            Gdk::RGBA gdk_color;
            gdk_color.set_red(  ((rgba >> 24) & 0xFF) / 255.0);
            gdk_color.set_green(((rgba >> 16) & 0xFF) / 255.0);
            gdk_color.set_blue( ((rgba >>  8) & 0xFF) / 255.0);
            gdk_color.set_alpha(1.0);
            palette_rows[index]->set_color(gdk_color);
            schedulePreviewUpdate(200, true);
        });
    }
}

void TraceDialogImpl::extractPaletteFromImage()
{
    auto desktop = getDesktop();
    if (!desktop) return;

    auto selection = desktop->getSelection();
    if (!selection) return;

    auto item = selection->singleItem();
    auto img = cast<SPImage>(item);
    if (!img || !img->pixbuf) return;

    // Clear the cache entry so the fresh extraction replaces any user edits.
    if (auto id = img->getId()) {
        _palette_cache.erase(id);
        _current_image_id = id;
    }

    // Make a non-const copy to allow pixel format conversion (Cairo -> GDK).
    auto copy = Inkscape::Pixbuf(*img->pixbuf);
    auto gdkpixbuf = Glib::wrap(copy.getPixbufRaw(), true);
    auto rgbmap = Trace::gdkPixbufToRgbMap(gdkpixbuf);

    // Automatically determine optimal number of colors.
    auto prefs = Preferences::get();
    int maxColors = prefs->getIntLimited(getPrefsPath() + "maxBitmapTracingAutoPaletteColors", 10, 2, 50);
    auto palette = Trace::estimateOptimalPalette(rgbmap, maxColors);

    // Clear existing palette.
    clearPalette();

    // Add extracted colors.
    for (auto const &rgb : palette) {
        Gdk::RGBA color;
        color.set_red(rgb.r / 255.0);
        color.set_green(rgb.g / 255.0);
        color.set_blue(rgb.b / 255.0);
        color.set_alpha(1.0);
        addPaletteColor(color);
    }

    schedulePreviewUpdate(200, true);
}

void TraceDialogImpl::savePaletteToCache()
{
    if (_current_image_id.empty()) return;
    auto palette = getCustomPalette();
    if (!palette.empty()) {
        _palette_cache[_current_image_id] = std::move(palette);
    }
}

void TraceDialogImpl::restorePaletteFromCache(std::string const &image_id)
{
    auto it = _palette_cache.find(image_id);
    if (it == _palette_cache.end()) return;

    clearPalette();
    for (auto const &rgb : it->second) {
        Gdk::RGBA color;
        color.set_red(rgb.r / 255.0);
        color.set_green(rgb.g / 255.0);
        color.set_blue(rgb.b / 255.0);
        color.set_alpha(1.0);
        addPaletteColor(color);
    }
    schedulePreviewUpdate(200, true);
}

void TraceDialogImpl::toggleColorPanel(int index)
{
    if (index < 0 || index >= (int)palette_rows.size()) return;

    // If the panel is already showing for this row, close it.
    if (active_panel_index == index && color_panel) {
        palette_rows[index]->set_expanded(false);
        palette_list.remove(*color_panel);
        color_panel.reset();
        active_panel_index = -1;
        return;
    }

    // Remove the panel from its current position if open.
    if (color_panel) {
        if (active_panel_index >= 0 && active_panel_index < (int)palette_rows.size()) {
            palette_rows[active_panel_index]->set_expanded(false);
        }
        palette_list.remove(*color_panel);
        color_panel.reset();
    }

    // Create a new panel bound to this row's ColorSet.
    auto *row = palette_rows[index];
    color_panel = UI::Widget::ColorPickerPanel::create(
        Colors::Space::Type::HSL,
        UI::Widget::ColorPickerPanel::None,
        row->color_set
    );
    color_panel->set_margin_start(36); // indent under the swatch

    // Insert the panel right after the target row.
    // Gtk::Box::insert_child_after places the child after the reference widget.
    palette_list.insert_child_after(*color_panel, *row);
    row->set_expanded(true);
    active_panel_index = index;
}

void TraceDialogImpl::updatePaletteCountLabel()
{
    int n = palette_rows.size();
    L_palette_count.set_text(std::to_string(n) + (n == 1 ? " color" : " colors"));
}

void TraceDialogImpl::updatePaletteVisibility()
{
    bool custom = isCustomPaletteMode(CBT_MS.get_selected());
    palette_box.set_visible(custom);

    // Hide the scans row (row 2 widgets) when in custom palette mode.
    // The scans row has widgets at columns 0-3, row 2 in the grid.
    auto grid = dynamic_cast<Gtk::Grid*>(palette_box.get_parent());
    if (grid) {
        for (int col = 0; col < 4; col++) {
            if (auto widget = grid->get_child_at(col, 2)) {
                widget->set_visible(!custom);
            }
        }
    }
}

void TraceDialogImpl::adjustParamsVisible()
{
    int constexpr start_row = 2;
    int option = CBT_SS.get_selected();
    if (option >= 3) option = 3;
    int show1 = start_row + option;
    int show2 = show1;
    if (option == 3) ++show2;

    for (int row = start_row; row < start_row + 5; ++row) {
        for (int col = 0; col < 4; ++col) {
            if (auto widget = _param_grid.get_child_at(col, row)) {
                if (row == show1 || row == show2) {
                    widget->set_visible(true);
                } else {
                    widget->set_visible(false);
                }
            }
        }
    }
}

} // namespace

std::unique_ptr<TraceDialog> TraceDialog::create()
{
    return std::make_unique<TraceDialogImpl>();
}

} // namespace Inkscape::UI::Dialog

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
