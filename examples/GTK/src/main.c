#include <gtk/gtk.h>
#include "client_gui.h"
#include "client_wrapper.h"

static ClientGUI *g_gui = NULL;

static void activate(GtkApplication *app, gpointer user_data);

static void on_connect_clicked(GtkButton *btn, gpointer data);
static void on_disconnect_clicked(GtkButton *btn, gpointer data);
static void on_refresh_model_clicked(GtkButton *btn, gpointer data);
static void on_detect_logs_clicked(GtkButton *btn, gpointer data);
static void on_enable_log_clicked(GtkButton *btn, gpointer data);
static void on_disable_log_clicked(GtkButton *btn, gpointer data);
static void on_query_all_clicked(GtkButton *btn, gpointer data);
static void on_filter_by_time_clicked(GtkButton *btn, gpointer data);
static void on_filter_by_id_clicked(GtkButton *btn, gpointer data);
static void on_reset_filter_clicked(GtkButton *btn, gpointer data);
static void on_clear_client_log_clicked(GtkButton *btn, gpointer data);
static void on_window_destroy(GtkWindow *window, gpointer data);

static void wrapper_log_cb(const char *msg);
static void wrapper_log_entry_full_cb(LogEntryFullWrapper *entry);
static void wrapper_model_node_cb(void *parent, const char *name, const char *type);
static void wrapper_log_status_cb(const char *status);
static void wrapper_log_ref_cb(const char *ref);
static void connection_status_idle(gpointer data) {
    bool connected = (bool)(intptr_t)data;
    client_gui_set_connected(g_gui, connected);
}

static void connection_status_callback(bool connected) {
    g_idle_add(connection_status_idle, (void*)(intptr_t)connected);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.iec61850.client", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
static void on_use_password_toggled(GtkCheckButton *check, ClientGUI *gui) {
    gboolean active = gtk_check_button_get_active(check);
    gtk_widget_set_sensitive(gui->password_entry, active);
    if (!active) {
        gtk_editable_set_text(GTK_EDITABLE(gui->password_entry), "");
    }
}
static void activate(GtkApplication *app, gpointer user_data) {
    ClientGUI *gui = client_gui_create();
    g_gui = gui;
client_set_connection_status_callback(connection_status_callback);
    client_set_log_callback(wrapper_log_cb);
    client_set_log_entry_full_callback(wrapper_log_entry_full_cb);
    client_set_model_node_callback(wrapper_model_node_cb);
    client_set_log_status_callback(wrapper_log_status_cb);
    client_set_log_ref_callback(wrapper_log_ref_cb);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "IEC 61850 Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 1400, 900);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    gui->window = window;

    GtkWidget *top_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child(GTK_WINDOW(window), top_box);

    // Панель подключения
    GtkWidget *connect_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(connect_box, 10);
    gtk_widget_set_margin_end(connect_box, 10);
    gtk_widget_set_margin_top(connect_box, 10);
    gtk_box_append(GTK_BOX(top_box), connect_box);

    GtkWidget *host_label = gtk_label_new("Хост:");
    gtk_box_append(GTK_BOX(connect_box), host_label);
    gui->host_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(gui->host_entry), "localhost");
    gtk_widget_set_size_request(gui->host_entry, 150, -1);
    gtk_box_append(GTK_BOX(connect_box), gui->host_entry);

    GtkWidget *password_label = gtk_label_new("Пароль:");
    gtk_box_append(GTK_BOX(connect_box), password_label);
    gui->password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(gui->password_entry), FALSE);  // скрываем ввод
    gtk_entry_set_placeholder_text(GTK_ENTRY(gui->password_entry), "Введите пароль");
    gtk_widget_set_size_request(gui->password_entry, 150, -1);
    gtk_box_append(GTK_BOX(connect_box), gui->password_entry);
    
    

	gui->use_password_checkbox = gtk_check_button_new_with_label("Использовать пароль");
	gtk_box_append(GTK_BOX(connect_box), gui->use_password_checkbox);


	gtk_widget_set_sensitive(gui->password_entry, FALSE);


	g_signal_connect(gui->use_password_checkbox, "toggled", G_CALLBACK(on_use_password_toggled), gui);
    
    GtkWidget *port_label = gtk_label_new("Порт:");
    gtk_box_append(GTK_BOX(connect_box), port_label);
    gui->port_spin = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui->port_spin), 102);
    gtk_widget_set_size_request(gui->port_spin, 80, -1);
    gtk_box_append(GTK_BOX(connect_box), gui->port_spin);

    gui->connect_btn = gtk_button_new_with_label("Подключиться");
    gtk_box_append(GTK_BOX(connect_box), gui->connect_btn);
    gui->disconnect_btn = gtk_button_new_with_label("Отключиться");
    gtk_widget_set_sensitive(gui->disconnect_btn, FALSE);
    gtk_box_append(GTK_BOX(connect_box), gui->disconnect_btn);
    gui->status_label = gtk_label_new("Статус: Отключен");
    gtk_widget_set_halign(gui->status_label, GTK_ALIGN_END);
    gtk_widget_set_hexpand(gui->status_label, TRUE);
    gtk_box_append(GTK_BOX(connect_box), gui->status_label);

    // Notebook
    gui->notebook = gtk_notebook_new();
    gtk_box_append(GTK_BOX(top_box), gui->notebook);

    //Вкладка 1: Логи сервера
    GtkWidget *logs_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(logs_tab, 10);
    gtk_widget_set_margin_end(logs_tab, 10);
    gtk_widget_set_margin_top(logs_tab, 10);
    gtk_widget_set_margin_bottom(logs_tab, 10);
    gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), logs_tab, gtk_label_new("Журналы сервера"));

    GtkWidget *logs_control = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(logs_tab), logs_control);

    GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(logs_control), row1);
    gui->detect_logs_btn = gtk_button_new_with_label("Автоопределение журнала");
    gtk_widget_set_sensitive(gui->detect_logs_btn, FALSE);
    gtk_box_append(GTK_BOX(row1), gui->detect_logs_btn);
    gui->enable_log_btn = gtk_button_new_with_label("Включить журнал");
    gtk_widget_set_sensitive(gui->enable_log_btn, FALSE);
    gtk_box_append(GTK_BOX(row1), gui->enable_log_btn);
    gui->disable_log_btn = gtk_button_new_with_label("Выключить журнал");
    gtk_widget_set_sensitive(gui->disable_log_btn, FALSE);
    gtk_box_append(GTK_BOX(row1), gui->disable_log_btn);
    gui->logs_status_label = gtk_label_new("Статус журнала: не определён");
    gtk_widget_set_halign(gui->logs_status_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(gui->logs_status_label, TRUE);
    gtk_box_append(GTK_BOX(row1), gui->logs_status_label);

    GtkWidget *row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(logs_control), row2);
    GtkWidget *log_ref_label = gtk_label_new("LogRef:");
    gtk_box_append(GTK_BOX(row2), log_ref_label);
    gui->log_ref_entry = gtk_entry_new();
    gtk_widget_set_size_request(gui->log_ref_entry, 400, -1);
    gtk_editable_set_text(GTK_EDITABLE(gui->log_ref_entry), "");
    gtk_box_append(GTK_BOX(row2), gui->log_ref_entry);

    GtkWidget *row3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(logs_control), row3);
    gui->query_all_btn = gtk_button_new_with_label("Запросить все записи");
    gtk_widget_set_sensitive(gui->query_all_btn, FALSE);
    gtk_box_append(GTK_BOX(row3), gui->query_all_btn);
    gui->reset_filter_btn = gtk_button_new_with_label("Сбросить фильтр");
    gtk_widget_set_sensitive(gui->reset_filter_btn, FALSE);
    gtk_box_append(GTK_BOX(row3), gui->reset_filter_btn);

    GtkWidget *time_filter_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(logs_control), time_filter_box);
    gtk_box_append(GTK_BOX(time_filter_box), gtk_label_new("Время с:"));
    gui->start_time_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(gui->start_time_entry), "YYYY.MM.DD HH:MM:SS");
    gtk_widget_set_size_request(gui->start_time_entry, 150, -1);
    gtk_box_append(GTK_BOX(time_filter_box), gui->start_time_entry);
    gtk_box_append(GTK_BOX(time_filter_box), gtk_label_new("по:"));
    gui->end_time_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(gui->end_time_entry), "YYYY.MM.DD HH:MM:SS");
    gtk_widget_set_size_request(gui->end_time_entry, 150, -1);
    gtk_box_append(GTK_BOX(time_filter_box), gui->end_time_entry);
    gui->filter_by_time_btn = gtk_button_new_with_label("Фильтр по времени");
    gtk_widget_set_sensitive(gui->filter_by_time_btn, FALSE);
    gtk_box_append(GTK_BOX(time_filter_box), gui->filter_by_time_btn);
    



    GtkWidget *id_filter_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(logs_control), id_filter_box);
    gtk_box_append(GTK_BOX(id_filter_box), gtk_label_new("EntryID с:"));
    gui->start_id_spin = gtk_spin_button_new_with_range(0, 1000000, 1);
    gtk_widget_set_size_request(gui->start_id_spin, 80, -1);
    gtk_box_append(GTK_BOX(id_filter_box), gui->start_id_spin);
    gtk_box_append(GTK_BOX(id_filter_box), gtk_label_new("по:"));
    gui->end_id_spin = gtk_spin_button_new_with_range(0, 1000000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui->end_id_spin), 100);
    gtk_widget_set_size_request(gui->end_id_spin, 80, -1);
    gtk_box_append(GTK_BOX(id_filter_box), gui->end_id_spin);
    gui->filter_by_id_btn = gtk_button_new_with_label("Фильтр по ID");
    gtk_widget_set_sensitive(gui->filter_by_id_btn, FALSE);
    gtk_box_append(GTK_BOX(id_filter_box), gui->filter_by_id_btn);

    // Таблица логов (8 колонок)
    GtkWidget *logs_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(logs_scroll, TRUE);
    gtk_box_append(GTK_BOX(logs_tab), logs_scroll);

    gui->logs_tree = gtk_tree_view_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(logs_scroll), gui->logs_tree);

    gui->logs_store = gtk_list_store_new(6,
    G_TYPE_INT,      // 0: № п/п
    G_TYPE_STRING,   // 1: Дата-время UTC
    G_TYPE_STRING,   // 2: Причина включения
    G_TYPE_STRING,   // 3: Источник события
    G_TYPE_STRING,   // 4: Описание
    G_TYPE_STRING    // 5: Значение, качество, время изменения
);
gtk_tree_view_set_model(GTK_TREE_VIEW(gui->logs_tree), GTK_TREE_MODEL(gui->logs_store));

GtkCellRenderer *rend = gtk_cell_renderer_text_new();
GtkTreeViewColumn *col;

col = gtk_tree_view_column_new_with_attributes("№ п/п", rend, "text", 0, NULL);
gtk_tree_view_append_column(GTK_TREE_VIEW(gui->logs_tree), col);

col = gtk_tree_view_column_new_with_attributes("Дата-время UTC", rend, "text", 1, NULL);
gtk_tree_view_column_set_resizable(col, TRUE);
gtk_tree_view_append_column(GTK_TREE_VIEW(gui->logs_tree), col);

col = gtk_tree_view_column_new_with_attributes("Причина включения", rend, "text", 2, NULL);
gtk_tree_view_column_set_resizable(col, TRUE);
gtk_tree_view_append_column(GTK_TREE_VIEW(gui->logs_tree), col);

col = gtk_tree_view_column_new_with_attributes("Источник события", rend, "text", 3, NULL);
gtk_tree_view_column_set_resizable(col, TRUE);
gtk_tree_view_append_column(GTK_TREE_VIEW(gui->logs_tree), col);

col = gtk_tree_view_column_new_with_attributes("Описание", rend, "text", 4, NULL);
gtk_tree_view_column_set_resizable(col, TRUE);
gtk_tree_view_append_column(GTK_TREE_VIEW(gui->logs_tree), col);

col = gtk_tree_view_column_new_with_attributes("Значение, качество, время изменения", rend, "text", 5, NULL);
gtk_tree_view_column_set_resizable(col, TRUE);
gtk_tree_view_column_set_expand(col, TRUE);
gtk_tree_view_append_column(GTK_TREE_VIEW(gui->logs_tree), col);

    //Вкладка 2: Модель данных
    GtkWidget *model_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(model_tab, 10);
    gtk_widget_set_margin_end(model_tab, 10);
    gtk_widget_set_margin_top(model_tab, 10);
    gtk_widget_set_margin_bottom(model_tab, 10);
    gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), model_tab, gtk_label_new("Модель данных"));

    GtkWidget *model_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(model_tab), model_top);
    gui->refresh_model_btn = gtk_button_new_with_label("Запросить модель");
    gtk_widget_set_sensitive(gui->refresh_model_btn, FALSE);
    gtk_box_append(GTK_BOX(model_top), gui->refresh_model_btn);

    GtkWidget *model_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(model_scroll, TRUE);
    gtk_box_append(GTK_BOX(model_tab), model_scroll);
    gui->model_tree = gtk_tree_view_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(model_scroll), gui->model_tree);
    gui->model_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(gui->model_tree), GTK_TREE_MODEL(gui->model_store));
    rend = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Имя", rend, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(gui->model_tree), col);
    col = gtk_tree_view_column_new_with_attributes("Тип", rend, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(gui->model_tree), col);

    // ==================== Вкладка 3: Лог клиента ====================
    GtkWidget *client_log_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(client_log_tab, 10);
    gtk_widget_set_margin_end(client_log_tab, 10);
    gtk_widget_set_margin_top(client_log_tab, 10);
    gtk_widget_set_margin_bottom(client_log_tab, 10);
    gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), client_log_tab, gtk_label_new("Лог клиента"));

    GtkWidget *log_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(log_scroll, TRUE);
    gtk_box_append(GTK_BOX(client_log_tab), log_scroll);
    gui->client_log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(gui->client_log_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(gui->client_log_view), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(log_scroll), gui->client_log_view);
    gui->client_log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gui->client_log_view));
    gui->clear_client_log_btn = gtk_button_new_with_label("Очистить лог клиента");
    gtk_widget_set_halign(gui->clear_client_log_btn, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(client_log_tab), gui->clear_client_log_btn);

    // Сигналы
    g_signal_connect(gui->connect_btn, "clicked", G_CALLBACK(on_connect_clicked), gui);
    g_signal_connect(gui->disconnect_btn, "clicked", G_CALLBACK(on_disconnect_clicked), gui);
    g_signal_connect(gui->refresh_model_btn, "clicked", G_CALLBACK(on_refresh_model_clicked), gui);
    g_signal_connect(gui->detect_logs_btn, "clicked", G_CALLBACK(on_detect_logs_clicked), gui);
    g_signal_connect(gui->enable_log_btn, "clicked", G_CALLBACK(on_enable_log_clicked), gui);
    g_signal_connect(gui->disable_log_btn, "clicked", G_CALLBACK(on_disable_log_clicked), gui);
    g_signal_connect(gui->query_all_btn, "clicked", G_CALLBACK(on_query_all_clicked), gui);
    g_signal_connect(gui->filter_by_time_btn, "clicked", G_CALLBACK(on_filter_by_time_clicked), gui);
    g_signal_connect(gui->filter_by_id_btn, "clicked", G_CALLBACK(on_filter_by_id_clicked), gui);
    g_signal_connect(gui->reset_filter_btn, "clicked", G_CALLBACK(on_reset_filter_clicked), gui);
    g_signal_connect(gui->clear_client_log_btn, "clicked", G_CALLBACK(on_clear_client_log_clicked), gui);


    client_gui_log(gui, "GUI запущен. Подключитесь к серверу.");
    gtk_window_present(GTK_WINDOW(window));
}

// Реализация обработчиков кнопок
static void on_connect_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    const char *host = gtk_editable_get_text(GTK_EDITABLE(gui->host_entry));
    int port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui->port_spin));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(gui->password_entry));
gboolean use_password = gtk_check_button_get_active(GTK_CHECK_BUTTON(gui->use_password_checkbox));

	if (use_password && password && strlen(password) > 0) {
	    client_set_password(password);
	} else {
	    client_set_password(NULL);
	}
    
    client_gui_clear_logs(gui);
    client_gui_log(gui, "Подключение...");

    client_connect(host, port);
}

static void on_disconnect_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    client_disconnect();
    client_gui_set_connected(gui, FALSE);
    client_gui_log(gui, "Отключено");
}

static void on_refresh_model_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    client_gui_clear_model(gui);
    client_gui_log(gui, "Запрос модели...");
    client_refresh_model();
}

static void on_detect_logs_clicked(GtkButton *btn, gpointer data) {
    client_detect_logs();
}

static void on_enable_log_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    const char *ref = gtk_editable_get_text(GTK_EDITABLE(gui->log_ref_entry));
    if (ref && ref[0]) client_enable_log(ref);
    else client_gui_log(gui, "Сначала определите журнал");
}

static void on_disable_log_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    const char *ref = gtk_editable_get_text(GTK_EDITABLE(gui->log_ref_entry));
    if (ref && ref[0]) client_disable_log(ref);
    else client_gui_log(gui, "Сначала определите журнал");
}

static void on_query_all_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    const char *ref = gtk_editable_get_text(GTK_EDITABLE(gui->log_ref_entry));
    if (ref && ref[0]) {
        client_gui_clear_logs(gui);
        client_query_all_logs(ref);
        client_gui_reset_filter(gui);
    } else client_gui_log(gui, "Сначала определите журнал");
}

static void on_filter_by_time_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    const char *ref = gtk_editable_get_text(GTK_EDITABLE(gui->log_ref_entry));
    if (!ref || !ref[0]) {
        client_gui_log(gui, "Сначала определите журнал");
        return;
    }
    const char *start_str = gtk_editable_get_text(GTK_EDITABLE(gui->start_time_entry));
    const char *end_str = gtk_editable_get_text(GTK_EDITABLE(gui->end_time_entry));
    client_gui_apply_time_filter(gui, start_str, end_str);
    client_gui_log(gui, "Применён фильтр по времени");
}

static void on_filter_by_id_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    const char *ref = gtk_editable_get_text(GTK_EDITABLE(gui->log_ref_entry));
    if (!ref || !ref[0]) {
        client_gui_log(gui, "Сначала определите журнал");
        return;
    }
    int start = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui->start_id_spin));
    int end = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(gui->end_id_spin));
    client_gui_apply_id_filter(gui, start, end);
    char msg[256];
    snprintf(msg, sizeof(msg), "Применён фильтр по ID от %d до %d", start, end);
    client_gui_log(gui, msg);
}

static void on_reset_filter_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    client_gui_reset_filter(gui);
    client_gui_log(gui, "Фильтр сброшен");
}

static void on_clear_client_log_clicked(GtkButton *btn, gpointer data) {
    ClientGUI *gui = data;
    gtk_text_buffer_set_text(gui->client_log_buffer, "", -1);
}

static void on_window_destroy(GtkWindow *window, gpointer data) {
    client_disconnect();
}

// Обертки для callback'ов
static void wrapper_log_cb(const char *msg) {
    if (g_gui) client_gui_log(g_gui, msg);
}

static void wrapper_log_entry_full_cb(LogEntryFullWrapper *entry) {
    if (!g_gui) return;
    LogEntryFull full;
    full.entryId = entry->entryId;
    strcpy(full.timestamp, entry->timestamp);
    strcpy(full.reason, entry->reason);
    strcpy(full.source, entry->source);
    strcpy(full.description, entry->description);
    strcpy(full.valueQualityTime, entry->valueQualityTime);
    client_gui_add_log_entry_full(g_gui, &full);
}

static void wrapper_model_node_cb(void *parent, const char *name, const char *type) {
    if (g_gui) client_gui_add_model_node(g_gui, (GtkTreeIter*)parent, name, type);
}

static void wrapper_log_status_cb(const char *status) {
    if (g_gui) client_gui_update_logs_status(g_gui, status);
}

static void wrapper_log_ref_cb(const char *ref) {
    if (g_gui) client_gui_set_log_ref(g_gui, ref);
}
