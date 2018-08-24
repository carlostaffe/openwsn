/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2018-08-16 09:51:11.506771.
*/
/**
\brief This project runs the full OpenWSN stack.

\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, August 2010
*/

#include "board_obj.h"
#include "scheduler_obj.h"
#include "openstack_obj.h"
#include "opendefs_obj.h"

int mote_main(OpenMote* self) {

    // initialize
 board_init(self);
 scheduler_init(self);
 openstack_init(self);

    // start
 scheduler_start(self);
    return 0; // this line should never be reached
}
