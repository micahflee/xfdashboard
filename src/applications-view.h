/*
 * applications-view.h: A view showing all installed applications as menu
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

#ifndef __XFDASHBOARD_APPLICATIONS_VIEW__
#define __XFDASHBOARD_APPLICATIONS_VIEW__

#include <garcon/garcon.h>

#include "view.h"
#include "common.h"

G_BEGIN_DECLS

#define XFDASHBOARD_TYPE_APPLICATIONS_VIEW				(xfdashboard_applications_view_get_type())
#define XFDASHBOARD_APPLICATIONS_VIEW(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), XFDASHBOARD_TYPE_APPLICATIONS_VIEW, XfdashboardApplicationsView))
#define XFDASHBOARD_IS_APPLICATIONS_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), XFDASHBOARD_TYPE_APPLICATIONS_VIEW))
#define XFDASHBOARD_APPLICATIONS_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), XFDASHBOARD_TYPE_APPLICATIONS_VIEW, XfdashboardApplicationsViewClass))
#define XFDASHBOARD_IS_APPLICATIONS_VIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), XFDASHBOARD_TYPE_APPLICATIONS_VIEW))
#define XFDASHBOARD_APPLICATIONS_VIEW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), XFDASHBOARD_TYPE_APPLICATIONS_VIEW, XfdashboardApplicationsViewClass))

typedef struct _XfdashboardApplicationsView				XfdashboardApplicationsView; 
typedef struct _XfdashboardApplicationsViewPrivate		XfdashboardApplicationsViewPrivate;
typedef struct _XfdashboardApplicationsViewClass		XfdashboardApplicationsViewClass;

struct _XfdashboardApplicationsView
{
	/* Parent instance */
	XfdashboardView						parent_instance;

	/* Private structure */
	XfdashboardApplicationsViewPrivate	*priv;
};

struct _XfdashboardApplicationsViewClass
{
	/* Parent class */
	XfdashboardViewClass				parent_class;
};

/* Public API */
GType xfdashboard_applications_view_get_type(void) G_GNUC_CONST;

ClutterActor* xfdashboard_applications_view_new();

XfdashboardViewMode xfdashboard_applications_view_get_list_mode(XfdashboardApplicationsView *self);
void xfdashboard_applications_view_set_list_mode(XfdashboardApplicationsView *self, XfdashboardViewMode inListMode);

const GarconMenu* xfdashboard_applications_view_get_active_menu(XfdashboardApplicationsView *self);
void xfdashboard_applications_view_set_active_menu(XfdashboardApplicationsView *self, const GarconMenu *inMenu);

const gchar* xfdashboard_applications_view_get_filter_text(XfdashboardApplicationsView *self);
void xfdashboard_applications_view_set_filter_text(XfdashboardApplicationsView *self, const gchar *inFilterText);

G_END_DECLS

#endif
