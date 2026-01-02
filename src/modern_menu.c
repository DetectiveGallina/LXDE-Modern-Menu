/*
 * Modern Menu Plugin for LXPanel
 * Based on lxpanel-plugin-example structure
 */
#define _GNU_SOURCE

/* ========== SECCIÓN 1: INCLUDES ========== */
#include <lxpanel/plugin.h>
#include <lxpanel/misc.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gdk/gdkkeysyms.h>
#include <menu-cache/menu-cache.h>
#include <libintl.h>
#include <libfm/fm-gtk.h>
#include <libfm/fm-utils.h>
#include <libfm/fm.h>
#include <string.h>


/* ========== SECCIÓN 2: DEFINES Y MACROS ========== */
#ifndef APPS_PER_ROW
#define APPS_PER_ROW 3  // Cantidad máxima de aplicaciones que se van a mostrar por fila
#endif
/* CONFIGURACIÓN GETTEXT */

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(String) gettext(String)
#define N_(String) (String)
#else
#define _(String) (String)
#define N_(String) (String)
#endif

/* ========== SECCIÓN 3: ESTRUCTURAS ========== */
typedef struct {
    // UI widgets
    GtkWidget *icon, *window, *search, *categories, *apps_box, *apps_scroll, *plugin_button, *btn_fav;

    // Datos
    MenuCache *menu_cache;
    MenuCacheDir *current_dir;
    GtkListStore *cat_store;
    LXPanel *panel;
    config_setting_t *settings;

    // Listas
    GSList *all_apps, *favorites, *hidden_apps;

    // Paths
    gchar *favorites_path, *icon_path;

    // Estado
    gboolean window_shown, suppress_hide, switching_category;
    gpointer reload_notify;
    FmDndSrc *ds;
} ModernMenu;

enum {
    COL_NAME = 0,
    COL_DIR_PTR,
    N_COLS
};

extern void logout(void); // Llama a la función de logout de la configuración

/* ========== SECCIÓN 4: DECLARACIONES ========== */

// Iconos y gráficos
static GdkPixbuf *get_app_icon(MenuCacheItem *item, int size);

// UI y widgets
static GtkWidget* create_app_button(MenuCacheItem *item, ModernMenu *m);
static void populate_apps_for_dir(ModernMenu *m, MenuCacheDir *dir);
static void show_favorites_category(GtkWidget *widget, gpointer user_data);

// Datos y persistencia
static void load_favorites(ModernMenu *m);
static void save_favorites(ModernMenu *m);
static void load_hidden_apps(ModernMenu *m);
static void build_all_apps_list(ModernMenu *m);

// Eventos y callbacks
static void show_error_dialog(const gchar *message);
static void show_properties(GtkMenuItem *menuitem, gpointer user_data);
static void launch_app_from_item(GtkWidget *button, gpointer user_data);
static gboolean on_app_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static void on_app_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer user_data);
static void on_app_drag_data_get(FmDndSrc *ds, GtkWidget *btn);
static void on_remove_package(GtkWidget *widget, gpointer user_data);
static void toggle_favorite(GtkWidget *menuitem, gpointer user_data);

// Menú contextual
static void show_context_menu(GtkWidget *app_button, ModernMenu *m, GdkEventButton *event);
static void on_context_menu_done(GtkMenuShell *menu_shell, gpointer user_data);
static void add_to_desktop(GtkMenuItem *menuitem, gpointer user_data);

// Plugin core
static void modern_menu_destructor(gpointer user_data);
static gboolean modernmenu_apply_config(gpointer user_data);
static void on_menu_cache_reload_real(MenuCache *cache, gpointer user_data);

/* ========== SECCIÓN 5: IMPLEMENTACIONES ========== */

/* ----- 5.1 Iconos y gráficos ----- */


/* Obtener el icono de la app */
static GdkPixbuf *get_app_icon(MenuCacheItem *item, int size)
{
    const char *icon_name = menu_cache_item_get_icon(item);
    if (!icon_name || !*icon_name) {
        icon_name = "application-x-executable"; // fallback seguro
    }

    GtkIconTheme *theme = gtk_icon_theme_get_default();
    GdkPixbuf *pb = NULL;

    // Si es ruta absoluta
    if (g_path_is_absolute(icon_name)) {
        pb = gdk_pixbuf_new_from_file_at_scale(icon_name, size, size, TRUE, NULL);
        if (pb) return pb;
    }

    // Quita extensión (.png, .svg, etc.)
    gchar *icon_no_ext = g_strdup(icon_name);
    gchar *dot = strrchr(icon_no_ext, '.');
    if (dot && (g_strcmp0(dot, ".png") == 0 ||
        g_strcmp0(dot, ".svg") == 0 ||
        g_strcmp0(dot, ".xpm") == 0)) {
        *dot = '\0';
        }

        // Busca en el tema
        pb = gtk_icon_theme_load_icon(theme, icon_no_ext, size,
                                      GTK_ICON_LOOKUP_USE_BUILTIN |
                                      GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

        g_free(icon_no_ext);

        // Fallback si no se encontró
        if (!pb) {
            pb = gtk_icon_theme_load_icon(theme, "application-x-executable", size,
                                          GTK_ICON_LOOKUP_USE_BUILTIN |
                                          GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        }

        return pb;
}

/* ===== 5.2 FUNCIONES DE DATOS Y PERSISTENCIA ===== */
/* ==== FAVORITOS ==== */
static void load_favorites(ModernMenu *m)
{
    g_return_if_fail(m != NULL);

    gchar *config_dir = g_build_filename(g_get_user_config_dir(), "modernmenu", NULL);
    g_mkdir_with_parents(config_dir, 0700);
    m->favorites_path = g_build_filename(config_dir, "favorites.list", NULL);
    g_free(config_dir);

    m->favorites = NULL;

    if (!g_file_test(m->favorites_path, G_FILE_TEST_EXISTS))
        return;

    gchar *content = NULL;
    if (g_file_get_contents(m->favorites_path, &content, NULL, NULL) && content) {
        gchar **lines = g_strsplit(content, "\n", -1);
        for (int i = 0; lines[i]; i++) {
            if (g_strcmp0(lines[i], "") != 0)
                m->favorites = g_slist_prepend(m->favorites, g_strdup(lines[i]));
        }
        g_strfreev(lines);
        g_free(content);
    }
}

static void save_favorites(ModernMenu *m)
{
    if (!m || !m->favorites_path) return;

    GString *data = g_string_new("");
    for (GSList *l = m->favorites; l; l = l->next)
        g_string_append_printf(data, "%s\n", (char*)l->data);
    g_file_set_contents(m->favorites_path, data->str, -1, NULL);
    g_string_free(data, TRUE);
}

static gboolean is_favorite(ModernMenu *m, const char *app_id)
{
    if (!app_id) return FALSE;

    for (GSList *l = m->favorites; l; l = l->next) {
        if (g_strcmp0(l->data, app_id) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static void toggle_favorite(GtkWidget *menuitem, gpointer user_data)
{
    GtkWidget *btn = GTK_WIDGET(user_data);
    ModernMenu *m = g_object_get_data(G_OBJECT(btn), "modern-menu");
    MenuCacheItem *item = g_object_get_data(G_OBJECT(btn), "menu-item");

    if (!m || !item) return;

    const char *id = menu_cache_item_get_id(item);
    if (!id) return;

    if (is_favorite(m, id)) {
        GSList *node = m->favorites;
        while (node) {
            if (g_strcmp0(node->data, id) == 0) {
                g_free(node->data);
                m->favorites = g_slist_delete_link(m->favorites, node);
                break;
            }
            node = node->next;
        }
    } else {
        m->favorites = g_slist_prepend(m->favorites, g_strdup(id));
    }

    save_favorites(m);

    // Actualizar label del menu contextual
    GtkWidget *menu = gtk_widget_get_parent(menuitem);
    if (menu) {
        gtk_widget_destroy(menuitem);
        GtkWidget *new_item;
        if (is_favorite(m, id))
            new_item = gtk_menu_item_new_with_label(_("Remove from Favorites"));
        else
            new_item = gtk_menu_item_new_with_label(_("Add to Favorites"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_item);
        gtk_widget_show(new_item);
        g_signal_connect(new_item, "activate", G_CALLBACK(toggle_favorite), btn);
    }
}
/* ==== FIN FAVORITOS ==== */

/* ===== OCULTAS ===== */

/* Cargar lista de apps ocultas desde archivo */
static void load_hidden_apps(ModernMenu *m)
{
    if (m->hidden_apps) {
        g_slist_free_full(m->hidden_apps, g_free);
        m->hidden_apps = NULL;
    }

    const char *home = g_get_home_dir();
    gchar *hidden_file = g_build_filename(home, ".config", "modernmenu", "hidden.list", NULL);

    FILE *f = fopen(hidden_file, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            g_strstrip(line);
            if (*line && *line != '#') {
                m->hidden_apps = g_slist_prepend(m->hidden_apps, g_strdup(line));
            }
        }
        fclose(f);
    }
    g_free(hidden_file);
}

/* Guardar lista de apps ocultas */
static void save_hidden_apps(ModernMenu *m)
{
    const char *home = g_get_home_dir();
    gchar *config_dir = g_build_filename(home, ".config", "modernmenu", NULL);
    g_mkdir_with_parents(config_dir, 0755);

    gchar *hidden_file = g_build_filename(config_dir, "hidden.list", NULL);
    FILE *f = fopen(hidden_file, "w");

    if (f) {
        fprintf(f, _("# Hidden applications for Modern Menu\n"));
        for (GSList *l = m->hidden_apps; l; l = l->next) {
            fprintf(f, "%s\n", (char *)l->data);
        }
        fclose(f);
    }

    g_free(hidden_file);
    g_free(config_dir);
}

/* Verificar si una app está oculta */
static gboolean is_hidden(ModernMenu *m, const char *app_id)
{
    if (!app_id || !m->hidden_apps) return FALSE;
    return (g_slist_find_custom(m->hidden_apps, app_id, (GCompareFunc)g_strcmp0) != NULL);
}

static void toggle_hidden(GtkMenuItem *item, gpointer user_data)
{
    GtkWidget *app_button = GTK_WIDGET(user_data);
    MenuCacheItem *menu_item = g_object_get_data(G_OBJECT(app_button), "menu-item");
    ModernMenu *m = g_object_get_data(G_OBJECT(app_button), "modern-menu");

    if (!menu_item || !m) return;

    const char *app_id = menu_cache_item_get_id(menu_item);
    if (!app_id) return;

    GSList *found = g_slist_find_custom(m->hidden_apps, app_id, (GCompareFunc)g_strcmp0);

    if (found) {
        // Mostrar
        m->hidden_apps = g_slist_remove(m->hidden_apps, found->data);
        g_free(found->data);
    } else {
        // Ocultar
        m->hidden_apps = g_slist_prepend(m->hidden_apps, g_strdup(app_id));
    }

    save_hidden_apps(m);

    // Refrescar la vista actual
    if (m->current_dir) {
        populate_apps_for_dir(m, m->current_dir);
    } else {
        show_favorites_category(NULL, m);
    }
}
/* ===== FIN OCULTAS ===== */

static void load_categories(ModernMenu *m)
{
    if (!m || !m->cat_store || !m->menu_cache) return;

    gtk_list_store_clear(m->cat_store);

    #if MENU_CACHE_CHECK_VERSION(0,4,0)
    MenuCacheDir *root = menu_cache_dup_root_dir(m->menu_cache);
    #else
    MenuCacheDir *root = menu_cache_get_root_dir(m->menu_cache);
    #endif
    if (!root) return;

    GSList *children = menu_cache_dir_list_children(root);
    gboolean first = TRUE;
    GtkTreeIter first_iter;

    for (GSList *l = children; l; l = l->next) {
        MenuCacheItem *item = MENU_CACHE_ITEM(l->data);
        if (menu_cache_item_get_type(item) != MENU_CACHE_TYPE_DIR)
            continue;

        const char *name = menu_cache_item_get_name(item);
        MenuCacheDir *dir_ptr = MENU_CACHE_DIR(item);


        GtkTreeIter iter;
        gtk_list_store_append(m->cat_store, &iter);
        gtk_list_store_set(m->cat_store, &iter,
                           COL_NAME, name ? name : "",
                           COL_DIR_PTR, dir_ptr,
                           -1);

        if (first) {
            first_iter = iter;
            first = FALSE;
        }
    }

    if (!first) {
        GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(m->categories));
        gtk_tree_selection_select_iter(sel, &first_iter);
    }

    g_slist_foreach(children, (GFunc)menu_cache_item_unref, NULL);
    g_slist_free(children);

    #if MENU_CACHE_CHECK_VERSION(0,4,0)
    menu_cache_item_unref(MENU_CACHE_ITEM(root));
    #endif
}
static void build_all_apps_list(ModernMenu *m)
{
    if (!m || !m->menu_cache) return;

    if (m->all_apps) {
        g_slist_free_full(m->all_apps, (GDestroyNotify)menu_cache_item_unref);
        m->all_apps = NULL;
    }

    #if MENU_CACHE_CHECK_VERSION(0,4,0)
    MenuCacheDir *root = menu_cache_dup_root_dir(m->menu_cache);
    #else
    MenuCacheDir *root = menu_cache_get_root_dir(m->menu_cache);
    #endif
    if (!root) return;

    GSList *stack = g_slist_append(NULL, root);
    GSList *added_ids = NULL; // <--- lista temporal de IDs únicos

    while (stack) {
        MenuCacheDir *dir = stack->data;
        stack = g_slist_delete_link(stack, stack);

        GSList *children = menu_cache_dir_list_children(dir);
        for (GSList *l = children; l; l = l->next) {
            MenuCacheItem *item = MENU_CACHE_ITEM(l->data);
            if (menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP) {
                const char *id = menu_cache_item_get_id(item);
                if (!g_slist_find_custom(added_ids, id, (GCompareFunc)g_strcmp0)) {
                    m->all_apps = g_slist_prepend(m->all_apps, menu_cache_item_ref(item));
                    added_ids = g_slist_prepend(added_ids, (gpointer)id);
                }
            } else if (menu_cache_item_get_type(item) == MENU_CACHE_TYPE_DIR) {
                stack = g_slist_prepend(stack, MENU_CACHE_DIR(item));
            }
        }
        g_slist_foreach(children, (GFunc)menu_cache_item_unref, NULL);
        g_slist_free(children);
    }

    m->all_apps = g_slist_reverse(m->all_apps);
    g_slist_free(added_ids); // liberamos la lista temporal

    #if MENU_CACHE_CHECK_VERSION(0,4,0)
    menu_cache_item_unref(MENU_CACHE_ITEM(root));
    #endif
}
static gchar *get_exec_from_desktop(const char *desktop_file)
{
    if (!desktop_file) return NULL;

    GKeyFile *keyfile = g_key_file_new();
    GError *error = NULL;

    if (!g_key_file_load_from_file(keyfile, desktop_file, G_KEY_FILE_NONE, &error)) {
        show_error_dialog(_("Could not load .desktop file."));
        if (error) g_error_free(error);
        g_key_file_free(keyfile);
        return NULL;
    }

    gchar *exec = g_key_file_get_string(keyfile, "Desktop Entry", "Exec", NULL);
    g_key_file_free(keyfile);

    if (!exec || !*exec) {
        show_error_dialog(_("The .desktop file does not have a valid 'Exec' field."));
        g_free(exec);
        return NULL;
    }

    /* Limpiar parámetros tipo %f, %u, %U, etc. */
    gchar *cleaned = g_strdup(exec);
    gchar *percent = strchr(cleaned, '%');
    if (percent) *percent = '\0';

    /* Eliminar comillas */
    gchar *trimmed = g_strstrip(cleaned);
    if (trimmed[0] == '"' || trimmed[0] == '\'') {
        trimmed++;
        size_t len = strlen(trimmed);
        if (len > 0 && (trimmed[len - 1] == '"' || trimmed[len - 1] == '\'')) {
            trimmed[len - 1] = '\0';
        }
    }

    gchar *result = g_strdup(trimmed);
    g_free(cleaned);
    g_free(exec);

    return result;
}

/* ===== 5.3 FUNCIONES DE UI Y WIDGETS ===== */
static GtkWidget* create_app_button(MenuCacheItem *item, ModernMenu *m)
{
    GtkWidget *btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);

    // ===== CREAR FmFileInfo EXACTAMENTE como lxpanel (líneas 407-414) =====
    char *mpath = menu_cache_dir_make_path(MENU_CACHE_DIR(item));
    FmPath *path = fm_path_new_relative(fm_path_get_apps_menu(), mpath + 13);
    /* skip "/Applications" */
    FmFileInfo *fi = fm_file_info_new_from_menu_cache_item(path, item);
    g_free(mpath);
    fm_path_unref(path);

    // GUARDAR el FmFileInfo en el widget con el mismo quark que lxpanel
    static GQuark SYS_MENU_ITEM_ID = 0;
    if (SYS_MENU_ITEM_ID == 0)
        SYS_MENU_ITEM_ID = g_quark_from_static_string("SysMenuItem");

    g_object_set_qdata_full(G_OBJECT(btn), SYS_MENU_ITEM_ID, fi,
                            (GDestroyNotify)fm_file_info_unref);
    // ===== FIN =====

    // También guardar referencias necesarias
    g_object_set_data(G_OBJECT(btn), "menu-item", item);
    g_object_set_data(G_OBJECT(btn), "modern-menu", m);

    // ===== DRAG AND DROP (igual que lxpanel) =====
    gtk_drag_source_set(btn,
                        GDK_BUTTON1_MASK,
                        NULL, 0,
                        GDK_ACTION_COPY);

    g_signal_connect(btn, "drag-begin", G_CALLBACK(on_app_drag_begin), NULL);
    // ===== FIN DRAG AND DROP =====

    // Crear contenido del botón
    GtkWidget *vbox = gtk_vbox_new(FALSE, 4);
    gtk_container_add(GTK_CONTAINER(btn), vbox);

    GtkWidget *img = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_DIALOG);
    GdkPixbuf *pb = get_app_icon(item, 48);
    if (pb) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(img), pb);
        gtk_image_set_pixel_size(GTK_IMAGE(img), 48);
        g_object_unref(pb);
    }
    gtk_misc_set_alignment(GTK_MISC(img), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), img, FALSE, FALSE, 0);

    const char *name = menu_cache_item_get_name(item);
    GtkWidget *lbl = gtk_label_new(name ? name : "");
    gtk_label_set_max_width_chars(GTK_LABEL(lbl), 16);
    gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

    if (name) gtk_widget_set_tooltip_text(btn, name);

    g_signal_connect(btn, "clicked", G_CALLBACK(launch_app_from_item), item);
    g_signal_connect(btn, "button-press-event", G_CALLBACK(on_app_button_press), m);

    gtk_widget_set_size_request(btn, 110, 100);
    gtk_widget_show_all(btn);

    return btn;
}
// Filtro para favoritos
static gboolean filter_favorites(MenuCacheItem *item, ModernMenu *m) {
    const char *id = menu_cache_item_get_id(item);
    return id && is_favorite(m, id);
}
static int add_apps_to_container(GSList *apps_list, ModernMenu *m,
                                 GtkWidget *container,
                                 gboolean (*filter_func)(MenuCacheItem*, ModernMenu*))
{
    int count = 0;
    GtkWidget *current_hbox = NULL;

    for (GSList *l = apps_list; l; l = l->next) {
        MenuCacheItem *item = MENU_CACHE_ITEM(l->data);

        // Aplicar filtro si existe
        if (filter_func && !filter_func(item, m)) {
            continue;
        }

        // Verificar si está oculta
        const char *id = menu_cache_item_get_id(item);
        if (!id || is_hidden(m, id)) {
            continue;
        }

        // Crear nueva fila si es necesario
        if (count % APPS_PER_ROW == 0 || current_hbox == NULL) {
            current_hbox = gtk_hbox_new(TRUE, 8);
            gtk_box_pack_start(GTK_BOX(container), current_hbox, FALSE, FALSE, 4);
            gtk_widget_show(current_hbox);
        }

        // Crear botón y añadir
        GtkWidget *btn = create_app_button(item, m);
        gtk_box_pack_start(GTK_BOX(current_hbox), btn, FALSE, FALSE, 0);
        count++;
    }

    return count;
}


static void populate_apps_for_dir(ModernMenu *m, MenuCacheDir *dir) {
    if (!m || !m->apps_box) return;
    m->current_dir = dir;

    // Limpiar container
    GList *children = gtk_container_get_children(GTK_CONTAINER(m->apps_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    if (!dir) {
        GtkWidget *lbl = gtk_label_new(_("No applications"));
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
        return;
    }

    GSList *list = menu_cache_dir_list_children(dir);
    if (!list) {
        GtkWidget *lbl = gtk_label_new(_("No applications in this category"));
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
        return;
    }

    int count = add_apps_to_container(list, m, m->apps_box, NULL);

    g_slist_foreach(list, (GFunc)menu_cache_item_unref, NULL);
    g_slist_free(list);

    if (count == 0) {
        GtkWidget *lbl = gtk_label_new(_("No applications in this category"));
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
    }
}
static void show_favorites_category(GtkWidget *widget, gpointer user_data) {
    ModernMenu *m = user_data;
    if (!m) return;

    m->switching_category = TRUE;

    if (GTK_IS_TOGGLE_BUTTON(widget)) {
        gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
        if (!active) {
            m->switching_category = FALSE;
            return;
        }
    }

    /* Marcar el botón de favoritos como seleccionado sin disparar de nuevo la señal */
    if (m->btn_fav) {
        g_signal_handlers_block_by_func(m->btn_fav, G_CALLBACK(show_favorites_category), m);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->btn_fav), TRUE);
        g_signal_handlers_unblock_by_func(m->btn_fav, G_CALLBACK(show_favorites_category), m);
    }

    /* Desmarcar la categoría actual para permitir volver a seleccionarla */
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(m->categories));
    gtk_tree_selection_unselect_all(sel);

    // Limpiar container
    GList *children = gtk_container_get_children(GTK_CONTAINER(m->apps_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    if (!m->favorites) {
        // Mostrar mensaje "no hay favoritos"
        GtkWidget *lbl = gtk_label_new(_("There's no favorite applications"));
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
        m->switching_category = FALSE;
        return;
    }

    // ¡USAR HELPER!
    int count = add_apps_to_container(m->all_apps, m, m->apps_box, filter_favorites);

    if (count == 0) {
        GtkWidget *lbl = gtk_label_new(_("No favorite applications visible"));
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
    }

    m->switching_category = FALSE;
}
static void on_search_changed(GtkEditable *entry, gpointer user_data) {
    ModernMenu *m = user_data;
    if (!m || !m->apps_box) return;

    const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
    gboolean empty = (text == NULL || *text == '\0');

    // Limpiar container
    GList *children = gtk_container_get_children(GTK_CONTAINER(m->apps_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    if (empty) {
        populate_apps_for_dir(m, m->current_dir);
        return;
    }

    int count = 0;
    GtkWidget *current_hbox = NULL;

    for (GSList *l = m->all_apps; l; l = l->next) {
        MenuCacheItem *item = MENU_CACHE_ITEM(l->data);
        const char *name = menu_cache_item_get_name(item);
        const char *id = menu_cache_item_get_id(item);

        if (!name || !id || !strcasestr(name, text) || is_hidden(m, id))
            continue;

        if (count % APPS_PER_ROW == 0 || current_hbox == NULL) {
            current_hbox = gtk_hbox_new(TRUE, 8);
            gtk_box_pack_start(GTK_BOX(m->apps_box), current_hbox, FALSE, FALSE, 4);
            gtk_widget_show(current_hbox);
        }

        GtkWidget *btn = create_app_button(item, m);
        gtk_box_pack_start(GTK_BOX(current_hbox), btn, FALSE, FALSE, 0);
        count++;
    }

    if (count == 0) {
        GtkWidget *lbl = gtk_label_new(_("No matching applications found"));
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
    }
}


static void position_window_near_button(ModernMenu *m)
{
    if (!m || !m->plugin_button || !m->window) return;

    GdkWindow *w = gtk_widget_get_window(m->plugin_button);
    if (!w) return;

    gint bx, by;
    gdk_window_get_origin(w, &bx, &by);

    GtkAllocation alloc;
    gtk_widget_get_allocation(m->plugin_button, &alloc);

    GtkRequisition req;
    gtk_widget_size_request(m->window, &req);
    int win_w = req.width;
    int win_h = req.height;

    int x = bx + alloc.x - (win_w / 2) + (alloc.width / 2);
    int y = by - win_h;

    GdkScreen *screen = gdk_screen_get_default();
    int screen_width = gdk_screen_get_width(screen);
    int screen_height = gdk_screen_get_height(screen);

    if (x < 0) x = 0;
    if (x + win_w > screen_width) x = screen_width - win_w;
    if (y < 0) y = 0;
    if (y + win_h > screen_height) y = screen_height - win_h;

    gtk_window_move(GTK_WINDOW(m->window), x, y);
}
static void hide_menu(ModernMenu *m)
{
    if (!m || !m->window) return;

    if (m->suppress_hide) {
        return; // evita cierre si hay menú contextual abierto
    }

    if (m->window_shown) {
        gtk_widget_hide(m->window);
        m->window_shown = FALSE;
        gtk_entry_set_text(GTK_ENTRY(m->search), "");
    }
}

/* ===== 5.4 FUNCIONES DE EVENTOS Y CALLBACKS ===== */
/* Callback cuando se hace click en el botón del menú */
static gboolean on_plugin_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    ModernMenu *m = (ModernMenu *)user_data;

    // Solo responder al click izquierdo
    if (event->button == 1) {
        if (m->window_shown) {
            hide_menu(m);
        } else {
            position_window_near_button(m);
            show_favorites_category(NULL, m);
            gtk_widget_show_all(m->window);
            gtk_window_present(GTK_WINDOW(m->window));
            gtk_widget_grab_focus(m->search);
            m->window_shown = TRUE;
        }
        return TRUE;  // Evento manejado
    }

    return FALSE;  // Dejar pasar otros eventos
}
/* Manejador de clicks */
static gboolean on_app_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    ModernMenu *m = (ModernMenu *)user_data;

    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        show_context_menu(widget, m, event);
        return TRUE;
    } else if (event->button == 1) { // Click izquierdo - preparar drag
        // Desconectar handler anterior
        g_signal_handlers_disconnect_matched(m->ds, G_SIGNAL_MATCH_FUNC,
                                             0, 0, NULL, on_app_drag_data_get, NULL);

        // Reasignar FmDndSrc al botón actual
        fm_dnd_src_set_widget(m->ds, widget);
        g_signal_connect(m->ds, "data-get", G_CALLBACK(on_app_drag_data_get), widget);

        return FALSE; // Permitir que continúe el evento
    }

    return FALSE;
}

/* Callback cuando comienza el drag */
static void on_app_drag_begin(GtkWidget *widget, GdkDragContext *context, gpointer user_data)
{
    MenuCacheItem *item = g_object_get_data(G_OBJECT(widget), "menu-item");
    if (!item) return;

    // Establecer icono para el drag
    GdkPixbuf *pb = get_app_icon(item, 48);
    if (pb) {
        gtk_drag_set_icon_pixbuf(context, pb, 0, 0);
        g_object_unref(pb);
    }
}
/* Callback para proveer los datos del drag */
static void on_app_drag_data_get(FmDndSrc *ds, GtkWidget *btn)
{
    static GQuark SYS_MENU_ITEM_ID = 0;
    if (SYS_MENU_ITEM_ID == 0)
        SYS_MENU_ITEM_ID = g_quark_from_static_string("SysMenuItem");

    FmFileInfo *fi = g_object_get_qdata(G_OBJECT(btn), SYS_MENU_ITEM_ID);
    if (!fi) return;

    fm_dnd_src_set_file(ds, fi);
}

/* Launch application */
static void launch_app_from_item(GtkWidget *button, gpointer user_data)
{
    MenuCacheItem *item = (MenuCacheItem *) user_data;
    ModernMenu *m = g_object_get_data(G_OBJECT(button), "modern-menu");
    if (!item) {
        g_warning(_("No MenuCacheItem associated with the button"));
        return;
    }

    const gchar *desktop_file = menu_cache_item_get_file_path(item);
    if (!desktop_file) {
        g_warning(_("Could not get .desktop file from the item"));
        return;
    }

    GDesktopAppInfo *dinfo = g_desktop_app_info_new_from_filename(desktop_file);
    if (dinfo) {
        GError *error = NULL;

        // Crear contexto de lanzamiento para GTK+2
        GdkScreen *screen = gtk_widget_get_screen(button);
        GdkAppLaunchContext *context = gdk_app_launch_context_new();
        gdk_app_launch_context_set_screen(context, screen);
        gdk_app_launch_context_set_timestamp(context, gtk_get_current_event_time());

        if (!g_app_info_launch(G_APP_INFO(dinfo), NULL, G_APP_LAUNCH_CONTEXT(context), &error)) {
            g_warning(_("Error launching '%s': %s"), desktop_file, error->message);
            g_clear_error(&error);
        }

        g_object_unref(context);
        g_object_unref(dinfo);

        if (m) {
            hide_menu(m);
        }
    } else {
        g_warning(_("Could not create GDesktopAppInfo from file '%s'"), desktop_file);
    }
}

/* Menu contextual */
static void show_context_menu(GtkWidget *app_button, ModernMenu *m, GdkEventButton *event)
{
    if (!app_button || !m) return;
    GtkWidget *menu = gtk_menu_new();

    /* Obtener el id desde el MenuCacheItem asociado al botón */
    MenuCacheItem *item = g_object_get_data(G_OBJECT(app_button), "menu-item");
    const char *app_id = NULL;
    gboolean is_fav = FALSE;
    gboolean is_hid = FALSE;

    if (item)
        app_id = menu_cache_item_get_id(item);

    if (app_id) {
        if (g_slist_find_custom(m->favorites, app_id, (GCompareFunc)g_strcmp0))
            is_fav = TRUE;
        if (g_slist_find_custom(m->hidden_apps, app_id, (GCompareFunc)g_strcmp0))
            is_hid = TRUE;
    }

    /* ===== Agregar/Quitar de Favoritos ===== */
    GtkWidget *fav_item = gtk_menu_item_new_with_label(
        is_fav ? _("Remove from Favorites") : _("Add to Favorites")
    );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), fav_item);
    gtk_widget_show(fav_item);
    g_signal_connect(fav_item, "activate", G_CALLBACK(toggle_favorite), app_button);

    /* ===== Ocultar/Mostrar aplicación ===== */
    GtkWidget *hide_item = gtk_menu_item_new_with_label(
        is_hid ? _("Show application") : _("Hide application")
    );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), hide_item);
    gtk_widget_show(hide_item);
    g_signal_connect(hide_item, "activate", G_CALLBACK(toggle_hidden), app_button);


    /* ===== Agregar al Escritorio ===== */
    const char *desktop_file = item ? menu_cache_item_get_file_path(item) : NULL;
    if (desktop_file) {
        const char *home = g_get_home_dir();
        gchar *desktop_dir = g_build_filename(home, _("Desktop"), NULL);
        if (!g_file_test(desktop_dir, G_FILE_TEST_IS_DIR)) {
            g_free(desktop_dir);
            desktop_dir = g_build_filename(home, "Desktop", NULL);
        }

        gchar *basename = g_path_get_basename(desktop_file);
        gchar *dest_file = g_build_filename(desktop_dir, basename, NULL);

        if (!g_file_test(dest_file, G_FILE_TEST_EXISTS)) {
            GtkWidget *desktop_item = gtk_menu_item_new_with_label(_("Add to Desktop"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), desktop_item);
            gtk_widget_show(desktop_item);
            g_signal_connect(desktop_item, "activate", G_CALLBACK(add_to_desktop), app_button);
        }

        g_free(basename);
        g_free(dest_file);
        g_free(desktop_dir);
    }

    /* Separador */
    GtkWidget *separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    gtk_widget_show(separator);

    /* ==== ELIMINAR PAQUETE ==== */
    GtkWidget *remove_pkg_item = gtk_menu_item_new_with_label(_("Remove package"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), remove_pkg_item);
    gtk_widget_show(remove_pkg_item);

    if (desktop_file) {
        // Guardar la ruta en el widget con destructor automático
        g_object_set_data_full(G_OBJECT(remove_pkg_item), "desktop-path",
                               g_strdup(desktop_file), g_free);

        g_signal_connect(remove_pkg_item, "activate",
                         G_CALLBACK(on_remove_package), NULL);
    }

    /* ===== Propiedades ===== */
    GtkWidget *prop_item = gtk_menu_item_new_with_label(_("Properties"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), prop_item);
    gtk_widget_show(prop_item);
    g_signal_connect(prop_item, "activate", G_CALLBACK(show_properties), app_button);

    gtk_widget_show_all(menu);
    m->suppress_hide = TRUE;
    g_signal_connect(menu, "selection-done", G_CALLBACK(on_context_menu_done), m);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
}
static void on_context_menu_done(GtkMenuShell *menu_shell, gpointer user_data)
{
    ModernMenu *m = (ModernMenu *)user_data;
    if (m)
        m->suppress_hide = FALSE;
}

static void add_to_desktop(GtkMenuItem *menuitem, gpointer user_data)
{
    GtkWidget *app_button = GTK_WIDGET(user_data);
    if (!app_button) return;

    MenuCacheItem *item = g_object_get_data(G_OBJECT(app_button), "menu-item");
    if (!item) return;

    const char *desktop_file = menu_cache_item_get_file_path(item);
    const char *app_name = menu_cache_item_get_name(item);
    if (!desktop_file || !app_name) return;

    /* Ruta de destino */
    const char *home = g_get_home_dir();
    gchar *desktop_dir = g_build_filename(home, _("Desktop"), NULL);
    if (!g_file_test(desktop_dir, G_FILE_TEST_IS_DIR)) {
        g_free(desktop_dir);
        desktop_dir = g_build_filename(home, "Desktop", NULL);
    }

    gchar *basename = g_path_get_basename(desktop_file);
    gchar *dest_file = g_build_filename(desktop_dir, basename, NULL);

    /* Si ya existe, no hacer nada */
    if (g_file_test(dest_file, G_FILE_TEST_EXISTS)) {
        g_message(_("The shortcut already exists on the desktop."));
        goto cleanup;
    }

    /* Copiar archivo */
    GError *error = NULL;
    if (!g_file_copy(
        g_file_new_for_path(desktop_file),
                     g_file_new_for_path(dest_file),
                     G_FILE_COPY_NONE,
                     NULL, NULL, NULL, &error))
    {
        g_warning(_("Error copying to desktop: %s"), error->message);
        g_error_free(error);
    } else {
        g_chmod(dest_file, 0755);
        /* Notificación opcional */
        gchar *cmd = g_strdup_printf("notify-send '%s' '%s'",
                                     _("Added to desktop"), app_name);
        g_spawn_command_line_async(cmd, NULL);
        g_free(cmd);
    }

    cleanup:
    g_free(basename);
    g_free(dest_file);
    g_free(desktop_dir);
}
static void show_properties(GtkMenuItem *menuitem, gpointer user_data)
{
    GtkWidget *app_button = GTK_WIDGET(user_data);
    if (!app_button) return;

    // Obtener el MenuCacheItem desde el botón (como ya lo haces)
    MenuCacheItem *item = g_object_get_data(G_OBJECT(app_button), "menu-item");
    if (!item) return;

    // Crear FmFileInfo a partir del MenuCacheItem (como hace LXPanel)
    char *mpath = menu_cache_dir_make_path(MENU_CACHE_DIR(item));
    FmPath *path = fm_path_new_relative(fm_path_get_apps_menu(), mpath + 13); // skip "/Applications"
    FmFileInfo *fi = fm_file_info_new_from_menu_cache_item(path, item);

    g_free(mpath);
    fm_path_unref(path);

    if (!fi) {
        g_warning(_("Could not create FmFileInfo for the menu item"));
        return;
    }

    // Crear lista de archivos (solo este archivo)
    FmFileInfoList *files = fm_file_info_list_new();
    fm_file_info_list_push_tail(files, fi);

    // Mostrar diálogo de propiedades (igual que LXPanel)
    fm_show_file_properties(NULL, files);

    // Liberar recursos
    fm_file_info_list_unref(files);
    // NOTA: fi se libera automáticamente al liberar la lista
}
static void on_remove_package(GtkWidget *widget, gpointer user_data)
{
    (void)user_data; // No usamos user_data ahora

    // Obtener la ruta desde el widget
    const char *desktop_file = g_object_get_data(G_OBJECT(widget), "desktop-path");
    if (!desktop_file) {
        g_warning(_("No desktop path found in widget data"));
        return;
    }

    gchar *exec = get_exec_from_desktop(desktop_file);
    if (!exec || !*exec) {
        show_error_dialog(_("Could not determine the executable from the .desktop file."));
        g_free(exec);
        return;
    }

    /* Resolver ruta completa del ejecutable */
    gchar *exec_path = NULL;
    if (g_path_is_absolute(exec)) {
        exec_path = g_strdup(exec);
    } else {
        gchar *found = g_find_program_in_path(exec);
        exec_path = found ? found : g_strdup(exec);
    }

    /* Detectar gestor de paquetes */
    const gchar *pkg_manager = NULL;
    if (g_find_program_in_path("pacman")) {
        pkg_manager = "pacman";
    } else if (g_find_program_in_path("dpkg")) {
        pkg_manager = "dpkg";
    }

    if (!pkg_manager) {
        show_error_dialog(_("No compatible package manager detected (dpkg or pacman)."));
        g_free(exec);
        g_free(exec_path);
        return;
    }

    /* Confirmación antes de eliminar */
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
        _("Do you want to remove the package associated with this application?"));
    gtk_window_set_title(GTK_WINDOW(dialog), _("Confirm removal"));
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (response != GTK_RESPONSE_YES) {
        g_free(exec);
        g_free(exec_path);
        return;
    }

    /* Determinar a qué paquete pertenece */
    gchar *cmd_query = NULL;
    if (g_strcmp0(pkg_manager, "pacman") == 0)
        cmd_query = g_strdup_printf("/bin/sh -c \"LANG=C pacman -Qo '%s' 2>&1\"", exec_path);
    else
        cmd_query = g_strdup_printf("/bin/sh -c \"LANG=C dpkg -S '%s' 2>&1\"", exec_path);

    gchar *output = NULL;
    gint status = -1;
    g_spawn_command_line_sync(cmd_query, &output, NULL, &status, NULL);
    g_free(cmd_query);

    /* Extraer nombre del paquete */
    gchar *pkg_name = NULL;
    if (output && status == 0) {
        if (g_strcmp0(pkg_manager, "dpkg") == 0) {
            gchar **parts = g_strsplit(output, ":", 2);
            if (parts[0]) pkg_name = g_strstrip(g_strdup(parts[0]));
            g_strfreev(parts);
        } else if (g_strcmp0(pkg_manager, "pacman") == 0) {
            gchar *owned_by = g_strstr_len(output, -1, "owned by");
            if (owned_by) {
                owned_by += 9;
                gchar **parts = g_strsplit(owned_by, " ", 2);
                if (parts[0]) pkg_name = g_strstrip(g_strdup(parts[0]));
                g_strfreev(parts);
            }
        }
    }
    g_free(output);

    if (!pkg_name || !*pkg_name) {
        show_error_dialog(_("Could not determine which package this application belongs to."));
        g_free(exec);
        g_free(exec_path);
        g_free(pkg_name);
        return;
    }

    /* Intentar eliminar con el gestor correspondiente */
    gchar *remove_cmd = NULL;
    if (g_strcmp0(pkg_manager, "dpkg") == 0)
        remove_cmd = g_strdup_printf("apt remove -y %s", pkg_name);
    else
        remove_cmd = g_strdup_printf("pacman -R --noconfirm %s", pkg_name);

    const gboolean has_pkexec = g_find_program_in_path("pkexec") != NULL;
    const gchar *askpass = g_getenv("SUDO_ASKPASS");

    const gchar *terminals[] = {"x-terminal-emulator", "lxterminal", "xterm", "mate-terminal", "konsole", "terminator", NULL};
    const gchar *terminal_cmd = NULL;
    for (int i = 0; terminals[i] != NULL; i++) {
        if (g_find_program_in_path(terminals[i]) != NULL) {
            terminal_cmd = terminals[i];
            break;
        }
    }

    gchar *cmd_remove = NULL;
    if (has_pkexec) {
        cmd_remove = g_strdup_printf("/bin/sh -c \"pkexec %s 2>&1\"", remove_cmd);
    } else if (askpass && *askpass) {
        cmd_remove = g_strdup_printf("/bin/sh -c \"sudo -A %s 2>&1\"", remove_cmd);
    } else if (terminal_cmd) {
        cmd_remove = g_strdup_printf("%s -e 'sudo %s'", terminal_cmd, remove_cmd);
    } else {
        show_error_dialog(_("Could not find a method to request authentication (pkexec, sudo -A)."));
        g_free(remove_cmd);
        g_free(exec);
        g_free(exec_path);
        g_free(pkg_name);
        return;
    }

    gchar *output_remove = NULL;
    gint status_remove = -1;
    g_spawn_command_line_sync(cmd_remove, &output_remove, NULL, &status_remove, NULL);

    if (status_remove != 0) {
        show_error_dialog(_("Could not remove the package. Check permissions or authentication method."));
    }

    g_free(output_remove);
    g_free(cmd_remove);
    g_free(remove_cmd);
    g_free(exec);
    g_free(exec_path);
    g_free(pkg_name);
}
static void on_category_selected(GtkTreeSelection *sel, gpointer user_data)
{
    ModernMenu *m = user_data;
    if (!m || m->switching_category) return;

    GtkTreeIter iter;
    GtkTreeModel *model;

    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        MenuCacheDir *dir = NULL;
        gtk_tree_model_get(model, &iter, COL_DIR_PTR, &dir, -1);
        populate_apps_for_dir(m, dir);
    }

    // Si no estamos en modo "Favoritos", desactivamos el toggle
    if (m->btn_fav && !m->switching_category)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m->btn_fav), FALSE);
}

/* Cierre automático al perder el foco */
static gboolean on_window_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    (void)widget;
    (void)event;
    ModernMenu *m = user_data;

    // Pequeño delay para permitir que los clicks en el menú se procesen
    g_timeout_add(100, (GSourceFunc)gtk_false, NULL);

    if (m && m->window_shown) {
        hide_menu(m);
    }

    return FALSE;
}

static gboolean on_window_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    (void)widget;
    ModernMenu *m = user_data;

    if (event->keyval == GDK_Escape) {
        hide_menu(m);
        return TRUE;
    }

    return FALSE;
}

static void on_logout_clicked(GtkButton *b, gpointer user_data)
{
    (void)b;
    ModernMenu *m = (ModernMenu *)user_data;

    logout();  // Ejecuta la función para mostrar el mismo logout que el menú de LXDE

    if (m) hide_menu(m);
}


/* ===== 5.6 FUNCIONES AUXILIARES ===== */
static void show_error_dialog(const gchar *message)
{
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        "%s",
        message
    );
    gtk_window_set_title(GTK_WINDOW(dialog), _("Error"));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
static void unhide_app(GtkButton *button, gpointer user_data)
{
    ModernMenu *m = (ModernMenu *)user_data;
    const char *app_id = (const char *)g_object_get_data(G_OBJECT(button), "app-id");

    if (!app_id || !m) {
        g_print(_("unhide_app: app_id or m is NULL, aborting\n"));
        return;
    }

    // Buscar en la lista
    for (GSList *l = m->hidden_apps; l; l = l->next) {
        if (g_strcmp0((const char *)l->data, app_id) == 0) {
            g_free(l->data);

            // Remover de la lista
            m->hidden_apps = g_slist_delete_link(m->hidden_apps, l);
            save_hidden_apps(m);

            // Eliminar visualmente la fila del diálogo
            GtkWidget *hbox = gtk_widget_get_parent(GTK_WIDGET(button));
            if (hbox) {
                gtk_widget_destroy(hbox);
            }
            g_print(_("unhide_app: Completed successfully\n"));
            break;
        }
    }
}
/* Callback para abrir el diálogo de apps ocultas desde la configuración */
static void on_manage_hidden_button_clicked(GtkButton *button, gpointer user_data)
{
    ModernMenu *m = (ModernMenu *)user_data;

    if (!m->hidden_apps) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   _("No hidden applications"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Hidden Applications"),
                                                    NULL,
                                                    GTK_DIALOG_MODAL,
                                                    GTK_STOCK_CLOSE,
                                                    GTK_RESPONSE_CLOSE,
                                                    NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 300);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 5);

    GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll), vbox);

    // Buscar info de cada app oculta
    for (GSList *l = m->hidden_apps; l; l = l->next) {
        const char *hidden_id = (const char *)l->data;

        // Buscar el MenuCacheItem correspondiente
        MenuCacheItem *found_item = NULL;
        for (GSList *al = m->all_apps; al; al = al->next) {
            MenuCacheItem *item = MENU_CACHE_ITEM(al->data);
            const char *item_id = menu_cache_item_get_id(item);
            if (item_id && g_strcmp0(item_id, hidden_id) == 0) {
                found_item = item;
                break;
            }
        }

        GtkWidget *hbox = gtk_hbox_new(FALSE, 10);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

        // Icono
        GtkWidget *img = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_BUTTON);
        if (found_item) {
            GdkPixbuf *pb = get_app_icon(found_item, 24);
            if (pb) {
                gtk_image_set_from_pixbuf(GTK_IMAGE(img), pb);
                g_object_unref(pb);
            }
        }
        gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 5);

        // Nombre
        const char *name = found_item ? menu_cache_item_get_name(found_item) : hidden_id;
        GtkWidget *label = gtk_label_new(name);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 5);

        // Botón para mostrar
        GtkWidget *btn = gtk_button_new_with_label(_("Show"));
        g_object_set_data_full(G_OBJECT(btn), "app-id", g_strdup(hidden_id), g_free);
        g_signal_connect(btn, "clicked", G_CALLBACK(unhide_app), m);
        gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 5);
    }

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));  // <-- ESTA LÍNEA FALTABA
    gtk_widget_destroy(dialog);

    // Refrescar la vista si el menú está abierto
    if (m->window_shown) {
        if (m->current_dir) {
            populate_apps_for_dir(m, m->current_dir);
        } else {
            show_favorites_category(NULL, m);
        }
    }
}

/* ===== 5.6 FUNCIONES DEL PLUGIN (CORE) ===== */

GtkWidget *modernmenu_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* ==== CREACIÓN Y CONFIGURACIÓN INICIAL ==== */
    #ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain("modernmenu", "/usr/share/locale");
    bind_textdomain_codeset("modernmenu", "UTF-8");
    textdomain("modernmenu");
    #endif

    ModernMenu *m = g_new0(ModernMenu, 1);
    m->panel = panel;
    m->window_shown = FALSE;
    m->current_dir = NULL;
    m->settings = settings;
    m->ds = fm_dnd_src_new(NULL);

    // Color del hover
    GdkColor tint_color = {0, 0, 36 * 0xffff / 0xff, 96 * 0xffff / 0xff};

    /* ==== LECTURA DEL ICONO CONFIGURADO ==== */
    const char *icon_str = NULL;
    if (settings && config_setting_lookup_string(settings, "icon", &icon_str) && icon_str && *icon_str) {
        m->icon_path = g_strdup(icon_str);
    } else {
        m->icon_path = g_strdup("start-here");
    }

    /* ==== CREAR BOTÓN DEL MENÚ (SIMPLIFICADO) ==== */
    // lxpanel_button_new_for_icon devuelve un GtkEventBox
    m->plugin_button = lxpanel_button_new_for_icon(m->panel, m->icon_path, &tint_color, NULL);

    // Conectar señal de button-press-event (porque es un EventBox)
    g_signal_connect(m->plugin_button, "button-press-event",
                     G_CALLBACK(on_plugin_button_press), m);

    gtk_widget_set_tooltip_text(m->plugin_button, _("Applications Menu"));

    /* ==== CREAR LA VENTANA POPUP DEL MENÚ ==== */
    m->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(m->window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(m->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(m->window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(m->window), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    gtk_window_set_resizable(GTK_WINDOW(m->window), FALSE);
    gtk_widget_set_size_request(m->window, 700, 500);

    /* ==== CONTENEDOR PRINCIPAL DE LA VENTANA ==== */
    GtkWidget *main_box = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 12);
    gtk_container_add(GTK_CONTAINER(m->window), main_box);

    /* ==== CONTENEDOR HORIZONTAL PRINCIPAL ==== */
    GtkWidget *content = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);

    /* ==== CARGA DE FAVORITOS ==== */
    load_favorites(m);

    /* ==== CARGA DE OCULTOS ==== */
    load_hidden_apps(m);

    /* ==== PANEL DE CATEGORÍAS ==== */
    GtkWidget *cat_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cat_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(cat_scroll, 200, 300);

    GtkWidget *cat_box = gtk_vbox_new(FALSE, 4);
    gtk_widget_set_size_request(cat_box, -1, 300);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(cat_scroll), cat_box);
    gtk_widget_show(cat_box);

    /* ==== BOTÓN DE FAVORITOS ==== */
    GtkWidget *btn_fav = gtk_toggle_button_new_with_label(_("★ Favorites"));
    gtk_button_set_relief(GTK_BUTTON(btn_fav), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(btn_fav, _("Favorite applications"));
    g_signal_connect(btn_fav, "toggled", G_CALLBACK(show_favorites_category), m);
    m->btn_fav = btn_fav;
    gtk_box_pack_start(GTK_BOX(cat_box), btn_fav, FALSE, FALSE, 4);
    gtk_widget_show(btn_fav);

    /* ==== MODELO Y VISTA DE CATEGORÍAS ==== */
    m->cat_store = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_POINTER);
    m->categories = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m->cat_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(m->categories), FALSE);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        _("Category"), renderer, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m->categories), column);

    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(m->categories));
    gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
    g_signal_connect(sel, "changed", G_CALLBACK(on_category_selected), m);

    gtk_box_pack_start(GTK_BOX(cat_box), m->categories, TRUE, TRUE, 0);
    gtk_widget_show(m->categories);

    gtk_box_pack_start(GTK_BOX(content), cat_scroll, FALSE, FALSE, 0);

    /* ==== PANEL DE APLICACIONES ==== */
    m->apps_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m->apps_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    m->apps_box = gtk_vbox_new(FALSE, 4);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(m->apps_scroll), m->apps_box);
    gtk_box_pack_start(GTK_BOX(content), m->apps_scroll, TRUE, TRUE, 0);

    /* ==== BARRA INFERIOR: SALIR + BUSCAR ==== */
    GtkWidget *bottom_bar = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(main_box), bottom_bar, FALSE, FALSE, 0);

    GtkWidget *btn_logout = gtk_button_new();
    GtkWidget *logout_img = gtk_image_new_from_icon_name("system-log-out", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(btn_logout), logout_img);
    gtk_button_set_label(GTK_BUTTON(btn_logout), _("Exit"));
    gtk_button_set_image_position(GTK_BUTTON(btn_logout), GTK_POS_LEFT);
    gtk_widget_set_tooltip_text(btn_logout, _("Shutdown | Restart | Suspend | Logout"));
    g_signal_connect(btn_logout, "clicked", G_CALLBACK(on_logout_clicked), m);
    gtk_box_pack_start(GTK_BOX(bottom_bar), btn_logout, FALSE, FALSE, 0);

    GtkWidget *search_label = gtk_label_new(_("Search:"));
    gtk_box_pack_start(GTK_BOX(bottom_bar), search_label, FALSE, FALSE, 4);

    m->search = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(bottom_bar), m->search, TRUE, TRUE, 0);

    /* ==== CARGA DE MENÚS Y DATOS ==== */
    m->menu_cache = menu_cache_lookup_sync("applications.menu");
    if (m->menu_cache) {
        m->reload_notify = menu_cache_add_reload_notify(m->menu_cache,
                                                        on_menu_cache_reload_real, m);
        load_categories(m);
    }

    build_all_apps_list(m);
    show_favorites_category(NULL, m);

    /* ==== CONEXIONES DE SEÑALES ==== */
    g_signal_connect(m->search, "changed", G_CALLBACK(on_search_changed), m);
    g_signal_connect(m->window, "key-press-event", G_CALLBACK(on_window_key_press), m);
    g_signal_connect(m->window, "focus-out-event", G_CALLBACK(on_window_focus_out), m);

    /* ==== FINALIZACIÓN ==== */
    lxpanel_plugin_set_data(m->plugin_button, m, modern_menu_destructor);
    return m->plugin_button;
}
static void modern_menu_destructor(gpointer user_data)
{
    ModernMenu *m = (ModernMenu *)user_data;
    if (!m) return;

    /* En el destructor del plugin, agrega: */
    if (m->ds) {
        g_object_unref(m->ds);
    }

    if (m->reload_notify && m->menu_cache)
        menu_cache_remove_reload_notify(m->menu_cache, m->reload_notify);

    if (m->menu_cache)
        menu_cache_unref(m->menu_cache);

    if (m->cat_store)
        g_object_unref(m->cat_store);

    if (m->all_apps) {
        g_slist_free_full(m->all_apps, (GDestroyNotify)menu_cache_item_unref);
    }

    if (m->hidden_apps) {
        g_slist_free_full(m->hidden_apps, g_free);
        m->hidden_apps = NULL;
    }

    if (m->favorites) {
        g_slist_free_full(m->favorites, g_free);
    }

    g_free(m->favorites_path);
    g_free(m->icon_path);

    if (m->window)
        gtk_widget_destroy(m->window);

    g_free(m);
}
static GtkWidget *modernmenu_config(LXPanel *panel, GtkWidget *p)
{
    ModernMenu *m = lxpanel_plugin_get_data(p);

    // Crear el diálogo de configuración estándar
    GtkWidget *dlg = lxpanel_generic_config_dlg(_("Modern Menu"), panel,
                                                modernmenu_apply_config, p,
                                                _("Icon"), &m->icon_path, CONF_TYPE_FILE_ENTRY,
                                                NULL);

    // Agregar un botón personalizado para gestionar apps ocultas
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));

    // Separador
    GtkWidget *separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(content), separator, FALSE, FALSE, 10);

    // Botón para gestionar aplicaciones ocultas
    GtkWidget *hidden_box = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(content), hidden_box, FALSE, FALSE, 5);

    GtkWidget *hidden_label = gtk_label_new(_("Hidden applications:"));
    gtk_box_pack_start(GTK_BOX(hidden_box), hidden_label, FALSE, FALSE, 5);

    GtkWidget *hidden_button = gtk_button_new_with_label(_("Manage"));
    gtk_box_pack_start(GTK_BOX(hidden_box), hidden_button, FALSE, FALSE, 5);
    g_signal_connect(hidden_button, "clicked", G_CALLBACK(on_manage_hidden_button_clicked), m);

    gtk_widget_show_all(content);
    return dlg;
}

static gboolean modernmenu_apply_config(gpointer user_data)
{
    GtkWidget *p = GTK_WIDGET(user_data);
    ModernMenu *m = lxpanel_plugin_get_data(p);

    if (!m || !m->settings)
        return FALSE;

    // Guardar ruta del icono en la configuración
    config_group_set_string(m->settings, "icon", m->icon_path);

    // Actualizar el ícono (el EventBox se actualiza automáticamente)
    if (m->icon_path && m->plugin_button) {
        lxpanel_button_set_icon(m->plugin_button, m->icon_path, -1);
    }

    return FALSE;
}
static void on_menu_cache_reload_real(MenuCache *cache, gpointer user_data)
{
    (void)cache;
    ModernMenu *m = user_data;
    if (!m) return;
    load_categories(m);
    build_all_apps_list(m);
}

/* ===== 6 DEFINICIÓN DEL PLUGIN ===== */
FM_DEFINE_MODULE(lxpanel_gtk, modernmenu)
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = N_("Modern menu"),
    .description = N_("Modern applications menu with search and favorites included"),
    .new_instance = modernmenu_constructor,
    .config = modernmenu_config
};
