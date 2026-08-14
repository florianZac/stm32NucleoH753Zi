/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define EN_LDO3_PSp_Pin GPIO_PIN_2
#define EN_LDO3_PSp_GPIO_Port GPIOE
#define PG_LDO3_PSp_Pin GPIO_PIN_3
#define PG_LDO3_PSp_GPIO_Port GPIOE
#define OC_LDO2_PSp_Pin GPIO_PIN_4
#define OC_LDO2_PSp_GPIO_Port GPIOE
#define EN_LDO2_PSp_Pin GPIO_PIN_5
#define EN_LDO2_PSp_GPIO_Port GPIOE
#define BLD_LDO1_PLn_Pin GPIO_PIN_6
#define BLD_LDO1_PLn_GPIO_Port GPIOE
#define BLD_LDO5_PSp_Pin GPIO_PIN_13
#define BLD_LDO5_PSp_GPIO_Port GPIOC
#define PG_LDO5_PSp_Pin GPIO_PIN_14
#define PG_LDO5_PSp_GPIO_Port GPIOC
#define BLD_LDO4_PSp_Pin GPIO_PIN_15
#define BLD_LDO4_PSp_GPIO_Port GPIOC
#define EN_LDO1_PSp_Pin GPIO_PIN_0
#define EN_LDO1_PSp_GPIO_Port GPIOF
#define BLD_LDO2_PSp_Pin GPIO_PIN_1
#define BLD_LDO2_PSp_GPIO_Port GPIOF
#define PG_LDO2_PSp_Pin GPIO_PIN_2
#define PG_LDO2_PSp_GPIO_Port GPIOF
#define EN_LDO5_PSn_Pin GPIO_PIN_3
#define EN_LDO5_PSn_GPIO_Port GPIOF
#define PG_LDO3_PSn_Pin GPIO_PIN_5
#define PG_LDO3_PSn_GPIO_Port GPIOF
#define OC_LDO5_PSp_Pin GPIO_PIN_6
#define OC_LDO5_PSp_GPIO_Port GPIOF
#define EN_LDO5_PSp_Pin GPIO_PIN_7
#define EN_LDO5_PSp_GPIO_Port GPIOF
#define BLD_LDO1_PSp_Pin GPIO_PIN_8
#define BLD_LDO1_PSp_GPIO_Port GPIOF
#define OC_LDO1_PSp_Pin GPIO_PIN_9
#define OC_LDO1_PSp_GPIO_Port GPIOF
#define EN_LDO4_PSp_Pin GPIO_PIN_0
#define EN_LDO4_PSp_GPIO_Port GPIOA
#define EN_LDO2_PLp_Pin GPIO_PIN_3
#define EN_LDO2_PLp_GPIO_Port GPIOA
#define PG_LDO4_PSp_Pin GPIO_PIN_4
#define PG_LDO4_PSp_GPIO_Port GPIOA
#define BLD_LDO2_PSn_Pin GPIO_PIN_5
#define BLD_LDO2_PSn_GPIO_Port GPIOA
#define EN_LDO2_PSn_Pin GPIO_PIN_6
#define EN_LDO2_PSn_GPIO_Port GPIOA
#define OC_LDO4_PSp_Pin GPIO_PIN_0
#define OC_LDO4_PSp_GPIO_Port GPIOB
#define BLD_LDO3_PSn_Pin GPIO_PIN_1
#define BLD_LDO3_PSn_GPIO_Port GPIOB
#define PG_LDO5_PSn_Pin GPIO_PIN_11
#define PG_LDO5_PSn_GPIO_Port GPIOF
#define BLD_LDO3_PLp_Pin GPIO_PIN_12
#define BLD_LDO3_PLp_GPIO_Port GPIOF
#define EN_LDO3_PLp_Pin GPIO_PIN_13
#define EN_LDO3_PLp_GPIO_Port GPIOF
#define OC_LDO4_PSn_Pin GPIO_PIN_14
#define OC_LDO4_PSn_GPIO_Port GPIOF
#define OC_LDO5_PSn_Pin GPIO_PIN_15
#define OC_LDO5_PSn_GPIO_Port GPIOF
#define EN_LDO1_PLn_Pin GPIO_PIN_0
#define EN_LDO1_PLn_GPIO_Port GPIOG
#define PG_LDO2_PLp_Pin GPIO_PIN_1
#define PG_LDO2_PLp_GPIO_Port GPIOG
#define PG_LDO4_PSn_Pin GPIO_PIN_9
#define PG_LDO4_PSn_GPIO_Port GPIOE
#define OC_LDO2_PLp_Pin GPIO_PIN_10
#define OC_LDO2_PLp_GPIO_Port GPIOE
#define BLD_LDO5_PSn_Pin GPIO_PIN_11
#define BLD_LDO5_PSn_GPIO_Port GPIOE
#define BLD_LDO2_PLn_Pin GPIO_PIN_12
#define BLD_LDO2_PLn_GPIO_Port GPIOE
#define EN_LDO3_PLn_Pin GPIO_PIN_13
#define EN_LDO3_PLn_GPIO_Port GPIOE
#define PG_LDO3_PLn_Pin GPIO_PIN_14
#define PG_LDO3_PLn_GPIO_Port GPIOE
#define PG_LDO3_PLp_Pin GPIO_PIN_15
#define PG_LDO3_PLp_GPIO_Port GPIOE
#define PG_LDO2_PSn_Pin GPIO_PIN_12
#define PG_LDO2_PSn_GPIO_Port GPIOB
#define PG_LDO1_PSn_Pin GPIO_PIN_8
#define PG_LDO1_PSn_GPIO_Port GPIOD
#define OC_LDO1_PLn_Pin GPIO_PIN_9
#define OC_LDO1_PLn_GPIO_Port GPIOD
#define BLD_LDO3_PLn_Pin GPIO_PIN_10
#define BLD_LDO3_PLn_GPIO_Port GPIOD
#define OC_LDO2_PLn_Pin GPIO_PIN_11
#define OC_LDO2_PLn_GPIO_Port GPIOD
#define BLD_LDO2_PLp_Pin GPIO_PIN_12
#define BLD_LDO2_PLp_GPIO_Port GPIOD
#define PG_LDO2_PLn_Pin GPIO_PIN_13
#define PG_LDO2_PLn_GPIO_Port GPIOD
#define BLD_LDO4_PSn_Pin GPIO_PIN_14
#define BLD_LDO4_PSn_GPIO_Port GPIOD
#define EN_LDO4_PSn_Pin GPIO_PIN_15
#define EN_LDO4_PSn_GPIO_Port GPIOD
#define OC_LDO3_PLp_Pin GPIO_PIN_4
#define OC_LDO3_PLp_GPIO_Port GPIOG
#define OC_LDO6_PSn_Pin GPIO_PIN_5
#define OC_LDO6_PSn_GPIO_Port GPIOG
#define PG_LDO6_PSn_Pin GPIO_PIN_6
#define PG_LDO6_PSn_GPIO_Port GPIOG
#define OC_LDO3_PLn_Pin GPIO_PIN_7
#define OC_LDO3_PLn_GPIO_Port GPIOG
#define EN_LDO6_PSn_Pin GPIO_PIN_8
#define EN_LDO6_PSn_GPIO_Port GPIOG
#define OC_LDO1_PSn_Pin GPIO_PIN_6
#define OC_LDO1_PSn_GPIO_Port GPIOC
#define EN_LDO1_PSn_Pin GPIO_PIN_8
#define EN_LDO1_PSn_GPIO_Port GPIOC
#define BLD_LDO1_PSn_Pin GPIO_PIN_9
#define BLD_LDO1_PSn_GPIO_Port GPIOC
#define EN_LDO2_PLn_Pin GPIO_PIN_10
#define EN_LDO2_PLn_GPIO_Port GPIOA
#define OC_LDO2_PSn_Pin GPIO_PIN_11
#define OC_LDO2_PSn_GPIO_Port GPIOA
#define EN_LDO6_PSp_Pin GPIO_PIN_10
#define EN_LDO6_PSp_GPIO_Port GPIOC
#define OC_LDO6_PSp_Pin GPIO_PIN_11
#define OC_LDO6_PSp_GPIO_Port GPIOC
#define PG_LDO6_PSp_Pin GPIO_PIN_12
#define PG_LDO6_PSp_GPIO_Port GPIOC
#define PG_LDO1_PLp_Pin GPIO_PIN_0
#define PG_LDO1_PLp_GPIO_Port GPIOD
#define PG_LDO1_PSp_Pin GPIO_PIN_1
#define PG_LDO1_PSp_GPIO_Port GPIOD
#define BLD_LDO6_PSp_Pin GPIO_PIN_2
#define BLD_LDO6_PSp_GPIO_Port GPIOD
#define OC_LDO3_PSp_Pin GPIO_PIN_6
#define OC_LDO3_PSp_GPIO_Port GPIOD
#define BLD_LDO3_PSp_Pin GPIO_PIN_7
#define BLD_LDO3_PSp_GPIO_Port GPIOD
#define PG_LDO1_PLn_Pin GPIO_PIN_9
#define PG_LDO1_PLn_GPIO_Port GPIOG
#define OC_LDO1_PLp_Pin GPIO_PIN_10
#define OC_LDO1_PLp_GPIO_Port GPIOG
#define BLD_LDO1_PLp_Pin GPIO_PIN_12
#define BLD_LDO1_PLp_GPIO_Port GPIOG
#define OC_LDO3_PSn_Pin GPIO_PIN_3
#define OC_LDO3_PSn_GPIO_Port GPIOB
#define EN_LDO3_PSn_Pin GPIO_PIN_4
#define EN_LDO3_PSn_GPIO_Port GPIOB
#define BLD_LDO6_PSn_Pin GPIO_PIN_0
#define BLD_LDO6_PSn_GPIO_Port GPIOE
#define EN_LDO1_PLp_Pin GPIO_PIN_1
#define EN_LDO1_PLp_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */
#define	WHO_AM_I				"STM32 NULCEO H753ZI IRIS V1.0\n"
#define DELTA_INIT_DVDD_AVDD	500
#define DELTA_INIT_AVDD_VREF	500
#define DELTA_INIT_VREF_NRST	500

#define DELTA_INIT_NRST_VREF	500
#define DELTA_INIT_VREF_AVDD	500
#define DELTA_INIT_AVDD_DVDD	500
//Ethernet constant
#define IP4_1					192
#define	IP4_2					168
#define	IP4_3					0
#define	IP4_4					123
#define UDP_SERVER_PORT			7
//DAC MIN and MAX VALUE
//LDO	1
#define	LDO_1PSp_VAL_MIN		0
#define	LDO_1PSp_VAL_MAX		0
#define	LDO_1PSp_CUR_MIN		0
#define	LDO_1PSp_CUR_MAX		0
#define	LDO_1PSn_VAL_MIN		0
#define	LDO_1PSn_VAL_MAX		0
#define	LDO_1PSn_CUR_MIN		0
#define	LDO_1PSn_CUR_MAX		0

struct LDO {
	_Bool		EN;
	_Bool		PG;
	_Bool		OC;
	_Bool		BLD;
	uint16_t	VAL;
	uint16_t	CURRENT;
	uint16_t	RET;
	uint16_t	RET_OFF;
};


//DAC CONSTANTS
// NOTE : HAL_I2C_Master_Transmit() attend l'adresse 7 bits DEJA decalee
// a gauche d'un bit (bits [7:1] du registre CR2->SADD). Les adresses
// datasheet (0x60/0x61/0x62) sont donc multipliees par 2 ici.

#define MCP4725_I2CADDR_DEFAULT (0x60 << 1)	//DAC1 & DAC2
#define MPC4725_I2CADDR_A0		(0x61 << 1)		//DAC3 & DAC5
#define MPC4725_I2CADDR_A1		(0x62 << 1)		//DAC4

//#define MCP4725_CMD_WRITEDAC	(0x40)
//#define MCP4725_CMD_WRITEDACEEPROM	(0x60)
#define MCP4725_VAL_DEF			(0x7FF)			//2047 ou 2.5 Volts
#define MCP4725_VAL_DEF_BIN		2047

#define	SPI_SIZE				24
#define	SPI_TIMEOUT				100
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
