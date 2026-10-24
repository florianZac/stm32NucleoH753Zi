# -*- coding: utf-8 -*-
"""
@file    generer_config.py
@brief   Etape "pre-build" de CubeIDE : lit configurationLDO/ldo_config.csv,
         le verifie, puis genere Core/Inc/ldo_config_genere.h et
         Core/Inc/ldo_config_lignes.inc. En cas d'erreur, la compilation
         s'arrete et les messages s'affichent dans la console de CubeIDE.
@par     Lancement
         - automatique : C/C++ Build > Settings > Build Steps > Pre-build :
           python ../configurationLDO/generer_config.py
         - manuel : python configurationLDO/generer_config.py
         - verification seule (sans ecrire) : ajouter --verifier
@par     Les fichiers generes ne sont reecrits que si leur contenu change
         (pas de recompilation complete inutile).
@author  Florian Aizac
@date    22/10/2026
"""

import glob
import os
import sys

DOSSIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, DOSSIER)
import ldo_config_csv as cfg  # noqa: E402  (meme dossier)

PROJET = os.path.dirname(DOSSIER)
CSV = os.path.join(DOSSIER, "ldo_config.csv")
SORTIE_H = os.path.join(PROJET, "Core", "Inc", "ldo_config_genere.h")
SORTIE_INC = os.path.join(PROJET, "Core", "Inc", "ldo_config_lignes.inc")


def ecrire_si_change(chemin, texte):
    """
    @brief  Ecrit un fichier seulement si son contenu change.
    @param  chemin : fichier a ecrire.
    @param  texte  : nouveau contenu.
    @retval True si le fichier a ete reecrit.
    @par Exemple d'utilisation
         ecrire_si_change("Core/Inc/ldo_config_genere.h", h)
    """
    if os.path.isfile(chemin):
        with open(chemin, "r", encoding="utf-8") as f:
            if f.read() == texte:
                return False
    with open(chemin, "w", encoding="utf-8", newline="\n") as f:
        f.write(texte)
    return True


def main():
    """
    @brief  Verifie le CSV et genere les fichiers C.
    @retval Code de sortie : 0 = OK, 1 = erreur (la compilation s'arrete).
    @par Exemple d'utilisation
         python configurationLDO/generer_config.py --verifier
    """
    if not os.path.isfile(CSV):
        print("%s:1: error: fichier introuvable" % CSV)
        return 1
    iocs = glob.glob(os.path.join(PROJET, "*.ioc"))
    ioc = iocs[0] if iocs else None

    lignes, erreurs_format = cfg.lire_fichier(CSV)
    erreurs, avertissements = cfg.verifier(lignes, ioc) if not erreurs_format else ([], [])
    erreurs = erreurs_format + erreurs

    # Format "fichier:ligne: error:" : CubeIDE l'affiche dans l'onglet Problems.
    for a in avertissements:
        print("%s:1: warning: %s" % (CSV, a))
    for e in erreurs:
        print("%s:1: error: %s" % (CSV, e))
    if erreurs:
        print("generer_config : %d erreur(s), configuration refusee, rien n'est genere." % len(erreurs))
        return 1

    if "--verifier" in sys.argv:
        print("generer_config : %d LDO, configuration valide, empreinte 0x%08X." % (len(lignes), cfg.empreinte(lignes)))
        return 0

    h, inc = cfg.generer_c(lignes)
    modifs = [os.path.basename(p) for p, t in ((SORTIE_H, h), (SORTIE_INC, inc)) if ecrire_si_change(p, t)]
    print("generer_config : %d LDO, empreinte 0x%08X, %s." % (
        len(lignes), cfg.empreinte(lignes), ("mis a jour : " + ", ".join(modifs)) if modifs else "deja a jour"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
