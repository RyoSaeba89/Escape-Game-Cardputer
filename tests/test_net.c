/* Recherche des equipages (net.c) face a un faux equipage Android sur
 * 127.0.0.7 (ports 8081 et 8080, /sym.js), et adresses tapees (addr.c).
 * Linux seulement : toute adresse 127.x.x.x y est locale et un port ferme
 * repond tout de suite, la recherche du /24 prend moins d'une seconde. */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "addr.h"
#include "check.h"
#include "net.h"

static const uint8_t SELF[4] = {127, 0, 0, 1};
static const uint8_t CREW[4] = {127, 0, 0, 7};
static const uint8_t NOBODY[4] = {127, 0, 0, 9};

static int listen_on(const uint8_t ip[4], int port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)port);
    memcpy(&a.sin_addr.s_addr, ip, 4);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(s, 64) < 0)
        return -1;
    return s;
}

/* La page de l'equipage : /sym.js pour toute requete */
static void *page_server(void *arg)
{
    int s = *(int *)arg;
    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0)
            continue;
        char buf[512];
        if (recv(c, buf, sizeof(buf), 0) > 0) {
            const char *r = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nconst SYM=[];";
            send(c, r, strlen(r), 0);
        }
        close(c);
    }
    return NULL;
}

/* Le flux : accepte et referme (la recherche ne fait que s'y connecter) */
static void *stream_server(void *arg)
{
    int s = *(int *)arg;
    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c >= 0)
            close(c);
    }
    return NULL;
}

/* Attend la fin de la recherche (20 s au plus) ; rend le nombre trouve */
static int wait_done(found_t *f)
{
    int n = 0;
    for (int i = 0; i < 2000; i++) {
        if (finder_poll(f, &n) == FINDER_DONE)
            return n;
        struct timespec t = {0, 10 * 1000000L};
        nanosleep(&t, NULL);
    }
    CHECK(!"recherche jamais finie");
    return -1;
}

static void test_finder(void)
{
    static int page, stream;
    page = listen_on(CREW, 8080);
    stream = listen_on(CREW, PORT_ANDROID);
    CHECK(page >= 0 && stream >= 0);
    pthread_t t;
    pthread_create(&t, NULL, page_server, &page);
    pthread_create(&t, NULL, stream_server, &stream);

    found_t f[FINDER_MAX];

    /* tout le reseau : l'equipage, sur le port d'Android */
    finder_start(SELF, NULL);
    CHECK_EQ_INT(wait_done(f), 1);
    CHECK(memcmp(f[0].ip, CREW, 4) == 0);
    CHECK_EQ_INT(f[0].port, PORT_ANDROID);

    /* une adresse seule */
    finder_start(SELF, CREW);
    CHECK_EQ_INT(wait_done(f), 1);
    finder_start(SELF, NOBODY);
    CHECK_EQ_INT(wait_done(f), 0);

    /* adresse tapee pendant la recherche du reseau : c'est elle qui compte
     * (avant la v4.0.0, elle etait ignoree et l'equipage .7 etait trouve) */
    finder_start(SELF, NULL);
    finder_start(SELF, NOBODY);
    CHECK_EQ_INT(wait_done(f), 0);
    finder_start(SELF, NULL);
    finder_start(SELF, CREW);
    CHECK_EQ_INT(wait_done(f), 1);
    CHECK(memcmp(f[0].ip, CREW, 4) == 0);

    /* recherche relancee pendant une adresse seule : tout le reseau */
    finder_start(SELF, NOBODY);
    finder_start(SELF, NULL);
    CHECK_EQ_INT(wait_done(f), 1);
    CHECK_EQ_INT(finder_progress(), 100);
}

static int parse_ip(const char *s, uint8_t out[4])
{
    return ip_parse(s, strlen(s), out);
}

static void test_addr(void)
{
    uint8_t ip[4];
    const uint8_t self[4] = {192, 168, 1, 37};
    CHECK(parse_ip("192.168.1.42", ip));
    CHECK(ip[0] == 192 && ip[3] == 42);
    CHECK(!parse_ip("192.168.1.", ip));
    CHECK(!parse_ip("192.168.1.256", ip));
    CHECK(!parse_ip("192.168.01.4", ip));
    CHECK(!parse_ip("192.168.1.4.", ip));
    CHECK(!parse_ip("1921.168.1.4", ip));
    CHECK(!parse_ip("", ip));

    const uint8_t t1[4] = {192, 168, 1, 42}, t2[4] = {192, 168, 1, 37}, t3[4] = {192, 168, 1, 255},
                  t4[4] = {10, 0, 0, 5}, t5[4] = {127, 0, 0, 1};
    CHECK_EQ_INT(addr_check(t1, self), ADDR_OK);
    CHECK_EQ_INT(addr_check(t2, self), ADDR_SELF);
    CHECK_EQ_INT(addr_check(t3, self), ADDR_NET_BCAST);
    CHECK_EQ_INT(addr_check(t4, self), ADDR_OTHER_NET);
    CHECK_EQ_INT(addr_check(t5, self), ADDR_INVALID);
}

int main(void)
{
    test_addr();
    test_finder();
    return check_report("recherche");
}
