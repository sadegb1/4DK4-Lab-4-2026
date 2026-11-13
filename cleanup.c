#include <stdlib.h>

#include "cleanup.h"
#include "main.h"

void
cleanup(Simulation_Run_Ptr simulation_run)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  Transmission_Attempt_Ptr attempt = data->attempts;
  size_t i;

  while (fifoqueue_size(data->source_buffer) > 0)
    (void) fifoqueue_get(data->source_buffer);
  xfree(data->source_buffer);

  simulation_run_free_memory(simulation_run);

  while (attempt != NULL) {
    Transmission_Attempt_Ptr next = attempt->next;
    free(attempt);
    attempt = next;
  }
  for (i = 0; i < data->packet_capacity; i++)
    free(data->packets[i]);
  free(data->packets);
  xfree(data->channel);
}
