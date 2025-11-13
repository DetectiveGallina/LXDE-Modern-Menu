/* modern_menu_fixed.c
 * Modern Menu Plugin for LXPanel (corregido)
 * Based on lxpanel-plugin-example structure
 */

#include <lxpanel/plugin.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gdk/gdkkeysyms.h>
#include <menu-cache/menu-cache.h>
#include <libintl.h>
#include <libfm/fm-gtk.h>
#include <libfm/fm-utils.h>
#include <string.h>

#ifndef _
#define _(String) (String)
#endif

/* Plugin structure */
typedef struct {
    GtkWidget *window;
    GtkWidget *search;
    GtkWidget *categories;
    GtkWidget *apps_box;
    GtkWidget *apps_scroll;
    MenuCache *menu_cache;
    gpointer reload_notify;
    gboolean window_shown;
    MenuCacheDir *current_dir;
    GtkListStore *cat_store;
    LXPanel *panel;
    GtkWidget *plugin_button;
    GSList *all_apps;
    GSList *favorites;
    gchar *favorites_path;
    gchar *icon_path;
    config_setting_t *settings;
    gboolean suppress_hide;
    GtkWidget *btn_fav;
    gboolean switching_category;
} ModernMenu;

enum {
    COL_NAME = 0,
    COL_DIR_PTR,
    N_COLS
};

/* Forward declarations */
static void hide_menu(ModernMenu *m);
static void populate_apps_for_dir(ModernMenu *m, MenuCacheDir *dir);
static void show_favorites_category(GtkWidget *widget, gpointer user_data);
static GdkPixbuf *get_app_icon(MenuCacheItem *item, int size);
/* Launch application - CORREGIDO */
static void launch_app_from_item(GtkWidget *button, gpointer user_data)
{
    MenuCacheItem *item = (MenuCacheItem *) user_data;
    ModernMenu *m = g_object_get_data(G_OBJECT(button), "modern-menu");

    if (!item) {
        g_warning("No hay MenuCacheItem asociado al botón");
        return;
    }

    const gchar *desktop_id = menu_cache_item_get_id(item);
    if (!desktop_id) {
        g_warning("El item no tiene ID");
        return;
    }

    g_print("Intentando lanzar: %s\n", desktop_id);

    GDesktopAppInfo *dinfo = g_desktop_app_info_new(desktop_id);
    if (dinfo) {
        GError *error = NULL;

        if (!g_app_info_launch(G_APP_INFO(dinfo), NULL, NULL, &error)) {
            g_warning("Error al lanzar '%s': %s", desktop_id, error->message);
            g_clear_error(&error);
        } else {
            g_print("Aplicación lanzada exitosamente\n");
        }

        g_object_unref(dinfo);

        if (m) {
            hide_menu(m);
        }
    } else {
        g_warning("No se pudo crear GDesktopAppInfo desde '%s'", desktop_id);
    }
}

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

    g_print("Favoritos cargados: %d\n", g_slist_length(m->favorites));
}

static void save_favorites(ModernMenu *m)
{
    if (!m || !m->favorites_path) return;

    GString *data = g_string_new("");
    for (GSList *l = m->favorites; l; l = l->next)
        g_string_append_printf(data, "%s\n", (char*)l->data);
    g_file_set_contents(m->favorites_path, data->str, -1, NULL);
    g_string_free(data, TRUE);

    g_print("Favoritos guardados: %d\n", g_slist_length(m->favorites));
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
            new_item = gtk_menu_item_new_with_label("Quitar de Favoritos");
        else
            new_item = gtk_menu_item_new_with_label("Agregar a Favoritos");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_item);
        gtk_widget_show(new_item);
        g_signal_connect(new_item, "activate", G_CALLBACK(toggle_favorite), btn);
    }
}
/* ===== Callback: Agregar al Escritorio ===== */
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
    gchar *desktop_dir = g_build_filename(home, "Escritorio", NULL);
    if (!g_file_test(desktop_dir, G_FILE_TEST_IS_DIR)) {
        g_free(desktop_dir);
        desktop_dir = g_build_filename(home, "Desktop", NULL);
    }

    gchar *basename = g_path_get_basename(desktop_file);
    gchar *dest_file = g_build_filename(desktop_dir, basename, NULL);

    /* Si ya existe, no hacer nada */
    if (g_file_test(dest_file, G_FILE_TEST_EXISTS)) {
        g_message("El acceso directo ya existe en el escritorio.");
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
        g_warning("Error copiando a escritorio: %s", error->message);
        g_error_free(error);
    } else {
        g_chmod(dest_file, 0755);
        /* Notificación opcional */
        gchar *cmd = g_strdup_printf("notify-send 'Agregado al escritorio' '%s'", app_name);
        g_spawn_command_line_async(cmd, NULL);
        g_free(cmd);
    }

    cleanup:
    g_free(basename);
    g_free(dest_file);
    g_free(desktop_dir);
}
/* ==== Mostrar diálogo de error simple ==== */
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
    gtk_window_set_title(GTK_WINDOW(dialog), "Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/* ==== Obtener el comando Exec del archivo .desktop ==== */
static gchar *get_exec_from_desktop(const char *desktop_path)
{
    if (!desktop_path) return NULL;

    GKeyFile *keyfile = g_key_file_new();
    GError *error = NULL;

    if (!g_key_file_load_from_file(keyfile, desktop_path, G_KEY_FILE_NONE, &error)) {
        show_error_dialog("No se pudo cargar el archivo .desktop.");
        if (error) g_error_free(error);
        g_key_file_free(keyfile);
        return NULL;
    }

    gchar *exec = g_key_file_get_string(keyfile, "Desktop Entry", "Exec", NULL);
    g_key_file_free(keyfile);

    if (!exec || !*exec) {
        show_error_dialog("El archivo .desktop no tiene un campo 'Exec' válido.");
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

/* ==== Eliminar paquete del sistema ==== */
static void on_remove_package(GtkWidget *widget, gpointer user_data)
{
    const char *desktop_path = (const char *)user_data;
    if (!desktop_path) return;

    gchar *exec = get_exec_from_desktop(desktop_path);
    if (!exec || !*exec) {
        show_error_dialog("No se pudo determinar el ejecutable del archivo .desktop.");
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
        show_error_dialog("No se detectó ningún gestor de paquetes compatible (dpkg o pacman).");
        g_free(exec);
        g_free(exec_path);
        return;
    }

    /* Confirmación antes de eliminar */
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL,
        GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
        "¿Deseas eliminar el paquete asociado a esta aplicación?");
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirmar eliminación");
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
        show_error_dialog("No se pudo determinar el paquete al que pertenece esta aplicación.");
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
        show_error_dialog("No se encontró un método para pedir autenticación (pkexec, sudo -A o terminal).");
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
        show_error_dialog("No se pudo eliminar el paquete. Verifica los permisos o el método de autenticación.");
    }

    g_free(output_remove);
    g_free(cmd_remove);
    g_free(remove_cmd);
    g_free(exec);
    g_free(exec_path);
    g_free(pkg_name);
}

/* ===== Callback: Propiedades ===== */
static void show_properties(GtkMenuItem *menuitem, gpointer user_data)
{
    GtkWidget *app_button = GTK_WIDGET(user_data);
    if (!app_button) return;

    MenuCacheItem *item = g_object_get_data(G_OBJECT(app_button), "menu-item");
    if (!item) return;

    const char *desktop_file = menu_cache_item_get_file_path(item);
    if (!desktop_file) return;

    // Detectar editor de propiedades
    const char *editor = g_find_program_in_path("lxshortcut");
    if (!editor) {
        editor = NULL;
    }

    if (editor) {
        gchar *cmd = g_strdup_printf("%s -i \"%s\"", editor, desktop_file);
        g_spawn_command_line_async(cmd, NULL);
        g_free(cmd);
    } else {
        show_error_dialog("Editar propiedades no está disponible. Tal vez deba intantar instalar lxshortcut o libfm-tools");
    }
}

/* ==== MENU CONTEXTUAL ==== */

/* Cuando el menú contextual termina */
static void on_context_menu_done(GtkMenuShell *menu_shell, gpointer user_data)
{
    /* Ya no hace falta hacer nada acá, la ventana nunca se oculta */
    ModernMenu *m = (ModernMenu *)user_data;
    if (m)
        m->suppress_hide = FALSE;
}

/* Mostrar menú contextual sin ocultar la ventana principal */
static void show_context_menu(GtkWidget *app_button, ModernMenu *m, GdkEventButton *event)
{
    if (!app_button || !m) return;

    GtkWidget *menu = gtk_menu_new();
    /* Obtener el id desde el MenuCacheItem asociado al botón (más seguro) */

    MenuCacheItem *item = g_object_get_data(G_OBJECT(app_button), "menu-item");
    const char *app_id = NULL;
    gboolean is_fav = FALSE;
    if (item)
        app_id = menu_cache_item_get_id(item);

    if (app_id && g_slist_find_custom(m->favorites, app_id, (GCompareFunc)g_strcmp0))
        is_fav = TRUE;

    /* ===== Agregar/Quitar de Favoritos ===== */
    GtkWidget *fav_item = gtk_menu_item_new_with_label(
        is_fav ? "Quitar de Favoritos" : "Agregar a Favoritos"
    );
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), fav_item);
    gtk_widget_show(fav_item);
    g_signal_connect(fav_item, "activate", G_CALLBACK(toggle_favorite), app_button);

    /* ===== Agregar al Escritorio ===== */
    const char *desktop_file = item ? menu_cache_item_get_file_path(item) : NULL;
    if (desktop_file) {
        const char *home = g_get_home_dir();
        gchar *desktop_dir = g_build_filename(home, "Escritorio", NULL);
        if (!g_file_test(desktop_dir, G_FILE_TEST_IS_DIR)) {
            g_free(desktop_dir);
            desktop_dir = g_build_filename(home, "Desktop", NULL);
        }

        gchar *basename = g_path_get_basename(desktop_file);
        gchar *dest_file = g_build_filename(desktop_dir, basename, NULL);

        if (!g_file_test(dest_file, G_FILE_TEST_EXISTS)) {
            GtkWidget *desktop_item = gtk_menu_item_new_with_label("Agregar al Escritorio");
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), desktop_item);
            gtk_widget_show(desktop_item);
            g_signal_connect(desktop_item, "activate", G_CALLBACK(add_to_desktop), app_button);
        }

        g_free(basename);
        g_free(dest_file);
        g_free(desktop_dir);
    }
    /* ==== ELIMINAR PAQUETE ==== */
    GtkWidget *remove_pkg_item = gtk_menu_item_new_with_label("Eliminar paquete");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), remove_pkg_item);
    const char *desktop_path = NULL;

    /* Si estás usando MenuCacheItem (de libfm) */
    if (item)
        desktop_path = menu_cache_item_get_file_path(item);

    if (desktop_path)
        g_signal_connect(remove_pkg_item, "activate", G_CALLBACK(on_remove_package), g_strdup(desktop_path));

    /* ===== Propiedades ===== */
    GtkWidget *prop_item = gtk_menu_item_new_with_label("Propiedades");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), prop_item);
    gtk_widget_show(prop_item);
    g_signal_connect(prop_item, "activate", G_CALLBACK(show_properties), app_button);

    gtk_widget_show_all(menu);

    m->suppress_hide = TRUE;  /* prevenimos cierres por focus-out */
    g_signal_connect(menu, "selection-done", G_CALLBACK(on_context_menu_done), m);

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
}


/* ==== MANEJADOR DE CLIC DERECHO ==== */
static gboolean on_app_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    ModernMenu *m = (ModernMenu *)user_data;
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) { // clic derecho
        show_context_menu(widget, m, event);
        return TRUE;
    }
    return FALSE;
}

/* Sección de favoritos */
static void show_favorites_category(GtkWidget *widget, gpointer user_data)
{
    ModernMenu *m = user_data;
    if (!m) return;

    /* Evita conflictos con la selección de categorías */
    m->switching_category = TRUE;

    /* Verifica si el botón fue activado (presionado) */
    if (GTK_IS_TOGGLE_BUTTON(widget)) {
        gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
        if (!active) {
            m->switching_category = FALSE;
            return; // Si se desactiva, no hacer nada
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

    /* Limpiar la caja de apps */
    GList *children = gtk_container_get_children(GTK_CONTAINER(m->apps_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    if (!m->favorites) {
        GtkWidget *lbl = gtk_label_new("No hay aplicaciones favoritas");
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
        m->switching_category = FALSE;
        return;
    }

    int count = 0;
    GtkWidget *current_hbox = NULL;

    for (GSList *l = m->all_apps; l; l = l->next) {
        MenuCacheItem *item = MENU_CACHE_ITEM(l->data);
        const char *id = menu_cache_item_get_id(item);

        if (!is_favorite(m, id)) continue;

        if (count % 3 == 0 || current_hbox == NULL) {
            current_hbox = gtk_hbox_new(TRUE, 8);
            gtk_box_pack_start(GTK_BOX(m->apps_box), current_hbox, FALSE, FALSE, 4);
            gtk_widget_show(current_hbox);
        }

        GtkWidget *btn = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        g_object_set_data(G_OBJECT(btn), "menu-item", item);
        g_object_set_data(G_OBJECT(btn), "modern-menu", m);

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

        GtkWidget *lbl = gtk_label_new(menu_cache_item_get_name(item));
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 16);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

        // Tooltip con nombre completo
        const char *name = menu_cache_item_get_name(item);
        if (name)
            gtk_widget_set_tooltip_text(btn, name);

        g_signal_connect(btn, "clicked", G_CALLBACK(launch_app_from_item), item);
        g_signal_connect(btn, "button-press-event", G_CALLBACK(on_app_button_press), m);

        gtk_widget_set_size_request(btn, 110, 100);
        gtk_widget_show_all(btn);
        gtk_box_pack_start(GTK_BOX(current_hbox), btn, FALSE, FALSE, 0);
        count++;
    }

    /* Fin del modo de cambio */
    m->switching_category = FALSE;
}

static GdkPixbuf *get_app_icon(MenuCacheItem *item, int size)
{
    const char *icon_name = menu_cache_item_get_icon(item);
    if (!icon_name || !*icon_name)
        icon_name = "application-x-executable"; // fallback seguro

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

static void populate_apps_for_dir(ModernMenu *m, MenuCacheDir *dir)
{
    if (!m || !m->apps_box) return;

    m->current_dir = dir;

    GList *children = gtk_container_get_children(GTK_CONTAINER(m->apps_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    if (!dir) {
        GtkWidget *lbl = gtk_label_new("No applications");
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
        return;
    }

    GSList *list = menu_cache_dir_list_children(dir);
    if (!list) {
        GtkWidget *lbl = gtk_label_new("No applications in this category");
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
        return;
    }

    int count = 0;
    GtkWidget *current_hbox = NULL;

    for (GSList *l = list; l; l = l->next) {
        MenuCacheItem *item = MENU_CACHE_ITEM(l->data);
        if (menu_cache_item_get_type(item) != MENU_CACHE_TYPE_APP) continue;

        const char *id = menu_cache_item_get_id(item);
        if (!id) continue;

        if (count % 3 == 0 || current_hbox == NULL) {
            current_hbox = gtk_hbox_new(TRUE, 8);
            gtk_box_pack_start(GTK_BOX(m->apps_box), current_hbox, FALSE, FALSE, 4);
            gtk_widget_show(current_hbox);
        }

        GtkWidget *btn = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);

        g_object_set_data(G_OBJECT(btn), "menu-item", item);
        g_object_set_data(G_OBJECT(btn), "modern-menu", m);

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

        // Tooltip con nombre completo
        if (name)
            gtk_widget_set_tooltip_text(btn, name);

        g_signal_connect(btn, "clicked", G_CALLBACK(launch_app_from_item), item);
        g_signal_connect(btn, "button-press-event", G_CALLBACK(on_app_button_press), m);

        gtk_widget_set_size_request(btn, 110, 100);
        gtk_widget_show_all(btn);

        if (current_hbox)
            gtk_box_pack_start(GTK_BOX(current_hbox), btn, FALSE, FALSE, 0);
        else
            gtk_box_pack_start(GTK_BOX(m->apps_box), btn, FALSE, FALSE, 0);

        count++;
    }

    g_slist_foreach(list, (GFunc)menu_cache_item_unref, NULL);
    g_slist_free(list);

    if (count == 0) {
        GtkWidget *lbl = gtk_label_new("No applications in this category");
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
    }
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

    g_print("Total apps en all_apps: %d\n", g_slist_length(m->all_apps));
}

static void on_search_changed(GtkEditable *entry, gpointer user_data)
{
    ModernMenu *m = user_data;
    if (!m || !m->apps_box) return;

    const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
    gboolean empty = (text == NULL || *text == '\0');

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

        if (!name || !id || !strcasestr(name, text)) continue;

        if (count % 3 == 0 || current_hbox == NULL) {
            current_hbox = gtk_hbox_new(TRUE, 8);
            gtk_box_pack_start(GTK_BOX(m->apps_box), current_hbox, FALSE, FALSE, 4);
            gtk_widget_show(current_hbox);
        }

        GtkWidget *btn = gtk_button_new();
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);

        g_object_set_data(G_OBJECT(btn), "menu-item", item);
        g_object_set_data(G_OBJECT(btn), "modern-menu", m);

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

        GtkWidget *lbl = gtk_label_new(name ? name : "");
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 16);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

        g_signal_connect(btn, "clicked", G_CALLBACK(launch_app_from_item), item);
        g_signal_connect(btn, "button-press-event", G_CALLBACK(on_app_button_press), m);

        gtk_widget_set_size_request(btn, 110, 100);
        gtk_widget_show_all(btn);

        gtk_box_pack_start(GTK_BOX(current_hbox), btn, FALSE, FALSE, 0);
        count++;
    }

    if (count == 0) {
        GtkWidget *lbl = gtk_label_new("No matching applications");
        gtk_misc_set_alignment(GTK_MISC(lbl), 0.5, 0.5);
        gtk_box_pack_start(GTK_BOX(m->apps_box), lbl, FALSE, FALSE, 4);
        gtk_widget_show(lbl);
    }
}

static void run_command_async(const char *cmd, ModernMenu *m)
{
    if (!cmd) return;
    g_spawn_command_line_async(cmd, NULL);
    if (m) hide_menu(m);
}

static void on_logout_clicked(GtkButton *b, gpointer user_data)
{
    (void)b;
    run_command_async("lxsession-logout || i3-msg exit", user_data);
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

    if (m->suppress_hide)
        return; // evita cierre si hay menú contextual abierto

    if (m->window_shown) {
        gtk_widget_hide(m->window);
        m->window_shown = FALSE;
        gtk_entry_set_text(GTK_ENTRY(m->search), "");
    }
}

/* ===== CIERRE AUTOMÁTICO AL PERDER FOCO ===== */
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

static void show_or_hide_menu(GtkWidget *button, ModernMenu *m)
{
    (void)button;
    if (!m) return;

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
}

static void on_menu_cache_reload_real(MenuCache *cache, gpointer user_data)
{
    (void)cache;
    ModernMenu *m = user_data;
    if (!m) return;
    load_categories(m);
    build_all_apps_list(m);
}

static void modern_menu_destructor(gpointer user_data)
{
    ModernMenu *m = (ModernMenu *)user_data;
    if (!m) return;

    if (m->reload_notify && m->menu_cache)
        menu_cache_remove_reload_notify(m->menu_cache, m->reload_notify);

    if (m->menu_cache)
        menu_cache_unref(m->menu_cache);

    if (m->cat_store)
        g_object_unref(m->cat_store);

    if (m->all_apps) {
        g_slist_free_full(m->all_apps, (GDestroyNotify)menu_cache_item_unref);
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

/* Actualizar el icono */
static void update_button_icon(ModernMenu *m)
{
    if (!m || !m->plugin_button) return;

    GtkWidget *image = NULL;
    GdkPixbuf *pix = NULL;

    int icon_size = panel_get_icon_size(m->panel); // tamaño estándar del panel

    // Intenta cargar el ícono personalizado o el por defecto
    const char *icon_name = (m->icon_path && *m->icon_path) ? m->icon_path : "start-here";

    // Intenta cargar el ícono como nombre de tema (más coherente con el resto)
    pix = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                   icon_name,
                                   icon_size,
                                   GTK_ICON_LOOKUP_USE_BUILTIN,
                                   NULL);

    // Si no existe en el tema, intenta cargarlo desde archivo
    if (!pix && g_file_test(icon_name, G_FILE_TEST_EXISTS))
        pix = gdk_pixbuf_new_from_file_at_size(icon_name, icon_size, icon_size, NULL);

    // Si aún no hay ícono, crea uno genérico
    if (!pix)
        pix = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                       "application-x-executable",
                                       icon_size,
                                       GTK_ICON_LOOKUP_USE_BUILTIN,
                                       NULL);

    if (pix) {
        image = gtk_image_new_from_pixbuf(pix);
        g_object_unref(pix);
    } else {
        image = gtk_image_new_from_icon_name("application-x-executable", GTK_ICON_SIZE_BUTTON);
    }

    gtk_button_set_image(GTK_BUTTON(m->plugin_button), image);
    gtk_widget_show(image);
}

/* ==== CONSTRUCTOR PRINCIPAL DEL PLUGIN MODERN MENU ==== */
GtkWidget *modernmenu_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* ==== CREACIÓN Y CONFIGURACIÓN INICIAL ==== */
    // Crea la estructura base que guarda todos los datos del menú
    ModernMenu *m = g_new0(ModernMenu, 1);
    m->panel = panel;
    m->window_shown = FALSE;
    m->current_dir = NULL;
    m->settings = settings;

    /* ==== LECTURA DEL ICONO CONFIGURADO ==== */
    // Intenta leer desde la configuración el icono elegido por el usuario
    // Si no hay ninguno, usa el icono del tema llamado "start-here"
    const char *icon_str = NULL;
    if (settings && config_setting_lookup_string(settings, "icon", &icon_str) && icon_str && *icon_str) {
        m->icon_path = g_strdup(icon_str);
    } else {
        m->icon_path = g_strdup("start-here");
    }

    /* ==== CREAR CONTENEDOR PRINCIPAL DEL PLUGIN EN EL PANEL ==== */
    // 'p' es el widget que se mostrará en el panel de LXDE
    GtkWidget *p = gtk_event_box_new();
    gtk_widget_set_has_window(p, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(p), 0);

    /* ==== BOTÓN DEL MENÚ (ICONO EN EL PANEL) ==== */
    // Este botón abre o cierra la ventana del menú
    GtkWidget *button = gtk_button_new();
    m->plugin_button = button;
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_container_set_border_width(GTK_CONTAINER(button), 0);

    // Actualiza el icono y agrega tooltip
    update_button_icon(m);
    gtk_widget_set_tooltip_text(button, "Menú de aplicaciones");
    gtk_container_add(GTK_CONTAINER(p), button);
    gtk_widget_show_all(p);

    /* ==== CREAR LA VENTANA POPUP DEL MENÚ ==== */
    // Es una ventana sin bordes, tipo menú emergente
    m->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(m->window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(m->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(m->window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(m->window), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    gtk_window_set_resizable(GTK_WINDOW(m->window), FALSE);
    gtk_widget_set_size_request(m->window, 750, 500);

    /* ==== CONTENEDOR PRINCIPAL DE LA VENTANA ==== */
    GtkWidget *main_box = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 12);
    gtk_container_add(GTK_CONTAINER(m->window), main_box);

    /* ==== CONTENEDOR HORIZONTAL PRINCIPAL ==== */
    // Aquí dentro se dividen categorías a la izquierda y apps a la derecha
    GtkWidget *content = gtk_hbox_new(FALSE, 8);
    gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);

    /* ==== CARGA DE FAVORITOS ==== */
    // Se prepara la lista de favoritos para poder mostrarlos luego
    load_favorites(m);

    /* ==== PANEL DE CATEGORÍAS ==== */
    GtkWidget *cat_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cat_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(cat_scroll, 200, 300); // <-- tamaño mínimo vertical

    GtkWidget *cat_box = gtk_vbox_new(FALSE, 4);
    gtk_widget_set_size_request(cat_box, -1, 300);      // <-- tamaño mínimo vertical
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(cat_scroll), cat_box);
    gtk_widget_show(cat_box);

    /* ==== BOTÓN DE FAVORITOS ==== */
    GtkWidget *btn_fav = gtk_toggle_button_new_with_label("★ Favoritos");
    gtk_button_set_relief(GTK_BUTTON(btn_fav), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(btn_fav, "Aplicaciones favoritas");
    g_signal_connect(btn_fav, "toggled", G_CALLBACK(show_favorites_category), m);
    m->btn_fav = btn_fav;
    gtk_box_pack_start(GTK_BOX(cat_box), btn_fav, FALSE, FALSE, 4);
    gtk_widget_show(btn_fav);

    /* ==== MODELO Y VISTA DE CATEGORÍAS ==== */
    m->cat_store = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_POINTER);
    m->categories = gtk_tree_view_new_with_model(GTK_TREE_MODEL(m->cat_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(m->categories), FALSE);

    // Renderiza texto en la lista
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Category", renderer, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(m->categories), column);

    // Conecta selección de categoría con su callback
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(m->categories));
    gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
    g_signal_connect(sel, "changed", G_CALLBACK(on_category_selected), m);

    gtk_box_pack_start(GTK_BOX(cat_box), m->categories, TRUE, TRUE, 0);
    gtk_widget_show(m->categories);


    /* ==== AÑADIR CATEGORÍAS AL PANEL PRINCIPAL ==== */
    gtk_box_pack_start(GTK_BOX(content), cat_scroll, FALSE, FALSE, 0);

    /* ==== PANEL DE APLICACIONES ==== */
    // Contenedor derecho que muestra las apps dentro de la categoría elegida
    m->apps_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m->apps_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    m->apps_box = gtk_vbox_new(FALSE, 4);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(m->apps_scroll), m->apps_box);
    gtk_box_pack_start(GTK_BOX(content), m->apps_scroll, TRUE, TRUE, 0);

    /* ==== BARRA INFERIOR: SALIR + BUSCAR ==== */
    GtkWidget *bottom_bar = gtk_hbox_new(FALSE, 6);
    gtk_box_pack_start(GTK_BOX(main_box), bottom_bar, FALSE, FALSE, 0);

    /* Botón "Salir" con icono */
    GtkWidget *btn_logout = gtk_button_new();
    GtkWidget *logout_img = gtk_image_new_from_icon_name("system-log-out", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(btn_logout), logout_img);
    gtk_button_set_label(GTK_BUTTON(btn_logout), "Salir");
    gtk_button_set_image_position(GTK_BUTTON(btn_logout), GTK_POS_LEFT);
    gtk_widget_set_tooltip_text(btn_logout, "Cerrar sesión");
    g_signal_connect(btn_logout, "clicked", G_CALLBACK(on_logout_clicked), m);
    gtk_box_pack_start(GTK_BOX(bottom_bar), btn_logout, FALSE, FALSE, 0);

    /* Espacio y buscador */
    GtkWidget *search_label = gtk_label_new("Buscar:");
    gtk_box_pack_start(GTK_BOX(bottom_bar), search_label, FALSE, FALSE, 4);

    m->search = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(bottom_bar), m->search, TRUE, TRUE, 0);

    /* ==== CARGA DE MENÚS Y DATOS ==== */
    // Usa menu-cache (del sistema) para obtener las categorías
    m->menu_cache = menu_cache_lookup_sync("applications.menu");
    if (m->menu_cache) {
        m->reload_notify = menu_cache_add_reload_notify(m->menu_cache,
                                                        on_menu_cache_reload_real, m);
        load_categories(m);
    }

    // Crea la lista completa de apps y muestra favoritos al inicio
    build_all_apps_list(m);
    show_favorites_category(NULL, m);

    /* ==== CONEXIONES DE SEÑALES ==== */
    g_signal_connect(m->search, "changed", G_CALLBACK(on_search_changed), m);
    g_signal_connect(button, "clicked", G_CALLBACK(show_or_hide_menu), m);
    g_signal_connect(m->window, "key-press-event", G_CALLBACK(on_window_key_press), m);
    g_signal_connect(m->window, "focus-out-event", G_CALLBACK(on_window_focus_out), m);

    /* ==== FINALIZACIÓN ==== */
    lxpanel_plugin_set_data(p, m, modern_menu_destructor);
    return p;
}

FM_DEFINE_MODULE(lxpanel_gtk, modernmenu)

/* Aplicar configuración */
static gboolean modernmenu_apply_config(gpointer user_data)
{
    GtkWidget *p = GTK_WIDGET(user_data);
    ModernMenu *m = lxpanel_plugin_get_data(p);
    if (!m || !m->settings)
        return FALSE;

    // Guardar ruta del icono en la configuración
    config_group_set_string(m->settings, "icon", m->icon_path);

    // Aplicar el cambio visual inmediatamente
    update_button_icon(m);

    return FALSE;  // FALSE indica que se destruye el diálogo al aplicar
}


static GtkWidget *modernmenu_config(LXPanel *panel, GtkWidget *p)
{
    ModernMenu *m = lxpanel_plugin_get_data(p);
    return lxpanel_generic_config_dlg(_("Modern Menu"), panel,
                                      modernmenu_apply_config, p,
                                      _("Icon"), &m->icon_path, CONF_TYPE_FILE_ENTRY,
                                      NULL);
}

LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = "Menu moderno",
    .description = "Menu de aplicaciones moderno con busqueda y favoritos incluidos",
    .new_instance = modernmenu_constructor,
    .config = modernmenu_config
};
