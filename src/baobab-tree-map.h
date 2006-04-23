/*
 **************************************************************************
 *            baobab-treemap.h
 *
 *  Copyright  2005  Fabio Marzocca <thesaltydog@gmail.com>
 *  
 *
 *  Draw a graphical treemap from a hierarchical data structure
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 ***************************************************************************
 */

#ifndef __BAOBAB_TREE_MAP_H__
#define __BAOBAB_TREE_MAP_H__

#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/libgnomecanvas.h>


G_BEGIN_DECLS
/*
 * Type checking and casting macros
 */
#define BAOBAB_TYPE_TREE_MAP              (baobab_tree_map_get_type())
#define BAOBAB_TREE_MAP(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), BAOBAB_TYPE_TREE_MAP, BaobabTreeMap))
#define BAOBAB_TREE_MAP_CONST(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), BAOBAB_TREE_MAP, BaobabTreeMap const))
#define BAOBAB_TREE_MAP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), BAOBAB_TREE_MAP, BaobabTreeMapClass))
#define BAOBAB_IS_TREE_MAP(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), BAOBAB_TYPE_TREE_MAP))
#define BAOBAB_IS_TREE_MAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), BAOBAB_TYPE_TREE_MAP))
#define BAOBAB_TREE_MAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), BAOBAB_TYPE_TREE_MAP, BaobabTreeMapClass))


/* Private structure type */
typedef struct _BaobabTreeMapPrivate BaobabTreeMapPrivate;

/*
 * Main object structure
 */
typedef struct _BaobabTreeMap BaobabTreeMap;

struct _BaobabTreeMap 
{	

	GnomeCanvas parent;
	
	/*< private > */
	BaobabTreeMapPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _BaobabTreeMapClass BaobabTreeMapClass;

struct _BaobabTreeMapClass 
{
	GnomeCanvasClass parent_class;
};


/*
 * Public methods
 */
GType		baobab_tree_map_get_type 		(void) G_GNUC_CONST;

BaobabTreeMap	*baobab_tree_map_new			(void);
void 		baobab_tree_map_clear			(BaobabTreeMap *tm);
gint		baobab_tree_map_get_total_elements	(BaobabTreeMap *tm);
void 		baobab_tree_map_set_zoom 		(BaobabTreeMap *tm, gdouble new_zoom);
gdouble 	baobab_tree_map_get_zoom 		(BaobabTreeMap *tm);
GdkPixbuf 	*baobab_tree_map_get_pixbuf 		(BaobabTreeMap *tm);
const gchar 	*baobab_tree_map_get_selected_item_name	(BaobabTreeMap *tm);
void		baobab_tree_map_refresh			(BaobabTreeMap *tm, gint new_depth);
void		baobab_tree_map_draw 			(BaobabTreeMap *tm,
							GtkTreeModel *model,
		     					GtkTreePath *path,
		     					gint nNameCol,
		     					gint nSizeCol,
		     					gint required_depth);
G_END_DECLS



#endif /* __BAOBAB_TREE_MAP_H__ */
