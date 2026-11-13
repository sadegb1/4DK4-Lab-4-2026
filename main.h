#ifndef _MAIN_H_
#define _MAIN_H_

#include <stddef.h>
#include <stdint.h>

#include "channel.h"
#include "parameters.h"
#include "simlib.h"
#include "simparameters.h"

typedef double Time;

typedef enum {
  WAITING,
  TRANSMITTING,
  ACKNOWLEDGED
} Packet_Status;

typedef struct _packet_ {
  uint64_t id;
  uint64_t sequence_number;
  Time arrive_time;
  Time service_time;
  Packet_Status status;
  uint64_t transmission_attempts;
  int received;
  int delivered;
  int acknowledged;
  Time acknowledged_time;
  long int timeout_event_id;
} Packet, *Packet_Ptr;

typedef struct _transmission_attempt_ {
  Packet_Ptr packet;
  Time start_time;
  struct _transmission_attempt_ *next;
  struct _transmission_attempt_ *previous;
} Transmission_Attempt, *Transmission_Attempt_Ptr;

typedef struct _simulation_run_data_ {
  Simulation_Parameters parameters;
  Packet_Ptr *packets;
  size_t packet_capacity;
  Fifoqueue_Ptr source_buffer;
  Channel_Ptr channel;
  uint64_t next_to_send;
  uint64_t sender_base;
  uint64_t receiver_base;
  uint64_t arrival_count;
  uint64_t number_of_packets_processed;
  uint64_t number_of_packets_acked;
  uint64_t data_packet_errors;
  uint64_t ack_transmission_attempts;
  uint64_t ack_errors;
  uint64_t timeout_count;
  uint64_t blip_counter;
  uint64_t random_seed;
  unsigned long long transmission_attempts;
  unsigned long long retransmission_attempts;
  double accumulated_delay;
  Time data_link_available;
  long int go_back_n_timer_event_id;
  uint64_t go_back_n_timer_packet_id;
  Transmission_Attempt_Ptr attempts;
} Simulation_Run_Data, *Simulation_Run_Data_Ptr;

Simulation_Run_Data_Ptr
simulation_run_get_data(Simulation_Run_Ptr);

Packet_Ptr
packet_get(Simulation_Run_Data_Ptr, uint64_t);

long int
schedule_event(Simulation_Run_Ptr,
               const char *,
               void (*)(Simulation_Run_Ptr, void *),
               void *,
               Time);

void
cancel_event(Simulation_Run_Ptr, long int *);

#endif /* _MAIN_H_ */
