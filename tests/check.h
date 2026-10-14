/* Micro-harnais de test commun : compte les verifications, affiche celles
 * qui echouent, et rend un code de sortie non nul s'il y en a une. */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>
#include <stdlib.h>

static int g_checks, g_failed;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failed++;                                                      \
            printf("  ECHEC %s:%d : %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                    \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                   \
    do {                                                                     \
        long long va_ = (long long)(a), vb_ = (long long)(b);                \
        g_checks++;                                                          \
        if (va_ != vb_) {                                                    \
            g_failed++;                                                      \
            printf("  ECHEC %s:%d : %s == %lld, attendu %lld\n",             \
                   __FILE__, __LINE__, #a, va_, vb_);                        \
        }                                                                    \
    } while (0)

static int check_report(const char *name)
{
    printf("%-14s %4d verifications, %d echec(s)\n", name, g_checks, g_failed);
    return g_failed != 0;
}

#endif
