#include "barriers.h"


unsigned long barrierCounter = 0;

struct TicketLock g_ticketlock = {.ticket = 0, .turn = 1};
fback_lock_t g_fallback_lock = {.ticket = 0, .turn = 1};
g_spec_vars_t g_specvars = {.tx_order = 1};