/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpcontainertreestore.c
 * Copyright (C) 2010 Michael Natterer <mitch@gimp.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>

#include "widgets-types.h"

#include "core/gimpcontainer.h"
#include "core/gimpviewable.h"

#include "gimpcontainertreestore.h"
#include "gimpcontainerview.h"
#include "gimpviewrenderer.h"


enum
{
  PROP_0,
  PROP_CONTAINER_VIEW
};


typedef struct _GimpContainerTreeStorePrivate GimpContainerTreeStorePrivate;

struct _GimpContainerTreeStorePrivate
{
  GimpContainerView *container_view;
};

#define GET_PRIVATE(store) \
        G_TYPE_INSTANCE_GET_PRIVATE (store, \
                                     GIMP_TYPE_CONTAINER_TREE_STORE, \
                                     GimpContainerTreeStorePrivate)


static GObject * gimp_container_tree_store_constructor     (GType                   type,
                                                            guint                   n_params,
                                                            GObjectConstructParam  *params);
static void      gimp_container_tree_store_finalize        (GObject                *object);
static void      gimp_container_tree_store_set_property    (GObject                *object,
                                                            guint                   property_id,
                                                            const GValue           *value,
                                                            GParamSpec             *pspec);
static void      gimp_container_tree_store_get_property    (GObject                *object,
                                                            guint                   property_id,
                                                            GValue                 *value,
                                                            GParamSpec             *pspec);

static void      gimp_container_tree_store_set             (GimpContainerTreeStore *store,
                                                            GtkTreeIter            *iter,
                                                            GimpViewable           *viewable);
static void      gimp_container_tree_store_renderer_update (GimpViewRenderer       *renderer,
                                                            GimpContainerTreeStore *store);


G_DEFINE_TYPE (GimpContainerTreeStore, gimp_container_tree_store,
               GTK_TYPE_TREE_STORE)

#define parent_class gimp_container_tree_store_parent_class


static void
gimp_container_tree_store_class_init (GimpContainerTreeStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor  = gimp_container_tree_store_constructor;
  object_class->finalize     = gimp_container_tree_store_finalize;
  object_class->set_property = gimp_container_tree_store_set_property;
  object_class->get_property = gimp_container_tree_store_get_property;

  g_object_class_install_property (object_class, PROP_CONTAINER_VIEW,
                                   g_param_spec_object ("container-view",
                                                        NULL, NULL,
                                                        GIMP_TYPE_CONTAINER_VIEW,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (GimpContainerTreeStorePrivate));
}

static void
gimp_container_tree_store_init (GimpContainerTreeStore *store)
{
}

static GObject *
gimp_container_tree_store_constructor (GType                  type,
                                       guint                  n_params,
                                       GObjectConstructParam *params)
{
  GimpContainerTreeStore *store;
  GObject                *object;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  store = GIMP_CONTAINER_TREE_STORE (object);

  return object;
}

static void
gimp_container_tree_store_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_container_tree_store_set_property (GObject      *object,
                                        guint         property_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GimpContainerTreeStorePrivate *private = GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_CONTAINER_VIEW:
      private->container_view = g_value_get_object (value); /* don't ref */
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_container_tree_store_get_property (GObject    *object,
                                        guint       property_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GimpContainerTreeStorePrivate *private = GET_PRIVATE (object);

  switch (property_id)
    {
    case PROP_CONTAINER_VIEW:
      g_value_set_object (value, private->container_view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}


/*  public functions  */

GtkTreeModel *
gimp_container_tree_store_new (GimpContainerView *container_view,
                               gint               n_columns,
                               GType             *types)
{
  GimpContainerTreeStore *store;

  g_return_val_if_fail (GIMP_IS_CONTAINER_VIEW (container_view), NULL);
  g_return_val_if_fail (n_columns >= GIMP_CONTAINER_TREE_STORE_N_COLUMNS, NULL);
  g_return_val_if_fail (types != NULL, NULL);

  store = g_object_new (GIMP_TYPE_CONTAINER_TREE_STORE,
                        "container-view", container_view,
                        NULL);

  gtk_tree_store_set_column_types (GTK_TREE_STORE (store), n_columns, types);

  return GTK_TREE_MODEL (store);
}

static gboolean
gimp_container_tree_store_set_context_foreach (GtkTreeModel *model,
                                               GtkTreePath  *path,
                                               GtkTreeIter  *iter,
                                               gpointer      data)
{
  GimpContext      *context = data;
  GimpViewRenderer *renderer;

  gtk_tree_model_get (model, iter,
                      GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer,
                      -1);

  gimp_view_renderer_set_context (renderer, context);

  g_object_unref (renderer);

  return FALSE;
}

void
gimp_container_tree_store_set_context (GimpContainerTreeStore *store,
                                       GimpContext            *context)
{
  g_return_if_fail (GIMP_IS_CONTAINER_TREE_STORE (store));

  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          gimp_container_tree_store_set_context_foreach,
                          context);
}

GtkTreeIter *
gimp_container_tree_store_insert_item (GimpContainerTreeStore *store,
                                       GimpViewable           *viewable,
                                       GtkTreeIter            *parent,
                                       gint                    index)
{
  GtkTreeIter iter;

  g_return_val_if_fail (GIMP_IS_CONTAINER_TREE_STORE (store), NULL);

  if (index == -1)
    gtk_tree_store_append (GTK_TREE_STORE (store), &iter, parent);
  else
    gtk_tree_store_insert (GTK_TREE_STORE (store), &iter, parent, index);

  gimp_container_tree_store_set (store, &iter, viewable);

  return gtk_tree_iter_copy (&iter);
}

void
gimp_container_tree_store_remove_item (GimpContainerTreeStore *store,
                                       GimpViewable           *viewable,
                                       GtkTreeIter            *iter)
{
  if (iter)
    gtk_tree_store_remove (GTK_TREE_STORE (store), iter);
}

void
gimp_container_tree_store_reorder_item (GimpContainerTreeStore *store,
                                        GimpViewable           *viewable,
                                        gint                    new_index,
                                        GtkTreeIter            *iter)
{
  GimpContainerTreeStorePrivate *private = GET_PRIVATE (store);
  GimpViewable                  *parent;
  GimpContainer                 *container;

  g_return_if_fail (GIMP_IS_CONTAINER_TREE_STORE (store));

  private = GET_PRIVATE (store);

  if (! iter)
    return;

  parent = gimp_viewable_get_parent (viewable);

  if (parent)
    container = gimp_viewable_get_children (parent);
  else
    container = gimp_container_view_get_container (private->container_view);

  if (new_index == -1 ||
      new_index == gimp_container_get_n_children (container) - 1)
    {
      gtk_tree_store_move_before (GTK_TREE_STORE (store), iter, NULL);
    }
  else if (new_index == 0)
    {
      gtk_tree_store_move_after (GTK_TREE_STORE (store), iter, NULL);
    }
  else
    {
      GtkTreePath *path;
      GtkTreeIter  place_iter;
      gint         depth;
      gint        *indices;
      gint         old_index;

      path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
      indices = gtk_tree_path_get_indices (path);

      depth = gtk_tree_path_get_depth (path);

      old_index = indices[depth - 1];

      if (new_index != old_index)
        {
          indices[depth - 1] = new_index;

          gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &place_iter, path);

          if (new_index > old_index)
            gtk_tree_store_move_after (GTK_TREE_STORE (store),
                                       iter, &place_iter);
          else
            gtk_tree_store_move_before (GTK_TREE_STORE (store),
                                        iter, &place_iter);
        }

      gtk_tree_path_free (path);
    }
}

gboolean
gimp_container_tree_store_rename_item (GimpContainerTreeStore *store,
                                       GimpViewable           *viewable,
                                       GtkTreeIter            *iter)
{
  gboolean new_name_shorter = FALSE;

  g_return_val_if_fail (GIMP_IS_CONTAINER_TREE_STORE (store), FALSE);

  if (iter)
    {
      gchar *name = gimp_viewable_get_description (viewable, NULL);
      gchar *old_name;

      gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
                          GIMP_CONTAINER_TREE_STORE_COLUMN_NAME, &old_name,
                          -1);

      gtk_tree_store_set (GTK_TREE_STORE (store), iter,
                          GIMP_CONTAINER_TREE_STORE_COLUMN_NAME, name,
                          -1);

      if (name && old_name && strlen (name) < strlen (old_name))
        new_name_shorter = TRUE;

      g_free (name);
      g_free (old_name);
    }

  return new_name_shorter;
}

void
gimp_container_tree_store_clear_items (GimpContainerTreeStore *store)
{
  g_return_if_fail (GIMP_IS_CONTAINER_TREE_STORE (store));

  gtk_tree_store_clear (GTK_TREE_STORE (store));
}

typedef struct
{
  gint view_size;
  gint border_width;
} SetSizeForeachData;

static gboolean
gimp_container_tree_store_set_view_size_foreach (GtkTreeModel *model,
                                                 GtkTreePath  *path,
                                                 GtkTreeIter  *iter,
                                                 gpointer      data)
{
  SetSizeForeachData *size_data = data;
  GimpViewRenderer   *renderer;

  gtk_tree_model_get (model, iter,
                      GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer,
                      -1);

  gimp_view_renderer_set_size (renderer,
                               size_data->view_size,
                               size_data->border_width);

  g_object_unref (renderer);

  return FALSE;
}

void
gimp_container_tree_store_set_view_size (GimpContainerTreeStore *store)
{
  GimpContainerTreeStorePrivate *private;
  SetSizeForeachData             size_data;

  g_return_if_fail (GIMP_IS_CONTAINER_TREE_STORE (store));

  private = GET_PRIVATE (store);

  size_data.view_size =
    gimp_container_view_get_view_size (private->container_view,
                                       &size_data.border_width);

  gtk_tree_model_foreach (GTK_TREE_MODEL (store),
                          gimp_container_tree_store_set_view_size_foreach,
                          &size_data);
}


/*  private functions  */

gint
gimp_container_tree_store_columns_add (GType *types,
                                       gint  *n_types,
                                       GType  type)
{
  g_return_val_if_fail (types != NULL, 0);
  g_return_val_if_fail (n_types != NULL, 0);
  g_return_val_if_fail (*n_types >= 0, 0);

  types[*n_types] = type;
  (*n_types)++;

  return *n_types - 1;
}

static void
gimp_container_tree_store_set (GimpContainerTreeStore *store,
                               GtkTreeIter            *iter,
                               GimpViewable           *viewable)
{
  GimpContainerTreeStorePrivate *private = GET_PRIVATE (store);
  GimpContext                   *context;
  GimpViewRenderer              *renderer;
  gchar                         *name;
  gint                           view_size;
  gint                           border_width;

  context = gimp_container_view_get_context (private->container_view);

  view_size = gimp_container_view_get_view_size (private->container_view,
                                                 &border_width);

  renderer = gimp_view_renderer_new (context,
                                     G_TYPE_FROM_INSTANCE (viewable),
                                     view_size, border_width,
                                     FALSE);
  gimp_view_renderer_set_viewable (renderer, viewable);
  gimp_view_renderer_remove_idle (renderer);

  g_signal_connect (renderer, "update",
                    G_CALLBACK (gimp_container_tree_store_renderer_update),
                    store);

  name = gimp_viewable_get_description (viewable, NULL);

  gtk_tree_store_set (GTK_TREE_STORE (store), iter,
                      GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER,       renderer,
                      GIMP_CONTAINER_TREE_STORE_COLUMN_NAME,           name,
                      GIMP_CONTAINER_TREE_STORE_COLUMN_NAME_SENSITIVE, TRUE,
                      -1);

  g_free (name);
  g_object_unref (renderer);
}

static void
gimp_container_tree_store_renderer_update (GimpViewRenderer       *renderer,
                                           GimpContainerTreeStore *store)
{
  GimpContainerTreeStorePrivate *private = GET_PRIVATE (store);
  GtkTreeIter                   *iter;

  iter = gimp_container_view_lookup (private->container_view,
                                     renderer->viewable);

  if (iter)
    {
      GtkTreePath *path;

      path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
      gtk_tree_model_row_changed (GTK_TREE_MODEL (store), path, iter);
      gtk_tree_path_free (path);
    }
}