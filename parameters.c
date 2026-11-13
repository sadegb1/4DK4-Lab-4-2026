#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parameters.h"
#include "simparameters.h"

static int
parse_unsigned_value(const char *text, uint64_t *value)
{
  char *end;
  unsigned long long parsed;

  errno = 0;
  parsed = strtoull(text, &end, 10);
  if (errno != 0 || text[0] == '\0' || *end != '\0' || text[0] == '-')
    return 0;
  *value = (uint64_t) parsed;
  return 1;
}

static int
parse_real_value(const char *text, double *value)
{
  char *end;

  errno = 0;
  *value = strtod(text, &end);
  return errno == 0 && text[0] != '\0' && *end == '\0' && isfinite(*value);
}

static int
validate_parameters(const Simulation_Parameters *parameters)
{
  if (parameters->number_of_packets == 0 || parameters->runlength == 0 ||
      parameters->send_window == 0 || parameters->receive_window == 0 ||
      parameters->sequence_space == 0) {
    fprintf(stderr, "Packet counts, windows, and sequence space must be positive\n");
    return 0;
  }
  if (parameters->send_window > parameters->sequence_space ||
      parameters->receive_window > parameters->sequence_space ||
      parameters->send_window >
          parameters->sequence_space - parameters->receive_window) {
    fprintf(stderr, "Window sizes must satisfy Ws + Wr <= sequence space\n");
    return 0;
  }
  if (parameters->timeout <= 0.0 ||
      parameters->propagation_delay < 0.0) {
    fprintf(stderr, "Timeout must be positive and propagation delay nonnegative\n");
    return 0;
  }
  if (parameters->packet_error_probability < 0.0 ||
      parameters->packet_error_probability >= 1.0 ||
      parameters->ack_error_probability < 0.0 ||
      parameters->ack_error_probability >= 1.0) {
    fprintf(stderr, "Packet and ACK error probabilities must be in [0, 1)\n");
    return 0;
  }
  return 1;
}

void
parameters_print_usage(const char *program, FILE *stream)
{
  fprintf(stream,
          "Usage: %s [options]\n"
          "  --source backlogged|finite   Source model (default: backlogged)\n"
          "  --packets N                  Finite source packet count (default: %d)\n"
          "  --runlength N                Backlogged delivered-packet target (default: %d)\n"
          "  --ws N                       Sender window (default: %d)\n"
          "  --wr N                       Receiver window (default: %d)\n"
          "  --seq-space N                Sequence-number space (default: %d)\n"
          "  --pd P                       Data packet error probability\n"
          "  --pa P                       ACK error probability\n"
          "  --timeout T                  Timeout in milliseconds (default: %.1f)\n"
          "  --prop-delay T               One-way propagation in milliseconds (default: %.1f)\n"
          "  --seed N                     Use one random seed instead of the seed list\n"
          "  --help                       Show this help\n",
          program, DEFAULT_NUMBER_OF_PACKETS, RUNLENGTH,
          DEFAULT_SEND_WINDOW, DEFAULT_RECEIVE_WINDOW,
          DEFAULT_SEQUENCE_SPACE, DEFAULT_TIMEOUT, PROPAGATION_DELAY);
}

int
parameters_parse(int argc, char **argv, Simulation_Parameters *parameters)
{
  int i;

  parameters->source_mode = BACKLOGGED_SOURCE;
  parameters->number_of_packets = DEFAULT_NUMBER_OF_PACKETS;
  parameters->runlength = RUNLENGTH;
  parameters->send_window = DEFAULT_SEND_WINDOW;
  parameters->receive_window = DEFAULT_RECEIVE_WINDOW;
  parameters->sequence_space = DEFAULT_SEQUENCE_SPACE;
  parameters->packet_error_probability = DEFAULT_PD;
  parameters->ack_error_probability = DEFAULT_PA;
  parameters->timeout = DEFAULT_TIMEOUT;
  parameters->propagation_delay = PROPAGATION_DELAY;
  parameters->seed_is_set = 0;
  parameters->seed = 0;

  for (i = 1; i < argc; i++) {
    const char *option = argv[i];
    const char *value;
    uint64_t integer;
    double real;

    if (strcmp(option, "--help") == 0) {
      parameters_print_usage(argv[0], stdout);
      return 2;
    }
    if (i + 1 >= argc) {
      fprintf(stderr, "Missing value for %s\n", option);
      return 0;
    }
    value = argv[++i];

    if (strcmp(option, "--source") == 0) {
      if (strcmp(value, "backlogged") == 0)
        parameters->source_mode = BACKLOGGED_SOURCE;
      else if (strcmp(value, "finite") == 0)
        parameters->source_mode = FINITE_SOURCE;
      else {
        fprintf(stderr, "Invalid source mode: %s\n", value);
        return 0;
      }
    } else if (strcmp(option, "--packets") == 0 ||
               strcmp(option, "--runlength") == 0 ||
               strcmp(option, "--ws") == 0 ||
               strcmp(option, "--wr") == 0 ||
               strcmp(option, "--seq-space") == 0 ||
               strcmp(option, "--seed") == 0) {
      if (!parse_unsigned_value(value, &integer)) {
        fprintf(stderr, "Invalid integer for %s: %s\n", option, value);
        return 0;
      }
      if (strcmp(option, "--packets") == 0)
        parameters->number_of_packets = integer;
      else if (strcmp(option, "--runlength") == 0)
        parameters->runlength = integer;
      else if (strcmp(option, "--ws") == 0)
        parameters->send_window = integer;
      else if (strcmp(option, "--wr") == 0)
        parameters->receive_window = integer;
      else if (strcmp(option, "--seq-space") == 0)
        parameters->sequence_space = integer;
      else if (integer > UINT32_MAX) {
        fprintf(stderr, "Seed must be at most %u\n", UINT32_MAX);
        return 0;
      } else {
        parameters->seed = (unsigned) integer;
        parameters->seed_is_set = 1;
      }
    } else if (strcmp(option, "--pd") == 0 ||
               strcmp(option, "--pa") == 0 ||
               strcmp(option, "--timeout") == 0 ||
               strcmp(option, "--prop-delay") == 0) {
      if (!parse_real_value(value, &real)) {
        fprintf(stderr, "Invalid number for %s: %s\n", option, value);
        return 0;
      }
      if (strcmp(option, "--pd") == 0)
        parameters->packet_error_probability = real;
      else if (strcmp(option, "--pa") == 0)
        parameters->ack_error_probability = real;
      else if (strcmp(option, "--timeout") == 0)
        parameters->timeout = real;
      else
        parameters->propagation_delay = real;
    } else {
      fprintf(stderr, "Unknown option: %s\n", option);
      return 0;
    }
  }

  return validate_parameters(parameters);
}
