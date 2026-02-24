#ifndef NPROXY_SIGNAL_H
#define NPROXY_SIGNAL_H

#include "core/types.h"
#include "net/event_loop.h"

np_status_t signal_init(event_loop_t *loop, int *running_flag);

#endif
