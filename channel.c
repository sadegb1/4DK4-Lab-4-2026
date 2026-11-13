#include "simlib.h"
#include "channel.h"

Channel_Ptr
channel_new(double packet_error_probability, double ack_error_probability)
{
  Channel_Ptr channel = (Channel_Ptr) xmalloc(sizeof(Channel));

  channel->packet_error_probability = packet_error_probability;
  channel->ack_error_probability = ack_error_probability;
  return channel;
}

int
channel_data_packet_success(Channel_Ptr channel)
{
  return uniform_generator() >= channel->packet_error_probability;
}

int
channel_ack_success(Channel_Ptr channel)
{
  return uniform_generator() >= channel->ack_error_probability;
}
