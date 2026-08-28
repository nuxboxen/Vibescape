// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::UI::Widget::PageSelector - page selector widget
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2004 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_WIDGETS_PAGE_SELECTOR
#define SEEN_INKSCAPE_WIDGETS_PAGE_SELECTOR

#include <giomm/liststore.h>
#include <gtkmm/box.h>
#include <gtkmm/combobox.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/listitem.h>
#include <gtkmm/liststore.h>
#include <gtkmm/listview.h>

class SPDesktop;
class SPDocument;
class SPPage;

namespace Inkscape::UI::Widget {

class PageSelector : public Gtk::Box
{
public:
    PageSelector();
    ~PageSelector() override;

    void setDesktop(SPDesktop *desktop);

private:
    class PageModelColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        Gtk::TreeModelColumn<SPPage *> object;

        PageModelColumns() { add(object); }
    };

    class PageItem : public Glib::Object {
    public:
        static Glib::RefPtr<PageItem> create(SPPage* page)
        {
            return Glib::make_refptr_for_instance<PageItem>(new PageItem(page));
        }
        SPPage* getPage() const { return _page; }
    protected:
        PageItem(SPPage* page)
            : Glib::ObjectBase(typeid(PageItem))
            , _page(page)
        {}

    private:
        SPPage* _page = nullptr;
    };

    SPDesktop *_desktop = nullptr;
    SPDocument *_document = nullptr;

    Gtk::ComboBox _selector;
    Gtk::DropDown _dropdown;
    Gtk::Button _prev_button;
    Gtk::Button _next_button;

    PageModelColumns _model_columns;
    Gtk::CellRendererText _label_renderer;
    Glib::RefPtr<Gtk::ListStore> _page_model;
    Glib::RefPtr<Gio::ListStore<PageItem>> _pageitem_liststore;

    sigc::connection _selector_changed_connection;
    sigc::connection _pages_changed_connection;
    sigc::connection _page_selected_connection;
    sigc::connection _doc_replaced_connection;

    void setDocument(SPDocument *document);
    void pagesChanged(SPPage *new_page);
    void selectonChanged(SPPage *page);

    void renderPageLabel(Gtk::TreeModel::const_iterator const &row);
    void setSelectedPage();
    void nextPage();
    void prevPage();

    void setupPageItemCB(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bindPageItemCB( const Glib::RefPtr<Gtk::ListItem>& list_item);
    void onDropDownChangedCB();
};

} // namespace Inkscape::UI::Widget

#endif
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
