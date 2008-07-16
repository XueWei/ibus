/* vim:set et ts=4: */
/* IBus - The Input Bus
 * Copyright (C) 2008-2009 Huang Peng <shawn.p.huang@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __IBUS_IM_CONTEXT_H_
#define __IBUS_IM_CONTEXT_H_

#include <gtk/gtk.h>

/*
 * Type macros.
 */
#define IBUS_TYPE_IM_CONTEXT             \
    (_ibus_type_im_context)
#define IBUS_IM_CONTEXT(obj)             \
    (GTK_CHECK_CAST ((obj), IBUS_TYPE_IM_CONTEXT, IBusIMContext))
#define IBUS_IM_CONTEXT_CLASS(klass)     \
    (GTK_CHECK_CLASS_CAST ((klass), IBUS_TYPE_IM_CONTEXT, IBusIMContextClass))
#define IBUS_IS_IM_CONTEXT(obj)          \
    (GTK_CHECK_TYPE ((obj), IBUS_TYPE_IM_CONTEXT))
#define IBUS_IS_IM_CONTEXT_CLASS(klass)  \
    (GTK_CHECK_CLASS_TYPE ((klass), IBUS_TYPE_IM_CONTEXT))
#define IBUS_IM_CONTEXT_GET_CLASS(obj)   \
    (GTK_CHECK_GET_CLASS ((obj), IBUS_TYPE_IM_CONTEXT, IBusIMContextClass))

G_BEGIN_DECLS
typedef struct _IBusIMContext IBusIMContext;
typedef struct _IBusIMContextClass IBusIMContextClass;
typedef struct _IBusIMContextPrivate IBusIMContextPrivate;

struct _IBusIMContext {
  GtkIMContext parent;
  /* instance members */
  IBusIMContextPrivate *priv;
};

struct _IBusIMContextClass {
  GtkIMContextClass parent;
  /* class members */
};

extern GType                _ibus_type_im_context;

GtkIMContext
        *ibus_im_context_new    (void);
void    ibus_im_context_register_type
                                (GTypeModule    *type_module);
void    ibus_im_context_shutdown
                                (void);
gchar   *ibus_im_context_get_ic (IBusIMContext  *context);
void    ibus_im_context_set_ic  (IBusIMContext  *context,
                                 const gchar    *ic);
void    ibus_im_context_enable  (IBusIMContext  *context);
void    ibus_im_context_disable (IBusIMContext  *context);
void    ibus_im_context_commit_string
                                (IBusIMContext  *context,
                                const gchar     *string);
void    ibus_im_context_update_preedit
                                (IBusIMContext  *context,
                                const gchar     *string,
                                PangoAttrList   *attrs,
                                gint            cursor_pos,
                                gboolean        show);
G_END_DECLS
#endif

