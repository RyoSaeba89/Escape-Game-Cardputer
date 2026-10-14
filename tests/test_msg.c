/* Messages texte du Cardputer et chrono des pages. Les exemples suivent les
 * snprintf de netTask() et les chaines de updatePanel() et updateBanner()
 * (diffusion.cpp et main.cpp, v2.2.1). */
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "msg.h"

static int parse(const char *s, msg_t *m)
{
    return msg_parse(s, strlen(s), m);
}

static void test_valid(void)
{
    msg_t m;

    CHECK_EQ_INT(parse("P,4294967295", &m), 0);
    CHECK_EQ_INT(m.kind, 'P');
    CHECK_EQ_INT(m.now, 4294967295u);

    CHECK_EQ_INT(parse("T,123456,123500,700,200,2", &m), 0);
    CHECK_EQ_INT(m.kind, 'T');
    CHECK_EQ_INT(m.now, 123456);
    CHECK_EQ_INT(m.at, 123500);
    CHECK_EQ_INT(m.freq, 700);
    CHECK_EQ_INT(m.dur, 200);
    CHECK_EQ_INT(m.ch, 2);

    CHECK_EQ_INT(parse("S,10,11,255", &m), 0);
    CHECK_EQ_INT(m.ch, MSG_CH_ALL);
    CHECK_EQ_INT(parse("S,10,11,4", &m), 0);
    CHECK_EQ_INT(m.ch, 4);

    CHECK_EQ_INT(parse("R,99,100", &m), 0);
    CHECK_EQ_INT(m.kind, 'R');
    CHECK_EQ_INT(m.at, 100);

    CHECK_EQ_INT(parse("L,5,fr", &m), 0);
    CHECK_EQ_INT(m.lang, 0);
    CHECK_EQ_INT(parse("L,5,en", &m), 0);
    CHECK_EQ_INT(m.lang, 1);

    /* pages sans chrono */
    CHECK_EQ_INT(parse("K,5,0", &m), 0);
    CHECK_EQ_INT(m.page, PAGE_SCREEN);
    CHECK(!page_timed(m.page));
    CHECK_EQ_INT(parse("K,5,R", &m), 0);
    CHECK_EQ_INT(m.page, PAGE_RULES);
    CHECK(!page_timed(m.page));

    /* enigmes 1 et 3 : pas de donnees, la virgule finale reste */
    CHECK_EQ_INT(parse("K,77000,1,287654,0,", &m), 0);
    CHECK_EQ_INT(m.page, PAGE_LAMP);
    CHECK(page_timed(m.page));
    CHECK_EQ_INT(m.remaining, 287654);
    CHECK_EQ_INT(m.errors, 0);
    CHECK_EQ_INT(parse("K,77000,3,200000,2,", &m), 0);
    CHECK_EQ_INT(m.page, PAGE_SOUND);
    CHECK_EQ_INT(m.errors, 2);

    /* enigme 2 : grille, indices des lignes, des colonnes, curseur */
    CHECK_EQ_INT(parse("K,9,2,250000,1,1000001000001000001000001,1/1/1/1/1,1/1/1/1/1,7", &m), 0);
    CHECK_EQ_INT(m.page, PAGE_PICROSS);
    CHECK(m.pic_grid[0] && m.pic_grid[6] && m.pic_grid[24] && !m.pic_grid[1]);
    CHECK_EQ_INT(m.pic_rows[4].n, 1);
    CHECK_EQ_INT(m.pic_rows[4].v[0], 1);
    CHECK_EQ_INT(m.pic_cursor, 7);
    CHECK_EQ_INT(parse("K,9,2,250000,1,1000001000001000001000001,1/1/1/1/1,1/1/1/1/1,24", &m), 0);
    CHECK_EQ_INT(m.pic_cursor, 24);
    CHECK_EQ_INT(parse("K,9,2,250000,1,1000001000001000001000001,1/1/1/1/1,1/1/1/1/1,0", &m), 0);
    CHECK_EQ_INT(m.pic_cursor, 0);
    /* jeu v2.2.1 : pas de curseur */
    CHECK_EQ_INT(parse("K,9,2,250000,1,1000001000001000001000001,1/1/1/1/1,1/1/1/1/1", &m), 0);
    CHECK_EQ_INT(m.pic_cursor, -1);
    CHECK_EQ_INT(parse("K,9,2,250000,1,0000000000000000000000000,5/1.1.1/0/3.1/1.3,2.2/0/1.1/4/5", &m), 0);
    CHECK_EQ_INT(m.pic_rows[0].n, 1);
    CHECK_EQ_INT(m.pic_rows[0].v[0], 5);
    CHECK_EQ_INT(m.pic_rows[1].n, 3);
    CHECK_EQ_INT(m.pic_rows[2].v[0], 0);
    CHECK_EQ_INT(m.pic_rows[3].v[1], 1);
    CHECK_EQ_INT(m.pic_cols[0].n, 2);
    CHECK_EQ_INT(m.pic_cols[3].v[0], 4);

    /* enigme 4 : grille, position, trace, mauvaise case */
    CHECK_EQ_INT(parse("K,9,4,100000,3,3E125487263175841X63,9,01100010010000000000,-1", &m), 0);
    CHECK_EQ_INT(m.page, PAGE_MAZE);
    CHECK_EQ_INT(m.maze[1], 'E');
    CHECK_EQ_INT(m.maze[17], 'X');
    CHECK_EQ_INT(m.maze_pos, 9);
    CHECK(m.maze_trail[1] && m.maze_trail[2] && m.maze_trail[9] && !m.maze_trail[3]);
    CHECK_EQ_INT(m.maze_bad, -1);
    CHECK_EQ_INT(parse("K,9,4,100000,3,3E125487263175841X63,1,00000000000000000000,19", &m), 0);
    CHECK_EQ_INT(m.maze_bad, 19);

    /* clavier code : erreurs PUIS saisis (l'ordre a change depuis la v1.8) */
    CHECK_EQ_INT(parse("K,77000,C,187654,3,2,A3Cb", &m), 0);
    CHECK_EQ_INT(m.page, PAGE_KEYPAD);
    CHECK_EQ_INT(m.remaining, 187654);
    CHECK_EQ_INT(m.errors, 3);
    CHECK_EQ_INT(m.typed, 2);
    CHECK_EQ_INT(m.ncells, 2);
    CHECK_EQ_INT(m.cells[1].letter, 'C');
    CHECK_EQ_INT(m.cells[1].sym, 11);
    CHECK_EQ_INT(parse("K,1,C,300000,0,0,A0B1N2Q3S4T5W6X7Za", &m), 0);
    CHECK_EQ_INT(m.ncells, 9);
    CHECK_EQ_INT(m.cells[8].letter, 'Z');
    CHECK_EQ_INT(m.cells[8].sym, 10);

    /* bandeau : le texte garde ses virgules ; vide = plus de bandeau */
    const char *b = "B,123,103,15,Un, deux, trois : \xC3\xA9t\xC3\xA9";
    CHECK_EQ_INT(parse(b, &m), 0);
    CHECK_EQ_INT(m.kind, 'B');
    CHECK_EQ_INT(m.now, 123);
    CHECK_EQ_INT(m.band_y, 103);
    CHECK_EQ_INT(m.band_h, 15);
    CHECK_EQ_INT(m.text_len, strlen(b) - 13);
    CHECK(m.text == b + 13);
    CHECK_EQ_INT(parse("B,123,103,15,", &m), 0);
    CHECK_EQ_INT(m.text_len, 0);
    CHECK_EQ_INT(parse("B,1,0,135,x", &m), 0);

    /* le message n'a pas forcement de zero final : seule len compte */
    const char raw[] = "P,12XYZ";
    CHECK_EQ_INT(msg_parse(raw, 4, &m), 0);
    CHECK_EQ_INT(m.now, 12);
    const char rawb[] = "B,1,2,3,abcZZZ";
    CHECK_EQ_INT(msg_parse(rawb, 11, &m), 0);
    CHECK_EQ_INT(m.text_len, 3);
}

static void test_invalid(void)
{
    msg_t m;
    const char *syntax[] = {
        "P", "P,", "P,1,2", "P,-1", "P, 1", "P,1 ", "P,4294967296", "P,12345678901",
        "T,1,2,3,4", "T,1,2,3,4,5,6", "T,1,2,65536,4,0", "T,1,2,3,65536,0", "T,1,2,x,4,0",
        "S,1,2", "R,1", "R,1,2,3", "L,1", "L,1,fra", "PP,1",
        /* pages */
        "K,1", "K,1,", "K,1,00", "K,1,0,5", "K,1,R,5", "K,1,1", "K,1,1,2", "K,1,1,2,3",
        "K,1,1,2,3,x", "K,1,3,2,3,,", "K,1,1,x,3,",
        /* l'ancien format de la v1.8 n'est plus accepte */
        "K,1,1,300000,2,3,A3Cb",
        /* picross */
        "K,1,2,2,3,", "K,1,2,2,3,000,1/1/1/1/1,1/1/1/1/1",
        "K,1,2,2,3,0000000000000000000000002,1/1/1/1/1,1/1/1/1/1",
        "K,1,2,2,3,0000000000000000000000000,1/1/1/1,1/1/1/1/1",
        "K,1,2,2,3,0000000000000000000000000,1/1/1/1/1/1,1/1/1/1/1",
        "K,1,2,2,3,0000000000000000000000000,1/1/1/1/,1/1/1/1/1",
        "K,1,2,2,3,0000000000000000000000000,1/1..1/1/1/1,1/1/1/1/1",
        "K,1,2,2,3,0000000000000000000000000,1/1/1/1/1",
        "K,1,2,2,3,0000000000000000000000000,1/1/1/1/1,1/1/1/1/1,",
        "K,1,2,2,3,0000000000000000000000000,1/1/1/1/1,1/1/1/1/1,-1",
        "K,1,2,2,3,0000000000000000000000000,1/1/1/1/1,1/1/1/1/1,x",
        "K,1,2,2,3,0000000000000000000000000,1/1/1/1/1,1/1/1/1/1,3,4",
        /* labyrinthe */
        "K,1,4,2,3,3E125487263175841X6,9,01100010010000000000,-1",
        "K,1,4,2,3,3E125487263175841X69,9,01100010010000000000,-1",
        "K,1,4,2,3,3E125487263175841X63,9,0110001001000000000,-1",
        "K,1,4,2,3,3E125487263175841X63,9,01100010010000000002,-1",
        "K,1,4,2,3,3E125487263175841X63,9,01100010010000000000,-2",
        "K,1,4,2,3,3E125487263175841X63,9,01100010010000000000",
        "K,1,4,2,3,3E125487263175841X63,-1,01100010010000000000,-1",
        /* clavier code */
        "K,1,C,2,3,4", "K,1,C,2,3,4,A", "K,1,C,2,3,4,a1",
        "K,1,C,2,3,4,A0B0C0D0E0F0G0H0I0J0",
        /* bandeau */
        "B", "B,", "B,1", "B,1,2", "B,1,2,3", "B,x,2,3,a", "B,1,-2,3,a",
    };
    for (size_t i = 0; i < sizeof(syntax) / sizeof(syntax[0]); i++) {
        int r = parse(syntax[i], &m);
        CHECK(r == MSG_ERR_SYNTAX);
        if (r != MSG_ERR_SYNTAX)
            printf("    (%s -> %d)\n", syntax[i], r);
    }
    const char *range[] = {
        "T,1,2,3,4,5", "S,1,2,5", "S,1,2,254", "L,1,de",
        "K,1,5,2,3,", "K,1,X", "K,1,C,2,3,5,A0", "K,1,C,2,3,4,Ac",
        "K,1,2,2,3,0000000000000000000000000,6/1/1/1/1,1/1/1/1/1",
        "K,1,2,2,3,0000000000000000000000000,1.1.1.1/1/1/1/1,1/1/1/1/1",
        "K,1,2,2,3,0000000000000000000000000,1/1/1/1/1,1/1/1/1/1,25",
        "K,1,4,2,3,3E125487263175841X63,20,01100010010000000000,-1",
        "K,1,4,2,3,3E125487263175841X63,9,01100010010000000000,20",
        "B,1,121,15,x", "B,1,0,136,", "B,1,103,0,x",
    };
    for (size_t i = 0; i < sizeof(range) / sizeof(range[0]); i++) {
        int r = parse(range[i], &m);
        CHECK(r == MSG_ERR_RANGE);
        if (r != MSG_ERR_RANGE)
            printf("    (%s -> %d)\n", range[i], r);
    }
    /* bandeau de plus de 599 octets (bannerText[600] du Cardputer) */
    char big[700] = "B,1,103,15,";
    memset(big + 11, 'a', 600);
    big[611] = 0;
    CHECK_EQ_INT(parse(big, &m), MSG_ERR_RANGE);
    big[610] = 0;
    CHECK_EQ_INT(parse(big, &m), 0);
    CHECK_EQ_INT(m.text_len, BANNER_MAX);

    CHECK_EQ_INT(parse("", &m), MSG_ERR_KIND);
    CHECK_EQ_INT(parse("X,1", &m), MSG_ERR_KIND);
    CHECK_EQ_INT(parse(",1", &m), MSG_ERR_KIND);
    CHECK_EQ_INT(msg_parse("\0,1", 3, &m), MSG_ERR_KIND);

    /* bruit : ne plante pas, ne lit pas au-dela de len (len est passe
     * exactement, le tampon est plus grand et rempli de '#') */
    srand(3);
    char buf[96];
    for (int t = 0; t < 400000; t++) {
        size_t n = (size_t)rand() % 80;
        memset(buf, '#', sizeof(buf));
        for (size_t i = 0; i < n; i++)
            buf[i] = "PTSRKLBM,,,0123456789abACEXR/.-x "[rand() % 33];
        if (rand() % 2 && n > 1)
            buf[1] = ',';
        if (msg_parse(buf, n, &m) == 0 && m.kind == 'B')
            CHECK(m.text + m.text_len <= buf + n);
        if (msg_parse(buf, n, &m) == 0 && m.kind == 'M' && m.theme)
            CHECK(m.hint.p + m.hint.n <= buf + n);
    }
    CHECK(1);
}

/* Theme du labyrinthe (message M, mazeThemeText() du jeu) : un theme
 * invente, pas celui d'un mot du jeu. */
static char theme_buf[4096];

static const char *theme_msg(int ndraw, const char *ranks, const char *name1, const char *hint)
{
    char *p = theme_buf;
    p += sprintf(p, "M,77,");
    for (int k = 0; k < THEME_COLOURS; k++)
        p += sprintf(p, "%02x%02x%02x", k, 0x10 + k, 0xf0 - k);
    *p++ = ',';
    for (int d = 0; d < ndraw; d++)
        for (int i = 0; i < THEME_SIZE * THEME_SIZE; i++)
            *p++ = "0123456789abcdef"[(d + i) % 16];
    p += sprintf(p, ",%s,%s,Deux,Trois,Quatre,Cinq,Six,Sept,Huit,%s", ranks, name1, hint);
    return theme_buf;
}

static void test_theme(void)
{
    msg_t m;

    /* les planetes */
    CHECK_EQ_INT(parse("M,4242", &m), 0);
    CHECK_EQ_INT(m.kind, 'M');
    CHECK_EQ_INT(m.now, 4242);
    CHECK(!m.theme);

    const char *t = theme_msg(2, "01010101", "Un", "Indice : un, deux, trois.");
    CHECK_EQ_INT(parse(t, &m), 0);
    CHECK(m.theme);
    CHECK_EQ_INT(m.now, 77);
    CHECK_EQ_INT(m.palette[0], 0x0010f0);
    CHECK_EQ_INT(m.palette[14], 0x0e1ee2);
    CHECK_EQ_INT(m.npics, 2);
    CHECK(m.pics.p == t + 5 + 6 * THEME_COLOURS + 1);
    CHECK_EQ_INT(m.pics.p[THEME_SIZE * THEME_SIZE], '1');
    CHECK_EQ_INT(m.ranks[0], 0);
    CHECK_EQ_INT(m.ranks[7], 1);
    CHECK_EQ_INT(m.names[0].n, 2);
    CHECK(memcmp(m.names[0].p, "Un", 2) == 0);
    CHECK(memcmp(m.names[7].p, "Huit", 4) == 0);
    /* l'indice garde ses virgules : tout ce qui suit la 13e */
    CHECK_EQ_INT(m.hint.n, strlen("Indice : un, deux, trois."));
    CHECK(memcmp(m.hint.p, "Indice : un, deux, trois.", m.hint.n) == 0);

    /* huit dessins, nom vide, indice vide, accents */
    CHECK_EQ_INT(parse(theme_msg(8, "01234567", "", ""), &m), 0);
    CHECK_EQ_INT(m.npics, 8);
    CHECK_EQ_INT(m.names[0].n, 0);
    CHECK_EQ_INT(m.hint.n, 0);
    CHECK_EQ_INT(parse(theme_msg(1, "00000000", "\xC3\xA9t\xC3\xA9", "Ao\xC3\xBBt"), &m), 0);

    /* rang sans dessin : la case reste vide, le message passe */
    CHECK_EQ_INT(parse(theme_msg(2, "01020101", "Un", "x"), &m), 0);
    CHECK_EQ_INT(m.ranks[3], 0xff);

    /* refuses */
    CHECK_EQ_INT(parse("M,", &m), MSG_ERR_SYNTAX);
    CHECK_EQ_INT(parse("M,x", &m), MSG_ERR_SYNTAX);
    CHECK_EQ_INT(parse(theme_msg(9, "01234567", "Un", "x"), &m), MSG_ERR_SYNTAX);
    CHECK_EQ_INT(parse(theme_msg(2, "0101010", "Un", "x"), &m), MSG_ERR_SYNTAX);
    CHECK_EQ_INT(parse(theme_msg(2, "0101010g", "Un", "x"), &m), MSG_ERR_SYNTAX);

    /* palette trop courte, pixel qui n'est pas hexa, dessin coupe */
    t = theme_msg(2, "01010101", "Un", "x");
    char bad[4096];
    strcpy(bad, t);
    bad[10] = ',';
    CHECK(parse(bad, &m) < 0);
    strcpy(bad, t);
    bad[5 + 6 * THEME_COLOURS + 1 + 3] = 'g';
    CHECK_EQ_INT(parse(bad, &m), MSG_ERR_SYNTAX);
    strcpy(bad, t);
    memmove(bad + 100, bad + 101, strlen(bad + 101) + 1);   /* 511 chiffres */
    CHECK_EQ_INT(parse(bad, &m), MSG_ERR_SYNTAX);

    /* pas tous les champs (pas d'indice) : les planetes, comme la page */
    strcpy(bad, t);
    *strrchr(bad, ',') = 0;
    CHECK_EQ_INT(parse(bad, &m), 0);
    CHECK(!m.theme);
    CHECK_EQ_INT(m.now, 77);
    CHECK_EQ_INT(parse("M,1,", &m), 0);
    CHECK(!m.theme);
}

static void test_o2(void)
{
    char t[12];
    struct {
        uint32_t ms;
        const char *s;
    } cases[] = {
        {0, "O2 00:00"},     {1, "O2 00:01"},      {999, "O2 00:01"},
        {1000, "O2 00:01"},  {1001, "O2 00:02"},   {59999, "O2 01:00"},
        {60000, "O2 01:00"}, {60001, "O2 01:01"},  {300000, "O2 05:00"},
        {187654, "O2 03:08"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        panel_o2_text(cases[i].ms, t);
        CHECK(strcmp(t, cases[i].s) == 0);
    }
    panel_o2_text(4294967295u, t);   /* absurde mais sans debordement */
    CHECK(strlen(t) == 8);
}

int main(void)
{
    test_valid();
    test_invalid();
    test_theme();
    test_o2();
    return check_report("messages");
}
