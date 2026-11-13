#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "packet_arrival.h"
#include "packet_transmission.h"

static double
next_data_start(const Simulation_State *state, double now)
{
  return state->data_tx_available > now ? state->data_tx_available : now;
}

void
send_frame(Simulation_State *state, Packet_State *packet, double now)
{
  Transmission_Attempt *attempt;
  double start_time = next_data_start(state, now);
  double end_time = start_time + state->config.data_tx_time;

  attempt = (Transmission_Attempt *) malloc(sizeof(*attempt));
  if (attempt == NULL) {
    fprintf(stderr, "Unable to allocate transmission event\n");
    exit(EXIT_FAILURE);
  }
  attempt->packet = packet;
  attempt->start_time = start_time;
  attempt->next = state->attempts;
  attempt->previous = NULL;
  if (state->attempts != NULL)
    state->attempts->previous = attempt;
  state->attempts = attempt;

  state->data_tx_available = end_time;
  schedule_event(state, "Data transmission end", transmission_end_event,
                 attempt, end_time);
}

static void
schedule_selective_repeat_timer(Simulation_State *state,
                               Packet_State *packet,
                               double now)
{
  cancel_event(state, &packet->timer_event_id);
  packet->timer_event_id =
      schedule_event(state, "Selective-repeat timeout",
                     selective_repeat_timeout_event, packet,
                     now + state->config.timeout);
}

void
schedule_go_back_n_timer(Simulation_State *state,
                         Packet_State *packet,
                         double now)
{
  if (state->go_back_n_timer_id != 0 &&
      state->go_back_n_timer_packet == packet->id)
    return;
  cancel_event(state, &state->go_back_n_timer_id);
  state->go_back_n_timer_packet = packet->id;
  state->go_back_n_timer_id =
      schedule_event(state, "Go-back-N timeout", go_back_n_timeout_event,
                     packet, now + state->config.timeout);
}

void
fill_sender_window(Simulation_State *state, double now)
{
  while (state->next_to_send - state->sender_base <
             state->config.send_window &&
         (state->config.source == SOURCE_BACKLOGGED ||
          state->next_to_send < state->config.packet_limit)) {
    Packet_State *packet = get_packet(state, state->next_to_send);

    packet->created_at =
        state->config.source == SOURCE_FINITE ? 0.0 : now;
    send_frame(state, packet, now);
    state->next_to_send++;
    state->generated++;
  }
}

void
send_acknowledgment(Simulation_State *state,
                    Packet_State *packet,
                    double now)
{
  double start_time;
  double end_time;

  if (packet == NULL)
    return;
  start_time = state->ack_tx_available > now ? state->ack_tx_available : now;
  end_time = start_time + state->config.ack_tx_time;
  state->ack_tx_available = end_time;
  schedule_event(state, "ACK transmission end", ack_transmission_end_event,
                 packet, end_time);
}

static void
release_attempt(Simulation_State *state, Transmission_Attempt *attempt)
{
  if (attempt->previous != NULL)
    attempt->previous->next = attempt->next;
  else
    state->attempts = attempt->next;
  if (attempt->next != NULL)
    attempt->next->previous = attempt->previous;
  free(attempt);
}

void
transmission_end_event(Simulation_Run_Ptr run, void *attachment)
{
  Simulation_State *state = state_from_run(run);
  Transmission_Attempt *attempt = (Transmission_Attempt *) attachment;
  Packet_State *packet = attempt->packet;
  double now = simulation_run_get_time(run);

  if (packet->acknowledged &&
      packet->acknowledged_at <= attempt->start_time) {
    release_attempt(state, attempt);
    return;
  }

  if (packet->attempts == 0)
    state->unique_packets_transmitted++;
  else
    state->retransmissions++;
  packet->attempts++;

  if (rand_stream_uniform_generator(state->random) <
      state->config.data_error) {
    state->data_losses++;
  } else {
    schedule_event(state, "Data frame arrival", data_arrival_event, packet,
                   now + state->config.propagation_delay);
  }

  if (state->config.receive_window == 1) {
    if (packet->id == state->sender_base && !packet->acknowledged)
      schedule_go_back_n_timer(state, packet, now);
  } else if (!packet->acknowledged) {
    schedule_selective_repeat_timer(state, packet, now);
  }
  release_attempt(state, attempt);
}

void
ack_transmission_end_event(Simulation_Run_Ptr run, void *attachment)
{
  Simulation_State *state = state_from_run(run);
  Packet_State *packet = (Packet_State *) attachment;

  state->ack_transmissions++;
  if (rand_stream_uniform_generator(state->random) <
      state->config.ack_error) {
    state->ack_losses++;
    return;
  }

  schedule_event(state, "ACK arrival", ack_arrival_event, packet,
                 simulation_run_get_time(run) +
                     state->config.propagation_delay);
}

void
selective_repeat_timeout_event(Simulation_Run_Ptr run, void *attachment)
{
  Simulation_State *state = state_from_run(run);
  Packet_State *packet = (Packet_State *) attachment;
  double now = simulation_run_get_time(run);

  packet->timer_event_id = 0;
  if (packet->acknowledged)
    return;
  send_frame(state, packet, now);
}

void
go_back_n_timeout_event(Simulation_Run_Ptr run, void *attachment)
{
  Simulation_State *state = state_from_run(run);
  Packet_State *base_packet = (Packet_State *) attachment;
  uint64_t id;
  double now = simulation_run_get_time(run);

  state->go_back_n_timer_id = 0;
  if (base_packet->id != state->sender_base ||
      base_packet->acknowledged)
    return;

  for (id = state->sender_base; id < state->next_to_send; id++) {
    Packet_State *packet = state->packets[id];
    if (packet != NULL && !packet->acknowledged && packet->attempts != 0)
      send_frame(state, packet, now);
  }
}
