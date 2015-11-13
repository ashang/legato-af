/** @file appCtrl.c
 *
 * Control Legato applications.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "interfaces.h"
#include "limit.h"
#include "fileDescriptor.h"
#include "user.h"
#include "cgroups.h"

/// @todo Use the appCfg component instead of reading from the config directly.


/// Pointer to application name argument from command line.
static const char* AppNamePtr = NULL;


/// Address of the command function to be executed.
static void (*CommandFunc)(void);


//--------------------------------------------------------------------------------------------------
/**
 * The location where all applications are installed.
 */
//--------------------------------------------------------------------------------------------------
#define APPS_INSTALL_DIR                                "/opt/legato/apps"


//--------------------------------------------------------------------------------------------------
/**
 * The application's info file.
 */
//--------------------------------------------------------------------------------------------------
#define APP_INFO_FILE                                   "info.properties"


//--------------------------------------------------------------------------------------------------
/**
 * Prints a generic message on stderr so that the user is aware there is a problem, logs the
 * internal error message and exit.
 */
//--------------------------------------------------------------------------------------------------
#define INTERNAL_ERR(formatString, ...)                                                 \
            { fprintf(stderr, "Internal error check logs for details.\n");              \
              LE_FATAL(formatString, ##__VA_ARGS__); }


//--------------------------------------------------------------------------------------------------
/**
 * If the condition is true, print a generic message on stderr so that the user is aware there is a
 * problem, log the internal error message and exit.
 */
//--------------------------------------------------------------------------------------------------
#define INTERNAL_ERR_IF(condition, formatString, ...)                                   \
        if (condition) { INTERNAL_ERR(formatString, ##__VA_ARGS__); }


//--------------------------------------------------------------------------------------------------
/**
 * Type for functions that prints some information for an application.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*PrintAppFunc_t)(const char* appNamePtr);


//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of threads to display.
 */
//--------------------------------------------------------------------------------------------------
#define MAX_NUM_THREADS_TO_DISPLAY             100


//--------------------------------------------------------------------------------------------------
/**
 * Estimated maximum number of processes per app.
 */
//--------------------------------------------------------------------------------------------------
#define EST_MAX_NUM_PROC                        29


//--------------------------------------------------------------------------------------------------
/**
 * Process object used to store process information.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    char procName[LIMIT_MAX_PATH_BYTES];    // The name of the process.
    pid_t procID;                           // The process ID.
    le_sls_List_t threadList;               // The list of thread in this process.
}
ProcObj_t;


//--------------------------------------------------------------------------------------------------
/**
 * Thread object.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    pid_t threadID;                         // The thread ID.
    le_sls_Link_t link;                     // The link in the process's list of threads.
}
ThreadObj_t;


//--------------------------------------------------------------------------------------------------
/**
 * Pool of process objects
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t ProcObjPool;


//--------------------------------------------------------------------------------------------------
/**
 * Pool of thread objects
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t ThreadObjPool;


//--------------------------------------------------------------------------------------------------
/**
 * Hash map of processes in the current app.
 */
//--------------------------------------------------------------------------------------------------
static le_hashmap_Ref_t ProcObjMap;


//--------------------------------------------------------------------------------------------------
/**
 * Prints help to stdout and exits.
 */
//--------------------------------------------------------------------------------------------------
static void PrintHelp
(
    void
)
{
    puts(
        "NAME:\n"
        "    appCtrl - Used to start, stop and get the status of Legato applications.\n"
        "\n"
        "SYNOPSIS:\n"
        "    appCtrl --help\n"
        "    appCtrl start APP_NAME\n"
        "    appCtrl stop APP_NAME\n"
        "    appCtrl stopLegato\n"
        "    appCtrl list\n"
        "    appCtrl status [APP_NAME]\n"
        "    appCtrl version APP_NAME\n"
        "    appCtrl info [APP_NAME]\n"
        "\n"
        "DESCRIPTION:\n"
        "    appCtrl --help\n"
        "       Display this help and exit.\n"
        "\n"
        "    appCtrl start APP_NAME\n"
        "       Starts the specified application.\n"
        "\n"
        "    appCtrl stop APP_NAME\n"
        "       Stops the specified application.\n"
        "\n"
        "    appCtrl stopLegato\n"
        "       Stops the Legato framework.\n"
        "\n"
        "    appCtrl list\n"
        "       List all installed applications.\n"
        "\n"
        "    appCtrl status [APP_NAME]\n"
        "       If no name is given, prints the status of all installed applications.\n"
        "       If a name is given, prints the status of the specified application.\n"
        "       The status of the application can be 'stopped', 'running', 'paused' or 'not installed'.\n"
        "\n"
        "    appCtrl version APP_NAME\n"
        "       Prints the version of the specified application.\n"
        "\n"
        "    appCtrl info [APP_NAME]\n"
        "       If no name is given, prints the information of all installed applications.\n"
        "       If a name is given, prints the information of the specified application.\n"
        );

    exit(EXIT_SUCCESS);
}


//--------------------------------------------------------------------------------------------------
/**
 * Requests the Supervisor to start an application.
 *
 * @note This function does not return.
 */
//--------------------------------------------------------------------------------------------------
static void StartApp
(
    void
)
{
    le_sup_ctrl_ConnectService();

    // Start the application.
    switch (le_sup_ctrl_StartApp(AppNamePtr))
    {
        case LE_OK:
            exit(EXIT_SUCCESS);

        case LE_DUPLICATE:
            fprintf(stderr, "Application '%s' is already running.\n", AppNamePtr);
            exit(EXIT_FAILURE);

        case LE_NOT_FOUND:
            fprintf(stderr, "Application '%s' is not installed.\n", AppNamePtr);
            exit(EXIT_FAILURE);

        default:
            fprintf(stderr, "There was an error.  Application '%s' could not be started.\n", AppNamePtr);
            exit(EXIT_FAILURE);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Requests the Supervisor to stop an application.
 *
 * @note This function does not return.
 */
//--------------------------------------------------------------------------------------------------
static void StopApp
(
    void
)
{
    le_sup_ctrl_ConnectService();

    // Stop the application.
    switch (le_sup_ctrl_StopApp(AppNamePtr))
    {
        case LE_OK:
            exit(EXIT_SUCCESS);

        case LE_NOT_FOUND:
            printf("Application '%s' was not running.\n", AppNamePtr);
            exit(EXIT_FAILURE);

        default:
            INTERNAL_ERR("Unexpected response from the Supervisor.");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Requests the Supervisor to stop the Legato framework.
 *
 * @note This function does not return.
 */
//--------------------------------------------------------------------------------------------------
static void StopLegato
(
    void
)
{
    le_sup_ctrl_ConnectService();

    // Stop the framework.
    le_result_t result = le_sup_ctrl_StopLegato();
    switch (result)
    {
        case LE_OK:
            exit(EXIT_SUCCESS);

        case LE_NOT_FOUND:
            printf("Legato is being stopped by someone else.\n");
            exit(EXIT_SUCCESS);

        default:
            INTERNAL_ERR("Unexpected response, %d, from the Supervisor.", result);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Prints the list of installed apps.
 *
 * Iterate over the list of apps and calls the specified printFunc for each app.  If the printFunc
 * is NULL then the name of the app is printed.
 */
//--------------------------------------------------------------------------------------------------
static void ListInstalledApps
(
    PrintAppFunc_t printFunc            ///< [IN] Function to use for printing app information.
)
{
    le_cfg_ConnectService();

    if (printFunc != NULL)
    {
        le_appInfo_ConnectService();
    }

    le_cfg_IteratorRef_t cfgIter = le_cfg_CreateReadTxn("/apps");

    if (le_cfg_GoToFirstChild(cfgIter) == LE_NOT_FOUND)
    {
        LE_DEBUG("There are no installed apps.");
        exit(EXIT_SUCCESS);
    }

    // Iterate over the list of apps.
    do
    {
        char appName[LIMIT_MAX_APP_NAME_BYTES];

        INTERNAL_ERR_IF(le_cfg_GetNodeName(cfgIter, "", appName, sizeof(appName)) != LE_OK,
                        "Application name in config is too long.");

        if (printFunc == NULL)
        {
            printf("%s\n", appName);
        }
        else
        {
            printFunc(appName);
        }
    }
    while (le_cfg_GoToNextSibling(cfgIter) == LE_OK);
}


//--------------------------------------------------------------------------------------------------
/**
 * Prints an installed application's state.
 */
//--------------------------------------------------------------------------------------------------
static const char* GetAppState
(
    const char* appNamePtr          ///< [IN] App name to get the state for.
)
{
    le_appInfo_State_t appState;

    appState = le_appInfo_GetState(appNamePtr);

    switch (appState)
    {
        case LE_APPINFO_STOPPED:
            return "stopped";

        case LE_APPINFO_RUNNING:
            return "running";

        default:
            INTERNAL_ERR("Supervisor returned an unknown state for app '%s'.", appNamePtr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Prints the application status.
 */
//--------------------------------------------------------------------------------------------------
static void PrintAppState
(
    const char* appNamePtr      ///< [IN] Application name to get the status for.
)
{
    le_appInfo_ConnectService();
    le_cfg_ConnectService();

    le_cfg_IteratorRef_t cfgIter = le_cfg_CreateReadTxn("/apps");

    if (!le_cfg_NodeExists(cfgIter, appNamePtr))
    {
        printf("[not installed] %s\n", appNamePtr);
    }
    else
    {
        printf("[%s] %s\n", GetAppState(appNamePtr), appNamePtr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Implements the "status" command.
 *
 * @note This function does not return.
 **/
//--------------------------------------------------------------------------------------------------
static void PrintStatus
(
    void
)
{
    if (AppNamePtr == NULL)
    {
        ListInstalledApps(PrintAppState);
    }
    else
    {
        PrintAppState(AppNamePtr);
    }

    exit(EXIT_SUCCESS);
}


//--------------------------------------------------------------------------------------------------
/**
 * Parses a line of the APP_INFO_FILE for display.
 *
 * @todo This is currently a dumb parse of the line string that just replaces the '=' with ': '.
 *
 * @note This function uses a static variable so is not re-entrant.
 *
 * @return
 *      Pointer to the parsed string.
 */
//--------------------------------------------------------------------------------------------------
static const char* ParsedInfoLine
(
    const char* linePtr             ///< [IN] The original line from the file.
)
{
    static char parsedStr[LIMIT_MAX_PATH_BYTES];

    size_t lineIndex = 0;
    size_t parsedIndex = 0;

    while (linePtr[lineIndex] != '\0')
    {
        if (linePtr[lineIndex] == '=')
        {
            LE_FATAL_IF((parsedIndex + 2) >= sizeof(parsedStr),
                        "'%s' is too long for buffer.", linePtr);

            parsedStr[parsedIndex++] = ':';
            parsedStr[parsedIndex++] = ' ';
        }
        else
        {
            LE_FATAL_IF((parsedIndex + 1) >= sizeof(parsedStr),
                        "'%s' is too long for buffer.", linePtr);

            parsedStr[parsedIndex++] = linePtr[lineIndex];
        }

        lineIndex++;
    }

    // Terminate the string.
    parsedStr[parsedIndex++] = '\0';

    return parsedStr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Prints the information in the APP_INFO_FILE file.
 */
//--------------------------------------------------------------------------------------------------
static void PrintedAppInfoFile
(
    const char* appNamePtr,         ///< [IN] App name.
    const char* prefixPtr           ///< [IN] Prefix to use when printing info lines.  This
                                    ///       prefix can be used to indent the info lines.
)
{
    // Get the path to the app's info file.
    char infoFilePath[LIMIT_MAX_PATH_BYTES] = APPS_INSTALL_DIR;
    INTERNAL_ERR_IF(le_path_Concat("/",
                                   infoFilePath,
                                   sizeof(infoFilePath),
                                   appNamePtr,
                                   APP_INFO_FILE,
                                   NULL) != LE_OK,
                    "Path to app %s's %s is too long.", appNamePtr, APP_INFO_FILE);



    // Open the info file.
    int fd;

    do
    {
        fd = open(infoFilePath, O_RDONLY);
    }
    while ( (fd == -1) && (errno == EINTR) );

    if (fd == -1)
    {
        if (errno == ENOENT)
        {
            LE_WARN("No %s file for app %s.", infoFilePath, appNamePtr);
            return;
        }
        else
        {
            INTERNAL_ERR("Could not open file %s.  %m.", infoFilePath);
        }
    }

    // Read the file a line at a time and print.
    char infoLine[LIMIT_MAX_PATH_BYTES];

    while (1)
    {
        le_result_t result = fd_ReadLine(fd, infoLine, sizeof(infoLine));

        if (result == LE_OK)
        {
            // Parse the info line and print.
            printf("%s%s\n", prefixPtr, ParsedInfoLine(infoLine));
        }
        else if (result == LE_OUT_OF_RANGE)
        {
            // Nothing else to read.
            break;
        }
        else if (result == LE_OVERFLOW)
        {
            INTERNAL_ERR("Line '%s...' in file %s is too long.", infoLine, infoFilePath);
        }
        else
        {
            INTERNAL_ERR("Error reading file %s.", infoFilePath);
        }
    }

    fd_Close(fd);
}


//--------------------------------------------------------------------------------------------------
/**
 * Deletes a process object.
 */
//--------------------------------------------------------------------------------------------------
static void DeleteProcObj
(
    ProcObj_t* procObjPtr
)
{
    // Iterate over the list of threads and free them first
    le_sls_Link_t* threadLinkPtr = le_sls_Pop(&(procObjPtr->threadList));

    while (threadLinkPtr != NULL)
    {
        ThreadObj_t* threadPtr = CONTAINER_OF(threadLinkPtr, ThreadObj_t, link);

        le_mem_Release(threadPtr);

        threadLinkPtr = le_sls_Pop(&(procObjPtr->threadList));
    }

    // Release the proc object.
    le_mem_Release(procObjPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Deletes all of the process objects from a hash map and frees its memory.
 */
//--------------------------------------------------------------------------------------------------
static void ClearProcHashmap
(
    le_hashmap_Ref_t procsMap
)
{
    // Iterate over the hashmap.
    le_hashmap_It_Ref_t iter = le_hashmap_GetIterator(procsMap);

    while (le_hashmap_NextNode(iter) == LE_OK)
    {
        ProcObj_t* procObjPtr = (ProcObj_t*)le_hashmap_GetValue(iter);
        le_hashmap_Remove(procsMap, le_hashmap_GetKey(iter));

        DeleteProcObj(procObjPtr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the PID of the process this thread belongs to.
 *
 * @return
 *      PID of the process if successful.
 *      LE_NOT_FOUND if the thread could not be found.
 */
//--------------------------------------------------------------------------------------------------
static pid_t GetThreadsProcID
(
    pid_t tid           ///< [IN] Thread ID.
)
{
#define TGID_STR            "Tgid:"

    // Get the proc file name.
    char procFile[LIMIT_MAX_PATH_BYTES];
    INTERNAL_ERR_IF(snprintf(procFile, sizeof(procFile), "/proc/%d/status", tid) >= sizeof(procFile),
                    "File name '%s...' size is too long.", procFile);

    // Open the proc file.
    int fd;

    do
    {
        fd = open(procFile, O_RDONLY);
    }
    while ( (fd == -1) && (errno == EINTR) );

    if ( (fd == -1) && (errno == ENOENT ) )
    {
        return LE_NOT_FOUND;
    }

    INTERNAL_ERR_IF(fd == -1, "Could not read file %s.  %m.", procFile);

    // Read the Tgid from the file.
    while (1)
    {
        char str[200];
        le_result_t result = fd_ReadLine(fd, str, sizeof(str));

        INTERNAL_ERR_IF(result == LE_OVERFLOW, "Buffer to read PID is too small.");

        if (result == LE_OK)
        {
            if (strstr(str, TGID_STR) == str)
            {
                // Convert the Tgid string to a pid.
                char* pidStrPtr = &(str[sizeof(TGID_STR)-1]);
                pid_t pid;

                INTERNAL_ERR_IF(le_utf8_ParseInt(&pid, pidStrPtr) != LE_OK,
                                "Could not convert %s to a pid.", pidStrPtr);

                fd_Close(fd);

                return pid;
            }
        }
        else
        {
            INTERNAL_ERR("Error reading the %s", procFile);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets the process name.
 *
 * @return
 *      LE_OK if successful.
 *      LE_NOT_FOUND if the process could not be found.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GetProcName
(
    pid_t pid,              ///< [IN] The process ID.
    char* bufPtr,           ///< [OUT] Buffer to store the process name.
    size_t bufSize          ///< [IN] Buffer size.
)
{
    // Get the proc file name.
    char procFile[LIMIT_MAX_PATH_BYTES];
    INTERNAL_ERR_IF(snprintf(procFile, sizeof(procFile), "/proc/%d/cmdline", pid) >= sizeof(procFile),
                    "File name '%s...' size is too long.", procFile);

    // Open the proc file.
    int fd;

    do
    {
        fd = open(procFile, O_RDONLY);
    }
    while ( (fd == -1) && (errno == EINTR) );

    if ( (fd == -1) && (errno == ENOENT ) )
    {
        return LE_NOT_FOUND;
    }

    INTERNAL_ERR_IF(fd == -1, "Could not read file %s.  %m.", procFile);

    // Read the name from the file.
    le_result_t result = fd_ReadLine(fd, bufPtr, bufSize);

    INTERNAL_ERR_IF(result == LE_FAULT, "Error reading the %s", procFile);

    fd_Close(fd);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Builds a process object for the specified tid and puts the object in the the specified hasmap.
 */
//--------------------------------------------------------------------------------------------------
static void BuildProcObjs
(
    le_hashmap_Ref_t procsMap,
    pid_t tid
)
{
    // Get the PID of the process this thread belongs to.
    pid_t pid = GetThreadsProcID(tid);

    if (pid == LE_NOT_FOUND)
    {
        // Thread does not exist anymore.
        return;
    }

    // Get the object for this process.
    ProcObj_t* procObjPtr = le_hashmap_Get(procsMap, &pid);

    if (procObjPtr == NULL)
    {
        // Create the object.
        procObjPtr = le_mem_ForceAlloc(ProcObjPool);

        procObjPtr->procID = pid;
        procObjPtr->threadList = LE_SLS_LIST_INIT;

        le_hashmap_Put(procsMap, &(procObjPtr->procID), procObjPtr);
    }

    // Get the name of our process.
    if (pid == tid)
    {
        if (GetProcName(pid, procObjPtr->procName, sizeof(procObjPtr->procName)) != LE_OK)
        {
            // The process no longer exists.  Delete the process object.
            le_hashmap_Remove(procsMap, &(procObjPtr->procID));
            DeleteProcObj(procObjPtr);

            return;
        }
    }

    // Create the thread object.
    ThreadObj_t* threadObjPtr = le_mem_ForceAlloc(ThreadObjPool);

    threadObjPtr->threadID = tid;
    threadObjPtr->link = LE_SLS_LINK_INIT;

    // Add this thread to the process object.
    le_sls_Queue(&(procObjPtr->threadList), &(threadObjPtr->link));
}


//--------------------------------------------------------------------------------------------------
/**
 * Prints the list of process objects.
 */
//--------------------------------------------------------------------------------------------------
static void PrintAppObjs
(
    le_hashmap_Ref_t procsMap,      ///< [IN] Map of process objects to print.
    const char* prefixPtr           ///< [IN] Prefix to use when printing process objects.  This
                                    ///       prefix can be used to indent the list of processes.
)
{
    // Iterate over the hashmap and print the process object data.
    le_hashmap_It_Ref_t iter = le_hashmap_GetIterator(procsMap);

    while (le_hashmap_NextNode(iter) == LE_OK)
    {
        const ProcObj_t* procObjPtr = le_hashmap_GetValue(iter);

        printf("%s%s[%d] (", prefixPtr, procObjPtr->procName, procObjPtr->procID);

        // Iterate over the list of threads and print them.
        bool first = true;
        le_sls_Link_t* threadLinkPtr = le_sls_Peek(&(procObjPtr->threadList));

        while (threadLinkPtr != NULL)
        {
            ThreadObj_t* threadPtr = CONTAINER_OF(threadLinkPtr, ThreadObj_t, link);

            if (first)
            {
                printf("%d", threadPtr->threadID);

                first = false;
            }
            else
            {
                printf(", %d", threadPtr->threadID);
            }

            threadLinkPtr = le_sls_PeekNext(&(procObjPtr->threadList), threadLinkPtr);
        }

        printf(")\n");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Prints and application's list of running processes and their threads.
 */
//--------------------------------------------------------------------------------------------------
static void PrintAppProcs
(
    const char* appNamePtr,         ///< [IN] App name.
    const char* prefixPtr           ///< [IN] Prefix to use when printing process lines.  This
                                    ///       prefix can be used to indent the list of processes.
)
{
    // Get the list of thread IDs for this app.
    pid_t tidList[MAX_NUM_THREADS_TO_DISPLAY];

    ssize_t numAvailThreads = cgrp_GetThreadList(CGRP_SUBSYS_FREEZE,
                                                 appNamePtr,
                                                 tidList,
                                                 MAX_NUM_THREADS_TO_DISPLAY);

    if (numAvailThreads <= 0)
    {
        // No threads/processes for this app.
        return;
    }

    // Calculate the number of threads to iterate over.
    size_t numThreads = numAvailThreads;

    if (numAvailThreads > MAX_NUM_THREADS_TO_DISPLAY)
    {
        numThreads = MAX_NUM_THREADS_TO_DISPLAY;
    }

    // Iterate over the list of threads and build the process objects.
    size_t i;
    for (i = 0; i < numThreads; i++)
    {
        BuildProcObjs(ProcObjMap, tidList[i]);
    }

    // Print the process object information.
    printf("%srunning processes:\n", prefixPtr);
    PrintAppObjs(ProcObjMap, "    ");

    // Clean-up the list of process objects.
    ClearProcHashmap(ProcObjMap);

    if (numAvailThreads > numThreads)
    {
        // More threads/processes are available.
        printf("...\n");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Prints an installed application's info.
 */
//--------------------------------------------------------------------------------------------------
static void PrintInstalledAppInfo
(
    const char* appNamePtr
)
{
    printf("%s\n", appNamePtr);
    printf("  status: %s\n", GetAppState(appNamePtr));

    PrintAppProcs(appNamePtr, "  ");
    PrintedAppInfoFile(appNamePtr, "  ");

    printf("\n");
}


//--------------------------------------------------------------------------------------------------
/**
 * Prints the application information.
 */
//--------------------------------------------------------------------------------------------------
static void PrintAppInfo
(
    const char* appNamePtr      ///< [IN] Application name to get the information for.
)
{
    le_appInfo_ConnectService();
    le_cfg_ConnectService();

    le_cfg_IteratorRef_t cfgIter = le_cfg_CreateReadTxn("/apps");

    if (!le_cfg_NodeExists(cfgIter, appNamePtr))
    {
        printf("[not installed] %s\n", appNamePtr);
        printf("\n");
    }
    else
    {
        PrintInstalledAppInfo(appNamePtr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Implements the "info" command.
 *
 * @note This function does not return.
 **/
//--------------------------------------------------------------------------------------------------
static void PrintInfo
(
    void
)
{
    if (AppNamePtr == NULL)
    {
        ListInstalledApps(PrintAppInfo);
    }
    else
    {
        PrintAppInfo(AppNamePtr);
    }

    exit(EXIT_SUCCESS);
}


//--------------------------------------------------------------------------------------------------
/**
 * Implements the "list" command.
 *
 * @note This function does not return.
 **/
//--------------------------------------------------------------------------------------------------
static void ListApps
(
    void
)
{
    ListInstalledApps(NULL);

    exit(EXIT_SUCCESS);
}


//--------------------------------------------------------------------------------------------------
/**
 * Prints the application version.
 *
 * @note This function does not return.
 */
//--------------------------------------------------------------------------------------------------
static void PrintAppVersion
(
    void
)
{
    le_cfg_ConnectService();

    le_cfg_IteratorRef_t cfgIter = le_cfg_CreateReadTxn("/apps");
    le_cfg_GoToNode(cfgIter, AppNamePtr);

    if (!le_cfg_NodeExists(cfgIter, ""))
    {
        printf("%s is not installed.\n", AppNamePtr);
    }
    else
    {
        char version[LIMIT_MAX_PATH_BYTES];

        le_result_t result = le_cfg_GetString(cfgIter, "version", version, sizeof(version), "");

        if (strcmp(version, "") == 0)
        {
            printf("%s has no version\n", AppNamePtr);
        }
        else if (result == LE_OK)
        {
            printf("%s %s\n", AppNamePtr, version);
        }
        else
        {
            LE_WARN("Version string for app %s is too long.", AppNamePtr);
            printf("%s %s...\n", AppNamePtr, version);
        }
    }

    exit(EXIT_SUCCESS);
}


//--------------------------------------------------------------------------------------------------
/**
 * Function that gets called by le_arg_Scan() when it encounters an application name argument on the
 * command line.
 **/
//--------------------------------------------------------------------------------------------------
static void AppNameArgHandler
(
    const char* appNamePtr
)
{
    AppNamePtr = appNamePtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Function that gets called by le_arg_Scan() when it encounters the command argument on the
 * command line.
 **/
//--------------------------------------------------------------------------------------------------
static void CommandArgHandler
(
    const char* command
)
{
    if (strcmp(command, "help") == 0)
    {
        PrintHelp();    // Doesn't return;
    }

    if (strcmp(command, "start") == 0)
    {
        CommandFunc = StartApp;

        le_arg_AddPositionalCallback(AppNameArgHandler);
    }
    else if (strcmp(command, "stop") == 0)
    {
        CommandFunc = StopApp;

        le_arg_AddPositionalCallback(AppNameArgHandler);
    }
    else if (strcmp(command, "stopLegato") == 0)
    {
        CommandFunc = StopLegato;
    }
    else if (strcmp(command, "list") == 0)
    {
        CommandFunc = ListApps;
    }
    else if (strcmp(command, "status") == 0)
    {
        CommandFunc = PrintStatus;

        // Accept an optional app name argument.
        le_arg_AddPositionalCallback(AppNameArgHandler);
        le_arg_AllowLessPositionalArgsThanCallbacks();
    }
    else if (strcmp(command, "version") == 0)
    {
        CommandFunc = PrintAppVersion;

        le_arg_AddPositionalCallback(AppNameArgHandler);
    }
    else if (strcmp(command, "info") == 0)
    {
        CommandFunc = PrintInfo;

        // Accept an optional app name argument.
        le_arg_AddPositionalCallback(AppNameArgHandler);
        le_arg_AllowLessPositionalArgsThanCallbacks();
    }
    else
    {
        fprintf(stderr, "Unknown command '%s'.  Try --help.\n", command);
        exit(EXIT_FAILURE);
    }
}


//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    ProcObjPool = le_mem_CreatePool("ProcObjPool", sizeof(ProcObj_t));
    ThreadObjPool = le_mem_CreatePool("ThreadObjPool", sizeof(ThreadObj_t));

    ProcObjMap = le_hashmap_Create("ProcsMap",
                                   EST_MAX_NUM_PROC,
                                   le_hashmap_HashUInt32,
                                   le_hashmap_EqualsUInt32);

    le_arg_SetFlagCallback(PrintHelp, "h", "help");

    le_arg_AddPositionalCallback(CommandArgHandler);

    le_arg_Scan();

    CommandFunc();
}
