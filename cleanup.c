#include <stdlib.h>

#include "cleanup.h"

void
cleanup(Simulation_State *state)
{
  size_t i;
  Transmission_Attempt *attempt = state->attempts;

  simulation_run_free_memory(state->run);
  while (attempt != NULL) {
    Transmission_Attempt *next = attempt->next;
    free(attempt);
    attempt = next;
  }
  for (i = 0; i < state->packet_capacity; i++)
    free(state->packets[i]);
  free(state->packets);
  free(state->random);
}
