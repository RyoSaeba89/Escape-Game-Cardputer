# Escape Game Cardputer : Explorer 3

Un mini escape game de 5 minutes pour le **M5Stack Cardputer ADV**.

> Journal de bord, Sol 1 : notre vaisseau spatial **Explorer 3** s'est écrasé sur Mars.
> Pour redécoller, il faut réparer le vaisseau et retrouver le code de démarrage de la fusée…
> avant la fin de la réserve d'oxygène !

*English version: branch [`english`](https://github.com/RyoSaeba89/Escape-Game-Cardputer/tree/english).*

## Le jeu

- **4 énigmes** à résoudre dans l'ordre. Chacune ouvre une partie du vaisseau et révèle une lettre du **code de démarrage** (4 lettres).
- **5 minutes d'oxygène.** Une jauge et un compte à rebours restent affichés en haut de l'écran, et une alarme « bip bip bip » accélère à mesure que l'oxygène baisse.
- **Chaque mauvaise réponse coûte 10 secondes.**
- Une fois le code retrouvé, on le tape au clavier pour lancer le **décollage** (animation et son), suivi d'un écran de fin en pixel art.
- Si l'oxygène tombe à zéro, la partie est perdue (écran « Oxygène épuisé »). On peut rejouer immédiatement.
- Le **record** (oxygène restant à l'arrivée) reste en mémoire même après extinction.

### Les énigmes

| # | Lieu | Type d'énigme |
|---|------|---------------|
| 1 | Coffre du fer à souder | Un voyant clignote en **code Morse** |
| 2 | Réservoirs de carburant | Une question de **culture spatiale** (QCM) |
| 3 | Stockage des pièces détachées | Un **picross** (nonogramme) 5×5 |
| 4 | Ordinateur de bord | Un signal **Morse sonore** (l'alarme se tait pour qu'on l'entende) |

Les solutions ne sont pas données ici. ⚠️ **Joueurs : ne lisez pas le code source, il contient les réponses !**

## Commandes

| Touche | Action |
|--------|--------|
| `ENTRÉE` | Valider, continuer, démarrer, rejouer |
| Lettres | Répondre aux énigmes, taper le code |
| `ESPACE` | Rejouer le signal Morse (énigmes 1 et 4) |
| `TAB` | Afficher ou masquer l'alphabet Morse (énigmes 1 et 4) |
| `;` `.` `,` `/` | Déplacer le curseur du picross (haut, bas, gauche, droite) |
| `ENTRÉE` (picross) | Noircir ou effacer une case |
| `DEL` | Effacer une lettre du code |

### Pour le maître du jeu

Appuyer **3 fois de suite sur `Fn`** (moins de 0,8 s entre deux appuis) met la partie en **pause** : le chrono s'arrête, le son se coupe et l'énigme est masquée. Encore **3 fois `Fn`** pour reprendre.

## Mode diffusion : le jeu aussi sur l'écran d'un PC

Au démarrage, choisir **Solo** (le jeu sur le Cardputer seul) ou **Diffusion** : l'écran du Cardputer est recopié en direct dans le navigateur d'un PC branché sur la même box, et **le son sort du PC** (le Cardputer reste muet). On joue toujours avec le clavier du Cardputer.

1. Choisir **Diffusion**, puis le Wi-Fi et taper son mot de passe (mémorisé pour les fois suivantes).
2. Le Cardputer affiche une adresse, par exemple `http://192.168.1.42` : l'ouvrir dans le navigateur du PC (Chrome, Edge, Firefox). Rien à installer.
3. Cliquer une fois sur la page pour **activer le son** (les navigateurs l'exigent). Double-clic : plein écran.
4. `ENTRÉE` sur le Cardputer pour lancer le jeu.

### Le clavier codé (à jouer en équipe)

En mode diffusion, le code de démarrage ne se tape pas en lettres : l'ordinateur de bord affiche un **clavier codé**, 9 symboles sur les touches `1` à `9` (♥ ☺ ♪ ☼…). Seule l'**équipe du PC** voit la **table** qui donne le symbole de chaque lettre (l'écran du PC n'affiche plus la copie du Cardputer à ce moment-là). Le joueur du Cardputer dit une lettre, l'équipe du PC lui décrit le symbole, et ainsi de suite.

- La table est mélangée à chaque partie.
- `1` à `9` : choisir un symbole, `DEL` : effacer, `ENTRÉE` : valider les 4 symboles (−10 s si le code est faux).
- La page du PC affiche aussi le chrono d'oxygène, le nombre de symboles déjà tapés et clignote en rouge à chaque erreur.
- Placer le PC pour que le joueur du Cardputer ne voie pas son écran !

- `` ` `` sur l'écran de connexion ou d'adresse : choisir un autre Wi-Fi.
- Le Cardputer ne capte que le Wi-Fi 2,4 GHz. Les réseaux « invités » bloquent souvent les échanges entre appareils.
- Si la page perd la connexion, elle se reconnecte toute seule ; la partie continue sur le Cardputer.

## Installation

### Avec M5Launcher (carte SD)

1. Télécharger `Escape-Game-Explorer-3.bin` dans la page [Releases](../../releases).
2. Le copier dans le dossier `apps/` de la carte SD.
3. Sur le Cardputer, dans le menu SD de M5Launcher, choisir le fichier pour l'installer.

### Compiler soi-même

Le projet utilise [PlatformIO](https://platformio.org/) :

```
pio run                # compile
pio run -t upload      # compile et flashe par USB
```

- Plateforme `espressif32 @ 6.7.0` (Arduino core 2.0.x)
- Bibliothèques : M5Cardputer, M5Unified, M5GFX, WebSockets (links2004)
- Le jeu : `src/main.cpp` ; le mode diffusion (Wi-Fi, page web, flux écran et son) : `src/diffusion.cpp`

## Matériel

Développé pour le **Cardputer ADV** (ESP32-S3, sans PSRAM, haut-parleur intégré). Aucun accessoire n'est nécessaire.

## Licence

[MIT](LICENSE)
