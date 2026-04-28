#ifndef CLIENT_GUI_H
#define CLIENT_GUI_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <glib.h>

// Структура для хранения полной записи лога (8 полей)
typedef struct {
    int entryId;                 // № п/п
    char timestamp[64];          // Дата-время UTC
    char reason[64];             // Причина включения
    char source[256];            // Источник события
    char description[256];       // Описание
    char valueQualityTime[512];  // Значение, качество, метка времени
} LogEntryFull;

typedef struct {
    GtkWidget *window;
    GtkWidget *notebook;

    // Вкладка логов сервера
    GtkWidget *logs_tree;
    GtkListStore *logs_store;
    GtkWidget *log_ref_entry;
    GtkWidget *detect_logs_btn;
    GtkWidget *enable_log_btn;
    GtkWidget *disable_log_btn;
    GtkWidget *query_all_btn;
    GtkWidget *filter_by_time_btn;
    GtkWidget *filter_by_id_btn;
    GtkWidget *reset_filter_btn;
    GtkWidget *start_time_entry;
    GtkWidget *end_time_entry;
    GtkWidget *start_id_spin;
    GtkWidget *end_id_spin;
    GtkWidget *logs_status_label;

    // Вкладка модели
    GtkWidget *model_tree;
    GtkTreeStore *model_store;
    GtkWidget *refresh_model_btn;

    // Вкладка лога клиента
    GtkWidget *client_log_view;
    GtkTextBuffer *client_log_buffer;
    GtkWidget *clear_client_log_btn;
    GtkWidget *password_entry;   // поле ввода пароля
    // Общее
    GtkWidget *host_entry;
    GtkWidget *port_spin;
    GtkWidget *connect_btn;
    GtkWidget *disconnect_btn;
    GtkWidget *status_label;

    // Для хранения и фильтрации
    GPtrArray *log_entries;      // массив LogEntryFull*
    gboolean filter_active;
    int filter_start_id;
    int filter_end_id;
    time_t filter_start_time;
    time_t filter_end_time;

} ClientGUI;

ClientGUI* client_gui_create(void);

// Потокобезопасные функции
void client_gui_log(ClientGUI *gui, const char *message);
void client_gui_add_log_entry_full(ClientGUI *gui, LogEntryFull *entry);
void client_gui_clear_logs(ClientGUI *gui);
void client_gui_add_model_node(ClientGUI *gui, GtkTreeIter *parent, const char *name, const char *type);
void client_gui_clear_model(ClientGUI *gui);
void client_gui_update_logs_status(ClientGUI *gui, const char *status);
void client_gui_set_log_ref(ClientGUI *gui, const char *logRef);
void client_gui_set_connected(ClientGUI *gui, gboolean connected);

// Функции фильтрации
void client_gui_apply_id_filter(ClientGUI *gui, int start, int end);
void client_gui_apply_time_filter(ClientGUI *gui, const char *start_str, const char *end_str);
void client_gui_reset_filter(ClientGUI *gui);

#endif
