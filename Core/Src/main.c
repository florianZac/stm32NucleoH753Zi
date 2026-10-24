/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "lwip/udp.h"
// Configuration des LDO GENEREE depuis configurationLDO/ldo_config.csv par
// configurationLDO/generer_config.py (etape pre-build) : LDO_NB, empreinte.
// Les lignes de ldoConfig[] sont dans ldo_config_lignes.inc (meme generateur).
#include "ldo_config_genere.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* Alias vers "struct LDO" (defini dans main.h) pour usage dans
   la table de configuration generique LDO_Config_t ci-dessous. */
typedef struct LDO LDO_TypeDef;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart1;

osThreadId lwipTaskHandle;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC3_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC2_Init(void);
void StartLwipTask(void const * argument);

/* USER CODE BEGIN PFP */
void udpServer_init(void);
void udp_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
void tc_request(void);
int map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max);
void EXTI15_10_IRQHandler(void);
void EXTI0_IRQHandler(void);
// Declaree tot : appelee depuis ExecuterCaptureComplete() (ligne ~763),
// alors que la definition complete de GetMesureReelle() se trouve bien
// plus loin dans ce fichier (proche de h_WHO).
static void GetMesureReelle(int idx, const char *nom);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
    Author      : F.Aizac
    Date        : 11/08/2026
    Revision    : 1
    Commentaire :
    Git         :
*/

// Declaration anticipee : UART1_EnvoyerTexte() est definie plus bas
// dans ce fichier (avec l'init USART1), mais utilisee plus tot par
// BalayerSortieDAC() pour rapporter chaque palier en direct.
void UART1_EnvoyerTexte(const char *texte);   /* non statique : appelee aussi par ethernetif.c */
static uint8_t LDO_RelacherBleeder(int idx);  /* definie avec la protection OC, utilisee par les handlers BL */
static void LDO_GPIO_Init(void);              /* broches des LDO depuis ldoConfig[] (CSV), apres MX_GPIO_Init() */
static void h_GET_CONFIG_LDO(void);           /* "GET_CONFIG_LDO" : NB et empreinte de la configuration */
static uint8_t LDO_BrocheBouton(GPIO_TypeDef *port, uint16_t pin);   /* PC13 / PD0 : boutons */
static void Commande_Verrouiller(void);       /* verrou commun UDP / UART autour de tc_request() */
static void Commande_Deverrouiller(void);


// Etat logiciel courant de chacune des 18 LDO de la carte (En/Bl/OC/PG/
// Voltage/Current/delta_ON/delta_OFF). Mis a jour a chaque iteration
// de la boucle principale (ancien mecanisme, cf. StartLwipTask()).
//
// IMPORTANT - nature reelle du champ OC : ce n'est PAS un defaut
// absolu, c'est la sortie d'un COMPARATEUR dont le seuil de
// declenchement depend directement de "Current" (le code ecrit par
// SET_CURRENT_LDO_<nom>). Chaine materielle :
//   1) Shunt R97 = 100 mOhm en sortie du LDO.
//   2) U17 OP184 (soustracteur) : IN+ cote LDO du shunt (R94 1k / R96),
//      IN- cote charge (R95 1k / R98) -> VMES = OC_K_V_PAR_A x I
//      (10 V/A carte actuelle, 2,61 V/A carte corrigee), point TP24.
//      R100 (10k) = charge de sortie de l'AOP, pas un pont de seuil.
//   3) U2 LM211 (comparateur, sortie = OC) : IN+ (broche 2) = VMES via
//      R103 ; IN- (broche 3) = DAC_OUT (MCP47FEB28 0xC0, voie du courant)
//      qui fixe le SEUIL. Pont R104 (3k vers 5V) / RV2 (100k) : reglage
//      manuel, a ne pas monter quand le DAC est utilise. Sortie a
//      collecteur ouvert tiree par R109 (5,1k) vers 3,3 V.
// Consequence : OC=1 signifie "le courant reel depasse le seuil programme
// par Current" (polarite a confirmer par l'essai T5, voir OC_NIVEAU_DEFAUT).
// Seuil (A) = Current x 5 V / 4096 / OC_K_V_PAR_A (hors hysteresis).
// Depuis la revision du 13/10/2026, un depassement coupe l'EN de la LDO
// et reste verrouille jusqu'a RST_OC_LDO_<nom> (voir OC_Verrouiller()).
// Une struct LDO par ligne de ldoConfig[] (meme index). La ligne generee
// pointe vers son etat : &ldoEtat[i]. Tout a 0 au demarrage (EN, BL, codes).
static struct LDO ldoEtat[LDO_NB];

// Message UDP brut en cours de traitement (pointeur dans le buffer LwIP
// recu) et reponse texte a renvoyer a l'expediteur.
char 		*last_message;
char 		response[150] 		= "STM32 connect";

// Delais de sequencement (en ms) entre les differentes etapes de mise
// sous/hors tension des rails (DVDD, AVDD, VREF, NRST). Valeurs
// modifiables par commande reseau SET_DELTA_*, reinitialisables par
// RST_DELTA_* (voir tc_request()).
uint16_t	delta_dvdd_avdd		= DELTA_INIT_DVDD_AVDD;
uint16_t	delta_avdd_vref		= DELTA_INIT_AVDD_VREF;
uint16_t	delta_vref_nrst		= DELTA_INIT_VREF_NRST;
uint16_t	delta_nrst_vref		= DELTA_INIT_NRST_VREF;
uint16_t	delta_vref_avdd		= DELTA_INIT_VREF_AVDD;
uint16_t	delta_avdd_dvdd		= DELTA_INIT_AVDD_DVDD;

// Etat logiciel cible des 18 LDO, indexe dans le meme ordre que
// ldoConfig[] (0=1PSp, 1=1PSn, ... 17=3PLn). EN_LDO/BL_LDO pilotent
// directement les GPIO Enable/Bleeder ; VAL_LDO/CURRENT_LDO pilotent
// les DAC via les tableaux DACx_DEF ci-dessous.
volatile _Bool	EN_LDO[LDO_NB] 			= {0};
// volatile : BL_LDO[] est aussi ecrit par l'interruption OC (OC_Verrouiller()
// met le bleeder a 1 au declenchement).
volatile _Bool	BL_LDO[LDO_NB] 			= {0};
uint16_t	VAL_LDO[LDO_NB]		= {0};   /* MCP4725_VAL_DEF_BIN = 0 */

uint16_t	CURRENT_LDO[LDO_NB]		= {0};   /* MCP4725_VAL_DEF_BIN = 0 */

// Buffers de trame envoyes en continu par la boucle de StartLwipTask()
// (HAL_I2C_Master_Transmit direct, 24 octets = 8 ecritures chainees).
// Chaque groupe de 3 octets : {COMMANDE = registre << 3, VAL_MSB, VAL_LSB},
// meme format que EcrireDACVolatile()/BalayerSortieDAC() (MCP47FEB28).
// DAC1_DEF : I2C1, adresse 0xC0 (0x60 en 7 bits). Montage du 22/09/2026
// (tout sur I2C1 suite a un probleme sur I2C2) : 1PSp et 2PSp. Registres
// 0, 2, 6, 7 : libres, non cables. La 2e puce cablee sur I2C1 (0xC2, 1PLp)
// utilise DAC3_DEF, voir plus bas.
uint8_t		DAC1_DEF[24]			= {(0x00<<3),0x00, 0x00,		//libre
									(0x01<<3),0x00, 0x00,		//1PSp	COURANT
									(0x02<<3),0x00, 0x00,		//libre
									(0x03<<3),0x00, 0x00,		//1PSp	TENSION
									(0x04<<3),0x00, 0x00,		//2PSp	TENSION
									(0x05<<3),0x00, 0x00,		//2PSp	COURANT
									(0x06<<3),0x00, 0x00,		//libre
									(0x07<<3),0x00, 0x00};		//libre

// ATTENTION FORMAT (corrige le 24/09/2026) : TOUTES les puces sont des
// MCP47FEB28. L'octet de commande de chaque groupe doit etre (registre << 3)
// (AD4:AD0 = registre, C1:C0 = 00 = Write, X = 0), exactement comme DAC1_DEF.
// Les anciens octets 0x01..0x07 (format MCP4725) etaient interpretes par le
// MCP47FEB28 comme des commandes invalides / des ecritures sur le registre 0 :
// c'est pour cela que SET_VAL_LDO_1PLp ne faisait jamais varier la sortie.
//
// DAC2_DEF : I2C2, 0xC0 (bus I2C2 desactive, voir i2c2_actif). Tout a 0V par
// securite (l'ancien 0x0FFF sur le registre 3 sortait la pleine echelle des
// que I2C2 serait reactive).
uint8_t		DAC2_DEF[24]			= {(0x00<<3),0x00, 0x00,
									(0x01<<3),0x00, 0x00,
									(0x02<<3),0x00, 0x00,
									(0x03<<3),0x00, 0x00,
									(0x04<<3),0x00, 0x00,
									(0x05<<3),0x00, 0x00,
									(0x06<<3),0x00, 0x00,
									(0x07<<3),0x00, 0x00};

// DAC3_DEF : I2C1, adresse 0xC2 (0x61 en 7 bits, MPC4725_I2CADDR_A0).
// Montage du 22/09/2026 : sortie 0 = 1PLp TENSION (pas de DAC courant sur les
// PLp). Registres 1 a 7 : libres, non cables.
uint8_t		DAC3_DEF[24]			= {(0x00<<3),0x00, 0x00,		//1PLp	TENSION
									(0x01<<3),0x00, 0x00,		//libre
									(0x02<<3),0x00, 0x00,		//libre
									(0x03<<3),0x00, 0x00,		//libre
									(0x04<<3),0x00, 0x00,		//libre
									(0x05<<3),0x00, 0x00,		//libre
									(0x06<<3),0x00, 0x00,		//libre
									(0x07<<3),0x00, 0x00};		//libre

// DAC4_DEF : I2C2, 0xC4 (bus I2C2 desactive).
uint8_t		DAC4_DEF[24]			= {(0x00<<3),0x00, 0x00,
									(0x01<<3),0x00, 0x00,
									(0x02<<3),0x00, 0x00,
									(0x03<<3),0x00, 0x00,
									(0x04<<3),0x00, 0x00,
									(0x05<<3),0x00, 0x00,
									(0x06<<3),0x00, 0x00,
									(0x07<<3),0x00, 0x00};

// DAC5_DEF : I2C2, 0xC2 (bus I2C2 desactive).
uint8_t		DAC5_DEF[24]			= {(0x00<<3),0x00, 0x00,
									(0x01<<3),0x00, 0x00,
									(0x02<<3),0x00, 0x00,
									(0x03<<3),0x00, 0x00,
									(0x04<<3),0x00, 0x00,
									(0x05<<3),0x00, 0x00,
									(0x06<<3),0x00, 0x00,
									(0x07<<3),0x00, 0x00};

/* ===========================================================================
 *  TABLES DE CONFIGURATION DE LA CARTE - revision du 20/10/2026
 * ===========================================================================
 *  TOUT ce qui decrit le cablage est regroupe ici, dans deux tableaux :
 *
 *    1) tableDac[]  : une ligne par puce MCP47FEB28 (cible logique, bus I2C,
 *                     adresse, trame DACx_DEF rafraichie en continu).
 *    2) ldoConfig[] : une ligne par LDO (meme index partout, 0 = 1PSp ...
 *                     17 = 3PLn) : broches EN/BLD/OC/PG, DAC tension et DAC
 *                     courant (bus/puce + voie), protection surintensite,
 *                     bleeder, voies initiales des multiplexeurs de mesure.
 *
 *  Pour cabler / modifier une LDO : on ne touche QUE sa ligne de ldoConfig[].
 *  Les broches viennent des #define CubeMX de main.h (EN_LDOx_yyy_Pin, ...),
 *  via la macro BROCHES_LDO(). Les seuils par defaut viennent de main.h
 *  (OC_CODE_SEUIL_*, OC_BLANKING_MS, OC_K_V_PAR_A, OC_NIVEAU_DEFAUT).
 *
 *  Le fonctionnement du DAC de TENSION est inchange : memes trames DACx_DEF,
 *  meme ordre d'emission, meme mapping initial (copie dans mappingDacLdo[]
 *  au demarrage, toujours modifiable a chaud par SET_DAC_MAPPING_LDO_).
 * ======================================================================== */

#define NB_LDO LDO_NB   /* nombre de lignes de configurationLDO/ldo_config.csv */

/**
  * @brief  Identifie quel tableau DACx_DEF (donc quelle puce) est vise.
  *         CIBLE_AUCUNE = pas de DAC associe (ex : courant des LDO *PLp).
  *         Le numero (1-5) est aussi celui de SET_DAC_MAPPING_LDO_<nom>.
  */
typedef enum {
    CIBLE_AUCUNE = 0,
    CIBLE_DAC1,
    CIBLE_DAC2,
    CIBLE_DAC3,
    CIBLE_DAC4,
    CIBLE_DAC5,
} CibleDac_t;

/**
  * @brief  Correspondance DAC <-> LDO d'une LDO : puce et voie (0-7) pour la
  *         tension, idem pour le courant (CIBLE_AUCUNE si non applicable).
  *         Copie RAM de ldoConfig[], modifiable a chaud (SetMappingDacLdo()).
  */
typedef struct {
    CibleDac_t dac_tension;
    uint8_t    canal_tension;
    CibleDac_t dac_courant;
    uint8_t    canal_courant;
} MappingDacLdo_t;

/**
  * @brief  Une puce MCP47FEB28 rafraichie en continu par StartLwipTask().
  */
typedef struct {
    CibleDac_t          cible;        /**< CIBLE_DAC1..CIBLE_DAC5                      */
    I2C_HandleTypeDef  *hi2c;         /**< &hi2c1 ou &hi2c2                            */
    uint8_t             adresse_hal;  /**< adresse 7 bits decalee (0xC0, 0xC2, 0xC4)   */
    uint8_t            *trame;        /**< DACx_DEF (8 x {registre<<3, MSB, LSB})      */
} DacPuce_t;

// ---------------------------------------------------------------------------
// TABLE 1 : PUCES DAC. L'ordre des lignes = ordre d'emission dans la boucle
// (identique a l'ancien code). Une puce sur hi2c2 n'est emise que si
// i2c2_actif = 1 (SET_I2C2_ACTIF:1).
// ---------------------------------------------------------------------------
static const DacPuce_t tableDac[] = {
    //  cible        bus     adresse                   trame
    { CIBLE_DAC1, &hi2c1, MCP4725_I2CADDR_DEFAULT, DAC1_DEF },  // 0xC0 : 1PSp V=3 I=1, 2PSp V=4 I=5
    { CIBLE_DAC2, &hi2c2, MCP4725_I2CADDR_DEFAULT, DAC2_DEF },  // 0xC0 sur I2C2 (inactif)
    { CIBLE_DAC3, &hi2c1, MPC4725_I2CADDR_A0,      DAC3_DEF },  // 0xC2 : 1PLp V=0
    { CIBLE_DAC4, &hi2c2, MPC4725_I2CADDR_A1,      DAC4_DEF },  // 0xC4 sur I2C2 (inactif)
    { CIBLE_DAC5, &hi2c2, MPC4725_I2CADDR_A0,      DAC5_DEF },  // 0xC2 sur I2C2 (inactif)
};
#define NB_DAC_PUCES   (sizeof(tableDac) / sizeof(tableDac[0]))

/**
  * @brief  Parametres de la protection surintensite d'une LDO (circuit shunt
  *         R97 + OP184 + LM211, sortie OC sur une entree GPIO).
  */
typedef struct {
    uint8_t        surveille;      /**< 1 = OC lu et coupure de l'EN sur defaut        */
    uint8_t        interruption;   /**< 1 = broche OC en EXTI (coupure en quelques us) */
    GPIO_PinState  niveau_defaut;  /**< niveau de la broche OC = surintensite          */
    uint16_t       seuil_defaut;   /**< code DAC programme au demarrage                */
    uint16_t       seuil_min;      /**< code minimal accepte par SET_CURRENT_LDO_      */
    uint16_t       seuil_max;      /**< code maximal accepte par SET_CURRENT_LDO_      */
    uint16_t       blanking_ms;    /**< masquage apres activation de l'EN (soft-start) */
    uint16_t       k_mv_par_a;     /**< gain VMES/I de la chaine (mV/A), diagnostic    */
} OcConfig_t;

/**
  * @brief  Description complete d'une LDO : UNE ligne de ldoConfig[].
  */
typedef struct {
    const char      *name;              /**< nom des commandes (SET_EN_LDO_<name>)      */
    GPIO_TypeDef    *en_port;           /**< Enable (sortie)                            */
    uint16_t         en_pin;
    GPIO_TypeDef    *bld_port;          /**< Bleeder (sortie)                           */
    uint16_t         bld_pin;
    GPIO_TypeDef    *oc_port;           /**< Over-Current (entree)                      */
    uint16_t         oc_pin;
    GPIO_TypeDef    *pg_port;           /**< Power-Good (entree)                        */
    uint16_t         pg_pin;
    LDO_TypeDef     *ldo;               /**< etat logiciel (struct LDO)                 */
    CibleDac_t       dac_tension;       /**< puce du DAC de tension                     */
    uint8_t          canal_tension;     /**< voie 0-7 du DAC de tension                 */
    CibleDac_t       dac_courant;       /**< puce du DAC de seuil de courant            */
    uint8_t          canal_courant;     /**< voie 0-7 du DAC de seuil de courant        */
    OcConfig_t       oc;                /**< protection surintensite                    */
    GPIO_PinState    bld_niveau_actif;  /**< niveau de BLD qui fait conduire le bleeder */
    uint8_t          bld_coupe_en;      /**< 1 = bleeder actif -> EN coupe (protection de la sortie) */
    uint8_t          bld_sur_coupure_oc;/**< 1 = bleeder force tant que le defaut OC est verrouille */
    uint8_t          mux_tension;       /**< voie initiale mux mesure tension (MUX_NC)  */
    uint8_t          mux_courant;       /**< voie initiale mux mesure courant (MUX_NC)  */
    uint8_t          seuil_par_pot;     /**< SEUIL_DAC (0) ou SEUIL_POT (1) : source du seuil OC          */
    uint8_t          memoire_dac;       /**< MEM_EEPROM (1) : codes DAC sauvegardes/restaures en EEPROM */
    uint8_t          cmd_bld_defaut;    /**< CMD_ON_OFF_BLD du CSV : 1 = bleeder ON au 1er demarrage    */
    uint8_t          cmd_oc_defaut;     /**< CMD_ON_OFF_OC du CSV : 1 = protection OC ON au 1er demarrage */
} LDO_Config_t;

/* --- Raccourcis d'ecriture des lignes de ldoConfig[] ---------------------- */

/** Les 4 broches d'une LDO a partir de son suffixe CubeMX (ex : 2_PSp). */
#define BROCHES_LDO(s)   EN_LDO##s##_GPIO_Port,  EN_LDO##s##_Pin,  \
                         BLD_LDO##s##_GPIO_Port, BLD_LDO##s##_Pin, \
                         OC_LDO##s##_GPIO_Port,  OC_LDO##s##_Pin,  \
                         PG_LDO##s##_GPIO_Port,  PG_LDO##s##_Pin

/** Gain de la chaine de mesure en mV/A (2610 carte corrigee, 10000 sinon). */
#define OC_K_MV_PAR_A    ((uint16_t)(OC_K_V_PAR_A * 1000.0f + 0.5f))

/** LDO equipee du circuit shunt + OP184 + LM211. it = 1 : broche OC en EXTI. */
#define OC_LM211(it)     { 1U, (it), OC_NIVEAU_DEFAUT, OC_CODE_SEUIL_DEFAUT, \
                           OC_CODE_SEUIL_MIN, OC_CODE_SEUIL_MAX, OC_BLANKING_MS, OC_K_MV_PAR_A }

/** LDO sans protection : OC affiche seulement, jamais de coupure. */
#define OC_SANS          { 0U, 0U, OC_NIVEAU_DEFAUT, 0U, 0U, 4095U, 0U, 0U }

// Bleeder = protection de la sortie : avec "coupe EN" = 1, BL=1 coupe l'EN
// (le LDO ne peut plus fournir) puis R108 decharge la sortie a 0 V ; l'EN
// reste coupe apres RST_BL_LDO_ (renvoyer SET_EN_LDO_). Avec "sur OC" = 1,
// le bleeder est force tant qu'un defaut OC est verrouille (RST_OC_LDO_).
#define BLD_HAUT         GPIO_PIN_SET     /**< bleeder actif sur BLD = 1 (IRF7821 attaque en direct) */
#define BLD_BAS          GPIO_PIN_RESET   /**< bleeder actif sur BLD = 0 (etage inverseur)           */
#define MUX_NC           0xFFU            /**< voie de multiplexeur non cablee                      */

/** Source du seuil de courant (colonne "seuil"). SEUIL_SOURCE suit le define
 *  LIMITATION_PAR_POTENTIOMETRE de main.h ; SEUIL_DAC / SEUIL_POT forcent une LDO. */
#define SEUIL_DAC        0U
#define SEUIL_POT        1U
#if LIMITATION_PAR_POTENTIOMETRE
#define SEUIL_SOURCE     SEUIL_POT
#else
#define SEUIL_SOURCE     SEUIL_DAC
#endif

/** Memoire des codes DAC (colonne "memoire") : MEM_EEPROM = sauvegarde et
 *  restauration dans l'EEPROM du MCP47FEB28 ; MEM_RAM = comportement d'origine
 *  (0 au demarrage, valeurs en RAM uniquement). */
#define MEM_RAM          0U
#define MEM_EEPROM       1U

// ---------------------------------------------------------------------------
// TABLE 2 : LDO. UNE LIGNE = TOUT CE QUI EST PROPRE A UNE LDO.
// Les lignes ne sont PLUS ecrites ici : elles sont generees depuis
// configurationLDO/ldo_config.csv (modifiable dans la page /dac/config du
// controleur) par configurationLDO/generer_config.py, lance automatiquement
// avant chaque compilation (C/C++ Build > Settings > Build Steps > Pre-build).
// Un CSV incoherent (broche en double, broche Ethernet/I2C/UART du .ioc, voie
// DAC deja prise, deux OC en interruption sur la meme ligne EXTI...) arrete la
// compilation avec le message d'erreur : rien de faux ne peut etre flashe.
//
// Les fonctions (OC, bleeder, EN, DAC, EEPROM, GPIO) ne lisent QUE cette table.
// Index = ordre des lignes du CSV (SET_EN_TAB / SET_BL_TAB / IHM).
// Exemple (LDO 2, "2PSp", PS2+) :
//   2PSp;PRINCIPAL;PE5;PF1;HAUT;PE4;1;PF2;I2C1:0xC0:4;I2C1:0xC0:5;LM211;DEFAUT;EEPROM
//   EN = PE5, BLD = PF1, OC = PE4 (interruption), PG = PF2,
//   tension = I2C1 0xC0 voie 4, seuil de courant = I2C1 0xC0 voie 5.
// ---------------------------------------------------------------------------
static const LDO_Config_t ldoConfig[NB_LDO] = {
#include "ldo_config_lignes.inc"
};

// Etat de la protection surintensite, une entree par LDO (meme index que
// ldoConfig[]). Ecrit depuis l'interruption EXTI et depuis la boucle.
static volatile uint8_t  oc_verrou[NB_LDO];
static volatile uint8_t  oc_a_signaler[NB_LDO];
static volatile uint32_t oc_nb_declenchements[NB_LDO];
static volatile uint32_t oc_tick_declenchement[NB_LDO];
static volatile uint32_t en_tick_activation[NB_LDO];
static volatile uint8_t  en_actif[NB_LDO];
// Diagnostic (GET_DIAG_LDO_<nom>) : fronts vus par l'EXTI (armee ou non) et
// "OC vu actif" par la boucle depuis la derniere lecture du diagnostic.
static volatile uint32_t oc_nb_fronts[NB_LDO];
static volatile uint8_t  oc_vu_actif[NB_LDO];
// 1 = l'EXTI de la broche OC a reellement ete configuree (MX_OC_EXTI_Init()).
static uint8_t           oc_it_active[NB_LDO];
// Sauvegarde EEPROM des codes DAC (LDO declarees MEM_EEPROM) : demande posee par
// SET_VAL/SET_CURRENT (contexte lwIP) ou par le declenchement OC (interruption),
// traitee par la boucle (DAC_TraiterSauvegardes()), jamais en interruption.
static volatile uint8_t  nv_a_sauver[NB_LDO];      /**< 1 = ecriture EEPROM demandee        */
static volatile uint8_t  nv_immediat[NB_LDO];      /**< 1 = sans attendre DAC_EEPROM_DELAI_MS */
static volatile uint32_t nv_tick_demande[NB_LDO];  /**< date de la derniere demande (ms)     */
static uint8_t           nv_essais[NB_LDO];        /**< tentatives echouees consecutives     */
// Commandes ON/OFF par LDO (revision du 22/10/2026) : 1 = ON, 0 = OFF.
// CMD_ON_OFF_OC  = 0 : la broche OC est ignoree (pas de coupure, pas de defaut).
// CMD_ON_OFF_BLD = 0 : le bleeder est relache et ne se commande plus.
// Valeur initiale : colonnes du CSV ; ensuite gardee en flash (CmdOnOff_*).
static volatile uint8_t  cmd_on_off_bld[NB_LDO];
static volatile uint8_t  cmd_on_off_oc[NB_LDO];

/**
  * @brief  Sequence de recuperation du bus I2C1 (PB8=SCL, PB9=SDA) en cas
  *         de bus bloque (SDA maintenu bas par un esclave en plein
  *         transfert interrompu). Reconfigure temporairement les broches
  *         en GPIO, genere jusqu'a 9 fronts d'horloge pour forcer
  *         l'esclave a liberer SDA, emet une condition STOP manuelle,
  *         puis restaure la configuration I2C normale.
  * @param  Aucun.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * Appeler UNE FOIS juste avant MX_I2C1_Init() dans main(),
  * si HAL_I2C_IsDeviceReady() renvoie systematiquement HAL_BUSY.
  * @endcode
  * @ingroup grp_i2c
  */
static void I2C1_BusRecovery(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    // 1) Reconfigure SCL (PB8) et SDA (PB9) en GPIO open-drain simple
    GPIO_InitStruct.Pin   = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;   // pull-up externe deja present
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Etat de repos : les deux lignes relachees (= niveau haut via pull-up)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(1);

    // Genere jusqu'a 9 fronts d'horloge sur SCL : suffisant pour
    //    qu'un esclave bloque en plein octet termine sa transmission
    //    et relache SDA (chaque front peut correspondre a un bit,
    //    dans le pire cas 8 bits de donnees + 1 bit d'ACK).
    for (int i = 0; i < 9; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);  // SCL bas
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);    // SCL haut
        HAL_Delay(1);

        // Si SDA est deja relache (haut), inutile de continuer
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9) == GPIO_PIN_SET)
        {
            break;
        }
    }

    // Genere une condition STOP manuelle : SDA passe de bas a haut
    //    pendant que SCL est haut.
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(1);

    // Remet PB8/PB9 en mode Alternate Function I2C1 normal
    GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
  * @brief  Version generique de I2C1_BusRecovery() pour n'importe quel bus :
  *         passe SCL/SDA en sorties open-drain, envoie jusqu'a 9 fronts
  *         d'horloge pour qu'un esclave bloque relache SDA, genere un STOP
  *         manuel, puis remet les broches en fonction alternative I2C.
  * @param  port     Port GPIO des deux broches (ex : GPIOB).
  * @param  scl_pin  Broche SCL (ex : GPIO_PIN_10 pour I2C2).
  * @param  sda_pin  Broche SDA (ex : GPIO_PIN_11 pour I2C2).
  * @param  af_i2c   Fonction alternative I2C a restaurer (ex : GPIO_AF4_I2C2).
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * // Bus I2C2 bloque (SDA tenue a 0 par un esclave) :
  * I2C_BusRecovery(GPIOB, GPIO_PIN_10, GPIO_PIN_11, GPIO_AF4_I2C2);
  * MX_I2C2_Init();
  * @endcode
  * @ingroup grp_i2c
  */
static void I2C_BusRecovery(GPIO_TypeDef *port, uint16_t scl_pin, uint16_t sda_pin,
                             uint8_t af_i2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin   = scl_pin | sda_pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(port, scl_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(port, sda_pin, GPIO_PIN_SET);
    HAL_Delay(1);

    for (int i = 0; i < 9; i++)
    {
        HAL_GPIO_WritePin(port, scl_pin, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(port, scl_pin, GPIO_PIN_SET);
        HAL_Delay(1);
        if (HAL_GPIO_ReadPin(port, sda_pin) == GPIO_PIN_SET) break;
    }

    HAL_GPIO_WritePin(port, sda_pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(port, scl_pin, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(port, sda_pin, GPIO_PIN_SET);
    HAL_Delay(1);

    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Alternate = af_i2c;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
}



/**
  * @brief  Resultat d'un scan I2C complet (voir ScanI2C()) : la liste
  *         des adresses ayant repondu, et un compteur par type de
  *         reponse HAL (OK/NACK/bus occupe/timeout). Une instance
  *         separee par bus (ex : res_i2c1, res_i2c2), pour ne jamais
  *         melanger les resultats de deux bus differents.
  */
typedef struct {
    uint16_t addr_trouve[16];
    uint8_t  addr_cpt;
    uint32_t count_ok;       // HAL_OK    : adresse acquittee
    uint32_t count_error;    // HAL_ERROR : NACK propre
    uint32_t count_busy;     // HAL_BUSY  : bus bloque / occupe
    uint32_t count_timeout;  // HAL_TIMEOUT
} ScanResult_t;

// Buffer texte partage, rempli par FormaterRapport(). Inspectable au
// debogueur (Watch/Variables) ; a rediriger vers un UART si tu en
// ajoutes un plus tard (HAL_UART_Transmit, etc.).
static char rapport_scan[512];

// ---------------------------------------------------------------------------
// Verrous I2C (un par bus) : StartLwipTask() (boucle for(;;), envoi
// periodique DAC1_DEF..DAC5_DEF) et le traitement des commandes reseau
// (tc_request(), invoque depuis le contexte reseau lwIP a chaque paquet
// UDP recu) accedent TOUS LES DEUX a hi2c1/hi2c2, depuis deux contextes
// d'execution differents, sans aucune protection jusqu'ici. HAL_I2C
// n'est pas reentrant : un HAL_I2C_Master_Transmit() qui demarre pendant
// qu'une autre transaction est en cours sur le MEME bus produit au mieux
// un HAL_BUSY silencieusement ignore (EcrireDACVolatile() ne verifiait
// pas son retour), au pire une trame corrompue/tronquee sur le bus.
// C'est une cause plausible des "trucs bizarres" observes sur le DAC
// pendant un balayage (BALAYER_DAC, sur hi2c1) alors que la boucle
// principale continue en parallele d'emettre sur ce meme bus.
// Un mutex PAR BUS (plutot qu'un seul mutex global) : une transaction
// lente sur hi2c2 ne bloque pas hi2c1, et vice-versa.
static osMutexId i2c1_mutex_id;
static osMutexId i2c2_mutex_id;
osMutexDef(i2c1_mutex);
osMutexDef(i2c2_mutex);

// Verrou UART1 : UART1_EnvoyerTexte() est appelee depuis plusieurs contextes
// (boucle de StartLwipTask() : capture du bouton USER et battement de coeur ;
// contexte reseau lwIP : BALAYER_DAC, SCAN...). HAL_UART_Transmit() n'est pas
// reentrant : un appel concurrent sur le meme handle retourne HAL_BUSY et la
// ligne est PERDUE silencieusement. Ce mutex serialise les emissions. Il vaut
// NULL tant que CreerMutexUART() n'a pas ete appele (debut du demarrage) : dans
// ce cas UART1_EnvoyerTexte() emet sans verrou, comme avant.
static osMutexId uart1_mutex_id = NULL;
osMutexDef(uart1_mutex);

// Battement de coeur UART : une ligne "HB" est emise sur USART1 (PB6/PB7) toutes
// les UART_HEARTBEAT_MS millisecondes. Elle permet a l'interface PC de savoir si
// le cable UART est branche et si le firmware tourne, independamment de
// l'Ethernet (l'interface PC filtre ces lignes : elles n'apparaissent pas dans
// son journal). Mettre 0 pour desactiver.
#define UART_HEARTBEAT_MS   1000U
#if UART_HEARTBEAT_MS > 0U
static uint32_t tick_dernier_hb_uart = 0U;
#endif

// Drapeau runtime : desactive l'emission periodique sur I2C2 (boucle
// principale) tant que ce bus a un probleme materiel non resolu. Par
// defaut a 0 (desactive) : la boucle attendait jusqu'a SPI_TIMEOUT
// (100 ms) par tentative ratee sur I2C2, x3 par tour, ce qui retardait
// aussi les mises a jour I2C1 (meme thread, tout est sequentiel).
// Pilotable a chaud via SET_I2C2_ACTIF:0/1 (pas besoin de reflash pour
// re-basculer une fois le bus repare).
static volatile uint8_t i2c2_actif = 0;

// Coupure automatique de l'Enable sur OC : ACTIVEE depuis le 13/10/2026,
// avec verrouillage du defaut, masquage au demarrage (ldoConfig[].oc.blanking_ms)
// et interruption EXTI sur les broches OC declarees oc.interruption = 1
// (aujourd'hui 2PSp, PE4) - voir OC_Verrouiller(). Historique :
// elle etait DESACTIVEE pour la raison ci-dessous.
// L'ancien code ne l'appliquait qu'a 1PSp (index 0), ce qui empechait EN de
// 1PSp de rester a l'etat haut : des que le comparateur OC basculait (appel
// de courant a la mise sous tension, polarite du comparateur non encore
// validee), la boucle forcait EN bas, OC retombait, EN remontait... EN
// oscillait au rythme de la boucle au lieu de rester haut. Tant que la
// polarite et le seuil OC ne sont pas valides par les tests, EN suit
// UNIQUEMENT la commande operateur. Mettre a 1 pour reactiver la coupure,
// de facon identique sur les 18 LDO.
#define OC_COUPURE_AUTO     1

// Puce DAC actuellement balayee par BalayerSortieDAC() (BALAYER_DAC depuis
// l'interface web, ou capture du bouton USER) : bus + adresse I2C. NULL = aucun
// balayage en cours. La boucle de StartLwipTask() re-ecrit EN CONTINU les blocs
// DAC1_DEF..DAC5_DEF sur chaque puce (une iteration ne dure que quelques ms
// depuis que i2c2_actif = 0) : sans suspension, elle ecrase chaque palier du
// balayage quelques ms apres son ecriture et la sortie du DAC ne suit plus la
// rampe. Tant que ce couple (bus, adresse) est renseigne, la boucle NE reecrit
// PAS la puce concernee ; les autres puces continuent d'etre rafraichies.
static I2C_HandleTypeDef * volatile balayage_hi2c = NULL;
static volatile uint8_t              balayage_adresse = 0U;
#define BALAYAGE_SUR(h, a)   ((balayage_hi2c == (h)) && (balayage_adresse == (uint8_t)(a)))

/**
  * @brief  Cree les deux mutex I2C (un par bus). A appeler UNE SEULE
  *         FOIS, avant que quoi que ce soit puisse emettre sur hi2c1/
  *         hi2c2 (donc avant udpServer_init() dans StartLwipTask() et
  *         avant la premiere iteration de la boucle for(;;)).
  * @param  Aucun.
  * @retval Aucun (void). Remplit i2c1_mutex_id/i2c2_mutex_id.
  * @par Exemple d'utilisation
  * @code
  * // Debut de StartLwipTask(), avant udpServer_init() :
  * CreerMutexI2C();
  * @endcode
  * @ingroup grp_i2c
  */
static void CreerMutexI2C(void)
{
    i2c1_mutex_id = osMutexCreate(osMutex(i2c1_mutex));
    i2c2_mutex_id = osMutexCreate(osMutex(i2c2_mutex));
}

/**
  * @brief  Cree le mutex qui serialise les emissions de texte sur USART1
  *         (voir UART1_EnvoyerTexte()). A appeler UNE SEULE FOIS, depuis
  *         StartLwipTask(), juste apres CreerMutexI2C(), avant que la boucle
  *         ou une commande reseau puisse emettre sur l'UART.
  * @param  Aucun.
  * @retval Aucun (void). Remplit uart1_mutex_id.
  * @par Exemple d'utilisation
  * @code
  * CreerMutexI2C();
  * CreerMutexUART();   // dans StartLwipTask(), avant udpServer_init()
  * @endcode
  * @ingroup grp_comm
  */
static void CreerMutexUART(void)
{
    uart1_mutex_id = osMutexCreate(osMutex(uart1_mutex));
}

/**
  * @brief  Retourne le mutex correspondant au bus d'un handle I2C donne.
  * @param  hi2c  Handle I2C (&hi2c1 ou &hi2c2).
  * @retval osMutexId  i2c2_mutex_id si hi2c == &hi2c2, i2c1_mutex_id sinon
  *                    (comportement par defaut : hi2c1, comme ailleurs
  *                    dans ce fichier ou "bus == 2" bascule sur hi2c2).
  * @par Exemple d'utilisation
  * @code
  * osMutexId m = ResoudreMutexI2C(hi2c);
  * osMutexWait(m, osWaitForever);
  * HAL_I2C_Master_Transmit(hi2c, ...);
  * osMutexRelease(m);
  * @endcode
  * @ingroup grp_i2c
  */
static osMutexId ResoudreMutexI2C(I2C_HandleTypeDef *hi2c)
{
    return (hi2c == &hi2c2) ? i2c2_mutex_id : i2c1_mutex_id;
}

/**
  * @brief  Scanne toutes les adresses I2C possibles (hors General Call
  *         => 0x00) sur le bus passe en parametre, et remplit le
  *         resultat. Fonction "pure" : aucun effet de bord sur
  *         rapport_scan ou toute autre variable globale de texte.
  * @param  hi2c    Pointeur vers le handle I2C a scanner (&hi2c1, &hi2c2, ...).
  * @param  result  Pointeur vers la structure a remplir avec les resultats.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * static ScanResult_t res_i2c1;
  * ScanI2C(&hi2c1, &res_i2c1);
  * @endcode
  * @ingroup grp_i2c
  */
static void ScanI2C(I2C_HandleTypeDef *hi2c, ScanResult_t *result)
{
    result->addr_cpt = 0;
    result->count_ok = 0;
    result->count_error = 0;
    result->count_busy = 0;
    result->count_timeout = 0;

    for (uint16_t scan_addr = 2; scan_addr < 256; scan_addr += 2)
    {
        osMutexId m = ResoudreMutexI2C(hi2c);
        osMutexWait(m, osWaitForever);
        HAL_StatusTypeDef st = HAL_I2C_IsDeviceReady(hi2c, scan_addr, 2, 20);
        osMutexRelease(m);

        switch (st)
        {
            case HAL_OK:
                result->count_ok++;
                if (result->addr_cpt < 16)
                {
                    result->addr_trouve[result->addr_cpt++] = scan_addr;
                }
                break;
            case HAL_ERROR:   result->count_error++;   break;
            case HAL_BUSY:    result->count_busy++;    break;
            case HAL_TIMEOUT: result->count_timeout++; break;
        }
    }
}

/**
  * @brief  Construit, dans le buffer global rapport_scan, une ligne
  *         "I2Cx:0xNN:message" pour chaque adresse trouvee dans un
  *         resultat de scan. N'efface PAS rapport_scan avant d'ecrire :
  *         permet d'enchainer plusieurs bus dans le meme rapport
  *         (voir exemple).
  * @param  nom_bus  Nom du bus a afficher dans le rapport (ex : "I2C1").
  * @param  result   Pointeur vers un resultat deja rempli par ScanI2C().
  * @retval Aucun (void). Complete rapport_scan.
  * @par Exemple d'utilisation
  * @code
  * static ScanResult_t res_i2c1, res_i2c2;
  * ScanI2C(&hi2c1, &res_i2c1);
  * ScanI2C(&hi2c2, &res_i2c2);
  *
  * rapport_scan[0] = '\0';           // vide le rapport avant de le reconstruire
  * FormaterRapport("I2C1", &res_i2c1);
  * FormaterRapport("I2C2", &res_i2c2);
  * // rapport_scan contient alors par exemple :
  * // "I2C1:0x84:OK\nI2C1:0xC4:OK\nI2C2:0x50:OK\n"
  * @endcode
  * @ingroup grp_i2c
  */
static void FormaterRapport(const char *nom_bus, const ScanResult_t *result)
{
    for (uint8_t i = 0; i < result->addr_cpt; i++)
    {
        char ligne[32];
        int len = sprintf(ligne, "%s:0x%02X:OK\n", nom_bus, result->addr_trouve[i]);
        size_t used = strlen(rapport_scan);
        if (used + (size_t)len < sizeof(rapport_scan))
        {
            strcat(rapport_scan, ligne);
        }
    }
}

// Tension d'alimentation REELLE des DAC (= reference pleine echelle, VREF=VDD) : 5 V.
// Aucun DAC de la carte n'est alimente en 3.3 V. Valeur unique utilisee par la
// capture du bouton USER (ExecuterCaptureComplete) ; le balayage a distance
// (BALAYER_DAC) recoit sa propre tension_ref depuis l'interface web.
#define DAC_VDD_REFERENCE_V   5.0f

/**
  * @brief  Convertit une tension desiree (en volts) en code numerique
  *         12 bits (0-4095) compris par le DAC, selon la formule :
  *         code = (tension_desiree / tension_reference) * 4095.
  *         Dans la config actuelle du DAC (registre VREF a sa valeur
  *         par defaut, VDD utilise comme reference), tension_reference
  *         doit etre la tension VDD REELLE mesuree sur le DAC (pas
  *         forcement 5V pile - utilise la valeur que tu as mesuree
  *         au multimetre pour plus de precision).
  * @param  tension_desiree    Tension de sortie voulue, en volts (ex : 5.0f).
  *                            Ecretee automatiquement entre 0 et tension_reference.
  * @param  tension_reference  Tension de reference reelle du DAC (VDD mesure), en volts.
  * @retval uint16_t  Code 12 bits (0-4095) a passer a EcrireDACVolatile()
  *                   ou BalayerSortieDAC().
  * @par Exemple d'utilisation
  * @code
  * // Ecrire 5V sur VOUT0, avec un VDD reellement mesure a 5V :
  * uint16_t code = VoltageVersCodeDAC(5.0f, 5.0f);   // -> 4095
  * EcrireDACVolatile(&hi2c1, (0x62 << 1), 0x00, code);
  *
  * // Ecrire 2.5V (moitie de VDD), avec VDD mesure a 4.98V :
  * uint16_t code2 = VoltageVersCodeDAC(2.5f, 4.98f); // -> ~2056
  * @endcode
  * @ingroup grp_dac
  */
static uint16_t VoltageVersCodeDAC(float tension_desiree, float tension_reference)
{
    if (tension_desiree < 0.0f)               { tension_desiree = 0.0f; }
    if (tension_desiree > tension_reference)   { tension_desiree = tension_reference; }

    float ratio = tension_desiree / tension_reference;
    uint16_t code = (uint16_t)(ratio * 4095.0f + 0.5f);  // +0.5 pour arrondir au plus proche

    if (code > 4095) { code = 4095; }  // securite, ne devrait jamais arriver apres l'ecretage ci-dessus

    return code;
}

/**
  * @brief  Retrouve le tableau DACx_DEF (celui rafraichi en continu par la
  *         boucle de StartLwipTask(), voir "USER CODE BEGIN 2") qui
  *         correspond au bus/adresse donnes, ou NULL si aucun ne correspond
  *         (adresse non geree par la boucle periodique).
  * @param  hi2c         Handle I2C (&hi2c1 ou &hi2c2).
  * @param  adresse_hal  Adresse I2C 7 bits deja decalee (ex: MCP4725_I2CADDR_DEFAULT).
  * @retval uint8_t*  Pointeur vers DAC1_DEF, DAC2_DEF... DAC5_DEF, ou NULL.
  * @par Exemple d'utilisation
  * @code
  * uint8_t *tab = ResoudreTableauDacParAdresse(&hi2c1, (0x60 << 1));
  * // -> &DAC1_DEF[0]
  * @endcode
  * @ingroup grp_dac
  */
static uint8_t *ResoudreTableauDacParAdresse(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal)
{
    for (uint32_t k = 0; k < NB_DAC_PUCES; k++)
    {
        if ((tableDac[k].hi2c == hi2c) && (tableDac[k].adresse_hal == adresse_hal))
        {
            return tableDac[k].trame;
        }
    }
    return NULL;
}

// Declaration anticipee : definie apres mappingDacLdo[] (plus bas).
static void MajEtatLdoDepuisDac(const uint8_t *tableau, uint8_t canal, uint16_t valeur);

/**
  * @brief  Repercute une ecriture DAC directe (EcrireDACVolatile(), typiquement
  *         depuis BalayerSortieDAC()) dans le tableau DACx_DEF correspondant,
  *         pour que la boucle periodique de StartLwipTask() retransmette CETTE
  *         valeur au lieu d'ecraser la sortie avec l'ancien contenu du tableau
  *         (typiquement 0V, voir MCP4725_VAL_DEF_BIN) des la fin de l'ecriture
  *         directe. Sans cet appel, une valeur ecrite par un balayage ou un
  *         cycle de test "retombait" au palier suivant de la boucle. Ne fait
  *         rien si (hi2c, adresse_hal) ne correspond a aucun DACx_DEF connu
  *         (ResoudreTableauDacParAdresse() renvoie alors NULL).
  * @param  hi2c         Handle I2C utilise pour l'ecriture (&hi2c1 ou &hi2c2).
  * @param  adresse_hal  Adresse I2C 7 bits deja decalee de la puce ecrite.
  * @param  registre     Registre ecrit (0-7).
  * @param  valeur       Valeur 12 bits ecrite sur ce registre.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * EcrireDACVolatile(hi2c, adresse_hal, registre, valeur);
  * SynchroniserTableauDac(hi2c, adresse_hal, registre, valeur);
  * @endcode
  * @ingroup grp_dac
  */
static void SynchroniserTableauDac(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal,
                                    uint8_t registre, uint16_t valeur)
{
    uint8_t *tableau = ResoudreTableauDacParAdresse(hi2c, adresse_hal);
    if (tableau == NULL || registre > 0x07)
    {
        return;
    }
    tableau[registre * 3 + 1] = (uint8_t)((valeur & 0x0F00) >> 8);
    tableau[registre * 3 + 2] = (uint8_t)(valeur & 0x00FF);

    // Garde la page LDO coherente apres un balayage a distance : la LDO
    // cablee sur ce canal affiche la valeur reellement en place.
    MajEtatLdoDepuisDac(tableau, registre, valeur);
}

/**
  * @brief  Ecrit une valeur 12 bits dans un registre VOLATILE du DAC
  *         MCP47FEB28 (adresses memoire 00h-07h uniquement). Ne touche
  *         JAMAIS l'EEPROM (adresses 10h-1Ah) : refuse d'emettre quoi
  *         que ce soit si registre est hors de la plage 0x00-0x07,
  *         aucun risque de corrompre l'adresse I2C ou toute autre
  *         configuration du DAC, meme en cas d'appel avec un parametre
  *         invalide.
  * @param  hi2c         Pointeur vers le handle I2C a utiliser (&hi2c1, &hi2c2, ...).
  * @param  adresse_hal  Adresse I2C du DAC, deja au format HAL (decalee,
  *                      ex : 0xC4 pour un DAC a 0x62 en 7 bits).
  * @param  registre     Adresse memoire du registre volatile cible
  *                      (0x00 = DAC0/VOUT0, 0x01 = DAC1/VOUT1, ... 0x07 = DAC7/VOUT7).
  *                      Toute valeur > 0x07 est refusee (HAL_ERROR immediat,
  *                      aucune trame emise) pour eviter tout risque d'ecriture
  *                      accidentelle dans les registres nonvolatiles (EEPROM).
  * @param  valeur       Valeur 12 bits a ecrire (0-4095).
  * @retval HAL_StatusTypeDef  Resultat de la transmission I2C
  *         (HAL_OK si le DAC a acquitte les 3 octets, HAL_ERROR si
  *         registre est hors plage volatile ou si le DAC a NACK).
  * @par Exemple d'utilisation
  * @code
  * // Ecrit la pleine echelle (4095) sur VOUT0 du DAC a l'adresse 0x62 :
  * HAL_StatusTypeDef status = EcrireDACVolatile(&hi2c1, (0x62 << 1), 0x00, 4095);
  * if (status == HAL_OK) { ... }   // mesurer VOUT0 au voltmetre
  * @endcode
  * @ingroup grp_dac
  */
static HAL_StatusTypeDef EcrireDACVolatile(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal,
                                            uint8_t registre, uint16_t valeur)
{
    // Garde-fou : les registres nonvolatiles (EEPROM) commencent a 0x10.
    // On refuse toute adresse hors de la plage volatile 0x00-0x07 pour
    // ne jamais risquer d'y ecrire par erreur (adresse I2C, verrou, etc.).
    if (registre > 0x07)
    {
        return HAL_ERROR;
    }

    uint8_t trame[3];

    // Octet 1 (commande) : AD4:AD0 = adresse du registre (5 bits),
    //                       C1:C0 = 00 (Write), bit X = 0
    // Octets 2-3 (donnee) : valeur 12 bits alignee a droite dans le
    //                       registre 16 bits du DAC
    trame[0] = (uint8_t)((registre & 0x1F) << 3);
    trame[1] = (uint8_t)((valeur >> 8) & 0x0F);
    trame[2] = (uint8_t)(valeur & 0xFF);

    osMutexId m = ResoudreMutexI2C(hi2c);
    osMutexWait(m, osWaitForever);
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(hi2c, adresse_hal, trame, 3, 100);
    osMutexRelease(m);
    return st;
}

/**
  * @brief  Lit un registre VOLATILE de CONFIGURATION du DAC MCP47FEB28
  *         (adresses memoire 08h/09h/0Ah UNIQUEMENT - VREF/Power-Down/
  *         Gain, voir Table 4-1 du datasheet DS20006368A). Toute autre
  *         adresse est refusee (HAL_ERROR immediat, aucune trame emise) :
  *         meme garde-fou que EcrireDACVolatile(), pour ne jamais risquer
  *         de lire (et donc plus tard, par erreur de copier-coller,
  *         d'ecrire) 0Bh (WiperLock STATUS) ou l'EEPROM (10h-1Ah).
  *         Protocole : ecrit l'octet de commande (adresse memoire,
  *         bits C1:C0=11 pour READ), puis relit 2 octets de donnee via
  *         un Repeated Start (HAL_I2C_Mem_Read gere cette sequence).
  * @param  hi2c         Pointeur vers le handle I2C a utiliser (&hi2c1, &hi2c2, ...).
  * @param  adresse_hal  Adresse I2C du DAC, deja au format HAL (decalee).
  * @param  registre     0x08 (VREF), 0x09 (Power-Down) ou 0x0A (Gain/STATUS).
  *                      Toute autre valeur est refusee.
  * @param  valeur       Pointeur vers le uint16_t a remplir avec le mot lu.
  * @retval HAL_StatusTypeDef  Resultat de la lecture I2C (HAL_OK si le
  *         DAC a repondu, HAL_ERROR si registre est hors plage autorisee
  *         ou si le DAC a NACK/timeout).
  * @par Exemple d'utilisation
  * @code
  * uint16_t vref;
  * if (LireRegistreConfigDAC(&hi2c1, (0x62 << 1), 0x08, &vref) == HAL_OK)
  * {
  *     // vref == 0x0000 si tous les canaux sont sur VDD (defaut usine)
  * }
  * @endcode
  * @ingroup grp_dac
  */
static HAL_StatusTypeDef LireRegistreConfigDAC(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal,
                                                uint8_t registre, uint16_t *valeur)
{
    if (registre < 0x08 || registre > 0x0A || valeur == NULL)
    {
        return HAL_ERROR;
    }

    // Octet de commande : AD4:AD0 = adresse du registre (5 bits),
    // C1:C0 = 11 (Read), bit X = 0.
    uint8_t commande = (uint8_t)(((registre & 0x1F) << 3) | 0x06);
    uint8_t donnees[2] = {0, 0};

    osMutexId m = ResoudreMutexI2C(hi2c);
    osMutexWait(m, osWaitForever);
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(hi2c, adresse_hal, commande,
                                             I2C_MEMADD_SIZE_8BIT, donnees, 2, 100);
    osMutexRelease(m);

    if (st == HAL_OK)
    {
        *valeur = (uint16_t)((donnees[0] << 8) | donnees[1]);
    }
    return st;
}

/**
  * @brief  Ecrit un registre VOLATILE de CONFIGURATION du DAC MCP47FEB28
  *         (adresses memoire 08h/09h/0Ah UNIQUEMENT). Meme garde-fou que
  *         EcrireDACVolatile()/LireRegistreConfigDAC() : toute adresse
  *         hors de {0x08, 0x09, 0x0A} est refusee, aucune trame emise.
  *         ATTENTION : contrairement a EcrireDACVolatile() (une sortie a
  *         la fois), ces 3 registres s'appliquent AUX 8 CANAUX DE LA
  *         PUCE A LA FOIS (2 bits/canal pour VREF et Power-Down, 1 bit/
  *         canal pour Gain) - voir Registres 4-2/4-3/4-4 du datasheet.
  *         Une seule ecriture ici change donc potentiellement les 8
  *         sorties de cette puce.
  * @param  hi2c         Pointeur vers le handle I2C a utiliser (&hi2c1, &hi2c2, ...).
  * @param  adresse_hal  Adresse I2C du DAC, deja au format HAL (decalee).
  * @param  registre     0x08 (VREF), 0x09 (Power-Down) ou 0x0A (Gain).
  *                      Toute autre valeur est refusee.
  * @param  valeur       Mot 16 bits a ecrire tel quel (voir Registres
  *                      4-2/4-3/4-4 pour le format exact bit a bit -
  *                      c'est a l'appelant de le construire correctement,
  *                      cette fonction ne fait aucune interpretation).
  * @retval HAL_StatusTypeDef  Resultat de l'ecriture I2C.
  * @par Exemple d'utilisation
  * @code
  * // VREF = VDD sur les 8 canaux (VRnB:VRnA = 00 partout) :
  * EcrireRegistreConfigDAC(&hi2c1, (0x62 << 1), 0x08, 0x0000);
  * // Gain = 1x sur les 8 canaux (G7..G0 = 0, octet bas ignore) :
  * EcrireRegistreConfigDAC(&hi2c1, (0x62 << 1), 0x0A, 0x0000);
  * @endcode
  * @ingroup grp_dac
  */
static HAL_StatusTypeDef EcrireRegistreConfigDAC(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal,
                                                  uint8_t registre, uint16_t valeur)
{
    if (registre < 0x08 || registre > 0x0A)
    {
        return HAL_ERROR;
    }

    uint8_t trame[3];
    trame[0] = (uint8_t)((registre & 0x1F) << 3);   // C1:C0 = 00 (Write)
    trame[1] = (uint8_t)((valeur >> 8) & 0xFF);
    trame[2] = (uint8_t)(valeur & 0xFF);

    osMutexId m = ResoudreMutexI2C(hi2c);
    osMutexWait(m, osWaitForever);
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(hi2c, adresse_hal, trame, 3, 100);
    osMutexRelease(m);
    return st;
}

// Puces MCP47FEB28 presentes sur I2C1 (scan du 24/09/2026 : 0xC0, 0xC2, 0xC4).
static const uint8_t dacs_i2c1_adresses[] = { (0x60 << 1), (0x61 << 1), (0x62 << 1) };
#define NB_DACS_I2C1   (sizeof(dacs_i2c1_adresses) / sizeof(dacs_i2c1_adresses[0]))

/**
  * @brief  Force la configuration VOLATILE des MCP47FEB28 de I2C1 a l'etat
  *         attendu par tout le reste du firmware : VREF = VDD (5V) sur les 8
  *         canaux, sortie en fonctionnement normal (pas de Power-Down),
  *         gain 1x. Relit ensuite les 3 registres et envoie le resultat sur
  *         USART1.
  *         Pourquoi : le firmware ne configurait JAMAIS ces registres. Chaque
  *         puce demarre donc sur les valeurs de son EEPROM, qui peuvent
  *         differer d'une puce a l'autre (programmation anterieure, trame
  *         parasite, ex : le texte "Bonjour" de la capture bouton USER,
  *         envoye a 0xC0/0xC2 avant le correctif). Si l'EEPROM de la puce
  *         0xC2 contient VREF = bande interdite (~1.2V) ou un mode
  *         Power-Down, SET_VAL_LDO_1PLp ne peut pas couvrir 0-5V alors que
  *         la puce 0xC0 (1PSp/2PSp) fonctionne. Registres 08h/09h/0Ah
  *         volatils uniquement : l'EEPROM n'est jamais touchee.
  * @param  Aucun.
  * @retval Aucun (void). Une ligne UART par puce :
  *         "DAC_INIT:0xC2:VREF=0x0000:PD=0x0000:GAIN=0x0080:OK".
  * @par Exemple d'utilisation
  * @code
  * CreerMutexI2C();
  * CreerMutexUART();
  * InitialiserConfigDacsI2C1();   // avant udpServer_init()
  * @endcode
  * @ingroup grp_dac
  */
static void InitialiserConfigDacsI2C1(void)
{
    for (uint32_t i = 0; i < NB_DACS_I2C1; i++)
    {
        uint8_t adr = dacs_i2c1_adresses[i];
        HAL_StatusTypeDef st = HAL_OK;

        if (EcrireRegistreConfigDAC(&hi2c1, adr, 0x08, 0x0000) != HAL_OK) { st = HAL_ERROR; }  // VREF = VDD
        if (EcrireRegistreConfigDAC(&hi2c1, adr, 0x09, 0x0000) != HAL_OK) { st = HAL_ERROR; }  // PD = normal
        if (EcrireRegistreConfigDAC(&hi2c1, adr, 0x0A, 0x0000) != HAL_OK) { st = HAL_ERROR; }  // Gain = 1x

        uint16_t vref = 0xFFFF, pd = 0xFFFF, gain = 0xFFFF;
        LireRegistreConfigDAC(&hi2c1, adr, 0x08, &vref);
        LireRegistreConfigDAC(&hi2c1, adr, 0x09, &pd);
        LireRegistreConfigDAC(&hi2c1, adr, 0x0A, &gain);

        // Octet bas du registre 0Ah = bits d'etat (POR, EEWA), non significatifs ici.
        int conforme = (st == HAL_OK) && (vref == 0x0000) && (pd == 0x0000) && ((gain & 0xFF00) == 0x0000);

        char ligne[80];
        snprintf(ligne, sizeof(ligne), "DAC_INIT:0x%02X:VREF=0x%04X:PD=0x%04X:GAIN=0x%04X:%s\r\n",
                 adr, vref, pd, gain, conforme ? "OK" : "ERREUR");
        UART1_EnvoyerTexte(ligne);
    }
}

// ---------------------------------------------------------------------------
// Surveillance de la configuration des MCP47FEB28 de I2C1 (revision 20/10/2026)
// Les registres VREF (08h), Power-Down (09h) et Gain (0Ah) ne sont ecrits qu'au
// demarrage (InitialiserConfigDacsI2C1()) ou par SET_DAC_VREF/PD/GAIN, alors
// que les valeurs des voies sont reemises a chaque tour. Si une puce redemarre
// (chute de VDD, perturbation), elle recharge la configuration de son EEPROM :
// la valeur 860 reste bonne mais la reference peut changer (ex : bande
// interdite 1,214 V au lieu de VDD 5 V) et la sortie du LDO derive. Cette
// surveillance relit les 3 registres toutes les DAC_SURVEILLANCE_MS, reecrit la
// configuration attendue si elle a change et le signale sur USART1.
// ---------------------------------------------------------------------------
#define DAC_SURVEILLANCE_MS   500U

// Configuration attendue par puce (meme index que dacs_i2c1_adresses[]) :
// [0] = VREF, [1] = Power-Down, [2] = Gain (octet haut seul significatif).
// 0x0000 partout = VREF = VDD, fonctionnement normal, gain 1x (demarrage).
static uint16_t dac_cfg_attendue[NB_DACS_I2C1][3];
static volatile uint32_t dac_nb_reconfig[NB_DACS_I2C1];
static uint32_t tick_derniere_surveillance_dac = 0U;

/**
  * @brief  Index d'une puce de I2C1 dans dacs_i2c1_adresses[].
  * @param  hi2c         Handle I2C (seul &hi2c1 est surveille).
  * @param  adresse_hal  Adresse 7 bits decalee (0xC0, 0xC2, 0xC4).
  * @retval Index 0..NB_DACS_I2C1-1, ou -1 si la puce n'est pas surveillee.
  * @par Exemple d'utilisation
  * @code
  * int k = IndexDacI2C1(&hi2c1, 0xC0);   // -> 0
  * @endcode
  * @ingroup grp_dac
  */
static int IndexDacI2C1(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal)
{
    if (hi2c != &hi2c1)
    {
        return -1;
    }
    for (uint32_t k = 0; k < NB_DACS_I2C1; k++)
    {
        if (dacs_i2c1_adresses[k] == adresse_hal)
        {
            return (int)k;
        }
    }
    return -1;
}

/**
  * @brief  Memorise la nouvelle configuration attendue apres un SET_DAC_VREF/
  *         PD/GAIN reussi, pour que la surveillance ne la "corrige" pas.
  * @param  hi2c         Handle I2C de la puce ecrite.
  * @param  adresse_hal  Adresse 7 bits decalee de la puce.
  * @param  registre     0x08 (VREF), 0x09 (PD) ou 0x0A (Gain).
  * @param  mot          Mot 16 bits ecrit.
  * @retval Aucun.
  * @par Exemple d'utilisation
  * @code
  * MemoriserConfigDac(&hi2c1, 0xC0, 0x08, 0x0000);
  * @endcode
  * @ingroup grp_dac
  */
static void MemoriserConfigDac(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal, uint8_t registre, uint16_t mot)
{
    const int k = IndexDacI2C1(hi2c, adresse_hal);
    if ((k < 0) || (registre < 0x08U) || (registre > 0x0AU))
    {
        return;
    }
    dac_cfg_attendue[k][registre - 0x08U] = (registre == 0x0AU) ? (uint16_t)(mot & 0xFF00U) : mot;
}

/**
  * @brief  Relit VREF/PD/Gain des puces de I2C1 et reecrit la configuration
  *         attendue si une puce l'a perdue. Appelee par la boucle de
  *         StartLwipTask(), au plus une fois toutes les DAC_SURVEILLANCE_MS.
  *         Une puce qui ne repond pas (NACK) est ignoree jusqu'au tour suivant.
  * @retval Aucun. Ligne UART a chaque correction :
  *         "DAC_RECONFIG:0xC0:VREF=0x5555:PD=0x0000:GAIN=0x0000:NB=1".
  * @par Exemple d'utilisation
  * @code
  * SurveillerConfigDacsI2C1();   // dans la boucle for(;;)
  * @endcode
  * @ingroup grp_dac
  */
static void SurveillerConfigDacsI2C1(void)
{
    if ((HAL_GetTick() - tick_derniere_surveillance_dac) < DAC_SURVEILLANCE_MS)
    {
        return;
    }
    tick_derniere_surveillance_dac = HAL_GetTick();

    for (uint32_t k = 0; k < NB_DACS_I2C1; k++)
    {
        const uint8_t adr = dacs_i2c1_adresses[k];
        uint16_t lu[3] = { 0U, 0U, 0U };

        if (BALAYAGE_SUR(&hi2c1, adr))
        {
            continue;
        }
        if ((LireRegistreConfigDAC(&hi2c1, adr, 0x08, &lu[0]) != HAL_OK) ||
            (LireRegistreConfigDAC(&hi2c1, adr, 0x09, &lu[1]) != HAL_OK) ||
            (LireRegistreConfigDAC(&hi2c1, adr, 0x0A, &lu[2]) != HAL_OK))
        {
            continue;
        }
        if ((lu[0] == dac_cfg_attendue[k][0]) && (lu[1] == dac_cfg_attendue[k][1]) &&
            ((lu[2] & 0xFF00U) == dac_cfg_attendue[k][2]))
        {
            continue;
        }

        EcrireRegistreConfigDAC(&hi2c1, adr, 0x08, dac_cfg_attendue[k][0]);
        EcrireRegistreConfigDAC(&hi2c1, adr, 0x09, dac_cfg_attendue[k][1]);
        EcrireRegistreConfigDAC(&hi2c1, adr, 0x0A, dac_cfg_attendue[k][2]);
        dac_nb_reconfig[k]++;

        char ligne[96];
        snprintf(ligne, sizeof(ligne), "DAC_RECONFIG:0x%02X:VREF=0x%04X:PD=0x%04X:GAIN=0x%04X:NB=%lu\r\n",
                 adr, lu[0], lu[1], lu[2], (unsigned long)dac_nb_reconfig[k]);
        UART1_EnvoyerTexte(ligne);
    }
}

/**
  * @brief  Ecrit une valeur sur chacune des 8 sorties volatiles du DAC
  *         (VOUT0 a VOUT7, registres 00h-07h), et ajoute une ligne de
  *         compte-rendu dans rapport_scan pour chaque canal, au format
  *         "DACx:VOUTn:0xNNNN:message". Une valeur DIFFERENTE est
  *         ecrite sur chaque canal (palier croissant) pour verifier
  *         facilement au voltmetre que les 8 sorties sont bien
  *         independantes les unes des autres.
  * @param  hi2c         Pointeur vers le handle I2C a utiliser (&hi2c1, &hi2c2, ...).
  * @param  adresse_hal  Adresse I2C du DAC, deja au format HAL (decalee).
  * @param  nom_dac      Nom du DAC a afficher dans le rapport (ex : "DAC1").
  * @retval Aucun (void). Complete rapport_scan.
  * @par Exemple d'utilisation
  * @code
  * EcrireToutesLesSorties(&hi2c1, (0x62 << 1), "DAC1");
  * // rapport_scan contient alors, entre autres :
  * // "DAC1:VOUT0:0x01F4:OK\nDAC1:VOUT1:0x03E8:OK\n..."
  * @endcode
  * @ingroup grp_dac
  */
static void EcrireToutesLesSorties(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal, const char *nom_dac)
{
    for (uint8_t canal = 0; canal < 8; canal++)
    {
        // Palier croissant : 500, 1000, 1500, ... 4000 (reste dans la
        // plage 0-4095) -> facile a distinguer canal par canal au voltmetre.
        uint16_t valeur = (uint16_t)((canal + 1) * 500);

        HAL_StatusTypeDef st = EcrireDACVolatile(hi2c, adresse_hal, canal, valeur);

        char ligne[40];
        int len = sprintf(ligne, "%s:VOUT%u:0x%04X:%s\n",
                           nom_dac, canal, valeur, (st == HAL_OK) ? "OK" : "ERROR");
        size_t used = strlen(rapport_scan);
        if (used + (size_t)len < sizeof(rapport_scan))
        {
            strcat(rapport_scan, ligne);
        }

        HAL_Delay(10);  // pause entre chaque canal : laisse le temps au DAC de traiter
    }
}

/**
  * @brief  Balaie une seule sortie du DAC de 0 a valeur_max, par pas
  *         reguliers, avec une pause entre chaque valeur. Fonction
  *         BLOQUANTE : elle ne rend la main qu'une fois le balayage
  *         complet termine. Utile pour observer une montee de tension
  *         progressive au voltmetre sur une sortie precise. Envoie
  *         aussi chaque palier sur USART1 en direct, au format
  *         "nom_dac:VOUTn:code:tension_calculeeV".
  *         Generique : fonctionne sur n'importe quel DAC/canal/plage/
  *         vitesse, il suffit de changer les parametres.
  * @param  hi2c               Pointeur vers le handle I2C a utiliser (&hi2c1, &hi2c2, ...).
  * @param  adresse_hal        Adresse I2C du DAC, deja au format HAL (decalee).
  * @param  registre           Adresse memoire du registre volatile cible
  *                            (0x00 = VOUT0, 0x01 = VOUT1, ... 0x07 = VOUT7).
  * @param  nom_dac            Nom du DAC a afficher dans le rapport (ex : "DAC1").
  * @param  valeur_max         Valeur finale du balayage (0-4095 pour un DAC 12 bits).
  * @param  pas                Increment entre deux valeurs successives (ex : 100).
  *                            Doit etre >= 1.
  * @param  delai_ms           Pause en millisecondes entre chaque valeur ecrite
  *                            (ex : 200 pour bien voir chaque palier au voltmetre).
  * @param  tension_reference  Tension VDD reelle du DAC (mesuree au multimetre),
  *                            utilisee uniquement pour calculer la tension
  *                            affichee dans le rapport (n'affecte pas le code envoye).
  * @retval Aucun (void). Termine toujours exactement sur valeur_max,
  *         meme si (valeur_max - 0) n'est pas un multiple exact de pas.
  * @par Exemple d'utilisation
  * @code
  * // Balaie VOUT0 du DAC a 0x62, de 0 a 4095, par pas de 100,
  * // avec 200 ms entre chaque palier (~8 secondes au total),
  * // rapporte sur UART avec VDD mesure a 5V :
  * BalayerSortieDAC(&hi2c1, (0x62 << 1), 0x00, "DAC1", 4095, 100, 200, 5.0f);
  * @endcode
  * @ingroup grp_dac
  */
static void BalayerSortieDAC(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal, uint8_t registre,
                              const char *nom_dac, uint16_t valeur_max, uint16_t pas,
                              uint32_t delai_ms, float tension_reference)
{
    if (pas == 0) { pas = 1; }  // evite une boucle infinie si appelee avec pas=0 par erreur

    // Suspend le rafraichissement periodique de CETTE puce par la boucle de
    // StartLwipTask() pendant tout le balayage (voir balayage_hi2c).
    balayage_adresse = adresse_hal;
    balayage_hi2c    = hi2c;
    // ATTENTION duree totale = (valeur_max / pas) * delai_ms : avec un
    // pas fin (ex: 1) et un delai long, le balayage complet peut durer
    // tres longtemps (ex: pas=1, delai_ms=500 -> ~34 minutes). Choisis
    // pas et delai_ms ensemble selon la duree totale voulue.

    uint16_t valeur;
    for (valeur = 0; valeur < valeur_max; valeur = (uint16_t)(valeur + pas))
    {
        HAL_StatusTypeDef st_ecr = EcrireDACVolatile(hi2c, adresse_hal, registre, valeur);
        SynchroniserTableauDac(hi2c, adresse_hal, registre, valeur);  // garde la valeur : la boucle periodique ne doit pas revenir en arriere

        // Calcul en millivolts entiers : evite le besoin du support
        // flottant de printf (%f), non active par defaut sur STM32
        // (economie de flash). Le calcul flottant lui-meme (pas son
        // affichage) ne pose aucun probleme.
        uint32_t milliVolts = (uint32_t)(((float)valeur / 4095.0f) * tension_reference * 1000.0f + 0.5f);
        char ligne[56];
        sprintf(ligne, "%s:VOUT%u:%u:%lu.%03luV:%s\n", nom_dac, registre, valeur,
                (unsigned long)(milliVolts / 1000), (unsigned long)(milliVolts % 1000),
                (st_ecr == HAL_OK) ? "OK" : "ERREUR_I2C");
        UART1_EnvoyerTexte(ligne);

        HAL_Delay(delai_ms);
    }

    // Assure que le dernier palier ecrit est exactement valeur_max,
    // meme si le pas ne tombe pas juste dessus.
    EcrireDACVolatile(hi2c, adresse_hal, registre, valeur_max);
    SynchroniserTableauDac(hi2c, adresse_hal, registre, valeur_max);  // idem pour le palier final
    {
        uint32_t milliVolts = (uint32_t)(((float)valeur_max / 4095.0f) * tension_reference * 1000.0f + 0.5f);
        char ligne[48];
        sprintf(ligne, "%s:VOUT%u:%u:%lu.%03luV\n", nom_dac, registre, valeur_max,
                (unsigned long)(milliVolts / 1000), (unsigned long)(milliVolts % 1000));
        UART1_EnvoyerTexte(ligne);
    }
    HAL_Delay(delai_ms);

    // Fin du balayage : la boucle de StartLwipTask() reprend le rafraichissement
    // de la puce avec DAC1_DEF..DAC5_DEF (valeurs configurees).
    balayage_hi2c = NULL;
}

/**
  * @brief  Formate le temps ecoule depuis le demarrage (HAL_GetTick(),
  *         en millisecondes) en une chaine "HH:MM:SS". Le STM32 n'a
  *         pas de RTC configuree dans ce projet : ce n'est donc PAS
  *         une heure murale reelle, seulement une duree depuis le
  *         reset de la carte.
  * @param  dest    Buffer de sortie, doit faire au moins 9 octets
  *                 (8 caracteres "HH:MM:SS" + '\0').
  * @param  taille  Taille du buffer dest, pour eviter tout debordement.
  * @retval Aucun (void). Ecrit la chaine formatee dans dest.
  * @par Exemple d'utilisation
  * @code
  * char temps[9];
  * FormaterTempsEcoule(temps, sizeof(temps));
  * // temps contient par exemple "00:02:37"
  * @endcode
  * @ingroup grp_exti
  */
static void FormaterTempsEcoule(char *dest, size_t taille)
{
    uint32_t total_sec = HAL_GetTick() / 1000;
    uint32_t heures    = (total_sec / 3600) % 100;  // plafonne a 99h, evite le debordement du format
    uint32_t minutes   = (total_sec / 60) % 60;
    uint32_t secondes  = total_sec % 60;

    snprintf(dest, taille, "%02lu:%02lu:%02lu",
             (unsigned long)heures, (unsigned long)minutes, (unsigned long)secondes);
}

/**
  * @brief  Verifie si une adresse (format HAL) fait partie d'une liste
  *         donnee. Fonction utilitaire generique, reutilisable pour
  *         n'importe quelle liste d'adresses (exclusion, inclusion, ...).
  * @param  adresse  Adresse a rechercher (format HAL, deja decalee).
  * @param  liste    Tableau d'adresses (format HAL) dans lequel chercher.
  * @param  taille   Nombre d'elements dans liste.
  * @retval int  1 si l'adresse est trouvee dans la liste, 0 sinon.
  * @par Exemple d'utilisation
  * @code
  * static const uint16_t exclues[] = { 0xC4 };
  * if (EstDansListe(0xC4, exclues, 1)) { / * ignorer * / }
  * @endcode
  * @ingroup grp_i2c
  */
static int EstDansListe(uint16_t adresse, const uint16_t *liste, uint8_t taille)
{
    for (uint8_t i = 0; i < taille; i++)
    {
        if (liste[i] == adresse)
        {
            return 1;
        }
    }
    return 0;
}

/**
  * @brief  Envoie un message "Bonjour" a chacune des adresses trouvees
  *         par un scan I2C prealable (ScanI2C()), SAUF celles listees
  *         dans adresses_exclues (utile pour eviter d'envoyer du texte
  *         libre a un appareil qui attend un protocole binaire strict,
  *         comme un DAC - il repondrait par un NACK/ERROR sans que ce
  *         soit une vraie panne). Le message inclut : l'identifiant du
  *         maitre (STM32, texte fixe - un maitre I2C n'a pas d'adresse
  *         de bus a proprement parler), l'adresse de l'esclave
  *         destinataire, et le temps ecoule depuis le demarrage du
  *         STM32 (voir FormaterTempsEcoule()). Ajoute une ligne de
  *         compte-rendu dans rapport_scan pour chaque envoi (et pour
  *         chaque adresse ignoree).
  * @param  hi2c              Pointeur vers le handle I2C a utiliser (&hi2c1, &hi2c2, ...).
  * @param  nom_bus           Nom du bus a afficher dans le rapport (ex : "I2C1").
  * @param  result            Pointeur vers un resultat de scan deja rempli par ScanI2C().
  * @param  adresses_exclues  Tableau d'adresses (format HAL) a ne PAS contacter.
  *                           Passer NULL et 0 si aucune exclusion.
  * @param  nb_exclues        Nombre d'elements dans adresses_exclues.
  * @retval Aucun (void). Complete rapport_scan.
  * @par Exemple d'utilisation
  * @code
  * static const uint16_t exclues[] = { (0x62 << 1) };  // le DAC
  * EnvoyerBonjourATous(&hi2c1, "I2C1", &res_i2c1, exclues, 1);
  * // ou, sans aucune exclusion :
  * EnvoyerBonjourATous(&hi2c1, "I2C1", &res_i2c1, NULL, 0);
  * @endcode
  * @ingroup grp_i2c
  */
static void EnvoyerBonjourATous(I2C_HandleTypeDef *hi2c, const char *nom_bus, const ScanResult_t *result,
                                 const uint16_t *adresses_exclues, uint8_t nb_exclues)
{
    char temps[9];
    FormaterTempsEcoule(temps, sizeof(temps));

    for (uint8_t i = 0; i < result->addr_cpt; i++)
    {
        uint16_t adresse_hal = result->addr_trouve[i];

        if (EstDansListe(adresse_hal, adresses_exclues, nb_exclues))
        {
            char ligne_skip[48];
            int len_skip = sprintf(ligne_skip, "%s:0x%02X:Bonjour:SKIP\n", nom_bus, adresse_hal);
            size_t used_skip = strlen(rapport_scan);
            if (used_skip + (size_t)len_skip < sizeof(rapport_scan))
            {
                strcat(rapport_scan, ligne_skip);
            }
            continue;
        }

        char message[64];
        int len_msg = sprintf(message, "Bonjour Maitre:STM32 Esclave:0x%02X Temps:%s\n",
                               adresse_hal, temps);

        osMutexId m = ResoudreMutexI2C(hi2c);
        osMutexWait(m, osWaitForever);
        HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(hi2c, adresse_hal,
                                                        (uint8_t *)message, (uint16_t)len_msg, 100);
        osMutexRelease(m);

        char ligne[48];
        int len = sprintf(ligne, "%s:0x%02X:Bonjour:%s\n",
                           nom_bus, adresse_hal, (st == HAL_OK) ? "OK" : "ERROR");
        size_t used = strlen(rapport_scan);
        if (used + (size_t)len < sizeof(rapport_scan))
        {
            strcat(rapport_scan, ligne);
        }

        HAL_Delay(10);
    }
}

// ---------------------------------------------------------------------------
// Bouton USER (B1, PC13) : declenche une capture complete
// ---------------------------------------------------------------------------

// Etat partage entre l'ISR (HAL_GPIO_EXTI_Callback) et la boucle principale
// (StartLwipTask) : le flag est mis a 1 par l'interruption, et consomme/remis
// a 0 par la boucle qui appelle alors ExecuterCaptureComplete(). On ne fait
// JAMAIS de traitement lourd (I2C, UART) directement dans l'ISR.
static volatile uint8_t bouton_appuye = 0;
static uint32_t dernier_appui_ms = 0;   // pour l'anti-rebond (debounce)

// ---------------------------------------------------------------------------
// Bouton-poussoir de RESET materiel du multiplexeur (broche RESET des 3
// ADG728, cablee separement sur PD0/EXTI0 pour que le firmware sache que
// l'appui a eu lieu). Meme principe que le bouton B1 ci-dessus : l'ISR ne
// fait que positionner un flag, le vrai traitement se fait en tache de fond.
// ---------------------------------------------------------------------------
static volatile uint8_t mux_reset_appuye = 0;
static uint32_t dernier_reset_mux_ms = 0;   // anti-rebond

// Derniers canaux selectionnes sur chaque chaine (0-23 tension, 0-15
// courant), 0xFF = aucun. Declarees ici (avant TraiterResetMultiplexeur
// ci-dessous et avant SelectionnerCanalMuxTension()/Courant(), plus loin
// dans ce fichier) car utilisees par plusieurs fonctions.
static uint8_t canal_mux_tension_actuel = 0xFF;
static uint8_t canal_mux_courant_actuel = 0xFF;

/**
  * @brief  Configure PD0 en entree avec interruption, pour detecter les
  *         appuis sur le bouton-poussoir cable sur la broche RESET des
  *         ADG728. A appeler depuis MX_GPIO_Init_2, comme pour B1.
  *         ATTENTION : polarite (front montant ou descendant) et
  *         necessite d'un pull-up/pull-down externe ou interne a
  *         verifier selon le cablage reel du bouton et de la broche
  *         RESET de l'ADG728 (actif haut ou bas, non confirme cote
  *         datasheet au moment de l'ecriture de ce code). Si les deux
  *         chaines (tension/courant) ont chacune leur PROPRE ligne
  *         RESET physique separee, il faudra dupliquer ce mecanisme
  *         sur une deuxieme broche/interruption.
  * @param  Aucun.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * // Fin de MX_GPIO_Init(), zone USER CODE MX_GPIO_Init_2 :
  * MX_MuxResetSync_GPIO_Init();   // PD0 -> EXTI0 -> mux_reset_appuye
  * @endcode
  * @ingroup grp_adc
  */
static void MX_MuxResetSync_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;  // A ADAPTER selon la polarite reelle du bouton/RESET
    GPIO_InitStruct.Pull = GPIO_NOPULL;           // A ADAPTER si pas de pull-up/down externe deja present
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/**
  * @brief  Resynchronise l'etat logiciel des DEUX chaines de
  *         multiplexage (tension et courant) avec la realite materielle,
  *         apres un appui sur le bouton de RESET des ADG728 : le reset
  *         materiel coupe tous les canaux, quel que soit ce que le
  *         firmware pensait actif avant l'appui - sans cette
  *         resynchronisation, GET_MUX_CANAL/GET_MUX_CANAL_COURANT
  *         renverraient un canal qui n'est plus reellement selectionne.
  * @param  Aucun.
  * @retval Aucun (void). Remet les deux canaux actuels a 0xFF, et
  *         journalise l'evenement sur UART.
  * @par Exemple d'utilisation
  * @code
  * Appelee depuis la boucle de StartLwipTask(), quand
  * mux_reset_appuye est a 1 (mis par l'interruption PD0) :
  *     if (mux_reset_appuye) { mux_reset_appuye = 0; TraiterResetMultiplexeur(); }
  * @endcode
  * @ingroup grp_adc
  */
static void TraiterResetMultiplexeur(void)
{
    canal_mux_tension_actuel = 0xFF;
    canal_mux_courant_actuel = 0xFF;
    UART1_EnvoyerTexte("MUX:RESET_MATERIEL_DETECTE\n");
}

// Resultats de scan et compteur de cycle : globaux (et non plus locaux a la
// boucle de main(), qui ne s'execute plus une fois le RTOS demarre), pour
// etre accessibles depuis ExecuterCaptureComplete().
static ScanResult_t res_i2c1;
static ScanResult_t res_i2c2;
static uint32_t numero_cycle = 0;

/**
  * @brief  Capture complete de diagnostic : scan I2C1+I2C2, message
  *         "Bonjour" a chaque adresse trouvee, ecriture d'un palier
  *         different sur chacune des 8 sorties du DAC, balayage complet
  *         de VOUT0 (0 a 4095), ecriture de 2.5V sur VOUT1. Le tout
  *         encadre par des marqueurs SAVE_BEGIN/SAVE_END (avec numero
  *         de cycle et heure ecoulee), envoye en une fois sur USART1.
  *         Les marqueurs SAVE_BEGIN/SAVE_END permettent a un outil PC
  *         (ex : le script Python de controle) de detecter le debut et
  *         la fin d'une capture dans le flux UART, et de la sauvegarder
  *         automatiquement en fichier des qu'elle est complete.
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : ecrit sur le bus I2C1, incremente
  *         numero_cycle, envoie plusieurs kilo-octets sur USART1
  *         (duree totale ~8-10 secondes, dominee par BalayerSortieDAC()).
  * @par Exemple d'utilisation
  * @code
  * Appelee depuis la boucle de StartLwipTask() quand bouton_appuye
  * est a 1 (mis par l'interruption du bouton B1/PC13) :
  *     if (bouton_appuye) { bouton_appuye = 0; ExecuterCaptureComplete(); }
  * @endcode
  * @ingroup grp_exti
  */
static void ExecuterCaptureComplete(void)
{
    numero_cycle++;

    // 0. Marqueur de debut, avec numero de cycle et heure ecoulee.
    char temps_debut[9];
    FormaterTempsEcoule(temps_debut, sizeof(temps_debut));
    rapport_scan[0] = '\0';
    {
        char ligne[48];
        sprintf(ligne, "=== SAVE_BEGIN cycle=%lu heure=%s ===\n",
                (unsigned long)numero_cycle, temps_debut);
        strcat(rapport_scan, ligne);
    }
    UART1_EnvoyerTexte(rapport_scan);   // envoye tout de suite : le PC sait qu'une capture demarre

    // 1. Scan complet des bus I2C1 et I2C2.
    ScanI2C(&hi2c1, &res_i2c1);
    ScanI2C(&hi2c2, &res_i2c2);

    rapport_scan[0] = '\0';
    FormaterRapport("I2C1", &res_i2c1);
    FormaterRapport("I2C2", &res_i2c2);

    // 2. Message "Bonjour" a chaque adresse trouvee (DAC exclu : format
    //    binaire strict, pas de texte libre - voir EnvoyerBonjourATous()).
    // Les deux MCP47FEB28 cables sur I2C1 (0xC0 et 0xC2) sont exclus : le texte
    // "Bonjour..." serait interprete comme des commandes (ex : 'B' = 0x42 vise le
    // registre 0x08 VREF) et pourrait modifier la configuration du DAC.
    static const uint16_t adresses_exclues_i2c1[] = { (0x60 << 1), (0x61 << 1), (0x62 << 1) };
    EnvoyerBonjourATous(&hi2c1, "I2C1", &res_i2c1, adresses_exclues_i2c1, 3);
    EnvoyerBonjourATous(&hi2c2, "I2C2", &res_i2c2, NULL, 0);

    // 3. Palier different sur chacune des 8 sorties du DAC (0x62).
    EcrireToutesLesSorties(&hi2c1, (0x62 << 1), "DAC1");

    UART1_EnvoyerTexte(rapport_scan);   // envoie le scan+Bonjour+paliers avant le balayage (qui prend du temps)

    // 4. Balayage complet de VOUT0 (0 a 4095, pas 50, 100ms -> ~8s),
    //    rapporte ligne par ligne en direct sur l'UART (voir BalayerSortieDAC()).
    BalayerSortieDAC(&hi2c1, (0x62 << 1), 0x00, "DAC1", 4095, 50, 100, DAC_VDD_REFERENCE_V);

    // 5. Ecrit 2.5V sur VOUT1 via VoltageVersCodeDAC().
    rapport_scan[0] = '\0';
    {
        uint16_t code_2v5 = VoltageVersCodeDAC(2.5f, DAC_VDD_REFERENCE_V);
        HAL_StatusTypeDef st = EcrireDACVolatile(&hi2c1, (0x62 << 1), 0x01, code_2v5);
        char ligne[48];
        sprintf(ligne, "DAC1:VOUT1:2.5V->code=%u:%s\n",
                code_2v5, (st == HAL_OK) ? "OK" : "ERROR");
        strcat(rapport_scan, ligne);
    }

    // 6. Mesure physique reelle (ADC) de chaque LDO cablee. Chaque appel
    //    envoie deja son resultat sur UART (voir GetMesureReelle()) -
    //    les LDO pas encore cablees repondent simplement NON_CABLE.
    for (int i = 0; i < NB_LDO; i++)
    {
        GetMesureReelle(i, ldoConfig[i].name);
    }

    // 7. Marqueur de fin.
    {
        char temps_fin[9];
        FormaterTempsEcoule(temps_fin, sizeof(temps_fin));
        char ligne[48];
        sprintf(ligne, "=== SAVE_END cycle=%lu heure=%s ===\n",
                (unsigned long)numero_cycle, temps_fin);
        strcat(rapport_scan, ligne);
    }

    UART1_EnvoyerTexte(rapport_scan);
}

// Handle USART1 (PB6/PB7), utilise pour tout le rapport texte envoye
// vers Hercules via un adaptateur USB-UART externe (voir UART1_EnvoyerTexte()).
UART_HandleTypeDef huart1;

/**
  * @brief  Configure les broches PB6 (USART1_TX) et PB7 (USART1_RX)
  *         en Alternate Function. ATTENTION : ces broches ne sont PAS
  *         reliees au port COM virtuel du ST-LINK - il faut un
  *         adaptateur USB-UART externe branche sur PB6/PB7 pour voir
  *         quoi que ce soit dans un terminal (Hercules, etc.).
  * @param  Aucun.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * MX_USART1_GPIO_Init();
  * MX_USART1_Init();
  * @endcode
  * @ingroup grp_comm
  */
static void MX_USART1_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
  * @brief  Initialise USART1 : 115200 bauds, 8 bits, sans parite,
  *         1 bit de stop, sans controle de flux (correspond a la
  *         config Hercules : "8 b, pas de parity, handshake off").
  * @param  Aucun.
  * @retval Aucun (void). Initialise le handle global huart1.
  * @par Exemple d'utilisation
  * @code
  * MX_USART1_GPIO_Init();
  * MX_USART1_Init();
  * UART1_EnvoyerTexte("Test\r\n");
  * @endcode
  * @ingroup grp_comm
  */
static void MX_USART1_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief  Envoie une chaine de caracteres sur USART1 (PB6/PB7, via
  *         un adaptateur USB-UART externe), en mode bloquant. L'emission est
  *         protegee par uart1_mutex_id (une seule emission a la fois, depuis
  *         n'importe quelle tache) pour ne jamais perdre une ligne sur un
  *         HAL_BUSY ; sans verrou tant que le mutex n'est pas cree.
  * @param  texte  Chaine C terminee par '\0' a transmettre.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * UART1_EnvoyerTexte(rapport_scan);
  * @endcode
  * @ingroup grp_comm
  */
void UART1_EnvoyerTexte(const char *texte)
{
    if (uart1_mutex_id != NULL)
    {
        osMutexWait(uart1_mutex_id, osWaitForever);
    }

    HAL_UART_Transmit(&huart1, (uint8_t *)texte, (uint16_t)strlen(texte), 1000);

    if (uart1_mutex_id != NULL)
    {
        osMutexRelease(uart1_mutex_id);
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	int wait	=	1000000;
	while(wait-- > 0);		//Wait for sometime before initializing the system
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  HAL_Delay(1000);
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */
  //I2C1_BusRecovery();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  I2C_BusRecovery(GPIOB, GPIO_PIN_8,  GPIO_PIN_9,  GPIO_AF4_I2C1);
  I2C_BusRecovery(GPIOB, GPIO_PIN_10, GPIO_PIN_11, GPIO_AF4_I2C2);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_ADC3_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_USART1_UART_Init();
  MX_ADC2_Init();
  /* USER CODE BEGIN 2 */
  // Broches des LDO depuis configurationLDO/ldo_config.csv (EN a 0, BLD inactif,
  // OC/PG en entree) : prend le pas sur la configuration LDO du .ioc.
  LDO_GPIO_Init();
  //DAC1
  //HAL_I2C_Master_Transmit(&hi2c1,MCP4725_I2CADDR_DEFAULT,DAC1_DEF,SPI_SIZE, SPI_TIMEOUT);
  //DAC2
  //HAL_I2C_Master_Transmit(&hi2c2,MCP4725_I2CADDR_DEFAULT,DAC2_DEF,SPI_SIZE, SPI_TIMEOUT);
  //DAC3
  //HAL_I2C_Master_Transmit(&hi2c1,MPC4725_I2CADDR_A0,DAC3_DEF,SPI_SIZE, SPI_TIMEOUT);
  //DAC4
  //HAL_I2C_Master_Transmit(&hi2c2,MPC4725_I2CADDR_A1,DAC4_DEF,SPI_SIZE, SPI_TIMEOUT);
  //DAC5
  //HAL_I2C_Master_Transmit(&hi2c2,MPC4725_I2CADDR_A0,DAC5_DEF,SPI_SIZE, SPI_TIMEOUT);
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of lwipTask */
  osThreadDef(lwipTask, StartLwipTask, osPriorityNormal, 0, 256);
  lwipTaskHandle = osThreadCreate(osThread(lwipTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // Ne s'execute jamais : le RTOS (osKernelStart(), plus haut) a pris la
    // main. Le diagnostic complet (scan/DAC/Bonjour) se declenche desormais
    // via le bouton USER (B1/PC13) depuis la boucle de StartLwipTask() -
    // voir ExecuterCaptureComplete() et la gestion de bouton_appuye.
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CKPER;
  PeriphClkInitStruct.CkperClockSelection = RCC_CLKPSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_16B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.Oversampling.Ratio = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc2.Init.Resolution = ADC_RESOLUTION_16B;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc2.Init.OversamplingMode = DISABLE;
  hadc2.Init.Oversampling.Ratio = 1;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VBAT;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.Resolution = ADC_RESOLUTION_16B;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc3.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc3.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc3.Init.OversamplingMode = DISABLE;
  hadc3.Init.Oversampling.Ratio = 1;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x307075B1;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x307075B1;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, EN_LDO3_PSp_Pin|EN_LDO2_PSp_Pin|BLD_LDO1_PLn_Pin|BLD_LDO5_PSn_Pin
                          |BLD_LDO2_PLn_Pin|EN_LDO3_PLn_Pin|BLD_LDO6_PSn_Pin|EN_LDO1_PLp_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, BLD_LDO5_PSp_Pin|BLD_LDO4_PSp_Pin|EN_LDO1_PSn_Pin|BLD_LDO1_PSn_Pin
                          |EN_LDO6_PSp_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, EN_LDO1_PSp_Pin|BLD_LDO2_PSp_Pin|EN_LDO5_PSn_Pin|EN_LDO5_PSp_Pin
                          |BLD_LDO1_PSp_Pin|BLD_LDO3_PLp_Pin|EN_LDO3_PLp_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, EN_LDO4_PSp_Pin|EN_LDO2_PLp_Pin|BLD_LDO2_PSn_Pin|EN_LDO2_PSn_Pin
                          |EN_LDO2_PLn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, BLD_LDO3_PSn_Pin|EN_LDO3_PSn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, EN_LDO1_PLn_Pin|EN_LDO6_PSn_Pin|BLD_LDO1_PLp_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, BLD_LDO3_PLn_Pin|BLD_LDO2_PLp_Pin|BLD_LDO4_PSn_Pin|EN_LDO4_PSn_Pin
                          |BLD_LDO6_PSp_Pin|BLD_LDO3_PSp_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : EN_LDO3_PSp_Pin EN_LDO2_PSp_Pin BLD_LDO1_PLn_Pin BLD_LDO5_PSn_Pin
                           BLD_LDO2_PLn_Pin EN_LDO3_PLn_Pin BLD_LDO6_PSn_Pin EN_LDO1_PLp_Pin */
  GPIO_InitStruct.Pin = EN_LDO3_PSp_Pin|EN_LDO2_PSp_Pin|BLD_LDO1_PLn_Pin|BLD_LDO5_PSn_Pin
                          |BLD_LDO2_PLn_Pin|EN_LDO3_PLn_Pin|BLD_LDO6_PSn_Pin|EN_LDO1_PLp_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PG_LDO3_PSp_Pin OC_LDO2_PSp_Pin PG_LDO4_PSn_Pin OC_LDO2_PLp_Pin
                           PG_LDO3_PLn_Pin PG_LDO3_PLp_Pin */
  GPIO_InitStruct.Pin = PG_LDO3_PSp_Pin|OC_LDO2_PSp_Pin|PG_LDO4_PSn_Pin|OC_LDO2_PLp_Pin
                          |PG_LDO3_PLn_Pin|PG_LDO3_PLp_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : BLD_LDO5_PSp_Pin BLD_LDO4_PSp_Pin EN_LDO1_PSn_Pin BLD_LDO1_PSn_Pin
                           EN_LDO6_PSp_Pin */
  GPIO_InitStruct.Pin = BLD_LDO5_PSp_Pin|BLD_LDO4_PSp_Pin|EN_LDO1_PSn_Pin|BLD_LDO1_PSn_Pin
                          |EN_LDO6_PSp_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PG_LDO5_PSp_Pin OC_LDO1_PSn_Pin OC_LDO6_PSp_Pin PG_LDO6_PSp_Pin */
  GPIO_InitStruct.Pin = PG_LDO5_PSp_Pin|OC_LDO1_PSn_Pin|OC_LDO6_PSp_Pin|PG_LDO6_PSp_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : EN_LDO1_PSp_Pin BLD_LDO2_PSp_Pin EN_LDO5_PSn_Pin EN_LDO5_PSp_Pin
                           BLD_LDO1_PSp_Pin BLD_LDO3_PLp_Pin EN_LDO3_PLp_Pin */
  GPIO_InitStruct.Pin = EN_LDO1_PSp_Pin|BLD_LDO2_PSp_Pin|EN_LDO5_PSn_Pin|EN_LDO5_PSp_Pin
                          |BLD_LDO1_PSp_Pin|BLD_LDO3_PLp_Pin|EN_LDO3_PLp_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : PG_LDO2_PSp_Pin PG_LDO3_PSn_Pin OC_LDO5_PSp_Pin OC_LDO1_PSp_Pin
                           PG_LDO5_PSn_Pin OC_LDO4_PSn_Pin OC_LDO5_PSn_Pin */
  GPIO_InitStruct.Pin = PG_LDO2_PSp_Pin|PG_LDO3_PSn_Pin|OC_LDO5_PSp_Pin|OC_LDO1_PSp_Pin
                          |PG_LDO5_PSn_Pin|OC_LDO4_PSn_Pin|OC_LDO5_PSn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : EN_LDO4_PSp_Pin EN_LDO2_PLp_Pin BLD_LDO2_PSn_Pin EN_LDO2_PSn_Pin
                           EN_LDO2_PLn_Pin */
  GPIO_InitStruct.Pin = EN_LDO4_PSp_Pin|EN_LDO2_PLp_Pin|BLD_LDO2_PSn_Pin|EN_LDO2_PSn_Pin
                          |EN_LDO2_PLn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PG_LDO4_PSp_Pin OC_LDO2_PSn_Pin */
  GPIO_InitStruct.Pin = PG_LDO4_PSp_Pin|OC_LDO2_PSn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : OC_LDO4_PSp_Pin PG_LDO2_PSn_Pin OC_LDO3_PSn_Pin */
  GPIO_InitStruct.Pin = OC_LDO4_PSp_Pin|PG_LDO2_PSn_Pin|OC_LDO3_PSn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : BLD_LDO3_PSn_Pin EN_LDO3_PSn_Pin */
  GPIO_InitStruct.Pin = BLD_LDO3_PSn_Pin|EN_LDO3_PSn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : EN_LDO1_PLn_Pin EN_LDO6_PSn_Pin BLD_LDO1_PLp_Pin */
  GPIO_InitStruct.Pin = EN_LDO1_PLn_Pin|EN_LDO6_PSn_Pin|BLD_LDO1_PLp_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : PG_LDO2_PLp_Pin OC_LDO3_PLp_Pin OC_LDO6_PSn_Pin PG_LDO6_PSn_Pin
                           OC_LDO3_PLn_Pin PG_LDO1_PLn_Pin OC_LDO1_PLp_Pin */
  GPIO_InitStruct.Pin = PG_LDO2_PLp_Pin|OC_LDO3_PLp_Pin|OC_LDO6_PSn_Pin|PG_LDO6_PSn_Pin
                          |OC_LDO3_PLn_Pin|PG_LDO1_PLn_Pin|OC_LDO1_PLp_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : PG_LDO1_PSn_Pin OC_LDO1_PLn_Pin OC_LDO2_PLn_Pin PG_LDO2_PLn_Pin
                           PG_LDO1_PLp_Pin PG_LDO1_PSp_Pin OC_LDO3_PSp_Pin */
  GPIO_InitStruct.Pin = PG_LDO1_PSn_Pin|OC_LDO1_PLn_Pin|OC_LDO2_PLn_Pin|PG_LDO2_PLn_Pin
                          |PG_LDO1_PLp_Pin|PG_LDO1_PSp_Pin|OC_LDO3_PSp_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : BLD_LDO3_PLn_Pin BLD_LDO2_PLp_Pin BLD_LDO4_PSn_Pin EN_LDO4_PSn_Pin
                           BLD_LDO6_PSp_Pin BLD_LDO3_PSp_Pin */
  GPIO_InitStruct.Pin = BLD_LDO3_PLn_Pin|BLD_LDO2_PLp_Pin|BLD_LDO4_PSn_Pin|EN_LDO4_PSn_Pin
                          |BLD_LDO6_PSp_Pin|BLD_LDO3_PSp_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*AnalogSwitch Config */
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA0, SYSCFG_SWITCH_PA0_CLOSE);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;      // Alternate Function Open-Drain
  GPIO_InitStruct.Pull = GPIO_NOPULL;          // pull-up externe déjà présent
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;   // à vérifier dans l'onglet Pinout de CubeMX
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // I2C2 : SCL = PB10, SDA = PB11 (a verifier dans CubeMX, AF4 standard
  // pour I2C2 sur cette combinaison de broches en H7, mais a confirmer)
  GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Bouton USER B1 (PC13) : tire a la masse quand presse, pull-up deja
  // present sur la carte Nucleo -> GPIO_NOPULL, interruption front
  // descendant. Voir bouton_appuye / ExecuterCaptureComplete().
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitStruct.Pin   = GPIO_PIN_13;
  GPIO_InitStruct.Mode  = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  // Bouton-poussoir de RESET materiel des 3 ADG728 (PD0/EXTI0) - voir
  // MX_MuxResetSync_GPIO_Init() pour le detail et les points a verifier
  // (polarite, pull-up/down).
  MX_MuxResetSync_GPIO_Init();
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief  Initialise le serveur UDP applicatif.
  *         Cree un PCB UDP (udp_new), le lie a l'adresse IP et au port
  *         definis par les macros IP4_1..IP4_4 et UDP_SERVER_PORT (main.h),
  *         puis enregistre udp_receive_callback() comme fonction de reception.
  *         En cas d'echec de udp_bind(), le PCB est libere (udp_remove).
  * @param  Aucun.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * Appelee une seule fois au demarrage de la tache reseau :
  *     osDelay(1000);
  *     udpServer_init();
  * @endcode
  * @ingroup grp_comm
  */
void udpServer_init(void)
{
  struct	udp_pcb	*upcb;
  err_t		err;
  ip_addr_t	myIPADDR;
  IP_ADDR4(&myIPADDR,IP4_1,IP4_2,IP4_3,IP4_4);

  upcb	=	udp_new();

  UART1_EnvoyerTexte("[UDP] Initialisation...\r\n");

  if (upcb == NULL)
  {
    UART1_EnvoyerTexte("[UDP] ERREUR: udp_new() == NULL\r\n");
    return;
  }

  err	=	udp_bind(upcb, &myIPADDR,UDP_SERVER_PORT);

  if(err == ERR_OK)
  {
	  char msg[96];
	  snprintf(msg, sizeof(msg), "[UDP] BIND OK %d.%d.%d.%d:%d\r\n",
	           IP4_1, IP4_2, IP4_3, IP4_4, UDP_SERVER_PORT);
	  UART1_EnvoyerTexte(msg);
	  udp_recv(upcb, udp_receive_callback, NULL);
  }
  else
  {
	  char msg[64];
	  snprintf(msg, sizeof(msg), "[UDP] BIND ERROR = %d\r\n", (int)err);
	  UART1_EnvoyerTexte(msg);
	  udp_remove(upcb);
  }
}



/* =====================================================================
   udp_receive_callback
   ===================================================================== */
/**
  * @brief  Callback LwIP invoque automatiquement a chaque paquet UDP recu
  *         sur le port du serveur (enregistre par udpServer_init via udp_recv).
  *         Recupere le message recu, appelle tc_request() pour l'interpreter
  *         et construire la reponse, puis renvoie cette reponse a l'expediteur.
  *         Chaque commande est tracee sur l'UART ("[UDP] RX commande", "[UDP]
  *         REPONSE", "[UDP] TX OK"), SAUF "WHO" : c'est la sonde de liaison que
  *         l'interface PC envoie en continu, sa trace noierait le journal UART.
  * @param  arg   Parametre optionnel LwIP (non utilise ici).
  * @param  upcb  Pointeur vers le PCB UDP ayant recu le paquet.
  * @param  p     Buffer LwIP (pbuf) contenant les donnees recues (p->payload).
  * @param  addr  Adresse IP de l'expediteur.
  * @param  port  Port UDP de l'expediteur.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * Jamais appelee directement dans le code applicatif : c'est LwIP
  * qui l'invoque des qu'un datagramme arrive sur le port ecoute
  * (voir udp_recv(upcb, udp_receive_callback, NULL) dans udpServer_init).
  * @endcode
  * @ingroup grp_comm
  */
void udp_receive_callback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	struct pbuf *txBuf = NULL;
	char buf[150];
	char rxbuf[150];
	u16_t rxlen;
	int len;
	err_t err;
	uint8_t trace_uart;

	(void)arg;

	if (p == NULL)
	{
		return;
	}

	/*
	 * p->payload n'est PAS garanti termine par '\0' et un datagramme peut
	 * etre compose de plusieurs segments pbuf. Les handlers de tc_request()
	 * utilisent strsep()/strcmp(), donc ils ont besoin d'une vraie chaine C.
	 */
	rxlen = (p->tot_len < (sizeof(rxbuf) - 1U)) ? p->tot_len : (sizeof(rxbuf) - 1U);
	pbuf_copy_partial(p, rxbuf, rxlen, 0);
	rxbuf[rxlen] = '\0';
	Commande_Verrouiller();   /* UDP et UART ne traitent jamais deux commandes en meme temps */
	last_message = rxbuf;

	/* "WHO" = sonde de liaison de l'interface PC (toutes les ~1.5 s) : pas de trace UART. */
	trace_uart = (strcmp(rxbuf, "WHO") != 0) ? 1U : 0U;

	if (trace_uart != 0U)
	{
		UART1_EnvoyerTexte("[UDP] RX commande: ");
		UART1_EnvoyerTexte(rxbuf);
		UART1_EnvoyerTexte("\r\n");
	}

	tc_request();

	if (trace_uart != 0U)
	{
		UART1_EnvoyerTexte("[UDP] REPONSE: ");
		UART1_EnvoyerTexte(response);
		UART1_EnvoyerTexte("\r\n");
	}

	/* Reponse construite par tc_request(). */
	len = snprintf(buf, sizeof(buf), "%s", response);
	if (len < 0)
	{
		len = 0;
	}
	if ((size_t)len >= sizeof(buf))
	{
		len = (int)sizeof(buf) - 1;
	}
	Commande_Deverrouiller();

	txBuf = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
	if (txBuf != NULL)
	{
		pbuf_take(txBuf, buf, (u16_t)len);
		/*
		 * Ne pas udp_connect() le PCB serveur : udp_connect() modifie son
		 * etat et peut filtrer les paquets entrants sur le peer connecte.
		 * udp_sendto() repond directement au client qui vient d'envoyer le
		 * datagramme, tout en conservant le serveur UDP generique.
		 */
		err = udp_sendto(upcb, txBuf, addr, port);

		if (err == ERR_OK)
		{
			if (trace_uart != 0U)
			{
				UART1_EnvoyerTexte("[UDP] TX OK\r\n");
			}
		}
		else
		{
			char msg[64];
			snprintf(msg, sizeof(msg), "[UDP] TX ERROR = %d\r\n", (int)err);
			UART1_EnvoyerTexte(msg);
		}
		pbuf_free(txBuf);
	}

	pbuf_free(p);
	snprintf(response, sizeof(response), "%s", "no message");
}

/* =====================================================================
   Fonction Utilitaires LDO génériques (EN/BL/STATUS )
   ===================================================================== */
/**
  * @brief  Recherche l'indice (0 a NB_LDO-1) d'une LDO dans le tableau
  *         ldoConfig[] a partir de son suffixe de nom.
  * @param  suffix  Chaine de caracteres identifiant la LDO, sans le prefixe
  *                 de commande (ex : "1PSp", "3PLn").
  * @retval int  Indice trouve dans ldoConfig[] (0..17), ou -1 si le suffixe
  *              ne correspond a aucune LDO connue.
  * @par Exemple d'utilisation
  * @code
  * int idx = findLdoIndex("2PSn");   // idx == 3
  * int bad = findLdoIndex("ALL");    // bad == -1
  * @endcode
  * @ingroup grp_config
  */
static int findLdoIndex(const char *suffix)
{
	for (int i = 0; i < NB_LDO; i++)
		if (strcmp(ldoConfig[i].name, suffix) == 0)
			return i;
	return -1;
}

/**
  * @brief  Construit la chaine de statut complete d'une LDO et la place
  *         dans la variable globale response, pour repondre a une commande
  *         "GET_STATUS_LDO_<nom>".
  *         Cas particulier : pour les LDO de type "PL_p" (ex: 1PLp, 2PLp,
  *         3PLp), qui n'ont pas de mesure de courant, le champ Current
  *         affiche "NONE" au lieu d'une valeur numerique.
  *
  *         ATTENTION a l'interpretation du champ OC : ce n'est PAS un
  *         defaut absolu, mais la sortie d'un comparateur analogique
  *         dont le SEUIL de declenchement est fixe par la valeur
  *         "Current" elle-meme (via un pont diviseur pilote par le DAC
  *         courant, voir le commentaire au-dessus de la declaration des
  *         struct LDO_x_PS_p/n plus haut dans ce fichier). Un meme
  *         courant reel peut donc donner OC=0 ou OC=1 selon la valeur
  *         de Current configuree au moment de la lecture - toujours
  *         lire les deux champs ensemble, jamais OC isolement.
  * @param  idx  Indice de la LDO dans ldoConfig[] (0..17), typiquement
  *              obtenu via findLdoIndex().
  * @retval Aucun (void). Ecrit la chaine formatee dans response, ex :
  *         "LDO_1PSp:: En:1, Bl:0, PG:1, OC:0, Voltage:2047, Current:2047,
  *         delta_ON:0, delta_OFF:0".
  * @par Exemple d'utilisation
  * @code
  * int idx = findLdoIndex("1PSp");
  * if (idx != -1) buildStatusResponse(idx);
  * @endcode
  * @ingroup grp_etat
  */
static void buildStatusResponse(int idx)
{
	LDO_TypeDef *l = ldoConfig[idx].ldo;
	// Pas de DAC de courant dans la configuration (colonne dac_courant = "-") :
	// "Current:NONE". Ne depend plus du nom de la LDO (13PSp = ex-1PLp, etc.).
	const int sans_courant = (ldoConfig[idx].dac_courant == CIBLE_AUCUNE);
	if (sans_courant)
	{
		sprintf(response, "LDO_%s:: En:%d, Bl:%d, PG:%d, OC:%d, Voltage:%d, Current:NONE, delta_ON:%d, delta_OFF:%d, OCV:%u, CMDBLD:%u, CMDOC:%u\n",
			ldoConfig[idx].name, l->EN, l->BLD, l->PG, l->OC, l->VAL, l->RET, l->RET_OFF, (unsigned)oc_verrou[idx],
			(unsigned)cmd_on_off_bld[idx], (unsigned)cmd_on_off_oc[idx]);
	}
	else
	{
		sprintf(response, "LDO_%s:: En:%d, Bl:%d, PG:%d, OC:%d, Voltage:%d, Current:%d, delta_ON:%d, delta_OFF:%d, OCV:%u, CMDBLD:%u, CMDOC:%u\n",
			ldoConfig[idx].name, l->EN, l->BLD, l->PG, l->OC, l->VAL, l->CURRENT, l->RET, l->RET_OFF, (unsigned)oc_verrou[idx],
			(unsigned)cmd_on_off_bld[idx], (unsigned)cmd_on_off_oc[idx]);
	}
}

/* =====================================================================
   HANDLERS pour la table de dispatch
   Chaque handler lit la suite du message via last_message si besoin
   (exactement comme le faisait chaque bloc if/else avant).
   ===================================================================== */

/**
  * @brief  Handler de la commande reseau "SET_EN_LDO_ALL".
  *         Parcourt les 18 LDO et active (Enable=1) pour chacune d'entre elles
  *         (tableau global EN_LDO[NB_LDO]).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : EN_LDO[0..17] = 1.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_EN_LDO_ALL" -> h_SET_EN_LDO_ALL();
  * @endcode
  * @ingroup grp_etat
  */
static void h_SET_EN_LDO_ALL(void)  { for (int i = 0; i < NB_LDO; i++) { if (oc_verrou[i] == 0U) { EN_LDO[i] = 1; } } }
/**
  * @brief  Handler de la commande reseau "RST_EN_LDO_ALL".
  *         Parcourt les 18 LDO et desactive (Enable=0) pour chacune d'entre elles
  *         (tableau global EN_LDO[NB_LDO]).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : EN_LDO[0..17] = 0.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "RST_EN_LDO_ALL" -> h_RST_EN_LDO_ALL();
  * @endcode
  * @ingroup grp_etat
  */
static void h_RST_EN_LDO_ALL(void)  { for (int i = 0; i < NB_LDO; i++) EN_LDO[i] = 0; }
/**
  * @brief  Handler de la commande reseau "SET_BL_LDO_ALL".
  *         Parcourt les 18 LDO et active le bleeder (Bl=1) pour chacune d'entre elles
  *         (tableau global BL_LDO[NB_LDO]).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : BL_LDO[0..17] = 1.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_BL_LDO_ALL" -> h_SET_BL_LDO_ALL();
  * @endcode
  * @ingroup grp_etat
  */
static void h_SET_BL_LDO_ALL(void)  { for (int i = 0; i < NB_LDO; i++) { if (cmd_on_off_bld[i] != 0U) { BL_LDO[i] = 1; } } }
/**
  * @brief  Handler de la commande reseau "RST_BL_LDO_ALL".
  *         Parcourt les 18 LDO et desactive le bleeder (Bl=0) pour chacune d'entre elles
  *         (tableau global BL_LDO[NB_LDO]).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : BL_LDO[0..17] = 0.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "RST_BL_LDO_ALL" -> h_RST_BL_LDO_ALL();
  * @endcode
  * @ingroup grp_etat
  */
static void h_RST_BL_LDO_ALL(void)  { for (int i = 0; i < NB_LDO; i++) { if (cmd_on_off_bld[i] != 0U) { (void)LDO_RelacherBleeder(i); } } }

/**
  * @brief  Handler de la commande reseau "SET_EN_TAB".
  *         Renseigne en une seule fois l'etat Enable des 18 LDO a partir
  *         d'un tableau texte recu dans la trame.
  * @param  Aucun parametre C. Lit la variable globale last_message, au format
  *         attendu "SET_EN_TAB[v0,v1,v2,...,v17]" ou chaque vN vaut 0 ou 1.
  * @retval Aucun (void). Effet de bord : EN_LDO[0..17] mis a jour.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_EN_TAB[1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0]"
  * -> tc_request() appelle automatiquement h_SET_EN_TAB();
  * @endcode
  * @ingroup grp_etat
  */
static void h_SET_EN_TAB(void)
{
	// Trame incomplete : on s'arrete au premier element manquant (plus de
	// atoi(NULL), qui provoquait une HardFault et figeait les EN en place).
	// L'activation reste soumise au verrou OC et au seuil (LDO_MettreAJour()).
	char *token = strsep(&last_message, "[");
	for (int i = 0; i < NB_LDO; i++)
	{
		token = strsep(&last_message, (i < (NB_LDO - 1)) ? "," : "]");
		if (token == NULL) { return; }
		EN_LDO[i] = (_Bool)atoi(token);
	}
}

/**
  * @brief  Handler de la commande reseau "SET_BL_TAB".
  *         Renseigne en une seule fois l'etat Bleeder des 18 LDO a partir
  *         d'un tableau texte recu dans la trame.
  * @param  Aucun parametre C. Lit la variable globale last_message, au format
  *         attendu "SET_BL_TAB[v0,v1,v2,...,v17]" ou chaque vN vaut 0 ou 1.
  * @retval Aucun (void). Effet de bord : BL_LDO[0..17] mis a jour.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_BL_TAB[1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0]"
  * -> tc_request() appelle automatiquement h_SET_BL_TAB();
  * @endcode
  * @ingroup grp_etat
  */
static void h_SET_BL_TAB(void)
{
	// Trame incomplete : arret au premier element manquant (meme garde que
	// h_SET_EN_TAB : plus de atoi(NULL) -> HardFault).
	char *token = strsep(&last_message, "[");
	for (int i = 0; i < NB_LDO; i++)
	{
		token = strsep(&last_message, (i < (NB_LDO - 1)) ? "," : "]");
		if (token == NULL) { return; }
		if (cmd_on_off_bld[i] == 0U) { continue; }   /* BL OFF : non commandable */
		if (atoi(token) != 0) { BL_LDO[i] = 1; } else { (void)LDO_RelacherBleeder(i); }
	}
}

// ---- DELTA ----
/**
  * @brief  Handler de la commande reseau "SET_DELTA_DVDD_AVDD".
  *         Met a jour le delai de sequencement delta_dvdd_avdd (en ms) a partir
  *         de la valeur recue apres le "'" dans la trame.
  * @param  Aucun parametre C. La valeur est lue dans la variable globale
  *         last_message, au format attendu "SET_DELTA_DVDD_AVDD:<valeur_ms>".
  * @retval Aucun (void). Effet de bord : delta_dvdd_avdd mis a jour.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DELTA_DVDD_AVDD:750" -> delta_dvdd_avdd = 750;
  * @endcode
  * @ingroup grp_seq
  */
static void h_SET_DELTA_DVDD_AVDD(void) { char *t = strsep(&last_message, ":"); delta_dvdd_avdd = atoi(t); }
/**
  * @brief  Handler de la commande reseau "SET_DELTA_AVDD_VREF".
  *         Met a jour le delai de sequencement delta_avdd_vref (en ms) a partir
  *         de la valeur recue apres le "'" dans la trame.
  * @param  Aucun parametre C. La valeur est lue dans la variable globale
  *         last_message, au format attendu "SET_DELTA_AVDD_VREF:<valeur_ms>".
  * @retval Aucun (void). Effet de bord : delta_avdd_vref mis a jour.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DELTA_AVDD_VREF:750" -> delta_avdd_vref = 750;
  * @endcode
  * @ingroup grp_seq
  */
static void h_SET_DELTA_AVDD_VREF(void) { char *t = strsep(&last_message, ":"); delta_avdd_vref = atoi(t); }
/**
  * @brief  Handler de la commande reseau "SET_DELTA_VREF_NRST".
  *         Met a jour le delai de sequencement delta_vref_nrst (en ms) a partir
  *         de la valeur recue apres le "'" dans la trame.
  * @param  Aucun parametre C. La valeur est lue dans la variable globale
  *         last_message, au format attendu "SET_DELTA_VREF_NRST:<valeur_ms>".
  * @retval Aucun (void). Effet de bord : delta_vref_nrst mis a jour.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DELTA_VREF_NRST:750" -> delta_vref_nrst = 750;
  * @endcode
  * @ingroup grp_seq
  */
static void h_SET_DELTA_VREF_NRST(void) { char *t = strsep(&last_message, ":"); delta_vref_nrst = atoi(t); }
/**
  * @brief  Handler de la commande reseau "SET_DELTA_NRST_VREF".
  *         Met a jour le delai de sequencement delta_nrst_vref (en ms) a partir
  *         de la valeur recue apres le "'" dans la trame.
  * @param  Aucun parametre C. La valeur est lue dans la variable globale
  *         last_message, au format attendu "SET_DELTA_NRST_VREF:<valeur_ms>".
  * @retval Aucun (void). Effet de bord : delta_nrst_vref mis a jour.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DELTA_NRST_VREF:750" -> delta_nrst_vref = 750;
  * @endcode
  * @ingroup grp_seq
  */
static void h_SET_DELTA_NRST_VREF(void) { char *t = strsep(&last_message, ":"); delta_nrst_vref = atoi(t); }
/**
  * @brief  Handler de la commande reseau "SET_DELTA_VREF_AVDD".
  *         Met a jour le delai de sequencement delta_vref_avdd (en ms) a partir
  *         de la valeur recue apres le "'" dans la trame.
  * @param  Aucun parametre C. La valeur est lue dans la variable globale
  *         last_message, au format attendu "SET_DELTA_VREF_AVDD:<valeur_ms>".
  * @retval Aucun (void). Effet de bord : delta_vref_avdd mis a jour.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DELTA_VREF_AVDD:750" -> delta_vref_avdd = 750;
  * @endcode
  * @ingroup grp_seq
  */
static void h_SET_DELTA_VREF_AVDD(void) { char *t = strsep(&last_message, ":"); delta_vref_avdd = atoi(t); }
/**
  * @brief  Handler de la commande reseau "SET_DELTA_AVDD_DVDD".
  *         Met a jour le delai de sequencement delta_avdd_dvdd (en ms) a partir
  *         de la valeur recue apres le "'" dans la trame.
  * @param  Aucun parametre C. La valeur est lue dans la variable globale
  *         last_message, au format attendu "SET_DELTA_AVDD_DVDD:<valeur_ms>".
  * @retval Aucun (void). Effet de bord : delta_avdd_dvdd mis a jour.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DELTA_AVDD_DVDD:750" -> delta_avdd_dvdd = 750;
  * @endcode
  * @ingroup grp_seq
  */
static void h_SET_DELTA_AVDD_DVDD(void) { char *t = strsep(&last_message, ":"); delta_avdd_dvdd = atoi(t); }
/**
  * @brief  Handler de la commande reseau "RST_DELTA_DVDD_AVDD".
  *         Reinitialise le delai delta_dvdd_avdd a sa valeur par defaut (DELTA_INIT_DVDD_AVDD).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : delta_dvdd_avdd = DELTA_INIT_DVDD_AVDD.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "RST_DELTA_DVDD_AVDD" -> delta_dvdd_avdd = DELTA_INIT_DVDD_AVDD;
  * @endcode
  * @ingroup grp_seq
  */
static void h_RST_DELTA_DVDD_AVDD(void) { delta_dvdd_avdd = DELTA_INIT_DVDD_AVDD; }
/**
  * @brief  Handler de la commande reseau "RST_DELTA_AVDD_VREF".
  *         Reinitialise le delai delta_avdd_vref a sa valeur par defaut (DELTA_INIT_AVDD_VREF).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : delta_avdd_vref = DELTA_INIT_AVDD_VREF.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "RST_DELTA_AVDD_VREF" -> delta_avdd_vref = DELTA_INIT_AVDD_VREF;
  * @endcode
  * @ingroup grp_seq
  */
static void h_RST_DELTA_AVDD_VREF(void) { delta_avdd_vref = DELTA_INIT_AVDD_VREF; }
/**
  * @brief  Handler de la commande reseau "RST_DELTA_VREF_NRST".
  *         Reinitialise le delai delta_vref_nrst a sa valeur par defaut (DELTA_INIT_VREF_NRST).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : delta_vref_nrst = DELTA_INIT_VREF_NRST.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "RST_DELTA_VREF_NRST" -> delta_vref_nrst = DELTA_INIT_VREF_NRST;
  * @endcode
  * @ingroup grp_seq
  */
static void h_RST_DELTA_VREF_NRST(void) { delta_vref_nrst = DELTA_INIT_VREF_NRST; }
/**
  * @brief  Handler de la commande reseau "RST_DELTA_NRST_VREF".
  *         Reinitialise le delai delta_nrst_vref a sa valeur par defaut (DELTA_INIT_NRST_VREF).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : delta_nrst_vref = DELTA_INIT_NRST_VREF.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "RST_DELTA_NRST_VREF" -> delta_nrst_vref = DELTA_INIT_NRST_VREF;
  * @endcode
  * @ingroup grp_seq
  */
static void h_RST_DELTA_NRST_VREF(void) { delta_nrst_vref = DELTA_INIT_NRST_VREF; }
/**
  * @brief  Handler de la commande reseau "RST_DELTA_VREF_AVDD".
  *         Reinitialise le delai delta_vref_avdd a sa valeur par defaut (DELTA_INIT_VREF_AVDD).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : delta_vref_avdd = DELTA_INIT_VREF_AVDD.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "RST_DELTA_VREF_AVDD" -> delta_vref_avdd = DELTA_INIT_VREF_AVDD;
  * @endcode
  * @ingroup grp_seq
  */
static void h_RST_DELTA_VREF_AVDD(void) { delta_vref_avdd = DELTA_INIT_VREF_AVDD; }
/**
  * @brief  Handler de la commande reseau "RST_DELTA_AVDD_DVDD".
  *         Reinitialise le delai delta_avdd_dvdd a sa valeur par defaut (DELTA_INIT_AVDD_DVDD).
  * @param  Aucun.
  * @retval Aucun (void). Effet de bord : delta_avdd_dvdd = DELTA_INIT_AVDD_DVDD.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "RST_DELTA_AVDD_DVDD" -> delta_avdd_dvdd = DELTA_INIT_AVDD_DVDD;
  * @endcode
  * @ingroup grp_seq
  */
static void h_RST_DELTA_AVDD_DVDD(void) { delta_avdd_dvdd = DELTA_INIT_AVDD_DVDD; }

// ---------------------------------------------------------------------------
// Mapping DAC <-> LDO, desormais PILOTE PAR LOGICIEL (table modifiable a
// chaud) plutot que fige dans 36 fonctions individuelles. Remplace
// l'ancien systeme "un handler par LDO x par type (VAL/CURRENT)".
// ---------------------------------------------------------------------------

// Mapping DAC <-> LDO en RAM (modifiable a chaud via
// SET_DAC_MAPPING_LDO_<nom>:<V|I>:<0-5>:<0-7>, 0 = CIBLE_AUCUNE). Rempli au
// demarrage par LDO_InitialiserTables() a partir de ldoConfig[] : la valeur
// initiale se configure UNIQUEMENT dans ldoConfig[] (plus de double saisie).
static MappingDacLdo_t mappingDacLdo[NB_LDO];

/* ---------------------------------------------------------------------------
 * Protection surintensite (OC) - revision du 13/10/2026.
 * Constantes dans main.h (CARTE_LIM_CORRIGEE, OC_*), variables pres de
 * ldoConfig[].
 * ------------------------------------------------------------------------- */
/**
 * @brief  Indique si la LDO est surveillee : ldoConfig[idx].oc.surveille = 1 ET
 *         un DAC de seuil (dac_courant) declare dans sa ligne de ldoConfig[].
 *         Ne depend pas du mapping modifiable a chaud.
 * @param  idx  Index de la LDO (0-17).
 * @retval 1 si la LDO est surveillee, 0 sinon.
 * @par Exemple d'utilisation
 * @code
 * if (OC_EstSurveillee(findLdoIndex("2PSp")) != 0U) { ... }
 * @endcode
 * @ingroup grp_oc
 */
static uint8_t OC_EstSurveillee(int idx)
{
    return ((ldoConfig[idx].oc.surveille != 0U) && (ldoConfig[idx].dac_courant != CIBLE_AUCUNE)) ? 1U : 0U;
}

/**
 * @brief  Indique si la protection OC est AGISSANTE : LDO surveillee (circuit
 *         cable, voir OC_EstSurveillee()) ET commande CMD_ON_OFF_OC = 1 (ON).
 *         Avec CMD_ON_OFF_OC = 0, la broche OC est ignoree : pas de coupure de
 *         l'EN, pas de defaut memorise, l'EN n'attend pas de seuil valide.
 * @param  idx  Index de la LDO.
 * @retval 1 si la protection agit, 0 sinon.
 * @par Exemple d'utilisation
 * @code
 * if (OC_EstActive(findLdoIndex("2PSp")) != 0U) { ... }
 * @endcode
 * @ingroup grp_oc
 */
static uint8_t OC_EstActive(int idx)
{
    return ((OC_EstSurveillee(idx) != 0U) && (cmd_on_off_oc[idx] != 0U)) ? 1U : 0U;
}

/**
 * @brief  Verifie que le seuil programme est dans la plage specifiee du LM211
 *         (VDAC >= 0,5 V). Une LDO non surveillee est toujours valide.
 * @param  idx  Index de la LDO (0-17).
 * @retval 1 si le seuil est valide, 0 sinon.
 * @par Exemple d'utilisation
 * @code
 * if (OC_SeuilValide(findLdoIndex("2PSp")) == 0U) { refuser_activation(); }
 * @endcode
 * @ingroup grp_oc
 */
static uint8_t OC_SeuilValide(int idx)
{
    if ((OC_EstActive(idx) == 0U) || (ldoConfig[idx].seuil_par_pot != 0U))
    {
        return 1U;   /* pas de seuil DAC a attendre (OC OFF, non surveillee ou potentiometre) */
    }
    return ((CURRENT_LDO[idx] >= ldoConfig[idx].oc.seuil_min) &&
            (CURRENT_LDO[idx] <= ldoConfig[idx].oc.seuil_max)) ? 1U : 0U;
}


/**
 * @brief  Demande l'ecriture en EEPROM du MCP47FEB28 des codes DAC (tension et
 *         seuil de courant) d'une LDO declaree MEM_EEPROM dans ldoConfig[].
 *         Sans effet pour une LDO MEM_RAM. N'ecrit rien sur le bus : la boucle
 *         de StartLwipTask() fait l'ecriture (DAC_TraiterSauvegardes()).
 *         Appelable depuis l'interruption OC et depuis le contexte lwIP.
 * @param  idx       Index de la LDO (0-17).
 * @param  immediat  1 = au prochain tour de boucle (declenchement OC),
 *                   0 = apres DAC_EEPROM_DELAI_MS sans nouvelle valeur (SET_VAL).
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * DAC_DemanderSauvegarde(findLdoIndex("2PSp"), 0U);   // apres SET_VAL_LDO_2PSp
 * @endcode
 * @ingroup grp_eeprom
 */
static void DAC_DemanderSauvegarde(int idx, uint8_t immediat)
{
    if ((idx < 0) || (idx >= NB_LDO) || (ldoConfig[idx].memoire_dac != MEM_EEPROM))
    {
        return;
    }
    nv_tick_demande[idx] = HAL_GetTick();
    if (immediat != 0U)
    {
        nv_immediat[idx] = 1U;
    }
    nv_a_sauver[idx] = 1U;
}

/**
 * @brief  Lit la broche OC de la LDO et indique si elle signale une surintensite.
 * @param  idx  Index de la LDO (0-17).
 * @retval 1 si surintensite, 0 sinon.
 * @par Exemple d'utilisation
 * @code
 * uint8_t defaut = OC_LireDefaut(findLdoIndex("2PSp"));
 * @endcode
 * @ingroup grp_oc
 */
static uint8_t OC_LireDefaut(int idx)
{
    return (HAL_GPIO_ReadPin(ldoConfig[idx].oc_port, ldoConfig[idx].oc_pin) == ldoConfig[idx].oc.niveau_defaut) ? 1U : 0U;
}

/**
 * @brief  Coupe l'EN de la LDO et verrouille le defaut. N'utilise aucune API
 *         FreeRTOS : appelable depuis l'interruption EXTI ou depuis la boucle.
 * @param  idx  Index de la LDO (0-17).
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * OC_Verrouiller(findLdoIndex("2PSp"));
 * @endcode
 * @ingroup grp_oc
 */
static void OC_Verrouiller(int idx)
{
    /* 1) EN coupe tout de suite. */
    HAL_GPIO_WritePin(ldoConfig[idx].en_port, ldoConfig[idx].en_pin, GPIO_PIN_RESET);
    EN_LDO[idx] = 0;
    en_actif[idx] = 0U;
    /* 2) Bleeder : c'est le MEME drapeau que le bouton BL de l'IHM (BL_LDO[]).
     *    Le desactiver (RST_BL_LDO_<nom>) est la procedure de rearmement. */
    if ((ldoConfig[idx].bld_sur_coupure_oc != 0U) && (cmd_on_off_bld[idx] != 0U))
    {
        BL_LDO[idx] = 1;
        HAL_GPIO_WritePin(ldoConfig[idx].bld_port, ldoConfig[idx].bld_pin, ldoConfig[idx].bld_niveau_actif);
    }
    /* 3) Defaut memorise (LED OC rouge tant que BL n'est pas desactive). */
    if (oc_verrou[idx] == 0U)
    {
        oc_verrou[idx] = 1U;
        oc_nb_declenchements[idx]++;
        oc_tick_declenchement[idx] = HAL_GetTick();
        oc_a_signaler[idx] = 1U;
        /* 4) Codes DAC tension + courant gardes en EEPROM (LDO MEM_EEPROM). */
        DAC_DemanderSauvegarde(idx, 1U);
    }
}

/**
 * @brief  Desactive le bleeder d'une LDO (RST_BL_LDO_<nom>, RST_BL_LDO_ALL,
 *         SET_BL_TAB avec 0). C'est aussi la procedure de rearmement apres un
 *         declenchement OC : le defaut memorise est efface si la broche OC est
 *         revenue au repos ; sinon le bleeder reste actif et le defaut aussi.
 *         L'EN reste a 0 dans tous les cas : l'operateur le reactive ensuite.
 * @param  idx  Index de la LDO (0-17).
 * @retval 0 = bleeder desactive, aucun defaut ;
 *         1 = bleeder desactive ET defaut OC efface (rearme) ;
 *         2 = refus : la broche OC signale encore une surintensite.
 * @par Exemple d'utilisation
 * @code
 * if (LDO_RelacherBleeder(findLdoIndex("2PSp")) == 2U) { refuser(); }
 * @endcode
 * @ingroup grp_etat
 */
static uint8_t LDO_RelacherBleeder(int idx)
{
    uint8_t resultat = 0U;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (oc_verrou[idx] != 0U)
    {
        if (OC_LireDefaut(idx) != 0U)
        {
            resultat = 2U;
        }
        else
        {
            oc_verrou[idx] = 0U;
            resultat = 1U;
        }
    }
    if (resultat != 2U)
    {
        /* EN deja a 0 : la boucle l'a coupe tant que BL = 1 (bld_coupe_en). On
         * n'y touche pas ici, pour que RST_BL_LDO_ALL / SET_BL_TAB ne coupent
         * pas les LDO dont le bleeder etait deja inactif. */
        BL_LDO[idx] = 0;
    }
    __set_PRIMASK(primask);
    return resultat;
}

/**
 * @brief  Indique si la surveillance est armee : EN actif depuis au moins
 *         ldoConfig[idx].oc.blanking_ms (fin du soft-start et de l'appel de courant).
 * @param  idx  Index de la LDO (0-17).
 * @retval 1 si armee, 0 sinon.
 * @par Exemple d'utilisation
 * @code
 * if (OC_EstArme(findLdoIndex("2PSp")) != 0U) { ... }
 * @endcode
 * @ingroup grp_oc
 */
static uint8_t OC_EstArme(int idx)
{
    return ((en_actif[idx] != 0U) &&
            ((HAL_GetTick() - en_tick_activation[idx]) >= (uint32_t)ldoConfig[idx].oc.blanking_ms)) ? 1U : 0U;
}

/**
 * @brief  Traitement d'un front de la broche OC en interruption.
 * @param  idx  Index de la LDO dont la broche OC a change.
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * OC_SurFrontIT(findLdoIndex("2PSp"));   // depuis HAL_GPIO_EXTI_Callback()
 * @endcode
 * @ingroup grp_oc
 */
static void OC_SurFrontIT(int idx)
{
    oc_nb_fronts[idx]++;               /* diagnostic : compte meme si non arme */
#if OC_COUPURE_AUTO
    if ((OC_EstActive(idx) != 0U) && (OC_EstArme(idx) != 0U) && (OC_LireDefaut(idx) != 0U))
    {
        OC_Verrouiller(idx);
    }
#else
    (void)idx;
#endif
}

/**
 * @brief  Met a jour EN, BLD, OC et PG d'une LDO. Remplace le corps de la
 *         boucle for de StartLwipTask(). OC est lu AVANT l'ecriture de EN.
 * @param  i  Index de la LDO (0-17).
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * for (int i = 0; i < NB_LDO; i++) { LDO_MettreAJour(i); }
 * @endcode
 * @ingroup grp_etat
 */
static void LDO_MettreAJour(int i)
{
    const LDO_Config_t *cfg = &ldoConfig[i];
    LDO_TypeDef *l = cfg->ldo;
    uint8_t defaut = OC_LireDefaut(i);
    uint8_t activer;
    uint8_t bleeder;
    uint32_t primask;

    if (defaut != 0U)
    {
        oc_vu_actif[i] = 1U;           /* diagnostic : remis a 0 par GET_DIAG_LDO_ */
    }

#if OC_COUPURE_AUTO
    /* Couvre un front manque et un defaut deja present a la fin du masquage. */
    if ((OC_EstActive(i) != 0U) && (defaut != 0U) && (OC_EstArme(i) != 0U))
    {
        OC_Verrouiller(i);
    }
#endif

    /* Section critique courte : l'IT EXTI ne doit pas verrouiller entre la
     * decision et l'ecriture de EN/BLD, sinon EN serait remis a 1 (ou le
     * bleeder a 0) pendant un tour. */
    primask = __get_PRIMASK();
    __disable_irq();
    /* Bleeder : UNIQUEMENT le drapeau BL_LDO[] (bouton BL de l'IHM). Le
     * declenchement OC le met a 1 (OC_Verrouiller()), RST_BL_LDO_ le remet a 0
     * et rearme (LDO_RelacherBleeder()). S'il est actif et que bld_coupe_en = 1,
     * la commande EN est annulee (le LDO ne doit pas debiter dans le bleeder). */
    bleeder = ((BL_LDO[i] != 0) && (cmd_on_off_bld[i] != 0U)) ? 1U : 0U;   /* BL OFF : jamais actif */
    if ((bleeder != 0U) && (cfg->bld_coupe_en != 0U))
    {
        EN_LDO[i] = 0;
    }
    activer = ((EN_LDO[i] == 1) && (oc_verrou[i] == 0U) && (OC_SeuilValide(i) != 0U)) ? 1U : 0U;
    if ((activer != 0U) && (en_actif[i] == 0U))
    {
        en_tick_activation[i] = HAL_GetTick();
    }
    en_actif[i] = activer;
    HAL_GPIO_WritePin(cfg->en_port, cfg->en_pin, (activer != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    /* Bleeder ecrit APRES l'EN (EN deja a 0 si bld_coupe_en). Polarite
     * donnee par bld_niveau_actif. */
    HAL_GPIO_WritePin(cfg->bld_port, cfg->bld_pin,
                      (bleeder != 0U) ? cfg->bld_niveau_actif
                                      : ((cfg->bld_niveau_actif == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET));
    __set_PRIMASK(primask);

    l->EN  = (_Bool)(HAL_GPIO_ReadPin(cfg->en_port, cfg->en_pin) == GPIO_PIN_SET);
    l->BLD = (_Bool)bleeder;
    l->OC  = (_Bool)defaut;            /* 1 = surintensite, quelle que soit la polarite */
    l->PG  = HAL_GPIO_ReadPin(cfg->pg_port, cfg->pg_pin);
}

/* ===========================================================================
 *  COMMANDES ON/OFF PAR LDO : CMD_ON_OFF_BLD et CMD_ON_OFF_OC (revision 22/10/2026)
 *  - 1 = ON  : fonctionnement normal (protection OC, bleeder commandable).
 *  - 0 = OFF : OC ignore (pas de coupure, pas de defaut) / bleeder relache et
 *              non commandable (SET_BL / RST_BL repondent REFUS_BL_OFF).
 *  Valeur au premier demarrage : colonnes CMD_ON_OFF_BLD / CMD_ON_OFF_OC du CSV.
 *  Chaque bascule est gardee en flash interne du STM32 (banque 2, secteur 7,
 *  128 Ko reserves dans STM32H753ZITX_FLASH.ld) et reprise au demarrage, tant
 *  que la configuration compilee (LDO_CONFIG_CRC32) est la meme. Si le CSV
 *  change, les valeurs du nouveau CSV s'appliquent.
 *  Commandes (UDP et UART) :
 *    GET_CMD_ON_OFF_BLD_LDO_<nom>    -> "CMD_ON_OFF_BLD:<nom>:0|1"
 *    GET_CMD_ON_OFF_OC_LDO_<nom>     -> "CMD_ON_OFF_OC:<nom>:0|1"
 *    SWITCH_CMD_ON_OFF_BLD_LDO_<nom> -> bascule, repond la nouvelle valeur
 *    SWITCH_CMD_ON_OFF_OC_LDO_<nom>  -> bascule, repond la nouvelle valeur
 * ======================================================================== */
#define CMD_GET_BLD          "GET_CMD_ON_OFF_BLD_LDO_"
#define CMD_GET_OC           "GET_CMD_ON_OFF_OC_LDO_"
#define CMD_SWITCH_BLD       "SWITCH_CMD_ON_OFF_BLD_LDO_"
#define CMD_SWITCH_OC        "SWITCH_CMD_ON_OFF_OC_LDO_"

#define CMD_FLASH_ADRESSE    0x081E0000UL     /* banque 2, secteur 7                     */
#define CMD_FLASH_TAILLE     0x00020000UL     /* 128 Ko                                  */
#define CMD_FLASH_ENREG      64UL             /* 1 enregistrement = 2 mots flash 256 bits */
#define CMD_FLASH_MOTS       16U              /* 64 octets en mots de 32 bits            */
#define CMD_FLASH_MAGIC      0x434D4431UL     /* "CMD1"                                  */
#define CMD_FLASH_DELAI_MS   500U             /* regroupe des bascules rapprochees       */

static volatile uint8_t  cmd_flash_a_sauver = 0U;
static volatile uint32_t cmd_flash_tick = 0U;
static uint32_t          cmd_flash_sequence = 0U;
static uint32_t          cmd_flash_prochain = 0U;   /* decalage du prochain emplacement libre */

/**
 * @brief  CRC-32 (polynome 0xEDB88320, comme zlib) d'une suite de mots 32 bits.
 * @param  mots  Donnees.
 * @param  n     Nombre de mots.
 * @retval CRC-32.
 * @par Exemple d'utilisation
 * @code
 * uint32_t c = Crc32Mots(enreg, 15U);
 * @endcode
 * @ingroup grp_cmd
 */
static uint32_t Crc32Mots(const uint32_t *mots, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFUL;
    const uint8_t *o = (const uint8_t *)mots;
    for (uint32_t i = 0; i < (n * 4U); i++)
    {
        crc ^= o[i];
        for (uint32_t b = 0; b < 8U; b++)
        {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/**
 * @brief  Charge les commandes ON/OFF au demarrage : valeurs du CSV, puis le
 *         dernier enregistrement valide de la flash s'il correspond a la meme
 *         configuration compilee (LDO_CONFIG_CRC32, meme nombre de LDO).
 *         Repere aussi le prochain emplacement libre du secteur.
 * @retval Aucun. Ligne USART1 : "CMD_ON_OFF:FLASH:SEQ=12" ou "CMD_ON_OFF:CSV".
 * @par Exemple d'utilisation
 * @code
 * CmdOnOff_Initialiser();   // avant MX_OC_EXTI_Init() et la boucle
 * @endcode
 * @ingroup grp_cmd
 */
static void CmdOnOff_Initialiser(void)
{
    const uint32_t *trouve = NULL;
    char ligne[64];

    for (int i = 0; i < NB_LDO; i++)
    {
        cmd_on_off_bld[i] = (ldoConfig[i].cmd_bld_defaut != 0U) ? 1U : 0U;
        cmd_on_off_oc[i]  = (ldoConfig[i].cmd_oc_defaut  != 0U) ? 1U : 0U;
    }

    SCB_InvalidateDCache_by_Addr((uint32_t *)CMD_FLASH_ADRESSE, (int32_t)CMD_FLASH_TAILLE);
    cmd_flash_prochain = CMD_FLASH_TAILLE;   /* secteur plein si aucun emplacement libre */
    for (uint32_t dec = 0; dec < CMD_FLASH_TAILLE; dec += CMD_FLASH_ENREG)
    {
        const uint32_t *r = (const uint32_t *)(CMD_FLASH_ADRESSE + dec);
        if (r[0] == 0xFFFFFFFFUL)
        {
            cmd_flash_prochain = dec;
            break;
        }
        if ((r[0] == CMD_FLASH_MAGIC) && (r[15] == Crc32Mots(r, 15U)))
        {
            if (r[2] >= cmd_flash_sequence)
            {
                cmd_flash_sequence = r[2];
            }
            if ((r[1] == LDO_CONFIG_CRC32) && (r[3] == (uint32_t)NB_LDO))
            {
                trouve = r;
            }
        }
    }

    if (trouve != NULL)
    {
        for (int i = 0; i < NB_LDO; i++)
        {
            cmd_on_off_bld[i] = (uint8_t)((trouve[4] >> i) & 1U);
            cmd_on_off_oc[i]  = (uint8_t)((trouve[5] >> i) & 1U);
        }
        snprintf(ligne, sizeof(ligne), "CMD_ON_OFF:FLASH:SEQ=%lu\r\n", (unsigned long)trouve[2]);
    }
    else
    {
        snprintf(ligne, sizeof(ligne), "CMD_ON_OFF:CSV (aucune sauvegarde pour cette configuration)\r\n");
    }
    UART1_EnvoyerTexte(ligne);
}

/**
 * @brief  Ecrit l'etat des commandes ON/OFF dans le prochain emplacement libre
 *         du secteur (efface le secteur quand il est plein, ~1 fois toutes les
 *         2048 sauvegardes ; l'effacement dure jusqu'a ~2 s, la LDO surveillee
 *         en interruption reste protegee pendant ce temps).
 *         A appeler depuis la boucle de StartLwipTask() uniquement.
 * @retval HAL_OK si l'enregistrement relu est conforme.
 * @par Exemple d'utilisation
 * @code
 * if (CmdOnOff_EcrireFlash() != HAL_OK) { ... }
 * @endcode
 * @ingroup grp_cmd
 */
static HAL_StatusTypeDef CmdOnOff_EcrireFlash(void)
{
    uint32_t enreg[CMD_FLASH_MOTS] __attribute__((aligned(32)));
    HAL_StatusTypeDef st = HAL_OK;
    uint32_t masque_bld = 0U, masque_oc = 0U;

    for (int i = 0; i < NB_LDO; i++)
    {
        masque_bld |= (cmd_on_off_bld[i] != 0U) ? (1UL << i) : 0UL;
        masque_oc  |= (cmd_on_off_oc[i]  != 0U) ? (1UL << i) : 0UL;
    }
    for (uint32_t k = 0; k < CMD_FLASH_MOTS; k++)
    {
        enreg[k] = 0xFFFFFFFFUL;
    }
    enreg[0] = CMD_FLASH_MAGIC;
    enreg[1] = LDO_CONFIG_CRC32;
    enreg[2] = ++cmd_flash_sequence;
    enreg[3] = (uint32_t)NB_LDO;
    enreg[4] = masque_bld;
    enreg[5] = masque_oc;
    enreg[15] = Crc32Mots(enreg, 15U);

    HAL_FLASH_Unlock();
    if ((cmd_flash_prochain + CMD_FLASH_ENREG) > CMD_FLASH_TAILLE)
    {
        FLASH_EraseInitTypeDef e = {0};
        uint32_t erreur_secteur = 0U;
        e.TypeErase    = FLASH_TYPEERASE_SECTORS;
        e.Banks        = FLASH_BANK_2;
        e.Sector       = FLASH_SECTOR_7;
        e.NbSectors    = 1U;
        e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        st = HAL_FLASHEx_Erase(&e, &erreur_secteur);
        cmd_flash_prochain = 0U;
        SCB_InvalidateDCache_by_Addr((uint32_t *)CMD_FLASH_ADRESSE, (int32_t)CMD_FLASH_TAILLE);
    }
    const uint32_t adresse = CMD_FLASH_ADRESSE + cmd_flash_prochain;
    if (st == HAL_OK)
    {
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, adresse, (uint32_t)&enreg[0]);
    }
    if (st == HAL_OK)
    {
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, adresse + 32UL, (uint32_t)&enreg[8]);
    }
    HAL_FLASH_Lock();

    cmd_flash_prochain += CMD_FLASH_ENREG;   /* emplacement consomme meme en cas d'echec */
    SCB_InvalidateDCache_by_Addr((uint32_t *)adresse, (int32_t)CMD_FLASH_ENREG);
    if ((st == HAL_OK) && (memcmp((const void *)adresse, enreg, CMD_FLASH_ENREG) != 0))
    {
        st = HAL_ERROR;
    }
    return st;
}

/**
 * @brief  Ecrit en flash les commandes ON/OFF modifiees, DELAI_MS apres la
 *         derniere bascule (plusieurs bascules rapprochees = une ecriture).
 * @retval Aucun. Ligne USART1 : "CMD_ON_OFF:SAUVE:SEQ=13:OK" (ou ":ERREUR").
 * @par Exemple d'utilisation
 * @code
 * CmdOnOff_TraiterSauvegarde();   // dans la boucle for(;;)
 * @endcode
 * @ingroup grp_cmd
 */
static void CmdOnOff_TraiterSauvegarde(void)
{
    char ligne[64];
    if ((cmd_flash_a_sauver == 0U) || ((HAL_GetTick() - cmd_flash_tick) < CMD_FLASH_DELAI_MS))
    {
        return;
    }
    cmd_flash_a_sauver = 0U;
    const HAL_StatusTypeDef st = CmdOnOff_EcrireFlash();
    if (st != HAL_OK)
    {
        cmd_flash_a_sauver = 1U;              /* nouvel essai au tour suivant */
        cmd_flash_tick = HAL_GetTick();
    }
    snprintf(ligne, sizeof(ligne), "CMD_ON_OFF:SAUVE:SEQ=%lu:%s\r\n",
             (unsigned long)cmd_flash_sequence, (st == HAL_OK) ? "OK" : "ERREUR");
    UART1_EnvoyerTexte(ligne);
}

/**
 * @brief  Bascule CMD_ON_OFF_OC d'une LDO. Passage a OFF : la broche OC est
 *         ignoree et un defaut memorise est efface (l'EN reste a 0). Passage
 *         a ON : surveillance immediate (pas de masquage si l'EN est deja actif).
 * @param  idx  Index de la LDO.
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * CmdOnOff_BasculerOc(findLdoIndex("2PSp"));
 * @endcode
 * @ingroup grp_cmd
 */
static void CmdOnOff_BasculerOc(int idx)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (cmd_on_off_oc[idx] != 0U)
    {
        cmd_on_off_oc[idx] = 0U;
        oc_verrou[idx] = 0U;
    }
    else
    {
        cmd_on_off_oc[idx] = 1U;
    }
    __set_PRIMASK(primask);
    cmd_flash_tick = HAL_GetTick();
    cmd_flash_a_sauver = 1U;
}

/**
 * @brief  Bascule CMD_ON_OFF_BLD d'une LDO. Passage a OFF : bleeder relache
 *         tout de suite et plus commandable. Passage a ON : bleeder commandable,
 *         inactif au depart.
 * @param  idx  Index de la LDO.
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * CmdOnOff_BasculerBld(findLdoIndex("2PSp"));
 * @endcode
 * @ingroup grp_cmd
 */
static void CmdOnOff_BasculerBld(int idx)
{
    const LDO_Config_t *cfg = &ldoConfig[idx];
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    BL_LDO[idx] = 0;
    cmd_on_off_bld[idx] = (cmd_on_off_bld[idx] != 0U) ? 0U : 1U;
    if ((cmd_on_off_bld[idx] == 0U) && (LDO_BrocheBouton(cfg->bld_port, cfg->bld_pin) == 0U))
    {
        HAL_GPIO_WritePin(cfg->bld_port, cfg->bld_pin,
                          (cfg->bld_niveau_actif == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
    __set_PRIMASK(primask);
    cmd_flash_tick = HAL_GetTick();
    cmd_flash_a_sauver = 1U;
}

/**
 * @brief  Reponse des commandes GET/SWITCH_CMD_ON_OFF_*.
 * @param  variable  "CMD_ON_OFF_BLD" ou "CMD_ON_OFF_OC".
 * @param  idx       Index de la LDO.
 * @param  valeur    Valeur courante (0 ou 1).
 * @retval Aucun. Ecrit "CMD_ON_OFF_OC:2PSp:1" dans response.
 * @par Exemple d'utilisation
 * @code
 * CmdOnOff_Repondre("CMD_ON_OFF_OC", idx, cmd_on_off_oc[idx]);
 * @endcode
 * @ingroup grp_cmd
 */
static void CmdOnOff_Repondre(const char *variable, int idx, uint8_t valeur)
{
    sprintf(response, "%s:%s:%u\n", variable, ldoConfig[idx].name, (unsigned)(valeur != 0U));
}

/* ===========================================================================
 *  RECEPTION DES COMMANDES SUR L'UART (USART1, PB6/PB7, 115200 8N1)
 *  Toute commande UDP est aussi acceptee sur l'UART : une ligne terminee par
 *  CR ou LF. La reception se fait en interruption (un octet a la fois), la
 *  ligne est executee par la boucle de StartLwipTask() et la reponse est
 *  renvoyee sur l'UART. Un verrou (tc_mutex) garantit que l'UDP et l'UART ne
 *  traitent jamais deux commandes en meme temps (tc_request() utilise les
 *  globales last_message et response).
 * ======================================================================== */
#define UART_CMD_TAILLE      150U

static osMutexId         tc_mutex_id = NULL;
osMutexDef(tc_mutex);
static uint8_t           uart_rx_octet;
static char              uart_rx_ligne[UART_CMD_TAILLE];
static volatile uint16_t uart_rx_longueur = 0U;
static char              uart_cmd[UART_CMD_TAILLE];
static volatile uint8_t  uart_cmd_prete = 0U;
static volatile uint32_t uart_cmd_perdues = 0U;

/**
 * @brief  Cree le verrou des commandes (UDP + UART). A appeler avant
 *         udpServer_init().
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * CreerMutexCommandes();
 * @endcode
 * @ingroup grp_comm
 */
static void CreerMutexCommandes(void)
{
    tc_mutex_id = osMutexCreate(osMutex(tc_mutex));
}

/**
 * @brief  Prend le verrou des commandes (tc_mutex). Sans effet tant qu'il
 *         n'est pas cree. Garantit que tc_request() (qui utilise les globales
 *         last_message et response) n'est jamais execute par l'UDP et l'UART
 *         en meme temps.
 * @param  Aucun.
 * @retval Aucun (void).
 * @par Exemple d'utilisation
 * @code
 * Commande_Verrouiller();
 * last_message = ligne;
 * tc_request();
 * Commande_Deverrouiller();
 * @endcode
 * @ingroup grp_comm
 */
static void Commande_Verrouiller(void)
{
    if (tc_mutex_id != NULL)
    {
        osMutexWait(tc_mutex_id, osWaitForever);
    }
}

/**
 * @brief  Rend le verrou des commandes pris par Commande_Verrouiller().
 * @param  Aucun.
 * @retval Aucun (void).
 * @par Exemple d'utilisation
 * @code
 * Commande_Verrouiller();
 * tc_request();
 * Commande_Deverrouiller();   // toujours apres la copie de response
 * @endcode
 * @ingroup grp_comm
 */
static void Commande_Deverrouiller(void)
{
    if (tc_mutex_id != NULL)
    {
        osMutexRelease(tc_mutex_id);
    }
}

/**
 * @brief  Demarre la reception des commandes sur USART1 (interruption, 1 octet).
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * UART1_DemarrerReceptionCommandes();   // apres MX_USART1_UART_Init()
 * @endcode
 * @ingroup grp_comm
 */
static void UART1_DemarrerReceptionCommandes(void)
{
    HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    (void)HAL_UART_Receive_IT(&huart1, &uart_rx_octet, 1U);
}

/**
 * @brief  Interruption USART1 : delegue au HAL, qui appelle
 *         HAL_UART_RxCpltCallback() a chaque octet recu (commandes UART).
 * @param  Aucun.
 * @retval Aucun (void).
 * @par Exemple d'utilisation
 * @code
 * // appele par le materiel (vecteur USART1_IRQn, priorite 6), ne pas appeler directement
 * @endcode
 * @ingroup grp_comm
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/**
 * @brief  Octet recu sur USART1 : assemble la ligne ; CR ou LF la termine et
 *         la confie a la boucle (une ligne en attente au plus, les suivantes
 *         sont comptees comme perdues). Aucune API FreeRTOS ici.
 * @param  huart  Handle UART de l'interruption.
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * // appele par HAL_UART_IRQHandler() ; la ligne "WHO\r" donne uart_cmd = "WHO",
 * // uart_cmd_prete = 1, puis UART1_TraiterCommande() l'execute dans la boucle
 * @endcode
 * @ingroup grp_comm
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }
    const char c = (char)uart_rx_octet;
    if ((c == '\r') || (c == '\n'))
    {
        if (uart_rx_longueur > 0U)
        {
            if (uart_cmd_prete == 0U)
            {
                memcpy(uart_cmd, uart_rx_ligne, uart_rx_longueur);
                uart_cmd[uart_rx_longueur] = '\0';
                uart_cmd_prete = 1U;
            }
            else
            {
                uart_cmd_perdues++;
            }
            uart_rx_longueur = 0U;
        }
    }
    else if (uart_rx_longueur < (UART_CMD_TAILLE - 1U))
    {
        uart_rx_ligne[uart_rx_longueur++] = c;
    }
    else
    {
        uart_rx_longueur = 0U;           /* ligne trop longue : abandonnee */
        uart_cmd_perdues++;
    }
    (void)HAL_UART_Receive_IT(&huart1, &uart_rx_octet, 1U);
}

/**
 * @brief  Erreur de reception USART1 (debordement, bruit...) : relance la reception.
 * @param  huart  Handle UART.
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * // appele par le HAL sur erreur ; la ligne en cours est abandonnee
 * @endcode
 * @ingroup grp_comm
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uart_rx_longueur = 0U;
        (void)HAL_UART_Receive_IT(&huart1, &uart_rx_octet, 1U);
    }
}

/**
 * @brief  Execute la commande recue sur l'UART (s'il y en a une) et renvoie la
 *         reponse sur l'UART. Meme traitement que l'UDP (tc_request()).
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * UART1_TraiterCommande();   // dans la boucle for(;;)
 *   // terminal : "GET_CMD_ON_OFF_OC_LDO_2PSp" -> "CMD_ON_OFF_OC:2PSp:0"
 * @endcode
 * @ingroup grp_comm
 */
static void UART1_TraiterCommande(void)
{
    char ligne[UART_CMD_TAILLE];
    char reponse_uart[sizeof(response) + 4U];

    if (uart_cmd_prete == 0U)
    {
        return;
    }
    strncpy(ligne, uart_cmd, sizeof(ligne) - 1U);
    ligne[sizeof(ligne) - 1U] = '\0';
    uart_cmd_prete = 0U;

    /* Espaces de fin (terminal) retires. */
    for (size_t n = strlen(ligne); (n > 0U) && (ligne[n - 1U] == ' '); n--)
    {
        ligne[n - 1U] = '\0';
    }
    if (ligne[0] == '\0')
    {
        return;
    }

    Commande_Verrouiller();
    last_message = ligne;
    tc_request();
    snprintf(reponse_uart, sizeof(reponse_uart), "%s", response);
    Commande_Deverrouiller();

    /* Reponse terminee par CR LF pour un terminal. */
    size_t n = strlen(reponse_uart);
    while ((n > 0U) && ((reponse_uart[n - 1U] == '\n') || (reponse_uart[n - 1U] == '\r')))
    {
        reponse_uart[--n] = '\0';
    }
    UART1_EnvoyerTexte(reponse_uart);
    UART1_EnvoyerTexte("\r\n");
}

/**
 * @brief  Emet sur USART1 une ligne par defaut nouvellement verrouille.
 *         Appelee depuis la boucle de StartLwipTask(), jamais en interruption.
 * @retval Aucun. Ligne emise : "OC_TRIP:2PSp:NB=1:T_MS=123456:SEUIL=4006".
 * @par Exemple d'utilisation
 * @code
 * OC_SignalerEvenements();
 * @endcode
 * @ingroup grp_oc
 */
static void OC_SignalerEvenements(void)
{
    for (int i = 0; i < NB_LDO; i++)
    {
        if (oc_a_signaler[i] != 0U)
        {
            char ligne[96];
            oc_a_signaler[i] = 0U;
            snprintf(ligne, sizeof(ligne), "OC_TRIP:%s:NB=%lu:T_MS=%lu:SEUIL=%u\r\n",
                     ldoConfig[i].name,
                     (unsigned long)oc_nb_declenchements[i],
                     (unsigned long)oc_tick_declenchement[i],
                     (unsigned)CURRENT_LDO[i]);
            UART1_EnvoyerTexte(ligne);
        }
    }
}

/**
 * @brief  Commande "RST_OC_LDO_<nom>" : deverrouille le defaut si la broche OC
 *         est revenue au repos. L'EN reste a 0 : l'operateur doit renvoyer
 *         SET_EN_LDO_<nom>.
 * @param  idx  Index de la LDO (0-17).
 * @param  nom  Nom de la LDO, pour la reponse.
 * @retval Aucun. Reponse "OC_RST:<nom>:OK" ou "OC_RST:<nom>:REFUS_OC_ACTIF".
 * @par Exemple d'utilisation
 * @code
 * Trame UDP recue : "RST_OC_LDO_2PSp"
 * @endcode
 * @ingroup grp_oc
 */
static void OC_Rearmer(int idx, const char *nom)
{
    if (OC_LireDefaut(idx) != 0U)
    {
        sprintf(response, "OC_RST:%s:REFUS_OC_ACTIF\n", nom);
        return;
    }
    oc_verrou[idx] = 0U;
    sprintf(response, "OC_RST:%s:OK\n", nom);
}

/**
 * @brief  Commande "GET_OC_LDO_<nom>" : etat complet de la protection.
 * @param  idx  Index de la LDO (0-17).
 * @param  nom  Nom de la LDO, pour la reponse.
 * @retval Aucun. Reponse
 *         "OC_LDO:2PSp:NIV=0:VERROU=1:NB=1:T_MS=123456:SEUIL=4006:SEUIL_VALIDE=1".
 * @par Exemple d'utilisation
 * @code
 * Trame UDP recue : "GET_OC_LDO_2PSp"
 * @endcode
 * @ingroup grp_oc
 */
static void OC_Etat(int idx, const char *nom)
{
    sprintf(response, "OC_LDO:%s:NIV=%u:VERROU=%u:NB=%lu:T_MS=%lu:SEUIL=%u:SEUIL_VALIDE=%u:SOURCE=%s\n",
            nom,
            (unsigned)OC_LireDefaut(idx),
            (unsigned)oc_verrou[idx],
            (unsigned long)oc_nb_declenchements[idx],
            (unsigned long)oc_tick_declenchement[idx],
            (unsigned)CURRENT_LDO[idx],
            (unsigned)OC_SeuilValide(idx),
            (ldoConfig[idx].seuil_par_pot != 0U) ? "POT" : "DAC");
}


/**
  * @brief  Retourne le pointeur vers le tableau DACx_DEF correspondant a
  *         une cible logique.
  * @param  cible  Cible a resoudre (CIBLE_DAC1..CIBLE_DAC5).
  * @retval uint8_t*  Pointeur vers DAC1_DEF..DAC5_DEF, ou NULL si
  *                   CIBLE_AUCUNE ou valeur invalide.
  * @par Exemple d'utilisation
  * @code
  * uint8_t *tab = ResoudreTableauDac(mappingDacLdo[idx].dac_tension);
  * if (tab != NULL) { EcrireValeurDac(mappingDacLdo[idx].dac_tension, 4, 2048); }
  * @endcode
  * @ingroup grp_dac
  */
static uint8_t *ResoudreTableauDac(CibleDac_t cible)
{
    for (uint32_t k = 0; k < NB_DAC_PUCES; k++)
    {
        if ((cible != CIBLE_AUCUNE) && (tableDac[k].cible == cible))
        {
            return tableDac[k].trame;
        }
    }
    return NULL;
}

/**
  * @brief  Ecrit une valeur 12 bits dans le tableau DACx_DEF/canal
  *         designe par une cible logique - remplace les 36 blocs
  *         d'ecriture individuels (un par LDO x par type) par UNE
  *         seule fonction generique, pilotee par mappingDacLdo[].
  * @param  cible    Tableau DACx_DEF a modifier (CIBLE_AUCUNE = ignore).
  * @param  canal    Canal 0-7 dans ce tableau (structure par groupes de
  *                  3 octets : adresse, MSB, LSB).
  * @param  valeur   Valeur 12 bits a ecrire (0-4095).
  * @retval Aucun (void). Ne fait rien si cible == CIBLE_AUCUNE ou canal
  *         hors plage (protege les LDO sans DAC courant, ex : *PLp).
  * @par    Fonctionnement
  *         Version identique a celle du 07/10/2026 (ecriture directe, sans
  *         mutex ni section critique), retablie le 19/10/2026.
  * @par Exemple d'utilisation
  * @code
  * EcrireValeurDac(CIBLE_DAC2, 3, 2048);  // DAC2_DEF, canal 3
  * @endcode
  * @ingroup grp_dac
  */
static void EcrireValeurDac(CibleDac_t cible, uint8_t canal, uint16_t valeur)
{
    uint8_t *tableau = ResoudreTableauDac(cible);
    if (tableau == NULL || canal > 7)
    {
        return;
    }
    uint8_t offset_msb = (uint8_t)(canal * 3 + 1);
    uint8_t offset_lsb = (uint8_t)(canal * 3 + 2);
    tableau[offset_msb] = (uint8_t)((valeur & 0x0F00) >> 8);
    tableau[offset_lsb] = (uint8_t)(valeur & 0x00FF);
}

/**
  * @brief  Repercute une ecriture directe sur un canal DAC (balayage a
  *         distance BALAYER_DAC, capture bouton USER) dans l'etat logiciel
  *         de la ou des LDO associees a ce canal par mappingDacLdo[]
  *         (VAL_LDO/CURRENT_LDO et struct LDO). Sans cela, apres un
  *         balayage sur I2C1 0xC0 ou 0xC2, la sortie restait a la valeur
  *         finale du balayage alors que GET_STATUS_LDO_&lt;nom&gt; (page valeur
  *         LDO, page dac/&lt;nom&gt;) affichait encore l'ancienne valeur.
  * @param  tableau  Tableau DACx_DEF ecrit (DAC1_DEF ... DAC5_DEF).
  * @param  canal    Canal ecrit (0-7).
  * @param  valeur   Code 12 bits ecrit (0-4095).
  * @retval Aucun (void). Ne fait rien si aucune LDO n'est associee.
  * @par Exemple d'utilisation
  * @code
  * // Apres un balayage de VOUT3 du DAC I2C1 0xC0 jusqu'a 4095 :
  * MajEtatLdoDepuisDac(DAC1_DEF, 3, 4095);  // -> VAL_LDO[0] (1PSp) = 4095
  * @endcode
  * @ingroup grp_dac
  */
static void MajEtatLdoDepuisDac(const uint8_t *tableau, uint8_t canal, uint16_t valeur)
{
    if (tableau == NULL || canal > 7)
    {
        return;
    }
    for (int i = 0; i < NB_LDO; i++)
    {
        const MappingDacLdo_t *m = &mappingDacLdo[i];
        if (m->dac_tension != CIBLE_AUCUNE && ResoudreTableauDac(m->dac_tension) == tableau
            && m->canal_tension == canal)
        {
            VAL_LDO[i] = valeur;
            ldoConfig[i].ldo->VAL = valeur;
        }
        if (m->dac_courant != CIBLE_AUCUNE && ResoudreTableauDac(m->dac_courant) == tableau
            && m->canal_courant == canal)
        {
            CURRENT_LDO[i] = valeur;
            ldoConfig[i].ldo->CURRENT = valeur;
        }
    }
}

/**
  * @brief  Handler GENERIQUE de "SET_VAL_LDO_<nom>:<valeur>" - remplace
  *         les 18 anciennes fonctions h_SET_VAL_LDO_<nom> individuelles.
  *         Met a jour VAL_LDO[idx]/l'etat logiciel de la LDO, PUIS
  *         repercute la valeur sur le DAC reellement associe via
  *         mappingDacLdo[idx] (modifiable a chaud, voir SetMappingDacLdo()).
  * @param  idx  Index de la LDO (0-17), deja resolu par findLdoIndex()
  *              dans tc_request() (branche a prefixe "SET_VAL_LDO_").
  * @retval Aucun (void). Lit last_message (deja positionne apres le
  *         ':' par tc_request()) pour la valeur (0-4095).
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_VAL_LDO_1PSp:2048"
  * -> idx=findLdoIndex("1PSp")=0, h_SET_VAL_LDO_GENERIQUE(0);
  * @endcode
  * @ingroup grp_dac
  */
static void h_SET_VAL_LDO_GENERIQUE(int idx)
{
    uint16_t valeur = (uint16_t)atoi(last_message);
    VAL_LDO[idx] = valeur;
    ldoConfig[idx].ldo->VAL = valeur;

    MappingDacLdo_t *m = &mappingDacLdo[idx];
    EcrireValeurDac(m->dac_tension, m->canal_tension, valeur);
    DAC_DemanderSauvegarde(idx, 0U);   /* EEPROM si MEM_EEPROM, apres DAC_EEPROM_DELAI_MS */
}

/**
  * @brief  Handler GENERIQUE de "SET_CURRENT_LDO_<nom>:<valeur>" -
  *         remplace les 18 anciennes fonctions h_SET_CURRENT_LDO_<nom>.
  *         Repond "NOT FOR YOU" (comme avant) si la LDO n'a pas de DAC
  *         courant associe (mappingDacLdo[idx].dac_courant == CIBLE_AUCUNE,
  *         cas des 3 LDO *PLp).
  * @param  idx  Index de la LDO (0-17), deja resolu par findLdoIndex().
  * @retval Aucun (void). Lit last_message pour la valeur (0-4095).
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_CURRENT_LDO_1PSp:1500"
  * @endcode
  * @ingroup grp_dac
  */
static void h_SET_CURRENT_LDO_GENERIQUE(int idx)
{
    MappingDacLdo_t *m = &mappingDacLdo[idx];

    if (m->dac_courant == CIBLE_AUCUNE)
    {
        sprintf(response, "NOT FOR YOU\n");
        return;
    }

    char *fin = NULL;
    long v;

    if ((OC_EstSurveillee(idx) != 0U) && (ldoConfig[idx].seuil_par_pot != 0U))
    {
        sprintf(response, "CURRENT:%s:REFUS_SEUIL_POTENTIOMETRE\n", ldoConfig[idx].name);
        return;
    }
    if (last_message == NULL)
    {
        sprintf(response, "CURRENT:%s:PARAMETRE_ABSENT\n", ldoConfig[idx].name);
        return;
    }
    // Bornes propres a la LDO (ligne de ldoConfig[], champ oc).
    const uint16_t code_min = ldoConfig[idx].oc.seuil_min;
    const uint16_t code_max = ldoConfig[idx].oc.seuil_max;
    v = strtol(last_message, &fin, 10);
    if ((fin == last_message) || (v < (long)code_min) || (v > (long)code_max))
    {
        sprintf(response, "CURRENT:%s:HORS_PLAGE_%u_%u\n", ldoConfig[idx].name,
                (unsigned)code_min, (unsigned)code_max);
        return;
    }
    uint16_t valeur = (uint16_t)v;
    CURRENT_LDO[idx] = valeur;
    ldoConfig[idx].ldo->CURRENT = valeur;
    if (OC_EstSurveillee(idx) != 0U)
    {
        EcrireValeurDac(ldoConfig[idx].dac_courant, ldoConfig[idx].canal_courant, valeur);
    }
    else
    {
        EcrireValeurDac(m->dac_courant, m->canal_courant, valeur);
    }
    DAC_DemanderSauvegarde(idx, 0U);   /* EEPROM si MEM_EEPROM, apres DAC_EEPROM_DELAI_MS */
}

/**
  * @brief  Programme le seuil de courant par defaut (ldoConfig[i].oc.seuil_defaut,
  *         0,50 A par defaut) sur toutes les LDO surveillees, au demarrage.
  *         Sans cela, le seuil vaudrait 0 V (MCP4725_VAL_DEF), hors de la
  *         plage d'entree du LM211, et l'activation serait refusee. Doit etre
  *         appelee apres LDO_InitialiserTables() et avant tout traitement de
  *         commande UDP.
  * @param  Aucun.
  * @retval Aucun. Met a jour CURRENT_LDO[], ldo->CURRENT et DACx_DEF ; la
  *         boucle de StartLwipTask() l'envoie au DAC au tour suivant.
  * @par Exemple d'utilisation
  * @code
  * LDO_InitialiserTables();
  * OC_InitialiserSeuilsDefaut();
  * @endcode
  * @ingroup grp_oc
  */
static void OC_InitialiserSeuilsDefaut(void)
{
    for (int i = 0; i < NB_LDO; i++)
    {
        if ((OC_EstSurveillee(i) != 0U) && (ldoConfig[i].seuil_par_pot == 0U))
        {
            const uint16_t code = ldoConfig[i].oc.seuil_defaut;
            CURRENT_LDO[i] = code;
            ldoConfig[i].ldo->CURRENT = code;
            EcrireValeurDac(ldoConfig[i].dac_courant, ldoConfig[i].canal_courant, code);
        }
    }
}

/**
 * @brief  Cherche la LDO surveillee dont le seuil de courant est sur le canal
 *         donne d'un tableau DACx_DEF.
 * @param  tableau  Tableau DACx_DEF (DAC1_DEF ... DAC5_DEF), peut etre NULL.
 * @param  canal    Canal (0-7).
 * @retval Index de la LDO (0-17), ou -1 si ce canal n'est pas un seuil OC.
 * @par Exemple d'utilisation
 * @code
 * int idx = OC_LdoDuCanalSeuil(DAC1_DEF, 5);   // -> 2 (2PSp)
 * @endcode
 * @ingroup grp_oc
 */
static int OC_LdoDuCanalSeuil(const uint8_t *tableau, uint8_t canal)
{
    if (tableau == NULL)
    {
        return -1;
    }
    for (int i = 0; i < NB_LDO; i++)
    {
        if ((OC_EstSurveillee(i) != 0U) && (ldoConfig[i].canal_courant == canal)
            && (ResoudreTableauDac(ldoConfig[i].dac_courant) == tableau))
        {
            return i;
        }
    }
    return -1;
}

/**
  * @brief  Handler de "SET_DAC_MAPPING_LDO_<nom>:<V|I>:<0-5>:<0-7>" -
  *         modifie A CHAUD, sans recompilation ni reflash, quel DAC/canal
  *         alimente la tension (V) ou le courant (I) d'une LDO.
  * @param  idx  Index de la LDO (0-17), deja resolu par findLdoIndex().
  * @param  nom  Nom texte de la LDO, pour la reponse.
  * @retval Aucun (void). Lit last_message au format "<V|I>:<0-5>:<0-7>"
  *         (0 pour le numero de DAC = CIBLE_AUCUNE, "debranche" ce canal).
  *         Remplit response avec "DAC_MAP:<nom>:OK\n" si valide,
  *         "DAC_MAP:<nom>:ERREUR_PARAMETRES\n" si la trame est mal formee, ou
  *         "DAC_MAP:<nom>:REFUS_PROTECTION_OC\n" si la demande deplace le
  *         seuil OC d'une LDO surveillee ou vise un canal de seuil OC.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DAC_MAPPING_LDO_1PSp:V:2:3"
  * -> tension de 1PSp desormais lue/ecrite sur DAC2_DEF, canal 3.
  * @endcode
  * @ingroup grp_dac
  */
static void SetMappingDacLdo(int idx, const char *nom)
{
    char *type_str  = strsep(&last_message, ":");
    char *dac_str   = strsep(&last_message, ":");
    char *canal_str = strsep(&last_message, ":");

    if (type_str == NULL || dac_str == NULL || canal_str == NULL)
    {
        sprintf(response, "DAC_MAP:%s:ERREUR_PARAMETRES\n", nom);
        return;
    }

    int numero_dac = atoi(dac_str);
    int canal = atoi(canal_str);

    if (numero_dac < 0 || numero_dac > 5 || canal < 0 || canal > 7)
    {
        sprintf(response, "DAC_MAP:%s:ERREUR_PARAMETRES\n", nom);
        return;
    }

    // Protection OC : le canal de seuil d'une LDO surveillee ne peut ni etre
    // deplace, ni etre attribue a une autre grandeur (sinon SET_VAL_LDO_ ou un
    // autre SET_CURRENT_LDO_ ecrirait le seuil sans bornage). Une entree
    // identique au mapping en place est acceptee (rejeu de la configuration).
    {
        const int est_courant = (type_str[0] == 'I' || type_str[0] == 'i') ? 1 : 0;
        const int meme_seuil = (est_courant != 0) && (OC_EstSurveillee(idx) != 0U)
                               && (numero_dac == (int)ldoConfig[idx].dac_courant)
                               && (canal == (int)ldoConfig[idx].canal_courant);
        const int ldo_seuil = (numero_dac == 0) ? -1
                              : OC_LdoDuCanalSeuil(ResoudreTableauDac((CibleDac_t)numero_dac), (uint8_t)canal);
        if ((meme_seuil == 0) && (((est_courant != 0) && (OC_EstSurveillee(idx) != 0U)) || (ldo_seuil >= 0)))
        {
            sprintf(response, "DAC_MAP:%s:REFUS_PROTECTION_OC\n", nom);
            return;
        }
    }

    if (type_str[0] == 'V' || type_str[0] == 'v')
    {
        mappingDacLdo[idx].dac_tension = (CibleDac_t)numero_dac;
        mappingDacLdo[idx].canal_tension = (uint8_t)canal;
    }
    else if (type_str[0] == 'I' || type_str[0] == 'i')
    {
        mappingDacLdo[idx].dac_courant = (CibleDac_t)numero_dac;
        mappingDacLdo[idx].canal_courant = (uint8_t)canal;
    }
    else
    {
        sprintf(response, "DAC_MAP:%s:ERREUR_PARAMETRES\n", nom);
        return;
    }

    sprintf(response, "DAC_MAP:%s:OK\n", nom);
}

/**
  * @brief  Handler de "GET_DAC_MAPPING_LDO_<nom>" - lit la correspondance
  *         DAC/canal actuellement active pour une LDO (tension et
  *         courant), sans la modifier.
  * @param  idx  Index de la LDO (0-17), deja resolu par findLdoIndex().
  * @param  nom  Nom texte de la LDO, pour la reponse.
  * @retval Aucun (void). Remplit response au format
  *         "DAC_MAP:<nom>:V=<dac>.<canal>:I=<dac>.<canal>\n" (dac=0
  *         signifie CIBLE_AUCUNE, aucun DAC associe a ce canal).
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_DAC_MAPPING_LDO_1PSp"
  * Reponse type    : "DAC_MAP:1PSp:V=2.3:I=2.1\n"
  * @endcode
  * @ingroup grp_dac
  */
static void GetMappingDacLdo(int idx, const char *nom)
{
    MappingDacLdo_t *m = &mappingDacLdo[idx];
    sprintf(response, "DAC_MAP:%s:V=%d.%d:I=%d.%d\n",
            nom, (int)m->dac_tension, m->canal_tension,
            (int)m->dac_courant, m->canal_courant);
}


// ---- ADC / WHO ----
/**
  * @brief  Handler de la commande reseau "GET_CURRENT_1".
  *         Declenche une conversion sur ADC1, la convertit en pourcentage
  *         (0-100) via map(), et prepare la reponse texte.
  * @param  Aucun.
  * @retval Aucun (void). Ecrit le resultat dans la variable globale response,
  *         format "Current ADC 1 = <valeur>\n".
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_CURRENT_1" -> reponse "Current ADC 1 = 42".
  * @endcode
  * @ingroup grp_adc
  */
static void h_GET_CURRENT_1(void)
{
	uint16_t ADC_VAL = 0; int value = 0;
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 100);
	ADC_VAL = HAL_ADC_GetValue(&hadc1);
	HAL_ADC_Stop(&hadc1);
	value = map(ADC_VAL, 0, 65535, 0, 100);
	sprintf(response, "Current ADC 1 = %d\n", value);
}
/**
  * @brief  Handler de la commande reseau "GET_CURRENT_2".
  *         Declenche une conversion sur ADC3, la convertit en pourcentage
  *         (0-100) via map(), et prepare la reponse texte.
  * @param  Aucun.
  * @retval Aucun (void). Ecrit le resultat dans la variable globale response,
  *         format "Current ADC 2 = <valeur>\n".
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_CURRENT_2" -> reponse "Current ADC 2 = 17".
  * @endcode
  * @ingroup grp_adc
  */
static void h_GET_CURRENT_2(void)
{
	uint16_t ADC_VAL = 0; int value = 0;
	HAL_ADC_Start(&hadc3);
	HAL_ADC_PollForConversion(&hadc3, 100);
	ADC_VAL = HAL_ADC_GetValue(&hadc3);
	HAL_ADC_Stop(&hadc3);
	value = map(ADC_VAL, 0, 65535, 0, 100);
	sprintf(response, "Current ADC 2 = %d\n", value);
}
/**
  * @brief  Handler de la commande reseau "SCAN_I2C1".
  *         Relance un scan complet du bus I2C1 (ScanI2C()) et renvoie le
  *         contenu integral de la structure ScanResult_t (compteurs +
  *         liste des adresses trouvees) dans la reponse UDP.
  * @param  Aucun.
  * @retval Aucun (void). Remplit response avec le format :
  *         "SCAN:I2C1:ok=<n>:err=<n>:busy=<n>:to=<n>:trouve=0xNN,0xNN,...\n"
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SCAN_I2C1" -> h_SCAN_I2C1();
  * Reponse type : "SCAN:I2C1:ok=1:err=126:busy=0:to=0:trouve=0xC4\n"
  * @endcode
  * @ingroup grp_i2c
  */
static void h_SCAN_I2C1(void)
{
    ScanI2C(&hi2c1, &res_i2c1);

    int len = sprintf(response, "SCAN:I2C1:ok=%lu:err=%lu:busy=%lu:to=%lu:trouve=",
                       (unsigned long)res_i2c1.count_ok, (unsigned long)res_i2c1.count_error,
                       (unsigned long)res_i2c1.count_busy, (unsigned long)res_i2c1.count_timeout);

    for (uint8_t i = 0; i < res_i2c1.addr_cpt && len < (int)sizeof(response) - 8; i++)
    {
        len += sprintf(response + len, "0x%02X%s", res_i2c1.addr_trouve[i],
                        (i < res_i2c1.addr_cpt - 1) ? "," : "");
    }
    sprintf(response + len, "\n");
}

/**
  * @brief  Handler de la commande reseau "SCAN_I2C2". Identique a
  *         h_SCAN_I2C1() mais sur le bus I2C2.
  * @param  Aucun.
  * @retval Aucun (void). Remplit response, meme format que h_SCAN_I2C1()
  *         avec "I2C2" a la place de "I2C1".
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SCAN_I2C2" -> h_SCAN_I2C2();
  * @endcode
  * @ingroup grp_i2c
  */
static void h_SCAN_I2C2(void)
{
    ScanI2C(&hi2c2, &res_i2c2);

    int len = sprintf(response, "SCAN:I2C2:ok=%lu:err=%lu:busy=%lu:to=%lu:trouve=",
                       (unsigned long)res_i2c2.count_ok, (unsigned long)res_i2c2.count_error,
                       (unsigned long)res_i2c2.count_busy, (unsigned long)res_i2c2.count_timeout);

    for (uint8_t i = 0; i < res_i2c2.addr_cpt && len < (int)sizeof(response) - 8; i++)
    {
        len += sprintf(response + len, "0x%02X%s", res_i2c2.addr_trouve[i],
                        (i < res_i2c2.addr_cpt - 1) ? "," : "");
    }
    sprintf(response + len, "\n");
}

/**
  * @brief  Handler de la commande reseau "BALAYER_DAC". Declenche
  *         BalayerSortieDAC() a distance, avec tous ses parametres
  *         fournis dans la trame UDP.
  *         ATTENTION : cette fonction est BLOQUANTE (comme
  *         BalayerSortieDAC() elle-meme) - pendant toute la duree du
  *         balayage, ce thread ne traite plus aucune autre commande UDP
  *         (meme comportement que la capture complete du bouton USER).
  * @param  Aucun parametre C. Lit la variable globale last_message, au
  *         format attendu (apres le premier ':' deja consomme par
  *         tc_request()) :
  *         "<bus>:<adresse_hal_hex>:<registre>:<valeur_max>:<pas>:<delai_ms>:<tension_ref>"
  *         - bus : 1 pour hi2c1, 2 pour hi2c2
  *         - adresse_hal_hex : adresse I2C au format HAL, en hexa avec 0x (ex : 0xC4)
  *         - registre : 0x00-0x07 (VOUT0-VOUT7)
  *         - valeur_max : 0-4095
  *         - pas : increment entre paliers
  *         - delai_ms : pause entre paliers, en millisecondes
  *         - tension_ref : VDD reel du DAC, en volts (ex : 5.0)
  * @retval Aucun (void). Remplit response avec "BALAYER_DAC:TERMINE\n"
  *         une fois le balayage complet termine,
  *         "BALAYER_DAC:ERREUR_PARAMETRES\n" si la trame est mal formee, ou
  *         "BALAYER_DAC:REFUS_SEUIL_OC_LDO_ACTIVE:<nom>\n" si le registre vise
  *         est le seuil OC d'une LDO active. Sur un registre de seuil OC (LDO
  *         coupee), le seuil d'avant le balayage est restaure a la fin.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue :
  * "BALAYER_DAC:1:0xC4:0:4095:50:100:5.0"
  * -> balaie VOUT0 du DAC a 0xC4 sur hi2c1, de 0 a 4095 par pas
  *    de 50, 100ms entre chaque palier, VDD=5V.
  * @endcode
  * @ingroup grp_dac
  */
static void h_BALAYER_DAC(void)
{
    int bus = 0, registre = 0, valeur_max = 0, pas = 0, delai_ms = 0;
    unsigned int adresse_hal = 0;
    char tension_str[16] = "5.0";

    // %f est volontairement evite dans sscanf() : sur newlib-nano (STM32),
    // le parsing de flottants par scanf() necessite le flag linker
    // "-u _scanf_float", separe de "-u _printf_float". Plutot que d'en
    // dependre, on lit la tension comme une chaine puis on la convertit
    // avec atof() (stdlib.h), qui ne necessite aucun flag supplementaire.
    int n = sscanf(last_message, "%d:0x%x:%d:%d:%d:%d:%15s",
                    &bus, &adresse_hal, &registre, &valeur_max, &pas, &delai_ms, tension_str);

    if (n != 7)
    {
        sprintf(response, "BALAYER_DAC:ERREUR_PARAMETRES\n");
        return;
    }

    float tension_ref = atof(tension_str);
    I2C_HandleTypeDef *hi2c = (bus == 2) ? &hi2c2 : &hi2c1;

    // Protection OC : un registre de seuil ne se balaie que LDO coupee
    // (le balayage bloque la boucle et passe par des codes hors 410..max),
    // et le seuil d'avant le balayage est restaure a la fin.
    const int ldo_seuil = ((registre >= 0) && (registre <= 7))
        ? OC_LdoDuCanalSeuil(ResoudreTableauDacParAdresse(hi2c, (uint8_t)adresse_hal), (uint8_t)registre) : -1;
    uint16_t seuil_avant = 0U;
    if (ldo_seuil >= 0)
    {
        if ((EN_LDO[ldo_seuil] != 0) || (en_actif[ldo_seuil] != 0U))
        {
            sprintf(response, "BALAYER_DAC:REFUS_SEUIL_OC_LDO_ACTIVE:%s\n", ldoConfig[ldo_seuil].name);
            return;
        }
        seuil_avant = CURRENT_LDO[ldo_seuil];
    }

    BalayerSortieDAC(hi2c, (uint8_t)adresse_hal, (uint8_t)registre, "DAC",
                      (uint16_t)valeur_max, (uint16_t)pas, (uint32_t)delai_ms, tension_ref);

    if (ldo_seuil >= 0)
    {
        CURRENT_LDO[ldo_seuil] = seuil_avant;
        ldoConfig[ldo_seuil].ldo->CURRENT = seuil_avant;
        EcrireValeurDac(ldoConfig[ldo_seuil].dac_courant, ldoConfig[ldo_seuil].canal_courant, seuil_avant);
    }

    sprintf(response, "BALAYER_DAC:TERMINE\n");
}

/**
  * @brief  Handler de la commande reseau "GET_DAC_CONFIG" - lit et
  *         renvoie les 3 registres de configuration (VREF/Power-Down/
  *         Gain) d'une puce DAC MCP47FEB28, adressable par bus et
  *         adresse I2C (paametrable, pas fige sur une puce donnee).
  * @param  Aucun parametre C. Lit last_message, au format attendu
  *         "<bus>:<adresse_hal_hex>" - bus : 1 pour hi2c1, 2 pour hi2c2.
  * @retval Aucun (void). Remplit response avec
  *         "DAC_CONFIG:<bus>:<adresse>:VREF=0x????:PD=0x????:GAIN=0x????\n"
  *         ou "DAC_CONFIG:ERREUR_PARAMETRES\n" / "...:ERREUR_I2C\n".
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_DAC_CONFIG:1:0xC4"
  * -> lit VREF/PD/GAIN du DAC a 0xC4 (0x62<<1) sur hi2c1.
  * @endcode
  * @ingroup grp_dac
  */
static void h_GET_DAC_CONFIG(void)
{
    int bus = 0;
    unsigned int adresse_hal = 0;

    int n = sscanf(last_message, "%d:0x%x", &bus, &adresse_hal);
    if (n != 2)
    {
        sprintf(response, "DAC_CONFIG:ERREUR_PARAMETRES\n");
        return;
    }

    I2C_HandleTypeDef *hi2c = (bus == 2) ? &hi2c2 : &hi2c1;
    uint16_t vref = 0, pd = 0, gain = 0;

    HAL_StatusTypeDef st_v = LireRegistreConfigDAC(hi2c, (uint8_t)adresse_hal, 0x08, &vref);
    HAL_StatusTypeDef st_p = LireRegistreConfigDAC(hi2c, (uint8_t)adresse_hal, 0x09, &pd);
    HAL_StatusTypeDef st_g = LireRegistreConfigDAC(hi2c, (uint8_t)adresse_hal, 0x0A, &gain);

    if (st_v != HAL_OK || st_p != HAL_OK || st_g != HAL_OK)
    {
        sprintf(response, "DAC_CONFIG:%d:0x%02X:ERREUR_I2C\n", bus, adresse_hal);
        return;
    }

    sprintf(response, "DAC_CONFIG:%d:0x%02X:VREF=0x%04X:PD=0x%04X:GAIN=0x%04X\n",
            bus, adresse_hal, vref, pd, gain);
}

/**
  * @brief  Lit un registre VOLATILE de VALEUR du DAC MCP47FEB28 (adresses
  *         memoire 00h a 07h, une par voie). Lecture seule : aucune
  *         ecriture n'est possible par cette fonction.
  * @param  hi2c         Pointeur vers le handle I2C a utiliser (&hi2c1, &hi2c2).
  * @param  adresse_hal  Adresse I2C du DAC, deja au format HAL (decalee).
  * @param  registre     0x00 a 0x07 (voie 0 a 7, valeur volatile), 0x0B
  *                      (WiperLock / STATUS) ou 0x10 a 0x17 (valeur de
  *                      demarrage EEPROM de la voie 0 a 7). Lecture seule.
  *                      Toute autre valeur est refusee.
  * @param  valeur       Pointeur vers le uint16_t a remplir avec le mot lu.
  * @retval HAL_StatusTypeDef  HAL_OK si le DAC a repondu, HAL_ERROR si le
  *         registre est hors plage ou si le DAC a NACK/timeout.
  * @par Exemple d'utilisation
  * @code
  * uint16_t v;
  * if (LireRegistreValeurDAC(&hi2c1, MCP4725_I2CADDR_DEFAULT, 0x04, &v) == HAL_OK)
  * {
  *     // v = code reellement en place sur la voie 4 (tension 2PSp)
  * }
  * @endcode
  * @ingroup grp_dac
  */
static HAL_StatusTypeDef LireRegistreValeurDAC(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal,
                                                uint8_t registre, uint16_t *valeur)
{
    if (((registre > 0x07U) && (registre != 0x0BU) && ((registre < 0x10U) || (registre > 0x17U))) || valeur == NULL)
    {
        return HAL_ERROR;
    }
    uint8_t commande = (uint8_t)(((registre & 0x1FU) << 3) | 0x06U);   // C1:C0 = 11 (Read)
    uint8_t donnees[2] = {0, 0};

    osMutexId m = ResoudreMutexI2C(hi2c);
    osMutexWait(m, osWaitForever);
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(hi2c, adresse_hal, commande,
                                             I2C_MEMADD_SIZE_8BIT, donnees, 2, 100);
    osMutexRelease(m);

    if (st == HAL_OK)
    {
        *valeur = (uint16_t)((donnees[0] << 8) | donnees[1]);
    }
    return st;
}

/* ===========================================================================
 *  MEMOIRE EEPROM DES CODES DAC (revision du 21/10/2026)
 *  Pour les LDO declarees MEM_EEPROM dans ldoConfig[] (aujourd'hui 2PSp) :
 *  - au demarrage, les codes tension et seuil sont relus dans les registres
 *    non volatils du MCP47FEB28 (10h + voie) : DAC_RestaurerMemoire() ;
 *  - chaque nouvelle valeur est ecrite dans ces registres par la boucle
 *    (DAC_TraiterSauvegardes()), jamais en interruption ni dans lwIP.
 *  Ecriture uniquement si la valeur en EEPROM est differente (usure), une
 *  seule voie a la fois (pas d'ecriture continue en non volatil, DS20006368A),
 *  attente DAC_EEPROM_TWC_MS puis relecture de controle.
 * ======================================================================== */

/**
 * @brief  Puce DAC (ligne de tableDac[]) correspondant a une cible logique.
 * @param  cible  CIBLE_DAC1 ... CIBLE_DAC5.
 * @retval Pointeur vers la ligne de tableDac[], ou NULL (CIBLE_AUCUNE/inconnue).
 * @par Exemple d'utilisation
 * @code
 * const DacPuce_t *p = DAC_PuceDeCible(CIBLE_DAC1);   // I2C1, 0xC0
 * @endcode
 * @ingroup grp_eeprom
 */
static const DacPuce_t *DAC_PuceDeCible(CibleDac_t cible)
{
    for (uint32_t k = 0; k < NB_DAC_PUCES; k++)
    {
        if ((cible != CIBLE_AUCUNE) && (tableDac[k].cible == cible))
        {
            return &tableDac[k];
        }
    }
    return NULL;
}

/**
 * @brief  Indique si une puce DAC est joignable maintenant : bus I2C1, ou I2C2
 *         active (i2c2_actif), et pas en cours de balayage (BALAYER_DAC).
 * @param  puce  Ligne de tableDac[] (peut etre NULL).
 * @retval 1 si la puce peut etre lue/ecrite, 0 sinon.
 * @par Exemple d'utilisation
 * @code
 * if (DAC_PuceJoignable(DAC_PuceDeCible(CIBLE_DAC1)) != 0U) { ... }
 * @endcode
 * @ingroup grp_eeprom
 */
static uint8_t DAC_PuceJoignable(const DacPuce_t *puce)
{
    if (puce == NULL)
    {
        return 0U;
    }
    if ((puce->hi2c == &hi2c2) && (i2c2_actif == 0U))
    {
        return 0U;
    }
    return BALAYAGE_SUR(puce->hi2c, puce->adresse_hal) ? 0U : 1U;
}

/**
 * @brief  Ecrit le code de demarrage (EEPROM, registre non volatil 10h + voie)
 *         d'une voie du MCP47FEB28. Commande "Write" simple (3 octets), seul
 *         mode autorise vers la memoire non volatile. La sortie analogique ne
 *         change pas (seul le registre volatil 00h-07h la pilote).
 * @param  hi2c         &hi2c1 ou &hi2c2.
 * @param  adresse_hal  Adresse 7 bits decalee (0xC0, 0xC2, 0xC4).
 * @param  voie         Voie 0-7 ; toute autre valeur est refusee.
 * @param  valeur       Code 12 bits (0-4095).
 * @retval HAL_StatusTypeDef de la transmission (HAL_ERROR si voie > 7).
 * @par Exemple d'utilisation
 * @code
 * EcrireDACEeprom(&hi2c1, 0xC0, 5, 1056);   // seuil 2PSp garde a la coupure
 * @endcode
 * @ingroup grp_eeprom
 */
static HAL_StatusTypeDef EcrireDACEeprom(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal,
                                          uint8_t voie, uint16_t valeur)
{
    if (voie > 7U)
    {
        return HAL_ERROR;
    }
    uint8_t trame[3];
    trame[0] = (uint8_t)(((0x10U + voie) & 0x1FU) << 3);   /* C1:C0 = 00 (Write) */
    trame[1] = (uint8_t)((valeur >> 8) & 0x0FU);
    trame[2] = (uint8_t)(valeur & 0xFFU);

    osMutexId m = ResoudreMutexI2C(hi2c);
    osMutexWait(m, osWaitForever);
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(hi2c, adresse_hal, trame, 3, 100);
    osMutexRelease(m);
    return st;
}

/**
 * @brief  Garde un code dans l'EEPROM d'une voie : relit la valeur en EEPROM,
 *         n'ecrit que si elle differe, attend DAC_EEPROM_TWC_MS et relit pour
 *         controler. A n'appeler que depuis la boucle de StartLwipTask().
 * @param  cible   Puce (CIBLE_DAC1 ... CIBLE_DAC5).
 * @param  voie    Voie 0-7.
 * @param  valeur  Code 12 bits a garder.
 * @param  lu      Recoit le code relu en EEPROM apres l'operation (peut etre NULL).
 * @retval 0 = code en EEPROM conforme, 1 = puce non joignable (ignore),
 *         2 = erreur I2C ou relecture differente.
 * @par Exemple d'utilisation
 * @code
 * uint16_t lu;
 * if (DAC_GarderEnEeprom(CIBLE_DAC1, 4, VAL_LDO[2], &lu) == 2U) { ... }
 * @endcode
 * @ingroup grp_eeprom
 */
static uint8_t DAC_GarderEnEeprom(CibleDac_t cible, uint8_t voie, uint16_t valeur, uint16_t *lu)
{
    const DacPuce_t *puce = DAC_PuceDeCible(cible);
    uint16_t v = 0U;

    valeur &= 0x0FFFU;
    if ((DAC_PuceJoignable(puce) == 0U) || (voie > 7U))
    {
        return 1U;
    }
    if ((LireRegistreValeurDAC(puce->hi2c, puce->adresse_hal, (uint8_t)(0x10U + voie), &v) == HAL_OK) &&
        ((v & 0x0FFFU) == valeur))
    {
        if (lu != NULL) { *lu = (uint16_t)(v & 0x0FFFU); }
        return 0U;   /* deja en EEPROM : pas d'ecriture inutile */
    }
    if (EcrireDACEeprom(puce->hi2c, puce->adresse_hal, voie, valeur) != HAL_OK)
    {
        return 2U;
    }
    osDelay(DAC_EEPROM_TWC_MS);
    if (LireRegistreValeurDAC(puce->hi2c, puce->adresse_hal, (uint8_t)(0x10U + voie), &v) != HAL_OK)
    {
        return 2U;
    }
    if (lu != NULL) { *lu = (uint16_t)(v & 0x0FFFU); }
    return ((v & 0x0FFFU) == valeur) ? 0U : 2U;
}

/**
 * @brief  Traite AU PLUS UNE demande de sauvegarde EEPROM par appel (une LDO :
 *         voie tension puis voie seuil). Une demande "differee" (SET_VAL,
 *         SET_CURRENT) n'est traitee qu'apres DAC_EEPROM_DELAI_MS sans nouvelle
 *         valeur ; une demande "immediate" (declenchement OC) au tour suivant.
 *         En cas d'echec, nouvel essai, jusqu'a DAC_EEPROM_ESSAIS_MAX.
 * @retval Aucun. Ligne USART1 a chaque ecriture :
 *         "DAC_NV:2PSp:V=860:I=1056:OK" (ou ":ERREUR_n").
 * @par Exemple d'utilisation
 * @code
 * DAC_TraiterSauvegardes();   // dans la boucle for(;;) de StartLwipTask()
 * @endcode
 * @ingroup grp_eeprom
 */
static void DAC_TraiterSauvegardes(void)
{
    for (int i = 0; i < NB_LDO; i++)
    {
        if (nv_a_sauver[i] == 0U)
        {
            continue;
        }
        if ((nv_immediat[i] == 0U) && ((HAL_GetTick() - nv_tick_demande[i]) < DAC_EEPROM_DELAI_MS))
        {
            continue;
        }
        nv_a_sauver[i] = 0U;   /* une nouvelle demande pendant l'ecriture la reposera */
        nv_immediat[i] = 0U;

        const LDO_Config_t *cfg = &ldoConfig[i];
        const MappingDacLdo_t *m = &mappingDacLdo[i];
        uint16_t lu_v = 0U, lu_i = 0U;
        uint8_t err_v = 1U, err_i = 1U;

        if (m->dac_tension != CIBLE_AUCUNE)
        {
            err_v = DAC_GarderEnEeprom(m->dac_tension, m->canal_tension, VAL_LDO[i], &lu_v);
        }
        if ((OC_EstSurveillee(i) != 0U) && (cfg->seuil_par_pot == 0U))
        {
            err_i = DAC_GarderEnEeprom(cfg->dac_courant, cfg->canal_courant, CURRENT_LDO[i], &lu_i);
        }

        char ligne[80];
        if ((err_v == 2U) || (err_i == 2U))
        {
            nv_essais[i]++;
            if (nv_essais[i] < DAC_EEPROM_ESSAIS_MAX)
            {
                DAC_DemanderSauvegarde(i, 1U);
            }
            snprintf(ligne, sizeof(ligne), "DAC_NV:%s:V=%u:I=%u:ERREUR_%u\r\n",
                     cfg->name, (unsigned)lu_v, (unsigned)lu_i, (unsigned)nv_essais[i]);
        }
        else
        {
            nv_essais[i] = 0U;
            snprintf(ligne, sizeof(ligne), "DAC_NV:%s:V=%u:I=%u:OK\r\n",
                     cfg->name, (unsigned)lu_v, (unsigned)lu_i);
        }
        UART1_EnvoyerTexte(ligne);
        return;   /* une seule LDO par tour : la boucle n'est retenue que ~2 x tWC */
    }
}

/**
 * @brief  Au demarrage, recharge depuis l'EEPROM du MCP47FEB28 les codes de
 *         tension et de seuil des LDO declarees MEM_EEPROM, dans VAL_LDO[],
 *         CURRENT_LDO[] et les trames DACx_DEF (la boucle les reemet ensuite).
 *         Un seuil lu hors de [seuil_min, seuil_max] est remplace par le seuil
 *         par defaut, qui est alors ecrit en EEPROM. L'EN reste a 0.
 *         A appeler apres CreerMutexI2C(), OC_InitialiserSeuilsDefaut() et
 *         InitialiserConfigDacsI2C1(), avant udpServer_init().
 * @retval Aucun. Une ligne USART1 par LDO : "DAC_NV_INIT:2PSp:V=860:I=1056:OK".
 * @par Exemple d'utilisation
 * @code
 * InitialiserConfigDacsI2C1();
 * DAC_RestaurerMemoire();
 * @endcode
 * @ingroup grp_eeprom
 */
static void DAC_RestaurerMemoire(void)
{
    for (int i = 0; i < NB_LDO; i++)
    {
        const LDO_Config_t *cfg = &ldoConfig[i];
        const MappingDacLdo_t *m = &mappingDacLdo[i];
        const char *etat = "OK";
        uint16_t v = 0U;

        if (cfg->memoire_dac != MEM_EEPROM)
        {
            continue;
        }

        /* Tension */
        const DacPuce_t *pv = DAC_PuceDeCible(m->dac_tension);
        if (DAC_PuceJoignable(pv) != 0U)
        {
            if (LireRegistreValeurDAC(pv->hi2c, pv->adresse_hal, (uint8_t)(0x10U + m->canal_tension), &v) == HAL_OK)
            {
                v &= 0x0FFFU;
                VAL_LDO[i] = v;
                cfg->ldo->VAL = v;
                EcrireValeurDac(m->dac_tension, m->canal_tension, v);
            }
            else
            {
                etat = "ERREUR_LECTURE_V";
            }
        }

        /* Seuil de courant (DAC uniquement) */
        if ((OC_EstSurveillee(i) != 0U) && (cfg->seuil_par_pot == 0U))
        {
            const DacPuce_t *pc = DAC_PuceDeCible(cfg->dac_courant);
            uint16_t c = 0U;
            if ((DAC_PuceJoignable(pc) != 0U) &&
                (LireRegistreValeurDAC(pc->hi2c, pc->adresse_hal, (uint8_t)(0x10U + cfg->canal_courant), &c) == HAL_OK))
            {
                c &= 0x0FFFU;
                if ((c >= cfg->oc.seuil_min) && (c <= cfg->oc.seuil_max))
                {
                    CURRENT_LDO[i] = c;
                    cfg->ldo->CURRENT = c;
                    EcrireValeurDac(cfg->dac_courant, cfg->canal_courant, c);
                }
                else
                {
                    etat = "SEUIL_EEPROM_INVALIDE_DEFAUT";   /* seuil par defaut garde et ecrit */
                    DAC_DemanderSauvegarde(i, 1U);
                }
            }
            else
            {
                etat = "ERREUR_LECTURE_I";
            }
        }

        char ligne[96];
        snprintf(ligne, sizeof(ligne), "DAC_NV_INIT:%s:V=%u:I=%u:%s\r\n",
                 cfg->name, (unsigned)VAL_LDO[i], (unsigned)CURRENT_LDO[i], etat);
        UART1_EnvoyerTexte(ligne);
    }
}

/**
 * @brief  Mode potentiometre (LDO dont la colonne "seuil" vaut SEUIL_POT) : met
 *         la voie du DAC de courant en Power-Down 11 (sortie ouverte) pour
 *         qu'elle ne pilote plus IN- du LM211, et memorise cette configuration
 *         pour que SurveillerConfigDacsI2C1() la conserve. Sans effet si aucune
 *         LDO n'est en SEUIL_POT (cas actuel). Puces de I2C1 uniquement.
 *         Codage PDnB:PDnA = 11 a confirmer (DS20006368A, registre 4-3).
 * @retval Aucun. Ligne USART1 : "DAC_PD_POT:2PSp:0xC0:PD=0x0C00:OK".
 * @par Exemple d'utilisation
 * @code
 * InitialiserConfigDacsI2C1();
 * DAC_ConfigurerSeuilsPotentiometre();
 * @endcode
 * @ingroup grp_eeprom
 */
static void DAC_ConfigurerSeuilsPotentiometre(void)
{
    for (int i = 0; i < NB_LDO; i++)
    {
        const LDO_Config_t *cfg = &ldoConfig[i];
        if ((OC_EstSurveillee(i) == 0U) || (cfg->seuil_par_pot == 0U) || (cfg->canal_courant > 7U))
        {
            continue;
        }
        const DacPuce_t *puce = DAC_PuceDeCible(cfg->dac_courant);
        const int k = (puce != NULL) ? IndexDacI2C1(puce->hi2c, puce->adresse_hal) : -1;
        if (k < 0)
        {
            continue;
        }
        const uint16_t pd = (uint16_t)(dac_cfg_attendue[k][1] | (uint16_t)(0x3U << (2U * cfg->canal_courant)));
        const HAL_StatusTypeDef st = EcrireRegistreConfigDAC(puce->hi2c, puce->adresse_hal, 0x09, pd);
        if (st == HAL_OK)
        {
            MemoriserConfigDac(puce->hi2c, puce->adresse_hal, 0x09, pd);
        }
        char ligne[80];
        snprintf(ligne, sizeof(ligne), "DAC_PD_POT:%s:0x%02X:PD=0x%04X:%s\r\n",
                 cfg->name, puce->adresse_hal, pd, (st == HAL_OK) ? "OK" : "ERREUR_I2C");
        UART1_EnvoyerTexte(ligne);
    }
}

/**
  * @brief  Handler de la commande reseau "GET_DAC_REG" - diagnostic : relit
  *         dans le DAC un registre de valeur (00h-07h) ou de configuration
  *         (08h VREF, 09h Power-Down, 0Ah Gain) et le compare a la valeur que
  *         le firmware envoie en continu (tableau DACx_DEF).
  * @param  Aucun parametre C. Lit last_message, au format attendu
  *         "<bus>:<adresse_hal_hex>:<registre>" en decimal : 0-7 valeur
  *         volatile de la voie, 8-10 VREF/PD/GAIN, 11 WiperLock (0Bh),
  *         16-23 valeur de demarrage EEPROM de la voie 0-7 (10h-17h).
  * @retval Aucun (void). Remplit response avec
  *         "DAC_REG:<bus>:<adresse>:REG=<r>:DAC=0x????:FIRMWARE=0x????\n"
  *         (FIRMWARE absent pour 08h-0Ah), ou "...:ERREUR_I2C\n" /
  *         "DAC_REG:ERREUR_PARAMETRES\n".
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_DAC_REG:1:0xC0:4"
  * -> "DAC_REG:1:0xC0:REG=4:DAC=0x0BB8:FIRMWARE=0x0BB8" apres SET_VAL_LDO_2PSp:3000.
  * @endcode
  * @ingroup grp_dac
  */
static void h_GET_DAC_REG(void)
{
    int bus = 0, registre = -1;
    unsigned int adresse_hal = 0;

    if ((last_message == NULL) ||
        (sscanf(last_message, "%d:0x%x:%d", &bus, &adresse_hal, &registre) != 3) ||
        (registre < 0) || (registre > 23) || ((registre > 11) && (registre < 16)))
    {
        sprintf(response, "DAC_REG:ERREUR_PARAMETRES\n");
        return;
    }

    I2C_HandleTypeDef *hi2c = (bus == 2) ? &hi2c2 : &hi2c1;
    uint16_t valeur = 0;
    HAL_StatusTypeDef st = ((registre <= 7) || (registre >= 11))
        ? LireRegistreValeurDAC(hi2c, (uint8_t)adresse_hal, (uint8_t)registre, &valeur)
        : LireRegistreConfigDAC(hi2c, (uint8_t)adresse_hal, (uint8_t)registre, &valeur);

    if (st != HAL_OK)
    {
        sprintf(response, "DAC_REG:%d:0x%02X:REG=%d:ERREUR_I2C\n", bus, adresse_hal, registre);
        return;
    }

    const uint8_t *tableau = ResoudreTableauDacParAdresse(hi2c, (uint8_t)adresse_hal);
    if ((registre <= 7) && (tableau != NULL))
    {
        uint16_t attendu = (uint16_t)(((tableau[registre * 3 + 1] & 0x0FU) << 8) | tableau[registre * 3 + 2]);
        sprintf(response, "DAC_REG:%d:0x%02X:REG=%d:DAC=0x%04X:FIRMWARE=0x%04X\n",
                bus, adresse_hal, registre, valeur, attendu);
    }
    else
    {
        sprintf(response, "DAC_REG:%d:0x%02X:REG=%d:DAC=0x%04X\n", bus, adresse_hal, registre, valeur);
    }
}

/**
  * @brief  Handler de la commande reseau "SET_DAC_VREF" - ecrit le
  *         registre VREF (0x08) d'une puce DAC : source de reference
  *         des 8 canaux EN UNE SEULE FOIS (2 bits/canal, voir Registre
  *         4-2 du datasheet - 00=VDD, 01=bande interne 1.22V, 10=VREF
  *         pin non bufferisee, 11=VREF pin bufferisee).
  * @param  Aucun parametre C. Lit last_message, au format attendu
  *         "<bus>:<adresse_hal_hex>:<valeur_hex16>". Ex : 0x0000 pour
  *         forcer VDD sur les 8 canaux.
  * @retval Aucun (void). Remplit response avec
  *         "SET_DAC_VREF:<bus>:<adresse>:0x????:OK\n" ou
  *         "...:ERREUR_I2C\n" / "SET_DAC_VREF:ERREUR_PARAMETRES\n".
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DAC_VREF:1:0xC4:0x0000"
  * -> force VREF=VDD sur les 8 canaux du DAC a 0xC4 sur hi2c1.
  * @endcode
  * @ingroup grp_dac
  */
static void h_SET_DAC_VREF(void)
{
    int bus = 0;
    unsigned int adresse_hal = 0, valeur = 0;

    int n = sscanf(last_message, "%d:0x%x:0x%x", &bus, &adresse_hal, &valeur);
    if (n != 3)
    {
        sprintf(response, "SET_DAC_VREF:ERREUR_PARAMETRES\n");
        return;
    }

    I2C_HandleTypeDef *hi2c = (bus == 2) ? &hi2c2 : &hi2c1;
    HAL_StatusTypeDef st = EcrireRegistreConfigDAC(hi2c, (uint8_t)adresse_hal, 0x08, (uint16_t)valeur);
    if (st == HAL_OK) { MemoriserConfigDac(hi2c, (uint8_t)adresse_hal, 0x08, (uint16_t)valeur); }

    sprintf(response, "SET_DAC_VREF:%d:0x%02X:0x%04X:%s\n",
            bus, adresse_hal, valeur, (st == HAL_OK) ? "OK" : "ERREUR_I2C");
}

/**
  * @brief  Handler de la commande reseau "SET_DAC_PD" - ecrit le
  *         registre Power-Down (0x09) d'une puce DAC : etat des 8
  *         canaux EN UNE SEULE FOIS (2 bits/canal, voir Registre 4-3 -
  *         00=normal, 01/10/11=power-down avec differentes resistances
  *         de tirage). 0x0000 = fonctionnement normal sur les 8 canaux.
  * @param  Aucun parametre C. Lit last_message, au format attendu
  *         "<bus>:<adresse_hal_hex>:<valeur_hex16>".
  * @retval Aucun (void). Remplit response avec
  *         "SET_DAC_PD:<bus>:<adresse>:0x????:OK\n" ou
  *         "...:ERREUR_I2C\n" / "SET_DAC_PD:ERREUR_PARAMETRES\n".
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DAC_PD:1:0xC4:0x0000"
  * -> force fonctionnement normal sur les 8 canaux (pas de power-down).
  * @endcode
  * @ingroup grp_dac
  */
static void h_SET_DAC_PD(void)
{
    int bus = 0;
    unsigned int adresse_hal = 0, valeur = 0;

    int n = sscanf(last_message, "%d:0x%x:0x%x", &bus, &adresse_hal, &valeur);
    if (n != 3)
    {
        sprintf(response, "SET_DAC_PD:ERREUR_PARAMETRES\n");
        return;
    }

    I2C_HandleTypeDef *hi2c = (bus == 2) ? &hi2c2 : &hi2c1;
    HAL_StatusTypeDef st = EcrireRegistreConfigDAC(hi2c, (uint8_t)adresse_hal, 0x09, (uint16_t)valeur);
    if (st == HAL_OK) { MemoriserConfigDac(hi2c, (uint8_t)adresse_hal, 0x09, (uint16_t)valeur); }

    sprintf(response, "SET_DAC_PD:%d:0x%02X:0x%04X:%s\n",
            bus, adresse_hal, valeur, (st == HAL_OK) ? "OK" : "ERREUR_I2C");
}

/**
  * @brief  Handler de la commande reseau "SET_DAC_GAIN" - ecrit le
  *         registre Gain (0x0A) d'une puce DAC : gain des 8 canaux EN
  *         UNE SEULE FOIS (1 bit/canal, G7..G0 dans l'octet haut - voir
  *         Registre 4-4 - 0=1x, 1=2x). L'octet bas (bits status POR/
  *         EEWA) est toujours envoye a 0x00, sans effet indesirable :
  *         POR est "clearable" (0 le laisse tel quel a la prochaine
  *         lecture), EEWA est en lecture seule.
  * @param  Aucun parametre C. Lit last_message, au format attendu
  *         "<bus>:<adresse_hal_hex>:<valeur_hex8>" - valeur_hex8 = G7..G0
  *         sur 8 bits (ex : 0x00 = gain 1x sur les 8 canaux).
  * @retval Aucun (void). Remplit response avec
  *         "SET_DAC_GAIN:<bus>:<adresse>:0x??:OK\n" ou
  *         "...:ERREUR_I2C\n" / "SET_DAC_GAIN:ERREUR_PARAMETRES\n".
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_DAC_GAIN:1:0xC4:0x00"
  * -> force gain 1x sur les 8 canaux du DAC a 0xC4 sur hi2c1.
  * @endcode
  * @ingroup grp_dac
  */
static void h_SET_DAC_GAIN(void)
{
    int bus = 0;
    unsigned int adresse_hal = 0, valeur = 0;

    int n = sscanf(last_message, "%d:0x%x:0x%x", &bus, &adresse_hal, &valeur);
    if (n != 3)
    {
        sprintf(response, "SET_DAC_GAIN:ERREUR_PARAMETRES\n");
        return;
    }

    I2C_HandleTypeDef *hi2c = (bus == 2) ? &hi2c2 : &hi2c1;
    uint16_t mot = (uint16_t)((valeur & 0xFF) << 8);
    HAL_StatusTypeDef st = EcrireRegistreConfigDAC(hi2c, (uint8_t)adresse_hal, 0x0A, mot);
    if (st == HAL_OK) { MemoriserConfigDac(hi2c, (uint8_t)adresse_hal, 0x0A, mot); }

    sprintf(response, "SET_DAC_GAIN:%d:0x%02X:0x%02X:%s\n",
            bus, adresse_hal, (unsigned int)(valeur & 0xFF), (st == HAL_OK) ? "OK" : "ERREUR_I2C");
}

/**
  * @brief  Handler de la commande reseau "SET_I2C2_ACTIF" - active ou
  *         desactive a chaud l'emission periodique sur I2C2 (boucle
  *         principale, DAC2/DAC4/DAC5). A desactiver tant que le
  *         probleme materiel sur ce bus n'est pas resolu : sinon,
  *         chaque tentative ratee bloque le thread jusqu'a
  *         SPI_TIMEOUT (100 ms), x3 par tour de boucle, ce qui retarde
  *         aussi les mises a jour I2C1 (meme thread, sequentiel).
  * @param  Aucun parametre C. Lit last_message : "0" ou "1".
  * @retval Aucun (void). Remplit response avec
  *         "SET_I2C2_ACTIF:0\n" / "SET_I2C2_ACTIF:1\n" ou
  *         "SET_I2C2_ACTIF:ERREUR_PARAMETRES\n".
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_I2C2_ACTIF:0" -> coupe I2C2.
  * @endcode
  * @ingroup grp_dac
  */
static void h_SET_I2C2_ACTIF(void)
{
    if (strcmp(last_message, "0") == 0)
    {
        i2c2_actif = 0;
    }
    else if (strcmp(last_message, "1") == 0)
    {
        i2c2_actif = 1;
    }
    else
    {
        sprintf(response, "SET_I2C2_ACTIF:ERREUR_PARAMETRES\n");
        return;
    }
    sprintf(response, "SET_I2C2_ACTIF:%d\n", i2c2_actif);
}

// ---------------------------------------------------------------------------
// Mesure ADC multiplexee des sorties LDO reelles (0-6V), via 3x ADG728
// (multiplexeurs analogiques NATIVEMENT I2C, 8 canaux chacun -> 24 canaux
// disponibles pour les 18 sorties), sur le MEME bus I2C1 que le DAC.
// Une SEULE entree ADC en sortie du mux. Voir schema_mesure_adc_mux.html.
//
// IMPORTANT : l'ADG728 supporte au maximum 5.5V d'alimentation - le pont
// diviseur (0-6V -> 0-3V, R1=R2=10k) doit donc etre place AVANT chaque
// entree du mux (un pont par LDO, 18 au total), pas en sortie commune.
// Le signal arrivant au mux est deja a l'echelle ADC : aucun pont
// supplementaire n'est necessaire en sortie (broche D commune -> ADC direct).
//
// SEPARATION TENSION / COURANT (deux chaines totalement independantes,
// chacune avec son propre bus I2C, ses propres puces ADG728, et sa
// propre broche ADC - plus de pool de canaux partage) :
//   - TENSION  : 3x ADG728 sur I2C1 (0x4C/0x4D/0x4E, 24 canaux, 18
//                utilises), broche commune D -> PC2.
//   - COURANT  : 2x ADG728 sur I2C2 (0x4C/0x4D, memes valeurs d'adresse
//                que la chaine tension car bus PHYSIQUEMENT different,
//                16 canaux, 12 utilises pour les LDO *PSp, *PSn), broche
//                commune D -> PC3.
//
// ATTENTION - A VERIFIER AVANT DE FLASHER :
//   1) PC2/PC3 ont un double bonding possible sur STM32H7 (ADC1/ADC2
//      canal 12/13 OU ADC3 canal 0/1 selon le pont de soudure de la
//      carte). Ce code suppose ADC1 canal 12 (PC2) et canal 13 (PC3) -
//      a confirmer dans CubeMX / le schema de la Nucleo utilisee.
//   2) I2C2 (PB10=SCL, PB11=SDA) montrait un bus verrouille (busy=127
//      au dernier scan) : a resoudre avant de cabler les puces courant
//      dessus, sinon GET_MESURE_COURANT_REELLE_<nom> echouera toujours.
// ---------------------------------------------------------------------------

// Adresses I2C des 3 ADG728 (7 bits, deja decalees au format HAL), fixees
// par le cablage des broches A0/A1 de chaque puce : adresse = 0x4C + 2*A1 + A0.
// A0=0,A1=0 -> 0x4C ; A0=1,A1=0 -> 0x4D ; A0=0,A1=1 -> 0x4E (celle-ci non
// utilisee, reste disponible en reserve si besoin d'une 4e puce plus tard).
#define ADG728_ADDR_0  (0x4C << 1)
#define ADG728_ADDR_1  (0x4D << 1)
#define ADG728_ADDR_2  (0x4E << 1)

static const uint8_t adg728_adresses_tension[3] = { ADG728_ADDR_0, ADG728_ADDR_1, ADG728_ADDR_2 };
static const uint8_t adg728_adresses_courant[2] = { ADG728_ADDR_0, ADG728_ADDR_1 };

// Canaux ADC dedies, confirmes via le fichier .ioc du projet reel :
//   - TENSION : PC0 -> hadc1, ADC_CHANNEL_10. ATTENTION : ce canal est
//     DEJA UTILISE par GET_CURRENT_1 (voir h_GET_CURRENT_1()) - collision
//     assumee et geree par ailleurs (cote utilisateur), pas par ce code.
//   - COURANT : PF4 -> hadc3, ADC_CHANNEL_9. Broche confirmee libre.
#define ADC_CANAL_TENSION  ADC_CHANNEL_10  // PC0 - PARTAGE avec GET_CURRENT_1, a arbitrer
#define ADC_CANAL_COURANT  ADC_CHANNEL_9   // PF4 - confirmee libre dans le .ioc

// Chaque pont diviseur (0-6V -> 0-3V) est deja dimensionne pour amener
// le signal a l'echelle ADC : aucune division supplementaire ici.
#define MUX_RATIO_PONT  1.0f

// canal_mux_tension_actuel / canal_mux_courant_actuel sont deja
// declarees plus haut dans ce fichier (voir section "Bouton-poussoir
// de RESET materiel du multiplexeur"), car utilisees aussi par
// TraiterResetMultiplexeur().

/**
  * @brief  Ecrit le masque de canaux actifs sur un ADG728 (un octet, un
  *         bit par canal - voir datasheet ADG728, "chaque canal pilote
  *         par un bit d'un mot de 8 bits").
  * @param  hi2c            Bus I2C a utiliser (&hi2c1 pour la chaine
  *                         tension, &hi2c2 pour la chaine courant).
  * @param  adresse_hal     Adresse I2C de la puce ciblee, format HAL
  *                         (ex : ADG728_ADDR_0).
  * @param  masque_canaux   Masque 8 bits, un bit a 1 = canal correspondant
  *                         actif. 0x00 desactive tous les canaux de cette
  *                         puce (etat par defaut a la mise sous tension).
  * @retval HAL_StatusTypeDef  Resultat de la transmission I2C.
  * @par Exemple d'utilisation
  * @code
  * EcrireADG728(&hi2c1, ADG728_ADDR_0, 0x04);  // canal 2 actif
  * EcrireADG728(&hi2c2, ADG728_ADDR_0, 0x00);  // tous desactives
  * @endcode
  * @ingroup grp_adc
  */
static HAL_StatusTypeDef EcrireADG728(I2C_HandleTypeDef *hi2c, uint8_t adresse_hal, uint8_t masque_canaux)
{
    osMutexId m = ResoudreMutexI2C(hi2c);
    osMutexWait(m, osWaitForever);
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(hi2c, adresse_hal, &masque_canaux, 1, 100);
    osMutexRelease(m);
    return st;
}

/**
  * @brief  Selectionne un canal (0-23) parmi les 3 ADG728 de la chaine
  *         TENSION (bus I2C1). Desactive systematiquement les 2 autres
  *         puces (masque 0x00) avant d'activer le canal voulu, pour que
  *         la broche D commune (vers PC2) ne porte jamais deux sorties
  *         a la fois.
  * @param  canal  Canal a selectionner, 0-23 (0-7 = puce 0, 8-15 = puce 1,
  *                16-23 = puce 2). Ignore silencieusement si hors plage.
  * @retval Aucun (void). Met a jour canal_mux_tension_actuel et attend
  *         un court delai d'etablissement.
  * @par Exemple d'utilisation
  * @code
  * SelectionnerCanalMuxTension(2);    // canal 2 de la puce 0 (0x4C)
  * SelectionnerCanalMuxTension(10);   // canal 2 de la puce 1 (0x4D)
  * @endcode
  * @ingroup grp_adc
  */
static void SelectionnerCanalMuxTension(uint8_t canal)
{
    if (canal > 23)
    {
        return;
    }

    uint8_t puce = canal / 8;
    uint8_t sous_canal = canal % 8;
    uint8_t masque_actif = (uint8_t)(1 << sous_canal);

    for (uint8_t i = 0; i < 3; i++)
    {
        uint8_t masque = (i == puce) ? masque_actif : 0x00;
        EcrireADG728(&hi2c1, adg728_adresses_tension[i], masque);
    }

    canal_mux_tension_actuel = canal;

    HAL_Delay(1);
}

/**
  * @brief  Selectionne un canal (0-15) parmi les 2 ADG728 de la chaine
  *         COURANT (bus I2C2). Meme principe que
  *         SelectionnerCanalMuxTension(), sur l'autre bus/puces/broche
  *         ADC (PC3).
  * @param  canal  Canal a selectionner, 0-15 (0-7 = puce 0, 8-15 = puce 1).
  *                Ignore silencieusement si hors plage.
  * @retval Aucun (void). Met a jour canal_mux_courant_actuel et attend
  *         un court delai d'etablissement.
  * @par Exemple d'utilisation
  * @code
  * SelectionnerCanalMuxCourant(9);  // canal 1 de la puce 1
  * @endcode
  * @ingroup grp_adc
  */
static void SelectionnerCanalMuxCourant(uint8_t canal)
{
    if (canal > 15)
    {
        return;
    }

    uint8_t puce = canal / 8;
    uint8_t sous_canal = canal % 8;
    uint8_t masque_actif = (uint8_t)(1 << sous_canal);

    for (uint8_t i = 0; i < 2; i++)
    {
        uint8_t masque = (i == puce) ? masque_actif : 0x00;
        EcrireADG728(&hi2c2, adg728_adresses_courant[i], masque);
    }

    canal_mux_courant_actuel = canal;

    HAL_Delay(1);
}

/**
  * @brief  Configuration du multiplexage pour la mesure reelle de
  *         TENSION d'une sortie LDO. Un element par LDO, indexe dans le
  *         MEME ORDRE que VAL_LDO[]/CURRENT_LDO[] (voir findLdoIndex()) :
  *         0=1PSp,1=1PSn,2=2PSp,3=2PSn,4=3PSp,5=3PSn,6=4PSp,7=4PSn,
  *         8=5PSp,9=5PSn,10=6PSp,11=6PSn,12=1PLp,13=1PLn,14=2PLp,
  *         15=2PLn,16=3PLp,17=3PLn.
  */
typedef struct {
    uint8_t canal_mux;  // 0-23 (3 x ADG728 sur I2C1, chaine tension), ou 0xFF si pas encore cable
} MuxLdoConfig_t;

// Configuration LOGICIELLE, modifiable a chaud sans recompilation, via
// les commandes reseau SET_MUX_LDO_<nom>:<canal> / GET_MUX_LDO_<nom>
// (voir SetMuxLdo()/GetMuxLdo() ci-dessous). Reinitialisee a 0xFF (non
// cablee) pour toutes les LDO a chaque reset/reflash du STM32 : cote PC,
// app.py est responsable de re-appliquer la configuration sauvegardee
// (assets/mux_ldo_config.json) apres chaque redemarrage du STM32.
// Valeurs initiales : colonnes "mux V" / "mux I" de ldoConfig[], recopiees
// par LDO_InitialiserTables() au demarrage (les 0xFF ci-dessous sont ecrases).
static MuxLdoConfig_t muxLdoConfig[NB_LDO];   /* MUX_NC recopie par LDO_InitialiserTables() */

/**
  * @brief  Handler de la commande reseau "SET_MUX_LDO_<nom>:<canal>" -
  *         associe une LDO a un canal du multiplexeur (0-31), a chaud,
  *         sans recompilation ni reflash. Voir le branchement dans
  *         tc_request(), meme motif que SET_VAL_LDO_<nom> : prefixe +
  *         findLdoIndex() pour resoudre idx directement.
  * @param  idx  Index de la LDO (0-17), deja resolu par findLdoIndex()
  *              dans tc_request().
  * @param  nom  Nom texte de la LDO (ex : "1PSp"), pour la reponse.
  * @retval Aucun (void). Lit last_message (deja positionne apres le
  *         premier ':' par tc_request()) au format "<canal>" (entier
  *         0-23). Remplit response avec "MUX_LDO:<nom>:CANAL=<n>\n" si
  *         valide, ou "MUX_LDO:<nom>:ERREUR_CANAL\n" sinon.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_MUX_LDO_1PSp:3"
  * Reponse type    : "MUX_LDO:1PSp:CANAL=3\n"
  * @endcode
  * @ingroup grp_adc
  */
static void SetMuxLdo(int idx, const char *nom)
{
    int canal = atoi(last_message);

    if (canal < 0 || canal > 23)
    {
        sprintf(response, "MUX_LDO:%s:ERREUR_CANAL\n", nom);
        return;
    }

    muxLdoConfig[idx].canal_mux = (uint8_t)canal;
    sprintf(response, "MUX_LDO:%s:CANAL=%d\n", nom, canal);
}

/**
  * @brief  Handler de la commande reseau "GET_MUX_LDO_<nom>" - lit le
  *         canal du multiplexeur actuellement associe a une LDO, sans
  *         le modifier.
  * @param  idx  Index de la LDO (0-17), deja resolu par findLdoIndex().
  * @param  nom  Nom texte de la LDO, pour la reponse.
  * @retval Aucun (void). Remplit response avec
  *         "MUX_LDO:<nom>:CANAL=<n>\n" si un canal est associe, ou
  *         "MUX_LDO:<nom>:NON_CABLE\n" sinon.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_MUX_LDO_1PSp"
  * Reponse type    : "MUX_LDO:1PSp:CANAL=3\n"
  * @endcode
  * @ingroup grp_adc
  */
static void GetMuxLdo(int idx, const char *nom)
{
    uint8_t canal = muxLdoConfig[idx].canal_mux;

    if (canal == 0xFF)
    {
        sprintf(response, "MUX_LDO:%s:NON_CABLE\n", nom);
    }
    else
    {
        sprintf(response, "MUX_LDO:%s:CANAL=%d\n", nom, canal);
    }
}

/**
  * @brief  Lit une conversion ADC sur le canal donne et la convertit en
  *         tension REELLE cote LDO (avant le pont diviseur), a partir du
  *         ratio R2/(R1+R2) du pont utilise.
  *         Resolution 16 bits (ADC_RESOLUTION_16B, deja configuree sur
  *         hadc1/hadc3) : plage brute 0-65535 pour 0-VDDA (3.3V typique).
  * @param  hadc          Handle ADC a utiliser (&hadc1 pour PC0/canal 10,
  *                       &hadc3 pour PF4/canal 9).
  * @param  canal         Canal ADC a lire (ex : ADC_CANAL_TENSION ou
  *                       ADC_CANAL_COURANT). RECONFIGURE le canal actif
  *                       de hadc a chaque appel (HAL_ADC_ConfigChannel) :
  *                       necessaire des qu'un meme ADC (ex : hadc3) doit
  *                       lire plusieurs canaux differents selon le
  *                       contexte (ex : canal 1 pour GET_CURRENT_2, canal
  *                       9 pour la mesure de courant LDO).
  * @param  ratio_pont    R2 / (R1 + R2) du pont diviseur utilise (0.5f
  *                       pour le pont partage R1=R2=10k de ce montage).
  * @retval float  Tension reelle en volts, cote sortie du LDO (deja
  *                divisee par ratio_pont pour "annuler" l'effet du pont).
  *                Retourne -1.0f en cas d'echec de conversion (timeout)
  *                ou de configuration du canal.
  * @par Exemple d'utilisation
  * @code
  * float v = LireTensionADC(&hadc1, ADC_CANAL_TENSION, MUX_RATIO_PONT);
  * @endcode
  * @ingroup grp_adc
  */
static float LireTensionADC(ADC_HandleTypeDef *hadc, uint32_t canal, float ratio_pont)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = canal;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.OffsetSignedSaturation = DISABLE;
    if (HAL_ADC_ConfigChannel(hadc, &sConfig) != HAL_OK)
    {
        return -1.0f;
    }

    if (HAL_ADC_Start(hadc) != HAL_OK)
    {
        return -1.0f;
    }
    if (HAL_ADC_PollForConversion(hadc, 10) != HAL_OK)
    {
        HAL_ADC_Stop(hadc);
        return -1.0f;
    }

    uint32_t code_brut = HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);

    float vadc = ((float)code_brut / 65535.0f) * 3.3f;  // VDDA suppose a 3.3V
    return vadc / ratio_pont;
}

/**
  * @brief  Handler de la commande reseau "GET_MESURE_REELLE_<nom>" (voir
  *         le branchement dans tc_request(), meme motif que
  *         GET_STATUS_LDO_<nom> : prefixe + findLdoIndex() pour resoudre
  *         idx directement, sans parser last_message).
  *         Selectionne d'abord le canal du multiplexeur associe a la
  *         LDO, PUIS lit l'unique ADC partage.
  * @param  idx  Index de la LDO (0-17, voir muxLdoConfig ci-dessus),
  *              deja resolu par findLdoIndex() dans tc_request().
  * @param  nom  Nom texte de la LDO (ex : "1PSp"), pour la reponse.
  * @retval Aucun (void). Remplit response avec :
  *         "MESURE:<nom>:<tension>V\n"   si la lecture reussit,
  *         "MESURE:<nom>:NON_CABLE\n"    si muxLdoConfig[idx].canal_mux
  *                                       vaut encore 0xFF,
  *         "MESURE:<nom>:ERREUR\n"       si la conversion ADC echoue.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_MESURE_REELLE_1PSp"
  * Reponse type    : "MESURE:1PSp:3.075V\n"
  * @endcode
  * @ingroup grp_adc
  */
static void GetMesureReelle(int idx, const char *nom)
{
    MuxLdoConfig_t *cfg = &muxLdoConfig[idx];

    if (cfg->canal_mux == 0xFF)
    {
        sprintf(response, "MESURE:%s:NON_CABLE\n", nom);
    }
    else
    {
        SelectionnerCanalMuxTension(cfg->canal_mux);
        float v = LireTensionADC(&hadc1, ADC_CANAL_TENSION, MUX_RATIO_PONT);
        if (v < 0.0f)
        {
            sprintf(response, "MESURE:%s:ERREUR\n", nom);
        }
        else
        {
            sprintf(response, "MESURE:%s:%.3fV\n", nom, v);
        }
    }

    // Envoye aussi sur UART, en plus de la reponse UDP (response) : le
    // meme resultat est donc visible sur les deux canaux, quel que soit
    // le declencheur (commande UDP directe, ou appel depuis
    // ExecuterCaptureComplete()/executer_rampe() cote Python).
    UART1_EnvoyerTexte(response);
}

// ---------------------------------------------------------------------------
// Mesure REELLE du courant de sortie (PS uniquement : PSp/PSn, 12 LDO -
// PLp/PLn n'ont pas ce circuit). Chaine materielle :
//   1) Shunt 100 mOhm en sortie du LDO.
//   2) AOP #1 (differentiel, gain x100 via 100k/1k) -> tension
//      proportionnelle au courant reel : Vampli = I_reel x 0.1 x 100
//      = I_reel x 10 (I en A, Vampli en V).
//   3) Cette meme tension Vampli est aussi ce qui, comparee au seuil
//      programme par SET_CURRENT_LDO, produit le signal OC (voir le
//      commentaire au-dessus des struct LDO_x_PS_p/n, plus haut).
// Reutilise le MEME multiplexeur/ADC que la mesure de tension (une
// seule sortie ADC au total, cf. muxLdoConfig) : seule la table de
// correspondance canal et la formule de conversion changent.
//
// TODO A CONFIRMER : la tension d'alimentation reelle de l'AOP #1
// n'est pas connue au moment de l'ecriture de ce code. MUX_RATIO_PONT
// (partage avec la mesure de tension) est repris tel quel ici par
// defaut (1.0f, aucune division supplementaire) - si l'AOP #1 peut
// sortir plus haut que ~3.3V, il faudra un pont diviseur DEDIE avant
// ce canal precis, avec un ratio propre a corriger dans
// LireCourantADC() ci-dessous.
// ---------------------------------------------------------------------------

/**
  * @brief  Configuration du multiplexage pour la mesure REELLE de
  *         courant. Un element par LDO, MEME INDEXATION que
  *         muxLdoConfig (voir findLdoIndex()) - mais seuls les index
  *         correspondant a une LDO *PSp, *PSn seront jamais renseignes
  *         (les *PLp, *PLn n'ont pas ce circuit de mesure).
  */
typedef struct {
    uint8_t canal_mux;  // 0-31, partage avec muxLdoConfig (jamais le meme numero utilise dans les deux tables), ou 0xFF si pas encore cable
} MuxLdoConfigCourant_t;

// TODO : completer canal_mux au fur et a mesure du cablage physique
// reel, UNIQUEMENT pour les index *PSp, *PSn (0-11). Les index *PLp/PLn
// (12-17) doivent rester a 0xFF en permanence (pas de circuit courant).
// Valeurs initiales : colonnes "mux V" / "mux I" de ldoConfig[], recopiees
// par LDO_InitialiserTables() au demarrage (les 0xFF ci-dessous sont ecrases).
static MuxLdoConfigCourant_t muxLdoConfigCourant[NB_LDO];   /* MUX_NC recopie par LDO_InitialiserTables() */

/**
  * @brief  Lit le canal ADC courant et convertit en courant REEL (mA),
  *         a partir du gain de l'AOP differentiel (x100) et du shunt
  *         (100 mOhm) : I_reel(A) = Vmesuree(V) / (shunt x gain) =
  *         Vmesuree / 10, soit I_reel(mA) = Vmesuree(V) x 100.
  * @param  hadc          Handle ADC a utiliser (&hadc1).
  * @param  canal         Canal ADC deja configure (ADC_CANAL_COURANT, dedie
  *                       avec la mesure de tension).
  * @param  ratio_pont    Ratio de pont diviseur eventuel avant ce canal
  *                       precis (1.0f = aucun, voir le TODO ci-dessus).
  * @retval float  Courant reel en mA, ou -1.0f si la conversion ADC echoue.
  * @par Exemple d'utilisation
  * @code
  * float i_mA = LireCourantADC(&hadc3, ADC_CANAL_COURANT, 1.0f);
  * @endcode
  * @ingroup grp_adc
  */
static float LireCourantADC(ADC_HandleTypeDef *hadc, uint32_t canal, float ratio_pont)
{
    float vmesuree = LireTensionADC(hadc, canal, ratio_pont);
    if (vmesuree < 0.0f)
    {
        return -1.0f;
    }
    // mA : I = VMES / k, k = OC_K_V_PAR_A (main.h). A corriger si un pont
    // diviseur est place avant l'ADC sur cette voie (voir section 13 du rapport).
    return vmesuree * 1000.0f / OC_K_V_PAR_A;
}

/**
  * @brief  Handler de la commande reseau "GET_MESURE_COURANT_REELLE_<nom>"
  *         (voir le branchement dans tc_request(), meme motif que
  *         GET_MESURE_REELLE_<nom> : prefixe + findLdoIndex()).
  *         Selectionne le canal du multiplexeur associe a la mesure de
  *         COURANT de cette LDO (muxLdoConfigCourant, pas muxLdoConfig),
  *         puis lit l'ADC partage.
  * @param  idx  Index de la LDO (0-17), deja resolu par findLdoIndex().
  * @param  nom  Nom texte de la LDO, pour la reponse.
  * @retval Aucun (void). Remplit response avec :
  *         "MESURE_I:<nom>:<courant_mA>mA\n"  si la lecture reussit,
  *         "MESURE_I:<nom>:NON_CABLE\n"       si muxLdoConfigCourant[idx]
  *                                            vaut encore 0xFF (LDO PL,
  *                                            ou LDO PS pas encore cablee),
  *         "MESURE_I:<nom>:ERREUR\n"          si la conversion ADC echoue.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_MESURE_COURANT_REELLE_1PSp"
  * Reponse type    : "MESURE_I:1PSp:145.230mA\n"
  * @endcode
  * @ingroup grp_adc
  */
static void GetMesureCourantReelle(int idx, const char *nom)
{
    MuxLdoConfigCourant_t *cfg = &muxLdoConfigCourant[idx];

    if (cfg->canal_mux == 0xFF)
    {
        sprintf(response, "MESURE_I:%s:NON_CABLE\n", nom);
    }
    else
    {
        SelectionnerCanalMuxCourant(cfg->canal_mux);
        float i_mA = LireCourantADC(&hadc3, ADC_CANAL_COURANT, MUX_RATIO_PONT);
        if (i_mA < 0.0f)
        {
            sprintf(response, "MESURE_I:%s:ERREUR\n", nom);
        }
        else
        {
            sprintf(response, "MESURE_I:%s:%.3fmA\n", nom, i_mA);
        }
    }

    UART1_EnvoyerTexte(response);
}

/**
  * @brief  Handler de la commande reseau "SET_MUX_COURANT_LDO_<nom>:<canal>"
  *         - ECRIT le canal du multiplexeur associe a la mesure de
  *         COURANT d'une LDO (table separee de SET_MUX_LDO_, qui gere
  *         la tension). A chaud, sans recompilation.
  * @param  idx  Index de la LDO (0-17), deja resolu par findLdoIndex().
  * @param  nom  Nom texte de la LDO, pour la reponse.
  * @retval Aucun (void). Lit last_message (format "<canal>", 0-15).
  *         Remplit response avec "MUX_I_LDO:<nom>:CANAL=<n>\n" si
  *         valide, ou "MUX_I_LDO:<nom>:ERREUR_CANAL\n" sinon.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_MUX_COURANT_LDO_1PSp:9"
  * Reponse type    : "MUX_I_LDO:1PSp:CANAL=26\n"
  * @endcode
  * @ingroup grp_adc
  */
static void SetMuxCourantLdo(int idx, const char *nom)
{
    int canal = atoi(last_message);

    if (canal < 0 || canal > 15)
    {
        sprintf(response, "MUX_I_LDO:%s:ERREUR_CANAL\n", nom);
        return;
    }

    muxLdoConfigCourant[idx].canal_mux = (uint8_t)canal;
    sprintf(response, "MUX_I_LDO:%s:CANAL=%d\n", nom, canal);
}

/**
  * @brief  Handler de la commande reseau "GET_MUX_COURANT_LDO_<nom>" -
  *         LIT le canal du multiplexeur associe a la mesure de courant
  *         d'une LDO, sans le modifier.
  * @param  idx  Index de la LDO (0-17), deja resolu par findLdoIndex().
  * @param  nom  Nom texte de la LDO, pour la reponse.
  * @retval Aucun (void). Remplit response avec
  *         "MUX_I_LDO:<nom>:CANAL=<n>\n" si un canal est associe, ou
  *         "MUX_I_LDO:<nom>:NON_CABLE\n" sinon.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_MUX_COURANT_LDO_1PSp"
  * Reponse type    : "MUX_I_LDO:1PSp:CANAL=26\n"
  * @endcode
  * @ingroup grp_adc
  */
static void GetMuxCourantLdo(int idx, const char *nom)
{
    uint8_t canal = muxLdoConfigCourant[idx].canal_mux;

    if (canal == 0xFF)
    {
        sprintf(response, "MUX_I_LDO:%s:NON_CABLE\n", nom);
    }
    else
    {
        sprintf(response, "MUX_I_LDO:%s:CANAL=%d\n", nom, canal);
    }
}

/**
  * @brief  Handler de la commande reseau "SET_MUX_CANAL_TENSION:<canal>"
  *         - ECRIT l'etat de la chaine de multiplexage TENSION (change
  *         le canal actif, bus I2C1, 0-23).
  * @param  Aucun parametre C. Lit last_message (deja positionne apres
  *         le premier ':' par tc_request()), attendu au format "<canal>"
  *         (entier 0-23).
  * @retval Aucun (void). Remplit response avec "MUX:CANAL=<n>\n" si le
  *         canal est valide, ou "MUX:ERREUR_CANAL\n" sinon (hors plage
  *         0-23, ou parametre non numerique).
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_MUX_CANAL_TENSION:3"
  * Reponse type    : "MUX:CANAL=3\n"
  * @endcode
  * @ingroup grp_adc
  */
static void h_SET_MUX_CANAL_TENSION(void)
{
    int canal = atoi(last_message);

    if (canal < 0 || canal > 23)
    {
        sprintf(response, "MUX:ERREUR_CANAL\n");
        return;
    }

    SelectionnerCanalMuxTension((uint8_t)canal);
    sprintf(response, "MUX:CANAL=%d\n", canal);
}

/**
  * @brief  Handler de la commande reseau "GET_MUX_CANAL_TENSION" - LIT
  *         l'etat actuel de la chaine TENSION (dernier canal selectionne),
  *         sans le modifier.
  * @param  Aucun.
  * @retval Aucun (void). Remplit response avec "MUX:CANAL=<n>\n" si un
  *         canal a deja ete selectionne depuis le demarrage, ou
  *         "MUX:AUCUN\n" sinon.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_MUX_CANAL_TENSION"
  * Reponse type    : "MUX:CANAL=3\n"
  * @endcode
  * @ingroup grp_adc
  */
static void h_GET_MUX_CANAL_TENSION(void)
{
    if (canal_mux_tension_actuel == 0xFF)
    {
        sprintf(response, "MUX:AUCUN\n");
    }
    else
    {
        sprintf(response, "MUX:CANAL=%d\n", canal_mux_tension_actuel);
    }
}

/**
  * @brief  Handler de la commande reseau "SET_MUX_CANAL_COURANT:<canal>"
  *         - ECRIT l'etat de la chaine de multiplexage COURANT (change
  *         le canal actif, bus I2C2, 0-15).
  * @param  Aucun parametre C. Lit last_message, format "<canal>"
  *         (entier 0-15).
  * @retval Aucun (void). Remplit response avec "MUX:CANAL=<n>\n" si le
  *         canal est valide, ou "MUX:ERREUR_CANAL\n" sinon (hors plage
  *         0-15, ou parametre non numerique).
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "SET_MUX_CANAL_COURANT:5"
  * Reponse type    : "MUX:CANAL=5\n"
  * @endcode
  * @ingroup grp_adc
  */
static void h_SET_MUX_CANAL_COURANT(void)
{
    int canal = atoi(last_message);

    if (canal < 0 || canal > 15)
    {
        sprintf(response, "MUX:ERREUR_CANAL\n");
        return;
    }

    SelectionnerCanalMuxCourant((uint8_t)canal);
    sprintf(response, "MUX:CANAL=%d\n", canal);
}

/**
  * @brief  Handler de la commande reseau "GET_MUX_CANAL_COURANT" - LIT
  *         l'etat actuel de la chaine COURANT (dernier canal selectionne),
  *         sans le modifier.
  * @param  Aucun.
  * @retval Aucun (void). Remplit response avec "MUX:CANAL=<n>\n" si un
  *         canal a deja ete selectionne depuis le demarrage, ou
  *         "MUX:AUCUN\n" sinon.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_MUX_CANAL_COURANT"
  * Reponse type    : "MUX:CANAL=5\n"
  * @endcode
  * @ingroup grp_adc
  */
static void h_GET_MUX_CANAL_COURANT(void)
{
    if (canal_mux_courant_actuel == 0xFF)
    {
        sprintf(response, "MUX:AUCUN\n");
    }
    else
    {
        sprintf(response, "MUX:CANAL=%d\n", canal_mux_courant_actuel);
    }
}

/**
  * @brief  Handler de la commande reseau "GET_ADC_BRUT:<numero_adc>:<canal>"
  *         - lit un canal ADC DIRECTEMENT, sans passer par le
  *         multiplexeur (SelectionnerCanalMuxTension/Courant). Utile en
  *         phase de test sur breadboard, quand le multiplexeur n'est pas
  *         encore cable : on branche l'ADC directement sur la sortie a
  *         verifier (ex : un VOUT precis de 0xC4).
  * @param  Aucun parametre C. Lit last_message (deja positionne apres le
  *         premier ':' par tc_request()), format "<numero_adc>:<canal>" :
  *         numero_adc = 1 (hadc1) ou 3 (hadc3), canal = numero ADC_CHANNEL_x.
  * @retval Aucun (void). Remplit response avec "ADC_BRUT:<tension>V\n"
  *         si la lecture reussit, ou "ADC_BRUT:ERREUR_PARAMETRES\n" /
  *         "ADC_BRUT:ERREUR\n" sinon. Tension calculee SANS aucun pont
  *         diviseur (ratio 1.0f) : c'est la tension reelle au pin ADC,
  *         a toi de tenir compte d'un eventuel pont externe manuellement.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "GET_ADC_BRUT:1:4"
  * -> lit hadc1, ADC_CHANNEL_4, repond "ADC_BRUT:1.650V\n"
  * @endcode
  * @ingroup grp_adc
  */
static void h_GET_ADC_BRUT(void)
{
    int numero_adc = 0, canal = 0;
    int n = sscanf(last_message, "%d:%d", &numero_adc, &canal);

    if (n != 2 || canal < 0 || canal > 19)
    {
        sprintf(response, "ADC_BRUT:ERREUR_PARAMETRES\n");
        return;
    }

    ADC_HandleTypeDef *hadc = (numero_adc == 3) ? &hadc3 : &hadc1;
    float v = LireTensionADC(hadc, (uint32_t)canal, 1.0f);  // ratio 1.0 = lecture brute, sans pont

    if (v < 0.0f)
    {
        sprintf(response, "ADC_BRUT:ERREUR\n");
    }
    else
    {
        sprintf(response, "ADC_BRUT:%.3fV\n", v);
    }
}

/**
  * @brief  Handler de la commande reseau "WHO".
  *         Renvoie la chaine d'identification de la carte (macro WHO_AM_I
  *         definie dans main.h).
  * @retval Aucun (void). Ecrit WHO_AM_I dans la variable globale response.
  * @par Exemple d'utilisation
  * @code
  * Trame UDP recue : "WHO" -> reponse "STM32 NULCEO H753ZI IRIS V1.0".
  * @endcode
  * @ingroup grp_comm
  */
static void h_WHO(void) { sprintf(response, WHO_AM_I); }

/* =====================================================================
   TABLE DE DISPATCH :

   BUT :

   	Pouvoir ajouter une commande n'importe où dans ce tableau, dans
   	n'importe quel ordre : sortCommandTable() la remettra à sa place
   	automatiquement au démarrage (voir plus bas + appel dans StartLwipTask).
	Attention la variable ne doit pas etre de type const" car qsort() doit pouvoir
   	réordonner le tableau en RAM.
   ===================================================================== */

typedef struct {
	const char *name;
	void (*handler)(void);
} Command_t;

static Command_t commandTable[] = {
	{ "BALAYER_DAC", h_BALAYER_DAC },
	{ "GET_CONFIG_LDO", h_GET_CONFIG_LDO },
	{ "GET_ADC_BRUT", h_GET_ADC_BRUT },
	{ "GET_CURRENT_1", h_GET_CURRENT_1 },
	{ "GET_CURRENT_2", h_GET_CURRENT_2 },
	{ "GET_DAC_CONFIG", h_GET_DAC_CONFIG },
	{ "GET_DAC_REG", h_GET_DAC_REG },
	{ "GET_MUX_CANAL_COURANT", h_GET_MUX_CANAL_COURANT },
	{ "GET_MUX_CANAL_TENSION", h_GET_MUX_CANAL_TENSION },
	{ "RST_BL_LDO_ALL", h_RST_BL_LDO_ALL },
	{ "RST_DELTA_AVDD_DVDD", h_RST_DELTA_AVDD_DVDD },
	{ "RST_DELTA_AVDD_VREF", h_RST_DELTA_AVDD_VREF },
	{ "RST_DELTA_DVDD_AVDD", h_RST_DELTA_DVDD_AVDD },
	{ "RST_DELTA_NRST_VREF", h_RST_DELTA_NRST_VREF },
	{ "RST_DELTA_VREF_AVDD", h_RST_DELTA_VREF_AVDD },
	{ "RST_DELTA_VREF_NRST", h_RST_DELTA_VREF_NRST },
	{ "RST_EN_LDO_ALL", h_RST_EN_LDO_ALL },
	{ "SCAN_I2C1", h_SCAN_I2C1 },
	{ "SCAN_I2C2", h_SCAN_I2C2 },
	{ "SET_BL_LDO_ALL", h_SET_BL_LDO_ALL },
	{ "SET_BL_TAB", h_SET_BL_TAB },
	{ "SET_DAC_GAIN", h_SET_DAC_GAIN },
	{ "SET_DAC_PD", h_SET_DAC_PD },
	{ "SET_DAC_VREF", h_SET_DAC_VREF },
	{ "SET_DELTA_AVDD_DVDD", h_SET_DELTA_AVDD_DVDD },
	{ "SET_DELTA_AVDD_VREF", h_SET_DELTA_AVDD_VREF },
	{ "SET_DELTA_DVDD_AVDD", h_SET_DELTA_DVDD_AVDD },
	{ "SET_DELTA_NRST_VREF", h_SET_DELTA_NRST_VREF },
	{ "SET_DELTA_VREF_AVDD", h_SET_DELTA_VREF_AVDD },
	{ "SET_DELTA_VREF_NRST", h_SET_DELTA_VREF_NRST },
	{ "SET_EN_LDO_ALL", h_SET_EN_LDO_ALL },
	{ "SET_EN_TAB", h_SET_EN_TAB },
	{ "SET_I2C2_ACTIF", h_SET_I2C2_ACTIF },
	{ "SET_MUX_CANAL_COURANT", h_SET_MUX_CANAL_COURANT },
	{ "SET_MUX_CANAL_TENSION", h_SET_MUX_CANAL_TENSION },
	{ "WHO", h_WHO },
};
#define NB_COMMANDS (sizeof(commandTable) / sizeof(commandTable[0]))

// Comparateur requis par bsearch() : compare le nom cherché (key)
// avec le champ "name" de chaque entrée Command_t.
/**
  * @brief  Fonction de comparaison utilisee par bsearch() pour retrouver
  *         une commande dans commandTable[] (qui doit etre triee, voir
  *         sortCommandTable()).
  * @param  key   Pointeur vers la chaine de caracteres recherchee (le token
  *               de commande extrait de la trame, ex: "WHO").
  * @param  elem  Pointeur vers l'entree Command_t courante du tableau,
  *               fourni automatiquement par bsearch().
  * @retval int  Resultat de strcmp() entre key et elem->name : negatif si
  *              key < elem->name, 0 si egal, positif si key > elem->name.
  * @par Exemple d'utilisation
  * @code
  * Jamais appelee directement : passee en argument a bsearch()
  * dans findCommandHandler().
  * @endcode
  * @ingroup grp_comm
  */
static int compareCommand(const void *key, const void *elem)
{
	const char *cmdName = (const char *)key;
	const Command_t *entry = (const Command_t *)elem;
	return strcmp(cmdName, entry->name);
}

/**
  * @brief  Recherche par dichotomie (bsearch) le handler associe a une
  *         commande exacte dans commandTable[].
  * @param  cmd  Chaine de commande complete telle qu'extraite de la trame
  *              (ex : "WHO", "SET_EN_TAB", "SET_VAL_LDO_1PSp").
  * @retval void(*)(void)  Pointeur vers la fonction handler correspondante,
  *                        ou NULL si la commande est inconnue.
  * @par Exemple d'utilisation
  * @code
  * void (*h)(void) = findCommandHandler("WHO");
  * if (h != NULL) h();
  * @endcode
  * @ingroup grp_comm
  */
static void (*findCommandHandler(const char *cmd))(void)
{
	Command_t *found = (Command_t *)bsearch(
		cmd, commandTable, NB_COMMANDS, sizeof(Command_t), compareCommand);
	return found ? found->handler : NULL;
}

// Comparateur pour qsort() : même logique que compareCommand(), mais
// qsort() passe deux "const void*" (deux éléments du tableau), alors
// que bsearch() passe une clé + un élément. D'où les deux casts.
/**
  * @brief  Fonction de comparaison utilisee par qsort() pour trier
  *         commandTable[] par ordre alphabetique de commande, prealable
  *         indispensable a l'utilisation de bsearch() dans findCommandHandler().
  * @param  a  Pointeur vers une premiere entree Command_t du tableau.
  * @param  b  Pointeur vers une seconde entree Command_t du tableau.
  * @retval int  Resultat de strcmp() entre les deux noms de commande.
  * @par Exemple d'utilisation
  * @code
  * Jamais appelee directement : passee en argument a qsort()
  * dans sortCommandTable().
  * @endcode
  * @ingroup grp_comm
  */
static int compareCommandForSort(const void *a, const void *b)
{
	const Command_t *ea = (const Command_t *)a;
	const Command_t *eb = (const Command_t *)b;
	return strcmp(ea->name, eb->name);
}

// À appeler UNE SEULE FOIS au démarrage, avant toute utilisation de
// findCommandHandler(). Trie commandTable[] par ordre alphabétique
// pour que bsearch() fonctionne, quel que soit l'ordre dans lequel
// les entrées ont été écrites dans le tableau ci-dessus.
/**
  * @brief  Trie une bonne fois pour toutes (au demarrage) le tableau
  *         commandTable[] par ordre alphabetique de commande.
  *         Doit imperativement etre appelee avant le premier appel a
  *         tc_request()/findCommandHandler(), sous peine de resultats
  *         de bsearch() incorrects.
  * @param  Aucun.
  * @retval Aucun (void). Trie commandTable[] en place.
  * @par Exemple d'utilisation
  * @code
  * Appelee une seule fois dans StartLwipTask(), avant la boucle
  * infinie :
  *     udpServer_init();
  *     sortCommandTable();
  * @endcode
  * @ingroup grp_comm
  */
static void sortCommandTable(void)
{
	qsort(commandTable, NB_COMMANDS, sizeof(Command_t), compareCommandForSort);
}

/**
 * @brief  Recopie dans les tables RAM modifiables a chaud les valeurs
 *         initiales declarees dans ldoConfig[] : mapping DAC tension/courant
 *         (mappingDacLdo[]) et voies des multiplexeurs de mesure
 *         (muxLdoConfig[], muxLdoConfigCourant[]). A appeler UNE FOIS au
 *         demarrage, avant OC_InitialiserSeuilsDefaut() et udpServer_init().
 *         Signale sur USART1 toute ligne incoherente (LDO surveillee sans DAC
 *         de seuil, seuil par defaut hors bornes, voie DAC > 7).
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * LDO_InitialiserTables();
 * OC_InitialiserSeuilsDefaut();
 * @endcode
 * @ingroup grp_config
 */
static void LDO_InitialiserTables(void)
{
    char ligne[80];

    for (int i = 0; i < NB_LDO; i++)
    {
        const LDO_Config_t *cfg = &ldoConfig[i];

        mappingDacLdo[i].dac_tension   = cfg->dac_tension;
        mappingDacLdo[i].canal_tension = cfg->canal_tension;
        mappingDacLdo[i].dac_courant   = cfg->dac_courant;
        mappingDacLdo[i].canal_courant = cfg->canal_courant;
        muxLdoConfig[i].canal_mux        = cfg->mux_tension;
        muxLdoConfigCourant[i].canal_mux = cfg->mux_courant;

        if ((cfg->oc.surveille != 0U) && (cfg->dac_courant == CIBLE_AUCUNE))
        {
            snprintf(ligne, sizeof(ligne), "CONFIG:%s:OC_SANS_DAC_SEUIL (non surveillee)\r\n", cfg->name);
            UART1_EnvoyerTexte(ligne);
        }
        if ((OC_EstSurveillee(i) != 0U) &&
            ((cfg->oc.seuil_defaut < cfg->oc.seuil_min) || (cfg->oc.seuil_defaut > cfg->oc.seuil_max)))
        {
            snprintf(ligne, sizeof(ligne), "CONFIG:%s:SEUIL_DEFAUT_%u_HORS_%u_%u\r\n", cfg->name,
                     (unsigned)cfg->oc.seuil_defaut, (unsigned)cfg->oc.seuil_min, (unsigned)cfg->oc.seuil_max);
            UART1_EnvoyerTexte(ligne);
        }
        if ((cfg->canal_tension > 7U) || (cfg->canal_courant > 7U))
        {
            snprintf(ligne, sizeof(ligne), "CONFIG:%s:VOIE_DAC_HORS_0_7\r\n", cfg->name);
            UART1_EnvoyerTexte(ligne);
        }
    }
}

/**
 * @brief  Active l'horloge d'un port GPIO (A a H).
 * @param  port  GPIOA ... GPIOH.
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * LDO_ActiverHorlogePort(GPIOE);
 * @endcode
 * @ingroup grp_config
 */
static void LDO_ActiverHorlogePort(GPIO_TypeDef *port)
{
    if      (port == GPIOA) { __HAL_RCC_GPIOA_CLK_ENABLE(); }
    else if (port == GPIOB) { __HAL_RCC_GPIOB_CLK_ENABLE(); }
    else if (port == GPIOC) { __HAL_RCC_GPIOC_CLK_ENABLE(); }
    else if (port == GPIOD) { __HAL_RCC_GPIOD_CLK_ENABLE(); }
    else if (port == GPIOE) { __HAL_RCC_GPIOE_CLK_ENABLE(); }
    else if (port == GPIOF) { __HAL_RCC_GPIOF_CLK_ENABLE(); }
    else if (port == GPIOG) { __HAL_RCC_GPIOG_CLK_ENABLE(); }
    else if (port == GPIOH) { __HAL_RCC_GPIOH_CLK_ENABLE(); }
    else { /* port inexistant sur le boitier : refuse par generer_config.py */ }
}

/**
 * @brief  Indique si une broche appartient a un bouton du firmware (PC13 USER,
 *         PD0 reset ADG728). Ces broches ne sont jamais reconfigurees par
 *         LDO_GPIO_Init() (generer_config.py le signale en avertissement).
 * @param  port  Port GPIO.
 * @param  pin   Masque de broche HAL.
 * @retval 1 si la broche est reservee a un bouton, 0 sinon.
 * @par Exemple d'utilisation
 * @code
 * if (LDO_BrocheBouton(GPIOC, GPIO_PIN_13) != 0U) { ... }
 * @endcode
 * @ingroup grp_config
 */
static uint8_t LDO_BrocheBouton(GPIO_TypeDef *port, uint16_t pin)
{
    return (((port == GPIOC) && (pin == GPIO_PIN_13)) || ((port == GPIOD) && (pin == GPIO_PIN_0))) ? 1U : 0U;
}

/**
 * @brief  Configure les broches de TOUTES les LDO a partir de ldoConfig[]
 *         (donc du CSV), independamment du .ioc : EN en sortie a 0, BLD en
 *         sortie au niveau inactif, OC et PG en entree (sans tirage : R109 et
 *         tirages externes). Les broches OC en interruption sont ensuite
 *         reprises par MX_OC_EXTI_Init(). Le niveau de sortie est ecrit AVANT
 *         le passage en sortie : aucune impulsion sur EN ni BLD.
 *         A appeler juste apres MX_GPIO_Init() (USER CODE BEGIN 2).
 * @retval Aucun.
 * @par Exemple d'utilisation
 * @code
 * MX_GPIO_Init();
 * LDO_GPIO_Init();
 * @endcode
 * @ingroup grp_config
 */
static void LDO_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    for (int i = 0; i < NB_LDO; i++)
    {
        const LDO_Config_t *cfg = &ldoConfig[i];
        const GPIO_PinState bld_inactif = (cfg->bld_niveau_actif == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;

        LDO_ActiverHorlogePort(cfg->en_port);
        LDO_ActiverHorlogePort(cfg->bld_port);
        LDO_ActiverHorlogePort(cfg->oc_port);
        LDO_ActiverHorlogePort(cfg->pg_port);

        g.Mode  = GPIO_MODE_OUTPUT_PP;
        g.Pull  = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        if (LDO_BrocheBouton(cfg->en_port, cfg->en_pin) == 0U)
        {
            HAL_GPIO_WritePin(cfg->en_port, cfg->en_pin, GPIO_PIN_RESET);
            g.Pin = cfg->en_pin;
            HAL_GPIO_Init(cfg->en_port, &g);
        }
        if (LDO_BrocheBouton(cfg->bld_port, cfg->bld_pin) == 0U)
        {
            HAL_GPIO_WritePin(cfg->bld_port, cfg->bld_pin, bld_inactif);
            g.Pin = cfg->bld_pin;
            HAL_GPIO_Init(cfg->bld_port, &g);
        }

        g.Mode = GPIO_MODE_INPUT;
        if (LDO_BrocheBouton(cfg->oc_port, cfg->oc_pin) == 0U)
        {
            g.Pin = cfg->oc_pin;
            HAL_GPIO_Init(cfg->oc_port, &g);
        }
        if (LDO_BrocheBouton(cfg->pg_port, cfg->pg_pin) == 0U)
        {
            g.Pin = cfg->pg_pin;
            HAL_GPIO_Init(cfg->pg_port, &g);
        }
    }
}

/**
 * @brief  Commande "GET_CONFIG_LDO" : nombre de LDO et empreinte de la
 *         configuration compilee. Le controleur la compare a celle du CSV pour
 *         signaler une carte non reflashee apres modification.
 * @retval Aucun. Reponse "CONFIG_LDO:NB=18:CRC=0x3A5E98C0".
 * @par Exemple d'utilisation
 * @code
 * Trame UDP recue : "GET_CONFIG_LDO"
 * @endcode
 * @ingroup grp_config
 */
static void h_GET_CONFIG_LDO(void)
{
    sprintf(response, "CONFIG_LDO:NB=%u:CRC=0x%08lX\n", (unsigned)NB_LDO, (unsigned long)LDO_CONFIG_CRC32);
}

/**
 * @brief  Mode d'une broche GPIO lu dans le registre MODER.
 * @param  port  Port GPIO (GPIOA ... GPIOK).
 * @param  pin   Masque de broche HAL (un seul bit).
 * @retval 'I' entree, 'O' sortie, 'F' fonction alternative, 'A' analogique.
 * @par Exemple d'utilisation
 * @code
 * char m = ModeBroche(GPIOF, GPIO_PIN_1);   // 'O' attendu pour BLD_LDO2_PSp
 * @endcode
 * @ingroup grp_config
 */
static char ModeBroche(GPIO_TypeDef *port, uint16_t pin)
{
    uint32_t n = 0U;
    while ((n < 16U) && ((pin & (1U << n)) == 0U)) { n++; }
    switch ((port->MODER >> (2U * n)) & 0x3U)
    {
        case 0U: return 'I';
        case 1U: return 'O';
        case 2U: return 'F';
        default: return 'A';
    }
}

/**
 * @brief  Commande "GET_DIAG_LDO_<nom>" : etat REEL des broches d'une LDO
 *         (niveau lu sur la broche, pas seulement la commande logicielle) et
 *         de la protection OC. Separe un defaut firmware d'un defaut carte :
 *         si BL=1/1O et que le bleeder ne conduit pas, ou si OC reste a 0
 *         avec VU=0 et FR=0 alors que le courant depasse le seuil, le
 *         probleme est en aval de la broche (carte), pas dans le code.
 *         VU (OC vu actif par la boucle) est remis a 0 a chaque lecture.
 * @param  idx  Index de la LDO (0-17).
 * @param  nom  Nom de la LDO, pour la reponse.
 * @retval Aucun. Reponse (aussi emise sur USART1), par exemple :
 *         "DIAG:2PSp:EN=1/1:BL=1/1O:OC=0/0:VU=0:FR=0:IT=1:ARME=1:VER=0:S=1056:1289mV:493mA:PG=1:V=860:DACR=0"
 *         EN=commande/broche, BL=commande/broche+mode MODER, OC=defaut/niveau
 *         brut, FR=fronts EXTI, S=code seuil, puis VDAC et seuil (hors hysteresis),
 *         V=code du DAC de tension en memoire, DACR=reconfigurations de sa puce
 *         par SurveillerConfigDacsI2C1().
 * @par Exemple d'utilisation
 * @code
 * Trame UDP recue : "GET_DIAG_LDO_2PSp"
 * @endcode
 * @ingroup grp_config
 */
static void LDO_Diagnostic(int idx, const char *nom)
{
    const LDO_Config_t *cfg = &ldoConfig[idx];
    const uint16_t code = CURRENT_LDO[idx];
    const uint32_t vdac_mv = (uint32_t)(((float)code * DAC_VDD_REFERENCE_V * 1000.0f) / 4096.0f + 0.5f);
    const uint32_t seuil_ma = (cfg->oc.k_mv_par_a != 0U) ? ((vdac_mv * 1000U) / cfg->oc.k_mv_par_a) : 0U;
    const uint8_t vu = oc_vu_actif[idx];
    oc_vu_actif[idx] = 0U;
    // Nombre de reconfigurations de la puce du DAC de tension (si sur I2C1).
    uint32_t dacr = 0U;
    for (uint32_t k = 0; k < NB_DAC_PUCES; k++)
    {
        if (tableDac[k].cible == mappingDacLdo[idx].dac_tension)
        {
            const int n = IndexDacI2C1(tableDac[k].hi2c, tableDac[k].adresse_hal);
            if (n >= 0) { dacr = dac_nb_reconfig[n]; }
        }
    }

    snprintf(response, sizeof(response),
             "DIAG:%s:EN=%u/%u:BL=%u/%u%c:OC=%u/%u:VU=%u:FR=%lu:IT=%u:ARME=%u:VER=%u:S=%u:%lumV:%lumA:PG=%u:V=%u:DACR=%lu\n",
             nom,
             (unsigned)EN_LDO[idx],
             (unsigned)(HAL_GPIO_ReadPin(cfg->en_port, cfg->en_pin) == GPIO_PIN_SET),
             (unsigned)BL_LDO[idx],
             (unsigned)(HAL_GPIO_ReadPin(cfg->bld_port, cfg->bld_pin) == GPIO_PIN_SET),
             ModeBroche(cfg->bld_port, cfg->bld_pin),
             (unsigned)OC_LireDefaut(idx),
             (unsigned)(HAL_GPIO_ReadPin(cfg->oc_port, cfg->oc_pin) == GPIO_PIN_SET),
             (unsigned)vu,
             (unsigned long)oc_nb_fronts[idx],
             (unsigned)oc_it_active[idx],
             (unsigned)OC_EstArme(idx),
             (unsigned)oc_verrou[idx],
             (unsigned)code,
             (unsigned long)vdac_mv,
             (unsigned long)seuil_ma,
             (unsigned)(HAL_GPIO_ReadPin(cfg->pg_port, cfg->pg_pin) == GPIO_PIN_SET),
             (unsigned)VAL_LDO[idx],
             (unsigned long)dacr);
    UART1_EnvoyerTexte(response);
}

/* =====================================================================
   tc_request : dispatch générique
   ===================================================================== */
/**
  * @brief  Analyseur (parser) principal des trames recues par UDP.
  *         Extrait le premier token de la trame (avant le premier ":"),
  *         puis tente de le faire correspondre, dans l'ordre :
  *         1) aux prefixes indexes par LDO (SET_EN_LDO_, RST_EN_LDO_,
  *            SET_BL_LDO_, RST_BL_LDO_, GET_STATUS_LDO_) via findLdoIndex() ;
  *         2) sinon, a une commande exacte de commandTable[] via
  *            findCommandHandler(), dont le handler associe est alors execute.
  *         Si aucune correspondance n'est trouvee, la reponse "UNKNOWN" est
  *         renvoyee.
  * @param  Aucun parametre C. Lit et modifie la variable globale
  *         last_message (message brut recu), et ecrit le resultat dans
  *         la variable globale response.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * last_message = "SET_VAL_LDO_1PSp:2048";
  * tc_request();   // -> execute h_SET_VAL_LDO_1PSp(), response="DONE"
  * @endcode
  * @ingroup grp_comm
  */
void tc_request(void)
{
	char *token = strsep(&last_message, ":");
	int idx;
	void (*handler)(void);

	sprintf(response, "DONE\n");

	// 1) Commandes à suffixe variable sur les 18 LDO (EN/BL/STATUS)
	if (strncmp(token, "SET_EN_LDO_", 11) == 0 && (idx = findLdoIndex(token + 11)) != -1)
	{
		if (oc_verrou[idx] != 0U)
		{
			sprintf(response, "EN:%s:REFUS_OC_VERROUILLE\n", token + 11);
		}
		else if ((BL_LDO[idx] != 0) && (ldoConfig[idx].bld_coupe_en != 0U))
		{
			sprintf(response, "EN:%s:REFUS_BLEEDER_ACTIF\n", token + 11);
		}
		else
		{
			EN_LDO[idx] = 1;
			if (OC_SeuilValide(idx) == 0U)
			{
				sprintf(response, "EN:%s:ATTENTE_SEUIL_OC\n", token + 11);
			}
		}
	}
	else if (strncmp(token, "RST_OC_LDO_", 11) == 0 && (idx = findLdoIndex(token + 11)) != -1)
	{
		OC_Rearmer(idx, token + 11);
	}
	else if (strncmp(token, "GET_OC_LDO_", 11) == 0 && (idx = findLdoIndex(token + 11)) != -1)
	{
		OC_Etat(idx, token + 11);
	}
	else if (strncmp(token, "GET_DIAG_LDO_", 13) == 0 && (idx = findLdoIndex(token + 13)) != -1)
	{
		LDO_Diagnostic(idx, token + 13);
	}
	else if (strncmp(token, "RST_EN_LDO_", 11) == 0 && (idx = findLdoIndex(token + 11)) != -1)
	{
		EN_LDO[idx] = 0;
	}
	else if (strncmp(token, "SET_BL_LDO_", 11) == 0 && (idx = findLdoIndex(token + 11)) != -1)
	{
		if (cmd_on_off_bld[idx] == 0U)
		{
			sprintf(response, "BL:%s:REFUS_BL_OFF\n", token + 11);
		}
		else
		{
			BL_LDO[idx] = 1;
		}
	}
	else if (strncmp(token, "RST_BL_LDO_", 11) == 0 && (idx = findLdoIndex(token + 11)) != -1 &&
	         cmd_on_off_bld[idx] == 0U)
	{
		sprintf(response, "BL:%s:REFUS_BL_OFF\n", token + 11);
	}
	else if (strncmp(token, CMD_GET_BLD, sizeof(CMD_GET_BLD) - 1U) == 0 &&
	         (idx = findLdoIndex(token + sizeof(CMD_GET_BLD) - 1U)) != -1)
	{
		CmdOnOff_Repondre("CMD_ON_OFF_BLD", idx, cmd_on_off_bld[idx]);
	}
	else if (strncmp(token, CMD_GET_OC, sizeof(CMD_GET_OC) - 1U) == 0 &&
	         (idx = findLdoIndex(token + sizeof(CMD_GET_OC) - 1U)) != -1)
	{
		CmdOnOff_Repondre("CMD_ON_OFF_OC", idx, cmd_on_off_oc[idx]);
	}
	else if (strncmp(token, CMD_SWITCH_BLD, sizeof(CMD_SWITCH_BLD) - 1U) == 0 &&
	         (idx = findLdoIndex(token + sizeof(CMD_SWITCH_BLD) - 1U)) != -1)
	{
		CmdOnOff_BasculerBld(idx);
		CmdOnOff_Repondre("CMD_ON_OFF_BLD", idx, cmd_on_off_bld[idx]);
	}
	else if (strncmp(token, CMD_SWITCH_OC, sizeof(CMD_SWITCH_OC) - 1U) == 0 &&
	         (idx = findLdoIndex(token + sizeof(CMD_SWITCH_OC) - 1U)) != -1)
	{
		CmdOnOff_BasculerOc(idx);
		CmdOnOff_Repondre("CMD_ON_OFF_OC", idx, cmd_on_off_oc[idx]);
	}
	else if (strncmp(token, "RST_BL_LDO_", 11) == 0 && (idx = findLdoIndex(token + 11)) != -1)
	{
		// Desactivation du bleeder = rearmement apres un declenchement OC.
		const uint8_t r = LDO_RelacherBleeder(idx);
		if (r == 2U)
		{
			sprintf(response, "BL:%s:REFUS_OC_ACTIF\n", token + 11);
		}
		else if (r == 1U)
		{
			sprintf(response, "DONE:BL:%s:DEFAUT_OC_EFFACE\n", token + 11);
		}
	}
	else if (strncmp(token, "GET_STATUS_LDO_", 15) == 0 && (idx = findLdoIndex(token + 15)) != -1)
	{
		buildStatusResponse(idx);
	}
	else if (strncmp(token, "GET_MESURE_REELLE_", 19) == 0 && (idx = findLdoIndex(token + 19)) != -1)
	{
		GetMesureReelle(idx, token + 19);
	}
	else if (strncmp(token, "GET_MESURE_COURANT_REELLE_", 26) == 0 && (idx = findLdoIndex(token + 26)) != -1)
	{
		GetMesureCourantReelle(idx, token + 26);
	}
	else if (strncmp(token, "SET_VAL_LDO_", 12) == 0 && (idx = findLdoIndex(token + 12)) != -1)
	{
		h_SET_VAL_LDO_GENERIQUE(idx);
	}
	else if (strncmp(token, "SET_CURRENT_LDO_", 16) == 0 && (idx = findLdoIndex(token + 16)) != -1)
	{
		h_SET_CURRENT_LDO_GENERIQUE(idx);
	}
	else if (strncmp(token, "SET_DAC_MAPPING_LDO_", 20) == 0 && (idx = findLdoIndex(token + 20)) != -1)
	{
		SetMappingDacLdo(idx, token + 20);
	}
	else if (strncmp(token, "GET_DAC_MAPPING_LDO_", 20) == 0 && (idx = findLdoIndex(token + 20)) != -1)
	{
		GetMappingDacLdo(idx, token + 20);
	}
	else if (strncmp(token, "SET_MUX_LDO_", 12) == 0 && (idx = findLdoIndex(token + 12)) != -1)
	{
		SetMuxLdo(idx, token + 12);
	}
	else if (strncmp(token, "GET_MUX_LDO_", 12) == 0 && (idx = findLdoIndex(token + 12)) != -1)
	{
		GetMuxLdo(idx, token + 12);
	}
	else if (strncmp(token, "SET_MUX_COURANT_LDO_", 20) == 0 && (idx = findLdoIndex(token + 20)) != -1)
	{
		SetMuxCourantLdo(idx, token + 20);
	}
	else if (strncmp(token, "GET_MUX_COURANT_LDO_", 20) == 0 && (idx = findLdoIndex(token + 20)) != -1)
	{
		GetMuxCourantLdo(idx, token + 20);
	}
	// 2) Toutes les autres commandes -> table de dispatch
	else if ((handler = findCommandHandler(token)) != NULL)
	{
		handler();
	}
	// 3) Commande inconnue
	else
	{
		sprintf(response, "UNKNOWN\n");
	}
}

/**
  * @brief  Met a l'echelle lineaire une valeur d'une plage d'entree vers
  *         une plage de sortie (equivalent de la fonction map() d'Arduino).
  *         Utilisee pour convertir une lecture ADC brute (0-65535) en
  *         pourcentage (0-100).
  * @param  x        Valeur a convertir.
  * @param  in_min   Borne basse de la plage d'entree.
  * @param  in_max   Borne haute de la plage d'entree.
  * @param  out_min  Borne basse de la plage de sortie.
  * @param  out_max  Borne haute de la plage de sortie.
  * @retval int  Valeur convertie, ramenee dans la plage [out_min, out_max].
  * @par Exemple d'utilisation
  * @code
  * int pourcentage = map(32768, 0, 65535, 0, 100);   // ~= 50
  * @endcode
  * @ingroup grp_outils
  */
int map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max)
{
	return (x - in_min) * (out_max - out_min + 1) / (in_max - in_min + 1) + out_min;
}

/**
 * @brief  Ligne EXTI (IRQn) correspondant a une broche GPIO (la ligne EXTI n
 *         est partagee par la broche n de TOUS les ports).
 * @param  pin  Masque de broche HAL (GPIO_PIN_0 ... GPIO_PIN_15), un seul bit.
 * @retval IRQn_Type de la ligne EXTI correspondante.
 * @par Exemple d'utilisation
 * @code
 * HAL_NVIC_EnableIRQ(OC_IrqDepuisBroche(GPIO_PIN_4));   // EXTI4_IRQn
 * @endcode
 * @ingroup grp_oc
 */
static IRQn_Type OC_IrqDepuisBroche(uint16_t pin)
{
    switch (pin)
    {
        case GPIO_PIN_0: return EXTI0_IRQn;
        case GPIO_PIN_1: return EXTI1_IRQn;
        case GPIO_PIN_2: return EXTI2_IRQn;
        case GPIO_PIN_3: return EXTI3_IRQn;
        case GPIO_PIN_4: return EXTI4_IRQn;
        case GPIO_PIN_5: case GPIO_PIN_6: case GPIO_PIN_7:
        case GPIO_PIN_8: case GPIO_PIN_9: return EXTI9_5_IRQn;
        default:         return EXTI15_10_IRQn;
    }
}

// Lignes EXTI deja prises par les boutons (PD0 reset ADG728, PC13 USER) :
// une broche OC sur ces numeros ne peut pas etre mise en interruption.
#define EXTI_LIGNES_RESERVEES   ((uint16_t)(GPIO_PIN_0 | GPIO_PIN_13))

/**
 * @brief  Met en interruption la broche OC de chaque LDO declaree avec
 *         oc.interruption = 1 dans ldoConfig[], sur le front qui correspond a
 *         l'apparition d'une surintensite (montant si niveau_defaut = SET).
 *         Refuse (ligne UART "OC_EXTI:<nom>:REFUS_LIGNE_n") une broche dont
 *         la ligne EXTI est deja prise (bouton, ou autre LDO de meme numero
 *         de broche sur un autre port) : la LDO reste alors surveillee par
 *         la boucle seule. Aucune API FreeRTOS n'est appelee dans l'IT : la
 *         priorite 2 peut etre plus haute que configMAX_SYSCALL_INTERRUPT_PRIORITY.
 * @retval Aucun. Remplit oc_it_active[].
 * @par Exemple d'utilisation
 * @code
 * InitialiserConfigDacsI2C1();
 * MX_OC_EXTI_Init();
 * @endcode
 * @ingroup grp_oc
 */
static void MX_OC_EXTI_Init(void)
{
    uint16_t lignes_prises = EXTI_LIGNES_RESERVEES;

    for (int i = 0; i < NB_LDO; i++)
    {
        const LDO_Config_t *cfg = &ldoConfig[i];
        oc_it_active[i] = 0U;
        if ((OC_EstSurveillee(i) == 0U) || (cfg->oc.interruption == 0U))
        {
            continue;
        }
        if ((lignes_prises & cfg->oc_pin) != 0U)
        {
            char ligne[64];
            uint32_t n = 0U;
            while ((n < 16U) && ((cfg->oc_pin & (1U << n)) == 0U)) { n++; }
            snprintf(ligne, sizeof(ligne), "OC_EXTI:%s:REFUS_LIGNE_%lu\r\n", cfg->name, (unsigned long)n);
            UART1_EnvoyerTexte(ligne);
            continue;
        }

        GPIO_InitTypeDef g = {0};
        g.Pin  = cfg->oc_pin;
        g.Mode = (cfg->oc.niveau_defaut == GPIO_PIN_SET) ? GPIO_MODE_IT_RISING : GPIO_MODE_IT_FALLING;
        g.Pull = GPIO_NOPULL;              /* pull-up externe R109 (5,1 kOhm vers 3,3 V) */
        HAL_GPIO_Init(cfg->oc_port, &g);

        HAL_NVIC_SetPriority(OC_IrqDepuisBroche(cfg->oc_pin), 2, 0);
        HAL_NVIC_EnableIRQ(OC_IrqDepuisBroche(cfg->oc_pin));
        lignes_prises |= cfg->oc_pin;
        oc_it_active[i] = 1U;
    }
}

/*
 * Gestionnaires d'interruption EXTI des lignes 1 a 4 et 9:5. Chacun
 * delegue a HAL_GPIO_EXTI_IRQHandler(), qui n'appelle le callback que
 * si le drapeau de la broche est leve : sans effet si la ligne n'est
 * pas utilisee. Lignes 0 et 15:10 : voir plus bas (boutons).
 */
/**
 * @brief  Interruption EXTI ligne 1 : delegue a HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1)
 *         (broche OC d'une LDO declaree oc_interruption = 1 dans le CSV).
 * @param  Aucun.
 * @retval Aucun (void).
 * @par Exemple d'utilisation
 * @code
 * // appele par le materiel, ne pas appeler directement
 * @endcode
 * @ingroup grp_exti
 */
void EXTI1_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1); }
/**
 * @brief  Interruption EXTI ligne 2 : delegue a HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2)
 *         (broche OC d'une LDO declaree oc_interruption = 1 dans le CSV).
 * @param  Aucun.
 * @retval Aucun (void).
 * @par Exemple d'utilisation
 * @code
 * // appele par le materiel, ne pas appeler directement
 * @endcode
 * @ingroup grp_exti
 */
void EXTI2_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2); }
/**
 * @brief  Interruption EXTI ligne 3 : delegue a HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3)
 *         (broche OC d'une LDO declaree oc_interruption = 1 dans le CSV).
 * @param  Aucun.
 * @retval Aucun (void).
 * @par Exemple d'utilisation
 * @code
 * // appele par le materiel, ne pas appeler directement
 * @endcode
 * @ingroup grp_exti
 */
void EXTI3_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3); }
/**
 * @brief  Interruption EXTI ligne 4 : delegue a HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4)
 *         (broche OC d'une LDO declaree oc_interruption = 1 dans le CSV, ex : PE4 pour 2PSp).
 * @param  Aucun.
 * @retval Aucun (void).
 * @par Exemple d'utilisation
 * @code
 * // appele par le materiel, ne pas appeler directement
 * @endcode
 * @ingroup grp_exti
 */
void EXTI4_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4); }
/**
 * @brief  Interruption EXTI lignes 9 a 5 : delegue a HAL_GPIO_EXTI_IRQHandler()
 *         pour les broches 5 a 9 (broches OC declarees en interruption).
 * @param  Aucun.
 * @retval Aucun (void).
 * @par Exemple d'utilisation
 * @code
 * // appele par le materiel, ne pas appeler directement
 * @endcode
 * @ingroup grp_exti
 */
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_5);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
}

/**
  * @brief  Gestionnaire d'interruption EXTI ligne 15:10 (couvre PC13,
  *         bouton USER B1). Delegue immediatement a HAL_GPIO_EXTI_IRQHandler(),
  *         qui identifie la broche et appelle HAL_GPIO_EXTI_Callback().
  * @param  Aucun.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * // appele par le materiel, ne pas appeler directement
  * @endcode
  * @ingroup grp_exti
  */
void EXTI15_10_IRQHandler(void)
{
    /* PC13 (bouton USER) et toute broche OC de numero 10 a 15 declaree en
     * interruption dans configurationLDO/ldo_config.csv. Sans effet sur une
     * ligne inutilisee (le HAL teste le drapeau de chaque broche). */
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_14);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
}

/**
  * @brief  Gestionnaire d'interruption EXTI ligne 0 (couvre PD0, bouton
  *         de RESET materiel des ADG728). Meme principe que
  *         EXTI15_10_IRQHandler() ci-dessus.
  * @param  Aucun.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * // appele par le materiel, ne pas appeler directement
  * @endcode
  * @ingroup grp_exti
  */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/**
  * @brief  Callback appele par le HAL a chaque interruption EXTI, quelle
  *         que soit la broche. Boutons (PC13, PD0) : ne fait QUE positionner
  *         un flag, le traitement est fait par la boucle de StartLwipTask()
  *         (anti-rebond 250 ms par HAL_GetTick()). Broche OC d'une LDO dont
  *         l'EXTI est active (oc_it_active[]) : coupure de l'EN dans
  *         l'interruption (quelques us), voir OC_SurFrontIT().
  * @param  GPIO_Pin  Broche ayant declenche l'interruption.
  * @retval Aucun (void).
  * @par Exemple d'utilisation
  * @code
  * // appele par HAL_GPIO_EXTI_IRQHandler(), ne pas appeler directement
  * @endcode
  * @ingroup grp_exti
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint32_t maintenant = HAL_GetTick();

    if (GPIO_Pin == GPIO_PIN_13)
    {
        if ((maintenant - dernier_appui_ms) > 250)
        {
            dernier_appui_ms = maintenant;
            bouton_appuye = 1;
        }
    }
    else if (GPIO_Pin == GPIO_PIN_0)
    {
        if ((maintenant - dernier_reset_mux_ms) > 250)
        {
            dernier_reset_mux_ms = maintenant;
            mux_reset_appuye = 1;
        }
    }
    else
    {
        for (int i = 0; i < NB_LDO; i++)
        {
            if ((oc_it_active[i] != 0U) && (ldoConfig[i].oc_pin == GPIO_Pin))
            {
                OC_SurFrontIT(i);
                break;                 /* une ligne EXTI = une seule LDO (MX_OC_EXTI_Init) */
            }
        }
    }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartLwipTask */
/**
  * @brief  Tache FreeRTOS principale (lwipTask). Initialise LwIP, les verrous
  *         (I2C, UART, commandes), la reception UART, les tables issues du CSV
  *         (LDO_InitialiserTables()), les seuils OC, les DAC (configuration,
  *         restauration EEPROM), les commandes ON/OFF (flash), l'EXTI des OC et
  *         le serveur UDP, puis boucle sans fin : etat des LDO, surveillance OC,
  *         sauvegardes EEPROM/flash differees, commande UART en attente,
  *         boutons (capture complete, reset des multiplexeurs).
  * @param  argument  Non utilise (signature imposee par osThreadDef).
  * @retval Aucun (void). Ne retourne jamais.
  * @par Exemple d'utilisation
  * @code
  * // Cree par main() (code CubeMX) :
  * osThreadDef(lwipTask, StartLwipTask, osPriorityNormal, 0, 256);
  * lwipTaskHandle = osThreadCreate(osThread(lwipTask), NULL);
  * @endcode
  * @ingroup grp_tache
  */
/* USER CODE END Header_StartLwipTask */
void StartLwipTask(void const * argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN 5 */
  osDelay(1000);
  CreerMutexI2C();      // AVANT udpServer_init() : plus rien n'emet sur hi2c1/hi2c2 sans passer par un mutex a partir d'ici
  CreerMutexUART();     // idem pour USART1 : emissions de texte serialisees (rapports + battement de coeur)
  CreerMutexCommandes();             // verrou commun UDP / UART autour de tc_request()
  UART1_DemarrerReceptionCommandes(); // toutes les commandes UDP aussi acceptees sur l'UART
  LDO_InitialiserTables();      // mapping DAC et voies mux initiaux copies depuis ldoConfig[] (avant toute commande)
  OC_InitialiserSeuilsDefaut(); // seuil par defaut (ldoConfig[].oc.seuil_defaut) avant toute activation (EN reste a 0)
  InitialiserConfigDacsI2C1();  // VREF=VDD, PD=normal, Gain=1x sur 0xC0/0xC2/0xC4 (sinon config EEPROM de chaque puce)
  DAC_RestaurerMemoire();       // LDO MEM_EEPROM (2PSp) : codes tension + seuil relus dans l'EEPROM du DAC
  DAC_ConfigurerSeuilsPotentiometre(); // LDO SEUIL_POT : voie du DAC de courant en haute impedance (aucune aujourd'hui)
  CmdOnOff_Initialiser();       // CMD_ON_OFF_BLD / CMD_ON_OFF_OC : flash si sauvegarde valide, sinon CSV
  MX_OC_EXTI_Init();            // broches OC declarees oc.interruption = 1 (2PSp : PE4) : coupure immediate de l'EN
  udpServer_init();
  sortCommandTable();   // tri unique de commandTable[] avant le premier message reçu

  for (;;)
  {
    // EN / BLD / OC / PG des 18 LDO. OC est lu AVANT l'ecriture de EN, le
    // defaut est verrouille (voir LDO_MettreAJour() et OC_Verrouiller()).
    for (int i = 0; i < NB_LDO; i++)
    {
        LDO_MettreAJour(i);
    }
    // Une ligne "OC_TRIP:<nom>:..." sur USART1 par defaut nouvellement verrouille.
    OC_SignalerEvenements();

    // Chaque envoi est protege individuellement (verrou pris/relache par
    // appel) : une transaction lente sur un bus ne retarde jamais l'autre
    // bus, et une commande reseau (BALAYER_DAC, GET_DAC_CONFIG, ...) qui
    // attend le meme mutex n'est jamais bloquee plus que la duree d'UNE
    // transmission (quelques centaines de microsecondes a 100 kHz).
    // Rafraichissement des puces DAC, dans l'ordre de tableDac[] (identique a
    // l'ancien code : 0xC0/I2C1, 0xC0/I2C2, 0xC2/I2C1, 0xC4/I2C2, 0xC2/I2C2).
    // Chaque envoi est protege individuellement (verrou pris/relache par
    // appel) : une transaction lente sur un bus ne retarde jamais l'autre
    // bus. Puces sur I2C2 ignorees tant que i2c2_actif = 0. Puce en cours de
    // balayage (BALAYER_DAC) ignoree pour ne pas ecraser la rampe.
    for (uint32_t k = 0; k < NB_DAC_PUCES; k++)
    {
        const DacPuce_t *puce = &tableDac[k];
        if ((puce->hi2c == &hi2c2) && (i2c2_actif == 0U))
        {
            continue;
        }
        osMutexId m = ResoudreMutexI2C(puce->hi2c);
        osMutexWait(m, osWaitForever);
        if (!BALAYAGE_SUR(puce->hi2c, puce->adresse_hal))
        {
            HAL_I2C_Master_Transmit(puce->hi2c, puce->adresse_hal, puce->trame, SPI_SIZE, SPI_TIMEOUT);
        }
        osMutexRelease(m);
    }

    // Bouton USER (B1/PC13) appuye : declenche une capture complete de
    // diagnostic (scan I2C1+I2C2, DAC, balayage...), rapportee sur USART1.
    // Le flag est mis par HAL_GPIO_EXTI_Callback() (voir USER CODE 4).
    if (bouton_appuye)
    {
        bouton_appuye = 0;
        ExecuterCaptureComplete();
    }

    // Bouton de RESET materiel des ADG728 (PD0) appuye : resynchronise
    // canal_mux_tension_actuel et canal_mux_courant_actuel avec la
    // realite (tous canaux coupes par le reset, sur les deux chaines).
    // Le flag est mis par HAL_GPIO_EXTI_Callback() (voir USER CODE 4).
    if (mux_reset_appuye)
    {
        mux_reset_appuye = 0;
        TraiterResetMultiplexeur();
    }

    // Configuration VREF/PD/Gain des DAC de I2C1 : relue et restauree si perdue.
    SurveillerConfigDacsI2C1();

    // Codes DAC des LDO MEM_EEPROM : ecriture EEPROM differee (voir DAC_TraiterSauvegardes()).
    DAC_TraiterSauvegardes();

    // Commandes ON/OFF (OC, BL) : ecriture en flash apres une bascule.
    CmdOnOff_TraiterSauvegarde();

    // Commande recue sur l'UART : executee ici, reponse renvoyee sur l'UART.
    UART1_TraiterCommande();

#if UART_HEARTBEAT_MS > 0U
    // Battement de coeur UART : ligne "HB" toutes les UART_HEARTBEAT_MS ms, pour que
    // l'interface PC detecte un cable UART (PB6/PB7) debranche ou une carte arretee.
    // Pendant une capture (bouton B1) la boucle est occupee, mais la capture emet
    // elle-meme sur l'UART : le PC voit du trafic.
    if ((HAL_GetTick() - tick_dernier_hb_uart) >= UART_HEARTBEAT_MS)
    {
        tick_dernier_hb_uart = HAL_GetTick();
        UART1_EnvoyerTexte("HB\r\n");
    }
#endif

  }
  /* USER CODE END 5 */
}

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x30020000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.BaseAddress = 0x30040000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512B;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
#ifdef USE_FULL_ASSERT
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
