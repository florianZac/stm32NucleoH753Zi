# -*- coding: utf-8 -*-
"""
@file    ldo_config_csv.py
@brief   Lecture, verification, ecriture et generation C du fichier de
         configuration des LDO (configurationLDO/ldo_config.csv).
@par     Utilise par :
         - generer_config.py (firmware, etape "pre-build" de CubeIDE) :
           genere Core/Inc/ldo_config_genere.h et Core/Inc/ldo_config_lignes.inc ;
         - le controleur Python (page /dac/config) : lecture, verification et
           enregistrement du CSV, liste des LDO de l'IHM.
         Les deux utilisent CE fichier : une seule regle de verification.
@par     Dependances : bibliotheque standard Python 3.8+ uniquement.
@author  Florian Aizac
@date    22/10/2026
"""

import csv
import io
import os
import re
import zlib

# ---------------------------------------------------------------------------
# Format du fichier
# ---------------------------------------------------------------------------
SEPARATEUR = ";"
NB_LDO_MAX = 24

#: Colonnes, dans l'ordre du fichier.
COLONNES = [
    "rang", "nom", "affichage",
    "en", "bld", "bld_actif", "oc", "oc_interruption", "pg",
    "dac_tension", "dac_courant",
    "protection", "seuil", "memoire", "CMD_ON_OFF_BLD", "CMD_ON_OFF_OC", "commentaire",
]

#: Colonnes facultatives (absentes d'un ancien CSV : valeur par defaut).
COLONNES_FACULTATIVES = ("rang", "CMD_ON_OFF_BLD", "CMD_ON_OFF_OC", "commentaire")

#: Explication de chaque colonne (en-tete du CSV, README, infobulles de l'IHM).
DESCRIPTION_COLONNES = {
    "rang": "Numero de la LDO (1, 2, 3...). Renumerote automatiquement : l'ordre des lignes fait foi.",
    "nom": "Nom des commandes : SET_VAL_LDO_<nom>, SET_EN_LDO_<nom>... (lettres et chiffres, 12 max).",
    "affichage": "PRINCIPAL = affichee dans /dac ; DE_COTE = mise de cote (toujours geree par la carte).",
    "en": "Broche Enable (sortie), ex : PE5.",
    "bld": "Broche Bleeder (sortie), ex : PF1.",
    "bld_actif": "Niveau qui fait conduire le bleeder : HAUT ou BAS.",
    "oc": "Broche Over-Current (entree, sortie du LM211), ex : PE4.",
    "oc_interruption": "1 = coupure de l'EN en interruption (quelques us) ; 0 = par la boucle (~5 ms).",
    "pg": "Broche Power-Good (entree), ex : PF2.",
    "dac_tension": "DAC de tension : BUS:ADRESSE:VOIE (ex : I2C1:0xC0:4), ou - si aucun.",
    "dac_courant": "DAC du seuil de courant : BUS:ADRESSE:VOIE (ex : I2C1:0xC0:5), ou - si aucun.",
    "protection": "LM211 = circuit shunt + OP184 + LM211 cable (OC surveille) ; SANS = OC affiche seulement.",
    "seuil": "Source du seuil : DEFAUT (define LIMITATION_PAR_POTENTIOMETRE), DAC ou POT.",
    "memoire": "EEPROM = codes DAC gardes a la coupure (EEPROM du MCP47FEB28) ; RAM = 0 au demarrage.",
    "CMD_ON_OFF_BLD": "Bleeder au premier demarrage : 1 = ON (commandable), 0 = OFF (ignore, gris). "
                      "Ensuite la valeur des switchs de /dac est gardee en flash du STM32.",
    "CMD_ON_OFF_OC": "Protection OC au premier demarrage : 1 = ON (coupure sur surintensite), 0 = OFF "
                     "(OC ignore, gris). Ensuite la valeur des switchs de /dac est gardee en flash du STM32.",
    "commentaire": "Texte libre (sans point-virgule).",
}

#: Puces DAC connues du firmware (tableDac[] dans main.c) : (bus, adresse HAL) -> cible C.
DACS_AUTORISES = {
    ("I2C1", 0xC0): "CIBLE_DAC1",
    ("I2C2", 0xC0): "CIBLE_DAC2",
    ("I2C1", 0xC2): "CIBLE_DAC3",
    ("I2C2", 0xC4): "CIBLE_DAC4",
    ("I2C2", 0xC2): "CIBLE_DAC5",
}

#: Broches prises par la carte si le .ioc n'est pas trouve (Nucleo-H753ZI, projet du 22/10/2026).
BROCHES_RESERVEES_DEFAUT = {
    "PA1": "ETH_REF_CLK", "PA2": "ETH_MDIO", "PA7": "ETH_CRS_DV", "PB13": "ETH_TXD1",
    "PC1": "ETH_MDC", "PC4": "ETH_RXD0", "PC5": "ETH_RXD1", "PG11": "ETH_TX_EN", "PG13": "ETH_TXD0",
    "PB8": "I2C1_SCL", "PB9": "I2C1_SDA", "PB10": "I2C2_SCL", "PB11": "I2C2_SDA",
    "PB6": "USART1_TX", "PB7": "USART1_RX", "PA13": "SWDIO", "PA14": "SWCLK",
    "PC0": "ADC", "PF4": "ADC3_INP9", "PF10": "ADC3_INP6",
}

#: Broches utilisees par des boutons du firmware (EXTI) : avertissement, jamais reconfigurees.
BROCHES_BOUTONS = {"PC13": "bouton USER B1 (EXTI13)", "PD0": "bouton reset ADG728 (EXTI0)"}

#: Lignes EXTI deja prises par les boutons : une OC en interruption ne peut pas les utiliser.
LIGNES_EXTI_RESERVEES = {0, 13}

_RE_NOM = re.compile(r"^[0-9A-Za-z]{1,12}$")
_RE_BROCHE = re.compile(r"^P([A-H])(\d{1,2})$")
_RE_DAC = re.compile(r"^(?:(I2C[12]):)?(0X[0-9A-F]{2}):([0-9]+)$")


class Ligne(dict):
    """Une LDO du CSV (dictionnaire colonne -> texte normalise) + numero de ligne du fichier."""
    numero_fichier = 0


# ---------------------------------------------------------------------------
# Normalisation
# ---------------------------------------------------------------------------
def normaliser_broche(texte):
    """
    @brief  Met une broche au format PE5 (majuscules, sans espace).
    @param  texte : saisie (ex : " pe5 ").
    @retval Broche normalisee, ou la saisie en majuscules si invalide.
    @par Exemple d'utilisation
         normaliser_broche("pe5")  # -> "PE5"
    """
    return str(texte or "").strip().upper().replace(" ", "")


def normaliser_dac(texte):
    """
    @brief  Met un champ DAC au format I2C1:0xC0:4 ("-" si vide).
    @param  texte : saisie (ex : "0XC0:4", "i2c2:0xc2:3", "-").
    @retval Champ normalise ; un bus absent vaut I2C1. Saisie inchangee si invalide.
    @par Exemple d'utilisation
         normaliser_dac("0XC0:4")  # -> "I2C1:0xC0:4"
    """
    t = str(texte or "").strip().upper().replace(" ", "")
    if t in ("", "-", "AUCUN", "NONE"):
        return "-"
    m = _RE_DAC.match(t)
    if not m:
        return t
    bus = m.group(1) or "I2C1"
    return "%s:0x%02X:%d" % (bus, int(m.group(2), 16), int(m.group(3)))


def normaliser_ligne(brut):
    """
    @brief  Normalise toutes les colonnes d'une LDO (majuscules, valeurs par defaut).
    @param  brut : dict colonne -> texte (colonnes absentes acceptees).
    @retval Ligne normalisee.
    @par Exemple d'utilisation
         l = normaliser_ligne({"nom": "2PSp", "en": "pe5", ...})
    """
    l = Ligne()
    for c in COLONNES:
        l[c] = str(brut.get(c, "") if brut.get(c, "") is not None else "").strip()
    l["nom"] = l["nom"]
    l["affichage"] = (l["affichage"] or "PRINCIPAL").upper()
    for c in ("en", "bld", "oc", "pg"):
        l[c] = normaliser_broche(l[c])
    l["bld_actif"] = (l["bld_actif"] or "HAUT").upper()
    l["oc_interruption"] = (l["oc_interruption"] or "0").strip()
    l["dac_tension"] = normaliser_dac(l["dac_tension"])
    l["dac_courant"] = normaliser_dac(l["dac_courant"])
    l["protection"] = (l["protection"] or "SANS").upper()
    l["seuil"] = (l["seuil"] or "DEFAUT").upper()
    l["memoire"] = (l["memoire"] or "RAM").upper()
    for c in ("CMD_ON_OFF_BLD", "CMD_ON_OFF_OC"):
        l[c] = {"ON": "1", "OFF": "0"}.get(l[c].upper(), l[c]) or "0"
    l["commentaire"] = l["commentaire"].replace(SEPARATEUR, ",")
    return l


# ---------------------------------------------------------------------------
# Lecture / ecriture
# ---------------------------------------------------------------------------
def lire_texte(texte):
    """
    @brief  Lit le contenu d'un CSV (lignes # = commentaires).
    @param  texte : contenu du fichier.
    @retval (lignes, erreurs) : liste de Ligne normalisees, liste de messages
            d'erreur de format (colonnes manquantes...).
    @par Exemple d'utilisation
         lignes, erreurs = lire_texte(open("ldo_config.csv", encoding="utf-8").read())
    """
    erreurs = []
    utiles = []
    for numero, brute in enumerate(texte.splitlines(), start=1):
        if brute.strip() and not brute.lstrip().startswith("#"):
            utiles.append((numero, brute))
    if not utiles:
        return [], ["Fichier vide : aucune ligne d'en-tete."]
    canonique = {c.lower(): c for c in COLONNES}
    entete = [canonique.get(c.strip().lower(), c.strip().lower()) for c in utiles[0][1].split(SEPARATEUR)]
    manquantes = [c for c in COLONNES if c not in entete and c not in COLONNES_FACULTATIVES]
    if manquantes:
        return [], ["Ligne %d : colonnes manquantes dans l'en-tete : %s" % (utiles[0][0], ", ".join(manquantes))]
    lignes = []
    for numero, brute in utiles[1:]:
        valeurs = next(csv.reader([brute], delimiter=SEPARATEUR))
        if len(valeurs) > len(entete):
            erreurs.append("Ligne %d : %d colonnes au lieu de %d (point-virgule en trop ?)" % (numero, len(valeurs), len(entete)))
            continue
        l = normaliser_ligne(dict(zip(entete, valeurs)))
        l.numero_fichier = numero
        lignes.append(l)
    return lignes, erreurs


def lire_fichier(chemin):
    """
    @brief  Lit le CSV sur disque (UTF-8, BOM accepte).
    @param  chemin : chemin du fichier ldo_config.csv.
    @retval (lignes, erreurs), voir lire_texte().
    @par Exemple d'utilisation
         lignes, erreurs = lire_fichier("configurationLDO/ldo_config.csv")
    """
    with open(chemin, "r", encoding="utf-8-sig") as f:
        return lire_texte(f.read())


def texte_csv(lignes):
    """
    @brief  Produit le texte complet du CSV (en-tete explicatif + lignes renumerotees).
    @param  lignes : liste de Ligne (ou de dict).
    @retval Texte du fichier.
    @par Exemple d'utilisation
         open("ldo_config.csv", "w", encoding="utf-8").write(texte_csv(lignes))
    """
    sortie = io.StringIO()
    sortie.write("# =============================================================================\n")
    sortie.write("#  CONFIGURATION DES LDO - UNE LIGNE PAR RAIL D'ALIMENTATION\n")
    sortie.write("#  Seul fichier a modifier (ou page /dac/config du controleur).\n")
    sortie.write("#  Separateur : point-virgule. Lignes commencant par # : commentaires.\n")
    sortie.write("#  L'ORDRE DES LIGNES = index des LDO (SET_EN_TAB, SET_BL_TAB, IHM).\n")
    sortie.write("#  Apres modification : mettre_a_jour.bat (ou compiler dans CubeIDE puis flasher).\n")
    sortie.write("#\n")
    for c in COLONNES:
        sortie.write("#  %-16s %s\n" % (c, DESCRIPTION_COLONNES[c]))
    sortie.write("# =============================================================================\n")
    sortie.write(SEPARATEUR.join(COLONNES) + "\n")
    for rang, brut in enumerate(lignes, start=1):
        l = normaliser_ligne(brut)
        l["rang"] = str(rang)
        sortie.write(SEPARATEUR.join(l[c] for c in COLONNES) + "\n")
    return sortie.getvalue()


def ecrire_fichier(chemin, lignes):
    """
    @brief  Ecrit le CSV sur disque sans risque de fichier tronque (fichier
            temporaire puis remplacement).
    @param  chemin : chemin du fichier ldo_config.csv.
    @param  lignes : liste de Ligne.
    @retval Aucun.
    @par Exemple d'utilisation
         ecrire_fichier("configurationLDO/ldo_config.csv", lignes)
    """
    temporaire = chemin + ".tmp"
    with open(temporaire, "w", encoding="utf-8", newline="\n") as f:
        f.write(texte_csv(lignes))
    os.replace(temporaire, chemin)


# ---------------------------------------------------------------------------
# Broches reservees (lecture du .ioc de CubeMX)
# ---------------------------------------------------------------------------
_RE_IOC = re.compile(r"^(P[A-K]\d{1,2})(?:[-\\ ][^.=]*)?\.Signal=(.+)$")


def broches_reservees_ioc(chemin_ioc):
    """
    @brief  Liste les broches attribuees a un peripherique dans le .ioc
            (Ethernet, I2C, UART, ADC, SWD...). Les broches GPIO (anciennes
            broches LDO) ne sont pas reservees. Le .ioc n'est jamais modifie.
    @param  chemin_ioc : chemin du fichier .ioc, ou None.
    @retval dict broche -> signal ; BROCHES_RESERVEES_DEFAUT si le .ioc est absent.
    @par Exemple d'utilisation
         r = broches_reservees_ioc("../Lwip_demo_H753.ioc")  # {"PB8": "I2C1_SCL", ...}
    """
    if not chemin_ioc or not os.path.isfile(chemin_ioc):
        return dict(BROCHES_RESERVEES_DEFAUT)
    reservees = {}
    with open(chemin_ioc, "r", encoding="utf-8", errors="replace") as f:
        for brute in f:
            m = _RE_IOC.match(brute.strip())
            if m and not m.group(2).startswith("GPIO_"):
                reservees[m.group(1)] = m.group(2)
    return reservees


# ---------------------------------------------------------------------------
# Verification
# ---------------------------------------------------------------------------
def _decoder_dac(champ):
    """(bus, adresse, voie) d'un champ DAC normalise, ou None pour '-'. Leve ValueError si invalide."""
    if champ == "-":
        return None
    m = _RE_DAC.match(champ.upper())
    if not m:
        raise ValueError("format attendu BUS:ADRESSE:VOIE, ex : I2C1:0xC0:4")
    bus, adresse, voie = (m.group(1) or "I2C1"), int(m.group(2), 16), int(m.group(3))
    if (bus, adresse) not in DACS_AUTORISES:
        connus = ", ".join("%s:0x%02X" % k for k in DACS_AUTORISES)
        raise ValueError("puce %s:0x%02X inconnue du firmware (connues : %s)" % (bus, adresse, connus))
    if not 0 <= voie <= 7:
        raise ValueError("voie %d hors 0-7" % voie)
    return bus, adresse, voie


def verifier(lignes, chemin_ioc=None):
    """
    @brief  Verifie une configuration complete : noms, broches (doublons,
            peripheriques du .ioc, boutons), DAC (puce connue, voie 0-7, pas de
            doublon), interruptions OC (une ligne EXTI par OC), coherences.
    @param  lignes     : liste de Ligne normalisees.
    @param  chemin_ioc : .ioc du projet (broches reservees), ou None (liste par defaut).
    @retval (erreurs, avertissements) : listes de messages. Erreur = la
            configuration est refusee (pas de generation, pas d'enregistrement).
    @par Exemple d'utilisation
         erreurs, avert = verifier(lignes, "../Lwip_demo_H753.ioc")
         if erreurs: print("\\n".join(erreurs))
    """
    erreurs, avert = [], []
    if not lignes:
        return ["Aucune LDO dans la configuration."], []
    if len(lignes) > NB_LDO_MAX:
        erreurs.append("%d LDO : maximum %d." % (len(lignes), NB_LDO_MAX))

    reservees = broches_reservees_ioc(chemin_ioc)
    if not chemin_ioc or not os.path.isfile(chemin_ioc):
        avert.append("Fichier .ioc non trouve : broches reservees verifiees avec la liste par defaut.")

    noms, broches, voies_dac, lignes_exti = {}, {}, {}, {}
    sur_i2c2 = []
    for rang, l in enumerate(lignes, start=1):
        ref = "LDO %d (%s)" % (rang, l["nom"] or "sans nom")

        # Nom
        if not _RE_NOM.match(l["nom"]):
            erreurs.append("%s : nom invalide (lettres et chiffres, 1 a 12 caracteres)." % ref)
        elif l["nom"].upper() in noms:
            erreurs.append("%s : nom deja utilise par la LDO %d." % (ref, noms[l["nom"].upper()]))
        else:
            noms[l["nom"].upper()] = rang
        if l["nom"].upper() in ("ALL", "TAB"):
            erreurs.append("%s : nom reserve par les commandes *_ALL / *_TAB." % ref)

        if l["affichage"] not in ("PRINCIPAL", "DE_COTE"):
            erreurs.append("%s : affichage doit etre PRINCIPAL ou DE_COTE." % ref)

        # Broches
        for champ in ("en", "bld", "oc", "pg"):
            b = l[champ]
            m = _RE_BROCHE.match(b)
            if not m or int(m.group(2)) > 15 or (m.group(1) == "H" and int(m.group(2)) > 1):
                erreurs.append("%s : broche %s '%s' invalide (PA0 a PG15, PH0, PH1)." % (ref, champ.upper(), b))
                continue
            if b in broches:
                erreurs.append("%s : broche %s %s deja utilisee (%s)." % (ref, champ.upper(), b, broches[b]))
            else:
                broches[b] = "%s %s" % (ref, champ.upper())
            if b in reservees:
                erreurs.append("%s : broche %s %s prise par %s (.ioc)." % (ref, champ.upper(), b, reservees[b]))
            if b in BROCHES_BOUTONS:
                avert.append("%s : broche %s %s = %s : elle reste au bouton, la fonction %s ne marche pas."
                             % (ref, champ.upper(), b, BROCHES_BOUTONS[b], champ.upper()))

        if l["bld_actif"] not in ("HAUT", "BAS"):
            erreurs.append("%s : bld_actif doit etre HAUT ou BAS." % ref)

        # DAC
        dacs = {}
        for champ in ("dac_tension", "dac_courant"):
            try:
                dacs[champ] = _decoder_dac(l[champ])
            except ValueError as e:
                erreurs.append("%s : %s : %s." % (ref, champ, e))
                dacs[champ] = None
                continue
            if dacs[champ] is not None:
                if dacs[champ] in voies_dac:
                    erreurs.append("%s : %s %s deja utilise (%s)." % (ref, champ, l[champ], voies_dac[dacs[champ]]))
                else:
                    voies_dac[dacs[champ]] = "%s %s" % (ref, champ)
                if dacs[champ][0] == "I2C2" and l["nom"] not in sur_i2c2:
                    sur_i2c2.append(l["nom"])

        # Protection / interruption / seuil / memoire
        if l["protection"] not in ("LM211", "SANS"):
            erreurs.append("%s : protection doit etre LM211 ou SANS." % ref)
        if l["oc_interruption"] not in ("0", "1"):
            erreurs.append("%s : oc_interruption doit etre 0 ou 1." % ref)
        if l["seuil"] not in ("DEFAUT", "DAC", "POT"):
            erreurs.append("%s : seuil doit etre DEFAUT, DAC ou POT." % ref)
        if l["memoire"] not in ("RAM", "EEPROM"):
            erreurs.append("%s : memoire doit etre RAM ou EEPROM." % ref)
        for c in ("CMD_ON_OFF_BLD", "CMD_ON_OFF_OC"):
            if l[c] not in ("0", "1"):
                erreurs.append("%s : %s doit etre 0 (OFF) ou 1 (ON)." % (ref, c))
        if l["protection"] == "LM211" and dacs.get("dac_courant") is None and l["seuil"] != "POT":
            avert.append("%s : protection LM211 sans DAC de courant : OC affiche mais pas surveille." % ref)
        if l["oc_interruption"] == "1":
            if l["protection"] != "LM211":
                erreurs.append("%s : oc_interruption = 1 demande protection = LM211." % ref)
            m = _RE_BROCHE.match(l["oc"])
            if m:
                n = int(m.group(2))
                if n in LIGNES_EXTI_RESERVEES:
                    erreurs.append("%s : OC %s en interruption impossible : ligne EXTI%d prise par un bouton." % (ref, l["oc"], n))
                elif n in lignes_exti:
                    erreurs.append("%s : OC %s et %s partagent la ligne EXTI%d : une seule en interruption."
                                   % (ref, l["oc"], lignes_exti[n], n))
                else:
                    lignes_exti[n] = "%s (%s)" % (l["oc"], ref)
        if l["memoire"] == "EEPROM" and dacs.get("dac_tension") is None and dacs.get("dac_courant") is None:
            avert.append("%s : memoire EEPROM sans DAC : rien a sauvegarder." % ref)
    if sur_i2c2:
        avert.append("DAC sur I2C2 (bus desactive par defaut, SET_I2C2_ACTIF:1) : %s." % ", ".join(sur_i2c2))
    return erreurs, avert


# ---------------------------------------------------------------------------
# Empreinte (comparaison CSV / firmware flashe)
# ---------------------------------------------------------------------------
def empreinte(lignes):
    """
    @brief  CRC32 de la configuration (sans rang ni commentaire). Le firmware
            la renvoie (GET_CONFIG_LDO) : si elle differe de celle du CSV, la
            carte n'a pas ete reflashee avec ce fichier.
    @param  lignes : liste de Ligne normalisees.
    @retval Entier 32 bits.
    @par Exemple d'utilisation
         "0x%08X" % empreinte(lignes)
    """
    champs = [c for c in COLONNES if c not in ("rang", "commentaire")]
    texte = "\n".join(SEPARATEUR.join(normaliser_ligne(l)[c] for c in champs) for l in lignes)
    return zlib.crc32(texte.encode("utf-8")) & 0xFFFFFFFF


# ---------------------------------------------------------------------------
# Generation C
# ---------------------------------------------------------------------------
def _broche_c(b):
    m = _RE_BROCHE.match(b)
    return "GPIO%s, GPIO_PIN_%d" % (m.group(1), int(m.group(2)))


def _dac_c(champ):
    d = _decoder_dac(champ)
    if d is None:
        return "CIBLE_AUCUNE, 0"
    return "%s, %d" % (DACS_AUTORISES[(d[0], d[1])], d[2])


def generer_c(lignes, nom_csv="ldo_config.csv"):
    """
    @brief  Produit les deux fichiers C a partir d'une configuration verifiee.
    @param  lignes  : liste de Ligne verifiees (verifier() sans erreur).
    @param  nom_csv : nom du CSV source, cite dans les commentaires.
    @retval (texte_h, texte_inc) : ldo_config_genere.h (LDO_NB, empreinte) et
            ldo_config_lignes.inc (lignes de ldoConfig[]).
    @par Exemple d'utilisation
         h, inc = generer_c(lignes)
    """
    crc = empreinte(lignes)
    h = io.StringIO()
    h.write("/* FICHIER GENERE par configurationLDO/generer_config.py depuis %s.\n" % nom_csv)
    h.write(" * NE PAS MODIFIER : modifier le CSV (ou la page /dac/config) puis recompiler. */\n")
    h.write("#ifndef LDO_CONFIG_GENERE_H\n#define LDO_CONFIG_GENERE_H\n\n")
    h.write("#define LDO_NB              %dU\n" % len(lignes))
    h.write("#define LDO_CONFIG_CRC32    0x%08XUL\n\n" % crc)
    h.write("#endif /* LDO_CONFIG_GENERE_H */\n")

    inc = io.StringIO()
    inc.write("/* FICHIER GENERE par configurationLDO/generer_config.py depuis %s.\n" % nom_csv)
    inc.write(" * NE PAS MODIFIER : modifier le CSV (ou la page /dac/config) puis recompiler.\n")
    inc.write(" * Inclus dans l'initialiseur de ldoConfig[] (main.c). Empreinte 0x%08X. */\n" % crc)
    for i, l in enumerate(lignes):
        oc = ("OC_LM211(%sU)" % l["oc_interruption"]) if l["protection"] == "LM211" else "OC_SANS"
        seuil = {"DEFAUT": "SEUIL_SOURCE", "DAC": "SEUIL_DAC", "POT": "SEUIL_POT"}[l["seuil"]]
        memoire = "MEM_EEPROM" if l["memoire"] == "EEPROM" else "MEM_RAM"
        bld = "BLD_HAUT" if l["bld_actif"] == "HAUT" else "BLD_BAS"
        inc.write("    /* LDO %d%s : %s */\n" % (i + 1, " (de cote)" if l["affichage"] == "DE_COTE" else "",
                                                l["commentaire"] or "-"))
        inc.write('    { "%s", %s, %s, %s, %s, &ldoEtat[%d], %s, %s, %s, %s, 1U, 1U, MUX_NC, MUX_NC, %s, %s, %sU, %sU },\n'
                  % (l["nom"], _broche_c(l["en"]), _broche_c(l["bld"]), _broche_c(l["oc"]), _broche_c(l["pg"]),
                     i, _dac_c(l["dac_tension"]), _dac_c(l["dac_courant"]), oc, bld, seuil, memoire,
                     l["CMD_ON_OFF_BLD"], l["CMD_ON_OFF_OC"]))
    return h.getvalue(), inc.getvalue()
