#include "bsp_uart.h"

uint8_t rec_data;
uint8_t rec_count = 0;
uint8_t rec_flag = 0;
uint8_t rec_buff[32];

void bsp_uart_start_receive(void)
{
    rec_count = 0;
    rec_flag = 0;
    memset(rec_buff, 0, sizeof(rec_buff));

    HAL_UART_Receive_IT(&huart1, &rec_data, 1);
}

void bsp_uart1_send_string(const char *str)
{
    if (str == NULL) {
        return;
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), 100);
}

static void bsp_uart_send_string(const char *str)
{
    bsp_uart1_send_string(str);
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        TIM3->CNT = 0;
        rec_flag = 1;

        if (rec_count < sizeof(rec_buff) - 1)
        {
            rec_buff[rec_count] = rec_data;
            rec_count++;
        }

        HAL_UART_Receive_IT(&huart1, &rec_data, 1);
    }
}

void bsp_uart_data_process(void)
{
    if (rec_flag)
    {
        if (TIM3->CNT > 2000)
        {
            if ((rec_count >= 3) &&
                (rec_buff[0] == 'l') &&
                (rec_buff[1] == 'a') &&
                (rec_buff[2] == 'n'))
            {
                bsp_uart_send_string("lan\r\n");
            }
            else if ((rec_count >= 4) &&
                     (rec_buff[0] == 'q') &&
                     (rec_buff[1] == 'i') &&
                     (rec_buff[2] == 'a') &&
                     (rec_buff[3] == 'o'))
            {
                bsp_uart_send_string("qiao\r\n");
            }
            else if ((rec_count >= 3) &&
                     (rec_buff[0] == 'b') &&
                     (rec_buff[1] == 'e') &&
                     (rec_buff[2] == 'i'))
            {
                bsp_uart_send_string("bei\r\n");
            }
            else
            {
                bsp_uart_send_string("error!\r\n");
            }

            rec_flag = 0;
            rec_count = 0;
            memset(rec_buff, 0, sizeof(rec_buff));
        }
    }
}
