// Textes du jeu en français et en anglais, côte à côte : { français, anglais }.
// tr(X) donne le texte dans la langue choisie. Les touches s'écrivent
// « touche : action » : le jeu met la touche en orange. Avant de changer un
// texte, vérifier qu'il tient à l'écran (240×135, police efontJA_12 : 6 px par caractère latin,
// accents compris ; voir docs/TECHNIQUE.md, section 3).
#pragma once

namespace textes {

using Tx = const char *const[2];

// Flèches du Cardputer (touches ; . , /), dessinées en orange par le jeu.
// Dans un pied de page, '|' sépare les groupes « touche : action ».
#define K_UP "\001"
#define K_DOWN "\002"
#define K_LEFT "\003"
#define K_RIGHT "\004"

// ---------------------------------------------------------------- langue
constexpr Tx LANG_TITLE = {"LANGUE / LANGUAGE", "LANGUE / LANGUAGE"};
constexpr Tx LANG_NAMES = {"Français", "English"};
constexpr Tx LANG_INFOS = {"Jouer en français", "Play in English"};
constexpr Tx LANG_FOOTER = {K_UP K_DOWN "|ENTRÉE / ENTER : ok", K_UP K_DOWN "|ENTRÉE / ENTER : ok"};

// ---------------------------------------------------------------- choix du mode
constexpr Tx MODE_SOLO = {"Cardputer seul", "Cardputer only"};
constexpr Tx MODE_SOLO_INFO = {"Tout le jeu sur le Cardputer", "Everything on the Cardputer"};
constexpr Tx MODE_SCREEN = {"Avec écran", "With a screen"};
constexpr Tx MODE_SCREEN_INFO = {"Écran et son sur PC ou télé", "Screen and sound on a PC or TV"};
constexpr Tx MODE_LANG = {"Langue / Language", "Langue / Language"};
constexpr Tx MODE_LANG_INFO = {"Français", "English"};
constexpr Tx MODE_FOOTER = {K_UP K_DOWN " : choisir|ENTRÉE : ok", K_UP K_DOWN ": select|ENTER: confirm"};

// ---------------------------------------------------------------- Wi-Fi
constexpr Tx WIFI_TITLE = {"CHOIX DU WI-FI", "CHOOSE WI-FI"};
constexpr Tx WIFI_REFRESH = {"R : actualiser", "R: refresh"};
constexpr Tx WIFI_SEARCHING = {"Recherche des réseaux...", "Searching for networks..."};
constexpr Tx WIFI_NONE = {"Aucun réseau trouvé", "No network found"};
constexpr Tx WIFI_SCAN_FAILED = {"Recherche impossible : R pour réessayer", "Search failed: R to try again"};
constexpr Tx WIFI_CREATE = {"Créer le réseau Explorer3", "Create the Explorer3 network"};
constexpr Tx WIFI_OTHER = {"Autre réseau (nom à taper)", "Other network (type its name)"};
constexpr Tx WIFI_FOOTER = {K_UP K_DOWN " : choisir|ENTRÉE : ok|ESC : retour", K_UP K_DOWN ": select|ENTER: confirm|ESC: back"};

constexpr Tx SSID_TITLE = {"NOM DU RÉSEAU", "NETWORK NAME"};
constexpr Tx SSID_HINT = {"Réseau masqué : tapez son nom exact", "Hidden network: type its exact name"};
constexpr Tx PASS_TITLE = {"MOT DE PASSE WI-FI", "WI-FI PASSWORD"};
constexpr Tx TYPING_FOOTER = {"ENTRÉE : ok|DEL : effacer|ESC : retour", "ENTER: confirm|DEL: erase|ESC: back"};

constexpr Tx CONNECT_TITLE = {"CONNEXION AU WI-FI", "CONNECTING TO WI-FI"};
constexpr Tx CONNECT_ATTEMPT = {"Essai n° ", "Attempt "};
constexpr Tx CONNECT_FOOTER = {"ESC : choisir un autre Wi-Fi", "ESC: choose another Wi-Fi"};

constexpr Tx ERR_PASSWORD = {"Mot de passe refusé", "Wrong password"};
constexpr Tx ERR_NOT_FOUND = {"Réseau introuvable", "Network not found"};
constexpr Tx ERR_NO_ANSWER = {"Le réseau ne répond pas", "The network is not responding"};
constexpr Tx ERR_MEMORY = {"Mémoire insuffisante", "Not enough memory"};
constexpr Tx ERR_AP = {"Création du réseau impossible", "Could not create the network"};

// ---------------------------------------------------------------- connecter l'écran
constexpr Tx ADDR_TITLE = {"CONNECTER L'ÉCRAN", "CONNECT THE SCREEN"};
constexpr Tx ADDR_OPEN = {"Scannez ou ouvrez :", "Scan or open:"};
constexpr Tx ADDR_JOIN = {"1. Rejoignez le Wi-Fi", "1. Join the Wi-Fi"};
constexpr Tx ADDR_PASSWORD = {"Mot de passe :", "Password:"};
constexpr Tx ADDR_PAGE = {"2. Ouvrez la page :", "2. Open the page:"};
constexpr Tx ADDR_DEVICES = {"Connectés : ", "Connected: "};
constexpr Tx ADDR_BROWSER_OK = {"Navigateur connecté", "Browser connected"};
constexpr Tx ADDR_WAITING = {"En attente d'un navigateur...", "Waiting for a browser..."};
constexpr Tx ADDR_CONTINUE = {"ENTRÉE : continuer", "ENTER: continue"};
constexpr Tx ADDR_FOOTER = {"ESC : changer de Wi-Fi", "ESC: change Wi-Fi"};
constexpr Tx ADDR_FOOTER_AP = {"TAB : autre QR code|ESC : retour", "TAB: other QR code|ESC: back"};

// ---------------------------------------------------------------- titre et briefing
constexpr Tx TITLE_SUB = {"Escape game : crash sur Mars", "Escape game: crash on Mars"};
constexpr Tx TITLE_START = {"ENTRÉE : commencer", "ENTER: start"};
constexpr Tx BRIEF_TITLE = {"JOURNAL DE BORD - SOL 1", "CAPTAIN'S LOG - SOL 1"};
constexpr Tx BRIEF_TEXT = {
    "Notre vaisseau Explorer 3 s'est écrasé sur Mars. Pour redécoller, nous devons le réparer et "
    "retrouver son code de démarrage.",
    "Our spaceship Explorer 3 has crashed on Mars. To take off again, we must repair it and find its "
    "start-up code."};
constexpr Tx BRIEF_RULES = {"4 énigmes, 5 minutes d'oxygène. Chaque erreur coûte 10 secondes !",
                            "4 puzzles, 5 minutes of oxygen. Each mistake costs 10 seconds!"};
constexpr Tx BRIEF_START = {"ENTRÉE : démarrer le chrono", "ENTER: start the countdown"};

// ---------------------------------------------------------------- énigmes
constexpr Tx DENIED = {" : ACCÈS REFUSÉ", ": ACCESS DENIED"};  // après la lettre tapée

constexpr Tx P1_TITLE = {"ÉNIGME 1/4 : COFFRE DU FER À SOUDER", "PUZZLE 1/4: SOLDERING IRON SAFE"};
constexpr Tx P1_TEXT = {"Le coffre est verrouillé. Son voyant clignote une lettre en Morse : tapez-la !",
                        "The safe is locked. Its light blinks a letter in Morse code: type it!"};
constexpr Tx P1_FOOTER = {"ESPACE : revoir|TAB : alphabet Morse", "SPACE: watch again|TAB: Morse code"};

constexpr Tx P2_TITLE = {"ÉNIGME 2/4 : RÉSERVOIRS DE CARBURANT", "PUZZLE 2/4: FUEL TANKS"};
constexpr Tx P2_TEXT = {"Verrou des réservoirs : quel est le premier rover à avoir roulé sur Mars ?",
                        "Fuel tank lock: which rover was the first to drive on Mars?"};
constexpr Tx P2_FOOTER = {"A B C D : répondre", "A B C D: answer"};

constexpr Tx P3_TITLE = {"ÉNIGME 3/4", "PUZZLE 3/4"};
constexpr Tx P3_PLACE = {"Soute à pièces", "Cargo hold"};
constexpr Tx P3_TEXT = {"Chaque chiffre = cases allumées à la suite, dans l'ordre.",
                        "Each number = lit cells in a row, in order."};
constexpr Tx P3_MOVE = {K_UP K_DOWN K_LEFT K_RIGHT " : bouger", K_UP K_DOWN K_LEFT K_RIGHT ": move"};
constexpr Tx P3_LIGHT = {"ENTRÉE : allumer", "ENTER: light up"};

constexpr Tx P4_TITLE = {"ÉNIGME 4/4 : ORDINATEUR DE BORD", "PUZZLE 4/4: ON-BOARD COMPUTER"};
constexpr Tx P4_TEXT = {"L'ordinateur de bord émet en Morse la dernière lettre du code. Écoutez et tapez-la !",
                        "The on-board computer beeps the last letter of the code in Morse. Listen and type it!"};
constexpr Tx P4_FOOTER = {"ESPACE : réécouter|TAB : alphabet Morse", "SPACE: listen again|TAB: Morse code"};

constexpr Tx MORSE_PLAYING = {"Signal en cours|TAB : alphabet Morse", "Signal playing|TAB: Morse code"};
constexpr Tx MORSE_HELP = {"ALPHABET MORSE", "MORSE CODE"};
constexpr Tx MORSE_CLOSE = {"Une touche : fermer", "Any key: close"};

// ---------------------------------------------------------------- partie réparée
constexpr Tx SOLVED_TITLES[3] = {
    {"COFFRE DÉVERROUILLÉ", "SAFE UNLOCKED"},
    {"RÉSERVOIR OUVERT !", "FUEL TANK OPEN!"},
    {"SOUTE OUVERTE !", "CARGO HOLD OPEN!"},
};
constexpr Tx SOLVED_ITEMS[3] = {
    {"Fer à souder récupéré", "Soldering iron recovered"},
    {"Carburant récupéré", "Fuel recovered"},
    {"Pièces détachées récupérées", "Spare parts recovered"},
};
constexpr Tx SOLVED_LETTER = {"Gravée dessus :", "Engraved on it:"};
constexpr Tx SOLVED_NEXT = {"ENTRÉE : énigme suivante", "ENTER: next puzzle"};

// ---------------------------------------------------------------- ordinateur de bord
constexpr int TERM_COUNT = 5;
constexpr Tx TERM_LINES[TERM_COUNT] = {
    {"> Diagnostic des systèmes...", "> Running system diagnostics..."},
    {"> Fer à souder ........ OK", "> Soldering iron ...... OK"},
    {"> Carburant ........... OK", "> Fuel ................ OK"},
    {"> Pièces détachées .... OK", "> Spare parts ......... OK"},
    {"> CODE DE DÉMARRAGE TROUVÉ : NASA", "> START-UP CODE FOUND: NASA"},
};
constexpr Tx CODE_LABEL = {"Code de démarrage :", "Start-up code:"};
constexpr Tx CODE_REFUSED = {"> CODE REFUSÉ", "> CODE REJECTED"};
constexpr Tx CODE_FOOTER = {"A-Z : code|ENTRÉE : ok|DEL : effacer", "A-Z: code|ENTER: confirm|DEL: erase"};

constexpr Tx KEYPAD_TITLE = {"CLAVIER CODÉ", "CODED KEYPAD"};
constexpr Tx KEYPAD_HINT = {"L'autre équipe a la table : demandez-lui les symboles.",
                            "The other team has the table: ask them for the symbols."};
constexpr Tx KEYPAD_REFUSED = {"CODE REFUSÉ", "CODE REJECTED"};
constexpr Tx KEYPAD_FOOTER = {"1-9 : symbole|DEL : effacer|ENTRÉE : ok", "1-9: symbol|DEL: erase|ENTER: confirm"};

// ---------------------------------------------------------------- décollage et fins
constexpr Tx LAUNCH_IGNITION = {"ALLUMAGE DES MOTEURS", "ENGINE IGNITION"};
constexpr Tx LAUNCH_LIFTOFF = {"DÉCOLLAGE !", "LIFTOFF!"};

constexpr Tx WIN_TITLE1 = {"MISSION", "MISSION"};
constexpr Tx WIN_TITLE2 = {"ACCOMPLIE !", "COMPLETE!"};
constexpr Tx WIN_TEXT = {"Explorer 3 a redécollé !", "Explorer 3 took off!"};
constexpr Tx WIN_O2 = {"O2 restant : ", "O2 left: "};
constexpr Tx WIN_NEW_RECORD = {"NOUVEAU RECORD !", "NEW RECORD!"};
constexpr Tx WIN_RECORD = {"Record : ", "Record: "};
constexpr Tx WIN_AGAIN = {"ENTRÉE : rejouer", "ENTER: play again"};

constexpr Tx LOST_TITLE = {"OXYGÈNE ÉPUISÉ", "OUT OF OXYGEN"};
constexpr Tx LOST_TEXT1 = {"Mission échouée...", "Mission failed..."};
constexpr Tx LOST_TEXT2 = {"Explorer 3 reste sur Mars.", "Explorer 3 is stuck on Mars."};
constexpr Tx LOST_AGAIN = {"ENTRÉE : nouvelle tentative", "ENTER: try again"};

// ---------------------------------------------------------------- pause (maître du jeu)
constexpr Tx PAUSE_TITLE = {"PAUSE", "PAUSED"};
constexpr Tx PAUSE_TIMER = {"Chrono arrêté : ", "Timer stopped: "};
constexpr Tx PAUSE_RESUME = {"Fn Fn Fn : reprendre", "Fn Fn Fn: resume"};

}  // namespace textes
