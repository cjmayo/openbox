#include "frame.h"
#include "openbox.h"
#include "extensions.h"
#include "framerender.h"
#include "render/theme.h"

#define PLATE_EVENTMASK (SubstructureRedirectMask | ButtonPressMask)
#define FRAME_EVENTMASK (EnterWindowMask | LeaveWindowMask | \
                         ButtonPressMask | ButtonReleaseMask)
#define ELEMENT_EVENTMASK (ButtonPressMask | ButtonReleaseMask | \
                           ButtonMotionMask | ExposureMask)

void frame_startup()
{
}

void frame_shutdown()
{
}

static Window createWindow(Window parent, unsigned long mask,
			   XSetWindowAttributes *attrib)
{
    return XCreateWindow(ob_display, parent, 0, 0, 1, 1, 0,
			 render_depth, InputOutput, render_visual,
			 mask, attrib);
                       
}

Frame *frame_new()
{
    XSetWindowAttributes attrib;
    unsigned long mask;
    Frame *self;

    self = g_new(Frame, 1);

    self->visible = FALSE;

    /* create all of the decor windows */
    mask = CWOverrideRedirect | CWEventMask;
    attrib.event_mask = FRAME_EVENTMASK;
    attrib.override_redirect = TRUE;
    self->window = createWindow(ob_root, mask, &attrib);

    mask = 0;
    self->plate = createWindow(self->window, mask, &attrib);
    XMapWindow(ob_display, self->plate);

    mask = CWEventMask;
    attrib.event_mask = ELEMENT_EVENTMASK;

    self->framedecors = 1;
    self->framedecor = g_new(FrameDecor, self->framedecors);
    self->framedecor[0].obwin.type = Window_Decoration;
    self->focused = FALSE;

    self->max_press = self->close_press = self->desk_press = 
	self->iconify_press = self->shade_press = FALSE;
    return (Frame*)self;
}

static void frame_free(Frame *self)
{
/* XXX WRITEME */
    XDestroyWindow(ob_display, self->window);

    g_free(self);
}

void frame_show(Frame *self)
{
    if (!self->visible) {
	self->visible = TRUE;
	XMapWindow(ob_display, self->window);
        XSync(ob_display, FALSE);
    }
}

void frame_hide(Frame *self)
{
    if (self->visible) {
	self->visible = FALSE;
	self->client->ignore_unmaps++;
	XUnmapWindow(ob_display, self->window);
    }
}

void frame_adjust_shape(Frame *self)
{
#ifdef SHAPEAGAERGGREA
    int num;
    XRectangle xrect[2];

    if (!self->client->shaped) {
	/* clear the shape on the frame window */
	XShapeCombineMask(ob_display, self->window, ShapeBounding,
			  self->innersize.left,
			  self->innersize.top,
			  None, ShapeSet);
    } else {
	/* make the frame's shape match the clients */
	XShapeCombineShape(ob_display, self->window, ShapeBounding,
			   self->innersize.left,
			   self->innersize.top,
			   self->client->window,
			   ShapeBounding, ShapeSet);

	num = 0;
	if (self->client->decorations & Decor_Titlebar) {
	    xrect[0].x = -theme_bevel;
	    xrect[0].y = -theme_bevel;
	    xrect[0].width = self->width + self->bwidth * 2;
	    xrect[0].height = theme_title_height +
		self->bwidth * 2;
	    ++num;
	}

	if (self->client->decorations & Decor_Handle) {
	    xrect[1].x = -theme_bevel;
	    xrect[1].y = FRAME_HANDLE_Y(self);
	    xrect[1].width = self->width + self->bwidth * 2;
	    xrect[1].height = theme_handle_height +
		self->bwidth * 2;
	    ++num;
	}

	XShapeCombineRectangles(ob_display, self->window,
				ShapeBounding, 0, 0, xrect, num,
				ShapeUnion, Unsorted);
    }
#endif
}

void frame_adjust_state(Frame *self)
{
    framerender_frame(self);
}

void frame_adjust_focus(Frame *self, gboolean hilite)
{
    self->focused = hilite;
    framerender_frame(self);
}

void frame_adjust_title(Frame *self)
{
    framerender_frame(self);
}

void frame_adjust_icon(Frame *self)
{
    framerender_frame(self);
}

void frame_grab_client(Frame *self, Client *client)
{
    int i;
    self->client = client;

    /* reparent the client to the frame */
    XReparentWindow(ob_display, client->window, self->plate, 0, 0);
    /*
      When reparenting the client window, it is usually not mapped yet, since
      this occurs from a MapRequest. However, in the case where Openbox is
      starting up, the window is already mapped, so we'll see unmap events for
      it. There are 2 unmap events generated that we see, one with the 'event'
      member set the root window, and one set to the client, but both get
      handled and need to be ignored.
    */
    if (ob_state == State_Starting)
	client->ignore_unmaps += 2;

    /* select the event mask on the client's parent (to receive config/map
       req's) the ButtonPress is to catch clicks on the client border */
    XSelectInput(ob_display, self->plate, PLATE_EVENTMASK);

    /* map the client so it maps when the frame does */
    XMapWindow(ob_display, client->window);

    frame_adjust_area(self, TRUE, TRUE);

    /* set all the windows for the frame in the window_map */
    g_hash_table_insert(window_map, &self->window, client);
    g_hash_table_insert(window_map, &self->plate, client);

    for (i = 0; i < self->framedecors; i++)
        g_hash_table_insert(window_map, &self->framedecor[i].window, 
                            &self->framedecor[i]);
}

void frame_release_client(Frame *self, Client *client)
{
    int i;
    XEvent ev;

    g_assert(self->client == client);

    /* check if the app has already reparented its window away */
    if (XCheckTypedWindowEvent(ob_display, client->window,
			       ReparentNotify, &ev)) {
	XPutBackEvent(ob_display, &ev);

	/* re-map the window since the unmanaging process unmaps it */

        /* XXX ... um no it doesnt it unmaps its parent, the window itself
           retains its mapped state, no?! XXX
           XMapWindow(ob_display, client->window); */
    } else {
	/* according to the ICCCM - if the client doesn't reparent itself,
	   then we will reparent the window to root for them */
	XReparentWindow(ob_display, client->window, ob_root,
			client->area.x,
			client->area.y);
    }

    /* remove all the windows for the frame from the window_map */
    g_hash_table_remove(window_map, &self->window);
    g_hash_table_remove(window_map, &self->plate);

    for (i = 0; i < self->framedecors; i++)
        g_hash_table_remove(window_map, &self->framedecor[i].window);

    frame_free(self);
}

Context frame_context_from_string(char *name)
{
    if (!g_ascii_strcasecmp("root", name))
        return Context_Root;
    else if (!g_ascii_strcasecmp("client", name))
        return Context_Client;
    else if (!g_ascii_strcasecmp("titlebar", name))
        return Context_Titlebar;
    else if (!g_ascii_strcasecmp("handle", name))
        return Context_Handle;
    else if (!g_ascii_strcasecmp("frame", name))
        return Context_Frame;
    else if (!g_ascii_strcasecmp("blcorner", name))
        return Context_BLCorner;
    else if (!g_ascii_strcasecmp("tlcorner", name))
        return Context_TLCorner;
    else if (!g_ascii_strcasecmp("brcorner", name))
        return Context_BRCorner;
    else if (!g_ascii_strcasecmp("trcorner", name))
        return Context_TRCorner;
    else if (!g_ascii_strcasecmp("maximize", name))
        return Context_Maximize;
    else if (!g_ascii_strcasecmp("alldesktops", name))
        return Context_AllDesktops;
    else if (!g_ascii_strcasecmp("shade", name))
        return Context_Shade;
    else if (!g_ascii_strcasecmp("iconify", name))
        return Context_Iconify;
    else if (!g_ascii_strcasecmp("icon", name))
        return Context_Icon;
    else if (!g_ascii_strcasecmp("close", name))
        return Context_Close;
    return Context_None;
}

Context frame_context(Client *client, Window win)
{
    ObWindow *obwin;

    if (win == ob_root) return Context_Root;
    if (client == NULL) return Context_None;
    if (win == client->window) return Context_Client;

    obwin = g_hash_table_lookup(window_map, &win);
    g_assert(obwin);

    if (client->frame->window == win)
        return Context_Frame;
    if (client->frame->plate == win)
        return Context_Client;

    g_assert(WINDOW_IS_DECORATION(obwin));
    return WINDOW_AS_DECORATION(obwin)->context;
}

void frame_client_gravity(Frame *self, int *x, int *y)
{
    /* horizontal */
    switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case SouthWestGravity:
    case WestGravity:
	break;

    case NorthGravity:
    case SouthGravity:
    case CenterGravity:
	*x -= (self->size.left + self->size.right) / 2;
	break;

    case NorthEastGravity:
    case SouthEastGravity:
    case EastGravity:
	*x -= self->size.left + self->size.right;
	break;

    case ForgetGravity:
    case StaticGravity:
	*x -= self->size.left;
	break;
    }

    /* vertical */
    switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case NorthEastGravity:
    case NorthGravity:
	break;

    case CenterGravity:
    case EastGravity:
    case WestGravity:
	*y -= (self->size.top + self->size.bottom) / 2;
	break;

    case SouthWestGravity:
    case SouthEastGravity:
    case SouthGravity:
	*y -= self->size.top + self->size.bottom;
	break;

    case ForgetGravity:
    case StaticGravity:
	*y -= self->size.top;
	break;
    }
}

void frame_frame_gravity(Frame *self, int *x, int *y)
{
    /* horizontal */
    switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
	break;
    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
	*x += (self->size.left + self->size.right) / 2;
	break;
    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
	*x += self->size.left + self->size.right;
	break;
    case StaticGravity:
    case ForgetGravity:
	*x += self->size.left;
	break;
    }

    /* vertical */
    switch (self->client->gravity) {
    default:
    case NorthWestGravity:
    case WestGravity:
    case SouthWestGravity:
	break;
    case NorthGravity:
    case CenterGravity:
    case SouthGravity:
	*y += (self->size.top + self->size.bottom) / 2;
	break;
    case NorthEastGravity:
    case EastGravity:
    case SouthEastGravity:
	*y += self->size.top + self->size.bottom;
	break;
    case StaticGravity:
    case ForgetGravity:
	*y += self->size.top;
	break;
    }
}
void frame_adjust_area(Frame *self, gboolean moved, gboolean resized)
{
    if (resized) {
        if (self->client->decorations & Decor_Border) {
            self->bwidth = theme_bwidth;
            self->cbwidth = theme_cbwidth;
        } else {
            self->bwidth = self->cbwidth = 0;
        }
        STRUT_SET(self->size, self->cbwidth, 
                  self->cbwidth,self->cbwidth, self->cbwidth);

        self->width = self->client->area.width + self->cbwidth * 2;
        g_assert(self->width > 0);
    }
    if (resized) {
        /* move and resize the plate */
        XMoveResizeWindow(ob_display, self->plate,
                          self->size.left - self->cbwidth,
                          self->size.top - self->cbwidth,
                          self->client->area.width,
                          self->client->area.height);

        /* when the client has StaticGravity, it likes to move around. */
        XMoveWindow(ob_display, self->client->window, 0, 0);
    }

    if (resized) {
        STRUT_SET(self->size,
                  self->cbwidth,
                  self->cbwidth,
                  self->cbwidth,
                  self->cbwidth);

    }

    /* shading can change without being moved or resized */
    RECT_SET_SIZE(self->area,
		  self->client->area.width +
		  self->size.left + self->size.right,
		  (self->client->shaded ? theme_title_height + self->bwidth*2:
                   self->client->area.height +
                   self->size.top + self->size.bottom));

    if (moved) {
        /* find the new coordinates, done after setting the frame.size, for
           frame_client_gravity. */
        self->area.x = self->client->area.x;
        self->area.y = self->client->area.y;
        frame_client_gravity((Frame*)self,
                             &self->area.x, &self->area.y);
    }

    /* move and resize the top level frame.
       shading can change without being moved or resized */
    XMoveResizeWindow(ob_display, self->window,
                      self->area.x, self->area.y,
                      self->width,
                      self->area.height - self->bwidth * 2);

    if (resized) {
        framerender_frame(self);

        frame_adjust_shape(self);
    }
}
