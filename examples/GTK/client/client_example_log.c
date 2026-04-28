
#include "iec61850_client.h"
#include "hal_thread.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>


 // Функции для просмотра модели данных 


void printSpaces(int spaces)
{
    int i;
    for (i = 0; i < spaces; i++)
        printf(" ");
}

void printDataDirectory(char* doRef, IedConnection con, int spaces)
{
    IedClientError error;

    LinkedList dataAttributes = IedConnection_getDataDirectory(con, &error, doRef);

    if (dataAttributes != NULL) {
        LinkedList dataAttribute = LinkedList_getNext(dataAttributes);

        while (dataAttribute != NULL) {
            char* daName = (char*) dataAttribute->data;

            printSpaces(spaces);
            printf("ДА: %s\n", daName);

            dataAttribute = LinkedList_getNext(dataAttribute);

            char daRef[130];
            sprintf(daRef, "%s.%s", doRef, daName);

            printDataDirectory(daRef, con, spaces + 2);
        }
    }

    LinkedList_destroy(dataAttributes);
}


static void printJournalEntries(LinkedList journalEntries, int* totalCount)
{
    char buf[1024];
    LinkedList journalEntriesElem = LinkedList_getNext(journalEntries);

    while (journalEntriesElem != NULL)
    {
        MmsJournalEntry journalEntry = (MmsJournalEntry) LinkedList_getData(journalEntriesElem);

        (*totalCount)++;
        printf("EntryID: %012d\n", *totalCount);
        MmsValue_printToBuffer(MmsJournalEntry_getOccurenceTime(journalEntry), buf, 1024);
        printf("  время события: %s\n", buf);
        
        LinkedList journalVariableElem = LinkedList_getNext(journalEntry->journalVariables);
        while (journalVariableElem != NULL)
        {
            MmsJournalVariable journalVariable = (MmsJournalVariable) LinkedList_getData(journalVariableElem);

            printf("   тег переменной: %s\n", MmsJournalVariable_getTag(journalVariable));
            MmsValue* value = MmsJournalVariable_getValue(journalVariable);
            if (value != NULL) {
                if (MmsValue_getType(value) == MMS_INTEGER) {
                    printf("   значение: %d\n", MmsValue_toInt32(value));
                } else if (MmsValue_getType(value) == MMS_UNSIGNED) {
                    printf("   значение: %u\n", MmsValue_toUint32(value));
                } else if (MmsValue_getType(value) == MMS_FLOAT) {
                    printf("   значение: %f\n", MmsValue_toFloat(value));
                } else if (MmsValue_getType(value) == MMS_BOOLEAN) {
                    printf("   значение: %s\n", MmsValue_getBoolean(value) ? "true" : "false");
                } else {
                    MmsValue_printToBuffer(value, buf, 1024);
                    printf("   значение: %s\n", buf);
                }
            }
            journalVariableElem = LinkedList_getNext(journalVariableElem);
        }
        printf("\n");
        journalEntriesElem = LinkedList_getNext(journalEntriesElem);
    }
}



int main(int argc, char** argv)
{
    char* hostname;
    int tcpPort = 102;


    if (argc > 1)
        hostname = argv[1];
    else
        hostname = "localhost";

    if (argc > 2)
        tcpPort = atoi(argv[2]);

    IedClientError error;
    IedConnection con = IedConnection_create();


    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error != IED_ERROR_OK) {
        printf("Не удалось подключиться к %s:%i\n", hostname, tcpPort);
        IedConnection_destroy(con);
        return 1;
    }

    printf("Подключено к серверу %s:%i\n", hostname, tcpPort);


    printf("\n--- Начало просмотра модели данных ---\n");
    printf("Получение списка логических устройств...\n");
    
    LinkedList deviceList = IedConnection_getLogicalDeviceList(con, &error);

    if (error != IED_ERROR_OK) {
        printf("Не удалось прочитать список устройств (код ошибки: %i)\n", error);
        goto cleanup_and_exit;
    }

    LinkedList device = LinkedList_getNext(deviceList);

    while (device != NULL) {
        printf("ЛУ (LD): %s\n", (char*) device->data);

        LinkedList logicalNodes = IedConnection_getLogicalDeviceDirectory(con, &error,
                (char*) device->data);

        LinkedList logicalNode = LinkedList_getNext(logicalNodes);

        while (logicalNode != NULL) {
            printf("  ЛУ (LN): %s\n", (char*) logicalNode->data);

            char lnRef[129];
            sprintf(lnRef, "%s/%s", (char*) device->data, (char*) logicalNode->data);


            LinkedList dataObjects = IedConnection_getLogicalNodeDirectory(con, &error,
                    lnRef, ACSI_CLASS_DATA_OBJECT);

            LinkedList dataObject = LinkedList_getNext(dataObjects);

            while (dataObject != NULL) {
                 char* dataObjectName = (char*) dataObject->data;

                printf("    ДО (DO): %s\n", dataObjectName);

                dataObject = LinkedList_getNext(dataObject);

                char doRef[129];
                sprintf(doRef, "%s/%s.%s", (char*) device->data, (char*) logicalNode->data, dataObjectName);


                printDataDirectory(doRef, con, 6);
            }

            LinkedList_destroy(dataObjects);


            LinkedList dataSets = IedConnection_getLogicalNodeDirectory(con, &error, lnRef,
                    ACSI_CLASS_DATA_SET);

            LinkedList dataSet = LinkedList_getNext(dataSets);

            while (dataSet != NULL) {
                char * dataSetName = (char*) dataSet->data;
                bool isDeletable;
                char dataSetRef[130];
                sprintf(dataSetRef, "%s.%s", lnRef, dataSetName);

                LinkedList dataSetMembers = IedConnection_getDataSetDirectory(con, &error, dataSetRef,
                         &isDeletable);

                if (isDeletable)
                    printf("    Набор данных: %s (удаляемый)\n", dataSetName);
                else
                    printf("    Набор данных: %s (не удаляемый)\n", dataSetName);

                LinkedList dataSetMemberRef = LinkedList_getNext(dataSetMembers);

                while (dataSetMemberRef != NULL) {
                    char* memberRef = (char*) dataSetMemberRef->data;
                    printf("      %s\n", memberRef);
                    dataSetMemberRef = LinkedList_getNext(dataSetMemberRef);
                }

                LinkedList_destroy(dataSetMembers);
                dataSet = LinkedList_getNext(dataSet);
            }

            LinkedList_destroy(dataSets);


            LinkedList reports = IedConnection_getLogicalNodeDirectory(con, &error, lnRef,
                    ACSI_CLASS_URCB);

            LinkedList report = LinkedList_getNext(reports);

            while (report != NULL) {
                char* reportName = (char*) report->data;
                printf("    Отчет (URCB): %s\n", reportName);
                report = LinkedList_getNext(report);
            }

            LinkedList_destroy(reports);


            reports = IedConnection_getLogicalNodeDirectory(con, &error, lnRef,
                    ACSI_CLASS_BRCB);

            report = LinkedList_getNext(reports);

            while (report != NULL) {
                char* reportName = (char*) report->data;
                printf("    Отчет (BRCB): %s\n", reportName);
                report = LinkedList_getNext(report);
            }

            LinkedList_destroy(reports);

            logicalNode = LinkedList_getNext(logicalNode);
        }

        LinkedList_destroy(logicalNodes);
        device = LinkedList_getNext(device);
    }

    LinkedList_destroy(deviceList);
    printf("--- Конец просмотра модели данных ---\n\n");


    printf("--- Начало работы с журналами ---\n");


    char* logRef = "simpleIOGenericIO/LLN0$EventLog";
    char* logLnRef = "simpleIOGenericIO/LLN0";

    printf("Попытка получить доступные журналы...\n");
    LinkedList logs = IedConnection_getLogicalNodeDirectory(con, &error, logLnRef, ACSI_CLASS_LOG);

    if (error == IED_ERROR_OK) {
        if (LinkedList_size(logs) > 0) {
            printf("Найдены журналы в ЛУ %s:\n", logLnRef);

            LinkedList log = LinkedList_getNext(logs);
            while (log != NULL) {
                char* logName = (char*) LinkedList_getData(log);
                printf("  %s\n", logName);
                log = LinkedList_getNext(log);
            }
        } else {
            printf("Журналы не найдены\n");
        }
        LinkedList_destroy(logs);
    } else {
        printf("Не удалось получить каталог журналов (ошибка=%i)\n", error);
    }
    
    printf("Попытка включить журнал...\n");
    IedConnection_writeBooleanValue(con, &error, "simpleIOGenericIO/LLN0.EventLog.LogEna", IEC61850_FC_LG, true);
    if (error == IED_ERROR_OK)
    {
        printf("Журнал включен. Ожидание...\n");
        Thread_sleep(1000);
    }
    else
    {
        printf("Не удалось включить журнал (ошибка=%i)\n", error);
    }

    printf("Попытка прочитать LCB...\n");
    MmsValue* lcbValue = IedConnection_readObject(con, &error, "simpleIOGenericIO/LLN0.EventLog", IEC61850_FC_LG);
    
    if ((error == IED_ERROR_OK) && (lcbValue != NULL) && (MmsValue_getType(lcbValue) != MMS_DATA_ACCESS_ERROR))
    {
        char printBuf[1024];
        MmsValue_printToBuffer(lcbValue, printBuf, 1024);
        printf("Значения LCB: %s\n", printBuf);

        MmsValue* oldEntry = MmsValue_getElement(lcbValue, 5);
        MmsValue* oldEntryTm = MmsValue_getElement(lcbValue, 3); 

        uint64_t timestamp = 0;
        if (oldEntryTm != NULL) {
            timestamp = MmsValue_getUtcTimeInMs(oldEntryTm);
        }

        bool moreFollows = true;
        int totalEntries = 0;
        MmsValue* lastEntry = NULL;
        uint64_t lastTimestamp = 0;
        int queryCount = 0;

        printf("\nЗапрос записей журнала (все записи)...\n");

        while (moreFollows) {
            queryCount++;
            printf("\n--- Запрос #%d ---\n", queryCount);
            
            LinkedList logEntries = IedConnection_queryLogAfter(con, &error, logRef, 
                                                               (lastEntry != NULL) ? lastEntry : oldEntry, 
                                                                (lastEntry != NULL) ? lastTimestamp : timestamp, 
                                                                &moreFollows);

            if (error == IED_ERROR_OK)
            {
                int receivedCount = LinkedList_size(logEntries);
                printf("Получено %d записей журнала для %s (осталось: %s)\n", 
                       receivedCount, logRef, moreFollows ? "да" : "нет");

                if (receivedCount > 0) {
                    printJournalEntries(logEntries, &totalEntries);
                    
                    LinkedList lastElem = LinkedList_getLastElement(logEntries);
                    if (lastElem != NULL) {
                         MmsJournalEntry lastJournalEntry = (MmsJournalEntry) LinkedList_getData(lastElem);
                        
                        if (lastEntry != NULL) {
                             MmsValue_delete(lastEntry);
                        }
                        lastEntry = MmsValue_clone(MmsJournalEntry_getEntryID(lastJournalEntry));
                        lastTimestamp = MmsValue_getUtcTimeInMs(MmsJournalEntry_getOccurenceTime(lastJournalEntry));
                    }
                } else {
                    break;
                }
                
                LinkedList_destroyDeep(logEntries, (LinkedListValueDeleteFunction) MmsJournalEntry_destroy);
            }
            else {
                 printf("Ошибка запроса журнала (код ошибки: %i)\n", error);
                break;
            }
        }
        
        printf("\n========================================\n");
        printf("Всего получено записей: %d\n", totalEntries);
        printf("Всего выполнено запросов: %d\n", queryCount);
        printf("========================================\n");

        if (lastEntry != NULL) {
            MmsValue_delete(lastEntry);
        }
        MmsValue_delete(lcbValue);
    }
    else {
        printf("Ошибка чтения LCB! (ошибка=%i)\n", error);
    }

    IedConnection_close(con);

cleanup_and_exit:
    IedConnection_destroy(con);
    return 0;
}
