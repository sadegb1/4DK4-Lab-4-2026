#ifndef _PACKET_TRANSMISSION_H_
#define _PACKET_TRANSMISSION_H_

#include "main.h"

void
fill_sender_window(Simulation_Run_Ptr);

void
schedule_go_back_n_timer(Simulation_Run_Ptr, Packet_Ptr, Time);

void
schedule_transmission_start_event(Simulation_Run_Ptr, Time, Packet_Ptr);

void
transmission_start_event(Simulation_Run_Ptr, void *);

void
transmission_end_event(Simulation_Run_Ptr, void *);

void
selective_repeat_timeout_event(Simulation_Run_Ptr, void *);

void
go_back_n_timeout_event(Simulation_Run_Ptr, void *);

#endif /* packet_transmission.h */
