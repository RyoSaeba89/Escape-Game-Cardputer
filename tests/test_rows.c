/* Decodeur des lignes d'ecran.
 *
 * L'encodeur ci-dessous est une transcription d'encodeRow() et de
 * sendFrame() (diffusion.cpp, Escape-Game-Cardputer v1.8) : choix du plus
 * court entre brut et repetitions, repetitions de 255 au plus, messages de
 * 12 Ko au plus repris a la ligne qui n'a pas tenu. Le pixel est ecrit poids
 * fort d'abord, comme l'ordre memoire du sprite que le Cardputer envoie. */
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "rows.h"

#define FRAME_BUF (12 * 1024)

static uint16_t dev[SCR_H][SCR_W];    /* l'ecran du Cardputer */
static uint16_t gc[SCR_H * SCR_W];    /* la copie de la GameCube */

static size_t encode_row(int y, uint8_t *out, size_t room)
{
    const uint16_t *p = dev[y];
    size_t rle = 2;
    for (int x = 0; x < SCR_W;) {
        int n = 1;
        while (x + n < SCR_W && n < 255 && p[x + n] == p[x])
            n++;
        rle += 3;
        x += n;
    }
    size_t raw = 2 + SCR_W * 2;
    size_t need = rle < raw ? rle : raw;
    if (need > room)
        return 0;
    out[0] = (uint8_t)y;
    if (rle < raw) {
        out[1] = 1;
        uint8_t *o = out + 2;
        for (int x = 0; x < SCR_W;) {
            int n = 1;
            while (x + n < SCR_W && n < 255 && p[x + n] == p[x])
                n++;
            *o++ = (uint8_t)n;
            *o++ = (uint8_t)(p[x] >> 8);
            *o++ = (uint8_t)p[x];
            x += n;
        }
    } else {
        out[1] = 0;
        for (int x = 0; x < SCR_W; x++) {
            out[2 + 2 * x] = (uint8_t)(p[x] >> 8);
            out[3 + 2 * x] = (uint8_t)p[x];
        }
    }
    return need;
}

/* Envoie tout l'ecran (comme apres une connexion : toutes les empreintes a
 * zero), en autant de messages que necessaire. Rend le nombre de messages. */
static int send_all(int (*apply)(const uint8_t *, size_t))
{
    static uint8_t buf[FRAME_BUF];
    int sent[SCR_H] = {0};
    int msgs = 0, left = SCR_H, next = 0;
    while (left > 0) {
        size_t len = 0;
        int stop = -1;
        for (int i = 0; i < SCR_H; i++) {
            int y = (next + i) % SCR_H;
            if (sent[y])
                continue;
            size_t n = encode_row(y, buf + len, FRAME_BUF - len);
            if (n == 0) {
                stop = y;
                break;
            }
            len += n;
            sent[y] = 1;
            left--;
        }
        next = stop < 0 ? 0 : stop;
        if (len > 0) {
            msgs++;
            if (apply(buf, len) < 0)
                return -1;
        }
    }
    return msgs;
}

static rows_dirty_t dirty;

static int rows_is_dirty(const rows_dirty_t *d, int y)
{
    return d->bits[y >> 5] >> (y & 31) & 1;
}

static int apply_gc(const uint8_t *d, size_t n)
{
    return rows_apply(gc, d, n, &dirty);
}

static int screens_equal(void)
{
    return memcmp(dev, gc, sizeof(gc)) == 0;
}

/* Ecrans de test : uni, bruit pur (tout en brut), et un ecran de jeu
 * plausible (aplats et quelques details). */
static void fill_flat(uint16_t v)
{
    for (int y = 0; y < SCR_H; y++)
        for (int x = 0; x < SCR_W; x++)
            dev[y][x] = v;
}

static void fill_noise(void)
{
    for (int y = 0; y < SCR_H; y++)
        for (int x = 0; x < SCR_W; x++)
            dev[y][x] = (uint16_t)rand();
}

static void fill_game(void)
{
    for (int y = 0; y < SCR_H; y++)
        for (int x = 0; x < SCR_W; x++) {
            uint16_t v = y < 12 ? 0x0841 : 0x0000;
            if (x > 20 + y && x < 60 + y)
                v = 0xFD20;                         /* orange */
            if ((x * 7 + y * 3) % 97 == 0)
                v = (uint16_t)(rand() & 0xFFFF);    /* etoiles */
            dev[y][x] = v;
        }
}

static void test_roundtrip(void)
{
    srand(42);
    for (int t = 0; t < 4; t++) {
        if (t == 0)
            fill_flat(0xF800);
        else if (t == 1)
            fill_noise();
        else
            fill_game();
        memset(gc, 0xAB, sizeof(gc));
        memset(&dirty, 0, sizeof(dirty));
        int msgs = send_all(apply_gc);
        CHECK(msgs >= 1);
        CHECK(screens_equal());
        for (int y = 0; y < SCR_H; y++)
            CHECK(rows_is_dirty(&dirty, y));
        if (t == 1)
            CHECK(msgs >= 6);   /* 135 lignes brutes de 482 octets : 6 messages */
        if (t == 0)
            CHECK_EQ_INT(msgs, 1);
    }

    /* l'ordre des octets : 0xF800 (rouge) arrive F8 puis 00 */
    fill_flat(0xF800);
    uint8_t one[8];
    size_t n = encode_row(0, one, sizeof(one));
    CHECK_EQ_INT(n, 2 + 3);    /* 240 = 240 en une repetition */
    CHECK_EQ_INT(one[2], 240);
    CHECK_EQ_INT(one[3], 0xF8);
    CHECK_EQ_INT(one[4], 0x00);
}

/* Toute coupure d'un message valide : ce qui est applique est exactement
 * les lignes completes, rien d'autre n'est touche. */
static void test_truncation(void)
{
    static uint8_t msg[FRAME_BUF];
    srand(7);
    fill_game();
    for (int y = 0; y < 3; y++)        /* quelques lignes brutes aussi */
        for (int x = 0; x < SCR_W; x++)
            dev[y][x] = (uint16_t)rand();
    size_t len = 0, ends[SCR_H];
    int rows = 0;
    for (int y = 0; y < SCR_H; y++) {
        size_t k = encode_row(y, msg + len, sizeof(msg) - len);
        if (k == 0)
            break;
        len += k;
        ends[rows++] = len;
    }
    for (size_t cut = 0; cut < len; cut++) {
        memset(gc, 0x5A, sizeof(gc));
        memset(&dirty, 0, sizeof(dirty));
        int full = 0;
        while (full < rows && ends[full] <= cut)
            full++;
        int r = rows_apply(gc, msg, cut, &dirty);
        /* coupe pile entre deux lignes : pas d'erreur ; sinon, message court */
        int exact = cut == 0 || (full > 0 && ends[full - 1] == cut);
        CHECK_EQ_INT(r, exact ? full : ROWS_ERR_SHORT);
        int ok = 1;
        for (int y = 0; y < SCR_H; y++) {
            int applied = y < full;
            if (applied != rows_is_dirty(&dirty, y))
                ok = 0;
            for (int x = 0; x < SCR_W; x++) {
                uint16_t want = applied ? dev[y][x] : 0x5A5A;
                if (gc[y * SCR_W + x] != want)
                    ok = 0;
            }
        }
        CHECK(ok);
    }
}

static void test_malformed(void)
{
    uint8_t m[600];
    memset(gc, 0, sizeof(gc));
    memset(&dirty, 0, sizeof(dirty));

    m[0] = SCR_H; m[1] = 1; m[2] = 240; m[3] = 0; m[4] = 0;
    CHECK_EQ_INT(rows_apply(gc, m, 5, &dirty), ROWS_ERR_Y);
    m[0] = 255;
    CHECK_EQ_INT(rows_apply(gc, m, 5, &dirty), ROWS_ERR_Y);

    m[0] = 3; m[1] = 2;
    CHECK_EQ_INT(rows_apply(gc, m, 5, &dirty), ROWS_ERR_MODE);

    m[1] = 1; m[2] = 0;          /* repetition nulle : boucle sans fin evitee */
    CHECK_EQ_INT(rows_apply(gc, m, 5, &dirty), ROWS_ERR_RUN);

    m[2] = 200; m[3] = 0x12; m[4] = 0x34;   /* 200 + 41 = 241 > 240 */
    m[5] = 41; m[6] = 0; m[7] = 0;
    CHECK_EQ_INT(rows_apply(gc, m, 8, &dirty), ROWS_ERR_RUN);
    m[5] = 40;                               /* 200 + 40 = 240 : bon */
    CHECK_EQ_INT(rows_apply(gc, m, 8, &dirty), 1);
    CHECK_EQ_INT(gc[3 * SCR_W + 0], 0x1234);
    CHECK_EQ_INT(gc[3 * SCR_W + 199], 0x1234);
    CHECK_EQ_INT(gc[3 * SCR_W + 200], 0x0000);

    /* une ligne refusee apres une bonne : la bonne reste appliquee, la
     * mauvaise ne laisse rien */
    memset(gc, 0, sizeof(gc));
    m[0] = 4; m[1] = 1; m[2] = 240; m[3] = 0xAA; m[4] = 0xBB;
    m[5] = 5; m[6] = 1; m[7] = 100; m[8] = 0xCC; m[9] = 0xDD; m[10] = 200; m[11] = 0; m[12] = 0;
    CHECK_EQ_INT(rows_apply(gc, m, 13, &dirty), ROWS_ERR_RUN);
    CHECK_EQ_INT(gc[4 * SCR_W + 239], 0xAABB);
    CHECK_EQ_INT(gc[5 * SCR_W + 0], 0);

    /* message vide : rien, sans erreur */
    CHECK_EQ_INT(rows_apply(gc, m, 0, &dirty), 0);

    /* bruit au hasard : jamais d'ecriture hors de l'ecran (guard[1] est
     * l'ecran, guard[0] et guard[2] l'entourent et doivent rester intacts) */
    static uint16_t guard[3][SCR_W * SCR_H];
    srand(99);
    int errors = 0;
    for (int t = 0; t < 20000; t++) {
        size_t n = (size_t)rand() % sizeof(m);
        for (size_t i = 0; i < n; i++)
            m[i] = (uint8_t)rand();
        if (n > 0 && t % 3 == 0)
            m[0] = (uint8_t)(rand() % SCR_H);
        if (n > 1 && t % 2 == 0)
            m[1] = (uint8_t)(rand() % 2);
        memset(guard[0], 0xEE, sizeof(guard[0]));
        memset(guard[2], 0xEE, sizeof(guard[2]));
        int r = rows_apply(guard[1], m, n, &dirty);
        if (r < 0)
            errors++;
        int clean = 1;
        for (int i = 0; i < SCR_W * SCR_H; i++)
            if (guard[0][i] != 0xEEEE || guard[2][i] != 0xEEEE)
                clean = 0;
        CHECK(clean);
    }
    CHECK(errors > 0);
}

int main(void)
{
    test_roundtrip();
    test_truncation();
    test_malformed();
    return check_report("rows");
}
