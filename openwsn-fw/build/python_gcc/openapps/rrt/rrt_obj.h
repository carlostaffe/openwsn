/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2018-08-16 09:52:18.532638.
*/
#ifndef __RRT_H
#define __RRT_H

/**
\addtogroup AppCoAP
\{
\addtogroup rrt
\{
*/

#include "Python.h"

#include "opendefs_obj.h"
#include "opencoap_obj.h"
#include "schedule_obj.h"

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
	coap_resource_desc_t desc;
	uint8_t discovered;
} rrt_vars_t;

#include "openwsnmodule_obj.h"
typedef struct OpenMote OpenMote;

//=========================== prototypes ======================================

void rrt_init(OpenMote* self);

void rrt_sendCoAPMsg(OpenMote* self, char actionMsg, uint8_t *ipv6mote);
void rrt_sendDone(OpenMote* self, OpenQueueEntry_t* msg, owerror_t error);
/**
\}
\}
*/

#endif