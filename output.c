#include <stdio.h>

#include "main.h"
#include "output.h"
#include "simparameters.h"

static const char *
protocol_name(const Simulation_Run_Data_Ptr data)
{
  if (data->parameters.send_window == 1 &&
      data->parameters.receive_window == 1)
    return "stop-and-wait";
  if (data->parameters.receive_window == 1)
    return "go-back-n";
  return "selective-repeat";
}

void
output_print_header(void)
{
  printf("protocol,source,ws,wr,packets,pd,pa,timeout_ms,seed,generated,"
         "delivered,acked,data_attempts,attempts_per_delivered,data_errors,"
         "ack_attempts,ack_errors,timeouts,simulation_time_ms,"
         "link_efficiency,mean_delay_ms\n");
}

void
output_results(Simulation_Run_Ptr simulation_run)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);
  double simulation_time = simulation_run_get_time(simulation_run);
  unsigned long long delivered_attempts = 0;
  uint64_t id;
  double attempts_per_delivered;
  double link_efficiency;
  double mean_delay;
  uint64_t packet_target =
      data->parameters.source_mode == FINITE_SOURCE
          ? data->parameters.number_of_packets
          : data->parameters.runlength;

  for (id = 0; id < data->receiver_base; id++) {
    Packet_Ptr packet = packet_get(data, id);
    if (packet != NULL)
      delivered_attempts += packet->transmission_attempts;
  }

  attempts_per_delivered =
      data->number_of_packets_processed == 0 ? 0.0 :
      (double) delivered_attempts /
          (double) data->number_of_packets_processed;
  link_efficiency = simulation_time == 0.0 ? 0.0 :
      (double) data->number_of_packets_processed * MEAN_PACKET_DURATION /
          simulation_time;
  mean_delay = data->number_of_packets_processed == 0 ? 0.0 :
      data->accumulated_delay /
          (double) data->number_of_packets_processed;

  printf("%s,%s,%llu,%llu,%llu,%.6f,%.6f,%.6f,%llu,%llu,%llu,%llu,"
         "%llu,%.6f,%llu,%llu,%llu,%llu,%.6f,%.6f,%.6f\n",
         protocol_name(data),
         data->parameters.source_mode == FINITE_SOURCE
             ? "finite" : "backlogged",
         (unsigned long long) data->parameters.send_window,
         (unsigned long long) data->parameters.receive_window,
         (unsigned long long) packet_target,
         data->parameters.packet_error_probability,
         data->parameters.ack_error_probability,
         data->parameters.timeout,
         (unsigned long long) data->random_seed,
         (unsigned long long) data->arrival_count,
         (unsigned long long) data->number_of_packets_processed,
         (unsigned long long) data->number_of_packets_acked,
         data->transmission_attempts,
         attempts_per_delivered,
         (unsigned long long) data->data_packet_errors,
         (unsigned long long) data->ack_transmission_attempts,
         (unsigned long long) data->ack_errors,
         (unsigned long long) data->timeout_count,
         simulation_time,
         link_efficiency,
         mean_delay);
}

void
output_blip_to_screen(Simulation_Run_Ptr simulation_run)
{
  Simulation_Run_Data_Ptr data = simulation_run_get_data(simulation_run);

  data->blip_counter++;
  if (data->blip_counter >= BLIPRATE) {
    data->blip_counter = 0;
    fprintf(stderr, "Delivered %llu packets at %.3f ms\r",
            (unsigned long long) data->number_of_packets_processed,
            simulation_run_get_time(simulation_run));
    fflush(stderr);
  }
}
