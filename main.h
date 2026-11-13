#ifndef LAB4_MAIN_H
#define LAB4_MAIN_H

#include <stddef.h>
#include <stdint.h>

#include "simlib.h"

typedef enum {
  SOURCE_FINITE,
  SOURCE_BACKLOGGED
} Source_Mode;

typedef struct {
  Source_Mode source;
  uint64_t packet_limit;
  uint64_t send_window;
  uint64_t receive_window;
  uint64_t sequence_space;
  double duration;
  double data_tx_time;
  double ack_tx_time;
  double propagation_delay;
  double timeout;
  double data_error;
  double ack_error;
  unsigned seed;
} Config;

typedef struct _packet_state_ {
  uint64_t id;
  uint64_t attempts;
  int received;
  int delivered;
  int acknowledged;
  double created_at;
  double acknowledged_at;
  long int timer_event_id;
} Packet_State;

typedef struct _transmission_attempt_ {
  Packet_State *packet;
  double start_time;
  struct _transmission_attempt_ *next;
  struct _transmission_attempt_ *previous;
} Transmission_Attempt;

typedef struct _simulation_state_ {
  Config config;
  Simulation_Run_Ptr run;
  Rand_Stream_Ptr random;
  Packet_State **packets;
  size_t packet_capacity;
  uint64_t next_to_send;
  uint64_t sender_base;
  uint64_t receiver_base;
  uint64_t generated;
  uint64_t delivered;
  uint64_t acknowledged;
  uint64_t unique_packets_transmitted;
  uint64_t retransmissions;
  uint64_t data_losses;
  uint64_t ack_transmissions;
  uint64_t ack_losses;
  double accumulated_delay;
  double data_tx_available;
  double ack_tx_available;
  long int go_back_n_timer_id;
  uint64_t go_back_n_timer_packet;
  Transmission_Attempt *attempts;
} Simulation_State;

Simulation_State *
state_from_run(Simulation_Run_Ptr);

Packet_State *
get_packet(Simulation_State *, uint64_t);

long int
schedule_event(Simulation_State *,
               const char *,
               void (*)(Simulation_Run_Ptr, void *),
               void *,
               double);

void
cancel_event(Simulation_State *, long int *);

#endif
