//--------------------------------------------------------------------------------------------------
/**
 * @file le_temp.c
 *
 * This file contains the source code of the high level temperature API.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "pa_temp.h"

//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of sensors (can be extended dynamically).
 *
 */
//--------------------------------------------------------------------------------------------------
#define MAX_NUM_OF_SENSOR   10

//--------------------------------------------------------------------------------------------------
// Data structures.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Data structure of a sensor context.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    pa_temp_Handle_t        paHandle;
    le_temp_SensorRef_t     ref;                             ///< sensor reference
    char                    thresholdEvent[LE_TEMP_THRESHOLD_NAME_MAX_BYTES];
    le_dls_Link_t           link;                            ///< Object node link
} SensorCtx_t;

//--------------------------------------------------------------------------------------------------
/**
 * Temperature threshold report structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_temp_SensorRef_t ref;                             ///< sensor reference
    char                threshold[LE_TEMP_THRESHOLD_NAME_MAX_BYTES];
} ThresholdReport_t;

//--------------------------------------------------------------------------------------------------
//                                       Static declarations
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Memory Pool for Sensors.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t   SensorPool;

//--------------------------------------------------------------------------------------------------
/**
 * list of sensor context.
 */
//--------------------------------------------------------------------------------------------------
static le_dls_List_t  SensorList;

//--------------------------------------------------------------------------------------------------
/**
 * Safe Reference Map for the antenna reference
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t SensorRefMap;

//--------------------------------------------------------------------------------------------------
/**
 * Event ID for New Temperature Threshold event notification.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t TemperatureThresholdEventId;

//--------------------------------------------------------------------------------------------------
/**
 * Pool for Temperature threshold Event reporting.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t ThresholdReportPool;


//--------------------------------------------------------------------------------------------------
/**
 * Look for a sensor reference corresponding to a name.
 */
//--------------------------------------------------------------------------------------------------
static le_temp_SensorRef_t FindSensorRef
(
    const char*  sensorPtr     ///< [IN] Name of the temperature sensor.
)
{
    le_temp_Handle_t    leHandle;

    if (pa_temp_GetHandle(sensorPtr, &leHandle) == LE_OK)
    {
        if (leHandle)
        {
            SensorCtx_t* sensorCtxPtr=(SensorCtx_t*)leHandle;
            return sensorCtxPtr->ref;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        return NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer Temperature Handler.
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerTemperatureChangeHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    ThresholdReport_t*                  tempPtr = reportPtr;
    le_temp_ThresholdEventHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    LE_DEBUG("Call application handler for %p sensor reference with '%s' threshold",
             tempPtr->ref,
             tempPtr->threshold);

    // Call the client handler
    clientHandlerFunc(tempPtr->ref,
                      tempPtr->threshold,
                      le_event_GetContextPtr() );

    le_mem_Release(reportPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * PA Temperature Change handler function.
 *
 */
//--------------------------------------------------------------------------------------------------
static void PaTemperatureThresholdHandler
(
    le_temp_Handle_t leHandle,      ///< [IN] Handle of the temperature sensor.
    const char*      thresholdPtr,  ///< [IN] Name of the threshold.
    void*            contextPtr
)
{
    SensorCtx_t* sensorCtxPtr = (SensorCtx_t*)leHandle;

    ThresholdReport_t* tempEventPtr = le_mem_ForceAlloc(ThresholdReportPool);

    tempEventPtr->ref = sensorCtxPtr->ref;
    strncpy(tempEventPtr->threshold, thresholdPtr, LE_TEMP_THRESHOLD_NAME_MAX_BYTES);

    LE_INFO("Report '%s' threshold for %p sensor reference",
             tempEventPtr->threshold,
             tempEventPtr->ref);

    le_event_ReportWithRefCounting(TemperatureThresholdEventId, tempEventPtr);
}



//--------------------------------------------------------------------------------------------------
//                                       Public declarations
//--------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
/**
 * Initialization of the Legato Temperature Monitoring Service
 */
//--------------------------------------------------------------------------------------------------
void le_temp_Init
(
    void
)
{
    LE_DEBUG("call marker.");

    // Create an event Id for temperature change notification
    TemperatureThresholdEventId = le_event_CreateIdWithRefCounting("TempThresholdEvent");

    ThresholdReportPool = le_mem_CreatePool("ThresholdReportPool",
                                            sizeof(ThresholdReport_t));

    SensorPool = le_mem_CreatePool("SensorPool",
                                   sizeof(SensorCtx_t));

    SensorRefMap = le_ref_CreateMap("SensorRefMap", MAX_NUM_OF_SENSOR);

    SensorList = LE_DLS_LIST_INIT;

    // Register a handler function for new temperature Threshold Event
    pa_temp_AddTempEventHandler(PaTemperatureThresholdHandler, NULL);
}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'le_temp_ThresholdEvent'
 *
 * This event provides information on Threshold reached.
 */
//--------------------------------------------------------------------------------------------------
le_temp_ThresholdEventHandlerRef_t le_temp_AddThresholdEventHandler
(
    le_temp_ThresholdEventHandlerFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
)
{
    le_event_HandlerRef_t        handlerRef;

    LE_DEBUG("call marker.");

    if (handlerPtr == NULL)
    {
        LE_KILL_CLIENT("Handler function is NULL !");
        return NULL;
    }

    handlerRef = le_event_AddLayeredHandler("TemperatureThresholdHandler",
                                            TemperatureThresholdEventId,
                                            FirstLayerTemperatureChangeHandler,
                                            (le_event_HandlerFunc_t)handlerPtr);

    le_event_SetContextPtr(handlerRef, contextPtr);

    return (le_temp_ThresholdEventHandlerRef_t)(handlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'le_temp_ThresholdEvent'
 */
//--------------------------------------------------------------------------------------------------
void le_temp_RemoveThresholdEventHandler
(
    le_temp_ThresholdEventHandlerRef_t addHandlerRef
        ///< [IN]
)
{
    LE_DEBUG("call marker.");

    if (addHandlerRef == NULL)
    {
        LE_KILL_CLIENT("addHandlerRef function is NULL !");
        return;
    }

    le_event_RemoveHandler((le_event_HandlerRef_t) addHandlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Request a temperature sensor reference.
 *
 * @return
 *      - Reference to the temperature sensor.
 *      - NULL when the requested sensor is not supported.
 */
//--------------------------------------------------------------------------------------------------
le_temp_SensorRef_t le_temp_Request
(
    const char*  sensorPtr ///< [IN] Name of the temperature sensor.
)
{
    size_t              length;
    SensorCtx_t*        currentPtr=NULL;
    le_temp_SensorRef_t sensorRef;

    LE_DEBUG("call marker.");

    if (sensorPtr == NULL)
    {
        LE_KILL_CLIENT("sensorPtr is NULL !");
        return NULL;
    }

    if(strlen(sensorPtr) > (LE_TEMP_SENSOR_NAME_MAX_BYTES-1))
    {
        LE_KILL_CLIENT("strlen(sensorPtr) > %d", (LE_TEMP_SENSOR_NAME_MAX_BYTES-1));
        return NULL;
    }

    length = strnlen(sensorPtr, LE_TEMP_SENSOR_NAME_MAX_BYTES+1);
    if (!length)
    {
        return NULL;
    }

    if (length > LE_TEMP_SENSOR_NAME_MAX_BYTES)
    {
        return NULL;
    }

    // Check if this sensor already exists
    if ((sensorRef = FindSensorRef(sensorPtr)) != NULL)
    {
        SensorCtx_t* sensorCtxPtr = le_ref_Lookup(SensorRefMap, sensorRef);
        le_mem_AddRef(sensorCtxPtr);
        return sensorRef;
    }
    else
    {
        currentPtr = le_mem_ForceAlloc(SensorPool);

        if (pa_temp_Request(sensorPtr,
                            (le_temp_Handle_t)currentPtr,
                            &currentPtr->paHandle) == LE_OK)
        {
            currentPtr->ref = le_ref_CreateRef(SensorRefMap, currentPtr);
            currentPtr->link = LE_DLS_LINK_INIT;
            le_dls_Queue(&SensorList, &(currentPtr->link));

            LE_DEBUG("Create a new sensor reference (%p)", currentPtr->ref);
            return currentPtr->ref;
        }
        else
        {
            le_mem_Release(currentPtr);
            LE_DEBUG("This sensor (%s) doesn't exist on your platform", sensorPtr);
            return NULL;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve the temperature sensor's name from its reference.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_OVERFLOW      The name length exceed the maximum length.
 *      - LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_temp_GetSensorName
(
    le_temp_SensorRef_t sensorRef,      ///< [IN]  Temperature sensor reference.
    char*               sensorNamePtr,  ///< [OUT] Name of the temperature sensor.
    size_t              len             ///< [IN] The maximum length of the sensor name.
)
{
    SensorCtx_t* sensorCtxPtr = le_ref_Lookup(SensorRefMap, sensorRef);

    LE_DEBUG("call marker.");

    if (sensorCtxPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", sensorRef);
        return LE_FAULT;
    }

    if (sensorNamePtr == NULL)
    {
        LE_KILL_CLIENT("sensorNamePtr is NULL !");
        return LE_FAULT;
    }

    return pa_temp_GetSensorName(sensorCtxPtr->paHandle, sensorNamePtr, len);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the temperature in degree Celsius.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_FAULT         The function failed to get the temperature.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_temp_GetTemperature
(
    le_temp_SensorRef_t  sensorRef,     ///< [IN] Temperature sensor reference.
    int32_t*             temperaturePtr ///< [OUT] Temperature in degree Celsius.
)
{
    SensorCtx_t* sensorCtxPtr = le_ref_Lookup(SensorRefMap, sensorRef);

    LE_DEBUG("call marker.");

    if (sensorCtxPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", sensorRef);
        return LE_FAULT;
    }

    if (temperaturePtr == NULL)
    {
        LE_KILL_CLIENT("temperaturePtr is NULL!!");
        return LE_FAULT;
    }

    return pa_temp_GetTemperature(sensorCtxPtr->paHandle, temperaturePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the temperature threshold in degree Celsius. This function does not start the temperature
 * monitoring, call le_temp_StartMonitoring() to start it.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_temp_SetThreshold
(
    le_temp_SensorRef_t  sensorRef,     ///< [IN] Temperature sensor reference.
    const char*          thresholdPtr,  ///< [IN] Name of the threshold.
    int32_t              temperature    ///< [IN] Temperature threshold in degree Celsius.
)
{
    size_t       length;
    SensorCtx_t* sensorCtxPtr = le_ref_Lookup(SensorRefMap, sensorRef);

    LE_DEBUG("call marker.");

    if (sensorCtxPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", sensorRef);
        return LE_FAULT;
    }

    if (thresholdPtr == NULL)
    {
        LE_KILL_CLIENT("thresholdPtr is NULL !");
        return LE_FAULT;
    }

    if(strlen(thresholdPtr) > (LE_TEMP_THRESHOLD_NAME_MAX_BYTES-1))
    {
        LE_KILL_CLIENT("strlen(thresholdPtr) > %d", (LE_TEMP_THRESHOLD_NAME_MAX_BYTES-1));
        return LE_FAULT;
    }

    length = strnlen(thresholdPtr, LE_TEMP_THRESHOLD_NAME_MAX_BYTES+1);
    if (!length)
    {
        return LE_FAULT;
    }

    if (length > LE_TEMP_THRESHOLD_NAME_MAX_BYTES)
    {
        return LE_FAULT;
    }

    return pa_temp_SetThreshold(sensorCtxPtr->paHandle, thresholdPtr, temperature);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the temperature threshold in degree Celsius.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_FAULT         The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_temp_GetThreshold
(
    le_temp_SensorRef_t  sensorRef,     ///< [IN] Temperature sensor reference.
    const char*          thresholdPtr,  ///< [IN] Name of the threshold.
    int32_t*             temperaturePtr ///< [OUT] Temperature threshold in degree Celsius.
)
{
    size_t       length;
    SensorCtx_t* sensorCtxPtr = le_ref_Lookup(SensorRefMap, sensorRef);

    LE_DEBUG("call marker.");

    if (sensorCtxPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", sensorRef);
        return LE_FAULT;
    }

    if (thresholdPtr == NULL)
    {
        LE_KILL_CLIENT("thresholdPtr is NULL !");
        return LE_FAULT;
    }

    if(strlen(thresholdPtr) > (LE_TEMP_THRESHOLD_NAME_MAX_BYTES-1))
    {
        LE_KILL_CLIENT("strlen(thresholdPtr) > %d", (LE_TEMP_THRESHOLD_NAME_MAX_BYTES-1));
        return LE_FAULT;
    }

    length = strnlen(thresholdPtr, LE_TEMP_THRESHOLD_NAME_MAX_BYTES+1);
    if (!length)
    {
        return LE_FAULT;
    }

    if (length > LE_TEMP_THRESHOLD_NAME_MAX_BYTES)
    {
        return LE_FAULT;
    }

    if (temperaturePtr == NULL)
    {
        LE_KILL_CLIENT("temperaturePtr is NULL!!");
        return LE_FAULT;
    }

    return pa_temp_GetThreshold(sensorCtxPtr->paHandle, thresholdPtr, temperaturePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Start the temperature monitoring with the temperature thresholds configured by
 * le_temp_SetThreshold() function.
 *
 * @return
 *      - LE_OK            The function succeeded.
 *      - LE_FAULT         The function failed to apply the thresholds.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_temp_StartMonitoring
(
    void
)
{
    LE_DEBUG("call marker.");

    return pa_temp_StartMonitoring();
}
