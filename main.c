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
  unsigned random_seed;
  size_t seed_index;
  int parse_result = parameters_parse(argc, argv, &parameters);

  if (parse_result == 2)
    return EXIT_SUCCESS;
  if (!parse_result) {
    parameters_print_usage(argv[0], stderr);
    return EXIT_FAILURE;
  }

  output_print_header();
  if (parameters.seed_is_set)
    return run_simulation(&parameters, parameters.seed)
        ? EXIT_SUCCESS : EXIT_FAILURE;

  for (seed_index = 0;
       (random_seed = random_seeds[seed_index]) != 0;
       seed_index++) {
    if (!run_simulation(&parameters, random_seed))
      return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
