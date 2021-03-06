/*
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <gdk/gdkkeysyms.h>
#include <xfconf/xfconf.h>
#include <glib/gstdio.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#include <src/appfinder-window.h>
#include <src/appfinder-model.h>
#include <src/appfinder-category-model.h>
#include <src/appfinder-preferences.h>
#include <src/appfinder-actions.h>
#include <src/appfinder-private.h>

#include <firejail/exechelper.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#define APPFINDER_WIDGET_XID(widget) ((guint) GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (widget))))
#else
#define APPFINDER_WIDGET_XID(widget) (0)
#endif

#define DEFAULT_WINDOW_WIDTH   400
#define DEFAULT_WINDOW_HEIGHT  400
#define DEFAULT_PANED_POSITION 180



static void       xfce_appfinder_window_finalize                      (GObject                     *object);
static void       xfce_appfinder_window_unmap                         (GtkWidget                   *widget);
static gboolean   xfce_appfinder_window_key_press_event               (GtkWidget                   *widget,
                                                                       GdkEventKey                 *event);
static gboolean   xfce_appfinder_window_window_state_event            (GtkWidget                   *widget,
                                                                       GdkEventWindowState         *event);
static void       xfce_appfinder_window_view                          (XfceAppfinderWindow         *window);
static gboolean   xfce_appfinder_window_popup_menu                    (GtkWidget                   *view,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_set_padding                   (GtkWidget                   *entry,
                                                                       GtkWidget                   *align);
static gboolean   xfce_appfinder_window_completion_match_func         (GtkEntryCompletion          *completion,
                                                                       const gchar                 *key,
                                                                       GtkTreeIter                 *iter,
                                                                       gpointer                     data);
static void       xfce_appfinder_window_entry_changed                 (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_entry_activate                (GtkEditable                 *entry,
                                                                       XfceAppfinderWindow         *window);
static gboolean   xfce_appfinder_window_entry_key_press_event         (GtkWidget                   *entry,
                                                                       GdkEventKey                 *event,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_entry_icon_released           (GtkEntry                    *entry,
                                                                       GtkEntryIconPosition         icon_pos,
                                                                       GdkEvent                    *event,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_drag_begin                    (GtkWidget                   *widget,
                                                                       GdkDragContext              *drag_context,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_drag_data_get                 (GtkWidget                   *widget,
                                                                       GdkDragContext              *drag_context,
                                                                       GtkSelectionData            *data,
                                                                       guint                        info,
                                                                       guint                        drag_time,
                                                                       XfceAppfinderWindow         *window);
static gboolean   xfce_appfinder_window_treeview_key_press_event      (GtkWidget                   *widget,
                                                                       GdkEventKey                 *event,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_category_changed              (GtkTreeSelection            *selection,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_category_set_categories       (XfceAppfinderModel          *model,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_preferences                   (GtkWidget                   *button,
                                                                       XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_property_changed              (XfconfChannel               *channel,
                                                                       const gchar                 *prop,
                                                                       const GValue                *value,
                                                                       XfceAppfinderWindow         *window);
static gboolean   xfce_appfinder_window_item_visible                  (GtkTreeModel                *model,
                                                                       GtkTreeIter                 *iter,
                                                                       gpointer                     data);
static gboolean   xfce_appfinder_window_view_get_selected             (XfceAppfinderWindow         *window,
                                                                       GtkTreeModel               **model,
                                                                       GtkTreeIter                 *iter);
static void       xfce_appfinder_window_item_changed                  (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_row_activated                 (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_icon_theme_changed            (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_launch_sandboxed_clicked      (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_launch_sandboxed_real_clicked (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_launch_sandboxed_back_clicked (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_launch_clicked                (XfceAppfinderWindow         *window);
static void       xfce_appfinder_window_execute                       (XfceAppfinderWindow         *window,
                                                                       gboolean                     close_on_succeed,
                                                                       gboolean                     sandboxed,
                                                                       const gchar                 *profile);
                                                                       
static void       populate_profile_box                                (GtkWidget                   *widget);



struct _XfceAppfinderWindowClass
{
  GtkWindowClass __parent__;
};

struct _XfceAppfinderWindow
{
  GtkWindow __parent__;

  XfceAppfinderModel         *model;

  XfceAppfinderCategoryModel *category_model;

  XfceAppfinderActions       *actions;

  GtkIconTheme               *icon_theme;

  GtkEntryCompletion         *completion;

  XfconfChannel              *channel;

  GtkWidget                  *paned;
  GtkWidget                  *entry;
  GtkWidget                  *image;
  GtkWidget                  *secure_ws_info;
  GtkWidget                  *message_label;
  GtkWidget                  *view;
  GtkWidget                  *viewscroll;
  GtkWidget                  *sidepane;

  GdkPixbuf                  *icon_find;

  GtkWidget                  *bbox;
  GtkWidget                  *button_launch;
  GtkWidget                  *button_launch_sandboxed;
  GtkWidget                  *button_launch_sandboxed_real;
  GtkWidget                  *button_launch_sandboxed_back;
  GtkWidget                  *button_preferences;
  GtkWidget                  *button_close;
  GtkWidget                  *profile_box;;
  GtkWidget                  *profile_label;
  GtkWidget                  *bin_collapsed;
  GtkWidget                  *bin_expanded;

  GarconMenuDirectory        *filter_category;
  gchar                      *filter_text;

  guint                       idle_entry_changed_id;

  gint                        last_window_height;

  gulong                      property_watch_id;
  gulong                      categories_changed_id;

  WnckScreen                 *wnck_screen;
  gulong                      active_workspace_changed_id;
};

static const GtkTargetEntry target_list[] =
{
  { "text/uri-list", 0, 0 }
};



G_DEFINE_TYPE (XfceAppfinderWindow, xfce_appfinder_window, GTK_TYPE_WINDOW)



static void
xfce_appfinder_window_class_init (XfceAppfinderWindowClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_appfinder_window_finalize;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->unmap = xfce_appfinder_window_unmap;
  gtkwidget_class->key_press_event = xfce_appfinder_window_key_press_event;
  gtkwidget_class->window_state_event = xfce_appfinder_window_window_state_event;
}



void
xfce_appfinder_update_button_labels (XfceAppfinderWindow *window)
{
  WnckWorkspace       *active_ws;
  gint                 active_n;
  GtkWidget           *image;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  /* get the active workspace if it exists */
  active_ws = wnck_screen_get_active_workspace (window->wnck_screen);
  active_n = active_ws? wnck_workspace_get_number (active_ws) : -1;

  /* not a secure workspace */
  if (active_n < 0 || !xfce_workspace_is_secure (active_n))
    {
      GarconMenuItem *item;
      GtkTreeModel   *model;
      GtkTreeIter     iter;
      gchar          *cmd = NULL;

      /* get command */
      if (gtk_widget_get_visible (window->paned))
        {
          if (xfce_appfinder_window_view_get_selected (window, &model, &iter))
            gtk_tree_model_get (model, &iter, XFCE_APPFINDER_MODEL_COLUMN_COMMAND, &cmd, -1);
        }
      else
          cmd = g_strdup (gtk_entry_get_text (GTK_ENTRY (window->entry)));

      /* get corresponding Garcon item and check if sandboxed */
      item = xfce_appfinder_model_get_item_for_command (window->model, cmd);
      g_free (cmd);
      if (item && garcon_menu_item_get_sandboxed (item))
        {
          /* make explicit reference to fact that app is sandboxed */
          gtk_button_set_label (GTK_BUTTON (window->button_launch), _("La_unch Sandboxed App"));
          image = gtk_image_new_from_icon_name ("firejail-run", GTK_ICON_SIZE_BUTTON);
          gtk_button_set_image (GTK_BUTTON (window->button_launch), image);

          /* hide the "Launch in Secure WS" button */
          gtk_widget_set_visible (window->button_launch_sandboxed, FALSE);
        }
      else
        {
          /* normal launch button */
          gtk_button_set_label (GTK_BUTTON (window->button_launch), _("La_unch"));
          image = gtk_image_new_from_stock (GTK_STOCK_EXECUTE, GTK_ICON_SIZE_BUTTON);
          gtk_button_set_image (GTK_BUTTON (window->button_launch), image);

          /* add an explicit "Launch in Sandbox" button */
          gtk_widget_set_visible (window->button_launch_sandboxed,
                xfconf_channel_get_bool (window->channel, "/show-sandboxed", TRUE));
        }

      if (item)
        g_object_unref (item);

      /* no longer in a secure WS, hide the secure WS notification */
      gtk_widget_set_visible (window->secure_ws_info, FALSE);
    }
  /* secure workspace, make sure it's clear sandboxing will occur */
  else
    {
      /* basic button makes explicit reference to secure workspace */
      gchar *label = g_strdup_printf (_("La_unch in Secure Workspace %d"), active_n + 1);
      gchar *ws_name = xfce_workspace_get_workspace_name (active_n);

      /* deal with workspace changes when in the profile selection mode of "Launch with Sanndbox" */
      if (gtk_widget_get_visible (window->button_launch_sandboxed_back))
        {
          gtk_button_clicked (GTK_BUTTON (window->button_launch_sandboxed_back));
          gtk_widget_set_visible (window->button_launch_sandboxed, FALSE);
        }

      gtk_button_set_label (GTK_BUTTON (window->button_launch), label);
      g_free (label);
      image = gtk_image_new_from_icon_name ("firejail-run", GTK_ICON_SIZE_BUTTON);
      gtk_button_set_image (GTK_BUTTON (window->button_launch), image);

      /* conflicts with the "Launch in Secure WS" button */
      gtk_widget_set_visible (window->button_launch_sandboxed, FALSE);

      /* high-visibility status notification */
      gtk_widget_set_visible (window->secure_ws_info, TRUE);
      label = g_strdup_printf (_("<b>Workspace %d is a Secure Workspace. Your app will run inside the '%s' sandbox.</b>"), active_n + 1, ws_name);
      gtk_label_set_markup (GTK_LABEL (window->message_label), label);
      g_free (label);
      g_free (ws_name);
    }
}



static void
xfce_appfinder_window_on_active_workspace_changed (WnckScreen    *screen,
                                                   WnckWorkspace *previously_active_space,
                                                   gpointer       user_data)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (user_data);
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  xfce_appfinder_update_button_labels (window);
}



static void
xfce_appfinder_window_on_screen_changed (GtkWidget *widget,
                                         GdkScreen *previous_screen)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (widget);
  GdkScreen           *screen;
  WnckScreen          *wnck_screen;

  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  screen = gtk_widget_get_screen (widget);
  wnck_screen = wnck_screen_get (gdk_screen_get_number (screen));

  if (window->wnck_screen != wnck_screen)
    {
      if (window->active_workspace_changed_id)
        {
          g_signal_handler_disconnect (window->wnck_screen, window->active_workspace_changed_id);
          window->active_workspace_changed_id = 0;
        }

      window->wnck_screen = wnck_screen;

      window->active_workspace_changed_id = g_signal_connect (G_OBJECT (window->wnck_screen),
                                                              "active-workspace-changed",
                                                              G_CALLBACK (xfce_appfinder_window_on_active_workspace_changed),
                                                              window);
      xfce_appfinder_window_on_active_workspace_changed (window->wnck_screen,
                                                         wnck_screen_get_active_workspace (window->wnck_screen),
                                                         window);
    }
}



static void
xfce_appfinder_window_init (XfceAppfinderWindow *window)
{
  GtkWidget          *vbox, *vbox2;
  GtkWidget          *entry;
  GtkWidget          *pane;
  GtkWidget          *scroll;
  GtkWidget          *sidepane;
  GtkWidget          *image;
  GtkWidget          *hbox;
  GtkWidget          *align;
  GtkWidget          *infobar;
  GtkWidget          *label;
  GtkWidget          *content_area;
  GtkTreeViewColumn  *column;
  GtkCellRenderer    *renderer;
  GtkTreeSelection   *selection;
  GtkWidget          *bbox;
  GtkWidget          *button;
  GtkEntryCompletion *completion;
  gint                integer;

  window->channel = xfconf_channel_get ("xfce4-appfinder");
  window->last_window_height = xfconf_channel_get_int (window->channel, "/last/window-height", DEFAULT_WINDOW_HEIGHT);

  window->category_model = xfce_appfinder_category_model_new ();
  xfconf_g_property_bind (window->channel, "/category-icon-size", G_TYPE_UINT,
                          G_OBJECT (window->category_model), "icon-size");

  window->model = xfce_appfinder_model_get ();
  xfconf_g_property_bind (window->channel, "/item-icon-size", G_TYPE_UINT,
                          G_OBJECT (window->model), "icon-size");

  gtk_window_set_title (GTK_WINDOW (window), _("Application Finder"));
  integer = xfconf_channel_get_int (window->channel, "/last/window-width", DEFAULT_WINDOW_WIDTH);
  gtk_window_set_default_size (GTK_WINDOW (window), integer, -1);
  gtk_window_set_icon_name (GTK_WINDOW (window), GTK_STOCK_EXECUTE);
#if GTK_CHECK_VERSION (3, 0, 0)
  gtk_window_set_has_resize_grip (GTK_WINDOW (window), FALSE);
#endif

  if (xfconf_channel_get_bool (window->channel, "/always-center", FALSE))
    gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);

#if GTK_CHECK_VERSION (3, 0, 0)
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
#else
  vbox = gtk_vbox_new (FALSE, 6);
#endif
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_widget_show (vbox);

  align = gtk_alignment_new (0.5, 0.0, 1.0, 1.0);
  gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);
  gtk_widget_show (align);

  window->secure_ws_info = infobar = gtk_info_bar_new ();
  gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), GTK_MESSAGE_INFO);
#if GTK_CHECK_VERSION (3, 0, 0)
  gtk_info_bar_set_show_close_button (GTK_INFO_BAR (infobar), TRUE);
#endif
  gtk_container_add (GTK_CONTAINER (align), window->secure_ws_info);
  gtk_widget_show (infobar);

#if GTK_CHECK_VERSION (3, 0, 0)
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
#else
  hbox = gtk_hbox_new (FALSE, 6);
#endif
  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar));
  gtk_container_add (GTK_CONTAINER (content_area), hbox);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name ("firejail-workspaces", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_size_request (image, 24, 24);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  window->message_label = label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), _("<b>This is a Secure Workspace</b>"));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

#if GTK_CHECK_VERSION (3, 0, 0)
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
#else
  hbox = gtk_hbox_new (FALSE, 6);
#endif
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  align = gtk_alignment_new (0.5, 0.0, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
  gtk_widget_show (align);

  window->icon_find = xfce_appfinder_model_load_pixbuf (GTK_STOCK_FIND, XFCE_APPFINDER_ICON_SIZE_48);
  window->image = image = gtk_image_new_from_pixbuf (window->icon_find);
  gtk_widget_set_size_request (image, 48, 48);
  gtk_container_add (GTK_CONTAINER (align), image);
  gtk_widget_show (image);

#if GTK_CHECK_VERSION (3, 0, 0)
  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
#else
  vbox2 = gtk_vbox_new (FALSE, 6);
#endif
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);
  gtk_widget_show (vbox2);

  align = gtk_alignment_new (0.0, 0.0, 1.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox2), align, TRUE, TRUE, 0);
  gtk_widget_show (align);

  window->entry = entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (align), entry);
  g_signal_connect (G_OBJECT (entry), "icon-release",
      G_CALLBACK (xfce_appfinder_window_entry_icon_released), window);
  g_signal_connect (G_OBJECT (entry), "realize",
      G_CALLBACK (xfce_appfinder_window_set_padding), align);
  g_signal_connect_swapped (G_OBJECT (entry), "changed",
      G_CALLBACK (xfce_appfinder_window_entry_changed), window);
  g_signal_connect (G_OBJECT (entry), "activate",
      G_CALLBACK (xfce_appfinder_window_entry_activate), window);
  g_signal_connect (G_OBJECT (entry), "key-press-event",
      G_CALLBACK (xfce_appfinder_window_entry_key_press_event), window);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     GTK_STOCK_GO_DOWN);
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
                                   GTK_ENTRY_ICON_SECONDARY,
                                   _("Toggle view mode"));
  gtk_widget_show (entry);

  window->completion = completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (window->model));
  gtk_entry_completion_set_match_func (completion, xfce_appfinder_window_completion_match_func, window, NULL);
  gtk_entry_completion_set_text_column (completion, XFCE_APPFINDER_MODEL_COLUMN_COMMAND);
  gtk_entry_completion_set_popup_completion (completion, TRUE);
  gtk_entry_completion_set_popup_single_match (completion, FALSE);
  gtk_entry_completion_set_inline_completion (completion, TRUE);

  window->bin_collapsed = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
  gtk_box_pack_start (GTK_BOX (vbox2), window->bin_collapsed, FALSE, TRUE, 0);
  gtk_widget_show (window->bin_collapsed);

#if GTK_CHECK_VERSION (3, 0, 0)
  window->paned = pane = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
#else
  window->paned = pane = gtk_hpaned_new ();
#endif
  gtk_box_pack_start (GTK_BOX (vbox), pane, TRUE, TRUE, 0);
  integer = xfconf_channel_get_int (window->channel, "/last/pane-position", DEFAULT_PANED_POSITION);
  gtk_paned_set_position (GTK_PANED (pane), integer);
  g_object_set (G_OBJECT (pane), "position-set", TRUE, NULL);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_paned_add1 (GTK_PANED (pane), scroll);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scroll);

  sidepane = window->sidepane = gtk_tree_view_new_with_model (GTK_TREE_MODEL (window->category_model));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (sidepane), FALSE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (sidepane), FALSE);
  g_signal_connect_swapped (G_OBJECT (sidepane), "start-interactive-search",
      G_CALLBACK (gtk_widget_grab_focus), entry);
  g_signal_connect (G_OBJECT (sidepane), "key-press-event",
      G_CALLBACK (xfce_appfinder_window_treeview_key_press_event), window);
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (sidepane),
      xfce_appfinder_category_model_row_separator_func, NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scroll), sidepane);
  gtk_widget_show (sidepane);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sidepane));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (selection), "changed",
      G_CALLBACK (xfce_appfinder_window_category_changed), window);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (sidepane), GTK_TREE_VIEW_COLUMN (column));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, FALSE);
  gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
                                       "pixbuf", XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_ICON, NULL);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
  gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
                                       "text", XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_NAME, NULL);

  window->viewscroll = scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_paned_add2 (GTK_PANED (pane), scroll);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scroll);

  /* set the icon or tree view */
  xfce_appfinder_window_view (window);

  window->bin_expanded = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
  gtk_box_pack_start (GTK_BOX (vbox), window->bin_expanded, FALSE, TRUE, 0);
  gtk_widget_show (window->bin_expanded);

#if GTK_CHECK_VERSION (3, 0, 0)
  window->bbox = hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
#else
  window->bbox = hbox = gtk_hbox_new (FALSE, 6);
#endif
  gtk_widget_show (hbox);

  /* preferences button */
  window->button_preferences = button = gtk_button_new_from_stock (GTK_STOCK_PREFERENCES);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
      G_CALLBACK (xfce_appfinder_window_preferences), window);

#if GTK_CHECK_VERSION (3, 0, 0)
  bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
#else
  bbox = gtk_hbox_new (FALSE, 6);
#endif
  gtk_box_pack_start (GTK_BOX (hbox), bbox, TRUE, TRUE, 0);
  gtk_widget_show (bbox);

  /* launch button */
  window->button_launch = button = gtk_button_new_with_mnemonic (_("La_unch"));
  gtk_box_pack_end (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
      G_CALLBACK (xfce_appfinder_window_launch_clicked), window);
  gtk_widget_set_sensitive (button, FALSE);
  gtk_widget_show (button);

  image = gtk_image_new_from_stock (GTK_STOCK_EXECUTE, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  /* launch sandboxed real button */
  window->button_launch_sandboxed_real = button = gtk_button_new_with_mnemonic (_("La_unch"));
  gtk_box_pack_end (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
      G_CALLBACK (xfce_appfinder_window_launch_sandboxed_real_clicked), window);
  gtk_widget_show (button);

  image = gtk_image_new_from_icon_name ("firejail-run", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  /* profile selection combo box */
  window->profile_box = gtk_combo_box_text_new ();
  gtk_box_pack_end (GTK_BOX (bbox), window->profile_box, FALSE, FALSE, 0);
  populate_profile_box (window->profile_box);

  window->profile_label = gtk_label_new (_("select profile:"));
  gtk_box_pack_end (GTK_BOX (bbox), window->profile_label, FALSE, FALSE, 0);

  /* launch sandboxed back button */
  window->button_launch_sandboxed_back = button = gtk_button_new_with_mnemonic (_("_Back"));
  gtk_box_pack_end (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
      G_CALLBACK (xfce_appfinder_window_launch_sandboxed_back_clicked), window);
  gtk_widget_show (button);

  image = gtk_image_new_from_stock (GTK_STOCK_GO_BACK, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);

  /* launch sandboxed button */
  window->button_launch_sandboxed = button = gtk_button_new_with_mnemonic (_("Launch _Sandboxed"));
  gtk_box_pack_end (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
      G_CALLBACK (xfce_appfinder_window_launch_sandboxed_clicked), window);
  gtk_widget_set_sensitive (button, FALSE);
  gtk_widget_set_visible (button, xfconf_channel_get_bool (window->channel, "/show-sandboxed", TRUE));

  image = gtk_image_new_from_icon_name ("firejail-run", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  
  /* close button */
  window->button_close = button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_box_pack_end (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
      G_CALLBACK (gtk_widget_destroy), window);
  gtk_widget_show (button);

  /* hide the Launch Sandboxed options for now */
  xfce_appfinder_window_launch_sandboxed_back_clicked (window);

  /* update button labels based on whether in a secure workspace */
  window->wnck_screen = NULL;
  g_signal_connect (G_OBJECT (window), "screen-changed", G_CALLBACK (xfce_appfinder_window_on_screen_changed), NULL);
  xfce_appfinder_window_on_screen_changed (GTK_WIDGET (window), NULL);

  /* load icon theme */
  window->icon_theme = gtk_icon_theme_get_for_screen (gtk_window_get_screen (GTK_WINDOW (window)));
  g_signal_connect_swapped (G_OBJECT (window->icon_theme), "changed",
      G_CALLBACK (xfce_appfinder_window_icon_theme_changed), window);
  g_object_ref (G_OBJECT (window->icon_theme));

  /* load categories in the model */
  xfce_appfinder_window_category_set_categories (NULL, window);
  window->categories_changed_id =
      g_signal_connect (G_OBJECT (window->model), "categories-changed",
                        G_CALLBACK (xfce_appfinder_window_category_set_categories),
                        window);

  /* monitor xfconf property changes */
  window->property_watch_id =
    g_signal_connect (G_OBJECT (window->channel), "property-changed",
        G_CALLBACK (xfce_appfinder_window_property_changed), window);

  APPFINDER_DEBUG ("constructed window");
}



static void
xfce_appfinder_window_finalize (GObject *object)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (object);

  if (window->idle_entry_changed_id != 0)
    g_source_remove (window->idle_entry_changed_id);

  g_signal_handler_disconnect (window->channel, window->property_watch_id);
  g_signal_handler_disconnect (window->model, window->categories_changed_id);

  /* release our reference on the icon theme */
  g_signal_handlers_disconnect_by_func (G_OBJECT (window->icon_theme),
      xfce_appfinder_window_icon_theme_changed, window);
  g_object_unref (G_OBJECT (window->icon_theme));

  g_object_unref (G_OBJECT (window->model));
  g_object_unref (G_OBJECT (window->category_model));
  g_object_unref (G_OBJECT (window->completion));
  g_object_unref (G_OBJECT (window->icon_find));

  if (window->actions != NULL)
    g_object_unref (G_OBJECT (window->actions));

  if (window->filter_category != NULL)
    g_object_unref (G_OBJECT (window->filter_category));
  g_free (window->filter_text);

  if (window->active_workspace_changed_id)
    {
      g_signal_handler_disconnect (window->wnck_screen, window->active_workspace_changed_id);
      window->active_workspace_changed_id = 0;
    }

  (*G_OBJECT_CLASS (xfce_appfinder_window_parent_class)->finalize) (object);
}



static void
xfce_appfinder_window_unmap (GtkWidget *widget)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (widget);
  gint                 width, height;
  gint                 position;

  position = gtk_paned_get_position (GTK_PANED (window->paned));
  gtk_window_get_size (GTK_WINDOW (window), &width, &height);
  if (!gtk_widget_get_visible (window->paned))
    height = window->last_window_height;

  (*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->unmap) (widget);

  xfconf_channel_set_int (window->channel, "/last/window-height", height);
  xfconf_channel_set_int (window->channel, "/last/window-width", width);
  xfconf_channel_set_int (window->channel, "/last/pane-position", position);

  return (*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->unmap) (widget);
}



static gboolean
xfce_appfinder_window_key_press_event (GtkWidget   *widget,
                                       GdkEventKey *event)
{
  XfceAppfinderWindow   *window = XFCE_APPFINDER_WINDOW (widget);
  GtkWidget             *entry;
  XfceAppfinderIconSize  icon_size = XFCE_APPFINDER_ICON_SIZE_DEFAULT_ITEM;

  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_widget_destroy (widget);
      return TRUE;
    }
  else if ((event->state & GDK_CONTROL_MASK) != 0)
    {
      switch (event->keyval)
        {
        case GDK_KEY_l:
          entry = XFCE_APPFINDER_WINDOW (widget)->entry;

          gtk_widget_grab_focus (entry);
          gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

          return TRUE;

        case GDK_KEY_1:
        case GDK_KEY_2:
          /* toggle between icon and tree view */
          xfconf_channel_set_bool (window->channel, "/icon-view",
                                   event->keyval == GDK_KEY_1);
          return TRUE;

        case GDK_KEY_plus:
        case GDK_KEY_minus:
        case GDK_KEY_KP_Add:
        case GDK_KEY_KP_Subtract:
          g_object_get (G_OBJECT (window->model), "icon-size", &icon_size, NULL);
          if ((event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_KP_Add))
            {
              if (icon_size < XFCE_APPFINDER_ICON_SIZE_LARGEST)
                icon_size++;
            }
          else if (icon_size > XFCE_APPFINDER_ICON_SIZE_SMALLEST)
            {
              icon_size--;
            }

        case GDK_KEY_0:
        case GDK_KEY_KP_0:
          g_object_set (G_OBJECT (window->model), "icon-size", icon_size, NULL);
          return TRUE;
        }
    }

  return  (*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->key_press_event) (widget, event);
}



static gboolean
xfce_appfinder_window_window_state_event (GtkWidget           *widget,
                                          GdkEventWindowState *event)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (widget);
  gint                 width;

  if (!gtk_widget_get_visible (window->paned))
    {
      if ((event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0)
        {
          gtk_window_unmaximize (GTK_WINDOW (widget));

          /* set sensible width instead of taking entire width */
          width = xfconf_channel_get_int (window->channel, "/last/window-width", DEFAULT_WINDOW_WIDTH);
          gtk_window_resize (GTK_WINDOW (widget), width, 100 /* should be corrected by wm */);
        }

      if ((event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0)
        gtk_window_unfullscreen (GTK_WINDOW (widget));
    }

  if ((*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->window_state_event) != NULL)
    return (*GTK_WIDGET_CLASS (xfce_appfinder_window_parent_class)->window_state_event) (widget, event);

  return FALSE;
}



static void
xfce_appfinder_window_set_item_width (XfceAppfinderWindow *window)
{
  gint                   width = 16;
  XfceAppfinderIconSize  icon_size;
  GtkOrientation         item_orientation = GTK_ORIENTATION_VERTICAL;
  GList                 *renderers;
  GList                 *li;

  appfinder_return_if_fail (GTK_IS_ICON_VIEW (window->view));

  g_object_get (G_OBJECT (window->model), "icon-size", &icon_size, NULL);

  /* some hard-coded values for the cell size that seem to work fine */
  switch (icon_size)
    {
    case XFCE_APPFINDER_ICON_SIZE_SMALLEST:
      width = 16 * 3.75;
      break;

    case XFCE_APPFINDER_ICON_SIZE_SMALLER:
      width = 24 * 3;
      break;

    case XFCE_APPFINDER_ICON_SIZE_SMALL:
      width = 36 * 2.5;
      break;

    case XFCE_APPFINDER_ICON_SIZE_NORMAL:
      width = 48 * 2;
      break;

    case XFCE_APPFINDER_ICON_SIZE_LARGE:
      width = 64 * 1.5;
      break;

    case XFCE_APPFINDER_ICON_SIZE_LARGER:
      width = 96 * 1.75;
      break;

    case XFCE_APPFINDER_ICON_SIZE_LARGEST:
      width = 128 * 1.25;
      break;
    }

  if (xfconf_channel_get_bool (window->channel, "/text-beside-icons", FALSE))
    {
      item_orientation = GTK_ORIENTATION_HORIZONTAL;
      width *= 2;
    }

#if GTK_CHECK_VERSION (2, 22, 0)
  gtk_icon_view_set_item_orientation (GTK_ICON_VIEW (window->view), item_orientation);
#else
  gtk_icon_view_set_orientation (GTK_ICON_VIEW (window->view), item_orientation);
#endif
  gtk_icon_view_set_item_width (GTK_ICON_VIEW (window->view), width);

  if (item_orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* work around the yalign = 0.0 gtk uses */
      renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (window->view));
      for (li = renderers; li != NULL; li = li->next)
        g_object_set (li->data, "yalign", 0.5, NULL);
      g_list_free (renderers);
    }
}



static gboolean
xfce_appfinder_window_view_button_press_event (GtkWidget           *widget,
                                               GdkEventButton      *event,
                                               XfceAppfinderWindow *window)
{
  gint         x, y;
  GtkTreePath *path;
  gboolean     have_selection = FALSE;

  if (event->button == 3
      && event->type == GDK_BUTTON_PRESS)
    {
      if (GTK_IS_TREE_VIEW (widget))
        {
          gtk_tree_view_convert_widget_to_bin_window_coords (GTK_TREE_VIEW (widget),
                                                             event->x, event->y, &x, &y);

          if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget), x, y, &path, NULL, NULL, NULL))
            {
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (widget), path, NULL, FALSE);
              gtk_tree_path_free (path);
              have_selection = TRUE;
            }
        }
      else
        {
          path = gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (widget), event->x, event->y);
          if (path != NULL)
            {
              gtk_icon_view_select_path (GTK_ICON_VIEW (widget), path);
              gtk_icon_view_set_cursor (GTK_ICON_VIEW (widget), path, NULL, FALSE);
              gtk_tree_path_free (path);
              have_selection = TRUE;
            }
        }

      if (have_selection)
        return xfce_appfinder_window_popup_menu (widget, window);
    }

  return FALSE;
}



static void
xfce_appfinder_window_view (XfceAppfinderWindow *window)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeModel      *filter_model;
  GtkTreeSelection  *selection;
  GtkWidget         *view;
  gboolean           icon_view;

  icon_view = xfconf_channel_get_bool (window->channel, "/icon-view", FALSE);
  if (window->view != NULL)
    {
      if (icon_view && GTK_IS_ICON_VIEW (window->view))
        return;
      gtk_widget_destroy (window->view);
    }

  filter_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (window->model), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter_model), xfce_appfinder_window_item_visible, window, NULL);

  if (icon_view)
    {
      window->view = view = gtk_icon_view_new_with_model (filter_model);
      gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (view), GTK_SELECTION_BROWSE);
      gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_ICON);
      gtk_icon_view_set_text_column (GTK_ICON_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_TITLE);
      gtk_icon_view_set_tooltip_column (GTK_ICON_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_TOOLTIP);
      xfce_appfinder_window_set_item_width (window);

      g_signal_connect_swapped (G_OBJECT (view), "selection-changed",
          G_CALLBACK (xfce_appfinder_window_item_changed), window);
      g_signal_connect_swapped (G_OBJECT (view), "item-activated",
          G_CALLBACK (xfce_appfinder_window_row_activated), window);
    }
  else
    {
      window->view = view = gtk_tree_view_new_with_model (filter_model);
      gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
      gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view), FALSE);
      gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_TOOLTIP);
      g_signal_connect_swapped (G_OBJECT (view), "row-activated",
          G_CALLBACK (xfce_appfinder_window_row_activated), window);
      g_signal_connect_swapped (G_OBJECT (view), "start-interactive-search",
          G_CALLBACK (gtk_widget_grab_focus), window->entry);
      g_signal_connect (G_OBJECT (view), "key-press-event",
           G_CALLBACK (xfce_appfinder_window_treeview_key_press_event), window);

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
      gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
      g_signal_connect_swapped (G_OBJECT (selection), "changed",
          G_CALLBACK (xfce_appfinder_window_item_changed), window);

      column = gtk_tree_view_column_new ();
      gtk_tree_view_column_set_spacing (column, 2);
      gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
      gtk_tree_view_append_column (GTK_TREE_VIEW (view), GTK_TREE_VIEW_COLUMN (column));

      renderer = gtk_cell_renderer_pixbuf_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, FALSE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
                                      "pixbuf", XFCE_APPFINDER_MODEL_COLUMN_ICON, NULL);

      renderer = gtk_cell_renderer_text_new ();
      g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (column), renderer,
                                      "markup", XFCE_APPFINDER_MODEL_COLUMN_ABSTRACT, NULL);
    }

  gtk_drag_source_set (view, GDK_BUTTON1_MASK, target_list,
                       G_N_ELEMENTS (target_list), GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (view), "drag-begin",
      G_CALLBACK (xfce_appfinder_window_drag_begin), window);
  g_signal_connect (G_OBJECT (view), "drag-data-get",
      G_CALLBACK (xfce_appfinder_window_drag_data_get), window);
  g_signal_connect (G_OBJECT (view), "popup-menu",
      G_CALLBACK (xfce_appfinder_window_popup_menu), window);
  g_signal_connect (G_OBJECT (view), "button-press-event",
      G_CALLBACK (xfce_appfinder_window_view_button_press_event), window);
  gtk_container_add (GTK_CONTAINER (window->viewscroll), view);
  gtk_widget_show (view);

  g_object_unref (G_OBJECT (filter_model));
}



static gboolean
xfce_appfinder_window_view_get_selected (XfceAppfinderWindow  *window,
                                         GtkTreeModel        **model,
                                         GtkTreeIter          *iter)
{
  GtkTreeSelection *selection;
  gboolean          have_iter;
  GList            *items;

  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_WINDOW (window), FALSE);
  appfinder_return_val_if_fail (model != NULL, FALSE);
  appfinder_return_val_if_fail (iter != NULL, FALSE);

  if (GTK_IS_TREE_VIEW (window->view))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->view));
      have_iter = gtk_tree_selection_get_selected (selection, model, iter);
    }
  else
    {
      items = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (window->view));
      appfinder_assert (g_list_length (items) <= 1);
      if (items != NULL)
        {
          *model = gtk_icon_view_get_model (GTK_ICON_VIEW (window->view));
          have_iter = gtk_tree_model_get_iter (*model, iter, items->data);

          gtk_tree_path_free (items->data);
          g_list_free (items);
        }
      else
        {
          have_iter = FALSE;
        }
    }

  return have_iter;
}



static void
xfce_appfinder_window_popup_menu_toggle_bookmark (GtkWidget           *mi,
                                                  XfceAppfinderWindow *window)
{
  const gchar  *uri;
  GFile        *gfile;
  gchar        *desktop_id;
  GtkWidget    *menu = gtk_widget_get_parent (mi);
  GtkTreeModel *filter;
  GtkTreeModel *model;
  GError       *error = NULL;

  uri = g_object_get_data (G_OBJECT (menu), "uri");
  if (uri != NULL)
    {
      gfile = g_file_new_for_uri (uri);
      desktop_id = g_file_get_basename (gfile);
      g_object_unref (G_OBJECT (gfile));

      /* toggle bookmarks */
      filter = g_object_get_data (G_OBJECT (menu), "model");
      model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (filter));
      xfce_appfinder_model_bookmark_toggle (XFCE_APPFINDER_MODEL (model), desktop_id, &error);

      g_free (desktop_id);

      if (G_UNLIKELY (error != NULL))
        {
          g_printerr ("%s: failed to save bookmarks: %s\n", G_LOG_DOMAIN, error->message);
          g_error_free (error);
        }
    }
}



static void
xfce_appfinder_window_popup_menu_execute (GtkWidget           *mi,
                                          XfceAppfinderWindow *window)
{
  xfce_appfinder_window_execute (window, FALSE, FALSE, NULL);
}



static void
xfce_appfinder_window_popup_menu_edit (GtkWidget           *mi,
                                       XfceAppfinderWindow *window)
{
  GError      *error = NULL;
  gchar       *cmd;
  const gchar *uri;
  GtkWidget   *menu = gtk_widget_get_parent (mi);

  appfinder_return_if_fail (GTK_IS_MENU (menu));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  uri = g_object_get_data (G_OBJECT (menu), "uri");
  if (G_LIKELY (uri != NULL))
    {
      cmd = g_strdup_printf ("exo-desktop-item-edit --xid=0x%x '%s'",
                             APPFINDER_WIDGET_XID (window), uri);
      if (!g_spawn_command_line_async (cmd, &error))
        {
          xfce_dialog_show_error (GTK_WINDOW (window), error,
                                  _("Failed to launch desktop item editor"));
          g_error_free (error);
        }
      g_free (cmd);
    }
}



static void
xfce_appfinder_window_popup_menu_revert (GtkWidget           *mi,
                                         XfceAppfinderWindow *window)
{
  const gchar *uri;
  const gchar *name;
  GError      *error;
  GtkWidget   *menu = gtk_widget_get_parent (mi);

  appfinder_return_if_fail (GTK_IS_MENU (menu));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  name = g_object_get_data (G_OBJECT (menu), "name");
  if (name == NULL)
    return;

  if (xfce_dialog_confirm (GTK_WINDOW (window), GTK_STOCK_REVERT_TO_SAVED, NULL,
          _("This will permanently remove the custom desktop file from your home directory."),
          _("Are you sure you want to revert \"%s\"?"), name))
    {
      uri = g_object_get_data (G_OBJECT (menu), "uri");
      if (uri != NULL)
        {
          if (g_unlink (uri + 7) == -1)
            {
              error = g_error_new (G_FILE_ERROR, g_file_error_from_errno (errno),
                                   "%s: %s", uri + 7, g_strerror (errno));
              xfce_dialog_show_error (GTK_WINDOW (window), error,
                                      _("Failed to remove desktop file"));
              g_error_free (error);
            }
        }
    }
}



static void
xfce_appfinder_window_popup_menu_hide (GtkWidget           *mi,
                                       XfceAppfinderWindow *window)
{
  const gchar  *uri;
  const gchar  *name;
  GtkWidget    *menu = gtk_widget_get_parent (mi);
  gchar        *path;
  gchar        *message;
  gchar       **dirs;
  guint         i;
  const gchar  *relpath;
  XfceRc       *rc;

  appfinder_return_if_fail (GTK_IS_MENU (menu));
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  name = g_object_get_data (G_OBJECT (menu), "name");
  if (name == NULL)
    return;

  path = xfce_resource_save_location (XFCE_RESOURCE_DATA, "applications/", FALSE);
  /* I18N: the first %s will be replace with users' applications directory, the
   * second with Hidden=true */
  message = g_strdup_printf (_("To unhide the item you have to manually "
                               "remove the desktop file from \"%s\" or "
                               "open the file in the same directory and "
                               "remove the line \"%s\"."), path, "Hidden=true");

  if (xfce_dialog_confirm (GTK_WINDOW (window), NULL, _("_Hide"), message,
          _("Are you sure you want to hide \"%s\"?"), name))
    {
      uri = g_object_get_data (G_OBJECT (menu), "uri");
      if (uri != NULL)
        {
          /* lookup the correct relative path */
          dirs = xfce_resource_lookup_all (XFCE_RESOURCE_DATA, "applications/");
          for (i = 0; dirs[i] != NULL; i++)
            {
              if (g_str_has_prefix (uri + 7, dirs[i]))
                {
                  /* relative path to XFCE_RESOURCE_DATA */
                  relpath = uri + 7 + strlen (dirs[i]) - 13;

                  /* xfcerc can handle everything else */
                  rc = xfce_rc_config_open (XFCE_RESOURCE_DATA, relpath, FALSE);
                  xfce_rc_set_group (rc, G_KEY_FILE_DESKTOP_GROUP);
                  xfce_rc_write_bool_entry (rc, G_KEY_FILE_DESKTOP_KEY_HIDDEN, TRUE);
                  xfce_rc_close (rc);

                  break;
                }
            }
          g_strfreev (dirs);
        }
    }

  g_free (path);
  g_free (message);
}



static gboolean
xfce_appfinder_window_popup_menu (GtkWidget           *view,
                                  XfceAppfinderWindow *window)
{
  GtkWidget    *menu;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *title;
  gchar        *uri;
  GtkWidget    *mi;
  GtkWidget    *image;
  gchar        *path;
  gboolean      uri_is_local;
  gboolean      is_bookmark;

  if (xfce_appfinder_window_view_get_selected (window, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          XFCE_APPFINDER_MODEL_COLUMN_TITLE, &title,
                          XFCE_APPFINDER_MODEL_COLUMN_URI, &uri,
                          XFCE_APPFINDER_MODEL_COLUMN_BOOKMARK, &is_bookmark,
                          -1);

      /* custom command don't have an uri */
      if (uri == NULL)
        {
          g_free (title);
          return FALSE;
        }

      uri_is_local = g_str_has_prefix (uri, "file://");

      menu = gtk_menu_new ();
      g_object_set_data_full (G_OBJECT (menu), "uri", uri, g_free);
      g_object_set_data_full (G_OBJECT (menu), "name", title, g_free);
      g_object_set_data (G_OBJECT (menu), "model", model);
      g_signal_connect (G_OBJECT (menu), "selection-done",
          G_CALLBACK (gtk_widget_destroy), NULL);

      mi = gtk_menu_item_new_with_label (title);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_set_sensitive (mi, FALSE);
      gtk_widget_show (mi);

      mi = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_show (mi);

      mi = gtk_image_menu_item_new_with_mnemonic (is_bookmark ? _("Remove From Bookmarks") : _("Add to Bookmarks"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_toggle_bookmark), window);
      gtk_widget_show (mi);

      if (is_bookmark)
        image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
      else
        image = gtk_image_new_from_icon_name ("bookmark-new", GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi), image);

      mi = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_show (mi);

      mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_EXECUTE, NULL);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_execute), window);
      gtk_widget_show (mi);

      mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_EDIT, NULL);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_edit), window);
      gtk_widget_show (mi);

      mi = gtk_image_menu_item_new_from_stock (GTK_STOCK_REVERT_TO_SAVED, NULL);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_revert), window);
      path = xfce_resource_save_location (XFCE_RESOURCE_DATA, "applications/", FALSE);
      gtk_widget_set_sensitive (mi, uri_is_local && g_str_has_prefix (uri + 7, path));
      gtk_widget_show (mi);
      g_free (path);

      mi = gtk_image_menu_item_new_with_mnemonic (_("_Hide"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
      gtk_widget_set_sensitive (mi, uri_is_local);
      g_signal_connect (G_OBJECT (mi), "activate",
          G_CALLBACK (xfce_appfinder_window_popup_menu_hide), window);
      gtk_widget_show (mi);

      gtk_menu_popup (GTK_MENU (menu),
                      NULL, NULL, NULL, NULL, 3,
                      gtk_get_current_event_time ());

      return TRUE;
    }

  return FALSE;
}



static void
xfce_appfinder_window_update_image (XfceAppfinderWindow *window,
                                    GdkPixbuf           *pixbuf)
{
  if (pixbuf == NULL)
    pixbuf = window->icon_find;

  /* gtk doesn't check this */
  if (gtk_image_get_pixbuf (GTK_IMAGE (window->image)) != pixbuf)
    gtk_image_set_from_pixbuf (GTK_IMAGE (window->image), pixbuf);
}



static void
xfce_appfinder_window_set_padding (GtkWidget *entry,
                                   GtkWidget *align)
{
  gint          padding;
  GtkAllocation alloc;

  /* 48 is the icon size of XFCE_APPFINDER_ICON_SIZE_48 */
  gtk_widget_get_allocation (entry, &alloc);
  padding = (48 - alloc.height) / 2;
  gtk_alignment_set_padding (GTK_ALIGNMENT (align), MAX (0, padding), 0, 0, 0);
}



static gboolean
xfce_appfinder_window_completion_match_func (GtkEntryCompletion *completion,
                                             const gchar        *key,
                                             GtkTreeIter        *iter,
                                             gpointer            data)
{
  const gchar *text;

  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (data);

  appfinder_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), FALSE);
  appfinder_return_val_if_fail (XFCE_IS_APPFINDER_WINDOW (data), FALSE);
  appfinder_return_val_if_fail (GTK_TREE_MODEL (window->model)
      == gtk_entry_completion_get_model (completion), FALSE);

  /* don't use the casefolded key generated by gtk */
  text = gtk_entry_get_text (GTK_ENTRY (window->entry));

  return xfce_appfinder_model_get_visible_command (window->model, iter, text);
}



static gboolean
xfce_appfinder_window_entry_changed_idle (gpointer data)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (data);
  const gchar         *text;
  GdkPixbuf           *pixbuf;
  gchar               *normalized;
  GtkTreeModel        *model;

  GDK_THREADS_ENTER ();

  text = gtk_entry_get_text (GTK_ENTRY (window->entry));

  if (gtk_widget_get_visible (window->paned))
    {
      g_free (window->filter_text);

      if (IS_STRING (text))
        {
          normalized = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
          window->filter_text = g_utf8_casefold (normalized, -1);
          g_free (normalized);
        }
      else
        {
          window->filter_text = NULL;
        }

      APPFINDER_DEBUG ("refilter entry");
      if (GTK_IS_TREE_VIEW (window->view))
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->view));
      else
        model = gtk_icon_view_get_model (GTK_ICON_VIEW (window->view));
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (model));
    }
  else
    {
      gtk_widget_set_sensitive (window->button_launch, IS_STRING (text));
      gtk_widget_set_sensitive (window->button_launch_sandboxed, IS_STRING (text));

      pixbuf = xfce_appfinder_model_get_icon_for_command (window->model, text);
      xfce_appfinder_window_update_image (window, pixbuf);
      if (pixbuf != NULL)
        g_object_unref (G_OBJECT (pixbuf));
    }

  xfce_appfinder_update_button_labels (window);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
xfce_appfinder_window_entry_changed_idle_destroyed (gpointer data)
{
  XFCE_APPFINDER_WINDOW (data)->idle_entry_changed_id = 0;
}



static void
xfce_appfinder_window_entry_changed (XfceAppfinderWindow *window)
{
  if (window->idle_entry_changed_id != 0)
    g_source_remove (window->idle_entry_changed_id);

  window->idle_entry_changed_id =
      g_idle_add_full (G_PRIORITY_DEFAULT, xfce_appfinder_window_entry_changed_idle,
                       window, xfce_appfinder_window_entry_changed_idle_destroyed);
}



static void
xfce_appfinder_window_entry_activate (GtkEditable         *entry,
                                      XfceAppfinderWindow *window)
{
  GtkTreePath *path;
  gboolean     cursor_set = FALSE;

  if (gtk_widget_get_visible (window->paned))
    {
      if (GTK_IS_TREE_VIEW (window->view))
        {
          if (gtk_tree_view_get_visible_range (GTK_TREE_VIEW (window->view), &path, NULL))
            {
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (window->view), path, NULL, FALSE);
              gtk_tree_path_free (path);

              cursor_set = TRUE;
            }
        }
      else if (gtk_icon_view_get_visible_range (GTK_ICON_VIEW (window->view), &path, NULL))
        {
          gtk_icon_view_select_path (GTK_ICON_VIEW (window->view), path);
          gtk_icon_view_set_cursor (GTK_ICON_VIEW (window->view), path, NULL, FALSE);
          gtk_tree_path_free (path);

          cursor_set = TRUE;
        }

      if (cursor_set)
        gtk_widget_grab_focus (window->view);
      else
        xfce_appfinder_window_execute (window, TRUE, FALSE, NULL);
    }
  else if (gtk_widget_get_sensitive (window->button_launch))
    {
      gtk_button_clicked (GTK_BUTTON (window->button_launch));
    }
}



static gboolean
xfce_appfinder_window_pointer_is_grabbed (GtkWidget *widget)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  GdkDeviceManager *device_manager;
  GList            *devices, *li;
  GdkDisplay       *display;
  gboolean          is_grabbed = FALSE;

  display = gtk_widget_get_display (widget);
  device_manager = gdk_display_get_device_manager (display);
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (li = devices; li != NULL; li = li->next)
    {
      if (gdk_device_get_source (li->data) == GDK_SOURCE_MOUSE
          && gdk_display_device_is_grabbed (display, li->data))
        {
          is_grabbed = TRUE;
          break;
        }
    }

  g_list_free (devices);

  return is_grabbed;
#else
  return gdk_pointer_is_grabbed ();
#endif
}



static gboolean
xfce_appfinder_window_entry_key_press_event (GtkWidget           *entry,
                                             GdkEventKey         *event,
                                             XfceAppfinderWindow *window)
{
  gboolean          expand;
  gboolean          is_expanded;

  if (event->keyval == GDK_KEY_Up
      || event->keyval == GDK_KEY_Down)
    {
      expand = (event->keyval == GDK_KEY_Down);
      is_expanded = gtk_widget_get_visible (window->paned);
      if (is_expanded != expand)
        {
          /* don't break entry completion navigation in collapsed mode */
          if (!is_expanded
              && xfce_appfinder_window_pointer_is_grabbed (entry))
            {
              /* window is still collapsed and the pointer is grabbed
               * by the popup menu, do nothing with the event */
              return FALSE;
            }

          xfce_appfinder_window_set_expanded (window, expand);
          return TRUE;
        }
    }
  else if (event->keyval == GDK_KEY_Tab
           && !gtk_widget_get_visible (window->paned)
           && xfce_appfinder_window_pointer_is_grabbed (entry))
    {
      /* don't tab to the close button */
      return TRUE;
    }

  return FALSE;
}



static void
xfce_appfinder_window_drag_begin (GtkWidget           *widget,
                                  GdkDragContext      *drag_context,
                                  XfceAppfinderWindow *window)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GdkPixbuf    *pixbuf;

  if (xfce_appfinder_window_view_get_selected (window, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, XFCE_APPFINDER_MODEL_COLUMN_ICON_LARGE, &pixbuf, -1);
      if (G_LIKELY (pixbuf != NULL))
        {
          gtk_drag_set_icon_pixbuf (drag_context, pixbuf, 0, 0);
          g_object_unref (G_OBJECT (pixbuf));
        }
    }
  else
    {
      gtk_drag_set_icon_stock (drag_context, GTK_STOCK_DIALOG_ERROR, 0, 0);
    }
}



static void
xfce_appfinder_window_drag_data_get (GtkWidget           *widget,
                                     GdkDragContext      *drag_context,
                                     GtkSelectionData    *data,
                                     guint                info,
                                     guint                drag_time,
                                     XfceAppfinderWindow *window)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *uris[2];

  if (xfce_appfinder_window_view_get_selected (window, &model, &iter))
    {
      uris[1] = NULL;
      gtk_tree_model_get (model, &iter, XFCE_APPFINDER_MODEL_COLUMN_URI, &uris[0], -1);
      gtk_selection_data_set_uris (data, uris);
      g_free (uris[0]);
    }
}



static gboolean
xfce_appfinder_window_treeview_key_press_event (GtkWidget           *widget,
                                                GdkEventKey         *event,
                                                XfceAppfinderWindow *window)
{
  if (widget == window->view)
    {
      if (event->keyval == GDK_KEY_Left)
        {
          gtk_widget_grab_focus (window->sidepane);
          return TRUE;
        }
    }
  else if (widget == window->sidepane)
    {
      if (event->keyval == GDK_KEY_Right)
        {
          gtk_widget_grab_focus (window->view);
          return TRUE;
        }
    }

  return FALSE;
}



static void
xfce_appfinder_window_entry_icon_released (GtkEntry             *entry,
                                           GtkEntryIconPosition  icon_pos,
                                           GdkEvent             *event,
                                           XfceAppfinderWindow  *window)
{
  if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
    xfce_appfinder_window_set_expanded (window, !gtk_widget_get_visible (window->paned));
}



static void
xfce_appfinder_window_category_changed (GtkTreeSelection    *selection,
                                        XfceAppfinderWindow *window)
{
  GtkTreeIter          iter;
  GtkTreeModel        *model;
  GarconMenuDirectory *category;
  gchar               *name;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_DIRECTORY, &category,
                          XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_NAME, &name, -1);

      if (window->filter_category != category)
        {
          if (window->filter_category != NULL)
            g_object_unref (G_OBJECT (window->filter_category));

          if (category == NULL)
            window->filter_category = NULL;
          else
            window->filter_category = g_object_ref (G_OBJECT (category));

          APPFINDER_DEBUG ("refilter category");

          /* update visible items */
          if (GTK_IS_TREE_VIEW (window->view))
            model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->view));
          else
            model = gtk_icon_view_get_model (GTK_ICON_VIEW (window->view));
          gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (model));

          /* store last category */
          if (xfconf_channel_get_bool (window->channel, "/remember-category", FALSE))
            xfconf_channel_set_string (window->channel, "/last/category", name);
        }

      g_free (name);
      if (category != NULL)
        g_object_unref (G_OBJECT (category));
    }
}



static void
xfce_appfinder_window_category_set_categories (XfceAppfinderModel  *signal_from_model,
                                               XfceAppfinderWindow *window)
{
  GSList           *categories;
  GtkTreePath      *path;
  gchar            *name = NULL;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GtkTreeSelection *selection;

  appfinder_return_if_fail (GTK_IS_TREE_VIEW (window->sidepane));

  if (signal_from_model != NULL)
    {
      /* reload from the model, make sure we restore the selected category */
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->sidepane));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        gtk_tree_model_get (model, &iter, XFCE_APPFINDER_CATEGORY_MODEL_COLUMN_NAME, &name, -1);
    }

  /* update the categories */
  categories = xfce_appfinder_model_get_categories (window->model);
  if (categories != NULL)
    xfce_appfinder_category_model_set_categories (window->category_model, categories);
  g_slist_free (categories);

  if (name == NULL && xfconf_channel_get_bool (window->channel, "/remember-category", FALSE))
    name = xfconf_channel_get_string (window->channel, "/last/category", NULL);

  path = xfce_appfinder_category_model_find_category (window->category_model, name);
  if (path != NULL)
    {
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (window->sidepane), path, NULL, FALSE);
      gtk_tree_path_free (path);
    }

  g_free (name);
}



static void
xfce_appfinder_window_preferences (GtkWidget           *button,
                                   XfceAppfinderWindow *window)
{
  appfinder_return_if_fail (GTK_IS_WIDGET (button));

  /* preload the actions, to make sure there are default values */
  if (window->actions == NULL)
    window->actions = xfce_appfinder_actions_get ();

  xfce_appfinder_preferences_show (gtk_widget_get_screen (button));
}



static void
xfce_appfinder_window_property_changed (XfconfChannel       *channel,
                                        const gchar         *prop,
                                        const GValue        *value,
                                        XfceAppfinderWindow *window)
{
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));
  appfinder_return_if_fail (window->channel == channel);

  if (g_strcmp0 (prop, "/icon-view") == 0)
    {
      xfce_appfinder_window_view (window);
    }
  else if (g_strcmp0 (prop, "/item-icon-size") == 0)
    {
      if (GTK_IS_ICON_VIEW (window->view))
        xfce_appfinder_window_set_item_width (window);
    }
  else if (g_strcmp0 (prop, "/text-beside-icons") == 0)
    {
      if (GTK_IS_ICON_VIEW (window->view))
        xfce_appfinder_window_set_item_width (window);
    }
  else if (g_strcmp0 (prop, "/show-sandboxed") == 0)
    {
      xfce_appfinder_update_button_labels (window);
    }
}



static gboolean
xfce_appfinder_window_item_visible (GtkTreeModel *model,
                                    GtkTreeIter  *iter,
                                    gpointer      data)
{
  XfceAppfinderWindow *window = XFCE_APPFINDER_WINDOW (data);

  return xfce_appfinder_model_get_visible (XFCE_APPFINDER_MODEL (model), iter,
                                           window->filter_category,
                                           window->filter_text);
}



static void
xfce_appfinder_window_item_changed (XfceAppfinderWindow *window)
{
  GtkTreeIter       iter;
  GtkTreeModel     *model;
  gboolean          can_launch;
  GdkPixbuf        *pixbuf;

  if (gtk_widget_get_visible (window->paned))
    {
      can_launch = xfce_appfinder_window_view_get_selected (window, &model, &iter);
      gtk_widget_set_sensitive (window->button_launch, can_launch);
      gtk_widget_set_sensitive (window->button_launch_sandboxed, can_launch);

      if (can_launch)
        {
          gtk_tree_model_get (model, &iter, XFCE_APPFINDER_MODEL_COLUMN_ICON_LARGE, &pixbuf, -1);
          if (G_LIKELY (pixbuf != NULL))
            {
              xfce_appfinder_window_update_image (window, pixbuf);
              g_object_unref (G_OBJECT (pixbuf));
            }
        }
      else
        {
          xfce_appfinder_window_update_image (window, NULL);
        }
    }

    xfce_appfinder_update_button_labels (window);
}



static void
xfce_appfinder_window_row_activated (XfceAppfinderWindow *window)
{
  if (gtk_widget_get_sensitive (window->button_launch))
    gtk_button_clicked (GTK_BUTTON (window->button_launch));
}



static void
xfce_appfinder_window_icon_theme_changed (XfceAppfinderWindow *window)
{
  appfinder_return_if_fail (XFCE_IS_APPFINDER_WINDOW (window));

  if (window->icon_find != NULL)
    g_object_unref (G_OBJECT (window->icon_find));
  window->icon_find = xfce_appfinder_model_load_pixbuf (GTK_STOCK_FIND, XFCE_APPFINDER_ICON_SIZE_48);

  /* drop cached pixbufs */
  if (G_LIKELY (window->model != NULL))
    xfce_appfinder_model_icon_theme_changed (window->model);

  if (G_LIKELY (window->category_model != NULL))
    xfce_appfinder_category_model_icon_theme_changed (window->category_model);

  /* update state */
  xfce_appfinder_window_entry_changed (window);
  xfce_appfinder_window_item_changed (window);
}



static void
populate_profile_box (GtkWidget *widget)
{
  GtkComboBoxText  *box   = (GtkComboBoxText *) widget;
  GList            *profiles = NULL, *lp;


  profiles = xfce_get_firejail_profile_names (FALSE);
  for (lp = profiles; lp != NULL; lp = lp->next)
    {
      gtk_combo_box_text_append_text (box, lp->data);
      g_free (lp->data);
    }
  g_list_free (profiles);

  gtk_combo_box_text_prepend_text (box, EXECHELP_DEFAULT_ROOT_PROFILE);
  gtk_combo_box_text_prepend_text (box, EXECHELP_DEFAULT_USER_PROFILE);

  gtk_combo_box_set_active (GTK_COMBO_BOX (box), 0);

  return;
}



gint
xfce_appfinder_window_ask_if_sandboxing (GarconMenuItem       *item,
                                         const gchar          *command,
                                         GdkScreen            *screen,
                                         XfceAppfinderWindow  *window)
{
  GtkWidget     *dialog;
  gint           response;
  GtkWidget     *vbox;
  GtkWidget     *label;
  gchar         *title;
  gchar         *string;
  const gchar   *name;
  const gchar   *exec;

  appfinder_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  appfinder_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  appfinder_return_val_if_fail (!window || XFCE_IS_APPFINDER_WINDOW (window), FALSE);

  name = garcon_menu_item_get_name (item);

  /* Ignore Xfce clients */
  exec = garcon_menu_item_get_command (item);
  if (xfce_client_is_xfce (exec))
    return APPFINDER_LAUNCH_RESPONSE;

  /* Create the dialog */
  title = g_strdup_printf (_("Launch %s inside a sandbox?"), name);
  dialog = xfce_security_dialog_new (screen, title);
  g_free (title);

  xfce_security_dialog_add_button (XFCE_SECURITY_DIALOG (dialog),
                                   _("_Configure Sandbox"),
                                   "firejail", "firejail-run",
                                   APPFINDER_CONF_SANDBOX_RESPONSE,
                                   FALSE, FALSE);

  title = g_strdup_printf (_("Launch %s"), name);
  xfce_security_dialog_add_button (XFCE_SECURITY_DIALOG (dialog),
                                   title,
                                   garcon_menu_item_get_icon_name (item), "gtk-execute",
                                   APPFINDER_LAUNCH_RESPONSE,
                                   FALSE, TRUE);
  g_free (title);

  /* Top widget area */
  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);

  string = g_strdup_printf (_("Sandboxes increase security, and they help you control how %s accesses your documents and uses your Internet resources."), name);
  label = gtk_label_new (string);
  g_free (string);

  gtk_misc_set_alignment (GTK_MISC (label), 0.00, 0.50);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  xfce_security_dialog_set_top_widget (XFCE_SECURITY_DIALOG (dialog), vbox);
  gtk_widget_show (label);

  /* Bottom widget area */
  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);

  string = g_strdup_printf (_("<small>This dialog will not be shown again for %s. You can change sandbox settings later by editing the %s launcher.</small>"), name, name);
  label = gtk_label_new (string);
  g_free (string);
  gtk_misc_set_alignment (GTK_MISC (label), 0.00, 0.50);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  xfce_security_dialog_set_bottom_widget (XFCE_SECURITY_DIALOG (dialog), vbox);
  gtk_widget_show (label);

  /* Run the widget now */
  response = xfce_security_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  /* If asked to configure a sandbox, wait a moment before running the app */
  if (response == APPFINDER_CONF_SANDBOX_RESPONSE)
    {
      GFile     *file  = garcon_menu_item_get_file (item);
      GError    *error = NULL;
      gchar    **argv;

      if (!file)
        {
          xfce_dialog_show_error_manual (window ? GTK_WINDOW (window) : NULL, _("Could not find corresponding desktop file"),
                                  _("Failed to launch desktop item editor"));
          response = GTK_RESPONSE_CANCEL;
        }
      else
        {
          gsize index = 0;
          argv = g_malloc0 (sizeof (gchar *) * (6));
          argv[index++] = g_strdup ("exo-desktop-item-edit");
          argv[index++] = g_strdup ("--edit-sandbox");
          argv[index++] = g_strdup_printf ("--launch-after-edit=%s", command);
          if (window)
            argv[index++] = g_strdup_printf ("--xid=0x%x", APPFINDER_WIDGET_XID (window));
          argv[index++] = g_file_get_path (file);
          argv[index++] = NULL;

          g_spawn_async (NULL, argv, environ, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
          g_strfreev (argv);
          g_object_unref (file);

          if (error)
            {
              xfce_dialog_show_error (window ? GTK_WINDOW (window) : NULL, error,
                                      _("Failed to launch desktop item editor"));
              g_error_free (error);
              response = GTK_RESPONSE_CANCEL;
            }
        }
    }

  return response;
}



static gboolean
xfce_appfinder_window_execute_command (const gchar          *text,
                                       gboolean              sandboxed,
                                       const gchar          *profile,
                                       GdkScreen            *screen,
                                       XfceAppfinderWindow  *window,
                                       gboolean              only_custom_cmd,
                                       gboolean             *save_cmd,
                                       GError              **error)
{
  gboolean  succeed = FALSE;
  gchar    *action_cmd = NULL;
  gchar    *space;
  gchar    *short_command;
  gchar    *expanded;
  gchar    *sandbox_expanded;
  gboolean  sandboxed_app = FALSE;
  gboolean  secure_ws = xfce_workspace_is_active_secure (screen);
  gboolean  launch_no_longer_needed = FALSE;

  appfinder_return_val_if_fail (error != NULL && *error == NULL, FALSE);
  appfinder_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  if (!IS_STRING (text))
    return TRUE;

  if (!only_custom_cmd)
    sandboxed_app = xfce_appfinder_model_is_command_sandboxed (window->model, text) && !secure_ws;

  if (window->actions == NULL)
    window->actions = xfce_appfinder_actions_get ();

  /* try to match a custom action */
  action_cmd = xfce_appfinder_actions_execute (window->actions, text, save_cmd, error);
  if (*error != NULL)
    return FALSE;
  else if (action_cmd != NULL)
    text = action_cmd;
  else if (only_custom_cmd)
    return FALSE;

  /* check if the command is a composite */
  space = strchr (text, ' ');
  short_command = space ? g_strndup (text, space - text) : NULL;

  /* if it's the first time we see this command, ask the user if the app should be sandboxed */
  if (!action_cmd && !sandboxed && !secure_ws && !sandboxed_app)
    {
      GarconMenuItem *item = xfce_appfinder_model_get_item_for_command (window->model, text);
      if (!item && short_command)
        item = xfce_appfinder_model_get_item_for_command (window->model, short_command);

      /* if there is an app for this command, and we've never asked before, ask now */
      if (item && !xfce_appfinder_model_app_history_contains (window->model, item))
        {
          gint response = xfce_appfinder_window_ask_if_sandboxing (item, text, screen, window);

          /* The user cancelled, we'll need to remember to ask again next time */
          if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT)
            {
              if (save_cmd)
                *save_cmd = FALSE;
              launch_no_longer_needed = TRUE;
            }
          else
            xfce_appfinder_model_save_application (window->model, item, NULL);

          /* The exo desktop item editor will be in charge of spawning the app */
          sandboxed_app = (response == APPFINDER_CONF_SANDBOX_RESPONSE);
          if (sandboxed_app)
            launch_no_longer_needed = TRUE;
        }

      if (item)
        g_object_unref (item);
    }

  /* if the user cancelled or the launcher editor takes care of the launch for us, abort */
  g_free (short_command);
  if (launch_no_longer_needed)
    return TRUE;

  /* start preparing the launch of the command */
  if (IS_STRING (text))
    {
      /* expand variables */
      expanded = xfce_expand_variables (text, NULL);

      /* spawn the command */
      APPFINDER_DEBUG ("spawn \"%s\"%s", expanded, sandboxed? " sandboxed": sandboxed_app? " as a sandboxed app":"");
      if (sandboxed_app)
        {
          GarconMenuItem *item = xfce_appfinder_model_get_item_for_command (window->model, text);

          if (item)
            {
              sandbox_expanded = garcon_menu_item_expand_command (item, expanded, secure_ws);

              succeed = xfce_spawn_command_line_on_screen (screen,
                                                           sandbox_expanded,
                                                           FALSE,
                                                           garcon_menu_item_supports_startup_notification (item),
                                                           error);

              g_free (sandbox_expanded);
              g_object_unref (item);
            }
        }
      else if (sandboxed && !secure_ws)
        {
          if (profile)
            {
              gchar *profiletxt = xfce_get_firejail_profile_for_name (profile);
              sandbox_expanded = g_strdup_printf ("firejail %s %s", profiletxt? profiletxt : "", expanded);
              g_free (profiletxt);
            }
          else
            sandbox_expanded = g_strdup_printf ("firejail %s", expanded);
          succeed = xfce_spawn_command_line_on_screen (screen, sandbox_expanded, FALSE, FALSE, error);
          g_free (sandbox_expanded);
        }
      else
        {
          succeed = xfce_spawn_command_line_on_screen (screen, expanded, FALSE, FALSE, error);
        }
      g_free (expanded);
    }

  g_free (action_cmd);

  return succeed;
}



static void
xfce_appfinder_window_launch_sandboxed_real_clicked (XfceAppfinderWindow *window)
{
  xfce_appfinder_window_execute (window, TRUE, TRUE, gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (window->profile_box)));
}



static void
xfce_appfinder_window_launch_sandboxed_back_clicked (XfceAppfinderWindow *window)
{
  gtk_widget_hide (window->profile_box);
  gtk_widget_hide (window->profile_label);
  gtk_widget_hide (window->button_launch_sandboxed_back);
  gtk_widget_hide (window->button_launch_sandboxed_real);
  gtk_widget_hide (window->button_preferences);

  gtk_widget_show (window->button_launch);
  gtk_widget_show (window->button_launch_sandboxed);
  gtk_widget_show (window->button_close);
  gtk_widget_show (window->button_preferences);
}



static void
xfce_appfinder_window_launch_sandboxed_clicked (XfceAppfinderWindow *window)
{
  gtk_widget_show (window->profile_box);
  gtk_widget_show (window->profile_label);
  gtk_widget_show (window->button_launch_sandboxed_back);
  gtk_widget_show (window->button_launch_sandboxed_real);
  gtk_widget_show (window->button_preferences);

  gtk_widget_hide (window->button_launch);
  gtk_widget_hide (window->button_launch_sandboxed);
  gtk_widget_hide (window->button_close);
  gtk_widget_hide (window->button_preferences);
}



static void
xfce_appfinder_window_launch_clicked (XfceAppfinderWindow *window)
{
  xfce_appfinder_window_execute (window, TRUE, FALSE, NULL);
}



static void
xfce_appfinder_window_execute (XfceAppfinderWindow *window,
                               gboolean             close_on_succeed,
                               gboolean             sandboxed,
                               const gchar         *profile)
{
  GtkTreeModel *model;
  GtkTreeIter   iter, orig;
  GError       *error = NULL;
  gboolean      result = FALSE;
  GdkScreen    *screen;
  const gchar  *text;
  gchar        *cmd = NULL;
  gboolean      regular_command = FALSE;
  gboolean      save_cmd;
  gboolean      only_custom_cmd = FALSE;

  screen = gtk_window_get_screen (GTK_WINDOW (window));
  if (gtk_widget_get_visible (window->paned))
    {
      if (!gtk_widget_get_sensitive (window->button_launch))
        {
          only_custom_cmd = TRUE;
          goto entry_execute;
        }

      if (xfce_appfinder_window_view_get_selected (window, &model, &iter))
        {
          gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &orig, &iter);
          gtk_tree_model_get (model, &iter, XFCE_APPFINDER_MODEL_COLUMN_COMMAND, &cmd, -1);
      
          result = xfce_appfinder_model_execute (window->model, &orig, screen, sandboxed, profile, &regular_command, &error);

          if (!result && regular_command)
            {
              result = xfce_appfinder_window_execute_command (cmd, sandboxed, profile, screen, window, FALSE, NULL, &error);
            }

          g_free (cmd);
        }
    }
  else
    {
      if (!gtk_widget_get_sensitive (window->button_launch))
        return;

      entry_execute:

      text = gtk_entry_get_text (GTK_ENTRY (window->entry));
      save_cmd = TRUE;
      
      if (xfce_appfinder_window_execute_command (text, sandboxed, profile, screen, window, only_custom_cmd, &save_cmd, &error))
        {
          if (save_cmd)
            result = xfce_appfinder_model_save_command (window->model, text, &error);
          else
            result = TRUE;
        }
    }

  if (!only_custom_cmd)
    {
      gtk_entry_set_icon_from_stock (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_PRIMARY,
                                     result ? NULL : GTK_STOCK_DIALOG_ERROR);
      gtk_entry_set_icon_tooltip_text (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_PRIMARY,
                                       error != NULL ? error->message : NULL);
    }

  if (error != NULL)
    {
      g_printerr ("%s: failed to execute: %s\n", G_LOG_DOMAIN, error->message);
      g_error_free (error);
    }

  if (result && close_on_succeed)
    gtk_widget_destroy (GTK_WIDGET (window));
}



void
xfce_appfinder_window_set_expanded (XfceAppfinderWindow *window,
                                    gboolean             expanded)
{
  GdkGeometry         hints;
  gint                width;
  GtkWidget          *parent;
  GtkEntryCompletion *completion;

  APPFINDER_DEBUG ("set expand = %s", expanded ? "true" : "false");

  /* force window geomentry */
  if (expanded)
    {
      gtk_window_set_geometry_hints (GTK_WINDOW (window), NULL, NULL, 0);
      gtk_window_get_size (GTK_WINDOW (window), &width, NULL);
      gtk_window_resize (GTK_WINDOW (window), width, window->last_window_height);
    }
  else
    {
      if (gtk_widget_get_visible (GTK_WIDGET (window)))
        gtk_window_get_size (GTK_WINDOW (window), NULL, &window->last_window_height);

      hints.max_height = -1;
      hints.max_width = G_MAXINT;
      gtk_window_set_geometry_hints (GTK_WINDOW (window), NULL, &hints, GDK_HINT_MAX_SIZE);
    }

  /* repack the button box */
  g_object_ref (G_OBJECT (window->bbox));
  parent = gtk_widget_get_parent (window->bbox);
  if (parent != NULL)
    gtk_container_remove (GTK_CONTAINER (parent), window->bbox);
  if (expanded)
    gtk_container_add (GTK_CONTAINER (window->bin_expanded), window->bbox);
  else
    gtk_container_add (GTK_CONTAINER (window->bin_collapsed), window->bbox);
  gtk_widget_set_visible (window->bin_expanded, expanded);
  gtk_widget_set_visible (window->bin_collapsed, !expanded);
  gtk_widget_set_visible (window->button_preferences, expanded);
  g_object_unref (G_OBJECT (window->bbox));

  /* show/hide pane with treeviews */
  gtk_widget_set_visible (window->paned, expanded);

  /* toggle icon */
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_SECONDARY,
                                     expanded ? GTK_STOCK_GO_UP : GTK_STOCK_GO_DOWN);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (window->entry), GTK_ENTRY_ICON_PRIMARY, NULL);

  /* update completion (remove completed text of restart completion) */
  completion = gtk_entry_get_completion (GTK_ENTRY (window->entry));
  if (completion != NULL)
    gtk_editable_delete_selection (GTK_EDITABLE (window->entry));
  gtk_entry_set_completion (GTK_ENTRY (window->entry), expanded ? NULL : window->completion);
  if (!expanded && gtk_entry_get_text_length (GTK_ENTRY (window->entry)) > 0)
    gtk_entry_completion_insert_prefix (window->completion);

  /* update state */
  xfce_appfinder_window_entry_changed (window);
  xfce_appfinder_window_item_changed (window);
}
