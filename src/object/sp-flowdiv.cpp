// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
/*
 */

#include "sp-flowdiv.h"

#include "sp-string.h"
#include "xml/document.h"

SPFlowdiv::SPFlowdiv() = default;

SPFlowdiv::~SPFlowdiv() = default;

void SPFlowdiv::update(SPCtx *ctx, unsigned int flags) {
    SPItemCtx *ictx = reinterpret_cast<SPItemCtx *>(ctx);
    SPItemCtx cctx = *ictx;

    unsigned childflags = flags;
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        childflags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }
    childflags &= SP_OBJECT_MODIFIED_CASCADE;

    std::vector<SPObject *> l;
    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for(auto child:l) {
        if (childflags || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            if (is<SPItem>(child)) {
                SPItem const &chi = *cast<SPItem>(child);
                cctx.i2doc = chi.transform * ictx->i2doc;
                cctx.i2vp = chi.transform * ictx->i2vp;
                child->updateDisplay((SPCtx *)&cctx, childflags);
            } else {
                child->updateDisplay(ctx, childflags);
            }
        }

        sp_object_unref(child);
    }

    Base::update(ctx, flags);
}

void SPFlowdiv::modified(unsigned int flags) {
    Base::modified(flags);

    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;

    std::vector<SPObject *> l;
    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for(auto child:l) {
        if ((flags & SP_OBJECT_FLAGS_ALL) || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }
        sp_object_unref(child);
    }
}


void SPFlowdiv::build(SPDocument *doc, Inkscape::XML::Node *repr) {
	this->_requireSVGVersion(Inkscape::Version(1, 2));

    Base::build(doc, repr);
}


Inkscape::XML::Node* SPFlowdiv::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ( flags & SP_OBJECT_WRITE_BUILD ) {
        if ( repr == nullptr ) {
            repr = xml_doc->createElement("svg:flowDiv");
        }

        std::vector<Inkscape::XML::Node *> l;

        for (auto& child: children) {
            Inkscape::XML::Node* c_repr = nullptr;

            if ( is<SPFlowtspan>(&child) ) {
                c_repr = child.updateRepr(xml_doc, nullptr, flags);
            } else if ( is<SPFlowpara>(&child) ) {
                c_repr = child.updateRepr(xml_doc, nullptr, flags);
            } else if ( is<SPString>(&child) ) {
                c_repr = xml_doc->createTextNode(cast<SPString>(&child)->string.c_str());
            }

            if ( c_repr ) {
                l.push_back(c_repr);
            }
        }
        for (auto i=l.rbegin();i!=l.rend();++i) {
            repr->addChild(*i, nullptr);
            Inkscape::GC::release(*i);
        }
    } else {
        for (auto& child: children) {
            if ( is<SPFlowtspan>(&child) ) {
                child.updateRepr(flags);
            } else if ( is<SPFlowpara>(&child) ) {
                child.updateRepr(flags);
            } else if ( is<SPString>(&child) ) {
                child.getRepr()->setContent(cast<SPString>(&child)->string.c_str());
            }
        }
    }

    Base::write(xml_doc, repr, flags);

    return repr;
}


/*
 *
 */

SPFlowtspan::SPFlowtspan() = default;

SPFlowtspan::~SPFlowtspan() = default;

void SPFlowtspan::update(SPCtx *ctx, unsigned int flags) {
    SPItemCtx *ictx = reinterpret_cast<SPItemCtx *>(ctx);
    SPItemCtx cctx = *ictx;

    unsigned childflags = flags;
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        childflags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }
    childflags &= SP_OBJECT_MODIFIED_CASCADE;
    std::vector<SPObject *> l;
    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for(auto child:l) {
        if (childflags || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            if (is<SPItem>(child)) {
                SPItem const &chi = *cast<SPItem>(child);
                cctx.i2doc = chi.transform * ictx->i2doc;
                cctx.i2vp = chi.transform * ictx->i2vp;
                child->updateDisplay((SPCtx *)&cctx, childflags);
            } else {
                child->updateDisplay(ctx, childflags);
            }
        }

        sp_object_unref(child);
    }

    Base::update(ctx, flags);
}

void SPFlowtspan::modified(unsigned int flags) {
    Base::modified(flags);

    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;
    std::vector<SPObject *> l;
    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for(auto child:l) {
        if ((flags & SP_OBJECT_FLAGS_ALL) || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }
        sp_object_unref(child);
    }
}

Inkscape::XML::Node *SPFlowtspan::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ( flags&SP_OBJECT_WRITE_BUILD ) {
        if ( repr == nullptr ) {
            repr = xml_doc->createElement("svg:flowSpan");
        }

        std::vector<Inkscape::XML::Node *> l;

        for (auto& child: children) {
            Inkscape::XML::Node* c_repr = nullptr;

            if ( is<SPFlowtspan>(&child) ) {
                c_repr = child.updateRepr(xml_doc, nullptr, flags);
            } else if ( is<SPFlowpara>(&child) ) {
                c_repr = child.updateRepr(xml_doc, nullptr, flags);
            } else if ( is<SPString>(&child) ) {
                c_repr = xml_doc->createTextNode(cast<SPString>(&child)->string.c_str());
            }

            if ( c_repr ) {
                l.push_back(c_repr);
            }
        }
        for (auto i=l.rbegin();i!=l.rend();++i) {
            repr->addChild(*i, nullptr);
            Inkscape::GC::release(*i);
        }
    } else {
        for (auto& child: children) {
            if ( is<SPFlowtspan>(&child) ) {
                child.updateRepr(flags);
            } else if ( is<SPFlowpara>(&child) ) {
                child.updateRepr(flags);
            } else if ( is<SPString>(&child) ) {
                child.getRepr()->setContent(cast<SPString>(&child)->string.c_str());
            }
        }
    }

    Base::write(xml_doc, repr, flags);

    return repr;
}


/*
 *
 */
SPFlowpara::SPFlowpara() = default;

SPFlowpara::~SPFlowpara() = default;

void SPFlowpara::update(SPCtx *ctx, unsigned int flags) {
    SPItemCtx *ictx = reinterpret_cast<SPItemCtx *>(ctx);
    SPItemCtx cctx = *ictx;

    Base::update(ctx, flags);

    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;

    std::vector<SPObject *> l;
    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for(auto child:l) {
        if ((flags & SP_OBJECT_FLAGS_ALL) || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            if (is<SPItem>(child)) {
                SPItem const &chi = *cast<SPItem>(child);
                cctx.i2doc = chi.transform * ictx->i2doc;
                cctx.i2vp = chi.transform * ictx->i2vp;
                child->updateDisplay((SPCtx *)&cctx, flags);
            } else {
                child->updateDisplay(ctx, flags);
            }
        }
        sp_object_unref(child);
    }
}

void SPFlowpara::modified(unsigned int flags) {
    Base::modified(flags);

    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;

    std::vector<SPObject *> l;
    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for(auto child:l) {
        if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }
        sp_object_unref(child);
    }
}

Inkscape::XML::Node *SPFlowpara::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ( flags&SP_OBJECT_WRITE_BUILD ) {
        if ( repr == nullptr ) {
        	repr = xml_doc->createElement("svg:flowPara");
        }

        std::vector<Inkscape::XML::Node *> l;

        for (auto& child: children) {
            Inkscape::XML::Node* c_repr = nullptr;

            if ( is<SPFlowtspan>(&child) ) {
                c_repr = child.updateRepr(xml_doc, nullptr, flags);
            } else if ( is<SPFlowpara>(&child) ) {
                c_repr = child.updateRepr(xml_doc, nullptr, flags);
            } else if ( is<SPString>(&child) ) {
                c_repr = xml_doc->createTextNode(cast<SPString>(&child)->string.c_str());
            }

            if ( c_repr ) {
                l.push_back(c_repr);
            }
        }

        for (auto i=l.rbegin();i!=l.rend();++i) {
            repr->addChild(*i, nullptr);
            Inkscape::GC::release(*i);
        }
    } else {
        for (auto& child: children) {
            if ( is<SPFlowtspan>(&child) ) {
                child.updateRepr(flags);
            } else if ( is<SPFlowpara>(&child) ) {
                child.updateRepr(flags);
            } else if ( is<SPString>(&child) ) {
                child.getRepr()->setContent(cast<SPString>(&child)->string.c_str());
            }
        }
    }

    Base::write(xml_doc, repr, flags);

    return repr;
}


/*
 *
 */

SPFlowline::SPFlowline() = default;

SPFlowline::~SPFlowline() = default;

Inkscape::XML::Node *SPFlowline::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ( flags & SP_OBJECT_WRITE_BUILD ) {
        if ( repr == nullptr ) {
            repr = xml_doc->createElement("svg:flowLine");
        }
    }

    SPObject::write(xml_doc, repr, flags);

    return repr;
}


/*
 *
 */

SPFlowregionbreak::SPFlowregionbreak() = default;

SPFlowregionbreak::~SPFlowregionbreak() = default;

Inkscape::XML::Node *SPFlowregionbreak::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ( flags & SP_OBJECT_WRITE_BUILD ) {
        if ( repr == nullptr ) {
            repr = xml_doc->createElement("svg:flowLine");
        }
    }

    SPObject::write(xml_doc, repr, flags);

    return repr;
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
