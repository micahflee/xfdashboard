/*
 * main.c: Common functions, shared data
 *         and main entry point of application
 * 
 * Copyright 2012 Stephan Haller <nomad@froevel.de>
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

#include "common.h"
#include "live-window.h"
#include "scaling-flow-layout.h"
#include "viewpad.h"
#include "searchbox.h"
#include "applications-view.h"
#include "windows-view.h"
#include "quicklaunch.h"
#include "scrollbar.h"

/* Stage and child actors */
ClutterActor	*stage=NULL;
ClutterActor	*group=NULL;
ClutterActor	*quicklaunch=NULL;
ClutterActor	*viewpad=NULL;
ClutterActor	*searchbox=NULL;

gulong			signalStageDestroyID=0L;
gulong			signalSearchEndedID=0L;

const gfloat	spacingToStage=8.0f;

/* TODO: Replace with xfconf */
static gchar	*quicklaunch_apps[]=	{
											"firefox.desktop",
											"evolution.desktop",
											"Terminal.desktop",
											"Thunar.desktop",
											"geany.desktop",
											"gajim.desktop"
										};
/* TODO: Replace with xfconf */

/* Search request ended */
void _xfdashboard_on_search_ended(ClutterActor *inSearchbox, gpointer inUserData)
{
	g_return_if_fail(XFDASHBOARD_IS_SEARCHBOX(inSearchbox));
	g_return_if_fail(XFDASHBOARD_IS_VIEW(inUserData));

	XfdashboardView			*activeView=XFDASHBOARD_VIEW(inUserData);

	/* Remove signal handler from search box */
	if(signalSearchEndedID &&
		g_signal_handler_is_connected(searchbox, signalSearchEndedID))
	{
		g_signal_handler_disconnect(searchbox, signalSearchEndedID);
		signalSearchEndedID=0L;
	}

	/* Set active view */
	xfdashboard_viewpad_set_active_view(XFDASHBOARD_VIEWPAD(viewpad), activeView);
}

/* Search request started */
void _xfdashboard_on_search_started(ClutterActor *inSearchbox, gpointer inUserData)
{
	g_return_if_fail(XFDASHBOARD_IS_SEARCHBOX(inSearchbox));
	g_return_if_fail(XFDASHBOARD_IS_VIEW(inUserData));
	
	XfdashboardView			*view=XFDASHBOARD_VIEW(inUserData);
	XfdashboardView			*activeView=NULL;
	
	/* Remove signal handler from search box if there is any connected */
	if(signalSearchEndedID &&
		g_signal_handler_is_connected(searchbox, signalSearchEndedID))
	{
		g_signal_handler_disconnect(searchbox, signalSearchEndedID);
		signalSearchEndedID=0L;
	}

	/* Get current active view and connect "search-ended" signal
	 * to switch back to this current active view
	 */
	activeView=xfdashboard_viewpad_get_active_view(XFDASHBOARD_VIEWPAD(viewpad));
	signalSearchEndedID=g_signal_connect(searchbox, "search-ended", G_CALLBACK(_xfdashboard_on_search_ended), activeView);

	/* Activate search view */
	xfdashboard_viewpad_set_active_view(XFDASHBOARD_VIEWPAD(viewpad), view);
}

/* Search request has changed */
void _xfdashboard_on_search_update(ClutterActor *inSearchbox, const gchar *inText, gpointer inUserData)
{
	g_return_if_fail(XFDASHBOARD_IS_SEARCHBOX(inSearchbox));
	g_return_if_fail(XFDASHBOARD_IS_VIEW(inUserData));

	XfdashboardView			*view=XFDASHBOARD_VIEW(inUserData);
	XfdashboardView			*activeView=NULL;

	/* Switch to view if not active */
	activeView=xfdashboard_viewpad_get_active_view(XFDASHBOARD_VIEWPAD(viewpad));
	if(activeView!=view)
	{
		xfdashboard_viewpad_set_active_view(XFDASHBOARD_VIEWPAD(viewpad), view);
	}

	/* Update search term and search view */
	xfdashboard_applications_view_set_filter_text(XFDASHBOARD_APPLICATIONS_VIEW(view), inText);
}

/* Secondary icon on searchbox was clicked to clear it */
void _xfdashboard_on_clear_searchbox(ClutterActor *inSearchbox, gpointer inUserData)
{
	g_return_if_fail(XFDASHBOARD_IS_SEARCHBOX(inSearchbox));

	/* Remove "search-ended" signal handler as we want to avoid switching view
	 * when secondary icon was clicked
	 */
	if(signalSearchEndedID &&
		g_signal_handler_is_connected(searchbox, signalSearchEndedID))
	{
		g_signal_handler_disconnect(searchbox, signalSearchEndedID);
		signalSearchEndedID=0L;
	}

	/* Clear search box */
	xfdashboard_searchbox_set_text(XFDASHBOARD_SEARCHBOX(inSearchbox), NULL);
}
/* "Switch to a view" button in quicklaunch was clicked */
void _xfdashboard_on_switch_to_view_clicked(ClutterActor *inView, gpointer inUserData)
{
	g_return_if_fail(XFDASHBOARD_IS_VIEW(inView));

	/* Remove signal handler from search box if there is any connected
	 * as we do not know which view to switch back to when search ended */
	if(signalSearchEndedID &&
		g_signal_handler_is_connected(searchbox, signalSearchEndedID))
	{
		g_signal_handler_disconnect(searchbox, signalSearchEndedID);
		signalSearchEndedID=0L;
	}

	/* Activate view */
	xfdashboard_viewpad_set_active_view(XFDASHBOARD_VIEWPAD(viewpad), XFDASHBOARD_VIEW(inView));
}

/* Windows view was activated */
void _xfdashboard_on_windows_view_activated(ClutterActor *inView, gpointer inUserData)
{
	g_return_if_fail(XFDASHBOARD_IS_WINDOWS_VIEW(inView));

	XfdashboardWindowsView			*view=XFDASHBOARD_WINDOWS_VIEW(inView);

	/* Unmark "view button" in quicklaunch */
	if(XFDASHBOARD_IS_QUICKLAUNCH(inUserData))
	{
		XfdashboardQuicklaunch		*quicklaunch=XFDASHBOARD_QUICKLAUNCH(inUserData);
		
		g_signal_handlers_block_by_func(quicklaunch, (gpointer)(_xfdashboard_on_switch_to_view_clicked), view);
		xfdashboard_quicklaunch_set_mark_state(quicklaunch, FALSE);
		g_signal_handlers_unblock_by_func(quicklaunch, (gpointer)(_xfdashboard_on_switch_to_view_clicked), view);
	}
}

/* Application view was activated */
void _xfdashboard_on_application_view_activated(ClutterActor *inView, gpointer inUserData)
{
	g_return_if_fail(XFDASHBOARD_IS_APPLICATIONS_VIEW(inView));

	/* Check if application view has an active menu. If not set top most menu. */
	XfdashboardApplicationsView		*view=XFDASHBOARD_APPLICATIONS_VIEW(inView);

	if(!xfdashboard_applications_view_get_active_menu(view))
	{
		xfdashboard_applications_view_set_active_menu(view, xfdashboard_get_application_menu());
	}

	/* Mark "view button" in quicklaunch */
	if(XFDASHBOARD_IS_QUICKLAUNCH(inUserData))
	{
		XfdashboardQuicklaunch		*quicklaunch=XFDASHBOARD_QUICKLAUNCH(inUserData);
		
		g_signal_handlers_block_by_func(quicklaunch, (gpointer)(_xfdashboard_on_switch_to_view_clicked), view);
		xfdashboard_quicklaunch_set_mark_state(quicklaunch, TRUE);
		g_signal_handlers_unblock_by_func(quicklaunch, (gpointer)(_xfdashboard_on_switch_to_view_clicked), view);
	}
}

/* A pressed key was released */
gboolean _xfdashboard_on_key_release(ClutterActor *inActor, ClutterEvent *inEvent, gpointer inUserData)
{
	ClutterKeyEvent		*keyEvent=(ClutterKeyEvent*)inEvent;

	/* Handle escape key */
	if(keyEvent->keyval==CLUTTER_Escape)
	{
		/* If search is active end the search now by clearing search box */
		if(!xfdashboard_searchbox_is_empty_text(XFDASHBOARD_SEARCHBOX(searchbox)))
		{
			xfdashboard_searchbox_set_text(XFDASHBOARD_SEARCHBOX(searchbox), NULL);
			return(TRUE);
		}
			/* Quit on ESC without activating any window. Window manager should choose
			 * the most recent window by itself */
			else
			{
				clutter_main_quit();
				return(TRUE);
			}
	}

	/* We did not handle this event so let next in clutter's chain handle it */
	return(FALSE);
}

/* Main entry point */
int main(int argc, char **argv)
{
	ClutterColor			stageColor={ 0, 0, 0, 0xd0 };
	GdkScreen				*screen;
	gint					primary;
	GdkRectangle			primarySize;
	ClutterActor			*view;

	/* Tell clutter to try to initialize an RGBA visual */
	clutter_x11_set_use_argb_visual(TRUE);

	/* Initialize GTK+ and Clutter */
	gtk_init(&argc, &argv);
	if(!clutter_init(&argc, &argv))
	{
		g_error("Initializing clutter failed!");
		return(1);
	}

	/* TODO: Check for running instance (libunique?) */

	/* Get size of primary monitor to set size of stage */
	screen=gdk_screen_get_default();
	primary=gdk_screen_get_primary_monitor(screen);
	gdk_screen_get_monitor_geometry(screen, primary, &primarySize);
	
	/* Create clutter stage */
	stage=clutter_stage_new();
	clutter_stage_set_color(CLUTTER_STAGE(stage), &stageColor);
	clutter_stage_set_use_alpha(CLUTTER_STAGE(stage), TRUE);
	clutter_actor_set_size(stage, primarySize.width, primarySize.height);

	/* Create group holding all actors for stage */
	group=clutter_group_new();
	clutter_actor_set_position(group, 0.0f, spacingToStage);
	clutter_actor_add_constraint(group, clutter_bind_constraint_new(stage, CLUTTER_BIND_WIDTH, 0.0f));
	clutter_actor_add_constraint(group, clutter_bind_constraint_new(stage, CLUTTER_BIND_HEIGHT, -(2*spacingToStage)));
	clutter_container_add_actor(CLUTTER_CONTAINER(stage), group);

	/* TODO: Create background by copying background of Xfce */

	/* Create quicklaunch box and add to box */
	quicklaunch=xfdashboard_quicklaunch_new();
	xfdashboard_quicklaunch_set_mark_icon(XFDASHBOARD_QUICKLAUNCH(quicklaunch), GTK_STOCK_HOME);
	xfdashboard_quicklaunch_set_marked_text(XFDASHBOARD_QUICKLAUNCH(quicklaunch), "Switch to windows");
	xfdashboard_quicklaunch_set_unmarked_text(XFDASHBOARD_QUICKLAUNCH(quicklaunch), "Switch to applications");
	clutter_actor_add_constraint(quicklaunch, clutter_bind_constraint_new(group, CLUTTER_BIND_HEIGHT, 0.0f));
	clutter_container_add_actor(CLUTTER_CONTAINER(group), quicklaunch);

	/* TODO: Remove the following actor(s) for application icons
	 *       in quicklaunch box as soon as xfconf is implemented
	 */
	for(gint i=0; i<(sizeof(quicklaunch_apps)/sizeof(quicklaunch_apps[0])); i++)
	{
		xfdashboard_quicklaunch_add_icon_by_desktop_file(XFDASHBOARD_QUICKLAUNCH(quicklaunch), quicklaunch_apps[i]);
	}

	/* Create search box */
	searchbox=xfdashboard_searchbox_new();
	xfdashboard_searchbox_set_primary_icon(XFDASHBOARD_SEARCHBOX(searchbox), GTK_STOCK_FIND);
	xfdashboard_searchbox_set_secondary_icon(XFDASHBOARD_SEARCHBOX(searchbox), GTK_STOCK_CLEAR);
	xfdashboard_searchbox_set_hint_text(XFDASHBOARD_SEARCHBOX(searchbox), "Just type to search ...");
	clutter_actor_add_constraint(searchbox, clutter_snap_constraint_new(quicklaunch, CLUTTER_SNAP_EDGE_LEFT, CLUTTER_SNAP_EDGE_RIGHT, spacingToStage));
	clutter_actor_add_constraint(searchbox, clutter_snap_constraint_new(group, CLUTTER_SNAP_EDGE_RIGHT, CLUTTER_SNAP_EDGE_RIGHT, -spacingToStage));
	clutter_actor_add_constraint(searchbox, clutter_bind_constraint_new(quicklaunch, CLUTTER_BIND_Y, 0.0f));
	clutter_container_add_actor(CLUTTER_CONTAINER(group), searchbox);
	g_signal_connect(searchbox, "secondary-icon-clicked", G_CALLBACK(_xfdashboard_on_clear_searchbox), NULL);

	/* Create viewpad */
	viewpad=xfdashboard_viewpad_new();
	clutter_actor_add_constraint(viewpad, clutter_snap_constraint_new(quicklaunch, CLUTTER_SNAP_EDGE_LEFT, CLUTTER_SNAP_EDGE_RIGHT, spacingToStage));
	clutter_actor_add_constraint(viewpad, clutter_snap_constraint_new(group, CLUTTER_SNAP_EDGE_RIGHT, CLUTTER_SNAP_EDGE_RIGHT, -spacingToStage));
	clutter_actor_add_constraint(viewpad, clutter_snap_constraint_new(searchbox, CLUTTER_SNAP_EDGE_TOP, CLUTTER_SNAP_EDGE_BOTTOM, spacingToStage));
	clutter_actor_add_constraint(viewpad, clutter_snap_constraint_new(group, CLUTTER_SNAP_EDGE_BOTTOM, CLUTTER_SNAP_EDGE_BOTTOM, -spacingToStage));
	clutter_container_add_actor(CLUTTER_CONTAINER(group), viewpad);

	/* Create views and add them to viewpad */
	view=xfdashboard_windows_view_new();
	xfdashboard_viewpad_add_view(XFDASHBOARD_VIEWPAD(viewpad), XFDASHBOARD_VIEW(view));
	xfdashboard_viewpad_set_active_view(XFDASHBOARD_VIEWPAD(viewpad), XFDASHBOARD_VIEW(view));
	g_signal_connect(view, "activated", G_CALLBACK(_xfdashboard_on_windows_view_activated), quicklaunch);
	g_signal_connect_swapped(quicklaunch, "unmarked", G_CALLBACK(_xfdashboard_on_switch_to_view_clicked), view);

	view=xfdashboard_applications_view_new();
	xfdashboard_viewpad_add_view(XFDASHBOARD_VIEWPAD(viewpad), XFDASHBOARD_VIEW(view));
	g_signal_connect(view, "activated", G_CALLBACK(_xfdashboard_on_application_view_activated), quicklaunch);
	g_signal_connect_swapped(quicklaunch, "marked", G_CALLBACK(_xfdashboard_on_switch_to_view_clicked), view);
	g_signal_connect(searchbox, "search-started", G_CALLBACK(_xfdashboard_on_search_started), view);
	g_signal_connect(searchbox, "text-changed", G_CALLBACK(_xfdashboard_on_search_update), view);

	/* Set up event handlers and connect signals */
	clutter_stage_set_key_focus(CLUTTER_STAGE(stage), searchbox);

	signalStageDestroyID=g_signal_connect(stage, "destroy", G_CALLBACK(clutter_main_quit), NULL);
	g_signal_connect(stage, "unfullscreen", G_CALLBACK(clutter_main_quit), NULL);
	g_signal_connect(stage, "key-release-event", G_CALLBACK(_xfdashboard_on_key_release), NULL);

	/* Show stage and go ;) */
	clutter_actor_show(stage);
	clutter_stage_set_fullscreen(CLUTTER_STAGE(stage), TRUE);

	clutter_main();

	if(signalStageDestroyID &&
		g_signal_handler_is_connected(stage, signalStageDestroyID))
	{
		g_signal_handler_disconnect(stage, signalStageDestroyID);
		signalStageDestroyID=0L;
	}
	clutter_actor_destroy(stage);

	return(0);
}
