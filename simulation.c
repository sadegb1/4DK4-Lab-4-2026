#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cleanup.h"
#include "main.h"
#include "output.h"
#include "packet_arrival.h"
#include "packet_transmission.h"
#include "simparameters.h"
#include "simulation.h"

Simulation_Run_Data_Ptr
simulation_run_get_data(Simulation_Run_Ptr simulation_run)
{
  return (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);
}

Packet_Ptr
packet_get(Simulation_Run_Data_Ptr data, uint64_t packet_id)
{
  if (packet_id >= data->packet_capacity)
    return NULL;
  return data->packets[packet_id];
}

long int
schedule_event(Simulation_Run_Ptr simulation_run,
               const char *description,
               void (*function)(Simulation_Run_Ptr, void *),
               void *attachment,
               Time event_time)
{
  Event event;

  event.description = description;
  event.function = function;
  event.attachment = attachment;
  return simulation_run_schedule_event(simulation_run, event, event_time);
}

void
cancel_event(Simulation_Run_Ptr simulation_run, long int *event_id)
{
  if (*event_id != 0) {
    (void) simulation_run_deschedule_event(simulation_run, *event_id);
    *event_id = 0;
  }
}

static void
initialize_finite_source(Simulation_Run_Data_Ptr data)
{
  uint64_t i;

  for (i = 0; i < data->parameters.number_of_packets; i++) {
    Packet_Ptr packet = packet_new(data, 0.0);
    fifoqueue_put(data->source_buffer, packet);
  }
}

int
run_simulation(const Simulation_Parameters *parameters, unsigned random_seed)
{
  Simulation_Run_Ptr simulation_run;
  Simulation_Run_Data data;
  int finished;

  memset(&data, 0, sizeof(data));
  data.parameters = *parameters;
  data.random_seed = random_seed;

  random_generator_initialize(random_seed);
  simulation_run = simulation_run_new();
  simulation_run_set_data(simulation_run, &data);
  data.source_buffer = fifoqueue_new();
  data.channel = channel_new(parameters->packet_error_probability,
                             parameters->ack_error_probability);

  if (parameters->source_mode == FINITE_SOURCE)
    initialize_finite_source(&data);

  fill_sender_window(simulation_run);

  for (;;) {
    finished = parameters->source_mode == FINITE_SOURCE
        ? data.number_of_packets_acked >= parameters->number_of_packets
        : data.number_of_packets_processed >= parameters->runlength;
    if (finished)
      break;
    if (simulation_run->eventlist->size == 0) {
      fprintf(stderr, "Simulation event list became empty before completion\n");
      cleanup(simulation_run);
      return 0;
    }
    simulation_run_execute_event(simulation_run);
  }

  output_results(simulation_run);
  cleanup(simulation_run);
  return 1;
}
