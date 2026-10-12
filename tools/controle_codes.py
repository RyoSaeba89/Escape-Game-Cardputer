"""Aucun code de démarrage ne doit figurer dans ses énigmes.

Pour chaque code de src/mots.h, relit ce que les joueurs voient pendant ses
énigmes, dans sa langue et dans chaque mode où il peut sortir :
  - sa question et ses 4 réponses (QCM, Cardputer seul) ;
  - les noms et l'indice de son labyrinthe, ou les planètes de la page
    (multijoueur) ;
  - les textes fixes des énigmes : src/textes.h, la page de src/diffusion.cpp
    et, si le dépôt est à côté, source/texts.c de GC-Explorer3.
Sans accents ni majuscules, un mot du texte est refusé s'il est le code, son
pluriel ou, pour un code qui finit par S, son singulier, ou s'il commence ou
finit par le code (« starship »). AUTORISES liste les mots qui contiennent un
code sans le dire.

    python tools/controle_codes.py            (depuis la racine du dépôt)

Rend 1 et liste les fautes s'il y en a. Ce fichier ne contient aucun code :
il les lit dans src/mots.h.
"""
import os
import re
import sys
import unicodedata

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GC = os.path.join(ROOT, '..', 'GC-Explorer3', 'source', 'texts.c')

# Mots qui contiennent un code au début ou à la fin sans le révéler
AUTORISES = {'trouvez', 'trouve', 'trouver', 'retrouver', 'start', 'starts', 'starting'}

# Textes fixes des énigmes, par mode (préfixes des noms de textes.h)
SOLO = ['P1_TEXT', 'P1_TITLE', 'P1_FOOTER', 'P2_TITLE', 'P2_LOCKED', 'P2_FOOTER', 'P3_TITLE', 'P3_PLACE',
        'P3_TEXT', 'P3_MOVE', 'P3_LIGHT', 'P4_TITLE', 'P4_TEXT', 'P4_FOOTER', 'MORSE', 'DENIED', 'SOLVED',
        'TERM', 'CODE_']
MULTI = ['P1_TITLE', 'P1_TEXT_MULTI', 'P1_FOOTER_MULTI', 'P2_TITLE_MULTI', 'P2_PLACE_MULTI', 'P2_TEXT_MULTI',
         'P3_MOVE', 'P3_LIGHT', 'P3_TITLE_MULTI', 'P3_FOOTER_MULTI', 'P4_TEXT_MAZE', 'P4_FOOTER_MAZE',
         'P4_WRONG_MAZE', 'P4_TITLE', 'MORSE', 'DENIED', 'SOLVED', 'KEYPAD']
# Textes des énigmes sur la page du centre de contrôle (objet TX)
PAGE = ['t1', 'p1', 'q1', 't2', 'p2', 't3', 'p3', 'q3', 't4', 'e4', 'ent', 'sor', 'tc', 'pc', 'sa']
PAGE_PLANETS = ['p4', 'h4']    # labyrinthe sans thème
PAGE_THEME = ['p4t']           # labyrinthe à thème


def lire(chemin):
    with open(chemin, encoding='utf-8') as f:
        return f.read()


def norm(t):
    t = unicodedata.normalize('NFD', t)
    return ''.join(c for c in t if unicodedata.category(c) != 'Mn').lower()


def chaines(bloc):
    """Chaînes C d'un bloc ; celles qui se suivent sont mises bout à bout."""
    out, cur = [], None
    for m in re.finditer(r'"((?:[^"\\]|\\.)*)"|([,;{}()])', bloc):
        if m.group(1) is not None:
            s = m.group(1).replace('\\"', '"')
            cur = s if cur is None else cur + s
        else:
            if cur is not None:
                out.append(cur)
            cur = None
    if cur is not None:
        out.append(cur)
    return out


def textes_h(textes, prefixes):
    """{langue: [(nom, texte)]} des Tx de textes.h dont le nom commence par un préfixe."""
    out = {0: [], 1: []}
    for m in re.finditer(r'constexpr Tx (\w+)(?:\[\d+\])? = (\{.*?\});\n', textes, re.S):
        if any(m.group(1).startswith(p) for p in prefixes):
            st = chaines(m.group(2))
            for i in range(0, len(st) - 1, 2):
                out[0].append(('textes.h ' + m.group(1), st[i]))
                out[1].append(('textes.h ' + m.group(1), st[i + 1]))
    return out


def page_tx(diff):
    """{langue: {clé: texte}} de l'objet TX de la page (tableaux compris)."""
    tx = re.search(r'const TX=\{(.*?)\}\};', diff, re.S).group(1)
    out = {}
    for lang, debut in ((0, 'fr:{'), (1, 'en:{')):
        b = tx.index(debut)
        bloc = tx[b:tx.index('en:{')] if lang == 0 else tx[b:]
        d = {}
        for m in re.finditer(r"[{,\s](\w+):(?:(['\"])(.*?)\2|\[(.*?)\])", bloc, re.S):
            d[m.group(1)] = m.group(3) if m.group(3) is not None else re.findall(r"'([^']*)'", m.group(4))
        out[lang] = d
    return out


def gc_textes(chemin):
    """{langue: {clé de la page: texte}} de texts.c de GC-Explorer3."""
    out = {0: {}, 1: {}}
    if not os.path.exists(chemin):
        return out
    for m in re.finditer(r'\[TX_\w+\]\s*=\s*\{"(\w+)",\s*\{(.*?)\}\}', lire(chemin), re.S):
        st = chaines('{' + m.group(2) + '}')
        if len(st) >= 2:
            out[0][m.group(1)] = st[0]
            out[1][m.group(1)] = st[1]
    return out


def main():
    mots = lire(os.path.join(ROOT, 'src', 'mots.h'))
    textes = lire(os.path.join(ROOT, 'src', 'textes.h'))
    page = page_tx(lire(os.path.join(ROOT, 'src', 'diffusion.cpp')))
    gc = gc_textes(GC)
    fixes = {'solo': textes_h(textes, SOLO), 'multi': textes_h(textes, MULTI)}

    themes = {}
    for m in re.finditer(r'const Theme (\w+) = \{(.*?)\};', mots, re.S):
        corps = m.group(2)
        noms = chaines(re.search(r'\{.*?\}', corps, re.S).group(0))
        themes[m.group(1)] = [('labyrinthe', t) for t in noms + chaines(corps[corps.rindex('}') + 1:])]

    fautes = 0
    vus = 0
    for lang, tableau in ((0, 'MOTS_FR'), (1, 'MOTS_EN')):
        bloc = re.search(r'const Mot ' + tableau + r'\[\] = \{(.*?)\n\};', mots, re.S).group(1)
        for m in re.finditer(r'\{"([A-Z]{4})", (".*?"),\s*\{(.*?)\},\s*.*?,\s*(\w+), ([&\w]+)\},', bloc, re.S):
            code, question, reponses, laby, theme = m.groups()
            vus += 1
            cle = norm(code)
            exacts = {cle, cle + 's'} | ({cle[:-1]} if cle.endswith('s') else set())
            modes = ['solo'] + (['multi'] if laby != 'nullptr' else [])
            for mode in modes:
                vu = list(fixes[mode][lang])
                if mode == 'solo':
                    vu += [('question', chaines(question)[0])]
                    vu += [('réponse', r) for r in chaines('{' + reponses + '}')]
                else:
                    cles = PAGE + (PAGE_THEME if theme != 'nullptr' else PAGE_PLANETS)
                    vu += [('page ' + k, page[lang][k]) for k in cles if k in page[lang]]
                    vu += [('GC ' + k, gc[lang][k]) for k in cles if k in gc[lang]]
                    if theme == 'nullptr':
                        vu += [('page pl', p) for p in page[lang]['pl']]
                    else:
                        vu += themes[theme.lstrip('&')]
                for ou, texte in vu:
                    for mot in re.findall(r'[a-z0-9]+', norm(texte)):
                        if mot in AUTORISES:
                            continue
                        if mot in exacts or mot.startswith(cle) or mot.endswith(cle):
                            fautes += 1
                            print('%s %s %-5s %-24s « %s » dans : %s' % (
                                'FR' if lang == 0 else 'EN', code, mode, ou, mot, texte))
    if vus == 0:
        print('aucun code lu dans src/mots.h : le format a changé ?')
        return 1
    if not gc[0]:
        print('(GC-Explorer3 absent à côté du dépôt : ses textes ne sont pas relus)')
    print('%d codes relus, %d faute(s)' % (vus, fautes))
    return 1 if fautes else 0


if __name__ == '__main__':
    sys.exit(main())
