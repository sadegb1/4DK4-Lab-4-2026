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
  const unsigned recieve_windows[] = {1,2,4,8};
  
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

  //init parameters for pt 2
  parameters.source_mode = BACKLOGGED_SOURCE;
  parameters.packet_error_probability = 0.0;
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
    for(receive_index = 0; receive_index < sizeof(recieve_windows) / sizeof(recieve_windows[0]); receive_index++) {
        parameters.receive_window = recieve_windows[receive_index];
        
        for(send_window = 1; send_window <= 24; send_window++){
          parameters.send_window = send_window;

          if (!run_simulation(&parameters, random_seed))
            return EXIT_FAILURE;
        }
    }
    
  }
  return EXIT_SUCCESS;
}
