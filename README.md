<!--
  Auteur   : Florian Aizac
  Révision : 1.0.0
  Date     : 24/10/2026
-->

# Firmware Lwip_demo_H753 - pilotage des LDO de la carte d'alimentation

Firmware STM32H753ZI (Nucleo-H753ZI, STM32CubeIDE, FreeRTOS + LwIP) qui pilote les régulateurs
LDO de la carte d'alimentation :

- **Enable** (EN), **bleeder** (BLD), lecture **Power-Good** (PG) et **Over-Current** (OC) de chaque LDO ;
- **consigne de tension** et **seuil de limitation de courant** par DAC MCP47FEB28 (I2C) ;
- **protection surintensité** : coupure de l'EN sur la sortie du comparateur LM211, défaut mémorisé ;
- serveur de commandes **UDP** (192.168.0.123, port 7) et **UART** (USART1, 115200 8N1) ;
- configuration du câblage des LDO dans un **fichier CSV**, compilé dans le firmware.

L'interface PC est le projet `Controleur` (Flask, voir son `README.md`).

## Matériel et liaisons

| Élément | Détail |
|---|---|
| Carte | Nucleo-H753ZI (STM32H753ZIT6) |
| Ethernet | RMII, IP fixe `192.168.0.123` (`IP4_1..IP4_4`), serveur UDP port `7` (`UDP_SERVER_PORT`), dans `main.h` |
| UART | USART1, PB6 (TX) / PB7 (RX), 115200 8N1 : journal + commandes |
| I2C1 | PB8 / PB9 : DAC MCP47FEB28 `0xC0`, `0xC2`, `0xC4` (adresses HAL décalées), multiplexeurs ADG728 |
| I2C2 | PB10 / PB11 : DAC hérités, **désactivé par défaut** (`SET_I2C2_ACTIF:1` pour l'activer) |
| Boutons | PC13 (USER B1) : capture de diagnostic ; PD0 : reset des ADG728 |

## Arborescence du projet

```
Lwip_demo_H753/
|-- .project / .cproject        <- projet actif STM32CubeIDE et configurations Debug / Release
|-- Lwip_demo_H753.ioc          <- CubeMX : horloges, Ethernet, I2C, UART, ADC et FreeRTOS
|-- Lwip_demo_H753.launch       <- configuration de lancement / débogage
|-- STM32H753ZITX_FLASH.ld      <- linker FLASH : 128 Ko réservés aux commandes ON/OFF
|-- STM32H753ZITX_RAM.ld        <- linker RAM
|-- configurationLDO/
|   |-- ldo_config.csv          <- câblage et paramètres des LDO
|   |-- ldo_config_csv.py       <- lecture, validation et génération du CSV
|   |-- generer_config.py       <- génération avant compilation
|   |-- mettre_a_jour.bat       <- procédure de compilation et de flash
|   `-- README.txt              <- colonnes du CSV et règles de configuration
|-- Core/
|   |-- Inc/                    <- configuration FreeRTOS, en-têtes et fichiers LDO générés
|   |-- Src/                    <- application dans main.c, initialisation et interruptions
|   `-- Startup/                <- démarrage STM32H753
|-- LWIP/
|   |-- App/                    <- initialisation et configuration LwIP
|   `-- Target/                 <- interface Ethernet et options LwIP
|-- Drivers/
|   |-- BSP/                    <- composants de carte, dont LAN8742
|   |-- CMSIS/                  <- en-têtes CMSIS
|   `-- STM32H7xx_HAL_Driver/   <- pilotes HAL ST
|-- Middlewares/Third_Party/
|   |-- FreeRTOS/               <- noyau RTOS
|   `-- LwIP/                   <- pile réseau
|-- Doxygen/                    <- sources et outils de documentation de conception
|-- STM32CubeIDE/               <- fichiers complémentaires du projet CubeIDE
|-- Debug/                      <- fichiers générés par la configuration Debug
`-- Release/                    <- fichiers générés par la configuration Release
```

Le projet CubeIDE actif est celui défini à la racine par `.project` et `.cproject`.
Les dossiers `Debug/` et `Release/` contiennent les sorties de compilation. Le code applicatif
et les gestionnaires de commandes sont principalement regroupés dans `Core/Src/main.c`.

Les fichiers générés `Core/Inc/ldo_config_genere.h` et `Core/Inc/ldo_config_lignes.inc`
ne se modifient pas à la main : ils sont réécrits à chaque compilation depuis le CSV.

## Configuration des LDO (CSV)

Le câblage de chaque LDO est décrit par **une ligne** de `configurationLDO/ldo_config.csv` :
nom, broches EN / BLD / OC / PG, DAC de tension et DAC de courant (`BUS:ADRESSE:VOIE`),
protection, source du seuil, mémoire des codes DAC, valeurs initiales `CMD_ON_OFF_BLD` /
`CMD_ON_OFF_OC`. Le détail des colonnes est dans `configurationLDO/README.txt`.

- L'ordre des lignes = index de la LDO (`SET_EN_TAB`, `SET_BL_TAB`, IHM).
- Toutes les fonctions (OC, bleeder, EN, DAC, EEPROM, GPIO) lisent uniquement la table générée
  `ldoConfig[]` : changer ou ajouter une LDO = modifier le CSV, jamais le code.
- Exemple, LDO 2 (PS2+), seul rail testé avec la limitation :
  ```
  2PSp;PRINCIPAL;PE5;PF1;HAUT;PE4;1;PF2;I2C1:0xC0:4;I2C1:0xC0:5;LM211;DEFAUT;EEPROM;0;0
  ```
  EN = PE5, BLD = PF1, OC = PE4 (en interruption), PG = PF2,
  tension = I2C1 0xC0 voie 4, seuil de courant = I2C1 0xC0 voie 5.

### Vérification à la compilation

`generer_config.py` refuse le CSV et **arrête la compilation** (message dans la console de CubeIDE) si :
- un nom est invalide ou en double ;
- une broche est utilisée deux fois, ou prise par un périphérique du `.ioc` (Ethernet, I2C, UART, ADC, SWD) ;
- une puce DAC est inconnue du firmware, une voie hors 0-7 ou déjà utilisée ;
- deux OC en interruption partagent la même ligne EXTI (ex : PE4 et PG4), ou une OC en interruption est sur EXTI0 / EXTI13 (boutons).

Avertissements (compilation acceptée) : broche sur un bouton (PC13, PD0 : jamais reconfigurée),
DAC sur I2C2 (bus désactivé par défaut), protection sans DAC de courant.

### Rôle du `.ioc`

Le `.ioc` ne décrit plus que le système. Les broches des LDO sont configurées par
`LDO_GPIO_Init()` depuis `ldoConfig[]`, juste après `MX_GPIO_Init()` : EN en sortie à 0,
BLD en sortie au niveau inactif, OC et PG en entrée. Les anciennes broches LDO du `.ioc`
peuvent rester (elles sont reconfigurées ensuite) ; pour un projet propre, les remettre à
`Reset_State` dans CubeMX.

### Mettre à jour la carte

1. Modifier le CSV, directement ou depuis la page `/dac/config` du Controleur.
2. Double-cliquer sur `configurationLDO/mettre_a_jour.bat` (régler une fois les 3 chemins
   en tête : CubeIDE, CubeProgrammer, workspace ; CubeIDE fermé).
   Ou dans CubeIDE : Project > Build Project, puis Run.
3. Vérifier l'empreinte : `GET_CONFIG_LDO` répond `CONFIG_LDO:NB=18:CRC=0x...`, identique à celle
   affichée par `generer_config.py` et par `/dac/config` (« Carte à jour »).

Prérequis : `python` (3.8+) dans le PATH de Windows, sinon l'étape pre-build échoue.

## Démarrage

`main()` : horloges, MPU, caches, `MX_GPIO_Init()`, `LDO_GPIO_Init()`, ADC, I2C, UART, puis FreeRTOS.
`StartLwipTask()` :

1. LwIP, verrous I2C / UART / commandes, réception des commandes UART.
2. `LDO_InitialiserTables()` : mapping DAC et voies de multiplexeur depuis `ldoConfig[]`.
3. `OC_InitialiserSeuilsDefaut()` : seuil par défaut 0,50 A (code 1056) sur les LDO surveillées.
4. `InitialiserConfigDacsI2C1()` : VREF = VDD, sortie normale, gain 1x sur les DAC de I2C1.
5. `DAC_RestaurerMemoire()` : codes tension / seuil relus dans l'EEPROM des DAC (LDO `EEPROM`).
6. `CmdOnOff_Initialiser()` : `CMD_ON_OFF_BLD` / `CMD_ON_OFF_OC` depuis la flash, sinon le CSV.
7. `MX_OC_EXTI_Init()` : interruptions OC (LDO `oc_interruption = 1`).
8. Serveur UDP.

Tous les EN sont à 0 au démarrage.

## Boucle principale (~5 ms)

Pour chaque LDO : lecture OC, coupure éventuelle, écriture EN et BLD, lecture PG (`LDO_MettreAJour()`).
Puis : émission des trames DAC (I2C1, et I2C2 si actif), événements `OC_TRIP`, boutons,
surveillance de la configuration des DAC, sauvegardes EEPROM des codes DAC, sauvegarde flash
des commandes ON/OFF, commande UART en attente, battement de cœur `HB` (1 s).

## Protection surintensité (OC) et bleeder

Chaîne de mesure (carte corrigée, `CARTE_LIM_CORRIGEE = 1`) : shunt R97 100 mΩ, OP184 gain 26,1
(k = 2,61 V/A), LM211 avec hystérésis (~17,5 mA). OC = 1 quand le courant dépasse le seuil fixé
par le DAC de courant. Seuil réglable de 0,19 A (code 410) à 1,90 A (code 4006).

Avec `CMD_ON_OFF_OC = 1` et `CMD_ON_OFF_BLD = 1`, au dépassement :
1. coupure de l'EN (en interruption pour PE4 : quelques µs) ;
2. bleeder activé (`BL_LDO = 1`, même drapeau que la commande BL) ;
3. défaut mémorisé (`OCV:1` dans le statut, ligne `OC_TRIP` sur l'UART) ;
4. codes DAC tension et seuil sauvegardés en EEPROM (LDO `EEPROM`).

Réarmement : `RST_BL_LDO_<nom>` (refusé si OC est encore à 1), puis `SET_EN_LDO_<nom>`.
Masquage de 10 ms après chaque activation (appel de courant de démarrage).

### Commandes ON/OFF par LDO

| Variable | ON (1) | OFF (0) |
|---|---|---|
| `CMD_ON_OFF_OC` | Protection active (ci-dessus) | Broche OC ignorée : pas de coupure, pas de défaut ; un défaut mémorisé est effacé au passage à OFF |
| `CMD_ON_OFF_BLD` | Bleeder commandable | Bleeder relâché ; `SET_BL` / `RST_BL` répondent `BL:<nom>:REFUS_BL_OFF` |

- Avec le bleeder sur OFF, un dépassement coupe l'EN et mémorise le défaut sans bleeder ;
  réarmement par `RST_OC_LDO_<nom>` (refusé si OC est encore à 1).
- Valeur initiale : colonnes du CSV (0 partout à la livraison : **protections désactivées**).
- Chaque bascule est écrite en flash 0,5 s après (banque 2, secteur 7, `0x081E0000`, 128 Ko
  réservés dans le `.ld`, 2048 enregistrements avant effacement) et reprise au démarrage tant que
  l'empreinte de la configuration (`LDO_CONFIG_CRC32`) ne change pas. Si le CSV change, les valeurs
  du nouveau CSV s'appliquent.

## Mémoire EEPROM des DAC

Pour les LDO `memoire = EEPROM` (aujourd'hui `2PSp`) : les codes de tension et de seuil sont
écrits dans les registres non volatils du MCP47FEB28 (`10h` + voie), 2 s après la dernière
modification (une rampe n'use pas l'EEPROM) ou tout de suite après un déclenchement OC.
Écriture seulement si la valeur change, relecture de contrôle. Au démarrage, le DAC recharge ces
valeurs et le firmware les relit. Endurance : 1 000 000 d'écritures, tWC = 16 ms max.

## Defines de `main.h`

| Define | Valeur | Rôle |
|---|---|---|
| `CARTE_LIM_CORRIGEE` | 1 | Valeurs de la chaîne de mesure : 1 = carte corrigée (2,61 V/A), 0 = carte d'origine (10 V/A) |
| `OC_CODE_SEUIL_MIN` / `MAX` / `DEFAUT` | 410 / 4006 / 1056 | Seuil 0,19 A / 1,90 A / 0,50 A |
| `OC_BLANKING_MS` | 10 | Masquage OC après activation de l'EN |
| `OC_NIVEAU_DEFAUT` | `GPIO_PIN_SET` | Niveau de la broche OC en surintensité |
| `LIMITATION_PAR_POTENTIOMETRE` | 0 | 1 = seuil par potentiomètre (voie DAC de courant en haute impédance, `SET_CURRENT` refusé) |
| `DAC_EEPROM_TWC_MS` / `DAC_EEPROM_DELAI_MS` | 20 / 2000 | Attente d'écriture EEPROM / délai avant sauvegarde |

## Commandes (UDP et UART)

Une commande par datagramme UDP, ou par ligne sur l'UART (terminée par CR ou LF). Un verrou
commun garantit que l'UDP et l'UART ne traitent jamais deux commandes en même temps.
Réponse par défaut : `DONE`. `<nom>` = nom de la LDO dans le CSV (ex : `2PSp`).

### Par LDO

| Commande | Rôle / réponse |
|---|---|
| `GET_STATUS_LDO_<nom>` | `LDO_<nom>:: En:, Bl:, PG:, OC:, Voltage:, Current:, delta_ON:, delta_OFF:, OCV:, CMDBLD:, CMDOC:` |
| `SET_EN_LDO_<nom>` / `RST_EN_LDO_<nom>` | Active / coupe l'EN. Refus : `EN:<nom>:REFUS_OC_VERROUILLE`, `REFUS_BLEEDER_ACTIF`, `ATTENTE_SEUIL_OC` |
| `SET_BL_LDO_<nom>` / `RST_BL_LDO_<nom>` | Active / désactive le bleeder (désactiver = réarmer). `DONE:BL:<nom>:DEFAUT_OC_EFFACE`, `BL:<nom>:REFUS_OC_ACTIF`, `BL:<nom>:REFUS_BL_OFF` |
| `SET_VAL_LDO_<nom>:<code>` | Code 0-4095 du DAC de tension |
| `SET_CURRENT_LDO_<nom>:<code>` | Code du seuil de courant (410-4006). `CURRENT:<nom>:HORS_PLAGE_...` |
| `GET_OC_LDO_<nom>` | `OC_LDO:<nom>:NIV=:VERROU=:NB=:T_MS=:SEUIL=:SEUIL_VALIDE=:SOURCE=DAC\|POT` |
| `RST_OC_LDO_<nom>` | Efface le défaut mémorisé (`OC_RST:<nom>:OK` ou `REFUS_OC_ACTIF`) |
| `GET_DIAG_LDO_<nom>` | État réel des broches EN/BL/OC/PG, fronts EXTI, seuil |
| `GET_CMD_ON_OFF_BLD_LDO_<nom>` | `CMD_ON_OFF_BLD:<nom>:0\|1` |
| `GET_CMD_ON_OFF_OC_LDO_<nom>` | `CMD_ON_OFF_OC:<nom>:0\|1` |
| `SWITCH_CMD_ON_OFF_BLD_LDO_<nom>` | Bascule 0 ↔ 1, répond la nouvelle valeur (sauvegardée en flash) |
| `SWITCH_CMD_ON_OFF_OC_LDO_<nom>` | Bascule 0 ↔ 1, répond la nouvelle valeur (sauvegardée en flash) |
| `SET_DAC_MAPPING_LDO_<nom>:<V\|I>:<0-5>:<0-7>` / `GET_DAC_MAPPING_LDO_<nom>` | Mapping DAC à chaud (non sauvegardé) |
| `SET_MUX_LDO_<nom>:<canal>` / `GET_MUX_LDO_<nom>` | Canal du multiplexeur de mesure de tension |
| `SET_MUX_COURANT_LDO_<nom>:<canal>` / `GET_MUX_COURANT_LDO_<nom>` | Canal du multiplexeur de mesure de courant |
| `GET_MESURE_REELLE_<nom>` / `GET_MESURE_COURANT_REELLE_<nom>` | Mesure ADC de la tension / du courant |

### Globales

| Commande | Rôle |
|---|---|
| `WHO` | Identification (`STM32 NULCEO H753ZI IRIS V1.0`) |
| `GET_CONFIG_LDO` | `CONFIG_LDO:NB=<n>:CRC=0x<empreinte>` de la configuration compilée |
| `SET_EN_LDO_ALL` / `RST_EN_LDO_ALL` | Toutes les LDO (sauf défaut mémorisé) |
| `SET_BL_LDO_ALL` / `RST_BL_LDO_ALL` | Active / désactive tous les bleeders (LDO avec `CMD_ON_OFF_BLD` = 1) |
| `SET_EN_TAB[v0,...,vN]` / `SET_BL_TAB[v0,...,vN]` | Une valeur 0/1 par LDO, dans l'ordre du CSV |
| `SET_DELTA_<A>_<B>:<ms>` / `RST_DELTA_<A>_<B>` | Délais de séquencement (DVDD, AVDD, VREF, NRST) |
| `SCAN_I2C1` / `SCAN_I2C2` | Scan du bus |
| `BALAYER_DAC:...` | Balayage d'une sortie DAC (bloquant) |
| `GET_DAC_CONFIG`, `GET_DAC_REG:<bus>:<adr>:<reg>` | Relecture des registres DAC |
| `SET_DAC_VREF:<bus>:<adr>:<valeur>`, `SET_DAC_PD:...`, `SET_DAC_GAIN:...` | Registres de configuration d'une puce DAC |
| `SET_I2C2_ACTIF:<0\|1>` | Active / désactive l'émission sur I2C2 |
| `SET_MUX_CANAL_TENSION` / `GET_MUX_CANAL_TENSION`, `..._COURANT` | Canal brut des multiplexeurs |
| `GET_ADC_BRUT`, `GET_CURRENT_1`, `GET_CURRENT_2` | Lectures ADC de diagnostic |

### Exemple sur l'UART (terminal 115200 8N1)

```
GET_CMD_ON_OFF_OC_LDO_2PSp
CMD_ON_OFF_OC:2PSp:0
SWITCH_CMD_ON_OFF_OC_LDO_2PSp
CMD_ON_OFF_OC:2PSp:1
```

## Lignes émises sur l'UART

| Ligne | Signification |
|---|---|
| `HB` | Battement de cœur (1 s) |
| `[UDP] RX commande: ...` / `[UDP] REPONSE: ...` | Trace des commandes UDP (sauf `WHO`) |
| `DAC_INIT:0xC0:VREF=...:OK` | Configuration des DAC au démarrage |
| `DAC_NV_INIT:2PSp:V=..:I=..:OK` | Codes DAC relus dans l'EEPROM au démarrage |
| `CMD_ON_OFF:FLASH:SEQ=..` / `CMD_ON_OFF:CSV ...` | Origine des commandes ON/OFF au démarrage |
| `CMD_ON_OFF:SAUVE:SEQ=..:OK` | Commandes ON/OFF écrites en flash |
| `OC_TRIP:<nom>:NB=..:T_MS=..:SEUIL=..` | Défaut de surintensité mémorisé |
| `DAC_NV:<nom>:V=..:I=..:OK` | Codes DAC écrits en EEPROM |
| `DAC_RECONFIG:0xC0:...` | Configuration d'un DAC perdue puis restaurée |
| `=== SAVE_BEGIN ... ===` ... `=== SAVE_END ... ===` | Capture du bouton USER |

## Points d'attention

- **Protections à la livraison** : `CMD_ON_OFF_OC = 0` et `CMD_ON_OFF_BLD = 0` pour toutes les LDO.
  Pour utiliser la limitation sur PS2+, passer OC (et BL) sur ON depuis `/dac` ou par `SWITCH_CMD_ON_OFF_*`.
- **Effacement du secteur flash** : environ une fois toutes les 2048 bascules ON/OFF, la boucle est
  bloquée jusqu'à ~2 s ; les LDO dont l'OC est en interruption restent protégées.
- **Coupure pendant une écriture flash** : l'enregistrement est rejeté par son CRC. Risque théorique
  d'erreur ECC à la relecture d'un mot flash à moitié écrit.
- **Broches sur boutons** dans la configuration actuelle : LDO 5 (`5PSp`) BLD = PC13, LDO 13 (`13PSp`)
  PG = PD0 : ces fonctions ne marchent pas.
- **Interruptions OC** : 14 lignes EXTI disponibles au maximum (0 et 13 prises par les boutons),
  une seule OC par numéro de broche. Les autres sont surveillées par la boucle (~5 ms).
- **I2C2** désactivé par défaut : les LDO dont les DAC sont sur I2C2 ne reçoivent aucune consigne
  tant que `SET_I2C2_ACTIF:1` n'est pas envoyé.

## Historique des révisions

### Révision 1.0.0 - 22/10/2026
- Configuration des LDO dans `configurationLDO/ldo_config.csv`, vérifiée et générée à chaque compilation ; `.ioc` réservé au système, broches LDO configurées par `LDO_GPIO_Init()`.
- 18 LDO : `1PSp` à `16PSp` + `2PLn`, `3PLn` mises de côté ; `2PSp` (PS2+) inchangée.
- Protection OC : défaut mémorisé, bleeder = drapeau BL, réarmement par `RST_BL_LDO_`, coupure en interruption sur PE4.
- Codes DAC de `2PSp` gardés dans l'EEPROM du MCP47FEB28.
- Commandes `CMD_ON_OFF_BLD` / `CMD_ON_OFF_OC` par LDO, sauvegardées en flash interne.
- Toutes les commandes acceptées aussi sur l'UART ; `GET_CONFIG_LDO` ; statut étendu (`OCV`, `CMDBLD`, `CMDOC`).
- Mode potentiomètre (`LIMITATION_PAR_POTENTIOMETRE`).
