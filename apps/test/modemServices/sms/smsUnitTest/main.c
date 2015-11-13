/**
 * This module implements the unit tests for SMS API.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */

//#include "legato.h"
#include "interfaces.h"

#include "log.h"
#include "pa_sms.h"
#include "pa_sim.h"
#include "smsPdu.h"
#include "pa_simu.h"
#include "pa_sim_simu.h"
#include "pa_sms_simu.h"

#include "le_sim_local.h"
#include "args.h"
#include "main.h"


le_log_SessionRef_t LE_LOG_SESSION;
le_log_Level_t* LE_LOG_LEVEL_FILTER_PTR;

#define DUMPSIZE 132


//--------------------------------------------------------------------------------------------------
/**
 * Dump the PDU
 */
//--------------------------------------------------------------------------------------------------
void DumpPdu
(
    const char      *labelStr,     ///< [IN] label
    const uint8_t   *bufferPtr,    ///< [IN] buffer
    size_t           bufferSize    ///< [IN] buffer size
)
{
        uint32_t index = 0;
        uint32_t i = 0;
        char output[DUMPSIZE] = {0};

        LE_DEBUG("%s:",labelStr);
        for (i=0; i<bufferSize; i++)
        {
            index += sprintf(&output[index],"%02X",bufferPtr[i]);
            if ( !((i+1)%32) )
            {
                LE_INFO("%s",output);
                index = 0;
            }
        }
        LE_INFO("%s",output);
}

//--------------------------------------------------------------------------------------------------
/**
 * main of the test
 *
 */
//--------------------------------------------------------------------------------------------------
int main
(
    int argc,
    char *argv[]
)
{
    LE_LOG_SESSION = log_RegComponent( "sms", &LE_LOG_LEVEL_FILTER_PTR);

    arg_SetArgs(argc,argv);

    le_log_TraceRef_t traceRef = le_log_GetTraceRef( "smsPdu" );

    le_log_SetFilterLevel(LE_LOG_DEBUG);
    le_log_EnableTrace(traceRef);

    // Init the test case / test suite data structures
    smsPdu_Initialize();

    // Init pa simu
    pa_simSimu_Init();

    // Init le_sim
    le_sim_Init();

    pa_simSimu_SetPin("0000");
    pa_sms_SetSmsc("+33123456789");

    // Init the sms PA Simu
    sms_simu_Init();

    //EnterPin Code
    pa_sim_EnterPIN(PA_SIM_PIN, "0000");

    LE_INFO("======== Start UnitTest of SMS API ========");

    LE_INFO("======== CDMA PDU Test ========");
    testle_sms_CdmaPduTest();

    LE_INFO("======== SMS PDU Test ========");
    testle_sms_SmsPduTest();

    LE_INFO("======== SMS API Unit Test ========");
    testle_sms_SmsApiUnitTest();

    LE_INFO("======== UnitTest of SMS API ends with SUCCESS ========");

    return 0;
}


