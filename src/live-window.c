/*
 * live-window: An actor showing the content of a window which will
 *              be updated if changed and visible on active workspace.
 *              It also provides controls to manipulate it.
 * 
 * Copyright 2012-2013 Stephan Haller <nomad@froevel.de>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "live-window.h"

#include <glib/gi18n-lib.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <gtk/gtk.h>
#include <math.h>

#include "button.h"
#include "stage.h"
#include "utils.h"

/* Define this class in GObject system */
G_DEFINE_TYPE(XfdashboardLiveWindow,
				xfdashboard_live_window,
				CLUTTER_TYPE_ACTOR)

/* Private structure - access only by public API if needed */
#define XFDASHBOARD_LIVE_WINDOW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), XFDASHBOARD_TYPE_LIVE_WINDOW, XfdashboardLiveWindowPrivate))

struct _XfdashboardLiveWindowPrivate
{
	/* Properties related */
	XfdashboardWindowTrackerWindow					*window;
	
	gfloat						paddingClose;
	gfloat						paddingTitle;

	/* Instance related */
	XfdashboardWindowTracker	*windowTracker;

	gboolean					isVisible;

	ClutterActor				*actorWindow;
	ClutterActor				*actorClose;
	ClutterActor				*actorTitle;
};

/* Properties */
enum
{
	PROP_0,

	PROP_WINDOW,

	PROP_CLOSE_BUTTON_PADDING,
	PROP_TITLE_ACTOR_PADDING,

	PROP_LAST
};

static GParamSpec* XfdashboardLiveWindowProperties[PROP_LAST]={ 0, };

/* Signals */
enum
{
	SIGNAL_CLICKED,
	SIGNAL_CLOSE,

	SIGNAL_GEOMETRY_CHANGED,
	SIGNAL_VISIBILITY_CHANGED,
	SIGNAL_WORKSPACE_CHANGED,

	SIGNAL_LAST
};

static guint XfdashboardLiveWindowSignals[SIGNAL_LAST]={ 0, };

/* IMPLEMENTATION: Private variables and methods */
#define DEFAULT_PADDING_TITLE		4.0f			// TODO: Replace by settings/theming object
#define DEFAULT_PADDING_CLOSE		4.0f			// TODO: Replace by settings/theming object
#define WINDOW_CLOSE_BUTTON_ICON	GTK_STOCK_CLOSE	// TODO: Replace by settings/theming object

/* Check if window should be shown */
static gboolean _xfdashboard_live_window_is_visible_window(XfdashboardLiveWindow *self, XfdashboardWindowTrackerWindow *inWindow)
{
	g_return_val_if_fail(XFDASHBOARD_IS_WINDOW_TRACKER_WINDOW(inWindow), FALSE);

	/* Determine if windows should be shown depending on its state */
	if(xfdashboard_window_tracker_window_is_skip_pager(inWindow) ||
		xfdashboard_window_tracker_window_is_skip_tasklist(inWindow))
	{
		return(FALSE);
	}

	/* If we get here the window should be shown */
	return(TRUE);
}

/* This actor was clicked */
static void _xfdashboard_live_window_on_clicked(XfdashboardLiveWindow *self, ClutterActor *inActor, gpointer inUserData)
{
	XfdashboardLiveWindowPrivate	*priv=self->priv;
	ClutterClickAction				*action=CLUTTER_CLICK_ACTION(inUserData);
	gfloat							eventX, eventY;
	gfloat							relX, relY;
	ClutterActorBox					closeBox;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(CLUTTER_IS_ACTOR(inActor));
	g_return_if_fail(CLUTTER_IS_CLICK_ACTION(inUserData));

	priv=self->priv;
	action=CLUTTER_CLICK_ACTION(inUserData);

	/* Check if click happened in "close button" */
	if(CLUTTER_ACTOR_IS_VISIBLE(priv->actorClose))
	{
		clutter_click_action_get_coords(action, &eventX, &eventY);
		if(clutter_actor_transform_stage_point(CLUTTER_ACTOR(self), eventX, eventY, &relX, &relY))
		{
			clutter_actor_get_allocation_box(priv->actorClose, &closeBox);
			if(clutter_actor_box_contains(&closeBox, relX, relY))
			{
				g_signal_emit(self, XfdashboardLiveWindowSignals[SIGNAL_CLOSE], 0);
				return;
			}
		}
	}

	/* Emit "clicked" signal */
	g_signal_emit(self, XfdashboardLiveWindowSignals[SIGNAL_CLICKED], 0);
}

/* Position and/or size of window has changed */
static void _xfdashboard_live_window_on_geometry_changed(XfdashboardLiveWindow *self, XfdashboardWindowTrackerWindow *inWindow, gpointer inUserData)
{
	XfdashboardLiveWindowPrivate	*priv;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(XFDASHBOARD_IS_WINDOW_TRACKER_WINDOW(inWindow));

	priv=self->priv;

	/* Check if signal is for this window */
	if(inWindow!=priv->window) return;

	/* Actor's allocation may change because of new geometry so relayout */
	clutter_actor_queue_relayout(CLUTTER_ACTOR(self));

	/* Emit "geometry-changed" signal */
	g_signal_emit(self, XfdashboardLiveWindowSignals[SIGNAL_GEOMETRY_CHANGED], 0);
}

/* Action items of window has changed */
static void _xfdashboard_live_window_on_actions_changed(XfdashboardLiveWindow *self,
														XfdashboardWindowTrackerWindow *inWindow,
														gpointer inUserData)
{
	XfdashboardLiveWindowPrivate	*priv;
	gboolean						currentCloseVisible;
	gboolean						newCloseVisible;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(XFDASHBOARD_IS_WINDOW_TRACKER_WINDOW(inWindow));

	priv=self->priv;

	/* Check if signal is for this window */
	if(inWindow!=priv->window) return;

	/* Determine current and new state of actions */
	currentCloseVisible=(CLUTTER_ACTOR_IS_VISIBLE(priv->actorClose) ? TRUE : FALSE);
	newCloseVisible=xfdashboard_window_tracker_window_has_close_action(priv->window);
	
	/* Show or hide close button actor */
	if(newCloseVisible!=currentCloseVisible)
	{
		if(newCloseVisible) clutter_actor_show(priv->actorClose);
			else clutter_actor_hide(priv->actorClose);
	}
}

/* Icon of window has changed */
static void _xfdashboard_live_window_on_icon_changed(XfdashboardLiveWindow *self,
														XfdashboardWindowTrackerWindow *inWindow,
														gpointer inUserData)
{
	XfdashboardLiveWindowPrivate	*priv;
	ClutterImage					*icon;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(XFDASHBOARD_IS_WINDOW_TRACKER_WINDOW(inWindow));

	priv=self->priv;

	/* Check if signal is for this window */
	if(inWindow!=priv->window) return;

	/* Set new icon in title actor */
	icon=xfdashboard_get_image_for_pixbuf(xfdashboard_window_tracker_window_get_icon(inWindow));
	xfdashboard_button_set_icon_image(XFDASHBOARD_BUTTON(priv->actorTitle), icon);
	g_object_unref(icon);
}

/* Title of window has changed */
static void _xfdashboard_live_window_on_name_changed(XfdashboardLiveWindow *self,
														XfdashboardWindowTrackerWindow *inWindow,
														gpointer inUserData)
{
	XfdashboardLiveWindowPrivate	*priv;
	gchar							*windowName;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(XFDASHBOARD_IS_WINDOW_TRACKER_WINDOW(inWindow));

	priv=self->priv;

	/* Check if signal is for this window */
	if(inWindow!=priv->window) return;

	/* Set new icon in title actor */
	windowName=g_markup_printf_escaped("%s", xfdashboard_window_tracker_window_get_title(inWindow));
	xfdashboard_button_set_text(XFDASHBOARD_BUTTON(priv->actorTitle), windowName);
	g_free(windowName);
}

/* Window's state has changed */
static void _xfdashboard_live_window_on_state_changed(XfdashboardLiveWindow *self,
														XfdashboardWindowTrackerWindow *inWindow,
														gpointer inUserData)
{
	XfdashboardLiveWindowPrivate	*priv;
	gboolean						isVisible;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(XFDASHBOARD_IS_WINDOW_TRACKER_WINDOW(inWindow));

	priv=self->priv;

	/* Check if signal is for this window */
	if(inWindow!=priv->window) return;

	/* Check if window's visibility has changed */
	isVisible=_xfdashboard_live_window_is_visible_window(self, inWindow);
	if(priv->isVisible!=isVisible)
	{
		priv->isVisible=isVisible;
		g_signal_emit(self, XfdashboardLiveWindowSignals[SIGNAL_VISIBILITY_CHANGED], 0);
	}
}

/* Window's workspace has changed */
static void _xfdashboard_live_window_on_workspace_changed(XfdashboardLiveWindow *self,
															XfdashboardWindowTrackerWindow *inWindow,
															gpointer inUserData)
{
	XfdashboardLiveWindowPrivate	*priv;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(XFDASHBOARD_IS_WINDOW_TRACKER_WINDOW(inWindow));

	priv=self->priv;

	/* Check if signal is for this window */
	if(inWindow!=priv->window) return;

	/* Emit "workspace-changed" signal */
	g_signal_emit(self, XfdashboardLiveWindowSignals[SIGNAL_WORKSPACE_CHANGED], 0);
}

/* IMPLEMENTATION: ClutterActor */

/* Get preferred width/height */
static void _xfdashboard_live_window_get_preferred_height(ClutterActor *self,
															gfloat inForWidth,
															gfloat *outMinHeight,
															gfloat *outNaturalHeight)
{
	XfdashboardLiveWindowPrivate	*priv=XFDASHBOARD_LIVE_WINDOW(self)->priv;
	gfloat							minHeight, naturalHeight;
	gfloat							childMinHeight, childNaturalHeight;

	minHeight=naturalHeight=0.0f;

	/* Determine size of window if available and visible (should usually be the largest actor) */
	if(priv->actorWindow && CLUTTER_ACTOR_IS_VISIBLE(priv->actorWindow))
	{
		clutter_actor_get_preferred_height(priv->actorWindow,
											inForWidth,
											&childMinHeight,
											&childNaturalHeight);
		if(childMinHeight>minHeight) minHeight=childMinHeight;
		if(childNaturalHeight>naturalHeight) naturalHeight=childNaturalHeight;
	}

	/* Determine size of title actor if visible */
	if(CLUTTER_ACTOR_IS_VISIBLE(priv->actorTitle))
	{
		clutter_actor_get_preferred_height(priv->actorTitle,
											inForWidth,
											&childMinHeight,
											&childNaturalHeight);
		childMinHeight+=(2*priv->paddingTitle);
		childNaturalHeight+=(2*priv->paddingTitle);
		if(childMinHeight>minHeight) minHeight=childMinHeight;
		if(childNaturalHeight>naturalHeight) naturalHeight=childNaturalHeight;
	}

	/* Determine size of close button actor if visible */
	if(CLUTTER_ACTOR_IS_VISIBLE(priv->actorClose))
	{
		clutter_actor_get_preferred_height(priv->actorClose,
											inForWidth,
											&childMinHeight,
											&childNaturalHeight);
		childMinHeight+=(2*priv->paddingClose);
		childNaturalHeight+=(2*priv->paddingClose);
		if(childMinHeight>minHeight) minHeight=childMinHeight;
		if(childNaturalHeight>naturalHeight) naturalHeight=childNaturalHeight;
	}

	/* Store sizes computed */
	if(outMinHeight) *outMinHeight=minHeight;
	if(outNaturalHeight) *outNaturalHeight=naturalHeight;
}

static void _xfdashboard_live_window_get_preferred_width(ClutterActor *self,
															gfloat inForHeight,
															gfloat *outMinWidth,
															gfloat *outNaturalWidth)
{
	XfdashboardLiveWindowPrivate	*priv=XFDASHBOARD_LIVE_WINDOW(self)->priv;
	gfloat							minWidth, naturalWidth;
	gfloat							childMinWidth, childNaturalWidth;

	minWidth=naturalWidth=0.0f;

	/* Determine size of window if available and visible (should usually be the largest actor) */
	if(priv->actorWindow && CLUTTER_ACTOR_IS_VISIBLE(priv->actorWindow))
	{
		clutter_actor_get_preferred_width(priv->actorWindow,
											inForHeight,
											&childMinWidth,
											&childNaturalWidth);
		if(childMinWidth>minWidth) minWidth=childMinWidth;
		if(childNaturalWidth>naturalWidth) naturalWidth=childNaturalWidth;
	}

	/* Determine size of title actor if visible */
	if(CLUTTER_ACTOR_IS_VISIBLE(priv->actorTitle))
	{
		clutter_actor_get_preferred_width(priv->actorTitle,
											inForHeight,
											&childMinWidth,
											 &childNaturalWidth);
		childMinWidth+=(2*priv->paddingTitle);
		childNaturalWidth+=(2*priv->paddingTitle);
		if(childMinWidth>minWidth) minWidth=childMinWidth;
		if(childNaturalWidth>naturalWidth) naturalWidth=childNaturalWidth;
	}

	/* Determine size of close button actor if visible */
	if(CLUTTER_ACTOR_IS_VISIBLE(priv->actorClose))
	{
		clutter_actor_get_preferred_width(priv->actorClose,
											inForHeight,
											&childMinWidth,
											&childNaturalWidth);
		childMinWidth+=(2*priv->paddingClose);
		childNaturalWidth+=(2*priv->paddingClose);
		if(childMinWidth>minWidth) minWidth=childMinWidth;
		if(childNaturalWidth>naturalWidth) naturalWidth=childNaturalWidth;
	}

	/* Store sizes computed */
	if(outMinWidth) *outMinWidth=minWidth;
	if(outNaturalWidth) *outNaturalWidth=naturalWidth;
}

/* Allocate position and size of actor and its children*/
static void _xfdashboard_live_window_allocate(ClutterActor *self,
												const ClutterActorBox *inBox,
												ClutterAllocationFlags inFlags)
{
	XfdashboardLiveWindowPrivate	*priv=XFDASHBOARD_LIVE_WINDOW(self)->priv;
	ClutterActorBox					*boxActorWindow=NULL;
	ClutterActorBox					*boxActorTitle=NULL;
	ClutterActorBox					*boxActorClose=NULL;
	gfloat							maxWidth;
	gfloat							titleWidth, titleHeight;
	gfloat							closeWidth, closeHeight;
	gfloat							left, top, right, bottom;

	/* Chain up to store the allocation of the actor */
	CLUTTER_ACTOR_CLASS(xfdashboard_live_window_parent_class)->allocate(self, inBox, inFlags);

	/* Set allocation on window texture */
	boxActorWindow=clutter_actor_box_copy(inBox);
	clutter_actor_box_set_origin(boxActorWindow, 0.0f, 0.0f);
	clutter_actor_allocate(priv->actorWindow, boxActorWindow, inFlags);

	/* Set allocation on close actor */
	clutter_actor_get_preferred_size(priv->actorClose,
										NULL, NULL,
										&closeWidth, &closeHeight);

	right=clutter_actor_box_get_x(boxActorWindow)+clutter_actor_box_get_width(boxActorWindow)-priv->paddingClose;
	left=MAX(right-closeWidth, priv->paddingClose);
	top=clutter_actor_box_get_y(boxActorWindow)+priv->paddingClose;
	bottom=top+closeHeight;

	right=MAX(left, right);
	bottom=MAX(top, bottom);

	boxActorClose=clutter_actor_box_new(floor(left), floor(top), floor(right), floor(bottom));
	clutter_actor_allocate(priv->actorClose, boxActorClose, inFlags);

	/* Set allocation on title actor
	 * But prevent that title overlaps close button
	 */
	clutter_actor_get_preferred_size(priv->actorTitle,
										NULL, NULL,
										&titleWidth, &titleHeight);

	maxWidth=clutter_actor_box_get_width(boxActorWindow)-(2*priv->paddingTitle);
	if(titleWidth>maxWidth) titleWidth=maxWidth;

	left=clutter_actor_box_get_x(boxActorWindow)+((clutter_actor_box_get_width(boxActorWindow)-titleWidth)/2.0f);
	right=left+titleWidth;
	bottom=clutter_actor_box_get_y(boxActorWindow)+clutter_actor_box_get_height(boxActorWindow)-(2*priv->paddingTitle);
	top=bottom-titleHeight;
	if(left>right) left=right-1.0f;
	if(top<(clutter_actor_box_get_y(boxActorClose)+clutter_actor_box_get_height(boxActorClose)))
	{
		if(right>=clutter_actor_box_get_x(boxActorClose))
		{
			right=clutter_actor_box_get_x(boxActorClose)-MIN(priv->paddingTitle, priv->paddingClose);
		}

		if(top<clutter_actor_box_get_y(boxActorClose))
		{
			top=clutter_actor_box_get_y(boxActorClose);
			bottom=top+titleHeight;
		}
	}

	right=MAX(left, right);
	bottom=MAX(top, bottom);

	boxActorTitle=clutter_actor_box_new(floor(left), floor(top), floor(right), floor(bottom));
	clutter_actor_allocate(priv->actorTitle, boxActorTitle, inFlags);

	/* Release allocated resources */
	if(boxActorWindow) clutter_actor_box_free(boxActorWindow);
	if(boxActorTitle) clutter_actor_box_free(boxActorTitle);
	if(boxActorClose) clutter_actor_box_free(boxActorClose);
}

/* IMPLEMENTATION: GObject */

/* Dispose this object */
static void _xfdashboard_live_window_dispose(GObject *inObject)
{
	XfdashboardLiveWindow			*self=XFDASHBOARD_LIVE_WINDOW(inObject);
	XfdashboardLiveWindowPrivate	*priv=self->priv;

	/* Dispose allocated resources */
	if(priv->window)
	{
		priv->window=NULL;
	}

	if(priv->windowTracker)
	{
		g_signal_handlers_disconnect_by_data(priv->windowTracker, self);
		g_object_unref(priv->windowTracker);
		priv->windowTracker=NULL;
	}

	if(priv->actorWindow)
	{
		clutter_actor_destroy(priv->actorWindow);
		priv->actorWindow=NULL;
	}

	if(priv->actorTitle)
	{
		clutter_actor_destroy(priv->actorTitle);
		priv->actorTitle=NULL;
	}

	if(priv->actorClose)
	{
		clutter_actor_destroy(priv->actorClose);
		priv->actorClose=NULL;
	}

	/* Call parent's class dispose method */
	G_OBJECT_CLASS(xfdashboard_live_window_parent_class)->dispose(inObject);
}

/* Set/get properties */
static void _xfdashboard_live_window_set_property(GObject *inObject,
													guint inPropID,
													const GValue *inValue,
													GParamSpec *inSpec)
{
	XfdashboardLiveWindow			*self=XFDASHBOARD_LIVE_WINDOW(inObject);
	
	switch(inPropID)
	{
		case PROP_WINDOW:
			xfdashboard_live_window_set_window(self, g_value_get_object(inValue));
			break;

		case PROP_CLOSE_BUTTON_PADDING:
			xfdashboard_live_window_set_close_button_padding(self, g_value_get_float(inValue));
			break;

		case PROP_TITLE_ACTOR_PADDING:
			xfdashboard_live_window_set_title_actor_padding(self, g_value_get_float(inValue));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

static void _xfdashboard_live_window_get_property(GObject *inObject,
													guint inPropID,
													GValue *outValue,
													GParamSpec *inSpec)
{
	XfdashboardLiveWindow	*self=XFDASHBOARD_LIVE_WINDOW(inObject);

	switch(inPropID)
	{
		case PROP_WINDOW:
			g_value_set_object(outValue, self->priv->window);
			break;

		case PROP_CLOSE_BUTTON_PADDING:
			g_value_set_float(outValue, self->priv->paddingClose);
			break;

		case PROP_TITLE_ACTOR_PADDING:
			g_value_set_float(outValue, self->priv->paddingTitle);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

/* Class initialization
 * Override functions in parent classes and define properties
 * and signals
 */
static void xfdashboard_live_window_class_init(XfdashboardLiveWindowClass *klass)
{
	ClutterActorClass	*actorClass=CLUTTER_ACTOR_CLASS(klass);
	GObjectClass		*gobjectClass=G_OBJECT_CLASS(klass);

	/* Override functions */
	actorClass->get_preferred_width=_xfdashboard_live_window_get_preferred_width;
	actorClass->get_preferred_height=_xfdashboard_live_window_get_preferred_height;
	actorClass->allocate=_xfdashboard_live_window_allocate;

	gobjectClass->dispose=_xfdashboard_live_window_dispose;
	gobjectClass->set_property=_xfdashboard_live_window_set_property;
	gobjectClass->get_property=_xfdashboard_live_window_get_property;

	/* Set up private structure */
	g_type_class_add_private(klass, sizeof(XfdashboardLiveWindowPrivate));

	/* Define properties */
	XfdashboardLiveWindowProperties[PROP_WINDOW]=
		g_param_spec_object("window",
								_("Window"),
								_("The window to show"),
								XFDASHBOARD_TYPE_WINDOW_TRACKER_WINDOW,
								G_PARAM_READWRITE);

	XfdashboardLiveWindowProperties[PROP_CLOSE_BUTTON_PADDING]=
		g_param_spec_float("close-button-padding",
							_("Close button padding"),
							_("Padding of close button to window actor in pixels"),
							0.0f, G_MAXFLOAT,
							DEFAULT_PADDING_CLOSE,
							G_PARAM_READWRITE);

	XfdashboardLiveWindowProperties[PROP_TITLE_ACTOR_PADDING]=
		g_param_spec_float("title-actor-padding",
							_("Title actor padding"),
							_("Padding of title actor to window actor in pixels"),
							0.0f, G_MAXFLOAT,
							DEFAULT_PADDING_TITLE,
							G_PARAM_READWRITE);

	g_object_class_install_properties(gobjectClass, PROP_LAST, XfdashboardLiveWindowProperties);

	/* Define signals */
	XfdashboardLiveWindowSignals[SIGNAL_CLICKED]=
		g_signal_new("clicked",
						G_TYPE_FROM_CLASS(klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET(XfdashboardLiveWindowClass, clicked),
						NULL,
						NULL,
						g_cclosure_marshal_VOID__VOID,
						G_TYPE_NONE,
						0);

	XfdashboardLiveWindowSignals[SIGNAL_CLOSE]=
		g_signal_new("close",
						G_TYPE_FROM_CLASS(klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET(XfdashboardLiveWindowClass, close),
						NULL,
						NULL,
						g_cclosure_marshal_VOID__VOID,
						G_TYPE_NONE,
						0);

	XfdashboardLiveWindowSignals[SIGNAL_GEOMETRY_CHANGED]=
		g_signal_new("geometry-changed",
						G_TYPE_FROM_CLASS(klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET(XfdashboardLiveWindowClass, geometry_changed),
						NULL,
						NULL,
						g_cclosure_marshal_VOID__VOID,
						G_TYPE_NONE,
						0);

	XfdashboardLiveWindowSignals[SIGNAL_VISIBILITY_CHANGED]=
		g_signal_new("visibility-changed",
						G_TYPE_FROM_CLASS(klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET(XfdashboardLiveWindowClass, visibility_changed),
						NULL,
						NULL,
						g_cclosure_marshal_VOID__BOOLEAN,
						G_TYPE_NONE,
						1,
						G_TYPE_BOOLEAN);

	XfdashboardLiveWindowSignals[SIGNAL_WORKSPACE_CHANGED]=
		g_signal_new("workspace-changed",
						G_TYPE_FROM_CLASS(klass),
						G_SIGNAL_RUN_LAST,
						G_STRUCT_OFFSET(XfdashboardLiveWindowClass, workspace_changed),
						NULL,
						NULL,
						g_cclosure_marshal_VOID__VOID,
						G_TYPE_NONE,
						0);
}

/* Object initialization
 * Create private structure and set up default values
 */
static void xfdashboard_live_window_init(XfdashboardLiveWindow *self)
{
	XfdashboardLiveWindowPrivate	*priv;
	ClutterAction					*action;

	priv=self->priv=XFDASHBOARD_LIVE_WINDOW_GET_PRIVATE(self);

	/* This actor reacts on events */
	clutter_actor_set_reactive(CLUTTER_ACTOR(self), TRUE);

	/* Set default values */
	priv->windowTracker=xfdashboard_window_tracker_get_default();
	priv->window=NULL;
	priv->paddingTitle=DEFAULT_PADDING_TITLE;
	priv->paddingClose=DEFAULT_PADDING_CLOSE;

	/* Set up child actors (order is important) */
	priv->actorWindow=clutter_x11_texture_pixmap_new();
	clutter_actor_show(priv->actorWindow);
	clutter_actor_add_child(CLUTTER_ACTOR(self), priv->actorWindow);

	priv->actorTitle=xfdashboard_button_new();
	clutter_actor_set_reactive(priv->actorTitle, FALSE);
	xfdashboard_button_set_style(XFDASHBOARD_BUTTON(priv->actorTitle), XFDASHBOARD_STYLE_BOTH);
	xfdashboard_background_set_background_type(XFDASHBOARD_BACKGROUND(priv->actorTitle),
												XFDASHBOARD_BACKGROUND_TYPE_FILL | XFDASHBOARD_BACKGROUND_TYPE_ROUNDED_CORNERS);
	xfdashboard_background_set_corner_radius(XFDASHBOARD_BACKGROUND(priv->actorTitle), priv->paddingTitle);
	clutter_actor_show(priv->actorTitle);
	clutter_actor_add_child(CLUTTER_ACTOR(self), priv->actorTitle);

	priv->actorClose=xfdashboard_button_new_with_icon(WINDOW_CLOSE_BUTTON_ICON);
	clutter_actor_set_reactive(priv->actorClose, FALSE);
	xfdashboard_background_set_background_type(XFDASHBOARD_BACKGROUND(priv->actorClose),
												XFDASHBOARD_BACKGROUND_TYPE_FILL | XFDASHBOARD_BACKGROUND_TYPE_ROUNDED_CORNERS);
	xfdashboard_background_set_corner_radius(XFDASHBOARD_BACKGROUND(priv->actorClose), priv->paddingClose);
	clutter_actor_show(priv->actorClose);
	clutter_actor_add_child(CLUTTER_ACTOR(self), priv->actorClose);

	/* Connect signals */
	action=clutter_click_action_new();
	clutter_actor_add_action(CLUTTER_ACTOR(self), action);
	g_signal_connect_swapped(action, "clicked", G_CALLBACK(_xfdashboard_live_window_on_clicked), self);

	g_signal_connect_swapped(priv->windowTracker, "window-geometry-changed", G_CALLBACK(_xfdashboard_live_window_on_geometry_changed), self);
	g_signal_connect_swapped(priv->windowTracker, "window-actions-changed", G_CALLBACK(_xfdashboard_live_window_on_actions_changed), self);
	g_signal_connect_swapped(priv->windowTracker, "window-state-changed", G_CALLBACK(_xfdashboard_live_window_on_state_changed), self);
	g_signal_connect_swapped(priv->windowTracker, "window-icon-changed", G_CALLBACK(_xfdashboard_live_window_on_icon_changed), self);
	g_signal_connect_swapped(priv->windowTracker, "window-name-changed", G_CALLBACK(_xfdashboard_live_window_on_name_changed), self);
	g_signal_connect_swapped(priv->windowTracker, "window-workspace-changed", G_CALLBACK(_xfdashboard_live_window_on_workspace_changed), self);
}

/* Implementation: Public API */

/* Create new instance */
ClutterActor* xfdashboard_live_window_new(void)
{
	return(CLUTTER_ACTOR(g_object_new(XFDASHBOARD_TYPE_LIVE_WINDOW, NULL)));
}

ClutterActor* xfdashboard_live_window_new_for_window(XfdashboardWindowTrackerWindow *inWindow)
{
	g_return_val_if_fail(XFDASHBOARD_IS_WINDOW_TRACKER_WINDOW(inWindow), NULL);

	return(CLUTTER_ACTOR(g_object_new(XFDASHBOARD_TYPE_LIVE_WINDOW,
										"window", inWindow,
										NULL)));
}

/* Get/set window to show */
XfdashboardWindowTrackerWindow* xfdashboard_live_window_get_window(XfdashboardLiveWindow *self)
{
	g_return_val_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self), NULL);

	return(self->priv->window);
}

void xfdashboard_live_window_set_window(XfdashboardLiveWindow *self, XfdashboardWindowTrackerWindow *inWindow)
{
	XfdashboardLiveWindowPrivate	*priv;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(XFDASHBOARD_IS_WINDOW_TRACKER_WINDOW(inWindow));

	priv=self->priv;

	/* Only set value if it changes */
	if(inWindow==priv->window) return;

	/* Release old value */
	if(priv->window)
	{
		g_signal_handlers_disconnect_by_data(priv->window, self);
		priv->window=NULL;
	}

	/* Set new value
	 * Window tracker objects should never be refed or unrefed, so just set new value
	 */
	priv->window=inWindow;
	priv->isVisible=_xfdashboard_live_window_is_visible_window(self, priv->window);

	/* Setup window actor */
	clutter_x11_texture_pixmap_set_window(CLUTTER_X11_TEXTURE_PIXMAP(priv->actorWindow), xfdashboard_window_tracker_window_get_xid(priv->window), TRUE);
	clutter_x11_texture_pixmap_sync_window(CLUTTER_X11_TEXTURE_PIXMAP(priv->actorWindow));
	clutter_x11_texture_pixmap_set_automatic(CLUTTER_X11_TEXTURE_PIXMAP(priv->actorWindow), TRUE);

	/* Set up this actor and child actor by calling each signal handler now */
	_xfdashboard_live_window_on_geometry_changed(self, priv->window, priv->windowTracker);
	_xfdashboard_live_window_on_actions_changed(self, priv->window, priv->windowTracker);
	_xfdashboard_live_window_on_icon_changed(self, priv->window, priv->windowTracker);
	_xfdashboard_live_window_on_name_changed(self, priv->window, priv->windowTracker);
	_xfdashboard_live_window_on_state_changed(self, priv->window, priv->windowTracker);
	_xfdashboard_live_window_on_workspace_changed(self, priv->window, priv->windowTracker);

	/* Notify about property change */
	g_object_notify_by_pspec(G_OBJECT(self), XfdashboardLiveWindowProperties[PROP_WINDOW]);
}

/* Get/set padding of title actor */
gfloat xfdashboard_live_window_get_title_actor_padding(XfdashboardLiveWindow *self)
{
	g_return_val_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self), 0.0f);

	return(self->priv->paddingTitle);
}

void xfdashboard_live_window_set_title_actor_padding(XfdashboardLiveWindow *self, gfloat inPadding)
{
	XfdashboardLiveWindowPrivate	*priv;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(inPadding>=0.0f);

	priv=self->priv;

	/* Set value if changed */
	if(priv->paddingTitle!=inPadding)
	{
		/* Set value */
		priv->paddingTitle=inPadding;
		xfdashboard_background_set_corner_radius(XFDASHBOARD_BACKGROUND(priv->actorTitle), priv->paddingTitle);
		clutter_actor_queue_relayout(CLUTTER_ACTOR(self));

		/* Notify about property change */
		g_object_notify_by_pspec(G_OBJECT(self), XfdashboardLiveWindowProperties[PROP_TITLE_ACTOR_PADDING]);
	}
}

/* Get/set padding of close button actor */
gfloat xfdashboard_live_window_get_close_button_padding(XfdashboardLiveWindow *self)
{
	g_return_val_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self), 0.0f);

	return(self->priv->paddingClose);
}

void xfdashboard_live_window_set_close_button_padding(XfdashboardLiveWindow *self, gfloat inPadding)
{
	XfdashboardLiveWindowPrivate	*priv;

	g_return_if_fail(XFDASHBOARD_IS_LIVE_WINDOW(self));
	g_return_if_fail(inPadding>=0.0f);

	priv=self->priv;

	/* Set value if changed */
	if(priv->paddingClose!=inPadding)
	{
		/* Set value */
		priv->paddingClose=inPadding;
		xfdashboard_background_set_corner_radius(XFDASHBOARD_BACKGROUND(priv->actorClose), priv->paddingClose);
		clutter_actor_queue_relayout(CLUTTER_ACTOR(self));

		/* Notify about property change */
		g_object_notify_by_pspec(G_OBJECT(self), XfdashboardLiveWindowProperties[PROP_CLOSE_BUTTON_PADDING]);
	}
}
