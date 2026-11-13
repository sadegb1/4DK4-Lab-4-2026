#include "main.h"
#include "packet_arrival.h"
#include "packet_transmission.h"

static void
deliver_packet(Simulation_State *state, Packet_State *packet, double now)
{
  if (!packet->delivered) {
    packet->delivered = 1;
    state->delivered++;
    state->accumulated_delay += now - packet->created_at;
  }
}

static void
advance_sender_base(Simulation_State *state, double now)
{
  uint64_t old_base = state->sender_base;

  while (state->sender_base < state->next_to_send &&
         state->packets[state->sender_base] != NULL &&
         state->packets[state->sender_base]->acknowledged)
    state->sender_base++;

  if (state->sender_base != old_base && state->config.receive_window == 1) {
    cancel_event(state, &state->go_back_n_timer_id);
    if (state->sender_base < state->next_to_send) {
      Packet_State *base_packet = state->packets[state->sender_base];
      if (base_packet->attempts != 0)
        schedule_go_back_n_timer(state, base_packet, now);
    }
  }
}

void
data_arrival_event(Simulation_Run_Ptr run, void *attachment)
{
  Simulation_State *state = state_from_run(run);
  Packet_State *packet = (Packet_State *) attachment;
  Packet_State *acknowledged_packet = NULL;
  double now = simulation_run_get_time(run);

  if (state->config.receive_window == 1) {
    if (packet->id == state->receiver_base) {
      packet->received = 1;
      deliver_packet(state, packet, now);
      state->receiver_base++;
    }
    if (state->receiver_base != 0)
      acknowledged_packet = state->packets[state->receiver_base - 1];
  } else if (packet->id < state->receiver_base) {
    acknowledged_packet = packet;
  } else if (packet->id - state->receiver_base <
             state->config.receive_window) {
    packet->received = 1;
    acknowledged_packet = packet;
    while (state->receiver_base < state->next_to_send) {
      Packet_State *next = state->packets[state->receiver_base];
      if (next == NULL || !next->received)
        break;
      deliver_packet(state, next, now);
      state->receiver_base++;
    }
  }

  send_acknowledgment(state, acknowledged_packet, now);
}

void
ack_arrival_event(Simulation_Run_Ptr run, void *attachment)
{
  Simulation_State *state = state_from_run(run);
  Packet_State *packet = (Packet_State *) attachment;
  double now = simulation_run_get_time(run);
  uint64_t id = packet->id;

  if (id >= state->next_to_send || packet->acknowledged)
    return;

  if (state->config.receive_window == 1) {
    if (id < state->sender_base)
      return;
    while (state->sender_base <= id) {
      Packet_State *cumulative = state->packets[state->sender_base];
      if (cumulative != NULL && !cumulative->acknowledged) {
        cumulative->acknowledged = 1;
        cumulative->acknowledged_at = now;
        state->acknowledged++;
      }
      state->sender_base++;
    }
    cancel_event(state, &state->go_back_n_timer_id);
    if (state->sender_base < state->next_to_send) {
      Packet_State *base_packet = state->packets[state->sender_base];
      if (base_packet->attempts != 0)
        schedule_go_back_n_timer(state, base_packet, now);
    }
  } else {
    packet->acknowledged = 1;
    packet->acknowledged_at = now;
    state->acknowledged++;
    cancel_event(state, &packet->timer_event_id);
    advance_sender_base(state, now);
  }

  fill_sender_window(state, now);
}
