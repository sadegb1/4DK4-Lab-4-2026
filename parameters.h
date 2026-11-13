#ifndef _PARAMETERS_H_
#define _PARAMETERS_H_

#include <stdio.h>
#include <stdint.h>

typedef enum {
  BACKLOGGED_SOURCE,
  FINITE_SOURCE
} Source_Mode;

typedef struct _simulation_parameters_ {
  Source_Mode source_mode;
  uint64_t number_of_packets;
  uint64_t runlength;
  uint64_t send_window;
  uint64_t receive_window;
  uint64_t sequence_space;
  double packet_error_probability;
  double ack_error_probability;
  double timeout;
  double propagation_delay;
  int seed_is_set;
  unsigned seed;
} Simulation_Parameters;

int
parameters_parse(int, char **, Simulation_Parameters *);

void
parameters_print_usage(const char *, FILE *);

#endif
