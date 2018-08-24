/**
DO NOT EDIT DIRECTLY!!

This file was 'objectified' by SCons as a pre-processing
step for the building a Python extension module.

This was done on 2018-08-16 09:57:39.647185.
*/
/**
\brief Implementation of Constrained Join Protocol (CoJP) from minimal-security-06 draft.
*/

#include "opendefs_obj.h"
#include "cjoin_obj.h"
#include "opencoap_obj.h"
#include "opentimers_obj.h"
#include "openqueue_obj.h"
#include "packetfunctions_obj.h"
#include "openserial_obj.h"
#include "openrandom_obj.h"
#include "scheduler_obj.h"
#include "idmanager_obj.h"
#include "IEEE802154E_obj.h"
#include "IEEE802154_security_obj.h"
#include "cojp_cbor.h"
#include "eui64_obj.h"
#include "neighbors_obj.h"

//=========================== defines =========================================

/// inter-packet period (in ms)
#define TIMEOUT                 60000

const uint8_t cjoin_path0[] = "j";

static const uint8_t masterSecret[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xde, 0xad, \
                                     0xbe, 0xef, 0xca, 0xfe, 0xde, 0xad, 0xbe, 0xef};

static const uint8_t jrcHostName[] = "6tisch.arpa";

static const uint8_t proxyScheme[] = "coap";

//=========================== variables =======================================

// declaration of global variable _cjoin_vars_ removed during objectification.

//=========================== prototypes ======================================
void cjoin_init_security_context(OpenMote* self);

owerror_t cjoin_receive(OpenMote* self, OpenQueueEntry_t* msg,
                          coap_header_iht*  coap_header,
                          coap_option_iht*  coap_incomingOptions,
                          coap_option_iht*  coap_outgoingOptions,
                          uint8_t*          coap_outgoingOptionsLen);
void cjoin_timer_cb(OpenMote* self, opentimers_id_t id);
void cjoin_task_cb(OpenMote* self);
void cjoin_sendDone(OpenMote* self, OpenQueueEntry_t* msg,
                       owerror_t error);
owerror_t cjoin_sendJoinRequest(OpenMote* self, open_addr_t *joinProxy);
void cjoin_retransmission_cb(OpenMote* self, opentimers_id_t id);
void cjoin_retransmission_task_cb(OpenMote* self);
bool cjoin_getIsJoined(OpenMote* self);
void cjoin_setIsJoined(OpenMote* self, bool newValue);
//=========================== public ==========================================

void cjoin_init(OpenMote* self) {
   // declare the usage of dynamic keying to L2 security module
 IEEE802154_security_setDynamicKeying(self);

   // prepare the resource descriptor for the /j path
   (self->cjoin_vars).desc.path0len                        = sizeof(cjoin_path0)-1;
   (self->cjoin_vars).desc.path0val                        = (uint8_t*)(&cjoin_path0);
   (self->cjoin_vars).desc.path1len                        = 0;
   (self->cjoin_vars).desc.path1val                        = NULL;
   (self->cjoin_vars).desc.componentID                     = COMPONENT_CJOIN;
//   (self->cjoin_vars).desc.securityContext                 = &(self->cjoin_vars).context;
   (self->cjoin_vars).desc.securityContext                 = NULL;
   (self->cjoin_vars).desc.discoverable                    = TRUE;
   (self->cjoin_vars).desc.callbackRx                      = &cjoin_receive;
   (self->cjoin_vars).desc.callbackSendDone                = &cjoin_sendDone;

   (self->cjoin_vars).isJoined                             = FALSE;

 opencoap_register(self, &(self->cjoin_vars).desc);

   (self->cjoin_vars).timerId = opentimers_create(self);

 idmanager_setJoinKey(self, (uint8_t *) masterSecret);

 cjoin_schedule(self);
}

void cjoin_init_security_context(OpenMote* self) {
   uint8_t senderID[1];     // 1 dummy byte
   uint8_t recipientID[3];  // 3 byte fixed value
   uint8_t id_context[8];
   uint8_t* joinKey;

 eui64_get(self, id_context);
   senderID[0] = 0x00;      // construct sender ID according to the minimal-security-06 draft
   // construct recipient ID according to the minimal-security-06 draft
   recipientID[0] = 0x4a; // "J"
   recipientID[1] = 0x52; // "R"
   recipientID[2] = 0x43; // "C"

 idmanager_getJoinKey(self, &joinKey);

   // TODO Pass id_context to the routine
 openoscoap_init_security_context(self, &(self->cjoin_vars).context,
                                senderID,
                                sizeof(senderID),
                                recipientID,
                                sizeof(recipientID),
                                joinKey,
                                16,
                                NULL,
                                0);
}

void cjoin_schedule(OpenMote* self) {
    uint16_t delay;

    if ( cjoin_getIsJoined(self) == FALSE) {
        delay = openrandom_get16b(self);

 opentimers_scheduleIn(self, (self->cjoin_vars).timerId,
                (uint32_t) delay, // random wait from 0 to 65535ms
                TIME_MS,
                TIMER_PERIODIC,
                cjoin_timer_cb
        );
    }

}

//=========================== private =========================================
owerror_t cjoin_receive(OpenMote* self, OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen) {


    cojp_configuration_object_t configuration;
    owerror_t ret;

 opentimers_cancel(self, (self->cjoin_vars).timerId); // cancel the retransmission timer

    if (coap_header->Code != COAP_CODE_RESP_CHANGED) {
        return E_FAIL;
    }

    ret = cojp_cbor_decode_configuration_object(msg->payload, msg->length, &configuration);
    if (ret == E_FAIL) { return E_FAIL; }

    if (configuration.keyset.num_keys == 1 &&
        configuration.keyset.key[0].key_usage == COJP_KEY_USAGE_6TiSCH_K1K2_ENC_MIC32) {
            // set the L2 keys as per the parsed value
 IEEE802154_security_setBeaconKey(self, configuration.keyset.key[0].key_index, configuration.keyset.key[0].key_value);
 IEEE802154_security_setDataKey(self, configuration.keyset.key[0].key_index, configuration.keyset.key[0].key_value);
 cjoin_setIsJoined(self, TRUE); // declare join is over
            return E_SUCCESS;
    } else {
        // TODO not supported for now
    }


    return E_FAIL;
}

//timer fired, but we don't want to execute task in ISR mode
//instead, push task to scheduler with COAP priority, and let scheduler take care of it
void cjoin_timer_cb(OpenMote* self, opentimers_id_t id){
 scheduler_push_task(self, cjoin_task_cb,TASKPRIO_COAP);
}

void cjoin_retransmission_cb(OpenMote* self, opentimers_id_t id) {
 scheduler_push_task(self, cjoin_retransmission_task_cb, TASKPRIO_COAP);
}

void cjoin_retransmission_task_cb(OpenMote* self) {
    open_addr_t* joinProxy;

    joinProxy = neighbors_getJoinProxy(self);
    if(joinProxy == NULL) {
 openserial_printError(self, 
         COMPONENT_CJOIN,
         ERR_ABORT_JOIN_PROCESS,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
        return;
    }

 cjoin_sendJoinRequest(self, joinProxy);
}

void cjoin_task_cb(OpenMote* self) {
    open_addr_t *joinProxy;

    // don't run if not synch
    if ( ieee154e_isSynch(self) == FALSE) return;

    // don't run if DAG root
    if ( idmanager_getIsDAGroot(self) == TRUE) {
 opentimers_destroy(self, (self->cjoin_vars).timerId);
        return;
    }

    joinProxy = neighbors_getJoinProxy(self);
    if(joinProxy == NULL) {
        return;
    }

    // cancel the startup timer but do not destroy it as we reuse it for retransmissions
 opentimers_cancel(self, (self->cjoin_vars).timerId);

    // init the security context only here in order to use the latest joinKey
    // that may be set over the serial
// cjoin_init_security_context(self);

 cjoin_sendJoinRequest(self, joinProxy);

    return;
}

void cjoin_sendDone(OpenMote* self, OpenQueueEntry_t* msg, owerror_t error) {
 openqueue_freePacketBuffer(self, msg);
}

owerror_t cjoin_sendJoinRequest(OpenMote* self, open_addr_t* joinProxy) {
   OpenQueueEntry_t*            pkt;
   owerror_t                    outcome;
   coap_option_iht              options[5];
   uint8_t                      tmp[10];
   uint8_t                      payload_len;
   cojp_join_request_object_t   join_request;

   payload_len = 0;

   // immediately arm the retransmission timer
 opentimers_scheduleIn(self, (self->cjoin_vars).timerId,
            (uint32_t) TIMEOUT,
            TIME_MS,
            TIMER_ONESHOT,
            cjoin_retransmission_cb
    );

   // create a CoAP RD packet
   pkt = openqueue_getFreePacketBuffer(self, COMPONENT_CJOIN);
   if (pkt==NULL) {
 openserial_printError(self, 
         COMPONENT_CJOIN,
         ERR_NO_FREE_PACKET_BUFFER,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return E_FAIL;
   }
   // take ownership over that packet
   pkt->creator                   = COMPONENT_CJOIN;
   pkt->owner                     = COMPONENT_CJOIN;

   // uri-host set to 6tisch.arpa
   options[0].type = COAP_OPTION_NUM_URIHOST;
   options[0].length = sizeof(jrcHostName)-1;
   options[0].pValue = (uint8_t *)jrcHostName;

   // location-path option
   options[1].type = COAP_OPTION_NUM_URIPATH;
   options[1].length = sizeof(cjoin_path0)-1;
   options[1].pValue = (uint8_t *)cjoin_path0;

   // object security option
   // length and value are set by the CoAP library
//   options[2].type = COAP_OPTION_NUM_OBJECTSECURITY;

   // FIXME content format is needed for testing with F-Interop
   (self->cjoin_vars).medType = COAP_MEDTYPE_APPCBOR;
   options[2].type = COAP_OPTION_NUM_CONTENTFORMAT;
   options[2].length = 1;
   options[2].pValue = &(self->cjoin_vars).medType;

   // ProxyScheme set to "coap"
   options[3].type = COAP_OPTION_NUM_PROXYSCHEME;
   options[3].length = sizeof(proxyScheme)-1;
   options[3].pValue = (uint8_t *)proxyScheme;

   // metadata
   pkt->l4_destination_port       = WKP_UDP_COAP;
   pkt->l3_destinationAdd.type    = ADDR_128B;
   pkt->l3_destinationAdd.addr_128b[0] = 0xfe;
   pkt->l3_destinationAdd.addr_128b[1] = 0x80;
   memset(&pkt->l3_destinationAdd.addr_128b[2], 0x00, 6);
   memcpy(&pkt->l3_destinationAdd.addr_128b[8],joinProxy->addr_64b,8); // set host to eui-64 of the join proxy

   // encode Join_Request object in the payload
   join_request.role = COJP_ROLE_VALUE_6N; // regular non-6LBR node
   join_request.pan_id = idmanager_getMyID(self, ADDR_PANID); // pre-configured PAN ID
   payload_len = cojp_cbor_encode_join_request_object(tmp, &join_request);
 packetfunctions_reserveHeaderSize(self, pkt, payload_len);
   memcpy(pkt->payload, tmp, payload_len);

   // send
   outcome = opencoap_send(self, 
      pkt,
      COAP_TYPE_NON,
      COAP_CODE_REQ_POST,
      0, // token len
      options,
      4, // options len
      &(self->cjoin_vars).desc
   );

   // avoid overflowing the queue if fails
   if (outcome==E_FAIL) {
 openqueue_freePacketBuffer(self, pkt);
      return E_FAIL;
   }

  return E_SUCCESS;
}

bool cjoin_getIsJoined(OpenMote* self) {
   bool res;
   INTERRUPT_DECLARATION();

   DISABLE_INTERRUPTS();
   res=(self->cjoin_vars).isJoined;
   ENABLE_INTERRUPTS();

   return res;
}

void cjoin_setIsJoined(OpenMote* self, bool newValue) {
    uint8_t array[5];
    asn_t joinAsn;

    if ((self->cjoin_vars).isJoined == newValue) {
        return;
    }

    (self->cjoin_vars).isJoined = newValue;

    // Update Join ASN value
 ieee154e_getAsn(self, array);
    joinAsn.bytes0and1           = ((uint16_t) array[1] << 8) | ((uint16_t) array[0]);
    joinAsn.bytes2and3           = ((uint16_t) array[3] << 8) | ((uint16_t) array[2]);
    joinAsn.byte4                = array[4];

 idmanager_setJoinAsn(self, &joinAsn);

    if (newValue == TRUE) {
        // log the info
 openserial_printInfo(self, COMPONENT_CJOIN, ERR_JOINED,
                             (errorparameter_t)0,
                             (errorparameter_t)0);
    }
}

