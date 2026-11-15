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
  const unsigned recieve_windows[] = {1, 2, 4, 8, 11};
  // double pd[] = {0.01, 0.025, 0.05, 0.075, 0.1};

  
  unsigned random_seed;
  unsigned send_window;
  size_t seed_index;
  size_t receive_index;

  int parse_result = parameters_parse(argc, argv, &parameters);

  if (parse_result == 2)
    return EXIT_SUCCESS;

  if (!parse_result) {
    parameters_print_usage(argv[0], stderr);
    return EXIT_FAILURE;
  }

  // Initialize parameters for Part 3b.
  parameters.source_mode = BACKLOGGED_SOURCE;
  parameters.packet_error_probability = 0.05;
  parameters.ack_error_probability = 0.0;
  parameters.sequence_space = DEFAULT_SEQUENCE_SPACE;
  // parameters.send_window = 1;

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
    for(receive_index = 0; receive_index < sizeof(recieve_windows) / sizeof(recieve_windows[0]); receive_index++) {
        parameters.receive_window = recieve_windows[receive_index];
        
        for(send_window = 1; send_window <= 24; send_window++){
          parameters.send_window = send_window;
          // for(pd_index = 0; pd_index < sizeof(pd) / sizeof(pd[0]); pd_index++) {
            // parameters.packet_error_probability = pd[pd_index];
          

          if (!run_simulation(&parameters, random_seed))
            return EXIT_FAILURE;
        // }
          }
    }
    
  }
  return EXIT_SUCCESS;
}
