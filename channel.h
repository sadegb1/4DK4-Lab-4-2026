#ifndef _CHANNEL_H_
#define _CHANNEL_H_

typedef struct _channel_ {
  double packet_error_probability;
  double ack_error_probability;
} Channel, *Channel_Ptr;

Channel_Ptr
channel_new(double, double);

int
channel_data_packet_success(Channel_Ptr);

int
channel_ack_success(Channel_Ptr);

#endif /* _CHANNEL_H_ */
