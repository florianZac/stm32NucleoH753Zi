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
// Valeur d'initialisation de chaque sortie DAC au demarrage ET valeur restauree
// par la boucle periodique de StartLwipTask() apres tout code operateur qui ne
// passe pas par VAL_LDO[]/ldoConfig[].ldo->VAL (ex: juste apres un balayage a
// distance BALAYER_DAC, dont la cible n'est pas forcement une LDO configuree) :
// 0V, jamais une tension intermediaire, par securite (aucune sortie ne doit
// "sortir" une tension non voulue par l'operateur simplement parce qu'elle n'a
// jamais ete explicitement configuree).
#define MCP4725_VAL_DEF			(0x000)			//0 code = 0V, quelle que soit VDD
#define MCP4725_VAL_DEF_BIN		0

#define	SPI_SIZE				24
#define	SPI_TIMEOUT				100

// ---------------------------------------------------------------------------
// Protection surintensite (OC) des LDO equipees du circuit shunt R97 +
// OP184 + LM211 (aujourd'hui 1PSp et 2PSp). Revision du 13/10/2026.
//
// CARTE_LIM_CORRIGEE : 1 = carte corrigee (R96 = R98 = 26,1 kOhm 0,1 %,
// C178 = C356 = 10 nF C0G, R103 = 10 kOhm, RH = 680 kOhm),
//                      0 = carte actuelle (R96 = R98 = 100 kOhm, 100 nF).
// C'est le SEUL define a changer pour passer d'une carte a l'autre.
// ---------------------------------------------------------------------------
#define CARTE_LIM_CORRIGEE		1

#if CARTE_LIM_CORRIGEE
#define OC_K_V_PAR_A			2.61f	// G x R97 = 26,1 x 0,1 Ohm (V/A)
#define OC_BLANKING_MS			10U		// soft-start + 5 tau (tau = 261 us)
#define OC_CODE_SEUIL_MAX		4006U	// 1,90 A : pire cas 1,951 A < 2,0 A
#define OC_CODE_SEUIL_DEFAUT	1056U	// 0,50 A au demarrage (hysteresis incluse)
#else
#define OC_K_V_PAR_A			10.0f	// G x R97 = 100 x 0,1 Ohm (V/A)
#define OC_BLANKING_MS			60U		// soft-start + 5 tau (tau = 10 ms)
#define OC_CODE_SEUIL_MAX		4095U	// 0,50 A, pleine echelle du DAC
#define OC_CODE_SEUIL_DEFAUT	4095U	// 0,50 A au demarrage (500 mA = 4096, ramene a 4095)
#endif

// Code minimal du seuil : 0,5 V = limite basse d'entree du LM211
// (0,5 x 4096 / 5 = 409,6 -> 410), identique pour les deux cartes.
#define OC_CODE_SEUIL_MIN		410U

// Niveau de la broche OC qui signifie "surintensite". Schema PS2+ : IN+ du
// LM211 = VMES (sortie OP184 via R103), IN- = DAC_OUT, donc OC = 1 en
// surintensite. A passer a GPIO_PIN_RESET uniquement si l'essai T5 montre
// la polarite inverse.
#define OC_NIVEAU_DEFAUT		GPIO_PIN_SET

// ---------------------------------------------------------------------------
// Source du seuil de limitation de courant (revision du 21/10/2026).
// 0 = seuil fixe par le DAC de courant (MCP47FEB28, voie "DAC courant" de la
//     ligne de ldoConfig[] : 0xC0 voie 5 pour 2PSp). Cas de la carte actuelle.
// 1 = seuil fixe par un potentiometre (R104 / RV2 montes, DAC_OUT deconnecte
//     de IN- du LM211) : la voie du DAC de courant est mise en haute impedance
//     (Power-Down 11), SET_CURRENT_LDO_ est refuse et l'EN n'attend plus de
//     seuil DAC valide.
// Valeur par defaut de la colonne "seuil" de ldoConfig[] (SEUIL_SOURCE) : une
// LDO peut aussi etre forcee individuellement a SEUIL_DAC ou SEUIL_POT.
// ---------------------------------------------------------------------------
#define LIMITATION_PAR_POTENTIOMETRE	0

// ---------------------------------------------------------------------------
// Memoire des codes DAC (tension + seuil de courant) dans l'EEPROM du
// MCP47FEB28 (registres non volatils 10h-17h), pour les LDO declarees MEM_EEPROM
// dans ldoConfig[]. Au demarrage le DAC recharge lui-meme ces valeurs et le
// firmware les relit ; chaque nouvelle valeur (SET_VAL_LDO_, SET_CURRENT_LDO_)
// est ecrite apres DAC_EEPROM_DELAI_MS sans changement (une rampe n'use pas
// l'EEPROM), et immediatement apres un declenchement OC.
// DAC_EEPROM_TWC_MS : duree d'ecriture EEPROM, tWC = 16 ms max (DS20006368A).
// ---------------------------------------------------------------------------
#define DAC_EEPROM_TWC_MS		20U
#define DAC_EEPROM_DELAI_MS		2000U
#define DAC_EEPROM_ESSAIS_MAX	3U
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
