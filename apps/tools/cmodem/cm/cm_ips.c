//-------------------------------------------------------------------------------------------------
/**
 * @file cm_ips.c
 *
 * Handle IPS (input power supply) related functionality.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
//-------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "cm_ips.h"
#include "cm_common.h"

//-------------------------------------------------------------------------------------------------
/**
 * Print the IPS help text to stdout.
 */
//-------------------------------------------------------------------------------------------------
void cm_ips_PrintIpsHelp
(
    void
)
{
    printf("IPS usage\n"
           "==========\n\n"
           "To read and print the voltage from the input power supply:\n"
           "\tcm ips\n"
           "\tcm ips read\n"
           );
}

//-------------------------------------------------------------------------------------------------
/**
 * Read the input voltage.
 */
//-------------------------------------------------------------------------------------------------
static le_result_t cm_ips_ReadAndPrintVoltage
(
    void
)
{
    le_result_t result = LE_FAULT;
    uint32_t value;

    result = le_ips_GetInputVoltage(&value);
    if (result == LE_OK)
    {
        printf("%u\n", value);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Process commands for IPS service.
 */
//--------------------------------------------------------------------------------------------------
void cm_ips_ProcessIpsCommand
(
    const char * command,   ///< [IN] Command
    size_t numArgs          ///< [IN] Number of arguments
)
{
    if (strcmp(command, "help") == 0)
    {
        cm_ips_PrintIpsHelp();
    }
    else if (strcmp(command, "read") == 0)
    {
        if (LE_OK != cm_ips_ReadAndPrintVoltage())
        {
            printf("Read failed.\n");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("Invalid command for IPS service.\n");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
