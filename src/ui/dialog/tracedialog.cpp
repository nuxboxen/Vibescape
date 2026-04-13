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

#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/eventcontrollerfocus.h>
#include <gtkmm/flowbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/grid.h>
#include <gtkmm/picture.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/snapshot.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/stack.h>

#include "colors/color.h"
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

/// A simple colored rectangle widget using GtkSnapshot (no Cairo).
class PaletteSwatchWidget : public Gtk::Widget
{
public:
    PaletteSwatchWidget()
    {
        set_size_request(24, 24);
    }

    void set_color(Gdk::RGBA const &c) { color = c; queue_draw(); }
    Gdk::RGBA get_color() const { return color; }

protected:
    void snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot) override
    {
        int w = get_width();
        int h = get_height();
        if (w <= 0 || h <= 0) return;

        // Fill with the swatch color.
        auto fill = GdkRGBA{
            static_cast<float>(color.get_red()),
            static_cast<float>(color.get_green()),
            static_cast<float>(color.get_blue()),
            1.0f
        };
        auto rect = GRAPHENE_RECT_INIT(0, 0, static_cast<float>(w), static_cast<float>(h));
        gtk_snapshot_append_color(snapshot->gobj(), &fill, &rect);

        // Draw a 1px border.
        auto border_color = GdkRGBA{0.3f, 0.3f, 0.3f, 1.0f};
        auto top    = GRAPHENE_RECT_INIT(0, 0, static_cast<float>(w), 1);
        auto bottom = GRAPHENE_RECT_INIT(0, static_cast<float>(h - 1), static_cast<float>(w), 1);
        auto left   = GRAPHENE_RECT_INIT(0, 0, 1, static_cast<float>(h));
        auto right  = GRAPHENE_RECT_INIT(static_cast<float>(w - 1), 0, 1, static_cast<float>(h));
        gtk_snapshot_append_color(snapshot->gobj(), &border_color, &top);
        gtk_snapshot_append_color(snapshot->gobj(), &border_color, &bottom);
        gtk_snapshot_append_color(snapshot->gobj(), &border_color, &left);
        gtk_snapshot_append_color(snapshot->gobj(), &border_color, &right);
    }

private:
    Gdk::RGBA color;
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
    Gtk::FlowBox &palette_flowbox;
    Gtk::Button &B_palette_add, &B_palette_remove, &B_palette_extract;
    Gtk::SpinButton &palette_extract_spin;
    Glib::RefPtr<Gtk::Adjustment> palette_extract_count;
    std::vector<PaletteSwatchWidget*> palette_swatches;
    sigc::scoped_connection _dropper_connection;

    void addPaletteColor(Gdk::RGBA const &color);
    void removeLastPaletteColor();
    void extractPaletteFromImage();
    void updatePaletteVisibility();
    void pickColorForSwatch(int index);
    std::vector<Trace::RGB> getCustomPalette() const;

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
  , palette_flowbox(get_widget<Gtk::FlowBox>     (builder,    "palette_flowbox"))
  , B_palette_add  (get_widget<Gtk::Button>      (builder,     "B_palette_add"))
  , B_palette_remove(get_widget<Gtk::Button>     (builder,  "B_palette_remove"))
  , B_palette_extract(get_widget<Gtk::Button>    (builder, "B_palette_extract"))
  , palette_extract_spin(get_widget<Gtk::SpinButton>(builder, "palette_extract_spin"))
  , palette_extract_count(get_object<Gtk::Adjustment>(builder, "palette_extract_count"))
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
        Gdk::RGBA white;
        white.set_red(1.0); white.set_green(1.0); white.set_blue(1.0); white.set_alpha(1.0);
        addPaletteColor(white);
        schedulePreviewUpdate(200, true);
    });
    B_palette_remove.signal_clicked().connect([this] { removeLastPaletteColor(); });
    B_palette_extract.signal_clicked().connect([this] { extractPaletteFromImage(); });
    CBT_MS.property_selected().signal_changed().connect([this] { updatePaletteVisibility(); });
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
    int index = palette_swatches.size();

    auto swatch = Gtk::make_managed<PaletteSwatchWidget>();
    swatch->set_color(color);

    // Left-click: pick color from canvas with eyedropper.
    auto click = Gtk::GestureClick::create();
    click->set_button(GDK_BUTTON_PRIMARY);
    click->signal_released().connect([this, index] (int, double, double) {
        pickColorForSwatch(index);
    });
    swatch->add_controller(click);

    swatch->set_tooltip_text("Click to pick a color from the canvas");

    palette_flowbox.append(*swatch);
    palette_swatches.push_back(swatch);
}

void TraceDialogImpl::removeLastPaletteColor()
{
    if (palette_swatches.empty()) return;
    auto *swatch = palette_swatches.back();
    palette_swatches.pop_back();
    palette_flowbox.remove(*swatch);
    schedulePreviewUpdate(500);
}

std::vector<Trace::RGB> TraceDialogImpl::getCustomPalette() const
{
    std::vector<Trace::RGB> palette;
    palette.reserve(palette_swatches.size());
    for (auto *swatch : palette_swatches) {
        auto c = swatch->get_color();
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
    if (index < 0 || index >= (int)palette_swatches.size()) return;

    auto desktop = getDesktop();
    if (!desktop) return;

    // Disconnect any previous pick-in-progress.
    _dropper_connection.disconnect();

    // Activate the dropper tool in one-time pick mode.
    Tools::sp_toggle_dropper(desktop);
    if (auto tool = dynamic_cast<Tools::DropperTool*>(desktop->getTool())) {
        _dropper_connection = tool->onetimepick_signal.connect([this, index] (Colors::Color const &picked) {
            if (index >= (int)palette_swatches.size()) return;
            uint32_t rgba = picked.toRGBA();
            Gdk::RGBA gdk_color;
            gdk_color.set_red(  ((rgba >> 24) & 0xFF) / 255.0);
            gdk_color.set_green(((rgba >> 16) & 0xFF) / 255.0);
            gdk_color.set_blue( ((rgba >>  8) & 0xFF) / 255.0);
            gdk_color.set_alpha(1.0);
            palette_swatches[index]->set_color(gdk_color);
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

    // Make a non-const copy to allow pixel format conversion (Cairo -> GDK).
    auto copy = Inkscape::Pixbuf(*img->pixbuf);
    auto gdkpixbuf = Glib::wrap(copy.getPixbufRaw(), true);
    auto rgbmap = Trace::gdkPixbufToRgbMap(gdkpixbuf);

    int ncolors = static_cast<int>(palette_extract_count->get_value());
    auto imap = Trace::rgbMapQuantizePerceptual(rgbmap, ncolors);

    // Clear existing palette.
    while (!palette_swatches.empty()) {
        removeLastPaletteColor();
    }

    // Add extracted colors.
    for (int i = 0; i < imap.nrColors; i++) {
        auto rgb = imap.clut[i];
        Gdk::RGBA color;
        color.set_red(rgb.r / 255.0);
        color.set_green(rgb.g / 255.0);
        color.set_blue(rgb.b / 255.0);
        color.set_alpha(1.0);
        addPaletteColor(color);
    }

    schedulePreviewUpdate(200, true);
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
