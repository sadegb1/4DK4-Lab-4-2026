#ifndef LAB4_PACKET_TRANSMISSION_H
#define LAB4_PACKET_TRANSMISSION_H

#include "main.h"

void
send_frame(Simulation_State *, Packet_State *, double);

void
fill_sender_window(Simulation_State *, double);

void
send_acknowledgment(Simulation_State *, Packet_State *, double);

void
schedule_go_back_n_timer(Simulation_State *, Packet_State *, double);

void
transmission_end_event(Simulation_Run_Ptr, void *);

void
ack_transmission_end_event(Simulation_Run_Ptr, void *);

void
selective_repeat_timeout_event(Simulation_Run_Ptr, void *);

void
go_back_n_timeout_event(Simulation_Run_Ptr, void *);

#endif
