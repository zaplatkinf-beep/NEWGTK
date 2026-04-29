#include "client_gui.h"
#include "client_wrapper.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

static ClientGUI *g_gui = NULL;

static void format_timestamp_display(const char* src, char* dst, size_t dst_size) {
    if (!src || src[0] == '\0') {
        strncpy(dst, "N/A", dst_size);
        return;
    }

    if (strlen(src) >= 19 && isdigit(src[0]) && src[15] == '.') {
        snprintf(dst, dst_size, "%.4s.%.2s.%.2s %.2s:%.2s:%.2s.%s",
                 src, src+4, src+6, src+8, src+10, src+12, src+15);
        char *z = strchr(dst, 'Z');
        if (z) *z = '\0';
        return;
    }

    strncpy(dst, src, dst_size);
    dst[dst_size-1] = '\0';
}

static time_t parse_timestamp_display(const char *ts) {
    if (!ts || strlen(ts) == 0) return 0;
    struct tm tm = {0};
    double sec_frac = 0.0;
    
    // Формат "2026.04.17 09:48:37.051"
    if (sscanf(ts, "%d.%d.%d %d:%d:%lf", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &sec_frac) == 6) {
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        tm.tm_sec = (int)sec_frac;
        return mktime(&tm);
    }
    
    // Формат "20260417094837.051Z" (исходный)
    int year, month, day, hour, min;
    if (sscanf(ts, "%4d%2d%2d%2d%2d%lf", &year, &month, &day, &hour, &min, &sec_frac) == 6) {
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = (int)sec_frac;
        return mktime(&tm);
    }
    
    return 0;
}


static gboolean gui_add_log_entry_full_idle(gpointer data) {
    LogEntryFull *entry = (LogEntryFull*)data;
    if (entry && g_gui && g_gui->logs_store) {
        GtkTreeIter iter;
        gtk_list_store_append(g_gui->logs_store, &iter);
        gtk_list_store_set(g_gui->logs_store, &iter,
                           0, entry->entryId,
                           1, entry->timestamp,
                           2, entry->reason,
                           3, entry->source,
                           4, entry->description,
                           5, entry->valueQualityTime,
                           -1);
    }
    free(entry);
    return G_SOURCE_REMOVE;
}

// Idle-функция для сообщений лога клиента
static gboolean gui_log_idle(gpointer data) {
    char *msg = (char*)data;
    if (g_gui && g_gui->client_log_buffer) {
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(g_gui->client_log_buffer, &end);
        gtk_text_buffer_insert(g_gui->client_log_buffer, &end, msg, -1);
        GtkTextMark *mark = gtk_text_buffer_create_mark(g_gui->client_log_buffer, NULL, &end, FALSE);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(g_gui->client_log_view), mark, 0.0, TRUE, 0.0, 0.0);
        gtk_text_buffer_delete_mark(g_gui->client_log_buffer, mark);
    }
    free(msg);
    return G_SOURCE_REMOVE;
}

// Idle-функция для обновления статуса
static gboolean gui_update_status_idle(gpointer data) {
    char *status = (char*)data;
    if (g_gui && g_gui->logs_status_label) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Статус журнала: %s", status);
        gtk_label_set_text(GTK_LABEL(g_gui->logs_status_label), buf);
    }
    free(status);
    return G_SOURCE_REMOVE;
}

// Idle-функция для установки LogRef
static gboolean gui_set_log_ref_idle(gpointer data) {
    char *ref = (char*)data;
    if (g_gui && g_gui->log_ref_entry) {
        gtk_editable_set_text(GTK_EDITABLE(g_gui->log_ref_entry), ref);
    }
    free(ref);
    return G_SOURCE_REMOVE;
}

// Idle-функция для добавления узла модели (плоский список)
static gboolean gui_add_model_node_idle(gpointer data) {
    char **params = (char**)data;
    char *name = params[0];
    char *type = params[1];
    if (g_gui && g_gui->model_store) {
        GtkTreeIter iter;
        gtk_tree_store_append(g_gui->model_store, &iter, NULL);
        gtk_tree_store_set(g_gui->model_store, &iter, 0, name, 1, type, -1);
    }
    free(name);
    free(type);
    free(params);
    return G_SOURCE_REMOVE;
}

// Потокобезопасная функция для лога клиента
void client_gui_log(ClientGUI *gui, const char *message) {
    if (!gui) return;
    char *msg = malloc(strlen(message) + 2);
    strcpy(msg, message);
    strcat(msg, "\n");
    g_idle_add(gui_log_idle, msg);
}

// Потокобезопасная функция для добавления полной записи лога
void client_gui_add_log_entry_full(ClientGUI *gui, LogEntryFull *entry) {
    if (!gui || !entry) return;


    LogEntryFull *stored = malloc(sizeof(LogEntryFull));
    memcpy(stored, entry, sizeof(LogEntryFull));
    g_ptr_array_add(gui->log_entries, stored);


    if (!gui->filter_active) {
        LogEntryFull *display = malloc(sizeof(LogEntryFull));
        memcpy(display, entry, sizeof(LogEntryFull));
        g_idle_add(gui_add_log_entry_full_idle, display);
    } else {
        // Проверяем подходит ли запись под фильтр
        gboolean show = TRUE;
        if (gui->filter_start_id > 0 || gui->filter_end_id > 0) {
            if (entry->entryId < gui->filter_start_id || entry->entryId > gui->filter_end_id)
                show = FALSE;
        }
        if (show && (gui->filter_start_time != 0 || gui->filter_end_time != 0)) {
            time_t t = parse_timestamp_display(entry->timestamp);
            if ((gui->filter_start_time != 0 && t < gui->filter_start_time) ||
                (gui->filter_end_time != 0 && t > gui->filter_end_time))
                show = FALSE;
        }
        if (show) {
            LogEntryFull *display = malloc(sizeof(LogEntryFull));
            memcpy(display, entry, sizeof(LogEntryFull));
            g_idle_add(gui_add_log_entry_full_idle, display);
        }
    }
}

// Очистка таблицы и массива хранения
void client_gui_clear_logs(ClientGUI *gui) {
    if (!gui) return;
    if (gui->logs_store) {
        gtk_list_store_clear(gui->logs_store);
    }
    if (gui->log_entries) {
        for (guint i = 0; i < gui->log_entries->len; i++) {
            free(g_ptr_array_index(gui->log_entries, i));
        }
        g_ptr_array_free(gui->log_entries, TRUE);
        gui->log_entries = g_ptr_array_new();
    }
    gui->filter_active = FALSE;
}

// Добавление узла модели (вызывается из wrapper'а)
void client_gui_add_model_node(ClientGUI *gui, GtkTreeIter *parent, const char *name, const char *type) {
    if (!gui) return;
    char **params = malloc(2 * sizeof(char*));
    params[0] = strdup(name);
    params[1] = strdup(type);
    g_idle_add(gui_add_model_node_idle, params);
}

void client_gui_clear_model(ClientGUI *gui) {
    if (gui && gui->model_store) gtk_tree_store_clear(gui->model_store);
}

void client_gui_update_logs_status(ClientGUI *gui, const char *status) {
    if (!gui) return;
    char *s = strdup(status);
    g_idle_add(gui_update_status_idle, s);
}

void client_gui_set_log_ref(ClientGUI *gui, const char *logRef) {
    if (!gui) return;
    char *ref = strdup(logRef);
    g_idle_add(gui_set_log_ref_idle, ref);
}

// Применение фильтра по EntryID
void client_gui_apply_id_filter(ClientGUI *gui, int start, int end) {
    if (!gui) return;
    gui->filter_active = TRUE;
    gui->filter_start_id = start;
    gui->filter_end_id = end;
    gui->filter_start_time = 0;
    gui->filter_end_time = 0;

    gtk_list_store_clear(gui->logs_store);
    for (guint i = 0; i < gui->log_entries->len; i++) {
        LogEntryFull *entry = g_ptr_array_index(gui->log_entries, i);
        if (entry->entryId >= start && entry->entryId <= end) {
            LogEntryFull *display = malloc(sizeof(LogEntryFull));
            memcpy(display, entry, sizeof(LogEntryFull));
            g_idle_add(gui_add_log_entry_full_idle, display);
        }
    }
}

// Применение фильтра по времени
void client_gui_apply_time_filter(ClientGUI *gui, const char *start_str, const char *end_str) {
    if (!gui) return;
    gui->filter_active = TRUE;
    gui->filter_start_time = parse_timestamp_display(start_str);
    gui->filter_end_time = parse_timestamp_display(end_str);
    gui->filter_start_id = 0;
    gui->filter_end_id = 0;

    gtk_list_store_clear(gui->logs_store);
    for (guint i = 0; i < gui->log_entries->len; i++) {
        LogEntryFull *entry = g_ptr_array_index(gui->log_entries, i);
        time_t t = parse_timestamp_display(entry->timestamp);
        if ((gui->filter_start_time == 0 || t >= gui->filter_start_time) &&
            (gui->filter_end_time == 0 || t <= gui->filter_end_time)) {
            LogEntryFull *display = malloc(sizeof(LogEntryFull));
            memcpy(display, entry, sizeof(LogEntryFull));
            g_idle_add(gui_add_log_entry_full_idle, display);
        }
    }
}

// Сброс фильтра
void client_gui_reset_filter(ClientGUI *gui) {
    if (!gui) return;
    gui->filter_active = FALSE;
    gtk_list_store_clear(gui->logs_store);
    for (guint i = 0; i < gui->log_entries->len; i++) {
        LogEntryFull *entry = g_ptr_array_index(gui->log_entries, i);
        LogEntryFull *display = malloc(sizeof(LogEntryFull));
        memcpy(display, entry, sizeof(LogEntryFull));
        g_idle_add(gui_add_log_entry_full_idle, display);
    }
}

// Обновление чувствительности кнопок при подключении
void client_gui_set_connected(ClientGUI *gui, gboolean connected) {
    if (!gui) return;
    gtk_widget_set_sensitive(gui->connect_btn, !connected);
    gtk_widget_set_sensitive(gui->disconnect_btn, connected);
    gtk_widget_set_sensitive(gui->host_entry, !connected);
    gtk_widget_set_sensitive(gui->port_spin, !connected);
    gtk_widget_set_sensitive(gui->refresh_model_btn, connected);
    gtk_widget_set_sensitive(gui->detect_logs_btn, connected);
    gtk_widget_set_sensitive(gui->enable_log_btn, connected);
    gtk_widget_set_sensitive(gui->disable_log_btn, connected);
    gtk_widget_set_sensitive(gui->query_all_btn, connected);
    gtk_widget_set_sensitive(gui->filter_by_time_btn, connected);
    gtk_widget_set_sensitive(gui->filter_by_id_btn, connected);
    gtk_widget_set_sensitive(gui->reset_filter_btn, connected);
    gtk_label_set_text(GTK_LABEL(gui->status_label), connected ? "Статус: Подключен" : "Статус: Отключен");
    
        if (!connected) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui->use_password_checkbox), FALSE);
        gtk_editable_set_text(GTK_EDITABLE(gui->password_entry), "");
        gtk_widget_set_sensitive(gui->password_entry, FALSE);
        }

}

ClientGUI* client_gui_create(void) {
    ClientGUI *gui = g_new0(ClientGUI, 1);
    g_gui = gui;
    gui->log_entries = g_ptr_array_new();
    gui->filter_active = FALSE;
    return gui;
}
