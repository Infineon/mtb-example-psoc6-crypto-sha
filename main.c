/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the PSoC 6 MCU Cryptography: SHA 
* Demonstration Example for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2019-2024, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/******************************************************************************
* Include header files
******************************************************************************/
#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include <string.h>

/*******************************************************************************
* Macros
********************************************************************************/
/* The input message size (inclusive of the string terminating character '\0').
 * Edit this macro to suit your message size */
#define MAX_MESSAGE_SIZE                    (100u)

/* Size of the message digest for SHA-256 encryption */
#define MESSAGE_DIGEST_SIZE                 (32u)

#define ASCII_RETURN_CARRIAGE               (0x0D)

#define UART_TIMEOUT_MS                     (1u)

#define SCREEN_HEADER "\r\n__________________________________________________"\
                  "____________________________\r\n*\t\tCE220511 PSoC 6 MCU "\
                  "Cryptography: SHA Demonstration\r\n*\r\n*\tThis code example"\
                  "shows how to generate a 32-byte hash value for an\r\n*\t"\
                  "arbitrary user input message using the SHA2 algorithm"\
                  "in PSoC 6 MCU\r\n*\tUART Terminal Settings: Baud Rate - "\
                  "115200 bps, 8N1\r\n*"\
                  "\r\n__________________________________________________"\
                  "____________________________\r\n"

#define SCREEN_HEADER1 "\r\n\n__________________________________________________"\
                  "____________________________\r\n"

/* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
#define CLEAR_SCREEN         "\x1b[2J\x1b[;H"

/* Number of bytes per line to be printed on the UART terminal */
#define BYTES_PER_LINE                        (16u)

/*******************************************************************************
* Data type definitions
********************************************************************************/
/* Data type definition to track the state machine accepting the user message */
typedef enum
{
    MESSAGE_ENTER_NEW,
    MESSAGE_READY,
    MESSAGE_NOT_READY
} message_status_t;

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void print_msg_digest(uint8_t* data, uint8_t len);

/*******************************************************************************
* Global Variables
********************************************************************************/
/* UART object used for reading character from terminal */
extern cyhal_uart_t cy_retarget_io_uart_obj;

cy_stc_crypto_sha_state_t sha_state;

/* Variables to hold the user message and the corresponding message digest */
CY_ALIGN(4) uint8_t message[MAX_MESSAGE_SIZE];
CY_ALIGN(4) uint8_t hash[MESSAGE_DIGEST_SIZE];

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* Main function
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_rslt_t uart_result = CY_RSLT_SUCCESS;
    cy_en_crypto_status_t crypto_status = CY_CRYPTO_NOT_INITIALIZED;

    /* Variable to track the status of the message entered by the user. */
    message_status_t msg_status = MESSAGE_ENTER_NEW;

    uint8_t msg_size = 0;

    bool uart_status = false;

    /* Initialize the device and board peripherals. */
    result = cybsp_init();

     /* Board init failed. Stop program execution. */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();
    
    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                                 CY_RETARGET_IO_BAUDRATE);
    
    /* retarget-io init failed. Stop program execution. */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen. */
    printf(CLEAR_SCREEN);

    printf(SCREEN_HEADER);

    /* Enable the Crypto block. */
    Cy_Crypto_Core_Enable(CRYPTO);

    for (;;)
    {
        switch (msg_status)
        {
            case MESSAGE_ENTER_NEW:
                memset(message, 0, MAX_MESSAGE_SIZE);
                msg_size = 0;
                printf("\r\nEnter the message:\r\n");
                uart_result = cyhal_uart_getc(&cy_retarget_io_uart_obj,
                                              &message[msg_size],
                                              UART_TIMEOUT_MS);
                msg_status = MESSAGE_NOT_READY;
                break;

            case MESSAGE_NOT_READY:
                uart_status = cyhal_uart_is_rx_active(&cy_retarget_io_uart_obj);
                if ((!uart_status) && (uart_result == CY_RSLT_SUCCESS))
                {
                    /* Check if the ENTER Key is pressed. If pressed,
                     * set the message status as MESSAGE_READY.
                     */
                    if (message[msg_size] == ASCII_RETURN_CARRIAGE)
                    {
                        message[msg_size]='\0';
                        msg_status = MESSAGE_READY;
                    }
                    else
                    {
                        cyhal_uart_putc(&cy_retarget_io_uart_obj,
                                        message[msg_size]);
                        msg_size++;
                        /* Check if size of the message  exceeds MAX_MESSAGE_SIZE
                         * (inclusive of the string terminating character '\0')
                         */
                        if (msg_size > (MAX_MESSAGE_SIZE - 1))
                        {
                            printf("\r\n\nMessage length exceeds 100 characters!!!"
                                   " Please enter a shorter message\r\nor edit the macro"
                                   " MAX_MESSAGE_SIZE to suit your message size\r\n");
                            msg_status = MESSAGE_ENTER_NEW;
                            break;
                        }
                    }
                }
                uart_result = cyhal_uart_getc(&cy_retarget_io_uart_obj,
                                              &message[msg_size],
                                              UART_TIMEOUT_MS);
                break;

            case MESSAGE_READY:
                /* Perform the message digest generation using SHA-256
                 * algorithm.
                 */
                crypto_status = Cy_Crypto_Core_Sha(CRYPTO, message, msg_size,
                                                   hash, CY_CRYPTO_MODE_SHA256);

                if(crypto_status == CY_CRYPTO_SUCCESS)
                {
                    printf("\r\n\nHash Value for the message:\r\n\n");
                    print_msg_digest(hash, MESSAGE_DIGEST_SIZE);
                }

                msg_status = MESSAGE_ENTER_NEW;
                break;
        }
    }
}

/*******************************************************************************
* Function Name: print_msg_digest()
********************************************************************************
* Summary: Function used to display the data in hexadecimal format
*
* Parameters:
*  uint8_t* data - Pointer to location of data to be printed
*  uint8_t  len  - length of data to be printed
*
* Return:
*  void
*
*******************************************************************************/
void print_msg_digest(uint8_t* data, uint8_t len)
{
    char print[10];
    for (uint32 i=0; i < len; i++)
    {
        if ((i % BYTES_PER_LINE) == 0)
        {
            printf("\r\n");
        }
        sprintf(print,"0x%02X ", *(data+i));
        printf("%s", print);
    }
    printf("\r\n");
    printf(SCREEN_HEADER1);
}

/* [] END OF FILE */
