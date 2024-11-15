/* USER CODE BEGIN Header */
/**
  ************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
#include "software_timer.h"
#include "led_7seg.h"
#include "button.h"
#include "lcd.h"
#include "picture.h"
#include "ds3231.h"
#include "uart.h"
/* Private typedef -----------------------------------------------------------*/
typedef enum {
    DISPLAY_MODE,
    SETTING_MODE,
    ALARM_MODE,
	UART_MODE
} ClockMode;

void system_init();
void displayModeHeader();
void switchMode();
void handleSettingMode();
void incrementCurrentSetting();
void savePendingValues();
void displayOnlySelectedParameter(uint8_t show);
void checkAlarmTrigger();
void turnOffAlarm();
void displayTime();
void updateTime();
void uart_handle();
uint8_t isNextParameterButtonPressed();
uint8_t isResetToSecondsButtonPressed();
uint8_t isButtonUpPressed();
uint8_t isButtonEnterPressed();
uint8_t isNextParameterButtonPressed();
uint8_t isResetToSecondsButtonPressed();
uint8_t isModeButtonPressed();


ClockMode currentMode = DISPLAY_MODE;
uint8_t currentSetting = 0; // Track which time parameter we're adjusting
uint8_t isSetting = 0; // Flag to toggle the blinking effect for settings

// Store the alarm time in separate variables
uint8_t alarm_hours = 6;
uint8_t alarm_minutes = 0;

// Temporary storage for setting mode adjustments
uint8_t pending_hours, pending_min, pending_sec;

/* USER CODE BEGIN 4 */
uint8_t alarm_set_hours = 0;
uint8_t alarm_set_minutes = 0;
uint8_t alarm_active = 1;       // Flag indicating if alarm is set
uint8_t alarm_triggered = 0;     // Flag indicating if alarm is currently ringing

uint8_t uart_hour_flag = 0;
uint8_t uart_minute_flag = 0;
uint8_t uart_hour = 0;
uint8_t count_request_uart = 0;
// Mode switching function
void switchMode() {
    currentMode = (currentMode + 1) % 4; // Cycle between DISPLAY_MODE, SETTING_MODE, ALARM_MODE
    lcd_Clear(BLACK);
    displayModeHeader();
    if (currentMode == SETTING_MODE) {
        // Load current time to pending values for modification
        ds3231_ReadTime();
        pending_hours = ds3231_hours;
        pending_min = ds3231_min;
        pending_sec = ds3231_sec;
    }
    if(currentMode == UART_MODE) {
    	// Print "Updating hour"
    	lcd_ShowStr(10, 30, "Updating hour...", GREEN, BLACK, 24, 1);
    	uart_hour_flag = 1;
    	count_request_uart = 0;
    }
}

// Display mode and header
void displayModeHeader() {
    lcd_ShowStr(10, 10, "Mode ", GREEN, BLACK, 24, 1);
    lcd_ShowIntNum(70, 10, currentMode, 1, GREEN, BLACK, 24);
}

// Increment parameter selector button
uint8_t isNextParameterButtonPressed() {
    return (button_count[15] == 1) ? 1 : 0;
}

// Reset to seconds parameter button
uint8_t isResetToSecondsButtonPressed() {
    return (button_count[11] == 1) ? 1 : 0;
}

// Increment value of the current parameter button
uint8_t isButtonUpPressed() {
    return (button_count[3] == 1 || button_count[3] >= 40) ? 1 : 0;
}

// Save parameter change button
uint8_t isButtonEnterPressed() {
    return (button_count[12] == 1) ? 1 : 0;
}

void handleSettingMode() {
    static uint32_t lastBlinkTime = 0;
    static uint32_t holdTime = 0;
    uint8_t holdInProgress = 0;

    // Blink the currently selected parameter at 2Hz
    if (HAL_GetTick() - lastBlinkTime >= 500) {
        isSetting = !isSetting;
        lastBlinkTime = HAL_GetTick();
    }

    // Display only the selected parameter to be adjusted
    displayOnlySelectedParameter(isSetting);

    // Cycle to the next parameter when next parameter button is pressed
    if (isNextParameterButtonPressed()) {
        currentSetting = (currentSetting + 1) % 3; // Cycles through 0 (seconds), 1 (minutes), 2 (hours)
    }

    // Reset current setting to seconds when reset button is pressed
    if (isResetToSecondsButtonPressed()) {
        currentSetting = 0; // Reset to seconds
    }

    // Increment behavior based on button press duration
    if(isButtonUpPressed()&&button_count[3] >= 40){
		incrementCurrentSetting();
	} else if(isButtonUpPressed()&&button_count[3] < 40){
		incrementCurrentSetting();
	}

    // Save the modified value only when pressing the Enter button
    if (isButtonEnterPressed()) {
        savePendingValues();
        currentSetting = (currentSetting + 1) % 3;
    }
}

// Increment the currently selected setting
void incrementCurrentSetting() {
    switch (currentSetting) {
        case 0: pending_sec = (pending_sec + 1) % 60; break;
        case 1: pending_min = (pending_min + 1) % 60; break;
        case 2: pending_hours = (pending_hours + 1) % 24; break;
    }
}

// Save modified values to RTC
void savePendingValues() {
    if (currentMode == SETTING_MODE) {
        // Save current time settings
        ds3231_Write(ADDRESS_HOUR, pending_hours);
        ds3231_Write(ADDRESS_MIN, pending_min);
        ds3231_Write(ADDRESS_SEC, pending_sec);
    } else if (currentMode == ALARM_MODE) {
        // Save alarm settings
        alarm_set_hours = pending_hours;
        alarm_set_minutes = pending_min;
        alarm_active = 1; // Re-enable the alarm after setting
    }
}
// Display only the selected parameter, hide others
void displayOnlySelectedParameter(uint8_t show) {
    uint16_t secColor = (currentSetting == 0 && show) ? GREEN : BLACK;
    uint16_t minColor = (currentSetting == 1 && show) ? GREEN : BLACK;
    uint16_t hourColor = (currentSetting == 2 && show) ? GREEN : BLACK;

    lcd_ShowIntNum(70, 100, pending_hours, 2, hourColor, BLACK, 24);
    lcd_ShowIntNum(110, 100, pending_min, 2, minColor, BLACK, 24);
    lcd_ShowIntNum(150, 100, pending_sec, 2, secColor, BLACK, 24);
}

// Detect if mode button is pressed
uint8_t isModeButtonPressed() {
    return (button_count[0] == 1) ? 1 : 0;
}

/* USER CODE END 4 */
void checkAlarmTrigger() {
    if (alarm_active && !alarm_triggered && (ds3231_hours == alarm_set_hours) && (ds3231_min == alarm_set_minutes)) {
        alarm_triggered = 1; // Set the alarm trigger flag
    }
}
void turnOffAlarm() {
    if (button_count[4] == 1) { // Button to turn off alarm
        alarm_triggered = 0;
        alarm_active = 0;

        lcd_Fill(20, 160, 240, 184, BLACK);// Disable alarm until reset
    }
}
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_SPI1_Init();
    MX_FSMC_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    /* Initialize peripherals and set initial time */
    system_init();

    /* Main loop */
    lcd_Clear(BLACK);
    updateTime();
    while (1)
    {
        while(!flag_timer2);
        flag_timer2 = 0;
        button_Scan();

        // Check if mode button is pressed and switch modes
        if (isModeButtonPressed()) {
            switchMode();
        }

        // Main functionality for each mode
        switch (currentMode) {
            case DISPLAY_MODE:
                ds3231_ReadTime();
                displayTime();
                checkAlarmTrigger();  // Check if alarm should trigger
                turnOffAlarm();       // Check if alarm should be turned off
                break;

            case SETTING_MODE:
                handleSettingMode();
                break;

            case ALARM_MODE:
            	handleSettingMode();
                break;
            case UART_MODE:
            	uart_handle();
            	break;
            default:
            	break;
        }
    }
}


  /* USER CODE END 3 */


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void system_init(){
	  HAL_GPIO_WritePin(OUTPUT_Y0_GPIO_Port, OUTPUT_Y0_Pin, 0);
	  HAL_GPIO_WritePin(OUTPUT_Y1_GPIO_Port, OUTPUT_Y1_Pin, 0);
	  HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port, DEBUG_LED_Pin, 0);
	  timer_init();
	  led7_init();
	  button_init();
	  lcd_init();
	  ds3231_init();
	  uart_init_rs232();
	  setTimer2(50);

}


void test_7seg(){
	led7_SetDigit(0, 0, 0);
	led7_SetDigit(5, 1, 0);
	led7_SetDigit(4, 2, 0);
	led7_SetDigit(7, 3, 0);
}
void test_button(){
	for(int i = 0; i < 16; i++){
		if(button_count[i] == 1){
			led7_SetDigit(i/10, 2, 0);
			led7_SetDigit(i%10, 3, 0);
		}
	}
}

void updateTime(){
	ds3231_Write(ADDRESS_YEAR, 24);
	ds3231_Write(ADDRESS_MONTH, 11);
	ds3231_Write(ADDRESS_DATE, 13);
	ds3231_Write(ADDRESS_DAY, 6);
	ds3231_Write(ADDRESS_HOUR, 13);
	ds3231_Write(ADDRESS_MIN, 16);
	ds3231_Write(ADDRESS_SEC, 24);
}

uint8_t isButtonUp()
{
    if (button_count[3] == 1)
        return 1;
    else
        return 0;
}
uint8_t isButtonDown()
{
    if (button_count[7] == 1)
        return 1;
    else
        return 0;
}
void displayTime(){
	displayModeHeader();
	lcd_ShowIntNum(70, 100, ds3231_hours, 2, GREEN, BLACK, 24);
	lcd_ShowIntNum(110, 100, ds3231_min, 2, GREEN, BLACK, 24);
	lcd_ShowIntNum(150, 100, ds3231_sec, 2, GREEN, BLACK, 24);
	lcd_ShowIntNum(20, 130, ds3231_day, 2, YELLOW, BLACK, 24);
	lcd_ShowIntNum(70, 130, ds3231_date, 2, YELLOW, BLACK, 24);
	lcd_ShowIntNum(110, 130, ds3231_month, 2, YELLOW, BLACK, 24);
	lcd_ShowIntNum(150, 130, ds3231_year, 2, YELLOW, BLACK, 24);


	  if (alarm_triggered) {
		  lcd_ShowStr(20, 160, "Alarm Ring Ring", RED, BLACK, 24, 1); // Display in red below other data
	  }
}

void uart_handle() {
	if(uart_hour_flag == 1) {
		uart_hour_flag = 0;
		uart_Rs232SendString("Hour(format h--e)\n");
		setTimer3(10000);
	}
	if(flag_timer3) {
		uart_hour_flag = 1;
		count_request_uart += 1;
		count_char = 0;
		curr_index = index_buffer;
		flag_timer3 = 0;
	}
	if(count_request_uart > 2) {
		count_request_uart = 0;
		uart_Rs232SendString("No response\n");
		currentMode = DISPLAY_MODE;
		count_char = 0;
		curr_index = index_buffer;
		lcd_Clear(BLACK);
		return;
	}
	if(buffer_flag == 1 && count_char == 4) {
		if(buffer[curr_index] == '#' && buffer[curr_index+3] == '!') {
			if(buffer[curr_index+1] >= '0' && buffer[curr_index+1] <= '9' && buffer[curr_index+2] >= '0' && buffer[curr_index+2] <= '9') {
				uart_hour = (buffer[curr_index+1]-'0')*10 + (buffer[curr_index+2]-'0');
				if(uart_hour >= 0 && uart_hour < 24) {
					ds3231_Write(ADDRESS_HOUR, uart_hour);
					uart_Rs232SendString("Hour update successfully\n");
					uart_Rs232SendNum(uart_hour);
					currentMode = DISPLAY_MODE;
					lcd_Clear(BLACK);
				}
				else {
					// Print data out of range
					uart_Rs232SendString("Data out of range\n");
					count_request_uart = 0;
				}
			}
			else {
				// Print Error
				uart_Rs232SendString("Error\n");
				count_request_uart = 0;
			}
		}
		else {
			// Print Wrong format
			uart_Rs232SendString("Wrong format\n");
			count_request_uart = 0;
		}

		buffer_flag = 0;
		curr_index = index_buffer;
		count_char = 0;
	}
//		if(uart_minute_flag == 1) {
//			uart_Rs232SendString("Request Minute");
//			if(buffer_flag == 1) {
//				uart_minute_flag = 0;
//				ds3231_Write(ADDRESS_MIN, receive_msg);
//				buffer_flag = 0;
//			}
//		}
//		currentMode = DISPLAY_MODE;
//	}

//	if(button_count[12] == 1){
//		uart_Rs232SendNum(ds3231_hours);
//		uart_Rs232SendString(":");
//		uart_Rs232SendNum(ds3231_min);
//		uart_Rs232SendString(":");
//		uart_Rs232SendNum(ds3231_sec);
//		uart_Rs232SendString("\n");
//	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
