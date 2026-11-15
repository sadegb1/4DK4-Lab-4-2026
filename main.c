#include <stdio.h>
#include <stdlib.h>

#include "output.h"
#include "parameters.h"
#include "simparameters.h"
#include "simulation.h"

int
main(int argc, char **argv)
{
  Simulation_Parameters parameters;
  unsigned random_seeds[] = {RANDOM_SEED_LIST, 0};
  const unsigned receive_windows[] = {1, 2, 4, 8, 11};
  const double pd_values[] = {0, 0.01, 0.025, 0.05, 0.075, 0.1};

  
  unsigned random_seed;
  size_t seed_index;
  size_t receive_index;
  size_t pd_index;

  int parse_result = parameters_parse(argc, argv, &parameters);

  if (parse_result == 2)
    return EXIT_SUCCESS;

  if (!parse_result) {
    parameters_print_usage(argv[0], stderr);
    return EXIT_FAILURE;
  }

  /* Initialize the common Part 3a parameters. */
  parameters.source_mode = BACKLOGGED_SOURCE;
  parameters.ack_error_probability = 0.0;
  parameters.sequence_space = DEFAULT_SEQUENCE_SPACE;

/*
   * If --seed was provided, replace the default seed list with
   * the requested single seed.
   */

  if (parameters.seed_is_set) {
    random_seeds[0] = parameters.seed;
    random_seeds[1] = 0;

  }
    

  output_print_header();

  for (seed_index = 0; (random_seed = random_seeds[seed_index]) != 0; seed_index++) {
    /* Stop-and-Wait. */
    parameters.send_window = 1;
    parameters.receive_window = 1;
    for (pd_index = 0;
         pd_index < sizeof(pd_values) / sizeof(pd_values[0]);
         pd_index++) {
      parameters.packet_error_probability = pd_values[pd_index];
      if (!run_simulation(&parameters, random_seed))
        return EXIT_FAILURE;
    }

    /* Go-Back-N (Wr=1) and Selective Repeat (Wr>1), with Ws=11. */
    parameters.send_window = 11;
    for (receive_index = 0;
         receive_index < sizeof(receive_windows) / sizeof(receive_windows[0]);
         receive_index++) {
      parameters.receive_window = receive_windows[receive_index];
      for (pd_index = 0;
           pd_index < sizeof(pd_values) / sizeof(pd_values[0]);
           pd_index++) {
        parameters.packet_error_probability = pd_values[pd_index];
        if (!run_simulation(&parameters, random_seed))
          return EXIT_FAILURE;
      }
    }
  }
  return EXIT_SUCCESS;
}
