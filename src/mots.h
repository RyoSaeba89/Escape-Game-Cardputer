// Codes de démarrage : un mot de 4 lettres tiré au hasard à chaque partie,
// jamais le même deux fois de suite (voir chooseWord() dans main.cpp). Une
// liste par langue. Les énigmes donnent les lettres dans l'ordre :
//   Cardputer seul : Morse lumineux, QCM, picross, Morse sonore
//   Multijoueur    : Morse lumineux, picross, Morse sonore, labyrinthe
// Seuls les mots qui ont un labyrinthe (laby) sortent en multijoueur : son
// chemin dessine leur dernière lettre (S, O, P ou U).
// Textes : 6 px par caractère latin (police efontJA_12). La question du QCM
// suit « Porte verrouillée ! » sur 3 lignes au plus ; une réponse tient en
// 87 px (14 caractères).
#pragma once

#include "dessins.h"

namespace mots {

// Picross 5 x 5 : la lettre à dessiner (3e du code seul, 2e en multijoueur).
// Chaque dessin n'a qu'une solution avec ses chiffres (vérifié par un
// programme qui essaie toutes les grilles).
struct Lettre {
    char lettre;
    const char *art[5];
};
const Lettre PICROSS[] = {
    {'A', {".###.", "#...#", "#####", "#...#", "#...#"}},
    {'D', {"####.", "#...#", "#...#", "#...#", "####."}},
    {'E', {"#####", "#....", "####.", "#....", "#####"}},
    {'H', {"#...#", "#...#", "#####", "#...#", "#...#"}},
    {'I', {"#####", "..#..", "..#..", "..#..", "#####"}},
    {'L', {"#....", "#....", "#....", "#....", "#####"}},
    {'N', {"#...#", "##..#", "#.#.#", "#..##", "#...#"}},
    {'O', {"#####", "#...#", "#...#", "#...#", "#####"}},
    {'P', {"####.", "#...#", "####.", "#....", "#...."}},
    {'R', {"####.", "#...#", "####.", "#..#.", "#...#"}},
    {'S', {"#####", "#....", "#####", "....#", "#####"}},
    {'T', {"#####", "..#..", "..#..", "..#..", "..#.."}},
    {'V', {"#...#", "#...#", "##.##", ".###.", "..#.."}},
};

// Labyrinthes 4 x 5 (multijoueur). 'E' entrée, 'X' sortie, '1' à '8' les
// cases à traverser dans l'ordre (pas haut, bas, gauche, droite), puis la
// sortie. Un seul chemin, qui dessine la dernière lettre du code. L'entrée et
// la sortie touchent chacune 3 cases : ni le premier ni le dernier pas ne se
// devinent sans les noms. Aucune case voisine d'une case du chemin n'est la
// suivante dans l'ordre : pas de fausse piste. Chaque chiffre est au moins
// 2 fois dans la grille. (Grilles tirées et vérifiées par un programme.)
const char *const LABY_ARES[5] = {"21E2", "3534", "4568", "3776", "1X84"};
const char *const LABY_MARS[5] = {"81E5", "7241", "2345", "5666", "3X87"};
const char *const LABY_JETS[5] = {"78X3", "6182", "5437", "5724", "6E18"};
const char *const LABY_BIPS[5] = {"38X1", "5712", "6654", "7473", "8E12"};
const char *const LABY_VOLS[5] = {"21E4", "3851", "4566", "7172", "3X88"};
const char *const LABY_CNES[5] = {"51E6", "1283", "8345", "7346", "2X87"};
const char *const LABY_ZERO[5] = {"5677", "4184", "38X5", "21E2", "6334"};
const char *const LABY_ALLO[5] = {"4327", "5811", "65E2", "78X6", "3214"};
const char *const LABY_STEP[5] = {"3456", "1377", "22X8", "8143", "6E53"};
const char *const LABY_LEAP[5] = {"3543", "1662", "77E1", "5884", "2X67"};
const char *const LABY_SHIP[5] = {"7456", "3327", "62X8", "8141", "5E47"};
const char *const LABY_TROU[5] = {"77X3", "E688", "1475", "2164", "3452"};

// Ce que le centre de contrôle voit dans les cases 1 à 8 (dans l'ordre du
// chemin) et l'indice affiché sous la grille. Pas de virgule dans les noms
// (séparateur du message M). Sans thème (ARES) : les planètes de la page.
struct Theme {
    const char *noms[8];
    uint8_t dessins[8];
    const char *indice;
};

using namespace dessins;

#define MEME8(d) {d, d, d, d, d, d, d, d}

// ---------------------------------------------------------------- anglais
const Theme THEME_MARS_EN = {
    {"March", "April", "May", "June", "July", "August", "September", "October"},
    MEME8(CALENDRIER),
    "Hint: follow the months, starting with the first month of spring."};
const Theme THEME_ZERO_EN = {
    {"VIII", "VII", "VI", "V", "IV", "III", "II", "I"},
    MEME8(HORLOGE),
    "Hint: countdown in Roman numerals, from eight to one."};
const Theme THEME_STEP = {
    {"Liftoff", "Earth orbit", "Moon orbit", "Landing", "Moonwalk", "Leave Moon", "Way home", "Splashdown"},
    {FUSEE, ORBITE_TERRE, ORBITE_LUNE, ALUNISSAGE, EMPREINTE, DEPART_LUNE, RETOUR, AMERRISSAGE},
    "Hint: the Apollo 11 mission, to the Moon and back."};
const Theme THEME_LEAP = {
    {"Sputnik", "Laika", "Gagarin", "Tereshkova", "Spacewalk", "Apollo 11", "Shuttle", "ISS"},
    {SPOUTNIK, LAIKA, CASQUE, CASQUE, SORTIE_EVA, ALUNISSAGE, NAVETTE, ISS},
    "Hint: space firsts, from oldest to newest."};
const Theme THEME_SHIP = {
    {"Raft", "Galley", "Vikings", "Caravel", "Steamboat", "Titanic", "Apollo", "Shuttle"},
    {RADEAU, GALERE, DRAKKAR, CARAVELLE, VAPEUR, TITANIC, APOLLO, NAVETTE},
    "Hint: from oldest to newest."};
const Theme THEME_JETS = {
    {"Snail", "Runner", "Cheetah", "Race car", "Airliner", "Concorde", "Rocket", "Light"},
    {ESCARGOT, COUREUR, GUEPARD, VOITURE, AVION, CONCORDE, FUSEE, LUMIERE},
    "Hint: from slowest to fastest."};

// ---------------------------------------------------------------- français
const Theme THEME_MARS_FR = {
    {"Avril", "Mai", "Juin", "Juillet", "Août", "Septembre", "Octobre", "Novembre"},
    MEME8(CALENDRIER),
    "Indice : suivez les mois, à partir d'avril."};
const Theme THEME_ZERO_FR = {
    {"VIII", "VII", "VI", "V", "IV", "III", "II", "I"},
    MEME8(HORLOGE),
    "Indice : compte à rebours en chiffres romains, de huit à un."};
const Theme THEME_TROU = {
    {"Atome", "Fourmi", "Humain", "Baleine", "Terre", "Jupiter", "Soleil", "Galaxie"},
    {ATOME, FOURMI, HUMAIN, BALEINE, TERRE, JUPITER, SOLEIL, GALAXIE},
    "Indice : du plus petit au plus grand."};
const Theme THEME_BIPS = {
    {"Do", "Ré", "Mi", "Fa", "Sol", "La", "Si", "Do aigu"},
    {NOTE_1, NOTE_2, NOTE_3, NOTE_4, NOTE_5, NOTE_6, NOTE_7, NOTE_8},
    "Indice : la gamme, du do au do aigu."};
const Theme THEME_VOLS = {
    {"Ballon", "Avion", "Spoutnik", "Gagarine", "Apollo 11", "Navette", "ISS", "Starship"},
    {MONTGOLFIERE, AVION, SPOUTNIK, CASQUE, ALUNISSAGE, NAVETTE, ISS, STARSHIP},
    "Indice : du plus ancien au plus récent."};
const Theme THEME_CNES = {
    {"Tour Eiffel", "Everest", "Avion", "Ballon", "ISS", "Galileo", "Lune", "Mars"},
    {TOUR_EIFFEL, EVEREST, AVION, MONTGOLFIERE, ISS, GALILEO, LUNE, MARS},
    "Indice : du plus bas au plus haut. Galileo est le GPS européen."};
const Theme THEME_ALLO = {
    {"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel"},
    MEME8(RADIO),
    "Indice : l'alphabet radio des pilotes et des astronautes."};

#undef MEME8

// Un code : sa question du QCM (après « Porte verrouillée ! »), ses 4
// réponses (la bonne d'abord ; mélangées à chaque partie), le bandeau de
// l'écran de fin, et en multijoueur son labyrinthe.
struct Mot {
    const char *code;
    const char *question;
    const char *reponses[4];
    const char *bandeau;
    const char *const *laby;  // nullptr : pas en multijoueur
    const Theme *theme;       // nullptr : les planètes
};

const Mot MOTS_EN[] = {
    {"NASA", "Which rover was the first to drive on Mars?",
     {"Sojourner", "Spirit", "Curiosity", "Perseverance"},
     "NASA: National Aeronautics and Space Administration, the United States space agency, founded in 1958.",
     nullptr, nullptr},
    {"ARES", "How many moons does Mars have?",
     {"2", "0", "1", "4"},
     "ARES: Ares is the Greek god of war; the Romans called him Mars, which gave the red planet its name. "
     "Ares was also the name of two NASA rockets (Ares I and Ares V) meant to take astronauts back to the Moon, "
     "then on to Mars. The project was cancelled in 2010.",
     LABY_ARES, nullptr},
    {"MARS", "Counting from the Sun, the red planet is the…",
     {"Fourth", "Second", "Third", "Fifth"},
     "MARS: the fourth planet from the Sun, named after the Roman god of war. Its soil is full of rust, which "
     "makes it red.",
     LABY_MARS, &THEME_MARS_EN},
    {"MOON", "How many people have walked on Earth's satellite?",
     {"12", "2", "6", "24"},
     "MOON: Earth's only natural satellite, 384,000 km away. Twelve astronauts walked on it between 1969 and 1972.",
     nullptr, nullptr},
    {"STAR", "Which of these is closest to Earth?",
     {"The Sun", "Sirius", "Polaris", "Vega"},
     "STAR: a huge ball of glowing gas. The Sun is a star: its light takes about 8 minutes to reach us.",
     nullptr, nullptr},
    {"RUST", "Why does Mars look red?",
     {"Iron oxide", "Hot lava", "Red algae", "Sunsets"},
     "RUST: Mars is red because its dust and rocks are full of iron oxide, in other words rust.",
     nullptr, nullptr},
    {"DUNE", "On Mars, what piles the sand into hills?",
     {"Wind", "Rain", "Rivers", "Rovers"},
     "DUNE: a hill of sand shaped by the wind. Mars has dunes everywhere, even though its air is about 100 times "
     "thinner than Earth's.",
     nullptr, nullptr},
    {"ZERO", "How many people have walked on Mars?",
     {"None yet", "2", "4", "12"},
     "ZERO: the last number of the countdown, just before lift-off. And still zero humans on Mars… maybe you'll "
     "be the first!",
     LABY_ZERO, &THEME_ZERO_EN},
    {"STEP", "On the Moon, you weigh…",
     {"6 times less", "2 times less", "The same", "6 times more"},
     "STEP: gravity on the Moon is six times weaker than on Earth, so every step turns into a bounce!",
     LABY_STEP, &THEME_STEP},
    {"LEAP", "Who said \"one small step for man\"?",
     {"Armstrong", "Aldrin", "Gagarin", "Collins"},
     "LEAP: \"That's one small step for man, one giant leap for mankind.\" Neil Armstrong's words as he set foot "
     "on the Moon in July 1969.",
     LABY_LEAP, &THEME_LEAP},
    {"SHIP", "Which company did Elon Musk found to go to space?",
     {"SpaceX", "Boeing", "Airbus", "Tesla"},
     "SHIP: short for spaceship. SpaceX is building Starship, a giant rocket designed to take people to Mars one "
     "day.",
     LABY_SHIP, &THEME_SHIP},
    {"JETS", "What pushes a rocket up?",
     {"Hot gas", "Wings", "Propellers", "Magnets"},
     "JETS: a rocket rises by pushing jets of hot gas downward. Action and reaction: that's Newton's third law!",
     LABY_JETS, &THEME_JETS},
};

const Mot MOTS_FR[] = {
    {"NASA", "Quel est le premier rover à avoir roulé sur Mars ?",
     {"Sojourner", "Spirit", "Curiosity", "Perseverance"},
     "NASA : National Aeronautics and Space Administration (Administration nationale de l'aéronautique et "
     "de l'espace), l'agence spatiale des États-Unis, créée en 1958.",
     nullptr, nullptr},
    {"ARES", "Combien de lunes a Mars ?",
     {"2", "0", "1", "4"},
     "ARES : Arès est le dieu grec de la guerre ; les Romains l'appelaient Mars, d'où le nom de la planète rouge. "
     "Ares, c'est aussi le nom de deux fusées de la NASA (Ares I et Ares V) qui devaient ramener des astronautes "
     "sur la Lune, puis les emmener vers Mars. Le projet a été abandonné en 2010.",
     LABY_ARES, nullptr},
    {"MARS", "En partant du Soleil, la planète rouge est la…",
     {"Quatrième", "Deuxième", "Troisième", "Cinquième"},
     "MARS : la quatrième planète en partant du Soleil, nommée d'après le dieu romain de la guerre. Son sol est "
     "plein de rouille, d'où sa couleur rouge.",
     LABY_MARS, &THEME_MARS_FR},
    {"LUNE", "Combien d'humains ont marché sur le satellite de la Terre ?",
     {"12", "2", "6", "24"},
     "LUNE : le seul satellite naturel de la Terre, à 384 000 km. Douze astronautes y ont marché entre 1969 et "
     "1972.",
     nullptr, nullptr},
    {"CIEL", "De quelle couleur sont les couchers de soleil sur Mars ?",
     {"Bleus", "Rouges", "Verts", "Violets"},
     "CIEL : sur Mars, le ciel est orangé le jour, et les couchers de soleil sont bleus.",
     nullptr, nullptr},
    {"VIDE", "Dans l'espace, qu'entend-on ?",
     {"Rien", "Le vent", "Un écho", "Des bips"},
     "VIDE : dans l'espace, il n'y a pas d'air pour porter les sons. En vrai, les explosions des films de "
     "science-fiction ne font aucun bruit !",
     nullptr, nullptr},
    {"LAVE", "Où est le plus grand volcan du système solaire ?",
     {"Sur Mars", "Sur Terre", "Sur la Lune", "Sur Vénus"},
     "LAVE : Olympus Mons, sur Mars, est le plus grand volcan du système solaire : environ 22 km de haut, deux "
     "fois et demie l'Everest.",
     nullptr, nullptr},
    {"ZERO", "Combien d'humains ont marché sur Mars ?",
     {"Aucun", "2", "4", "12"},
     "ZÉRO : le dernier chiffre du compte à rebours, juste avant le décollage. Et toujours zéro humain sur Mars… "
     "vous serez peut-être les premiers !",
     LABY_ZERO, &THEME_ZERO_FR},
    {"TROU", "Comment s'appelle notre galaxie ?",
     {"La Voie lactée", "Andromède", "Orion", "Cassiopée"},
     "TROU NOIR : un astre si dense que même la lumière ne peut pas en sortir. Il y en a un énorme au centre de "
     "notre galaxie.",
     LABY_TROU, &THEME_TROU},
    {"BIPS", "Quel est le premier satellite artificiel ?",
     {"Spoutnik", "Hubble", "Apollo", "Ariane"},
     "BIPS : en 1957, Spoutnik 1, le premier satellite artificiel, envoyait des bips radio que l'on captait "
     "depuis la Terre.",
     LABY_BIPS, &THEME_BIPS},
    {"VOLS", "Qui a été le premier humain dans l'espace ?",
     {"Gagarine", "Armstrong", "Pesquet", "Tintin"},
     "VOLS : le 12 avril 1961, Youri Gagarine fait le premier vol habité dans l'espace : un tour de la Terre en "
     "108 minutes.",
     LABY_VOLS, &THEME_VOLS},
    {"CNES", "Quel astronaute français est allé dans l'ISS ?",
     {"Pesquet", "Armstrong", "Gagarine", "Aldrin"},
     "CNES : Centre national d'études spatiales, l'agence spatiale française, créée en 1961. Les fusées "
     "européennes décollent de Kourou, en Guyane.",
     LABY_CNES, &THEME_CNES},
    {"ALLO", "Où est le centre de contrôle des vols habités de la NASA ?",
     {"Houston", "Paris", "Kourou", "Moscou"},
     "ALLÔ : « Houston, nous avons eu un problème. » En 1970, l'équipage d'Apollo 13 appelle le centre de "
     "contrôle après l'explosion d'un réservoir d'oxygène. Il rentre sain et sauf.",
     LABY_ALLO, &THEME_ALLO},
};

// Liste de la langue (0 français, 1 anglais) et son nombre de mots
inline const Mot *liste(int lang, int &n) {
    if (lang) {
        n = sizeof(MOTS_EN) / sizeof(MOTS_EN[0]);
        return MOTS_EN;
    }
    n = sizeof(MOTS_FR) / sizeof(MOTS_FR[0]);
    return MOTS_FR;
}

// Dessin du picross pour une lettre (celles de PICROSS seulement)
inline const char *const *picross(char l) {
    for (const Lettre &x : PICROSS) {
        if (x.lettre == l) {
            return x.art;
        }
    }
    return PICROSS[0].art;
}

}  // namespace mots
