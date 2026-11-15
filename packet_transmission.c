#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "packet_arrival.h"
#include "packet_transmission.h"
#include "trace.h"

static void
release_attempt(Simulation_Run_Data_Ptr data,
                Transmission_Attempt_Ptr attempt)
{
  if (attempt->previous != NULL)
    attempt->previous->next = attempt->next;
  else
    data->attempts = attempt->next;
  if (attempt->next != NULL)
    attempt->next->previous = attempt->previous;
  free(attempt);
}

void
fill_sender_window(Simulation_Run_Ptr simulation_run)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  Time now = simulation_run_get_time(simulation_run);

  while (data->next_to_send - data->sender_base <
         data->parameters.send_window) {
    Packet_Ptr packet;

    if (data->parameters.source_mode == FINITE_SOURCE) {
      packet = (Packet_Ptr) fifoqueue_get(data->source_buffer);
      if (packet == NULL)
        break;
    } else {
      packet = packet_new(data, now);
    }

    data->next_to_send++;
    schedule_transmission_start_event(simulation_run, now, packet);
  }
}

void
schedule_transmission_start_event(Simulation_Run_Ptr simulation_run,
                                  Time event_time,
                                  Packet_Ptr packet)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  Transmission_Attempt_Ptr attempt;
  Time start_time = data->data_link_available > event_time
      ? data->data_link_available : event_time;

  attempt = (Transmission_Attempt_Ptr) xmalloc(sizeof(Transmission_Attempt));
  attempt->packet = packet;
  attempt->start_time = start_time;
  attempt->next = data->attempts;
  attempt->previous = NULL;
  if (data->attempts != NULL)
    data->attempts->previous = attempt;
  data->attempts = attempt;

  data->data_link_available = start_time + packet->service_time;
  schedule_event(simulation_run, "Start of data packet transmission",
                 transmission_start_event, attempt, start_time);
}

void
transmission_start_event(Simulation_Run_Ptr simulation_run, void *attachment)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  Transmission_Attempt_Ptr attempt =
      (Transmission_Attempt_Ptr) attachment;
  Packet_Ptr packet = attempt->packet;
  Time now = simulation_run_get_time(simulation_run);

  if (packet->acknowledged &&
      packet->acknowledged_time <= attempt->start_time) {
    release_attempt(data, attempt);
    return;
  }

  if (packet->transmission_attempts != 0)
    data->retransmission_attempts++;
  packet->transmission_attempts++;
  data->transmission_attempts++;
  packet->status = TRANSMITTING;

  TRACE(printf("At %.3f: Packet %llu (seq %llu) transmission attempt %llu starts\n",
               now, (unsigned long long) packet->id,
               (unsigned long long) packet->sequence_number,
               (unsigned long long) packet->transmission_attempts);)

  schedule_event(simulation_run, "End of data packet transmission",
                 transmission_end_event, attempt,
                 now + packet->service_time);
}

void
transmission_end_event(Simulation_Run_Ptr simulation_run, void *attachment)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  Transmission_Attempt_Ptr attempt =
      (Transmission_Attempt_Ptr) attachment;
  Packet_Ptr packet = attempt->packet;
  Time now = simulation_run_get_time(simulation_run);

  packet->status = WAITING;
  if (channel_data_packet_success(data->channel)) {
    TRACE(printf("At %.3f: Packet %llu transmission succeeded\n", now,
                 (unsigned long long) packet->id);)
    schedule_packet_arrival_event(
        simulation_run, now + data->parameters.propagation_delay, packet);
  } else {
    data->data_packet_errors++;
    TRACE(printf("At %.3f: Packet %llu transmission failed\n", now,
                 (unsigned long long) packet->id);)
  }

  if (!packet->acknowledged) {
    if (data->parameters.receive_window == 1) {
      if (packet->id == data->sender_base)
        schedule_go_back_n_timer(simulation_run, packet, now);
    } else {
      cancel_event(simulation_run, &packet->timeout_event_id);
      packet->timeout_event_id =
          schedule_event(simulation_run, "Timeout",
                         selective_repeat_timeout_event, packet,
                         now + data->parameters.timeout);
    }
  }
  release_attempt(data, attempt);
}

void
schedule_go_back_n_timer(Simulation_Run_Ptr simulation_run,
                         Packet_Ptr packet,
                         Time now)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);

  if (data->go_back_n_timer_event_id != 0 &&
      data->go_back_n_timer_packet_id == packet->id)
    return;
  cancel_event(simulation_run, &data->go_back_n_timer_event_id);
  data->go_back_n_timer_packet_id = packet->id;
  data->go_back_n_timer_event_id =
      schedule_event(simulation_run, "Timeout",
                     go_back_n_timeout_event, packet,
                     now + data->parameters.timeout);
}

void
selective_repeat_timeout_event(Simulation_Run_Ptr simulation_run,
                               void *attachment)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  Packet_Ptr packet = (Packet_Ptr) attachment;
  Time now = simulation_run_get_time(simulation_run);

  packet->timeout_event_id = 0;
  if (packet->acknowledged)
    return;
  data->timeout_count++;
  TRACE(printf("At %.3f: Timeout for packet %llu\n", now,
               (unsigned long long) packet->id);)
  schedule_transmission_start_event(simulation_run, now, packet);
}

void
go_back_n_timeout_event(Simulation_Run_Ptr simulation_run, void *attachment)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  Packet_Ptr base_packet = (Packet_Ptr) attachment;
  uint64_t id;
  Time now = simulation_run_get_time(simulation_run);

  data->go_back_n_timer_event_id = 0;
  if (base_packet->id != data->sender_base ||
      base_packet->acknowledged)
    return;

  data->timeout_count++;
  TRACE(printf("At %.3f: Timeout at packet %llu\n", now,
               (unsigned long long) base_packet->id);)
  for (id = data->sender_base; id < data->next_to_send; id++) {
    Packet_Ptr packet = packet_get(data, id);
    if (packet != NULL && !packet->acknowledged &&
        packet->transmission_attempts != 0)
      schedule_transmission_start_event(simulation_run, now, packet);
  }
}
