#ifndef _PACKET_ARRIVAL_H_
#define _PACKET_ARRIVAL_H_

#include "main.h"

Packet_Ptr
packet_new(Simulation_Run_Data_Ptr, Time);

long int
schedule_packet_arrival_event(Simulation_Run_Ptr, Time, Packet_Ptr);

void
packet_arrival_event(Simulation_Run_Ptr, void *);

void
acknowledgment_arrival_event(Simulation_Run_Ptr, void *);

#endif /* packet_arrival.h */
