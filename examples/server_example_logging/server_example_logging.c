/*
 *  server_example_logging.c
 *
 *  - How to use a server with logging service
 *  - How to store arbitrary data in a log
 *
 */

#include "iec61850_server.h"
#include "hal_thread.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "static_model.h"

#include "logging_api.h"
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

/* Пароли */
static char* password1 = "user1@testpw";
static char* password2 = "user2@testpw";

/* Функция аутентификатор*/
static bool clientAuthenticator(void* parameter, AcseAuthenticationParameter authParameter, 
                                 void** securityToken, IsoApplicationReference* appRef)
{
    printf("ACSE Authenticator:\n");
    printf("  client ap-title: "); 
    for (int i = 0; i < appRef->apTitle.arcCount; i++)
        printf("%i%s", appRef->apTitle.arc[i], (i+1 < appRef->apTitle.arcCount) ? "." : "");
    printf("\n  ae-qualifier: %i\n", appRef->aeQualifier);

    if (authParameter->mechanism == ACSE_AUTH_PASSWORD) {
        if (authParameter->value.password.passwordLength == strlen(password1) &&
            memcmp(authParameter->value.password.octetString, password1, strlen(password1)) == 0) {
            *securityToken = (void*) password1;
            return true;
        }
        if (authParameter->value.password.passwordLength == strlen(password2) &&
            memcmp(authParameter->value.password.octetString, password2, strlen(password2)) == 0) {
            *securityToken = (void*) password2;
            return true;
        }
    }
    return false;
}

/* Проверка прав на управление */
static CheckHandlerResult performCheckHandler(ControlAction action, void* parameter, MmsValue* ctlVal,
                                               bool test, bool interlockCheck, ClientConnection connection)
{
    void* securityToken = ClientConnection_getSecurityToken(connection);
    // Разрешаем управление толькос паролем password2
    if (securityToken == password2)
        return CONTROL_ACCEPTED;
    else
        return CONTROL_OBJECT_ACCESS_DENIED;
}

static int running = 0;
static IedServer iedServer = NULL;
static LogStorage statusLog = NULL;

void
sigint_handler(int signalId)
{
    running = 0;
}

static void addTestLogEntry(uint64_t timestamp, const char* dataRef, MmsValue* value, uint8_t reasonCode) {
    uint64_t entryID = LogStorage_addEntry(statusLog, timestamp);
    uint8_t blob[256];
    int blobSize;
    
    // Значение
    blobSize = MmsValue_encodeMmsData(value, blob, 0, true);
    LogStorage_addEntryData(statusLog, entryID, dataRef, blob, blobSize, reasonCode);
    
    // Временная метка
    MmsValue* timeValue = MmsValue_newUtcTimeByMsTime(timestamp);
    blobSize = MmsValue_encodeMmsData(timeValue, blob, 0, true);
    char timeRef[100];
    snprintf(timeRef, 100, "%s$t", dataRef);
    LogStorage_addEntryData(statusLog, entryID, timeRef, blob, blobSize, REASON_DATA_CHANGE);
    MmsValue_delete(timeValue);
    
    // Сохраняем ReasonCode
    MmsValue* rcValue = MmsValue_newIntegerFromInt32(reasonCode);
    blobSize = MmsValue_encodeMmsData(rcValue, blob, 0, true);
    LogStorage_addEntryData(statusLog, entryID, "ReasonCode", blob, blobSize, reasonCode);
    MmsValue_delete(rcValue);
    
    printf("Added log entry: %s (reasonCode=0x%02X)\n", dataRef, reasonCode);
}

static ControlHandlerResult
controlHandlerForBinaryOutput(ControlAction action, void* parameter, MmsValue* value, bool test)
{
    if (test) {
        printf("Received test command\n");
        return CONTROL_RESULT_FAILED;
    }

    if (MmsValue_getType(value) == MMS_BOOLEAN) {
        printf("received binary control command: ");

        if (MmsValue_getBoolean(value))
            printf("on\n");
        else
            printf("off\n");
    }
    else
        return CONTROL_RESULT_FAILED;

    uint64_t timeStamp = Hal_getTimeInMs();

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO1) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_t, timeStamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_stVal, value);
        
        addTestLogEntry(timeStamp, "GenericIO/GGIO1$ST$SPCSO1$stVal", value, REASON_DATA_CHANGE);
        printf("Added log entry for SPCSO1 change\n");
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO2) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_t, timeStamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2_stVal, value);
        
        addTestLogEntry(timeStamp, "GenericIO/GGIO1$ST$SPCSO2$stVal", value, REASON_DATA_CHANGE);
        printf("Added log entry for SPCSO2 change\n");
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO3) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_t, timeStamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3_stVal, value);
        
        addTestLogEntry(timeStamp, "GenericIO/GGIO1$ST$SPCSO3$stVal", value, REASON_DATA_CHANGE);
        printf("Added log entry for SPCSO3 change\n");
    }

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO4) {
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_t, timeStamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4_stVal, value);
        
        addTestLogEntry(timeStamp, "GenericIO/GGIO1$ST$SPCSO4$stVal", value, REASON_DATA_CHANGE);
        printf("Added log entry for SPCSO4 change\n");
    }

    return CONTROL_RESULT_OK;
}

static void
connectionHandler (IedServer self, ClientConnection connection, bool connected, void* parameter)
{
    if (connected)
        printf("Connection opened - client connected\n");
    else
        printf("Connection closed - client disconnected\n");
}

static bool
entryCallback(void* parameter, uint64_t timestamp, uint64_t entryID, bool moreFollow)
{
    if (moreFollow)
        printf("Log entry - ID: %llu, timestamp: %llu\n", 
               (unsigned long long)entryID, (unsigned long long)timestamp);
    return true;
}

static bool
entryDataCallback (void* parameter, const char* dataRef, const uint8_t* data, int dataSize, uint8_t reasonCode, bool moreFollow)
{
    if (moreFollow) {
        printf("  Log data - ref: %s, reason: %d\n", dataRef, reasonCode);
    }
    return true;
}

int
main(int argc, char** argv)
{
    int tcpPort = 102;

    if (argc > 1) {
        tcpPort = atoi(argv[1]);
    }

    printf("Using libIEC61850 version %s\n", LibIEC61850_getVersionString());
    printf("Starting IEC 61850 server on port %d\n", tcpPort);
    printf("Press Ctrl+C to stop the server\n");

    iedServer = IedServer_create(&iedModel);

    if (iedServer == NULL) {
        printf("Failed to create IED server!\n");
        return -1;
    }

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO1);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO2);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO3);

    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4,
            (ControlHandler) controlHandlerForBinaryOutput,
            IEDMODEL_GenericIO_GGIO1_SPCSO4);

    IedServer_setConnectionIndicationHandler(iedServer, (IedConnectionIndicationHandler) connectionHandler, NULL);

    statusLog = SqliteLogStorage_createInstance("log_status.db");

    if (statusLog == NULL) {
        printf("Failed to create log storage!\n");
        IedServer_destroy(iedServer);
        return -1;
    }

    LogStorage_setMaxLogEntries(statusLog, 1000); 

    IedServer_setLogStorage(iedServer, "GenericIO/LLN0$EventLog", statusLog);
    printf("Log storage configured for: GenericIO/LLN0$EventLog\n");


    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_LLN0_Health_stVal, true);
    printf("Logging enabled by default\n");

    uint64_t initialTime = Hal_getTimeInMs();
    

    MmsValue* initValue = MmsValue_newBoolean(false);
    addTestLogEntry(initialTime, "GenericIO/GGIO1$ST$SPCSO1$stVal", initValue, REASON_DATA_CHANGE);
    MmsValue_delete(initValue);
    
    initValue = MmsValue_newBoolean(false);
    addTestLogEntry(initialTime + 1000, "GenericIO/GGIO1$ST$SPCSO2$stVal", initValue, REASON_DATA_CHANGE);
    MmsValue_delete(initValue);
    
    initValue = MmsValue_newFloat(3.14f);
    addTestLogEntry(initialTime + 2000, "GenericIO/GGIO1$MX$AnIn1$mag$f", initValue, REASON_DATA_CHANGE);
    MmsValue_delete(initValue);


    /* Включаем аутентификацию */
    IedServer_setAuthenticator(iedServer, clientAuthenticator, NULL);

    /*Ограничиваем доступ к управлению */
    IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1,
            (ControlPerformCheckHandler) performCheckHandler, NULL);
    IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO2,
            (ControlPerformCheckHandler) performCheckHandler, NULL);
    IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO3,
            (ControlPerformCheckHandler) performCheckHandler, NULL);
    IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO4,
            (ControlPerformCheckHandler) performCheckHandler, NULL);

    IedServer_start(iedServer, tcpPort);

    if (!IedServer_isRunning(iedServer)) {
        printf("Starting server failed! Exit.\n");
        IedServer_destroy(iedServer);
        LogStorage_destroy(statusLog);
        return -1;
    }

    printf("Server is running and waiting for connections...\n");
    printf("Available data points:\n");
    printf("  - GenericIO/GGIO1.SPCSO1 (binary output)\n");
    printf("  - GenericIO/GGIO1.SPCSO2 (binary output)\n");
    printf("  - GenericIO/GGIO1.SPCSO3 (binary output)\n");
    printf("  - GenericIO/GGIO1.SPCSO4 (binary output)\n");
    printf("  - GenericIO/GGIO1.AnIn1 (analog input)\n");
    printf("  - GenericIO/GGIO1.AnIn2 (analog input)\n");
    printf("  - GenericIO/GGIO1.AnIn3 (analog input)\n");
    printf("  - GenericIO/GGIO1.AnIn4 (analog input)\n");
    printf("Log: GenericIO/LLN0.EventLog\n");

    running = 1;
    signal(SIGINT, sigint_handler);

    float t = 0.f;
    int updateCounter = 0;
    int logCounter = 0;

    int qualityChangeCounter = 0;
    int integrityCounter = 0;
    int giCounter = 0;
    int appTriggerCounter = 0;

            while (running) {
        uint64_t timestamp = Hal_getTimeInMs();

        t += 0.1f;

        float an1 = sinf(t);
        float an2 = sinf(t + 1.f);
        float an3 = sinf(t + 2.f);
        float an4 = sinf(t + 3.f);

        IedServer_lockDataModel(iedServer);

        Timestamp iecTimestamp;
        Timestamp_clearFlags(&iecTimestamp);
        Timestamp_setTimeInMilliseconds(&iecTimestamp, timestamp);
        Timestamp_setLeapSecondKnown(&iecTimestamp, true);

        if (((int) t % 2) == 0)
            Timestamp_setClockNotSynchronized(&iecTimestamp, true);

        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn1_t, &iecTimestamp);
        IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn1_mag_f, an1);

        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn2_t, &iecTimestamp);
        IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn2_mag_f, an2);

        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn3_t, &iecTimestamp);
        IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn3_mag_f, an3);

        IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn4_t, &iecTimestamp);
        IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn4_mag_f, an4);

        IedServer_unlockDataModel(iedServer);

        // только лог, без обновления реального качества
        qualityChangeCounter++;
        if (qualityChangeCounter >= 30) {
            qualityChangeCounter = 0;
            MmsValue* qVal = MmsValue_newIntegerFromInt32(0x03);
            addTestLogEntry(timestamp, "GenericIO/GGIO1$MX$AnIn1$q", qVal, REASON_QUALITY_CHANGE);
            MmsValue_delete(qVal);
            printf("Generated quality change log for AnIn1\n");
        }

        // Периодический отчетintegrity
        integrityCounter++;
        if (integrityCounter >= 60) {
            integrityCounter = 0;
            MmsValue* intVal = MmsValue_newFloat(an1);
            addTestLogEntry(timestamp, "GenericIO/GGIO1$MX$AnIn1$mag$f", intVal, REASON_INTEGRITY);
            MmsValue_delete(intVal);
            printf("Generated integrity log for AnIn1\n");
        }

        // Общий опрос
        giCounter++;
        if (giCounter >= 45) {
            giCounter = 0;
            // Меняем состояние Ind1 
            bool current = IedServer_getBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind1_stVal);
            IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind1_stVal, !current);
            MmsValue* giVal = MmsValue_newBoolean(!current);
            addTestLogEntry(timestamp, "GenericIO/GGIO1$ST$Ind1$stVal", giVal, REASON_GI);
            MmsValue_delete(giVal);
            printf("Generated GI log for Ind1\n");
        }

        // Приложение application trigger
        appTriggerCounter++;
        if (appTriggerCounter >= 20) {
            appTriggerCounter = 0;
            bool current = IedServer_getBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind2_stVal);
            IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind2_stVal, !current);
            MmsValue* appVal = MmsValue_newBoolean(!current);
            addTestLogEntry(timestamp, "GenericIO/GGIO1$ST$Ind2$stVal", appVal, REASON_APPLICATION_TRIGGER);
            MmsValue_delete(appVal);
            printf("Generated application trigger log for Ind2\n");
        }

        //Обновление данных data-update
        if (updateCounter % 50 == 0) {
            MmsValue* logValue = MmsValue_newFloat(an1);
            addTestLogEntry(timestamp, "GenericIO/GGIO1$MX$AnIn1$mag$f", logValue, REASON_DATA_UPDATE);
            MmsValue_delete(logValue);
            
            logValue = MmsValue_newFloat(an2);
            addTestLogEntry(timestamp + 100, "GenericIO/GGIO1$MX$AnIn2$mag$f", logValue, REASON_DATA_UPDATE);
            MmsValue_delete(logValue);
            
            logCounter++;
            printf("Periodic log update #%d - AnIn1: %.3f, AnIn2: %.3f\n", logCounter, an1, an2);
        }

        Thread_sleep(100);
        updateCounter++;
        
        if (updateCounter % 300 == 0) {
            printf("Server running - %d updates, %d log entries added\n", updateCounter, logCounter);
        }
    }

    printf("\nStopping server...\n");
    
    IedServer_stop(iedServer);

    IedServer_destroy(iedServer);

    LogStorage_destroy(statusLog);
    
    printf("Server stopped successfully\n");
    return 0;
}
