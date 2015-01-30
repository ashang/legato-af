/**
 * @file pa_mon.c
 *
 * AT implementation of pa monitoring API.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "interfaces.h"
#include "pa_antenna.h"


//--------------------------------------------------------------------------------------------------
//                                       Public declarations
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA Monitoring module.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("pa_mon init");

    // Initialize the PA antenna monitoring
    pa_antenna_Init();

    LE_INFO("pa_mon init done");
}
