#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cleanup.h"
#include "main.h"
#include "packet_transmission.h"
#include "simparameters.h"
#include "output.h"

static void
usage(FILE *stream, const char *program)
{
  fprintf(stream,
          "Usage: %s [options]\n"
          "  --source finite|backlogged   Source model (default: finite)\n"
          "  --packets N                  Finite packet count (default: %llu)\n"
          "  --duration T                 Backlogged run duration (default: %.1f)\n"
          "  --ws N                       Sender window (default: %llu)\n"
          "  --wr N                       Receiver window (default: %llu)\n"
          "  --seq-space N                Sequence-number space (default: %llu)\n"
          "  --timeout T                  Retransmission timeout (default: %.1f)\n"
          "  --data-tx T                  Data-frame transmission time (default: %.1f)\n"
          "  --ack-tx T                   ACK transmission time (default: %.1f)\n"
          "  --prop-delay T               One-way propagation delay (default: %.1f)\n"
          "  --data-error P               Independent data-frame loss probability\n"
          "  --ack-error P                Independent ACK loss probability\n"
          "  --seed N                     Random seed (default: %u)\n"
          "  --help                       Show this help\n",
          program, DEFAULT_PACKET_LIMIT, DEFAULT_DURATION,
          DEFAULT_SEND_WINDOW, DEFAULT_RECEIVE_WINDOW,
          DEFAULT_SEQUENCE_SPACE, DEFAULT_TIMEOUT, DEFAULT_DATA_TX_TIME,
          DEFAULT_ACK_TX_TIME, DEFAULT_PROPAGATION_DELAY,
          DEFAULT_RANDOM_SEED);
}

static int
parse_u64(const char *text, uint64_t *value)
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
parse_double(const char *text, double *value)
{
  char *end;

  errno = 0;
  *value = strtod(text, &end);
  return errno == 0 && text[0] != '\0' && *end == '\0' && isfinite(*value);
}

static int
parse_arguments(int argc, char **argv, Config *config)
{
  int i;

  config->source = SOURCE_FINITE;
  config->packet_limit = DEFAULT_PACKET_LIMIT;
  config->send_window = DEFAULT_SEND_WINDOW;
  config->receive_window = DEFAULT_RECEIVE_WINDOW;
  config->sequence_space = DEFAULT_SEQUENCE_SPACE;
  config->duration = DEFAULT_DURATION;
  config->data_tx_time = DEFAULT_DATA_TX_TIME;
  config->ack_tx_time = DEFAULT_ACK_TX_TIME;
  config->propagation_delay = DEFAULT_PROPAGATION_DELAY;
  config->timeout = DEFAULT_TIMEOUT;
  config->data_error = DEFAULT_DATA_ERROR;
  config->ack_error = DEFAULT_ACK_ERROR;
  config->seed = DEFAULT_RANDOM_SEED;

  for (i = 1; i < argc; i++) {
    const char *option = argv[i];
    const char *value;
    uint64_t integer;
    double real;

    if (strcmp(option, "--help") == 0) {
      usage(stdout, argv[0]);
      return 2;
    }
    if (i + 1 >= argc) {
      fprintf(stderr, "Missing value for %s\n", option);
      return 0;
    }
    value = argv[++i];

    if (strcmp(option, "--source") == 0) {
      if (strcmp(value, "finite") == 0)
        config->source = SOURCE_FINITE;
      else if (strcmp(value, "backlogged") == 0)
        config->source = SOURCE_BACKLOGGED;
      else {
        fprintf(stderr, "Invalid source mode: %s\n", value);
        return 0;
      }
    } else if (strcmp(option, "--packets") == 0 ||
               strcmp(option, "--ws") == 0 ||
               strcmp(option, "--wr") == 0 ||
               strcmp(option, "--seq-space") == 0 ||
               strcmp(option, "--seed") == 0) {
      if (!parse_u64(value, &integer)) {
        fprintf(stderr, "Invalid integer for %s: %s\n", option, value);
        return 0;
      }
      if (strcmp(option, "--packets") == 0)
        config->packet_limit = integer;
      else if (strcmp(option, "--ws") == 0)
        config->send_window = integer;
      else if (strcmp(option, "--wr") == 0)
        config->receive_window = integer;
      else if (strcmp(option, "--seq-space") == 0)
        config->sequence_space = integer;
      else if (integer > UINT32_MAX) {
        fprintf(stderr, "Seed must be at most %u\n", UINT32_MAX);
        return 0;
      } else {
        config->seed = (unsigned) integer;
      }
    } else if (strcmp(option, "--duration") == 0 ||
               strcmp(option, "--timeout") == 0 ||
               strcmp(option, "--data-tx") == 0 ||
               strcmp(option, "--ack-tx") == 0 ||
               strcmp(option, "--prop-delay") == 0 ||
               strcmp(option, "--data-error") == 0 ||
               strcmp(option, "--ack-error") == 0) {
      if (!parse_double(value, &real)) {
        fprintf(stderr, "Invalid number for %s: %s\n", option, value);
        return 0;
      }
      if (strcmp(option, "--duration") == 0)
        config->duration = real;
      else if (strcmp(option, "--timeout") == 0)
        config->timeout = real;
      else if (strcmp(option, "--data-tx") == 0)
        config->data_tx_time = real;
      else if (strcmp(option, "--ack-tx") == 0)
        config->ack_tx_time = real;
      else if (strcmp(option, "--prop-delay") == 0)
        config->propagation_delay = real;
      else if (strcmp(option, "--data-error") == 0)
        config->data_error = real;
      else
        config->ack_error = real;
    } else {
      fprintf(stderr, "Unknown option: %s\n", option);
      return 0;
    }
  }
  return 1;
}

static int
validate_config(const Config *config)
{
  if (config->packet_limit == 0 || config->send_window == 0 ||
      config->receive_window == 0 || config->sequence_space == 0) {
    fprintf(stderr, "Packet count, windows, and sequence space must be positive\n");
    return 0;
  }
  if (config->send_window > config->sequence_space ||
      config->receive_window > config->sequence_space ||
      config->send_window > config->sequence_space - config->receive_window) {
    fprintf(stderr, "Window sizes must satisfy Ws + Wr <= sequence space\n");
    return 0;
  }
  if (config->timeout <= 0.0 || config->data_tx_time <= 0.0 ||
      config->ack_tx_time < 0.0 || config->propagation_delay < 0.0 ||
      config->duration <= 0.0) {
    fprintf(stderr, "Timeout and data transmission time must be positive; "
                    "other times must be nonnegative\n");
    return 0;
  }
  if (config->data_error < 0.0 || config->data_error >= 1.0 ||
      config->ack_error < 0.0 || config->ack_error >= 1.0) {
    fprintf(stderr, "Error probabilities must be in [0, 1)\n");
    return 0;
  }
  return 1;
}

Simulation_State *
state_from_run(Simulation_Run_Ptr run)
{
  return (Simulation_State *) simulation_run_data(run);
}

Packet_State *
get_packet(Simulation_State *state, uint64_t id)
{
  Packet_State **resized;
  size_t capacity;
  Packet_State *packet;

  if (id > (uint64_t) SIZE_MAX - 1) {
    fprintf(stderr, "Packet identifier exceeds addressable memory\n");
    exit(EXIT_FAILURE);
  }
  if ((size_t) id >= state->packet_capacity) {
    capacity = state->packet_capacity == 0 ? 16 : state->packet_capacity;
    while (capacity <= (size_t) id) {
      if (capacity > SIZE_MAX / 2) {
        fprintf(stderr, "Packet state capacity overflow\n");
        exit(EXIT_FAILURE);
      }
      capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*state->packets)) {
      fprintf(stderr, "Packet state allocation size overflow\n");
      exit(EXIT_FAILURE);
    }
    resized = (Packet_State **) realloc(state->packets,
                                        capacity * sizeof(*state->packets));
    if (resized == NULL) {
      fprintf(stderr, "Unable to allocate packet state\n");
      exit(EXIT_FAILURE);
    }
    memset(resized + state->packet_capacity, 0,
           (capacity - state->packet_capacity) * sizeof(*resized));
    state->packets = resized;
    state->packet_capacity = capacity;
  }
  if (state->packets[id] == NULL) {
    packet = (Packet_State *) calloc(1, sizeof(*packet));
    if (packet == NULL) {
      fprintf(stderr, "Unable to allocate packet state\n");
      exit(EXIT_FAILURE);
    }
    packet->id = id;
    state->packets[id] = packet;
  }
  return state->packets[id];
}

long int
schedule_event(Simulation_State *state,
               const char *description,
               void (*function)(Simulation_Run_Ptr, void *),
               void *attachment,
               double time)
{
  Event event;

  event.description = description;
  event.function = function;
  event.attachment = attachment;
  return simulation_run_schedule_event(state->run, event, time);
}

void
cancel_event(Simulation_State *state, long int *event_id)
{
  if (*event_id != 0) {
    (void) simulation_run_deschedule_event(state->run, *event_id);
    *event_id = 0;
  }
}

static int
run_simulation(const Config *config)
{
  Simulation_State state;
  double end_time;

  memset(&state, 0, sizeof(state));
  state.config = *config;
  state.run = simulation_run_new();
  state.random = rand_stream_new(config->seed);
  simulation_run_attach_data(state.run, &state);

  fill_sender_window(&state, 0.0);

  if (config->source == SOURCE_FINITE) {
    while (state.acknowledged < config->packet_limit &&
           state.run->eventlist->size != 0)
      simulation_run_execute_event(state.run);
    if (state.acknowledged != config->packet_limit) {
      fprintf(stderr, "Simulation ended before all finite-source packets were acknowledged\n");
      cleanup(&state);
      return 0;
    }
    end_time = simulation_run_get_time(state.run);
  } else {
    while (state.run->eventlist->size != 0 &&
           state.run->eventlist->front_ptr->occurrence_time <=
               config->duration)
      simulation_run_execute_event(state.run);
    end_time = config->duration;
  }

  output_results(state.run, config, end_time);
  cleanup(&state);
  return 1;
}

int
main(int argc, char **argv)
{
  Config config;
  int parsed = parse_arguments(argc, argv, &config);

  if (parsed == 2)
    return EXIT_SUCCESS;
  if (!parsed || !validate_config(&config)) {
    usage(stderr, argv[0]);
    return EXIT_FAILURE;
  }
  return run_simulation(&config) ? EXIT_SUCCESS : EXIT_FAILURE;
}
