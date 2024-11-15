/*
 * uart.c
 *
 *  Created on: Sep 26, 2023
 *      Author: HaHuyen
 */
#include "uart.h"

uint8_t receive_buffer1 = 0;
uint8_t msg[100];

uint8_t buffer [MAX_BUFFER_SIZE]={0};
uint8_t index_buffer=0;
uint8_t curr_index=0;
uint8_t buffer_flag=0;
uint8_t receive_msg=0;
uint8_t count_char=0;

void uart_init_rs232(){
	HAL_UART_Receive_IT(&huart1, &receive_msg, 1);
}

void uart_Rs232SendString(uint8_t* str){
	HAL_UART_Transmit(&huart1, (void*)msg, sprintf((void*)msg,"%s",str), 10);
}

void uart_Rs232SendBytes(uint8_t* bytes, uint16_t size){
	HAL_UART_Transmit(&huart1, bytes, size, 10);
}

void uart_Rs232SendNum(uint32_t num){
	if(num == 0){
		uart_Rs232SendString("0");
		return;
	}
    uint8_t num_flag = 0;
    int i;
	if(num < 0) uart_Rs232SendString("-");
    for(i = 10; i > 0; i--)
    {
        if((num / mypow(10, i-1)) != 0)
        {
            num_flag = 1;
            sprintf((void*)msg,"%d",num/mypow(10, i-1));
            uart_Rs232SendString(msg);
        }
        else
        {
            if(num_flag != 0)
            	uart_Rs232SendString("0");
        }
        num %= mypow(10, i-1);
    }
}

void uart_Rs232SendNumPercent(uint32_t num)
{
	sprintf((void*)msg,"%ld",num/100);
    uart_Rs232SendString(msg);
    uart_Rs232SendString(".");
    sprintf((void*)msg,"%ld",num%100);
    uart_Rs232SendString(msg);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart->Instance == USART1){
		// rs232 isr
		// can be modified
//		HAL_UART_Transmit(&huart1, &receive_msg, 1, 10);
		buffer[index_buffer++] = receive_msg;
		count_char += 1;
		if(index_buffer == 30) index_buffer = 0;

		buffer_flag = 1;
//		HAL_UART_Transmit(&huart1, &receive_msg, 1, 1000);
		HAL_UART_Receive_IT (&huart1 , &receive_msg , 1);
	}
}
