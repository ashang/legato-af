/**
 * This module implements the unit tests for eCall API.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */


#include "legato.h"
#include "interfaces.h"
#include "le_cfg_simu.h"
#include "le_ecall_local.h"
#include "le_mcc_local.h"
#include "pa_mcc_simu.h"
#include "log.h"
#include "pa_ecall.h"
#include "pa_ecall_simu.h"
#include "mdmCfgEntries.h"
#include "args.h"


//--------------------------------------------------------------------------------------------------
// Begin Stubbed functions.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Server Service Reference
 */
//--------------------------------------------------------------------------------------------------
static le_msg_ServiceRef_t _ServerServiceRef;

//--------------------------------------------------------------------------------------------------
/**
 * Client Session Reference for the current message received from a client
 */
//--------------------------------------------------------------------------------------------------
static le_msg_SessionRef_t _ClientSessionRef;

//--------------------------------------------------------------------------------------------------
/**
 * Get the server service reference
 */
//--------------------------------------------------------------------------------------------------
le_msg_ServiceRef_t le_mcc_GetServiceRef
(
    void
)
{
    return _ServerServiceRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionRef_t le_mcc_GetClientSessionRef
(
    void
)
{
    return _ClientSessionRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * Acquire a wakeup source
 *
 * @note The process exits on failures
 */
//--------------------------------------------------------------------------------------------------
void le_pm_StayAwake
(
    le_pm_WakeupSourceRef_t w
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Release a wakeup source
 *
 * @note The process exits on failure
 */
//--------------------------------------------------------------------------------------------------
void le_pm_Relax(
    le_pm_WakeupSourceRef_t w
)
{
    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Create a new wakeup source
 *
 * @return Reference to wakeup source, NULL on failure
 *
 * @note The process exits on syscall failures
 */
//--------------------------------------------------------------------------------------------------
le_pm_WakeupSourceRef_t le_pm_NewWakeupSource
(
    uint32_t    opts,
    const char *tag
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Registers a function to be called whenever one of this service's sessions is closed by
 * the client.
 *
 * @note    Server-only function.
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionEventHandlerRef_t le_msgSimu_AddServiceCloseHandler
(
    le_msg_ServiceRef_t             serviceRef, ///< [in] Reference to the service.
    le_msg_SessionEventHandler_t    handlerFunc,///< [in] Handler function.
    void*                           contextPtr  ///< [in] Opaque pointer value to pass to handler.
)
{
    return NULL;
}

//--------------------------------------------------------------------------------------------------
// End Stubbed functions.
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
// Test functions.
//--------------------------------------------------------------------------------------------------

#define NB_CLIENT 2

// Task context
typedef struct
{
    uint32_t                         appId;
    le_thread_Ref_t                  appThreadRef;
    le_ecall_StateChangeHandlerRef_t ecallHandler;
    le_ecall_CallRef_t               ecallRef;
    le_ecall_State_t                 ecallState;

} AppContext_t;

// VIN: ASDAJNPR1VABCDEFG
static uint8_t              ImportedMsd[] ={0x01, 0x4C, 0x07, 0x80, 0xA6, 0x4D, 0x29, 0x25,
                                            0x97, 0x60, 0x17, 0x0A, 0x2C, 0xC3, 0x4E, 0x3D,
                                            0x05, 0x1B, 0x18, 0x48, 0x61, 0xEB, 0xA0, 0xC8,
                                            0xFF, 0x73, 0x7E, 0x64, 0x20, 0xD1, 0x04, 0x01,
                                            0x3F, 0x81, 0x00};
static AppContext_t         AppCtx[NB_CLIENT];
static le_sem_Ref_t         ThreadSemaphore;
static le_sem_Ref_t         InitSemaphore;
static le_ecall_State_t     CurrentEcallState;
static le_ecall_CallRef_t   CurrentEcallRef = NULL;
static le_clk_Time_t        TimeToWait = { 0, 1000000 };


//--------------------------------------------------------------------------------------------------
/**
 * Handler function for eCall state Notifications.
 *
 */
//--------------------------------------------------------------------------------------------------
static void MyECallEventHandler
(
    le_ecall_CallRef_t  eCallRef,
    le_ecall_State_t    state,
    void*               contextPtr
)
{
    LE_INFO("eCall TEST: New eCall state: %d for eCall ref.%p", state, eCallRef);

    AppContext_t * appCtxPtr = (AppContext_t*) contextPtr;

    LE_INFO("Handler of app id: %d", appCtxPtr->appId);

    LE_ASSERT(CurrentEcallState == state);
    LE_ASSERT(CurrentEcallRef == eCallRef);

    appCtxPtr->ecallState = state;
    appCtxPtr->ecallRef = eCallRef;

    switch(state)
    {
        case LE_ECALL_STATE_STARTED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_STARTED.");
            break;
        }
        case LE_ECALL_STATE_CONNECTED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_CONNECTED.");
            break;
        }
        case LE_ECALL_STATE_DISCONNECTED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_DISCONNECTED.");
            LE_INFO("Termination reason: %d", le_ecall_GetTerminationReason(eCallRef) );
            break;
        }
        case LE_ECALL_STATE_WAITING_PSAP_START_IND:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_WAITING_PSAP_START_IND.");
            break;
        }
        case LE_ECALL_STATE_PSAP_START_IND_RECEIVED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_PSAP_START_IND_RECEIVED.");
            LE_INFO("Send MSD...");
            LE_ASSERT(le_ecall_SendMsd(eCallRef) == LE_OK);
            break;
        }
        case LE_ECALL_STATE_MSD_TX_STARTED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_MSD_TX_STARTED.");
            break;
        }
        case LE_ECALL_STATE_LLNACK_RECEIVED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_LLNACK_RECEIVED.");
            break;
        }
        case LE_ECALL_STATE_LLACK_RECEIVED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_LLACK_RECEIVED.");
            break;
        }
        case LE_ECALL_STATE_MSD_TX_COMPLETED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_MSD_TX_COMPLETED.");
            break;
        }
        case LE_ECALL_STATE_MSD_TX_FAILED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_MSD_TX_FAILED.");
            break;
        }
        case LE_ECALL_STATE_ALACK_RECEIVED_POSITIVE:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_ALACK_RECEIVED_POSITIVE.");
            break;
        }
        case LE_ECALL_STATE_ALACK_RECEIVED_CLEAR_DOWN:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_ALACK_RECEIVED_CLEAR_DOWN.");
            break;
        }
        case LE_ECALL_STATE_STOPPED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_STOPPED.");
            break;
        }
        case LE_ECALL_STATE_RESET:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_RESET.");
            break;
        }
        case LE_ECALL_STATE_COMPLETED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_COMPLETED.");
            break;
        }
        case LE_ECALL_STATE_FAILED:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_FAILED.");
            break;
        }
        case LE_ECALL_STATE_END_OF_REDIAL_PERIOD:
        {
            LE_INFO("Check MyECallEventHandler passed, state is LE_ECALL_STATE_END_OF_REDIAL_PERIOD.");
            break;
        }
        case LE_ECALL_STATE_UNKNOWN:
        default:
        {
            LE_INFO("Check MyECallEventHandler failed, unknown state.");
            break;
        }
    }

    // Semaphore is used to synchronize the task execution with the core test
    le_sem_Post(ThreadSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Synchronize test thread (i.e. main) and tasks
 *
 */
//--------------------------------------------------------------------------------------------------
static void SynchTest
(
    void
)
{
    int i=0;

    for (;i<NB_CLIENT;i++)
    {
        LE_ASSERT(le_sem_WaitWithTimeOut(ThreadSemaphore, TimeToWait) == LE_OK);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Check the result of the state handlers
 *
 */
//--------------------------------------------------------------------------------------------------
static void CheckStateHandlerResult
(
    void
)
{
    int i;

    // Check that contexts are correctly updated
    for (i=0; i < NB_CLIENT; i++)
    {
        LE_ASSERT(AppCtx[i].appId == i);
        LE_ASSERT(AppCtx[i].ecallState == CurrentEcallState);
        LE_ASSERT(AppCtx[i].ecallRef == CurrentEcallRef);
        LE_ASSERT(le_ecall_GetState(AppCtx[i].ecallRef) == CurrentEcallState);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Test tasks: this function handles the task and run an eventLoop
 *
 */
//--------------------------------------------------------------------------------------------------
static void* AppHandler
(
    void* ctxPtr
)
{
    AppContext_t * appCtxPtr = (AppContext_t*) ctxPtr;

    LE_INFO("App id: %d", appCtxPtr->appId);

    // Subscribe to eCall state handler
    LE_ASSERT((appCtxPtr->ecallHandler = le_ecall_AddStateChangeHandler(MyECallEventHandler,
                                                                        ctxPtr)) != NULL);

    // Semaphore is used to synchronize the task execution with the core test
    le_sem_Post(ThreadSemaphore);

    le_event_RunLoop();
}

//--------------------------------------------------------------------------------------------------
/**
 * Simulate and check the eCall state
 *
 */
//--------------------------------------------------------------------------------------------------
static void SimulateAndCheckState
(
    le_ecall_State_t state
)
{
    CurrentEcallState = state;

    LE_INFO("Simulate state.%d", CurrentEcallState);
    pa_ecallSimu_ReportEcallState(CurrentEcallState);

    // The tasks have subscribe to state event handler:
    // wait the handlers' calls
    SynchTest();

    // Check state handler result
    CheckStateHandlerResult();
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove state handlers
 *
 */
//--------------------------------------------------------------------------------------------------
static void RemoveHandler
(
    void* param1Ptr,
    void* param2Ptr
)
{
    AppContext_t * appCtxPtr = (AppContext_t*) param1Ptr;

    le_ecall_RemoveStateChangeHandler(appCtxPtr->ecallHandler);

    // Semaphore is used to synchronize the task execution with the core test
    le_sem_Post(ThreadSemaphore);
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the test environmeent:
 * - create some tasks (simulate multi app)
 * - create semaphore (to make checkpoints and synchronize test and tasks)
 * - simulate eCall states
 * - check that state handlers are correctly called
 *
 * API tested:
 * - le_ecall_AddStateChangeHandler
 * - le_ecall_GetState
 *
 * Exit if failed
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_ecall_AddHandlers
(
    void
)
{
    int i;

    // Create a semaphore to coordinate the test
    ThreadSemaphore = le_sem_Create("HandlerSem",0);

    // int app context
    memset(AppCtx, 0, NB_CLIENT*sizeof(AppContext_t));

    // Start tasks: simulate multi-user of le_ecall
    // each thread subcribes to state handler using le_ecall_AddStateChangeHandler
    for (i=0; i < NB_CLIENT; i++)
    {
        char string[20]={0};
        snprintf(string,20,"app%dhandler", i);
        AppCtx[i].appId = i;
        AppCtx[i].appThreadRef = le_thread_Create(string, AppHandler, &AppCtx[i]);
        le_thread_Start(AppCtx[i].appThreadRef);
    }

    // Wait that the tasks have started before continuing the test
    SynchTest();

    LE_ASSERT((CurrentEcallRef=le_ecall_Create()) != NULL);

    SimulateAndCheckState(LE_ECALL_STATE_STARTED);
    SimulateAndCheckState(LE_ECALL_STATE_CONNECTED);
    SimulateAndCheckState(LE_ECALL_STATE_WAITING_PSAP_START_IND);
    SimulateAndCheckState(LE_ECALL_STATE_PSAP_START_IND_RECEIVED);
    SimulateAndCheckState(LE_ECALL_STATE_MSD_TX_STARTED);
    SimulateAndCheckState(LE_ECALL_STATE_LLNACK_RECEIVED);
    SimulateAndCheckState(LE_ECALL_STATE_LLACK_RECEIVED);
    SimulateAndCheckState(LE_ECALL_STATE_MSD_TX_COMPLETED);
    SimulateAndCheckState(LE_ECALL_STATE_ALACK_RECEIVED_POSITIVE);
    SimulateAndCheckState(LE_ECALL_STATE_COMPLETED);
    SimulateAndCheckState(LE_ECALL_STATE_RESET);
// TODO: will be completed once pa_mcc_simu will be ready
// SimulateAndCheckState(LE_ECALL_STATE_DISCONNECTED);

    // Check that no more call of the semaphore
    LE_ASSERT(le_sem_GetValue(ThreadSemaphore) == 0);
    le_ecall_Delete(CurrentEcallRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test remove handlers
 *
 * API tested:
 * - le_ecall_RemoveStateChangeHandler
 *
 * Exit if failed
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_ecall_RemoveHandlers
(
    void
)
{
    int i;

    // Remove handlers: add le_ecall_RemoveStateChangeHandler to the eventLoop of tasks
    for (i=0; i<NB_CLIENT; i++)
    {
        le_event_QueueFunctionToThread(AppCtx[i].appThreadRef,
                                       RemoveHandler,
                                       &AppCtx[i],
                                       NULL);
    }

    // Wait for the tasks
    SynchTest();

    // Provoke event which should call the handlers
    pa_ecallSimu_ReportEcallState(LE_ECALL_STATE_STARTED);

    // Wait for the semaphore timeout to check that handlers are not called
    LE_ASSERT( le_sem_WaitWithTimeOut(ThreadSemaphore, TimeToWait) == LE_TIMEOUT );
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Set/Get Operation mode.
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_ecall_OperationMode
(
    void
)
{
    le_result_t         res = LE_FAULT;
    le_ecall_OpMode_t   mode = LE_ECALL_NORMAL_MODE;

    LE_ASSERT((res=le_ecall_ForceOnlyMode()) == LE_OK);
    LE_ASSERT((res=le_ecall_GetConfiguredOperationMode(&mode)) == LE_OK);
    LE_ASSERT(mode == LE_ECALL_ONLY_MODE);

    LE_ASSERT((res=le_ecall_ForcePersistentOnlyMode()) == LE_OK);
    LE_ASSERT((res=le_ecall_GetConfiguredOperationMode(&mode)) == LE_OK);
    LE_ASSERT(mode == LE_ECALL_FORCED_PERSISTENT_ONLY_MODE);

    LE_ASSERT((res=le_ecall_ExitOnlyMode()) == LE_OK);
    LE_ASSERT((res=le_ecall_GetConfiguredOperationMode(&mode)) == LE_OK);
    LE_ASSERT(mode == LE_ECALL_NORMAL_MODE);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Configuration settings.
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_ecall_ConfigSettings
(
    void
)
{
    char                 psap[LE_MDMDEFS_PHONE_NUM_MAX_BYTES];
    le_ecall_MsdTxMode_t mode = LE_ECALL_TX_MODE_PULL;
    uint16_t             deregTime = 0;

    LE_ASSERT(le_ecall_UseUSimNumbers() == LE_OK);

    LE_ASSERT(le_ecall_SetPsapNumber("0102030405") == LE_OK);
    LE_ASSERT(le_ecall_GetPsapNumber(psap, 1) == LE_OVERFLOW);
    LE_ASSERT(le_ecall_GetPsapNumber(psap, sizeof(psap)) == LE_OK);
    LE_ASSERT(strncmp(psap, "0102030405", strlen("0102030405")) == 0);


    LE_ASSERT(le_ecall_SetMsdTxMode(LE_ECALL_TX_MODE_PUSH) == LE_OK);
    LE_ASSERT(le_ecall_GetMsdTxMode(&mode) == LE_OK);
    LE_ASSERT(mode == LE_ECALL_TX_MODE_PUSH);

    LE_ASSERT(le_ecall_SetNadDeregistrationTime(180) == LE_OK);
    LE_ASSERT(le_ecall_GetNadDeregistrationTime(&deregTime) == LE_OK);
    LE_ASSERT(deregTime == 180);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: ERA-GLONASS settings.
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_ecall_EraGlonassSettings
(
    void
)
{
    uint16_t                            attempts = 0;
    uint16_t                            duration = 0;
    le_ecall_CallRef_t                   testECallRef = 0x00;

    LE_ASSERT((testECallRef=le_ecall_Create()) != NULL);

    LE_ASSERT(le_ecall_SetEraGlonassManualDialAttempts(7) == LE_OK);
    LE_ASSERT(le_ecall_GetEraGlonassManualDialAttempts(&attempts) == LE_OK);
    LE_ASSERT(attempts == 7);

    LE_ASSERT(le_ecall_SetEraGlonassAutoDialAttempts(9) == LE_OK);
    LE_ASSERT(le_ecall_GetEraGlonassAutoDialAttempts(&attempts) == LE_OK);
    LE_ASSERT(attempts == 9);

    LE_ASSERT(le_ecall_SetEraGlonassDialDuration(240) == LE_OK);
    LE_ASSERT(le_ecall_GetEraGlonassDialDuration(&duration) == LE_OK);
    LE_ASSERT(duration == 240);

    /* Crash Severity configuration */
    LE_ASSERT(le_ecall_SetMsdEraGlonassCrashSeverity(testECallRef, 0) == LE_OK);
    LE_ASSERT(le_ecall_ResetMsdEraGlonassCrashSeverity(testECallRef) == LE_OK);
    LE_ASSERT(le_ecall_SetMsdEraGlonassCrashSeverity(testECallRef, 99) == LE_OK);

    /* DataDiagnosticResult configuration */
    LE_ASSERT(le_ecall_SetMsdEraGlonassDiagnosticResult(testECallRef, 0x3FFFFFFFFFF) == LE_OK);
    LE_ASSERT(le_ecall_SetMsdEraGlonassDiagnosticResult(testECallRef, 0) == LE_OK);
    LE_ASSERT(le_ecall_ResetMsdEraGlonassDiagnosticResult(testECallRef) == LE_OK);
    LE_ASSERT(le_ecall_SetMsdEraGlonassDiagnosticResult(testECallRef,
              LE_ECALL_DIAG_RESULT_PRESENT_MIC_CONNECTION_FAILURE)
              == LE_OK);

    /* CrashInfo configuration */
    LE_ASSERT(le_ecall_SetMsdEraGlonassCrashInfo(testECallRef, 0xFFFF) == LE_OK);
    LE_ASSERT(le_ecall_SetMsdEraGlonassCrashInfo(testECallRef, 0) == LE_OK);
    LE_ASSERT(le_ecall_ResetMsdEraGlonassCrashInfo(testECallRef) == LE_OK);
    LE_ASSERT(le_ecall_SetMsdEraGlonassCrashInfo(testECallRef,
              LE_ECALL_CRASH_INFO_PRESENT_CRASH_FRONT_OR_SIDE
              | LE_ECALL_CRASH_INFO_CRASH_FRONT_OR_SIDE)
              == LE_OK);

    le_ecall_Delete(testECallRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Import or set MSD elements.
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_ecall_LoadMsd
(
    void
)
{
    le_ecall_CallRef_t   testECallRef = 0x00;

    LE_INFO("Start Testle_ecall_LoadMsd");

    LE_ASSERT((testECallRef=le_ecall_Create()) != NULL);

    LE_ASSERT(le_ecall_SetMsdPosition(testECallRef, true, +48898064, +2218092, 0) == LE_OK);
    LE_ASSERT(le_ecall_SetMsdPassengersCount(testECallRef, 3) == LE_OK);

    // Check LE_DUPLICATE on le_ecall_SetMsdPosition and le_ecall_SetMsdPassengersCount
    LE_ASSERT(le_ecall_ImportMsd(testECallRef, ImportedMsd, sizeof(ImportedMsd)) == LE_OK);
    LE_ASSERT(le_ecall_SetMsdPosition(testECallRef, true, +48070380, -11310000, 45) == LE_DUPLICATE);
    LE_ASSERT(le_ecall_SetMsdPassengersCount(testECallRef, 3) == LE_DUPLICATE);
    LE_ASSERT(le_ecall_ResetMsdEraGlonassCrashSeverity(testECallRef) == LE_DUPLICATE);
    LE_ASSERT(le_ecall_SetMsdEraGlonassCrashSeverity(testECallRef, 0) == LE_DUPLICATE);
    LE_ASSERT(le_ecall_ResetMsdEraGlonassDiagnosticResult(testECallRef) == LE_DUPLICATE);
    LE_ASSERT(le_ecall_SetMsdEraGlonassDiagnosticResult(testECallRef,
              LE_ECALL_DIAG_RESULT_PRESENT_MIC_CONNECTION_FAILURE)
              == LE_DUPLICATE);
    LE_ASSERT(le_ecall_ResetMsdEraGlonassCrashInfo(testECallRef) == LE_DUPLICATE);
    LE_ASSERT(le_ecall_SetMsdEraGlonassCrashInfo(testECallRef,
              LE_ECALL_CRASH_INFO_PRESENT_CRASH_FRONT_OR_SIDE
              | LE_ECALL_CRASH_INFO_CRASH_FRONT_OR_SIDE)
              == LE_DUPLICATE);

    le_ecall_Delete(testECallRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Create and start a manual eCall.
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_ecall_StartManual
(
    void
)
{
    le_ecall_CallRef_t  testECallRef = 0x00;
    le_ecall_State_t    state = LE_ECALL_STATE_UNKNOWN;

    LE_INFO("Start Testle_ecall_StartManual");

    LE_ASSERT(le_ecall_SetPsapNumber("0102030405") == LE_OK);

    LE_ASSERT(le_ecall_SetMsdTxMode(LE_ECALL_TX_MODE_PUSH) == LE_OK);

    LE_ASSERT((testECallRef=le_ecall_Create()) != NULL);

    LE_ASSERT(le_ecall_ImportMsd(testECallRef, ImportedMsd, sizeof(ImportedMsd)) == LE_OK);

    LE_ASSERT(le_ecall_StartManual(testECallRef) == LE_OK);

    LE_ASSERT(le_ecall_StartTest(testECallRef) == LE_BUSY);
    LE_ASSERT(le_ecall_StartAutomatic(testECallRef) == LE_BUSY);

    LE_ASSERT(le_ecall_End(testECallRef) == LE_OK);

    state=le_ecall_GetState(testECallRef);
    LE_ASSERT(((state>=LE_ECALL_STATE_STARTED) && (state<=LE_ECALL_STATE_FAILED)));

    le_ecall_Delete(testECallRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Create and start a test eCall.
 *
 */
//--------------------------------------------------------------------------------------------------
static void Testle_ecall_StartTest
(
    void
)
{
    le_ecall_CallRef_t                 testECallRef = 0x00;
    le_ecall_State_t                   state = LE_ECALL_STATE_UNKNOWN;

    LE_ASSERT(le_ecall_SetPsapNumber("0102030405") == LE_OK);

    LE_ASSERT(le_ecall_SetMsdTxMode(LE_ECALL_TX_MODE_PUSH) == LE_OK);

    LE_ASSERT((testECallRef=le_ecall_Create()) != NULL);

    LE_ASSERT(le_ecall_SetMsdPosition(testECallRef, true, +48898064, +2218092, 0) == LE_OK);

    LE_ASSERT(le_ecall_SetMsdPassengersCount(testECallRef, 3) == LE_OK);

    LE_ASSERT(le_ecall_StartTest(testECallRef) == LE_OK);

    LE_ASSERT(le_ecall_StartManual(testECallRef) == LE_BUSY);
    LE_ASSERT(le_ecall_StartAutomatic(testECallRef) == LE_BUSY);

    state=le_ecall_GetState(testECallRef);
    LE_ASSERT(((state>=LE_ECALL_STATE_STARTED) && (state<=LE_ECALL_STATE_FAILED)));

    le_ecall_Delete(testECallRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * UnitTestInit thread: this function initializes the test and run an eventLoop
 *
 */
//--------------------------------------------------------------------------------------------------
static void* UnitTestInit
(
    void* ctxPtr
)
{
    // pa simu init
    mcc_simu_Init();
    ecall_simu_Init();

    // Set ConfigTree settings for eCall
    le_cfg_IteratorRef_t eCallTestIteratorRef = (le_cfg_IteratorRef_t)0x000ECA11;
    le_cfgSimu_SetStringNodeValue(eCallTestIteratorRef, CFG_NODE_SYSTEM_STD, "ERA-GLONASS");
    le_cfgSimu_SetStringNodeValue(eCallTestIteratorRef, CFG_NODE_VIN, "WM9VDSVDSYA123456");
    le_cfgSimu_SetStringNodeValue(eCallTestIteratorRef, CFG_NODE_VEH, "Commercial-N1");
    le_cfgSimu_SetStringNodeValue(eCallTestIteratorRef, CFG_NODE_PROP, "Diesel");

    // init the services
    le_mcc_Init();
    le_ecall_Init();

    le_sem_Post(InitSemaphore);

    le_event_RunLoop();
}


//--------------------------------------------------------------------------------------------------
/**
 * main of the test
 *
 */
//--------------------------------------------------------------------------------------------------
int main
(
    int   argc,
    char* argv[]
)
{
    LE_LOG_SESSION = log_RegComponent("ecall", &LE_LOG_LEVEL_FILTER_PTR);

    arg_SetArgs(argc, argv);

    // Create a semaphore to coordinate Initialization
    InitSemaphore = le_sem_Create("InitSem",0);
    le_thread_Start(le_thread_Create("UnitTestInit", UnitTestInit, NULL));
    le_sem_Wait(InitSemaphore);

    LE_INFO("======== Start UnitTest of eCall API ========");

    LE_INFO("======== OperationMode Test  ========");
    Testle_ecall_OperationMode();
    LE_INFO("======== ConfigSettings Test  ========");
    Testle_ecall_ConfigSettings();
    LE_INFO("======== EraGlonassSettings Test  ========");
    Testle_ecall_EraGlonassSettings();
    LE_INFO("======== LoadMsd Test  ========");
    Testle_ecall_LoadMsd();
    LE_INFO("======== StartManual Test  ========");
    Testle_ecall_StartManual();
    LE_INFO("======== StartTest Test  ========");
    Testle_ecall_StartTest();
    LE_INFO("======== AddHandlers Test  ========");
    Testle_ecall_AddHandlers();
    LE_INFO("======== RemoveHandlers Test  ========");
    Testle_ecall_RemoveHandlers();

    LE_INFO("======== UnitTest of eCall API ends with SUCCESS ========");

    return 0;
}


