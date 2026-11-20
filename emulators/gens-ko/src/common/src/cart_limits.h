/* Gens KO modification, 2026-11-20. GPL-2.0 terms; see LICENSE.txt. */
#ifndef GENS_CART_LIMITS_H
#define GENS_CART_LIMITS_H

/* A bounded allocation is intentional. An unchecked "unlimited" ROM loader
 * would turn malformed files into memory corruption. 512 bytes accommodate
 * an SMD header, and two bytes accommodate an odd final word safely. */
#define GENS_ROM_MAX_SIZE (32U * 1024U * 1024U)
#define GENS_ROM_BUFFER_SIZE (GENS_ROM_MAX_SIZE + 512U + 2U)

#endif
