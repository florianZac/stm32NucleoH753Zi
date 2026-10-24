# -*- coding: utf-8 -*-
"""
@file    doxygen_vers_docx.py
@brief   Construit le document Word de conception du firmware a partir de la
         sortie XML de Doxygen (sortie/xml), sans aucune bibliotheque externe
         (Python 3.8+ standard uniquement).

Contenu du document produit :
  - les chapitres de conception (page principale, Doxygen/conception.dox) ;
  - la reference de TOUTES les fonctions, module par module (@ingroup) :
    signature, role, parametres, retour, exemple d'utilisation, appelants ;
  - les structures de donnees documentees ;
  - les scripts Python de configurationLDO/.

Mise en page reprise du modele modele_conception.docx (meme charte que les
autres documents du projet).

@par Exemple d'utilisation
     cd Doxygen
     doxygen Doxyfile
     python doxygen_vers_docx.py
     -> sortie/Document_Conception_Firmware.docx

     python doxygen_vers_docx.py --xml sortie/xml --sortie mon_doc.docx
"""
import argparse
import datetime
import os
import re
import sys
import xml.etree.ElementTree as ET
import zipfile
from xml.sax.saxutils import escape

ICI = os.path.dirname(os.path.abspath(__file__))

# =============================================================================
#  Ecriture du document Word (WordprocessingML a la main, comme les autres
#  documents du projet : styles, numerotation et marges du modele conserves)
# =============================================================================
CONSOLAS = '<w:rFonts w:ascii="Consolas" w:cs="Consolas" w:eastAsia="Consolas" w:hAnsi="Consolas"/>'
BORDURES = ''.join('<w:%s w:val="single" w:color="auto" w:sz="4"/>' % b
                   for b in ('top', 'left', 'bottom', 'right', 'insideH', 'insideV'))
_CTRL = re.compile('[\x00-\x08\x0b\x0c\x0e-\x1f]')


def _t(texte):
    """@brief Element w:t (espaces preserves, caracteres interdits retires)."""
    return '<w:t xml:space="preserve">%s</w:t>' % escape(_CTRL.sub('', texte), {"'": "&apos;"})


def _run(texte, gras=False, code=False, italique=False, taille=None, couleur=None):
    """@brief Un run Word avec sa mise en forme."""
    rpr = (CONSOLAS if code else '') + ('<w:b/><w:bCs/>' if gras else '') + ('<w:i/><w:iCs/>' if italique else '')
    rpr += ('<w:color w:val="%s"/>' % couleur) if couleur else ''
    rpr += ('<w:sz w:val="%d"/><w:szCs w:val="%d"/>' % (taille, taille)) if taille else ''
    return '<w:r>%s%s</w:r>' % (('<w:rPr>%s</w:rPr>' % rpr) if rpr else '', _t(texte))


def runs(morceaux, taille=None):
    """@brief Runs depuis une liste [(texte, style)] ; style : '', 'code', 'gras', 'ital'."""
    out = ''
    for texte, style in morceaux:
        if not texte:
            continue
        if style == 'code':
            out += _run(texte, code=True, taille=(taille - 2) if taille else 20)
        else:
            out += _run(texte, gras=(style == 'gras'), italique=(style == 'ital'), taille=taille)
    return out


class Doc:
    """@brief Accumule le corps du document (w:body) puis l'ecrit dans une copie du modele."""

    def __init__(self):
        self.x = []

    def page_titre(self, titre, sous_titre, description, date):
        self.x.append('<w:p><w:pPr><w:spacing w:before="1800"/></w:pPr></w:p>')
        self.x.append('<w:p><w:pPr><w:spacing w:after="200"/><w:jc w:val="center"/></w:pPr>%s</w:p>'
                      % _run(titre, gras=True, couleur='0A2A52', taille=56))
        self.x.append('<w:p><w:pPr><w:spacing w:after="800"/><w:jc w:val="center"/></w:pPr>%s</w:p>'
                      % _run(sous_titre, couleur='0090D0', taille=32))
        self.x.append('<w:p><w:pPr><w:spacing w:after="200"/><w:jc w:val="center"/></w:pPr>%s</w:p>'
                      % _run(description, italique=True, taille=24))
        self.x.append('<w:p><w:pPr><w:jc w:val="center"/></w:pPr>%s</w:p>' % _run(date, couleur='6B7684', taille=20))
        self.saut_page()

    def saut_page(self):
        self.x.append('<w:p><w:r><w:br w:type="page"/></w:r></w:p>')

    def sommaire(self, entrees):
        self.titre(1, 'Table des matières')
        for niveau, texte in entrees:
            if niveau == 1:
                self.x.append('<w:p><w:pPr><w:spacing w:after="160"/></w:pPr>%s</w:p>' % _run(texte, gras=True, taille=24))
            else:
                self.x.append('<w:p><w:pPr><w:spacing w:after="100"/><w:ind w:left="360"/></w:pPr>%s</w:p>'
                              % _run(texte, taille=21))
        self.saut_page()

    def titre(self, niveau, texte, nouvelle_page=False):
        saut = '<w:pageBreakBefore/>' if nouvelle_page else ''
        self.x.append('<w:p><w:pPr><w:pStyle w:val="Heading%d"/><w:keepNext/>%s'
                      '<w:spacing w:after="160" w:before="320"/></w:pPr>%s</w:p>' % (niveau, saut, _run(texte)))

    def para(self, morceaux, taille=None, retrait=0, apres=160):
        if isinstance(morceaux, str):
            morceaux = [(morceaux, '')]
        if not any(t.strip() for t, _ in morceaux):
            return
        ind = '<w:ind w:left="%d"/>' % retrait if retrait else ''
        self.x.append('<w:p><w:pPr><w:spacing w:after="%d"/>%s</w:pPr>%s</w:p>' % (apres, ind, runs(morceaux, taille)))

    def puce(self, morceaux, numero=None):
        if isinstance(morceaux, str):
            morceaux = [(morceaux, '')]
        if numero is not None:
            morceaux = [('%d. ' % numero, 'gras')] + morceaux
            self.x.append('<w:p><w:pPr><w:spacing w:after="100"/><w:ind w:left="360"/></w:pPr>%s</w:p>' % runs(morceaux))
        else:
            self.x.append('<w:p><w:pPr><w:pStyle w:val="ListParagraph"/><w:numPr><w:ilvl w:val="0"/>'
                          '<w:numId w:val="1"/></w:numPr><w:spacing w:after="100"/></w:pPr>%s</w:p>' % runs(morceaux))

    def tableau(self, entetes, lignes, largeurs, code_col0=True):
        """@brief Tableau ; chaque cellule = texte ou liste [(texte, style)]."""
        total = sum(largeurs)
        x = ['<w:tbl><w:tblPr><w:tblW w:type="dxa" w:w="%d"/><w:tblBorders>%s</w:tblBorders></w:tblPr><w:tblGrid>%s'
             '</w:tblGrid>' % (total, BORDURES, ''.join('<w:gridCol w:w="%d"/>' % l for l in largeurs))]
        x.append('<w:tr><w:trPr><w:tblHeader/></w:trPr>' + ''.join(
            '<w:tc><w:tcPr><w:tcW w:type="dxa" w:w="%d"/><w:shd w:fill="EAF6FC" w:val="clear"/></w:tcPr>'
            '<w:p>%s</w:p></w:tc>' % (l, _run(e, gras=True, taille=20)) for e, l in zip(entetes, largeurs)) + '</w:tr>')
        for ligne in lignes:
            cellules = []
            for i, (c, l) in enumerate(zip(ligne, largeurs)):
                m = [(c, '')] if isinstance(c, str) else c
                if i == 0 and code_col0:
                    m = [(''.join(t for t, _ in m), 'code')]
                cellules.append('<w:tc><w:tcPr><w:tcW w:type="dxa" w:w="%d"/></w:tcPr><w:p>%s</w:p></w:tc>'
                                % (l, runs(m, taille=20) if not (i == 0 and code_col0) else _run(m[0][0], code=True, taille=18)))
            x.append('<w:tr><w:trPr><w:cantSplit/></w:trPr>' + ''.join(cellules) + '</w:tr>')
        x.append('</w:tbl>')
        self.x.append(''.join(x))
        self.x.append('<w:p><w:pPr><w:spacing w:after="80"/></w:pPr></w:p>')

    def code(self, lignes):
        if not lignes:
            return
        paras = ''.join('<w:p><w:pPr><w:spacing w:after="0"/></w:pPr>%s</w:p>' % _run(l or ' ', code=True, taille=18)
                        for l in lignes)
        self.x.append('<w:tbl><w:tblPr><w:tblW w:type="dxa" w:w="9350"/><w:tblBorders>%s</w:tblBorders></w:tblPr>'
                      '<w:tblGrid><w:gridCol w:w="9350"/></w:tblGrid><w:tr><w:trPr><w:cantSplit/></w:trPr><w:tc><w:tcPr>'
                      '<w:tcW w:type="dxa" w:w="9350"/><w:shd w:fill="F4F6F8" w:val="clear"/><w:tcMar>'
                      '<w:top w:type="dxa" w:w="120"/><w:left w:type="dxa" w:w="150"/><w:bottom w:type="dxa" w:w="120"/>'
                      '<w:right w:type="dxa" w:w="150"/></w:tcMar></w:tcPr>%s</w:tc></w:tr></w:tbl>' % (BORDURES, paras))
        self.x.append('<w:p><w:pPr><w:spacing w:after="80"/></w:pPr></w:p>')

    def ecrire(self, modele, sortie, titre_doc):
        with zipfile.ZipFile(modele) as z:
            doc_xml = z.read('word/document.xml').decode('utf-8')
            entete = doc_xml[:doc_xml.index('<w:body>') + len('<w:body>')]
            sect = re.search(r'<w:sectPr>.*?</w:sectPr>', doc_xml, re.S).group(0)
            corps = entete + ''.join(self.x) + sect + '</w:body></w:document>'
            maintenant = datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
            with zipfile.ZipFile(sortie, 'w', zipfile.ZIP_DEFLATED) as zs:
                for info in z.infolist():
                    donnees = z.read(info.filename)
                    if info.filename == 'word/document.xml':
                        donnees = corps.encode('utf-8')
                    elif info.filename == 'docProps/core.xml':
                        c = donnees.decode('utf-8')
                        c = re.sub(r'(<dcterms:(?:created|modified)[^>]*>)[^<]*(</dcterms:)', r'\g<1>%s\g<2>' % maintenant, c)
                        if '<dc:title>' not in c:
                            c = c.replace('<dc:creator>', '<dc:title>%s</dc:title><dc:creator>' % escape(titre_doc), 1)
                        donnees = c.encode('utf-8')
                    zs.writestr(info, donnees)


# =============================================================================
#  Lecture du XML Doxygen
# =============================================================================
def inline(e, style=''):
    """@brief Texte en ligne d'un element XML -> [(texte, style)]."""
    out = []
    if e.text:
        out.append((e.text, style))
    for c in e:
        tag = c.tag
        if tag == 'sp':
            out.append((' ', style))
        elif tag == 'linebreak':
            out.append(('\n', style))
        elif tag == 'computeroutput':
            out += inline(c, 'code')
        elif tag == 'bold':
            out += inline(c, 'gras')
        elif tag in ('emphasis',):
            out += inline(c, 'ital')
        elif tag in ('parameterlist', 'simplesect', 'programlisting', 'verbatim', 'itemizedlist',
                     'orderedlist', 'table', 'sect1', 'sect2', 'sect3', 'xrefsect'):
            pass                                   # blocs : traites par blocs()
        else:
            out += inline(c, style)
        if c.tail:
            out.append((c.tail, style))
    return out


def nettoyer(morceaux):
    """@brief Fusionne les espaces multiples et retire les espaces de bord."""
    res = []
    for t, s in morceaux:
        t = re.sub(r'[ \t\r\n]+', ' ', t)
        if res and res[-1][1] == s:
            res[-1] = (res[-1][0] + t, s)
        else:
            res.append((t, s))
    if res:
        res[0] = (res[0][0].lstrip(), res[0][1])
        res[-1] = (res[-1][0].rstrip(), res[-1][1])
    return res


def texte_brut(e):
    return ''.join(t for t, _ in nettoyer(inline(e))) if e is not None else ''


def lignes_code(e):
    """@brief Lignes d'un programlisting (codeline/highlight/sp)."""
    lignes = []
    for cl in e.findall('codeline'):
        lignes.append(''.join(t for t, _ in inline(cl)).rstrip())
    return lignes


def blocs(d, e, profondeur=0):
    """@brief Ecrit dans d le contenu bloc d'un element (para, sect, listes...)."""
    for c in list(e):
        if c.tag == 'para':
            texte = nettoyer(inline(c))
            d.para(texte)
            for sous in c:
                if sous.tag == 'programlisting':
                    d.code(lignes_code(sous))
                elif sous.tag == 'verbatim':
                    d.code((sous.text or '').rstrip('\n').split('\n'))
                elif sous.tag in ('itemizedlist', 'orderedlist'):
                    for n, li in enumerate(sous.findall('listitem'), 1):
                        d.puce(nettoyer(sum((inline(p) for p in li.findall('para')), [])),
                               numero=n if sous.tag == 'orderedlist' else None)
                        for p in li.findall('para'):
                            for b in p:
                                if b.tag == 'programlisting':
                                    d.code(lignes_code(b))
                elif sous.tag == 'table':
                    lignes = []
                    for row in sous.findall('row'):
                        lignes.append([nettoyer(inline(en.find('para'))) if en.find('para') is not None else [('', '')]
                                       for en in row.findall('entry')])
                    if lignes:
                        nb = len(lignes[0])
                        larg = [9350 // nb] * nb
                        larg[-1] += 9350 - sum(larg)
                        d.tableau([''.join(t for t, _ in x) for x in lignes[0]], lignes[1:], larg, code_col0=False)
                elif sous.tag == 'simplesect' and sous.get('kind') in ('note', 'warning', 'attention', 'remark'):
                    lib = {'note': 'Note', 'warning': 'Attention', 'attention': 'Attention', 'remark': 'Remarque'}
                    d.para([(lib[sous.get('kind')] + ' : ', 'gras')] +
                           nettoyer(sum((inline(p) for p in sous.findall('para')), [])))
        elif c.tag in ('sect1', 'sect2', 'sect3'):
            niveau = {'sect1': 1, 'sect2': 2, 'sect3': 3}[c.tag]
            d.titre(niveau, texte_brut(c.find('title')))
            blocs(d, c, profondeur + 1)


# =============================================================================
#  Construction du document
# =============================================================================
def charger(xml_dir, nom):
    return ET.parse(os.path.join(xml_dir, nom)).getroot().find('compounddef')


def ordre_groupes(xml_dir):
    """@brief Groupes dans l'ordre de leur @defgroup (conception.dox), sinon alphabetique."""
    index = ET.parse(os.path.join(xml_dir, 'index.xml')).getroot()
    groupes = [(c.get('refid'), c.find('name').text) for c in index.findall('compound') if c.get('kind') == 'group']
    ordre = []
    dox = os.path.join(ICI, 'conception.dox')
    if os.path.exists(dox):
        ordre = re.findall(r'@defgroup\s+(\w+)', open(dox, encoding='utf-8').read())
    rang = {n: i for i, n in enumerate(ordre)}
    return sorted(groupes, key=lambda g: (rang.get(g[1], 999), g[1]))


def signature(md):
    t = texte_brut(md.find('type'))
    statique = 'static ' if md.get('static') == 'yes' and not t.startswith('static') else ''
    return ('%s%s %s%s' % (statique, t, md.find('name').text, md.find('argsstring').text or '')).strip()


def fonction(d, md, fichier_court):
    """@brief Fiche complete d'une fonction C."""
    nom = md.find('name').text
    d.titre(3, nom + '()')
    d.code([signature(md)])
    d.para(nettoyer(inline(md.find('briefdescription/para'))) if md.find('briefdescription/para') is not None else '')

    det = md.find('detaileddescription')
    params, retours, exemples, autres = [], [], [], []
    for p in det.iter('para'):
        texte = nettoyer(inline(p))
        if p in [x for pl in det.iter('parameterlist') for x in pl.iter('para')]:
            continue
        if p in [x for ss in det.iter('simplesect') for x in ss.iter('para')]:
            continue
        if any(t.strip() for t, _ in texte):
            autres.append(texte)
    for pl in det.iter('parameterlist'):
        for it in pl.findall('parameteritem'):
            noms = ', '.join(texte_brut(n) for n in it.findall('parameternamelist/parametername'))
            desc = nettoyer(sum((inline(x) for x in it.findall('parameterdescription/para')), []))
            if pl.get('kind') == 'param':
                if noms.rstrip('.').lower() not in ('aucun', 'none', ''):
                    params.append((noms, desc))
            else:
                retours.append([(noms + ' ', 'code' if noms not in ('Aucun', 'None') else '')] + desc)
    for ss in det.iter('simplesect'):
        if ss.get('kind') == 'return':
            retours.append(nettoyer(sum((inline(x) for x in ss.findall('para')), [])))
        elif ss.get('kind') == 'par':
            titre = texte_brut(ss.find('title'))
            if titre.lower().startswith('exemple'):
                for pl in ss.iter('programlisting'):
                    exemples.append(lignes_code(pl))
                if not list(ss.iter('programlisting')):
                    exemples.append([texte_brut(x) for x in ss.findall('para')])
            else:
                autres.append([(titre + ' : ', 'gras')] + nettoyer(sum((inline(x) for x in ss.findall('para')), [])))
        elif ss.get('kind') in ('note', 'warning', 'attention'):
            autres.append([('Note : ', 'gras')] + nettoyer(sum((inline(x) for x in ss.findall('para')), [])))
    for a in autres:
        d.para(a)
    if params:
        d.tableau(['Paramètre', 'Description'], params, [2200, 7150])
    for r in retours:
        r = [(t.replace('None', 'Aucun'), s) for t, s in r]
        d.para([('Retour : ', 'gras')] + r)
    for ex in exemples:
        d.para([("Exemple d'utilisation :", 'gras')], apres=60)
        d.code(ex)
    loc = md.find('location')
    appelants = sorted({x.text for x in md.findall('referencedby') if x.text and x.text != nom})
    info = [('Définie dans %s, ligne %s.' % (fichier_court, loc.get('bodystart') or loc.get('line')), 'ital')]
    if appelants:
        info.append((' Appelée par : ', 'ital'))
        info.append((', '.join(a + '()' for a in appelants), 'code'))
    d.para(info, taille=18)


def docstring_python(texte):
    """@brief Decoupe une docstring '@brief/@param/@retval/@par' en champs."""
    champs = {'brief': [], 'param': [], 'retval': [], 'exemple': []}
    courant = 'brief'
    for ligne in (texte or '').split('\n'):
        l = ligne.strip()
        m = re.match(r'@(brief|param|retval|return|par)\b\s*(.*)', l)
        if m:
            cle = m.group(1)
            courant = {'return': 'retval', 'par': 'exemple'}.get(cle, cle)
            if cle == 'par':
                continue
            champs[courant].append(m.group(2))
        else:
            if courant == 'exemple':
                champs[courant].append(ligne.rstrip())
            elif l:
                if courant == 'param' and champs['param'] and not l.startswith('@'):
                    champs['param'][-1] += ' ' + l
                else:
                    champs[courant].append(l)
    return champs


def main():
    ap = argparse.ArgumentParser(description="Document Word de conception depuis le XML Doxygen")
    ap.add_argument('--xml', default=os.path.join(ICI, 'sortie', 'xml'))
    ap.add_argument('--modele', default=os.path.join(ICI, 'modele_conception.docx'))
    ap.add_argument('--sortie', default=os.path.join(ICI, 'sortie', 'Document_Conception_Firmware.docx'))
    a = ap.parse_args()
    if not os.path.exists(os.path.join(a.xml, 'index.xml')):
        sys.exit("ERREUR : %s introuvable. Lancer d'abord : doxygen Doxyfile" % os.path.join(a.xml, 'index.xml'))

    index = ET.parse(os.path.join(a.xml, 'index.xml')).getroot()
    version_doxygen = index.get('version', '?')
    page = charger(a.xml, 'indexpage.xml')
    groupes = ordre_groupes(a.xml)

    # ------------------------------------------------------------- sommaire
    chapitres = [texte_brut(s.find('title')) for s in page.find('detaileddescription').findall('sect1')]
    n = len(chapitres)
    d = Doc()
    d.page_titre('Document de conception', 'Firmware Lwip_demo_H753 — Contrôleur de LDO',
                 'Référence développeur générée par Doxygen %s depuis les commentaires du code' % version_doxygen,
                 'Généré le %s' % datetime.date.today().strftime('%d/%m/%Y'))
    entrees = [(1, c) for c in chapitres]
    entrees.append((1, '%d. Référence des fonctions par module' % (n + 1)))
    gdefs = []
    for i, (refid, nom) in enumerate(groupes, 1):
        g = charger(a.xml, refid + '.xml')
        titre = texte_brut(g.find('title'))
        gdefs.append((g, titre))
        entrees.append((2, '%d.%d %s' % (n + 1, i, titre)))
    entrees.append((1, '%d. Structures de données' % (n + 2)))
    entrees.append((1, '%d. Scripts Python de configuration (configurationLDO)' % (n + 3)))
    d.sommaire(entrees)

    # ------------------------------------------------------------- conception
    blocs(d, page.find('detaileddescription'))

    # ------------------------------------------------------------- fonctions
    d.titre(1, '%d. Référence des fonctions par module' % (n + 1), nouvelle_page=True)
    d.para("Chaque fiche reprend le commentaire Doxygen de la fonction : rôle, paramètres, valeur de retour, "
           "exemple d'utilisation, emplacement dans le code et fonctions appelantes.")
    total = 0
    deja = set()
    for i, (g, titre) in enumerate(gdefs, 1):
        d.titre(2, '%d.%d %s' % (n + 1, i, titre))
        d.para(nettoyer(inline(g.find('briefdescription/para'))) if g.find('briefdescription/para') is not None else '')
        membres = [md for md in g.iter('memberdef') if md.get('kind') == 'function']
        membres.sort(key=lambda md: int(md.find('location').get('bodystart') or md.find('location').get('line') or 0))
        lignes = [(md.find('name').text + '()', nettoyer(inline(md.find('briefdescription/para')))
                   if md.find('briefdescription/para') is not None else '') for md in membres]
        d.tableau(['Fonction', 'Rôle'], lignes, [3000, 6350])
        for md in membres:
            fichier = os.path.basename(md.find('location').get('bodyfile') or md.find('location').get('file'))
            fonction(d, md, fichier)
            deja.add(md.find('name').text)
            total += 1
    # fonctions de main.c sans @ingroup
    orphelines = []
    fc = os.path.join(a.xml, 'main_8c.xml')
    if os.path.exists(fc):
        for md in charger(a.xml, 'main_8c.xml').iter('memberdef'):
            if md.get('kind') == 'function' and md.find('name').text not in deja:
                orphelines.append(md)
    if orphelines:
        d.titre(2, '%d.%d Fonctions sans module (@ingroup manquant)' % (n + 1, len(gdefs) + 1))
        for md in orphelines:
            fonction(d, md, 'main.c')
            total += 1

    # ------------------------------------------------------------- structures
    d.titre(1, '%d. Structures de données' % (n + 2), nouvelle_page=True)
    for c in index.findall('compound'):
        if c.get('kind') != 'struct':
            continue
        s = charger(a.xml, c.get('refid') + '.xml')
        brief = s.find('briefdescription/para')
        champs = []
        for md in s.iter('memberdef'):
            if md.get('kind') != 'variable':
                continue
            desc = md.find('briefdescription/para')
            champs.append((md.find('name').text, [(texte_brut(md.find('type')), 'code')] +
                           ([(' — ', '')] + nettoyer(inline(desc)) if desc is not None else [])))
        if brief is None and not any(len(x[1]) > 1 for x in champs):
            continue
        d.titre(3, texte_brut(s.find('compoundname')))
        if brief is not None:
            d.para(nettoyer(inline(brief)))
        if champs:
            d.tableau(['Champ', 'Type — description'], champs, [2600, 6750])

    # ------------------------------------------------------------- Python
    d.titre(1, '%d. Scripts Python de configuration (configurationLDO)' % (n + 3), nouvelle_page=True)
    d.para("Scripts exécutés à la compilation (étape pre-build de CubeIDE) pour générer la configuration des LDO "
           "depuis configurationLDO/ldo_config.csv.")
    for c in index.findall('compound'):
        if c.get('kind') != 'namespace':
            continue
        ns = charger(a.xml, c.get('refid') + '.xml')
        fonctions = [md for md in ns.iter('memberdef') if md.get('kind') == 'function']
        if not fonctions:
            continue
        d.titre(2, '%s.py' % texte_brut(ns.find('compoundname')))
        for md in sorted(fonctions, key=lambda m: int(m.find('location').get('line') or 0)):
            v = md.find('.//verbatim')
            ch = docstring_python(v.text if v is not None else texte_brut(md.find('detaileddescription')))
            d.titre(3, md.find('name').text + '()')
            d.code(['def %s%s' % (md.find('name').text, md.find('argsstring').text or '()')])
            d.para(' '.join(ch['brief']))
            if ch['param']:
                d.tableau(['Paramètre', 'Description'],
                          [tuple((p.split(':', 1) + [''])[:2]) if ':' in p else (p.split(' ', 1) + [''])[:2]
                           for p in ch['param']], [2200, 7150])
            if ch['retval']:
                d.para([('Retour : ', 'gras'), (' '.join(ch['retval']), '')])
            ex = [l for l in ch['exemple'] if l.strip()]
            if ex:
                retrait = min(len(l) - len(l.lstrip()) for l in ex)
                d.para([("Exemple d'utilisation :", 'gras')], apres=60)
                d.code([l[retrait:] for l in ch['exemple']] if ch['exemple'] else [])
            total += 1

    os.makedirs(os.path.dirname(os.path.abspath(a.sortie)), exist_ok=True)
    d.ecrire(a.modele, a.sortie, 'Document de conception - firmware Lwip_demo_H753')
    print('Document ecrit : %s (%d fonctions)' % (a.sortie, total))


if __name__ == '__main__':
    main()
