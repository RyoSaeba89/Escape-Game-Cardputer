/* Horloge du Cardputer : minimum glissant sur 40 messages, passage par zero
 * des compteurs 32 bits. */
#include "check.h"
#include "clocksync.h"

int main(void)
{
    clocksync_t c;
    clocksync_reset(&c);

    /* Cardputer en avance de 1000 ms sur nous ; delais reseau 5..30 ms */
    uint32_t dev = 50000;
    int delays[] = {30, 12, 25, 5, 18, 22};
    for (int i = 0; i < 6; i++) {
        clocksync_sample(&c, dev - 1000 + (uint32_t)delays[i], dev);
        dev += 250;
    }
    CHECK_EQ_INT((int32_t)c.off, -1000 + 5);     /* le plus petit delai */
    CHECK_EQ_INT(clocksync_to_local(&c, 60000), 60000 - 995);

    /* fenetre glissante : les 6 premiers (30 12 25 5 18 22 ms) sortent un a
     * un quand arrivent des messages a 20 ms */
#define MORE(k)                                                    \
    for (int i = 0; i < (k); i++) {                                \
        clocksync_sample(&c, dev - 1000 + 20, dev);                \
        dev += 250;                                                \
    }
    MORE(CLOCK_WINDOW - 6);                       /* fenetre pleine : 40 */
    CHECK_EQ_INT((int32_t)c.off, -995);
    MORE(3);                                      /* 30, 12, 25 sortis */
    CHECK_EQ_INT((int32_t)c.off, -995);
    MORE(1);                                      /* le 5 ms sort */
    CHECK_EQ_INT((int32_t)c.off, -1000 + 18);
    MORE(1);                                      /* le 18 ms sort, reste 20 */
    CHECK_EQ_INT((int32_t)c.off, -980);
    MORE(CLOCK_WINDOW);
    CHECK_EQ_INT((int32_t)c.off, -980);

    /* passage par zero : notre compteur et celui du Cardputer reviennent a
     * zero a des moments differents, le resultat ne bouge pas */
    clocksync_reset(&c);
    uint32_t local = 0xFFFFFF00u;
    dev = 0x7FFFFFF0u;
    for (int i = 0; i < 100; i++) {
        clocksync_sample(&c, local + 7 + (uint32_t)(i % 5), dev);
        CHECK_EQ_INT(clocksync_to_local(&c, dev), local + 7);
        local += 50;
        dev += 50;
    }
    clocksync_reset(&c);
    dev = 0xFFFFFFF0u;
    local = 100;
    for (int i = 0; i < 10; i++) {
        clocksync_sample(&c, local + 3, dev);
        CHECK_EQ_INT(clocksync_to_local(&c, dev + 150), local + 3 + 150);
        local += 40;
        dev += 40;
    }

    /* horloges presque egales : les ecarts tombent de part et d'autre de
     * zero (+4 puis -3). Compare sans signe, -3 (0xFFFFFFFD) passerait pour
     * le plus grand et l'on garderait +4. */
    clocksync_reset(&c);
    clocksync_sample(&c, 5004, 5000);
    clocksync_sample(&c, 5247, 5250);
    clocksync_sample(&c, 5501, 5500);
    CHECK_EQ_INT((int32_t)c.off, -3);
    CHECK_EQ_INT(clocksync_to_local(&c, 6000), 5997);

    /* un reset oublie tout : nouvelle connexion, Cardputer redemarre */
    clocksync_reset(&c);
    clocksync_sample(&c, 1000, 10);
    CHECK_EQ_INT(c.off, 990);
    return check_report("horloge");
}
