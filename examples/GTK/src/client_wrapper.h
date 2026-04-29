#ifndef CLIENT_WRAPPER_H
#define CLIENT_WRAPPER_H

#include <stdbool.h>
#include <stdint.h>

// Структура для полной записи лога (8 полей)
typedef struct {
    int entryId;
    char timestamp[64];
    char reason[64];
    char source[256];
    char description[256];
    char valueQualityTime[512];
} LogEntryFullWrapper;

// Callback'и
void client_set_log_callback(void (*callback)(const char*));
void client_set_log_entry_full_callback(void (*callback)(LogEntryFullWrapper*));
void client_set_model_node_callback(void (*callback)(void*, const char*, const char*));
void client_set_log_status_callback(void (*callback)(const char*));
void client_set_log_ref_callback(void (*callback)(const char*));
void client_subscribe_reports(const char* rcbReference);
void client_set_connection_status_callback(void (*callback)(bool connected));


// Публичные функции
int client_connect(const char* hostname, int port);
void client_disconnect(void);
void client_refresh_model(void);
void client_detect_logs(void);
void client_enable_log(const char* logRef);
void client_disable_log(const char* logRef);
void client_query_all_logs(const char* logRef);
void client_query_logs_by_time(const char* logRef, uint64_t startTime, uint64_t endTime);
void client_query_logs_by_entry_id(const char* logRef, int startId, int endId);

#endif

#define REASON_DATA_CHANGE        0x01
#define REASON_QUALITY_CHANGE     0x02
#define REASON_DATA_UPDATE        0x04
#define REASON_INTEGRITY          0x08
#define REASON_GI                 0x10
#define REASON_APPLICATION_TRIGGER 0x20

