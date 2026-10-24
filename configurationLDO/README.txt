CONFIGURATION DES LDO (dossier configurationLDO)
================================================

Ce dossier est le SEUL endroit ou se decrit le cablage des LDO (rails
d'alimentation). Le code C n'est pas a modifier.

Fichiers
  ldo_config.csv      configuration : une ligne par LDO (seul fichier a modifier)
  ldo_config_csv.py   lecture / verification / generation (utilise aussi par le controleur)
  generer_config.py   lance avant chaque compilation par CubeIDE
  mettre_a_jour.bat   verifier + compiler + flasher en un double-clic
  README.txt          ce fichier

Modifier la configuration
  1) Soit dans le controleur : page /dac/config, tableau "Configuration des LDO",
     bouton Enregistrer (le controleur verifie avant d'ecrire le CSV).
     Soit directement dans ldo_config.csv (Excel : separateur point-virgule).
  2) Double-clic sur mettre_a_jour.bat (regler une fois les 3 chemins en tete),
     ou dans CubeIDE : Project > Build Project puis Run (flash).
  3) Page /dac/config : le bandeau doit indiquer "Carte a jour" (l'empreinte
     de la carte, commande GET_CONFIG_LDO, est celle du CSV).

Si le CSV est faux (broche en double, broche Ethernet/I2C/UART du .ioc, voie
DAC deja prise, deux OC en interruption sur la meme ligne EXTI...), la
compilation s'arrete avec le message : rien de faux ne peut etre flashe.

Colonnes
  rang             Numero de la LDO (1, 2, 3...). Renumerote automatiquement : l'ordre des lignes fait foi.
  nom              Nom des commandes : SET_VAL_LDO_<nom>, SET_EN_LDO_<nom>... (lettres et chiffres, 12 max).
  affichage        PRINCIPAL = affichee dans /dac ; DE_COTE = mise de cote (toujours geree par la carte).
  en               Broche Enable (sortie), ex : PE5.
  bld              Broche Bleeder (sortie), ex : PF1.
  bld_actif        Niveau qui fait conduire le bleeder : HAUT ou BAS.
  oc               Broche Over-Current (entree, sortie du LM211), ex : PE4.
  oc_interruption  1 = coupure de l'EN en interruption (quelques us) ; 0 = par la boucle (~5 ms).
  pg               Broche Power-Good (entree), ex : PF2.
  dac_tension      DAC de tension : BUS:ADRESSE:VOIE (ex : I2C1:0xC0:4), ou - si aucun.
  dac_courant      DAC du seuil de courant : BUS:ADRESSE:VOIE (ex : I2C1:0xC0:5), ou - si aucun.
  protection       LM211 = circuit shunt + OP184 + LM211 cable (OC surveille) ; SANS = OC affiche seulement.
  seuil            Source du seuil : DEFAUT (define LIMITATION_PAR_POTENTIOMETRE), DAC ou POT.
  memoire          EEPROM = codes DAC gardes a la coupure (EEPROM du MCP47FEB28) ; RAM = 0 au demarrage.
  CMD_ON_OFF_BLD   Bleeder au premier demarrage : 1 = ON (commandable), 0 = OFF (ignore, gris). Ensuite la valeur des switchs de /dac est gardee en flash du STM32.
  CMD_ON_OFF_OC    Protection OC au premier demarrage : 1 = ON (coupure sur surintensite), 0 = OFF (OC ignore, gris). Ensuite la valeur des switchs de /dac est gardee en flash du STM32.
  commentaire      Texte libre (sans point-virgule).

Formats
  Broche     : PA0 a PG15 (ex : PE5).
  DAC        : BUS:ADRESSE:VOIE, ex : I2C1:0xC0:4. Puces connues du firmware :
               I2C1:0xC0, I2C2:0xC0, I2C1:0xC2, I2C2:0xC4, I2C2:0xC2. "-" = aucun.

Regles
  - L'ordre des lignes = index des LDO (SET_EN_TAB, SET_BL_TAB, IHM).
  - Une broche ne sert qu'une fois. Les broches des peripheriques du .ioc sont interdites.
  - PC13 (bouton USER) et PD0 (bouton reset ADG728) : acceptees avec avertissement,
    mais jamais reconfigurees (la fonction de la LDO sur cette broche ne marche pas).
  - OC en interruption : une seule par numero de broche (PE4 et PG4 = EXTI4),
    jamais sur les numeros 0 et 13 (boutons). Les autres sont surveillees par la boucle.
  - memoire EEPROM : ne la mettre qu'apres avoir verifie l'adresse et la voie
    des DAC de la LDO (sinon on ecrit l'EEPROM d'une autre sortie).

Le .ioc (CubeMX)
  Il ne sert plus qu'au systeme (horloges, Ethernet, I2C, UART, ADC, FreeRTOS).
  Les broches des LDO sont configurees par LDO_GPIO_Init() depuis le CSV, apres
  MX_GPIO_Init(). Les anciennes broches LDO du .ioc peuvent rester : elles sont
  reconfigurees ensuite. Pour un projet propre, les remettre a "Reset_State"
  dans CubeMX une fois (pas obligatoire).

Commandes ON/OFF (CMD_ON_OFF_BLD, CMD_ON_OFF_OC), en UDP et sur l'UART (115200 8N1, une commande par ligne)
  GET_CMD_ON_OFF_BLD_LDO_<nom>      -> CMD_ON_OFF_BLD:<nom>:0 ou 1
  GET_CMD_ON_OFF_OC_LDO_<nom>       -> CMD_ON_OFF_OC:<nom>:0 ou 1
  SWITCH_CMD_ON_OFF_BLD_LDO_<nom>   -> bascule 0 <-> 1, repond la nouvelle valeur
  SWITCH_CMD_ON_OFF_OC_LDO_<nom>    -> bascule 0 <-> 1, repond la nouvelle valeur
  Les colonnes du CSV ne servent qu'au premier demarrage. Ensuite la carte garde
  la valeur des switchs en flash (banque 2, secteur 7). Si le CSV change
  (nouvelle empreinte), les valeurs du nouveau CSV s'appliquent.
  OC sur OFF : broche OC ignoree (pas de coupure, pas de defaut).
  BL sur OFF : bleeder relache et non commandable ; apres une surintensite,
  rearmer en cliquant sur la LED OC (RST_OC_LDO_<nom>).
