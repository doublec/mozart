/*
 * Authors:
 *   Andreas Simon (1999-2000)
 *
 * Copyright:
 *   Andreas Simon (1999-2000)
 *
 * Last change:
 *   $Date$ by $Author$
 *   $Revision$
 *
 * This file is part of Mozart, an implementation
 * of Oz 3:
 *   http://www.mozart-oz.org
 *
 * See the file "LICENSE" or
 *   http://www.mozart-oz.org/LICENSE.html
 * for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL
 * WARRANTIES.
 *
 */

#include <gtk/gtk.h>
#include <mozart.h>
/*#include <goz_support.h>*/
#define GOZ_DECLARE_GTKOBJECT(i, val)            OZ_declareForeignType (i, val, GtkObject*)

OZ_Term signal_port;

OZ_BI_define (ozgtk_initialize_signal_port, 1, 0)
{
  OZ_declareTerm (0, port);
  signal_port = port;
  return OZ_ENTAILED;
} OZ_BI_end

/*
  Process all events in the queue
 */
OZ_BI_define (ozgtk_handle_pending_events, 0, 0)
{
  while (gtk_events_pending())
    gtk_main_iteration();
  return OZ_ENTAILED;
} OZ_BI_end

/*
  Put the signal id into the Oz stream
  This function is not to be exported via the OZ/C-interface
 */
void
signal_marshal (GtkObject * object,
		gpointer    oz_id,  /* This pointer holds an guint */
		guint       n_args,
		GtkArg *    args)
{
  /* OZ_warning ("Marshaller: object: %p, n_args: %i; oz_id: %i, args: %p",
     object, n_args, (guint) oz_id, args); */
  /* insert signal into the Oz signal queue */
  OZ_send (signal_port, OZ_int ((guint) oz_id));
  /*  OZ_warning ("Marshaller sent signal %i to port", (guint) oz_id); */
}

OZ_BI_define (ozgtk_signal_connect, 3, 1)
{
  /*
   * The callback function will allways be NULL,
   * we only use our marshaller
   *
   */
  guint id;
  GOZ_DECLARE_GTKOBJECT (0, object);
  OZ_declareTerm (1, _name); /* No strings for signals but atoms */
  gchar * name = (gchar *) OZ_atomToC(_name);
  OZ_declareInt (2, oz_id);
  id = gtk_signal_connect_full (GTK_OBJECT (object),
                                name,
                                NULL,
                                signal_marshal,
                                (gpointer) oz_id,
                                NULL, /* TODO: add a destroy notify function */
                                FALSE, /* TODO: verify if defaults are correct */
                                FALSE);
  OZ_RETURN_INT (id);
} OZ_BI_end

OZ_BI_define (ozgtk_signal_emit_by_name, 2, 0)
{
  GOZ_DECLARE_GTKOBJECT (0, object);
  OZ_declareTerm (1, _name); /* No strings for signals but atoms */
  gchar * name = (gchar *) OZ_atomToC(_name);
  gtk_signal_emit_by_name(object, name);
  return OZ_ENTAILED;
} OZ_BI_end

GList *
goz_oz_list_to_g_list(OZ_Term ozlist)
{
  guint len;
  guint i;
  GList * glist = NULL;

  len = OZ_length(ozlist);
  g_return_val_if_fail (len > 0, glist);
  OZ_warning ("length:%i\n", len);

  for (i=0; i < len; i++) {
    glist = g_list_prepend (glist, (gpointer) OZ_getForeignPointer (OZ_head (ozlist)));
    ozlist = OZ_tail (ozlist);
  }

  glist = g_list_reverse (glist); /* prevail order */

  return glist;
}

/*****************************************************************************
 * Gtk macros
 *****************************************************************************/

OZ_BI_define (ozgtk_widget_visible, 1, 1)
{
  GOZ_DECLARE_GTKOBJECT (0, wid);
  OZ_RETURN_BOOL (GTK_WIDGET_VISIBLE(wid));
} OZ_BI_end

OZ_BI_define (ozgtk_widget_sensitive, 1, 1)
{
  GOZ_DECLARE_GTKOBJECT (0, wid);
  OZ_RETURN_BOOL (GTK_WIDGET_SENSITIVE(wid));
} OZ_BI_end

OZ_BI_define (ozgtk_widget_set_flags, 2, 0)
{
  GOZ_DECLARE_GTKOBJECT (0, wid);
  OZ_declareInt (1, flag);
  GTK_WIDGET_SET_FLAGS(wid,flag);
  return OZ_ENTAILED;
} OZ_BI_end

OZ_BI_define (ozgtk_widget_unset_flags, 2, 0)
{
  GOZ_DECLARE_GTKOBJECT (0, wid);
  OZ_declareInt (1, flag);
  GTK_WIDGET_UNSET_FLAGS(wid,flag);
  return OZ_ENTAILED;
} OZ_BI_end

/*****************************************************************************
 * Dialog
 *****************************************************************************/

OZ_BI_define (ozgtk_dialog_window, 1, 1)
{
  OZ_declareForeignType (0, dialog, GtkWidget *);
  OZ_RETURN (OZ_makeForeignPointer(dialog->window));
} OZ_BI_end

OZ_BI_define (ozgtk_dialog_vbox, 1, 1)
{
  OZ_declareForeignType (0, dialog, GtkDialog *);
  OZ_RETURN (OZ_makeForeignPointer(dialog->vbox));
} OZ_BI_end

OZ_BI_define (ozgtk_dialog_action_area, 1, 1)
{
  OZ_declareForeignType (0, dialog, GtkDialog *);
  OZ_RETURN (OZ_makeForeignPointer(dialog->action_area));
} OZ_BI_end

OZ_BI_define (ozgtk_color_selection_dialog_colorsel, 1, 1)
{
  OZ_declareForeignType (0, color_selection_dialog, GtkColorSelectionDialog *);
  OZ_RETURN (OZ_makeForeignPointer(color_selection_dialog->colorsel));
} OZ_BI_end

OZ_BI_define (ozgtk_color_selection_dialog_cancelButton, 1, 1)
{
  OZ_declareForeignType (0, color_selection_dialog, GtkColorSelectionDialog *);
  OZ_RETURN (OZ_makeForeignPointer(color_selection_dialog->cancel_button));
} OZ_BI_end

OZ_BI_define (ozgtk_color_selection_dialog_ok_button, 1, 1)
{
  OZ_declareForeignType (0, color_selection_dialog, GtkColorSelectionDialog *);
  OZ_RETURN (OZ_makeForeignPointer(color_selection_dialog->ok_button));
} OZ_BI_end