// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Document properties dialog, Gtkmm-style.
 */
/* Authors:
 *   bulia byak <buliabyak@users.sf.net>
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon Phillips <jon@rejon.org>
 *   Ralf Stephan <ralf@ark.in-berlin.de> (Gtkmm)
 *   Diederik van Lierop <mail@diedenrezi.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006-2008 Johan Engelen  <johan@shouraizou.nl>
 * Copyright (C) 2000 - 2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_DOCUMENT_PREFERENCES_H
#define INKSCAPE_UI_DIALOG_DOCUMENT_PREFERENCES_H

#ifdef HAVE_CONFIG_H
#include "config.h" // only include where actually required!
#endif

#include <gtkmm/listbox.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/combobox.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include "object/sp-grid.h"
#include "ui/dialog/dialog-base.h"
#include "ui/widget/generic/popover-bin.h"
#include "ui/widget/generic/icon-combobox.h"
#include "ui/widget/licensor.h"
#include "ui/widget/registered-widget.h"
#include "xml/helper-observer.h"


namespace Glib {
class ustring;
} // namespace Glib

namespace Gtk {
class ListStore;
} // namespace gtk

namespace Inkscape {

namespace XML { class Node; }

namespace UI {

namespace Widget {
class EntityEntry;
class NotebookPage;
class PageProperties;
class GuidesPanel;
class ColorSystemPanel;
class ScriptingPanel;
class MetadataPanel;
} // namespace Widget

namespace Dialog {

class DocumentProperties : public DialogBase
{
public:
    DocumentProperties();
    ~DocumentProperties() override;

    void  update_widgets();
    static DocumentProperties &getInstance();
    static void destroy();

    void documentReplaced() override;

    void update() override;
    void rebuild_gridspage();

private:
    void  build_page();
    void  build_grid();
    void  build_guides();
    void  build_snap();
    void  build_gridspage();
    void  build_cms();
    void  build_scripting();
    void  build_metadata();
    void add_grid_widget(SPGrid *grid);
    void remove_grid_widget(XML::Node &node);
    void update_grid_placeholder();
    virtual void  on_response (int);
    void  load_default_metadata();
    void  save_default_metadata();
    void update_viewbox(SPDesktop* desktop);
    void update_scale_ui(SPDesktop* desktop);
    void update_viewbox_ui(SPDesktop* desktop);
    void set_content_scale(SPDesktop *desktop, double scale_x);
    void set_document_scale(SPDesktop* desktop, double scale_x);
    void set_viewbox_pos(SPDesktop* desktop, double x, double y);
    void set_viewbox_size(SPDesktop* desktop, double width, double height);

    UI::Widget::PopoverBin _popoverbin;
    Gtk::Notebook _notebook;

    UI::Widget::NotebookPage   *_page_page;
    UI::Widget::GuidesPanel* _guides_panel;
    UI::Widget::ColorSystemPanel* _cms_panel;
    UI::Widget::ScriptingPanel* _scripting_panel;
    UI::Widget::MetadataPanel* _metadata_panel;
    UI::Widget::NotebookPage  *_page_metadata2;

    Gtk::Box      _grids_vbox;

    UI::Widget::Registry _wr;
    //---------------------------------------------------------------
    UI::Widget::PageProperties* _page;
    //---------------------------------------------------------------
    Gtk::ScrolledWindow _grids_wnd;
    Gtk::ListBox _grids_list;
    Glib::RefPtr<Gtk::SizeGroup> _grids_unified_size = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL);
    Gtk::Label _no_grids;
    Gtk::Box   _grids_hbox_crea;
    Gtk::Label _grids_label_def;
    sigc::scoped_connection _on_idle_scroll;
    Inkscape::UI::Widget::IconComboBox _grid_type;
    //---------------------------------------------------------------
    UI::Widget::Licensor _licensor;

    Gtk::Box& _createPageTabLabel(const Glib::ustring& label, const char *label_image);

private:
    // callback methods for buttons on grids page.
    void onNewGrid(GridType type);

    // callback for display unit change
    void display_unit_change(const Inkscape::Util::Unit* unit);

    class WatchConnection : private XML::NodeObserver
    {
    public:
        WatchConnection(DocumentProperties *dialog)
            : _dialog(dialog)
        {}

        ~WatchConnection() override { disconnect(); }

        void connect(Inkscape::XML::Node *node);
        void disconnect();

    private:
        void notifyChildAdded(XML::Node &node, XML::Node &child, XML::Node *prev) final;
        void notifyChildRemoved(XML::Node &node, XML::Node &child, XML::Node *prev) final;
        void notifyAttributeChanged(XML::Node &node, GQuark name, Util::ptr_shared old_value,
                                    Util::ptr_shared new_value) final;

        Inkscape::XML::Node *_node{nullptr};
        DocumentProperties *_dialog;
    };

    // nodes connected to listeners
    WatchConnection _namedview_connection;
    WatchConnection _root_connection;
};

} // namespace Dialog

} // namespace UI

} // namespace Inkscape

#endif // INKSCAPE_UI_DIALOG_DOCUMENT_PREFERENCES_H

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
