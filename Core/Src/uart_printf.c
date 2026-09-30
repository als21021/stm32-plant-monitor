/*
 * uart_printf.c
 *
 *  Created on: 2026. 9. 30.
 *      Author: dotol
 */

#include "uart_printf.h"
#include "usart.h"
#include <stdio.h>

int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

void uart_printf_init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
}
