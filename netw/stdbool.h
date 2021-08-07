#ifndef NETW_STDBOOL_H
#define NETW_STDBOOL_H

/* Modififed from  MUSL */
#if !defined(_STDBOOL_H) && !defined(bool)
#define _STDBOOL_H

#include <stdlib.h>

#define bool size_t
#define true 1
#define false (!true)

#endif /* !defined(_STDBOOL_H) && !defined(bool) */

#endif /* NETW_STDBOOL_H */
