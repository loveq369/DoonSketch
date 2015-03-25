
#include <cstring>
#include <string>
#include <limits>

#include "display/curve.h"
#include "libnr/nr-matrix-fns.h"
#include "xml/repr.h"
#include "sp-conn-end.h"
#include "sp-path.h"
#include "uri.h"
#include "document.h"
#include "sp-item-group.h"
#include "2geom/path.h"
#include "2geom/pathvector.h"
#include "2geom/path-intersection.h"


static void change_endpts(SPCurve *const curve, double const endPos[2]);

SPConnEnd::SPConnEnd(SPObject *const owner) :
    ref(owner),
    href(NULL),
    // Default to center connection endpoint
    type(ConnPointDefault),
    id(4),
    _changed_connection(),
    _delete_connection(),
    _transformed_connection()
{
}

static SPObject const *
get_nearest_common_ancestor(SPObject const *const obj, SPItem const *const objs[2]) {
    SPObject const *anc_sofar = obj;
    for (unsigned i = 0; i < 2; ++i) {
        if ( objs[i] != NULL ) {
            anc_sofar = anc_sofar->nearestCommonAncestor(objs[i]);
        }
    }
    return anc_sofar;
}


static bool try_get_intersect_point_with_item_recursive(Geom::PathVector& conn_pv, SPItem* item,
        const Geom::Matrix& item_transform, double& intersect_pos) {

    double initial_pos = intersect_pos;
    // if this is a group...
    if (SP_IS_GROUP(item)) {
        SPGroup* group = SP_GROUP(item);

        // consider all first-order children
        double child_pos = 0.0;
        for (GSList const* i = sp_item_group_item_list(group); i != NULL; i = i->next) {
            SPItem* child_item = SP_ITEM(i->data);
            try_get_intersect_point_with_item_recursive(conn_pv, child_item,
                    item_transform * child_item->transform, child_pos);
            if (intersect_pos < child_pos)
                intersect_pos = child_pos;
        }
        return intersect_pos != initial_pos;
    }

    // if this is not a shape, nothing to be done
    if (!SP_IS_SHAPE(item)) return false;

    // make sure it has an associated curve
    SPCurve* item_curve = sp_shape_get_curve(SP_SHAPE(item));
    if (!item_curve) return false;

    // apply transformations (up to common ancestor)
    item_curve->transform(item_transform);

    const Geom::PathVector& curve_pv = item_curve->get_pathvector();
    Geom::CrossingSet cross = crossings(conn_pv, curve_pv);
    // iterate over all Crossings
    for (Geom::CrossingSet::const_iterator i = cross.begin(); i != cross.end(); i++) {
        const Geom::Crossings& cr = *i;

        for (Geom::Crossings::const_iterator i = cr.begin(); i != cr.end(); i++) {
            const Geom::Crossing& cr_pt = *i;
            if ( intersect_pos < cr_pt.ta)
                intersect_pos = cr_pt.ta;
        }
    }

    item_curve->unref();

    return intersect_pos != initial_pos;
}


// This function returns the outermost intersection point between the path (a connector)
// and the item given.  If the item is a group, then the component items are considered.
// The transforms given should be to a common ancestor of both the path and item.
//
static bool try_get_intersect_point_with_item(SPPath* conn, SPItem* item,
        const Geom::Matrix& item_transform, const Geom::Matrix& conn_transform,
        const bool at_start, double& intersect_pos) {

    // Copy the curve and apply transformations up to common ancestor.
    SPCurve* conn_curve = conn->curve->copy();
    conn_curve->transform(conn_transform);

    Geom::PathVector conn_pv = conn_curve->get_pathvector();

    // If this is not the starting point, use Geom::Path::reverse() to reverse the path
    if (!at_start)
    {
        // connectors are actually a single path, so consider the first element from a Geom::PathVector
        conn_pv[0] = conn_pv[0].reverse();
    }

    // We start with the intersection point at the beginning of the path
    intersect_pos = 0.0;

    // Find the intersection.
    bool result = try_get_intersect_point_with_item_recursive(conn_pv, item, item_transform, intersect_pos);

    if (!result)
        // No intersection point has been found (why?)
        // just default to connector end
        intersect_pos = 0;
    // If not at the starting point, recompute position with respect to original path
    if (!at_start)
        intersect_pos = conn_pv[0].size() - intersect_pos;
    // Free the curve copy.
    conn_curve->unref();

    return result;
}


static void
sp_conn_get_route_and_redraw(SPPath *const path,
        const bool updatePathRepr = true)
{
    // Get the new route around obstacles.
    bool rerouted = path->connEndPair.reroutePathFromLibavoid();
    if (!rerouted) {
        return;
    }

    SPItem *h2attItem[2];
    path->connEndPair.getAttachedItems(h2attItem);

    SPItem const *const path_item = SP_ITEM(path);
    SPObject const *const ancestor = get_nearest_common_ancestor(path_item, h2attItem);
    Geom::Matrix const path2anc(i2anc_affine(path_item, ancestor));

    // Set sensible values incase there the connector ends are not
    // attached to any shapes.
    Geom::PathVector conn_pv = path->curve->get_pathvector();
    double endPos[2] = { 0, static_cast<double>(conn_pv[0].size()) };

    SPConnEnd** _connEnd = path->connEndPair.getConnEnds();
    for (unsigned h = 0; h < 2; ++h) {
        if (h2attItem[h] && _connEnd[h]->type == ConnPointDefault && _connEnd[h]->id == ConnPointPosCC) {
            Geom::Matrix h2i2anc = i2anc_affine(h2attItem[h], ancestor);
            try_get_intersect_point_with_item(path, h2attItem[h], h2i2anc, path2anc,
                        (h == 0), endPos[h]);
        }
    }
    change_endpts(path->curve, endPos);
    if (updatePathRepr) {
        path->updateRepr();
        path->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    }
}


static void
sp_conn_end_shape_move(Geom::Matrix const */*mp*/, SPItem */*moved_item*/,
                            SPPath *const path)
{
    if (path->connEndPair.isAutoRoutingConn()) {
        path->connEndPair.tellLibavoidNewEndpoints();
    }
}


void
sp_conn_reroute_path(SPPath *const path)
{
    if (path->connEndPair.isAutoRoutingConn()) {
        path->connEndPair.tellLibavoidNewEndpoints();
    }
}


void
sp_conn_reroute_path_immediate(SPPath *const path)
{
    if (path->connEndPair.isAutoRoutingConn()) {
        bool processTransaction = true;
        path->connEndPair.tellLibavoidNewEndpoints(processTransaction);
    }
    // Don't update the path repr or else connector dragging is slowed by
    // constant update of values to the xml editor, and each step is also
    // needlessly remembered by undo/redo.
    bool const updatePathRepr = false;
    sp_conn_get_route_and_redraw(path, updatePathRepr);
}

void sp_conn_redraw_path(SPPath *const path)
{
    sp_conn_get_route_and_redraw(path);
}


static void
change_endpts(SPCurve *const curve, double const endPos[2])
{
    // Use Geom::Path::portion to cut the curve at the end positions
    if (endPos[0] > endPos[1])
    {
        // Path is "negative", reset the curve and return
        curve->reset();
        return;
    }
    const Geom::Path& old_path = curve->get_pathvector()[0];
    Geom::PathVector new_path_vector;
    new_path_vector.push_back(old_path.portion(endPos[0], endPos[1]));
    curve->set_pathvector(new_path_vector);
}

static void
sp_conn_end_deleted(SPObject *, SPObject *const owner, unsigned const handle_ix)
{
    // todo: The first argument is the deleted object, or just NULL if
    //       called by sp_conn_end_detach.
    g_return_if_fail(handle_ix < 2);
    char const * const attr_strs[] = {"inkscape:connection-start", "inkscape:connection-start-point",
                                      "inkscape:connection-end", "inkscape:connection-end-point"};
    SP_OBJECT_REPR(owner)->setAttribute(attr_strs[2*handle_ix], NULL);
    SP_OBJECT_REPR(owner)->setAttribute(attr_strs[2*handle_ix+1], NULL);
    /* I believe this will trigger sp_conn_end_href_changed. */
}

void
sp_conn_end_detach(SPObject *const owner, unsigned const handle_ix)
{
    sp_conn_end_deleted(NULL, owner, handle_ix);
}

void
SPConnEnd::setAttacherHref(gchar const *value, SPPath* /*path*/)
{
    if ( value && href && ( strcmp(value, href) == 0 ) ) {
        /* No change, do nothing. */
    } 
    else 
    {
        if (!value)
        {
            ref.detach();
            g_free(href);
            href = NULL;
        }
        else
        {
            bool validRef = true;
            href = g_strdup(value);
            // Now do the attaching, which emits the changed signal.
            try {
                ref.attach(Inkscape::URI(value));
            } catch (Inkscape::BadURIException &e) {
                /* TODO: Proper error handling as per
                * http://www.w3.org/TR/SVG11/implnote.html#ErrorProcessing.  (Also needed for
                * sp-use.) */
                g_warning("%s", e.what());
                validRef = false;
            }

            if ( !validRef )
            {
                ref.detach();
                g_free(href);
                href = NULL;
            }
        }
    }
}

void
SPConnEnd::setAttacherEndpoint(gchar const *value, SPPath* /*path*/)
{
    
    /* References to the connection points have the following format
        <t><id>, where t is the type of the point, which
        can be either "d" for default or "u" for user-defined, and
        id is the local (inside the item) id of the connection point.
        In the case of default points id represents the position on the
        item (i.e. Top-Left, Center-Center, etc.).
    */
    
    bool changed = false;
    ConnPointType newtype = type;
    
    if (!value)
    {
        // Default to center endpoint
        type = ConnPointDefault;
        id = 4;
    }
    else
    {
        switch (value[0])
        {
            case 'd':
                if ( newtype != ConnPointDefault )
                {
                    newtype = ConnPointDefault;
                    changed = true;
                }
                break;
            case 'u':
                if ( newtype != ConnPointUserDefined)
                {
                    newtype = ConnPointUserDefined;
                    changed = true;
                }
                break;
            default:
                g_warning("Bad reference to a connection point.");
        }
        
        int newid = (int) g_ascii_strtod( value+1, 0 );
        if ( id != newid )
        {
            id = newid;
            changed = true;
        }

        // We have to verify that the reference to the
        // connection point is a valid one.
        
        if ( changed )
        {

            // Get the item the connector is attached to
            SPItem* item = ref.getObject();
            if ( item )
            {
                if (!item->avoidRef->isValidConnPointId( newtype, newid ) )
                {
                    g_warning("Bad reference to a connection point.");
                }
                else
                {
                    type = newtype;
                    id = newid;
                }
    /*          // Update the connector
                if (path->connEndPair.isAutoRoutingConn()) {
                    path->connEndPair.tellLibavoidNewEndpoints();
                }
    */
            }
        }
    }
}

void
sp_conn_end_href_changed(SPObject */*old_ref*/, SPObject */*ref*/,
                         SPConnEnd *connEndPtr, SPPath *const path, unsigned const handle_ix)
{
    g_return_if_fail(connEndPtr != NULL);
    SPConnEnd &connEnd = *connEndPtr;
    connEnd._delete_connection.disconnect();
    connEnd._transformed_connection.disconnect();

    if (connEnd.href) {
        SPObject *refobj = connEnd.ref.getObject();
        if (refobj) {
            connEnd._delete_connection
                = SP_OBJECT(refobj)->connectDelete(sigc::bind(sigc::ptr_fun(&sp_conn_end_deleted),
                                                              SP_OBJECT(path), handle_ix));
            connEnd._transformed_connection
                = SP_ITEM(refobj)->connectTransformed(sigc::bind(sigc::ptr_fun(&sp_conn_end_shape_move),
                                                                 path));
        }
    }
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :