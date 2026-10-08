# Simulateur PC

Le jeu (`src/main.cpp`, sans aucun changement) tourne dans une fenêtre sur PC, avec le vrai **M5GFX** : mêmes polices, mêmes dessins, au pixel près. Pratique pour vérifier un texte ou un écran sans flasher le Cardputer.

Ce qui est simulé :

- l'écran (fenêtre SDL, agrandie ×3) ;
- le clavier du Cardputer, par le clavier du PC ;
- le mode multijoueur « à vide » (`sim_diffusion.cpp`) : réseaux Wi-Fi fictifs, connexion réussie au bout de 1,5 s (le réseau « Mauvais mot de passe » refuse toujours), un navigateur et un appareil Explorer3 qui se connectent tout seuls. Aucune page web n'est servie : la page du centre de contrôle ne se voit pas dans le simulateur.

Pas de son, et la mémoire (langue, record, Wi-Fi) est oubliée à la fermeture.

## Installer (Windows)

1. Installer [MSYS2](https://www.msys2.org/), puis dans le terminal MSYS2 :
   ```
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2
   ```
2. Mettre `C:\msys64\ucrt64\bin` dans le `PATH` (ou seulement dans le terminal qui compile).

Sous Linux : `sudo apt install build-essential libsdl2-dev`. Sous macOS : `brew install sdl2`. (Ces deux systèmes n'ont pas été essayés.)

## Compiler et lancer

```
pio run -e simulateur
.pio/build/simulateur/program.exe      (program sous Linux et macOS)
```

`pio run` seul ne compile que le firmware du Cardputer.

## Touches

| PC | Cardputer |
|----|-----------|
| Lettres, chiffres, ponctuation | Les mêmes touches |
| `Entrée` | `ENTRÉE` |
| `Retour arrière`, `Suppr` | `DEL` |
| `Tab`, `Espace` | `TAB`, `ESPACE` |
| `Échap` | `ESC` (touche `` ` ``, retour) |
| Flèches | Flèches (touches `;` `.` `,` `/`) |
| `F1` | `Fn` (3 fois : pause) |
| `F12` | Capture de l'écran (`capture-1.bmp`, `capture-2.bmp`…) |

Les raccourcis de M5GFX (zoom, rotation) passent par `Ctrl droit` + chiffre ou `R`/`L`.

## Options

- `--lang fr` ou `--lang en` : langue déjà choisie (sinon, écran de langue comme au premier démarrage).
- `--script "…"` : actions horodatées, en millisecondes depuis le lancement, séparées par `;`. Actions : un nom de touche (`enter`, `del`, `tab`, `space`, `fn`, `esc`, `up`, `down`, `left`, `right`) ou un caractère, `type texte`, `shot fichier.bmp`, `quit`.

Exemple : l'écran titre des pages GitHub (`docs/images/titre-fr.png`), au moment où « ENTRÉE : commencer » est affiché :

```
program.exe --lang fr --script "1000 enter; 3150 shot titre_fr.bmp; 3300 quit"
ffmpeg -i titre_fr.bmp -vf scale=iw*4:ih*4:flags=neighbor docs/images/titre-fr.png
```

## Fichiers

| Fichier | Rôle |
|---|---|
| `sim_main.cpp` | Lancement SDL, clavier, captures, scripts, fonctions Arduino (`millis`, `random` identique à celui de l'ESP32…) |
| `sim_diffusion.cpp` | Remplace `src/diffusion.cpp` (réseau fictif) |
| `include/` | Versions réduites de `Arduino.h`, `M5Cardputer.h`, `Preferences.h` et du lecteur de clavier de l'ADV |
