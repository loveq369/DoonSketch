/**
 * Glyph selector dialog.
 */

/* Authors:
 *   Jon A. Cruz
 *
 * Copyright (C) 2010 Jon A. Cruz
 * Released under GNU GPL, read the file 'COPYING' for more information
 */
#ifndef SEEN_DIALOG_DESKTOP_TRACKER
#define SEEN_DIALOG_DESKTOP_TRACKER

#include <stddef.h>
#include <sigc++/connection.h>
#include <glib.h>

typedef struct _GtkWidget GtkWidget;
class SPDesktop;

namespace Inkscape {

class Application;

namespace UI {
namespace Dialog {

class DesktopTracker
{
public:
    DesktopTracker();
    virtual ~DesktopTracker();

    void connect(GtkWidget *widget);
    void disconnect();

    SPDesktop *getDesktop() const;

    void setBase(SPDesktop *desktop);
    SPDesktop *getBase() const;

    sigc::connection connectDesktopChanged( const sigc::slot<void, SPDesktop*> & slot );

private:
    static gboolean activateDesktopCB(Inkscape::Application *inkscape, SPDesktop *desktop, DesktopTracker *self );
    static bool hierarchyChangeCB(GtkWidget *widget, GtkWidget* prev, DesktopTracker *self);

    void handleHierarchyChange();
    void setDesktop(SPDesktop *desktop);

    SPDesktop *base;
    SPDesktop *desktop;
    GtkWidget *widget;
    gulong hierID;
    gulong inkID;
    bool trackActive;
    sigc::signal<void, SPDesktop*> desktopChangedSig;
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // SEEN_DIALOG_DESKTOP_TRACKER
/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
