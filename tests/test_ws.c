/* Client WebSocket : poignee de main et trames.
 *
 * La reponse du serveur est recopiee de WebSocketsServer.cpp
 * (links2004/arduinoWebSockets 2.7.3, celle compilee dans le jeu) : c'est
 * elle, octet pour octet, que le Cardputer envoie. Les valeurs de la
 * RFC 6455 (cle d'exemple, Accept) ont ete recalculees en Python. */
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "ws.h"

/* ------------------------------------------------- enregistreur de messages */

#define MAX_EV 64

typedef struct {
    int      opcode;
    size_t   len;
    uint32_t sum;        /* empreinte du contenu */
    int      terminated; /* data[len] == 0 */
    char     head[32];   /* debut du contenu, pour les messages courts */
} ev_t;

typedef struct {
    ev_t ev[MAX_EV];
    int  n;
} rec_t;

static uint32_t fnv(const uint8_t *d, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++)
        h = (h ^ d[i]) * 16777619u;
    return h;
}

static void on_msg(void *ctx, int opcode, const uint8_t *data, size_t len)
{
    rec_t *r = ctx;
    if (r->n >= MAX_EV)
        return;
    ev_t *e = &r->ev[r->n++];
    e->opcode = opcode;
    e->len = len;
    e->sum = fnv(data, len);
    e->terminated = data[len] == 0;
    size_t k = len < sizeof(e->head) - 1 ? len : sizeof(e->head) - 1;
    memcpy(e->head, data, k);
    e->head[k] = 0;
}

static int same(const rec_t *a, const rec_t *b)
{
    if (a->n != b->n)
        return 0;
    for (int i = 0; i < a->n; i++)
        if (a->ev[i].opcode != b->ev[i].opcode || a->ev[i].len != b->ev[i].len ||
            a->ev[i].sum != b->ev[i].sum)
            return 0;
    return 1;
}

/* --------------------------------------- trames du serveur (non masquees) */

static size_t srv_frame(uint8_t *out, int fin, int opcode, const uint8_t *p, size_t len)
{
    size_t h = 0;
    out[h++] = (uint8_t)((fin ? 0x80 : 0) | opcode);
    if (len < 126) {
        out[h++] = (uint8_t)len;
    } else if (len <= 0xFFFF) {
        out[h++] = 126;
        out[h++] = (uint8_t)(len >> 8);
        out[h++] = (uint8_t)len;
    } else {
        out[h++] = 127;
        for (int i = 0; i < 8; i++)
            out[h++] = (uint8_t)((uint64_t)len >> (56 - 8 * i));
    }
    memcpy(out + h, p, len);
    return h + len;
}

/* Rejoue un flux en morceaux de taille chunk (0 = tout d'un coup). */
static int feed_chunks(ws_rx_t *rx, const uint8_t *s, size_t n, size_t chunk, rec_t *r)
{
    if (chunk == 0)
        return ws_rx_feed(rx, s, n, on_msg, r);
    for (size_t i = 0; i < n; i += chunk) {
        size_t k = i + chunk > n ? n - i : chunk;
        int e = ws_rx_feed(rx, s + i, k, on_msg, r);
        if (e < 0)
            return e;
    }
    return 0;
}

static ws_rx_t rx;   /* 16 Ko : pas sur la pile */

/* Rend l'erreur produite par un flux donne d'un coup. */
static int stream_error(const uint8_t *s, size_t n)
{
    rec_t r = {0};
    ws_rx_init(&rx);
    return ws_rx_feed(&rx, s, n, on_msg, &r);
}

/* ------------------------------------------------------------------ tests */

static void test_handshake(void)
{
    /* "the sample nonce", les 16 octets de l'exemple de la RFC */
    uint8_t nonce[16];
    memcpy(nonce, "the sample nonce", 16);
    /* RFC 6455 section 1.3 : l'Accept attendu (hs.accept) est verifie plus bas */
    ws_hs_t hs;
    char req[512];
    int n = ws_hs_request(&hs, req, sizeof(req), "192.168.1.42", 81, "/", nonce);
    const char *expect =
        "GET / HTTP/1.1\r\n"
        "Host: 192.168.1.42:81\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    CHECK(n == (int)strlen(expect));
    CHECK(strcmp(req, expect) == 0);
    CHECK(strcmp(hs.accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == 0);
    CHECK_EQ_INT(ws_hs_request(&hs, req, 40, "192.168.1.42", 81, "/", nonce), WS_ERR_NOROOM);

    /* reponse du Cardputer, suivie du ping vide qu'il envoie aussitot */
    const char *resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Server: arduino-WebSocketsServer\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
        "\r\n";
    size_t rlen = strlen(resp);
    uint8_t wire[256];
    memcpy(wire, resp, rlen);
    wire[rlen] = 0x89;      /* FIN + PING */
    wire[rlen + 1] = 0x00;  /* vide, non masque */
    size_t wlen = rlen + 2;

    /* d'un coup */
    size_t used;
    ws_hs_request(&hs, req, sizeof(req), "192.168.1.42", 81, "/", nonce);
    CHECK(ws_hs_feed(&hs, wire, wlen, &used) > 0);
    CHECK_EQ_INT(used, rlen);

    /* coupe a chaque position possible : jamais de faux depart, et la fin
     * des en-tetes tombe toujours au meme octet */
    for (size_t cut = 1; cut < wlen; cut++) {
        ws_hs_request(&hs, req, sizeof(req), "192.168.1.42", 81, "/", nonce);
        int r1 = ws_hs_feed(&hs, wire, cut, &used);
        if (cut < rlen) {
            CHECK_EQ_INT(r1, 0);
            CHECK_EQ_INT(used, cut);
            int r2 = ws_hs_feed(&hs, wire + cut, wlen - cut, &used);
            CHECK(r2 > 0);
            CHECK_EQ_INT(cut + used, rlen);
        } else {
            CHECK(r1 > 0);
            CHECK_EQ_INT(used, rlen);
        }
    }

    /* puis le flux qui suit les en-tetes : le ping est bien vu */
    {
        rec_t r = {0};
        ws_rx_init(&rx);
        CHECK_EQ_INT(ws_rx_feed(&rx, wire + rlen, 2, on_msg, &r), 0);
        CHECK_EQ_INT(r.n, 1);
        CHECK_EQ_INT(r.ev[0].opcode, WS_OP_PING);
        CHECK_EQ_INT(r.ev[0].len, 0);
    }

    /* en-tetes en minuscules et espaces : acceptes (sans casse, RFC 7230) */
    const char *lower =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "upgrade:WebSocket\r\n"
        "connection: keep-alive, Upgrade  \r\n"
        "sec-websocket-accept:   s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
        "\r\n";
    ws_hs_request(&hs, req, sizeof(req), "h", 81, "/", nonce);
    CHECK(ws_hs_feed(&hs, (const uint8_t *)lower, strlen(lower), &used) > 0);

    /* refus */
    struct {
        const char *text;
        int err;
    } bad[] = {
        {"HTTP/1.1 404 Not Found\r\n\r\n", WS_ERR_HS_STATUS},
        {"HTTP/1.1 200 OK\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n", WS_ERR_HS_STATUS},
        {"ICY 101 x\r\n\r\n", WS_ERR_HS_STATUS},
        {"HTTP/1.1 101 x\r\nConnection: Upgrade\r\n"
         "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n", WS_ERR_HS_UPGRADE},
        {"HTTP/1.1 101 x\r\nUpgrade: websocket\r\n"
         "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n", WS_ERR_HS_UPGRADE},
        {"HTTP/1.1 101 x\r\nUpgrade: h2c\r\nConnection: Upgrade\r\n"
         "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n", WS_ERR_HS_UPGRADE},
        {"HTTP/1.1 101 x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n", WS_ERR_HS_ACCEPT},
        {"HTTP/1.1 101 x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
         "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo\r\n\r\n", WS_ERR_HS_ACCEPT},
        {"HTTP/1.1 101 x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
         "Sec-WebSocket-Accept: S3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n", WS_ERR_HS_ACCEPT},
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        ws_hs_request(&hs, req, sizeof(req), "h", 81, "/", nonce);
        CHECK_EQ_INT(ws_hs_feed(&hs, (const uint8_t *)bad[i].text, strlen(bad[i].text), &used),
                     bad[i].err);
    }

    /* reponse sans fin : refusee au lieu de deborder */
    static uint8_t junk[3000];
    memset(junk, 'x', sizeof(junk));
    ws_hs_request(&hs, req, sizeof(req), "h", 81, "/", nonce);
    CHECK_EQ_INT(ws_hs_feed(&hs, junk, sizeof(junk), &used), WS_ERR_HS_TOOLONG);
}

static uint8_t stream[64 * 1024];
static uint8_t big[WS_MAX_MSG + 8];

/* Flux type du Cardputer : ping, horloge, image de 12 Ko, sons, table... */
static size_t build_session(void)
{
    size_t n = 0;
    for (size_t i = 0; i < 12000; i++)
        big[i] = (uint8_t)(i * 7 + (i >> 8));
    n += srv_frame(stream + n, 1, WS_OP_PING, NULL, 0);
    n += srv_frame(stream + n, 1, WS_OP_TEXT, (const uint8_t *)"L,1200,fr", 9);
    n += srv_frame(stream + n, 1, WS_OP_TEXT, (const uint8_t *)"K,1200,0", 8);
    n += srv_frame(stream + n, 1, WS_OP_BINARY, big, 12000);       /* 126 + 16 bits */
    n += srv_frame(stream + n, 1, WS_OP_TEXT, (const uint8_t *)"P,1250", 6);
    n += srv_frame(stream + n, 1, WS_OP_BINARY, big + 5, 125);     /* juste sous 126 */
    n += srv_frame(stream + n, 1, WS_OP_BINARY, big + 9, 126);     /* juste a 126 */
    n += srv_frame(stream + n, 1, WS_OP_TEXT, (const uint8_t *)"", 0);
    n += srv_frame(stream + n, 1, WS_OP_TEXT, (const uint8_t *)"T,1300,1450,700,200,2", 21);
    n += srv_frame(stream + n, 1, WS_OP_PING, (const uint8_t *)"hb", 2);
    return n;
}

static void test_frames(void)
{
    size_t n = build_session();

    rec_t whole = {0};
    ws_rx_init(&rx);
    CHECK_EQ_INT(ws_rx_feed(&rx, stream, n, on_msg, &whole), 0);
    CHECK_EQ_INT(whole.n, 10);
    CHECK_EQ_INT(whole.ev[0].opcode, WS_OP_PING);
    CHECK(strcmp(whole.ev[1].head, "L,1200,fr") == 0);
    CHECK_EQ_INT(whole.ev[1].opcode, WS_OP_TEXT);
    CHECK_EQ_INT(whole.ev[3].opcode, WS_OP_BINARY);
    CHECK_EQ_INT(whole.ev[3].len, 12000);
    CHECK_EQ_INT(whole.ev[3].sum, fnv(big, 12000));
    CHECK_EQ_INT(whole.ev[5].len, 125);
    CHECK_EQ_INT(whole.ev[6].len, 126);
    CHECK_EQ_INT(whole.ev[6].sum, fnv(big + 9, 126));
    CHECK_EQ_INT(whole.ev[7].len, 0);
    CHECK(strcmp(whole.ev[8].head, "T,1300,1450,700,200,2") == 0);
    CHECK(whole.ev[8].terminated);   /* texte termine par un zero */
    CHECK_EQ_INT(whole.ev[9].opcode, WS_OP_PING);
    CHECK(strcmp(whole.ev[9].head, "hb") == 0);

    /* le reseau livre ce qu'il veut : 1 octet a la fois, 2, 3, ..., 1460
     * (un segment TCP) : toujours exactement les memes messages */
    size_t chunks[] = {1, 2, 3, 5, 7, 13, 64, 125, 126, 127, 1460, 2920, 4096};
    for (size_t c = 0; c < sizeof(chunks) / sizeof(chunks[0]); c++) {
        rec_t r = {0};
        ws_rx_init(&rx);
        CHECK_EQ_INT(feed_chunks(&rx, stream, n, chunks[c], &r), 0);
        CHECK(same(&r, &whole));
    }
    /* et des morceaux de taille au hasard */
    srand(12345);
    for (int t = 0; t < 200; t++) {
        rec_t r = {0};
        ws_rx_init(&rx);
        size_t i = 0;
        int err = 0;
        while (i < n && err == 0) {
            size_t k = 1 + (size_t)rand() % 3000;
            if (k > n - i)
                k = n - i;
            err = ws_rx_feed(&rx, stream + i, k, on_msg, &r);
            i += k;
        }
        CHECK_EQ_INT(err, 0);
        CHECK(same(&r, &whole));
    }
}

static void test_fragments(void)
{
    /* texte en trois morceaux avec un ping au milieu (autorise par la RFC) */
    uint8_t s[256];
    size_t n = 0;
    n += srv_frame(s + n, 0, WS_OP_TEXT, (const uint8_t *)"K,10", 4);
    n += srv_frame(s + n, 1, WS_OP_PING, (const uint8_t *)"x", 1);
    n += srv_frame(s + n, 0, WS_OP_CONT, (const uint8_t *)",1,2", 4);
    n += srv_frame(s + n, 1, WS_OP_CONT, (const uint8_t *)"0000,0,0,", 9);
    n += srv_frame(s + n, 1, WS_OP_TEXT, (const uint8_t *)"P,1", 3);
    for (size_t chunk = 0; chunk <= 7; chunk++) {
        rec_t r = {0};
        ws_rx_init(&rx);
        CHECK_EQ_INT(feed_chunks(&rx, s, n, chunk, &r), 0);
        CHECK_EQ_INT(r.n, 3);
        CHECK_EQ_INT(r.ev[0].opcode, WS_OP_PING);
        CHECK_EQ_INT(r.ev[1].opcode, WS_OP_TEXT);
        CHECK(strcmp(r.ev[1].head, "K,10,1,20000,0,0,") == 0);
        CHECK(r.ev[1].terminated);
        CHECK(strcmp(r.ev[2].head, "P,1") == 0);
    }
}

static void test_errors(void)
{
    uint8_t s[WS_MAX_MSG + 64];
    size_t n;

    /* trame masquee venant du serveur */
    const uint8_t masked[] = {0x81, 0x83, 1, 2, 3, 4, 'a' ^ 1, 'b' ^ 2, 'c' ^ 3};
    CHECK_EQ_INT(stream_error(masked, sizeof(masked)), WS_ERR_MASKED);

    /* bits RSV (compression non negociee) */
    const uint8_t rsv[] = {0xC1, 0x01, 'a'};
    CHECK_EQ_INT(stream_error(rsv, sizeof(rsv)), WS_ERR_RSV);

    /* codes reserves */
    const uint8_t op3[] = {0x83, 0x00};
    CHECK_EQ_INT(stream_error(op3, sizeof(op3)), WS_ERR_OPCODE);
    const uint8_t opb[] = {0x8B, 0x00};
    CHECK_EQ_INT(stream_error(opb, sizeof(opb)), WS_ERR_OPCODE);

    /* controle de plus de 125 octets, ou fragmente */
    memset(big, 'p', 200);
    n = srv_frame(s, 1, WS_OP_PING, big, 126);
    CHECK_EQ_INT(stream_error(s, n), WS_ERR_CONTROL);
    n = srv_frame(s, 1, WS_OP_PING, big, 125);
    CHECK_EQ_INT(stream_error(s, n), 0);
    n = srv_frame(s, 0, WS_OP_PING, big, 1);
    CHECK_EQ_INT(stream_error(s, n), WS_ERR_CONTROL);
    n = srv_frame(s, 0, WS_OP_CLOSE, big, 0);
    CHECK_EQ_INT(stream_error(s, n), WS_ERR_CONTROL);

    /* suite sans debut ; nouveau message avant la fin du precedent */
    n = srv_frame(s, 1, WS_OP_CONT, (const uint8_t *)"x", 1);
    CHECK_EQ_INT(stream_error(s, n), WS_ERR_SEQUENCE);
    n = srv_frame(s, 0, WS_OP_TEXT, (const uint8_t *)"x", 1);
    n += srv_frame(s + n, 1, WS_OP_BINARY, (const uint8_t *)"y", 1);
    CHECK_EQ_INT(stream_error(s, n), WS_ERR_SEQUENCE);

    /* trop gros : d'un bloc, en morceaux, et longueur 64 bits absurde */
    memset(big, 'z', sizeof(big));
    n = srv_frame(s, 1, WS_OP_BINARY, big, WS_MAX_MSG);
    CHECK_EQ_INT(stream_error(s, n), 0);
    n = srv_frame(s, 1, WS_OP_BINARY, big, WS_MAX_MSG + 1);
    CHECK_EQ_INT(stream_error(s, n), WS_ERR_TOOBIG);
    n = srv_frame(s, 0, WS_OP_BINARY, big, WS_MAX_MSG - 10);
    n += srv_frame(s + n, 1, WS_OP_CONT, big, 11);
    CHECK_EQ_INT(stream_error(s, n), WS_ERR_TOOBIG);
    const uint8_t huge[] = {0x82, 127, 0x80, 0, 0, 0, 0, 0, 0, 1};
    CHECK_EQ_INT(stream_error(huge, sizeof(huge)), WS_ERR_TOOBIG);

    /* longueur 64 bits non minimale mais valide : acceptee */
    const uint8_t long8[] = {0x81, 127, 0, 0, 0, 0, 0, 0, 0, 2, 'o', 'k'};
    {
        rec_t r = {0};
        ws_rx_init(&rx);
        CHECK_EQ_INT(ws_rx_feed(&rx, long8, sizeof(long8), on_msg, &r), 0);
        CHECK_EQ_INT(r.n, 1);
        CHECK(strcmp(r.ev[0].head, "ok") == 0);
    }

    /* fermeture du serveur : remise telle quelle */
    const uint8_t close[] = {0x88, 0x02, 0x03, 0xE8};
    {
        rec_t r = {0};
        ws_rx_init(&rx);
        CHECK_EQ_INT(ws_rx_feed(&rx, close, sizeof(close), on_msg, &r), 0);
        CHECK_EQ_INT(r.n, 1);
        CHECK_EQ_INT(r.ev[0].opcode, WS_OP_CLOSE);
        CHECK_EQ_INT(r.ev[0].len, 2);
    }
}

/* Les trames du client, relues comme le ferait le serveur. */
static void test_build(void)
{
    static uint8_t out[70000], payload[70000];
    const uint8_t mask[4] = {0x37, 0xFA, 0x21, 0x3D};
    for (size_t i = 0; i < sizeof(payload); i++)
        payload[i] = (uint8_t)(i * 13);

    size_t lens[] = {0, 1, 2, 125, 126, 127, 65535, 65536, 69000};
    size_t hdrs[] = {2, 2, 2, 2, 4, 4, 4, 10, 10};
    for (size_t t = 0; t < sizeof(lens) / sizeof(lens[0]); t++) {
        size_t len = lens[t];
        int n = ws_build_frame(out, sizeof(out), WS_OP_PONG, payload, len, mask);
        CHECK_EQ_INT(n, hdrs[t] + 4 + len);
        CHECK_EQ_INT(out[0], 0x80 | WS_OP_PONG);
        CHECK(out[1] & 0x80);   /* masque, obligatoire pour un client */
        uint64_t got = out[1] & 0x7F;
        if (got == 126)
            got = (uint64_t)out[2] << 8 | out[3];
        else if (got == 127) {
            got = 0;
            for (int i = 2; i < 10; i++)
                got = got << 8 | out[i];
        }
        CHECK_EQ_INT(got, len);
        const uint8_t *m = out + hdrs[t];
        CHECK(memcmp(m, mask, 4) == 0);
        int ok = 1;
        for (size_t i = 0; i < len; i++)
            if ((uint8_t)(m[4 + i] ^ mask[i & 3]) != payload[i])
                ok = 0;
        CHECK(ok);
    }
    CHECK_EQ_INT(ws_build_frame(out, 6, WS_OP_PONG, payload, 1, mask), WS_ERR_NOROOM);
    CHECK_EQ_INT(ws_build_frame(out, 7, WS_OP_PONG, payload, 1, mask), 7);
}

int main(void)
{
    test_handshake();
    test_frames();
    test_fragments();
    test_errors();
    test_build();
    return check_report("websocket");
}
