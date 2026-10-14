// Textes du centre de contrôle sur le Cardputer (controle.h), { français, anglais }.
// Écrans avant la partie : ceux d'Explorer3-Android et d'Explorer3-RaspberryPi.
// Pages : celles de la page web (objet TX de diffusion.cpp), dans la langue de
// l'équipage. Même règle que textes.h : 240×135, 6 px par caractère.
#pragma once

namespace textes {

// ---------------------------------------------------------------- avant la partie
constexpr Tx C_SEARCH_TITLE = {"RECHERCHE DE L'ÉQUIPAGE", "LOOKING FOR THE CREW"};
constexpr Tx C_NETWORK = {"Réseau : ", "Network: "};
constexpr Tx C_SEARCH_FOOTER = {"TAB : adresse|ESC : menu", "TAB: address|ESC: menu"};
constexpr Tx C_NONE = {"Aucun équipage trouvé.", "No crew found."};
constexpr Tx C_NONE_HELP = {
    "Sur l'appareil de l'équipage : Multijoueur, sur le même réseau. Son adresse s'affiche sur l'écran "
    "CONNECTER L'ÉCRAN.",
    "On the crew's device: Multiplayer, on the same network. Its address shows on the CONNECT THE "
    "SCREEN screen."};
constexpr Tx C_SEARCH_AGAIN = {"ENTRÉE : chercher encore", "ENTER: search again"};
constexpr Tx C_LIST_TITLE = {"ÉQUIPAGES TROUVÉS", "CREWS FOUND"};
constexpr Tx C_CREW = {"Équipage ", "Crew "};
constexpr Tx C_LIST_FOOTER = {K_UP K_DOWN " : choisir|ENTRÉE : ok|TAB : adresse",
                              K_UP K_DOWN ": select|ENTER: confirm|TAB: address"};
constexpr Tx C_ADDR_TITLE = {"ADRESSE DE L'ÉQUIPAGE", "CREW ADDRESS"};
constexpr Tx C_ADDR_HELP = {"Elle s'affiche sur l'écran CONNECTER L'ÉCRAN de l'équipage.",
                            "It shows on the crew's CONNECT THE SCREEN screen."};
constexpr Tx C_ADDR_FOOTER = {"ENTRÉE : ok|DEL : effacer|ESC : retour", "ENTER: confirm|DEL: erase|ESC: back"};
constexpr Tx C_ADDR_OTHER_NET = {"Attention : cette adresse n'est pas sur le réseau de cet appareil.",
                                 "Warning: this address is not on this device's network."};
constexpr Tx C_ADDR_INVALID = {"Adresse impossible.", "Impossible address."};
constexpr Tx C_ADDR_SELF = {"C'est l'adresse de cet appareil.", "That is this device's own address."};
constexpr Tx C_ADDR_NET_BCAST = {"Adresse réservée du réseau : impossible.", "Reserved network address: impossible."};
constexpr Tx C_CONN_TITLE = {"CONNEXION", "CONNECTING"};
constexpr Tx C_CONN_WAIT = {"Connexion à l'équipage…", "Connecting to the crew…"};
constexpr Tx C_CANCEL_FOOTER = {"ESC : annuler", "ESC: cancel"};
constexpr Tx C_ERR_TITLE = {"CONNEXION IMPOSSIBLE", "CONNECTION FAILED"};
constexpr Tx C_ERR_UNREACHABLE = {
    "Équipage injoignable. Est-il allumé, en mode Multijoueur, sur le même réseau ? L'adresse est-elle la bonne ?",
    "Crew unreachable. Is it on, in Multiplayer mode, on the same network? Is the address right?"};
constexpr Tx C_ERR_REFUSED = {"Un appareil répond à cette adresse, mais le mode Multijoueur n'y est pas lancé.",
                              "A device answers at this address, but Multiplayer mode is not running on it."};
constexpr Tx C_ERR_NOT_GAME = {"L'appareil à cette adresse n'est pas un équipage Explorer 3.",
                               "The device at this address is not an Explorer 3 crew."};
constexpr Tx C_ERR_NO_SOCKET = {"Plus de connexion disponible : relancer le jeu.", "No connection left: restart the game."};
constexpr Tx C_ERR_NO_MEMORY = {"Mémoire insuffisante : relancer le jeu.", "Not enough memory: restart the game."};
constexpr Tx C_LOST = {"Connexion perdue.", "Connection lost."};
constexpr Tx C_ERR_FOOTER = {"ENTRÉE : réessayer|ESC : retour", "ENTER: try again|ESC: back"};
constexpr Tx C_BACK_FOOTER = {"ESC : retour", "ESC: back"};
constexpr Tx C_OLD_GAME = {"Jeu d'une autre version sur l'équipage : mettre les deux appareils à jour.",
                           "The crew's game is another version: update both devices."};
constexpr Tx C_LEAVE = {"ESC : changer d'équipage", "ESC: change crew"};

// ---------------------------------------------------------------- pages (langue de l'équipage)
constexpr Tx P_TEAM = {"CENTRE DE CONTRÔLE", "MISSION CONTROL"};
constexpr Tx P_RULES = {"RÈGLES", "RULES"};
constexpr Tx P_R1 = {"Vous êtes le centre de contrôle, sur Terre.", "You are mission control, on Earth."};
constexpr Tx P_R2 = {"L'équipage a l'autre partie des indices : parlez-vous !",
                     "The crew has the other half of the clues: talk to each other!"};
constexpr Tx P_R3 = {"Ne montrez pas votre écran à l'équipage.", "Don't show your screen to the crew."};
constexpr Tx P_R4 = {"Le chrono démarre quand l'équipage lance la partie.", "The countdown starts when the crew starts the game."};
// Titres : « ÉNIGME 2/4 » et le lieu, coupés au « : » quand la place manque
constexpr Tx P_T1 = {"ÉNIGME 1/4 : COFFRE DU FER À SOUDER", "PUZZLE 1/4: SOLDERING IRON SAFE"};
constexpr Tx P_P1 = {
    "Une lettre en Morse clignote sur le cadenas du coffre. L'équipage vous décrit les flashs : trouvez la lettre et "
    "dites-la-lui !",
    "A Morse letter is blinking on the safe's padlock. The crew describes the flashes: find the letter and tell them!"};
constexpr Tx P_Q1 = {"Point = flash court · Trait = flash long", "Dot = short flash · Dash = long flash"};
constexpr Tx P_T2 = {"ÉNIGME 2/4 : RÉSERVOIRS DE CARBURANT", "PUZZLE 2/4: FUEL TANKS"};
constexpr Tx P_P2 = {
    "La porte des réservoirs est verrouillée. Lisez les chiffres à l'équipage et guidez-le. Chaque chiffre = nombre "
    "de lumières à la suite, dans l'ordre. Une ligne juste passe au vert.",
    "The fuel tank door is locked. Read the numbers to the crew and guide them. Each number = lights in a row, in "
    "order. A correct line turns green."};
constexpr Tx P_GREEN = {"Une ligne juste passe au vert.", "A correct line turns green."};
constexpr Tx P_T3 = {"ÉNIGME 3/4 : SOUTE DES PIÈCES", "PUZZLE 3/4: PARTS HOLD"};
constexpr Tx P_P3 = {
    "Le verrou de la soute émet une lettre en Morse. Écoutez, puis décrivez les sons à l'équipage : il a l'alphabet "
    "Morse !",
    "The parts hold lock beeps a letter in Morse code. Listen, then describe the beeps to the crew: they have the "
    "Morse code!"};
constexpr Tx P_Q3 = {"Son court = point · Son long = trait. L'équipage peut relancer le signal.",
                     "Short beep = dot · Long beep = dash. The crew can replay the signal."};
constexpr Tx P_T4 = {"ÉNIGME 4/4 : ORDINATEUR DE BORD", "PUZZLE 4/4: ON-BOARD COMPUTER"};
constexpr Tx P_P4 = {
    "Guidez l'équipage jusqu'à la sortie : il doit passer par les 8 planètes dans l'ordre, une case à la fois.",
    "Guide the crew to the exit: they must step on the 8 planets in order, one cell at a time."};
constexpr Tx P_H4 = {"Indice : de la planète la plus proche du Soleil à la plus lointaine.",
                     "Hint: from the planet closest to the Sun to the farthest one."};
constexpr Tx P_P4T = {
    "Guidez l'équipage jusqu'à la sortie : il doit passer par les 8 images dans l'ordre, une case à la fois.",
    "Guide the crew to the exit: they must go through the 8 pictures in order, one cell at a time."};
constexpr Tx P_E4 = {"Mauvaise case : −10 s et retour à l'entrée.", "Wrong cell: −10 s and back to the entrance."};
constexpr Tx P_PLANETS[8] = {{"Mercure", "Mercury"}, {"Vénus", "Venus"},     {"Terre", "Earth"},   {"Mars", "Mars"},
                             {"Jupiter", "Jupiter"}, {"Saturne", "Saturn"}, {"Uranus", "Uranus"}, {"Neptune", "Neptune"}};
constexpr Tx P_TC = {"ORDINATEUR DE BORD : TABLE DE DÉCODAGE", "ON-BOARD COMPUTER: DECODING TABLE"};
constexpr Tx P_PC = {"L'équipage vous donne une lettre du code : décrivez-lui le symbole correspondant.",
                     "The crew gives you a letter of the code: describe the matching symbol to them."};
constexpr Tx P_SA = {"Symboles tapés", "Symbols typed"};
constexpr Tx P_TAB = {"TAB : consignes", "TAB: instructions"};
constexpr Tx P_TAB_CLOSE = {"TAB : fermer", "TAB: close"};
constexpr Tx P_TAB_SCROLL = {K_UP K_DOWN " : défiler|TAB : fermer", K_UP K_DOWN ": scroll|TAB: close"};

}  // namespace textes
