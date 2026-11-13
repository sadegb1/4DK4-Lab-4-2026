#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "packet_arrival.h"
#include "packet_duration.h"
#include "packet_transmission.h"
#include "output.h"
#include "trace.h"

static void
register_packet(Simulation_Run_Data_Ptr data, Packet_Ptr packet)
{
  Packet_Ptr *resized;
  size_t capacity;

  if (packet->id > (uint64_t) SIZE_MAX - 1) {
    fprintf(stderr, "Packet identifier exceeds addressable memory\n");
    exit(EXIT_FAILURE);
  }
  if ((size_t) packet->id >= data->packet_capacity) {
    capacity = data->packet_capacity == 0 ? 16 : data->packet_capacity;
    while (capacity <= (size_t) packet->id) {
      if (capacity > SIZE_MAX / 2) {
        fprintf(stderr, "Packet state capacity overflow\n");
        exit(EXIT_FAILURE);
      }
      capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*data->packets)) {
      fprintf(stderr, "Packet state allocation size overflow\n");
      exit(EXIT_FAILURE);
    }
    resized = (Packet_Ptr *) realloc(data->packets,
                                     capacity * sizeof(*data->packets));
    if (resized == NULL) {
      fprintf(stderr, "Unable to allocate packet state\n");
      exit(EXIT_FAILURE);
    }
    memset(resized + data->packet_capacity, 0,
           (capacity - data->packet_capacity) * sizeof(*resized));
    data->packets = resized;
    data->packet_capacity = capacity;
  }
  data->packets[packet->id] = packet;
}

Packet_Ptr
packet_new(Simulation_Run_Data_Ptr data, Time arrival_time)
{
  Packet_Ptr packet = (Packet_Ptr) xcalloc(1, sizeof(Packet));

  packet->id = data->arrival_count;
  packet->sequence_number =
      packet->id % data->parameters.sequence_space;
  packet->arrive_time = arrival_time;
  packet->service_time = get_packet_duration();
  packet->status = WAITING;
  register_packet(data, packet);
  data->arrival_count++;
  return packet;
}

long int
schedule_packet_arrival_event(Simulation_Run_Ptr simulation_run,
                              Time event_time,
                              Packet_Ptr packet)
{
  return schedule_event(simulation_run, "Data packet arrival",
                        packet_arrival_event, packet, event_time);
}

static void
deliver_packet(Simulation_Run_Ptr simulation_run,
               Simulation_Run_Data_Ptr data,
               Packet_Ptr packet,
               Time now)
{
  if (!packet->delivered) {
    packet->delivered = 1;
    data->number_of_packets_processed++;
    data->accumulated_delay += now - packet->arrive_time;
    output_blip_to_screen(simulation_run);
  }
}

static void
send_acknowledgment(Simulation_Run_Ptr simulation_run,
                    Packet_Ptr packet,
                    Time now)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);

  if (packet == NULL)
    return;

  data->ack_transmission_attempts++;
  if (!channel_ack_success(data->channel)) {
    data->ack_errors++;
    TRACE(printf("At %.3f: ACK for packet %llu lost\n", now,
                 (unsigned long long) packet->id);)
    return;
  }

  TRACE(printf("At %.3f: ACK for packet %llu sent\n", now,
               (unsigned long long) packet->id);)
  schedule_event(simulation_run, "ACK arrival", acknowledgment_arrival_event,
                 packet, now + data->parameters.propagation_delay);
}

void
packet_arrival_event(Simulation_Run_Ptr simulation_run, void *attachment)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  Packet_Ptr packet = (Packet_Ptr) attachment;
  Packet_Ptr ack_packet = NULL;
  Time now = simulation_run_get_time(simulation_run);

  TRACE(printf("At %.3f: Packet %llu (seq %llu) received\n", now,
               (unsigned long long) packet->id,
               (unsigned long long) packet->sequence_number);)

  if (data->parameters.receive_window == 1) {
    if (packet->id == data->receiver_base) {
      packet->received = 1;
      deliver_packet(simulation_run, data, packet, now);
      data->receiver_base++;
    }
    if (data->receiver_base != 0)
      ack_packet = packet_get(data, data->receiver_base - 1);
    else {
      TRACE(printf("At %.3f: No cumulative ACK available yet\n", now);)
    }
  } else if (packet->id < data->receiver_base) {
    ack_packet = packet;
  } else if (packet->id - data->receiver_base <
             data->parameters.receive_window) {
    packet->received = 1;
    ack_packet = packet;
    while (data->receiver_base < data->next_to_send) {
      Packet_Ptr next = packet_get(data, data->receiver_base);
      if (next == NULL || !next->received)
        break;
      deliver_packet(simulation_run, data, next, now);
      data->receiver_base++;
    }
  } else {
    TRACE(printf("At %.3f: Packet %llu outside receive window; discarded\n",
                 now, (unsigned long long) packet->id);)
  }

  send_acknowledgment(simulation_run, ack_packet, now);
}

static void
advance_sender_base(Simulation_Run_Data_Ptr data)
{
  while (data->sender_base < data->next_to_send) {
    Packet_Ptr packet = packet_get(data, data->sender_base);
    if (packet == NULL || !packet->acknowledged)
      break;
    data->sender_base++;
  }
}

void
acknowledgment_arrival_event(Simulation_Run_Ptr simulation_run,
                             void *attachment)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  Packet_Ptr packet = (Packet_Ptr) attachment;
  Time now = simulation_run_get_time(simulation_run);
  uint64_t ack_id = packet->id;
  uint64_t old_base = data->sender_base;

  if (ack_id >= data->next_to_send || packet->acknowledged)
    return;

  TRACE(printf("At %.3f: ACK for packet %llu arrived\n", now,
               (unsigned long long) ack_id);)

  if (data->parameters.receive_window == 1) {
    if (ack_id < data->sender_base)
      return;
    while (data->sender_base <= ack_id) {
      Packet_Ptr acknowledged_packet =
          packet_get(data, data->sender_base);
      if (acknowledged_packet != NULL &&
          !acknowledged_packet->acknowledged) {
        acknowledged_packet->acknowledged = 1;
        acknowledged_packet->acknowledged_time = now;
        acknowledged_packet->status = ACKNOWLEDGED;
        data->number_of_packets_acked++;
      }
      data->sender_base++;
    }
    cancel_event(simulation_run, &data->go_back_n_timer_event_id);
    if (data->sender_base < data->next_to_send) {
      Packet_Ptr base_packet = packet_get(data, data->sender_base);
      if (base_packet != NULL && base_packet->transmission_attempts != 0)
        schedule_go_back_n_timer(simulation_run, base_packet, now);
    }
  } else {
    packet->acknowledged = 1;
    packet->acknowledged_time = now;
    packet->status = ACKNOWLEDGED;
    data->number_of_packets_acked++;
    cancel_event(simulation_run, &packet->timeout_event_id);
    advance_sender_base(data);
  }

  if (old_base != data->sender_base)
    TRACE(printf("At %.3f: Sender base advanced to %llu\n", now,
                 (unsigned long long) data->sender_base);)

  fill_sender_window(simulation_run);
}
