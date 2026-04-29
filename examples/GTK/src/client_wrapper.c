#define _GNU_SOURCE
#include "client_wrapper.h"
#include "iec61850_client.h"
#include <glib.h>
#ifndef REASON_DATA_CHANGE
#define REASON_DATA_CHANGE        0x01
#endif

#ifndef REASON_QUALITY_CHANGE
#define REASON_QUALITY_CHANGE     0x02
#endif

#ifndef REASON_DATA_UPDATE
#define REASON_DATA_UPDATE        0x04
#endif

#ifndef REASON_INTEGRITY
#define REASON_INTEGRITY          0x08
#endif

#ifndef REASON_GI
#define REASON_GI                 0x10
#endif

#ifndef REASON_APPLICATION_TRIGGER
#define REASON_APPLICATION_TRIGGER 0x20
#endif
#include "hal_thread.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <ctype.h>
static AcseAuthenticationParameter g_auth = NULL;
static IedConnection g_con = NULL;
static pthread_t g_client_thread = 0;
static volatile int g_running = 0;
static char g_current_log_ref[256] = "";
static char g_password[64] = "";   // пароль
static void (*g_log_callback)(const char*) = NULL;
static void (*g_log_entry_full_callback)(LogEntryFullWrapper*) = NULL;
static void (*g_model_node_callback)(void*, const char*, const char*) = NULL;
static void (*g_log_status_callback)(const char*) = NULL;
static void (*g_log_ref_callback)(const char*) = NULL;

static void (*g_connection_status_callback)(bool) = NULL;

/* Установка пароля для аутентификации  */
void client_set_password(const char* password);

// Установка callback'ов
void client_set_log_callback(void (*callback)(const char*)) { g_log_callback = callback; }
void client_set_log_entry_full_callback(void (*callback)(LogEntryFullWrapper*)) {
    g_log_entry_full_callback = callback;
}

void client_set_model_node_callback(void (*callback)(void*, const char*, const char*)) { g_model_node_callback = callback; }
void client_set_log_status_callback(void (*callback)(const char*)) { g_log_status_callback = callback; }
void client_set_log_ref_callback(void (*callback)(const char*)) { g_log_ref_callback = callback; }
void client_set_connection_status_callback(void (*callback)(bool connected)) {
    g_connection_status_callback = callback;
}

// Вспомогательная функция логирования
static void client_log(const char* format, ...) {
    if (!g_log_callback) return;
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    g_log_callback(buffer);
}
void client_set_password(const char* password) {
    if (password)
        strncpy(g_password, password, sizeof(g_password)-1);
    else
        g_password[0] = '\0';
}
// Обход модели данных и отправка узлов в GUI
static void browse_data_directory(const char* ref, IedConnection con, void* parent, int spaces) {
    IedClientError error;
    LinkedList attrs = IedConnection_getDataDirectory(con, &error, ref);
    if (attrs) {
        LinkedList node = LinkedList_getNext(attrs);
        while (node) {
            char* name = (char*)node->data;
            char fullRef[512];
            snprintf(fullRef, sizeof(fullRef), "%s.%s", ref, name);
            if (g_model_node_callback)
                g_model_node_callback(parent, name, "DA");
            browse_data_directory(fullRef, con, parent, spaces+2);
            node = LinkedList_getNext(node);
        }
        LinkedList_destroy(attrs);
    }
}

// Запрос модели устройства
static void* model_thread_func(void* arg) {
    if (!g_con) {
        client_log("Нет подключения для запроса модели");
        return NULL;
    }
    IedClientError error;
    client_log("Начало обхода модели данных...");

    LinkedList deviceList = IedConnection_getLogicalDeviceList(g_con, &error);
    if (error != IED_ERROR_OK) {
        client_log("Ошибка получения списка устройств: %d", error);
        return NULL;
    }
    LinkedList device = LinkedList_getNext(deviceList);
    while (device) {
        char* ldName = (char*)device->data;
        if (g_model_node_callback)
            g_model_node_callback(NULL, ldName, "LD");
        LinkedList nodes = IedConnection_getLogicalDeviceDirectory(g_con, &error, ldName);
        LinkedList ln = LinkedList_getNext(nodes);
        while (ln) {
            char* lnName = (char*)ln->data;
            char lnRef[256];
            snprintf(lnRef, sizeof(lnRef), "%s/%s", ldName, lnName);
            if (g_model_node_callback)
                g_model_node_callback(NULL, lnName, "LN");
            LinkedList dataObjects = IedConnection_getLogicalNodeDirectory(g_con, &error, lnRef, ACSI_CLASS_DATA_OBJECT);
            LinkedList dobj = LinkedList_getNext(dataObjects);
            while (dobj) {
                char* doName = (char*)dobj->data;
                char doRef[256];
                snprintf(doRef, sizeof(doRef), "%s/%s.%s", ldName, lnName, doName);
                if (g_model_node_callback)
                    g_model_node_callback(NULL, doName, "DO");
                browse_data_directory(doRef, g_con, NULL, 0);
                dobj = LinkedList_getNext(dobj);
            }
            LinkedList_destroy(dataObjects);
            ln = LinkedList_getNext(ln);
        }
        LinkedList_destroy(nodes);
        device = LinkedList_getNext(device);
    }
    LinkedList_destroy(deviceList);
    client_log("Обход модели завершён");
    return NULL;
}

// Форматирование времени в "YYYY.MM.DD HH:MM:SS.ms"
static void format_timestamp(const char* src, char* dst, size_t dst_size) {
    if (!src || src[0] == '\0') {
        strncpy(dst, "N/A", dst_size);
        return;
    }
    
    int year, month, day, hour, min, sec, ms;
    double sec_frac = 0.0;
    
    // Формат 1
    if (sscanf(src, "%4d%2d%2d%2d%2d%lf", &year, &month, &day, &hour, &min, &sec_frac) == 6) {
        sec = (int)sec_frac;
        ms = (int)((sec_frac - sec) * 1000);
        if (ms < 0) ms = 0;
        snprintf(dst, dst_size, "%04d.%02d.%02d %02d:%02d:%02d.%03d",
                 year, month, day, hour, min, sec, ms);
        return;
    }
    
    // Формат 2
    if (sscanf(src, "%d-%d-%d %d:%d:%lf", &year, &month, &day, &hour, &min, &sec_frac) == 6) {
        sec = (int)sec_frac;
        ms = (int)((sec_frac - sec) * 1000);
        if (ms < 0) ms = 0;
        snprintf(dst, dst_size, "%04d.%02d.%02d %02d:%02d:%02d.%03d",
                 year, month, day, hour, min, sec, ms);
        return;
    }
    
    // Формат 3
    if (sscanf(src, "%d-%d-%dT%d:%d:%lf", &year, &month, &day, &hour, &min, &sec_frac) == 6) {
        sec = (int)sec_frac;
        ms = (int)((sec_frac - sec) * 1000);
        if (ms < 0) ms = 0;
        snprintf(dst, dst_size, "%04d.%02d.%02d %02d:%02d:%02d.%03d",
                 year, month, day, hour, min, sec, ms);
        return;
    }
    

    strncpy(dst, src, dst_size);
    dst[dst_size-1] = '\0';
}


static const char* reason_code_to_string(uint8_t code) {
    if (code == 0) return "По умолчанию";
    
    static char result[256];
    result[0] = '\0';
    int first = 1;
    
    if (code & REASON_DATA_CHANGE) {
        strcat(result, first ? "Изменение данных" : " | Изменение данных");
        first = 0;
    }
    if (code & REASON_QUALITY_CHANGE) {
        strcat(result, first ? "Изменение качества" : " | Изменение качества");
        first = 0;
    }
    if (code & REASON_DATA_UPDATE) {
        strcat(result, first ? "Обновление данных" : " | Обновление данных");
        first = 0;
    }
    if (code & REASON_INTEGRITY) {
        strcat(result, first ? "Периодический" : " | Периодический");
        first = 0;
    }
    if (code & REASON_GI) {
        strcat(result, first ? "Общий опрос" : " | Общий опрос");
        first = 0;
    }
    if (code & REASON_APPLICATION_TRIGGER) {
        strcat(result, first ? "Приложение" : " | Приложение");
        first = 0;
    }
    
    if (result[0] == '\0') {
        snprintf(result, sizeof(result), "0x%02X", code);
    }
    
    return result;
}

static void extract_source(const char* tag, char* dst, size_t dst_size) {
    if (!tag || !dst) return;

    char temp[512];
    strncpy(temp, tag, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';


    const char* patterns[] = {"$ST$", "$MX$", "$CF$", "$CO$"};
    for (int i = 0; i < 4; i++) {
        char* p = strstr(temp, patterns[i]);
        if (p) {

            *p = '.';
            memmove(p + 1, p + 4, strlen(p + 4) + 1);
        }
    }


    for (char* p = temp; *p; p++) {
        if (*p == '$' || *p == '/') *p = '.';
    }


    char* last_dot = strrchr(temp, '.');
    if (last_dot && last_dot > temp) {
        *last_dot = '\0';
    }

    strncpy(dst, temp, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void extract_description(const char* tag, char* dst, size_t dst_size) {
    if (!tag || !dst) return;
    
    // Ищем последний $ или . или /
    const char* last = strrchr(tag, '$');
    const char* last_dot = strrchr(tag, '.');
    const char* last_slash = strrchr(tag, '/');
    
    if (last && last_dot && last_dot > last) last = last_dot;
    if (last_slash && last_slash > last) last = last_slash;
    
    if (last && *(last + 1)) {
        const char* start = last + 1;
        size_t len = 0;
        while (start[len] && start[len] != '$' && start[len] != '.' && start[len] != '/' && len < dst_size - 1) {
            dst[len] = start[len];
            len++;
        }
        dst[len] = '\0';
    } else {
        strncpy(dst, tag, dst_size - 1);
        dst[dst_size - 1] = '\0';
    }
}



// Автоопределение журнала
void client_detect_logs(void) {
    if (!g_con) {
        client_log("Нет подключения");
        if (g_log_status_callback) g_log_status_callback("Ошибка: нет подключения");
        return;
    }
    IedClientError error;
    LinkedList deviceList = IedConnection_getLogicalDeviceList(g_con, &error);
    if (error != IED_ERROR_OK) {
        client_log("Ошибка получения списка устройств");
        return;
    }
    char foundLogRef[256] = "";
    LinkedList device = LinkedList_getNext(deviceList);
    while (device && foundLogRef[0]==0) {
        char* ld = (char*)device->data;
        LinkedList nodes = IedConnection_getLogicalDeviceDirectory(g_con, &error, ld);
        LinkedList ln = LinkedList_getNext(nodes);
        while (ln && foundLogRef[0]==0) {
            char* lnName = (char*)ln->data;
            char lnRef[256];
            snprintf(lnRef, sizeof(lnRef), "%s/%s", ld, lnName);
            LinkedList logs = IedConnection_getLogicalNodeDirectory(g_con, &error, lnRef, ACSI_CLASS_LOG);
            LinkedList log = LinkedList_getNext(logs);
            while (log && foundLogRef[0]==0) {
                char* logName = (char*)log->data;
                if (strstr(logName, "EventLog") != NULL || strstr(logName, "Log") != NULL) {
                    snprintf(foundLogRef, sizeof(foundLogRef), "%s/%s$%s", ld, lnName, logName);
                }
                log = LinkedList_getNext(log);
            }
            LinkedList_destroy(logs);
            ln = LinkedList_getNext(ln);
        }
        LinkedList_destroy(nodes);
        device = LinkedList_getNext(device);
    }
    LinkedList_destroy(deviceList);
    if (foundLogRef[0]) {
        strcpy(g_current_log_ref, foundLogRef);
        if (g_log_ref_callback) g_log_ref_callback(foundLogRef);
        if (g_log_status_callback) g_log_status_callback("Журнал найден");
        client_log("Найден журнал: %s", foundLogRef);
    } else {
        if (g_log_status_callback) g_log_status_callback("Журналы не найдены");
        client_log("Журналы не найдены");
    }
}

// Включение/выключение журнала
void client_enable_log(const char* logRef) {
    if (!g_con || !logRef) return;
    IedClientError error;
    char logEnaRef[512];
    strncpy(logEnaRef, logRef, sizeof(logEnaRef)-1);
    char *p = strrchr(logEnaRef, '$');
    if (p) *p = '.';
    strncat(logEnaRef, ".LogEna", sizeof(logEnaRef)-strlen(logEnaRef)-1);
    IedConnection_writeBooleanValue(g_con, &error, logEnaRef, IEC61850_FC_LG, true);
    if (error == IED_ERROR_OK) {
        client_log("Журнал %s включён", logRef);
        if (g_log_status_callback) g_log_status_callback("Журнал включён");
    } else {
        client_log("Ошибка включения журнала: %d", error);
    }
}

void client_disable_log(const char* logRef) {
    if (!g_con || !logRef) return;
    IedClientError error;
    char logEnaRef[512];
    strncpy(logEnaRef, logRef, sizeof(logEnaRef)-1);
    char *p = strrchr(logEnaRef, '$');
    if (p) *p = '.';
    strncat(logEnaRef, ".LogEna", sizeof(logEnaRef)-strlen(logEnaRef)-1);
    IedConnection_writeBooleanValue(g_con, &error, logEnaRef, IEC61850_FC_LG, false);
    if (error == IED_ERROR_OK) {
        client_log("Журнал %s выключен", logRef);
        if (g_log_status_callback) g_log_status_callback("Журнал выключен");
    } else {
        client_log("Ошибка выключения журнала: %d", error);
    }
}

void client_query_all_logs(const char* logRef) {
    if (!g_con || !logRef) {
        client_log("Ошибка: нет подключения или LogRef");
        return;
    }

    IedClientError error;

    // Включаем журнал (LogEna)
    char logEnaRef[512];
    strncpy(logEnaRef, logRef, sizeof(logEnaRef) - 1);
    char *p = strrchr(logEnaRef, '$');
    if (p) *p = '.';
    strncat(logEnaRef, ".LogEna", sizeof(logEnaRef) - strlen(logEnaRef) - 1);
    IedConnection_writeBooleanValue(g_con, &error, logEnaRef, IEC61850_FC_LG, true);
    if (error != IED_ERROR_OK)
        client_log("Предупреждение: не удалось включить журнал: %d", error);
    Thread_sleep(500);

    // Читаем LCB
    char logCbRef[512];
    strcpy(logCbRef, logEnaRef);
    char *dot = strrchr(logCbRef, '.');
    if (dot) *dot = '\0';
    MmsValue* lcbValue = IedConnection_readObject(g_con, &error, logCbRef, IEC61850_FC_LG);
    if (error != IED_ERROR_OK || !lcbValue) {
        client_log("Ошибка чтения LCB: %d", error);
        if (g_log_status_callback) g_log_status_callback("Ошибка чтения LCB");
        return;
    }

    MmsValue* oldEntry = MmsValue_getElement(lcbValue, 5);
    MmsValue* oldEntryTm = MmsValue_getElement(lcbValue, 3);
    uint64_t timestamp = oldEntryTm ? MmsValue_getUtcTimeInMs(oldEntryTm) : 0;

    bool more = true;
    int totalEntries = 0;
    int queryCount = 0;
    MmsValue* lastEntry = NULL;
    uint64_t lastTimestamp = 0;

    while (more) {
        queryCount++;
        LinkedList entries = IedConnection_queryLogAfter(g_con, &error, logRef,
                                    lastEntry ? lastEntry : oldEntry,
                                    lastEntry ? lastTimestamp : timestamp,
                                    &more);
        if (error != IED_ERROR_OK) {
            client_log("Ошибка запроса журнала: %d", error);
            break;
        }
        int cnt = LinkedList_size(entries);
        if (cnt == 0) break;

        LinkedList elem = LinkedList_getNext(entries);
        while (elem) {
            MmsJournalEntry je = (MmsJournalEntry)elem->data;
            totalEntries++;
            int entryId = totalEntries;

            // Время события
            char timeBuf[64] = "";
            MmsValue* occ = MmsJournalEntry_getOccurenceTime(je);
            if (occ) {
                char rawTime[64];
                MmsValue_printToBuffer(occ, rawTime, sizeof(rawTime));
                format_timestamp(rawTime, timeBuf, sizeof(timeBuf));
            }

            // Переменные для извлечения
            char valueStr[256] = "";
            char qualityStr[64] = "good";
            char changeTimeStr[64] = "";
            char sourceStr[256] = "";
            char descStr[256] = "";
            char reasonStr[64] = "По умолчанию";
            char tagValue[256] = "";      // тег значения stVal или mag.f
            char tagQuality[256] = "";    // тег качества q
            char tagTime[256] = "";       // тег времени t
            int foundValue = 0;
		int foundQuality = 0;

            // Обходим все переменные записи
            LinkedList vars = LinkedList_getNext(je->journalVariables);
            while (vars) {
                MmsJournalVariable var = (MmsJournalVariable)vars->data;
                const char* tag = MmsJournalVariable_getTag(var);
                MmsValue* val = MmsJournalVariable_getValue(var);

                char valStr[256] = "";
                if (val) {
                    switch (MmsValue_getType(val)) {
                        case MMS_BOOLEAN: snprintf(valStr, sizeof(valStr), "%s", MmsValue_getBoolean(val)?"true":"false"); break;
                        case MMS_INTEGER: snprintf(valStr, sizeof(valStr), "%d", MmsValue_toInt32(val)); break;
                        case MMS_UNSIGNED: snprintf(valStr, sizeof(valStr), "%u", MmsValue_toUint32(val)); break;
                        case MMS_FLOAT:   snprintf(valStr, sizeof(valStr), "%f", MmsValue_toFloat(val)); break;
                        default: MmsValue_printToBuffer(val, valStr, sizeof(valStr));
                    }
                }

                // Извлекаем последний компонент тега
                const char* last_sep = strrchr(tag, '$');
                if (!last_sep) last_sep = strrchr(tag, '.');
                const char* attr = last_sep ? last_sep + 1 : tag;

                if (strcmp(attr, "stVal") == 0 || strcmp(attr, "mag") == 0 || 
			(strcmp(attr, "f") == 0 && strstr(tag, "$mag$f"))) {
			strcpy(valueStr, valStr);
			strcpy(tagValue, tag);
			foundValue = 1;
		    } else if (strcmp(attr, "q") == 0) {
			strcpy(qualityStr, valStr);
			strcpy(tagQuality, tag);
			foundQuality = 1;
		    } else if (strcmp(attr, "t") == 0) {
			char tmp[64];
			format_timestamp(valStr, tmp, sizeof(tmp));
			strcpy(changeTimeStr, tmp);
			strcpy(tagTime, tag);
		    }
		    vars = LinkedList_getNext(vars);
		}


            if (valueStr[0] == '\0') {
                strcpy(valueStr, "—");
            }


			if (foundValue && tagValue[0] != '\0') {
			    extract_source(tagValue, sourceStr, sizeof(sourceStr));
			    extract_description(tagValue, descStr, sizeof(descStr));
			    client_log("DEBUG: source from value: %s -> %s", tagValue, sourceStr);
			} else if (foundQuality && tagQuality[0] != '\0') {
			    extract_source(tagQuality, sourceStr, sizeof(sourceStr));
			    extract_description(tagQuality, descStr, sizeof(descStr));
			    client_log("DEBUG: source from quality: %s -> %s", tagQuality, sourceStr);
			} else if (tagTime[0] != '\0') {
			    extract_source(tagTime, sourceStr, sizeof(sourceStr));
			    extract_description(tagTime, descStr, sizeof(descStr));
			    client_log("DEBUG: source from time: %s -> %s", tagTime, sourceStr);
			} else {
			    strcpy(sourceStr, "Неизвестно");
			    strcpy(descStr, "");
			}

            // Извлечение ReasonCode
            uint8_t rc = 0;
            LinkedList vars2 = LinkedList_getNext(je->journalVariables);
            while (vars2) {
                MmsJournalVariable var = (MmsJournalVariable)vars2->data;
                const char* tag = MmsJournalVariable_getTag(var);
                if (strcmp(tag, "ReasonCode") == 0) {
                    MmsValue* val = MmsJournalVariable_getValue(var);
                    if (val && MmsValue_getType(val) == MMS_INTEGER) {
                        rc = (uint8_t)MmsValue_toInt32(val);
                        break;
                    }
                }
                vars2 = LinkedList_getNext(vars2);
            }
            char rc_buf[16];
            snprintf(rc_buf, sizeof(rc_buf), "%d", rc);
		const char* rc_str = reason_code_to_string(rc);
            strcpy(reasonStr, rc_str);

            // Если время изменения не найдено, используем время события
            if (changeTimeStr[0] == '\0') {
                strcpy(changeTimeStr, timeBuf);
            }

            // Формируем колонку Значение, качество, время изменения
            char valueQualityTime[512] = "";
            snprintf(valueQualityTime, sizeof(valueQualityTime), "%s,%s,%s",
                     valueStr, qualityStr, changeTimeStr);

            // Заполняем структуру для GUI
            LogEntryFullWrapper fullEntry;
            fullEntry.entryId = entryId;
            strncpy(fullEntry.timestamp, timeBuf, sizeof(fullEntry.timestamp)-1);
            fullEntry.timestamp[sizeof(fullEntry.timestamp)-1] = '\0';
            strncpy(fullEntry.reason, reasonStr, sizeof(fullEntry.reason)-1);
            fullEntry.reason[sizeof(fullEntry.reason)-1] = '\0';
            strncpy(fullEntry.source, sourceStr, sizeof(fullEntry.source)-1);
            fullEntry.source[sizeof(fullEntry.source)-1] = '\0';
            strncpy(fullEntry.description, descStr, sizeof(fullEntry.description)-1);
            fullEntry.description[sizeof(fullEntry.description)-1] = '\0';
            strncpy(fullEntry.valueQualityTime, valueQualityTime, sizeof(fullEntry.valueQualityTime)-1);
            fullEntry.valueQualityTime[sizeof(fullEntry.valueQualityTime)-1] = '\0';

            if (g_log_entry_full_callback) {
                g_log_entry_full_callback(&fullEntry);
            }

            elem = LinkedList_getNext(elem);
        }

        // Запоминаем последнюю запись для следующего запроса
        LinkedList lastElem = LinkedList_getLastElement(entries);
        if (lastElem) {
            MmsJournalEntry lastJe = (MmsJournalEntry)lastElem->data;
            if (lastEntry) MmsValue_delete(lastEntry);
            lastEntry = MmsValue_clone(MmsJournalEntry_getEntryID(lastJe));
            lastTimestamp = MmsValue_getUtcTimeInMs(MmsJournalEntry_getOccurenceTime(lastJe));
        }

        LinkedList_destroyDeep(entries, (LinkedListValueDeleteFunction)MmsJournalEntry_destroy);
    }

    if (lastEntry) MmsValue_delete(lastEntry);
    MmsValue_delete(lcbValue);

    char status[128];
    snprintf(status, sizeof(status), "Получено записей: %d, запросов: %d", totalEntries, queryCount);
    if (g_log_status_callback) g_log_status_callback(status);
    client_log("Запрос завершён: %s", status);
}

void client_query_logs_by_time(const char* logRef, uint64_t startTime, uint64_t endTime) {
    (void)startTime; (void)endTime;
    client_log("Фильтр по времени (клиентская сторона)");
    client_query_all_logs(logRef);
}

void client_query_logs_by_entry_id(const char* logRef, int startId, int endId) {
    (void)startId; (void)endId;
    client_log("Фильтр по EntryID (клиентская сторона)");
    client_query_all_logs(logRef);
}

// Запрос модели
void client_refresh_model(void) {
    if (!g_con) {
        client_log("Нет подключения");
        return;
    }
    pthread_t tid;
    pthread_create(&tid, NULL, model_thread_func, NULL);
    pthread_detach(tid);
}

// Поток подключения
static void* client_thread_func(void* arg) {
    const char** params = (const char**)arg;
    const char* host = params[0];
    int port = atoi(params[1]);
    IedClientError error;
    g_con = IedConnection_create();
    client_log("Подключение к %s:%d...", host, port);
        if (g_connection_status_callback) {
g_connection_status_callback(true); 
    }
    if (g_password[0] != '\0') {
    MmsConnection mmsConnection = IedConnection_getMmsConnection(g_con);
    IsoConnectionParameters parameters = MmsConnection_getIsoConnectionParameters(mmsConnection);
    AcseAuthenticationParameter auth = AcseAuthenticationParameter_create();
    AcseAuthenticationParameter_setAuthMechanism(auth, ACSE_AUTH_PASSWORD);
    AcseAuthenticationParameter_setPassword(auth, g_password);
    IsoConnectionParameters_setAcseAuthenticationParameter(parameters, auth);
}
    printf("DEBUG: Sending password: '%s'\n", g_password);
    IedConnection_connect(g_con, &error, host, port);
    if (error != IED_ERROR_OK) {
        client_log("Ошибка подключения: %d", error);
        IedConnection_destroy(g_con);
        g_con = NULL;
        g_running = 0;
            if (g_connection_status_callback) {
g_connection_status_callback(true); 
    }
        return NULL;
    }
    client_log("Подключено к %s:%d", host, port);
    client_detect_logs();
        while (g_running) {
        Thread_sleep(1000);
    }
    client_log("Отключение...");
    

    if (g_connection_status_callback && g_con && IedConnection_getState(g_con) == IED_STATE_CONNECTED) {
g_connection_status_callback(true); 
    }
    
    IedConnection_close(g_con);
    IedConnection_destroy(g_con);
    g_con = NULL;

    if (g_auth) {
        AcseAuthenticationParameter_destroy(g_auth);
        g_auth = NULL;
    }
    return NULL;
}

int client_connect(const char* hostname, int port) {
    if (g_running) return -1;
    g_running = 1;
    static char host_buf[256];
    static char port_buf[16];
    static const char* params[2];
    strncpy(host_buf, hostname, sizeof(host_buf)-1);
    snprintf(port_buf, sizeof(port_buf), "%d", port);
    params[0] = host_buf;
    params[1] = port_buf;
    pthread_create(&g_client_thread, NULL, client_thread_func, (void*)params);
    return 0;
}

void client_disconnect(void) {
    if (!g_running) return;
    g_running = 0;
    if (g_client_thread) {
#ifdef __linux__
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 2;
        pthread_timedjoin_np(g_client_thread, NULL, &timeout);
#else
        pthread_join(g_client_thread, NULL);
#endif
        g_client_thread = 0;
    }
}
