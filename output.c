#include <stdio.h>

#include "main.h"
#include "output.h"

static const char *
protocol_name(const Config *config)
{
  if (config->send_window == 1 && config->receive_window == 1)
    return "stop-and-wait";
  if (config->receive_window == 1)
    return "go-back-n";
  return "selective-repeat";
}

void
output_results(Simulation_Run_Ptr run,
               const Config *config,
               double end_time)
{
  Simulation_State *state = state_from_run(run);

  printf("protocol,source,ws,wr,seq_space,packet_limit,sim_time,generated,"
         "delivered,acked,unique_packets_transmitted,retransmissions,data_lost,"
         "ack_tx,ack_lost,throughput,mean_delay,seed\n");
  printf("%s,%s,%llu,%llu,%llu,%llu,%.9f,%llu,%llu,%llu,%llu,%llu,%llu,"
         "%llu,%llu,%.9f,%.9f,%u\n",
         protocol_name(config),
         config->source == SOURCE_FINITE ? "finite" : "backlogged",
         (unsigned long long) config->send_window,
         (unsigned long long) config->receive_window,
         (unsigned long long) config->sequence_space,
         (unsigned long long) (config->source == SOURCE_FINITE
                                   ? config->packet_limit : 0),
         end_time,
         (unsigned long long) state->generated,
         (unsigned long long) state->delivered,
         (unsigned long long) state->acknowledged,
         (unsigned long long) state->unique_packets_transmitted,
         (unsigned long long) state->retransmissions,
         (unsigned long long) state->data_losses,
         (unsigned long long) state->ack_transmissions,
         (unsigned long long) state->ack_losses,
         end_time > 0.0 ? (double) state->delivered / end_time : 0.0,
         state->delivered != 0
             ? state->accumulated_delay / (double) state->delivered : 0.0,
         config->seed);
}
