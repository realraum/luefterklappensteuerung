/*
 *  Damper Control Firmware
 *
 *
 *  Copyright (C) 2016 Bernhard Tittelbach <xro@realraum.at>
 *
 *  This software is made with love and spreadspace avr utils.
 *
 *  Damper Control Firmware is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This firmware is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with these files. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include "Arduino.h"
#include "PJON.h"
#include "dampercontrol.h"


PJON<SoftwareBitBang> pjonbus_;

// --- PJON ID LIST ---

#define PJON_ID_LIST_LEN 10
uint8_t pjon_id_list_[PJON_ID_LIST_LEN];
uint8_t pjon_id_list_idx_ = 0;

#define PJON_MSGBUF_LEN 3
uint8_t pjon_msgbuf_idx_ = 0;
pjon_message_with_sender_t pjon_msgbuf_[PJON_MSGBUF_LEN];

///////// PJON List ///////////
// These methods implement a list of PJON_ID_LIST_LEN bytes
// which implements 3 operations: clear, add, sort

void pjoinidlist_clear()
{
  for (uint8_t c=0; c < PJON_ID_LIST_LEN; c++)
    pjon_id_list_[c] = 0;
  pjon_id_list_idx_ = 0;
}

void pjoinidlist_add(uint8_t id)
{
  if (pjon_id_list_idx_ < PJON_ID_LIST_LEN)
    pjon_id_list_[pjon_id_list_idx_++] = id;
}

int compare_uint8(const void *v1, const void *v2)
{
  return (*((uint8_t*) v1) < *((uint8_t*) v2))? -1 : (*((uint8_t*) v1) > *((uint8_t*) v2))? 1 : 0;
}

void pjoinidlist_sort()
{
  qsort(pjon_id_list_, pjon_id_list_idx_, sizeof(uint8_t), compare_uint8);
}


///////// PJON Callback Handler for Errors ///////////

void pjon_error_handler(uint8_t code, uint8_t data)
{
  if(code == CONNECTION_LOST) {
    printf("Connection with device ID %d is lost.\r\n", data);
  }
  if(code == PACKETS_BUFFER_FULL) {
    printf("Packet buffer is full, has now a length of %d\r\n", data);
    printf("Possible wrong bus configuration!\r\n");
    printf("higher MAX_PACKETS in PJON.h if necessary.\r\n");
  }
  if(code == MEMORY_FULL) {
    printf("Packet memory allocation failed. Memory is full.\r\n");
  }
  if(code == CONTENT_TOO_LONG) {
    printf("Content is too long, length: %d\r\n", data);
  }
  if(code == ID_ACQUISITION_FAIL) {
    printf("Can't acquire a free id %d\r\n", data);
  }
}


///////// PJON Callback Handler for incoming messages ///////////

// Called by PJON when a message in incoming
// Function needs to return as quickly as possible
// since ACK is not sent until after pjon_recv_handler returns
// thus the message is not actually handled here but saved in a roundbuffer of size PJON_MSGBUF_LEN
// where pjon_postrecv_handle_msg can handle it later.
void pjon_recv_handler(uint8_t id, uint8_t *payload, uint8_t length)
{
  if(length < 1 || length >= sizeof(pjon_message_t)) {
    //accepting no messages without a type or messages larger than pjon_message_t
    return;
  }

  //for some reason memcpy needs to come first, because otherwise if we would write the length first, it would get overwriten.
  //Not sure how this can be, but it suggest some kind of bug or memory corruption here. Though I'm obviously too blind
  //to find it right now. (FIXME)
  memcpy(&(pjon_msgbuf_[pjon_msgbuf_idx_].msg), payload, length);
  pjon_msgbuf_[pjon_msgbuf_idx_].id = id;
  pjon_msgbuf_[pjon_msgbuf_idx_].length = length;
  pjon_msgbuf_idx_++;
  pjon_msgbuf_idx_%= PJON_MSGBUF_LEN;
}


///////// Helper Functions ///////////

void pjon_change_deviceid(uint8_t id)
{
  pjon_device_id_ = id;
  pjonbus_.set_id(pjon_device_id_);
  saveSettings2EEPROM();
}

//for each MSG type defined in dampercontrol.h return the length of the msg in bytes
uint8_t pjon_type_to_msg_length(uint8_t type)
{
  switch(type)
  {
    case MSG_DAMPERCMD:
      return sizeof(dampercmd_t)+2;
      break;
    case MSG_PRESSUREINFO:
      return sizeof(pressureinfo_t)+1;
      break;
    case MSG_ERROR:
      return sizeof(errorinfo_t)+1;
      break;
    case MSG_UPDATESETTINGS:
      return sizeof(updatesettings_t)+2;
      break;
    case MSG_PJONID_DOAUTO:
    case MSG_PJONID_QUESTION:
      return 1;
    case MSG_PJONID_INFO:
    case MSG_PJONID_SET:
      return sizeof(pjonidsetting_t)+1;
    default:
      return 1;
      break;
  }
}

//DEBUG: print a pjon msg to tty
void pjon_printf_msg(pjon_message_with_sender_t *bufferedmsg)
{
  printf("<%02x%02x", bufferedmsg->id, bufferedmsg->length);
  for(uint16_t i = 0; i < bufferedmsg->length; ++i)
    printf("%02x",((uint8_t*) &bufferedmsg->msg)[i]);
  printf("\r\n");
}

//DEBUG: send a pjon msg while also printing it to tty
void pjon_debug_send_msg(uint8_t id, const char *payload, uint8_t length)
{
  printf(">%02x%02x", id, length);
  for(uint16_t i = 0; i < length; ++i)
    printf("%02x",payload[i]);
  printf("\r\n");
  pjonbus_.send(id, payload, length);
}

//send a message to the pjon bus while
//also sending it to ourselves
void pjon_inject_msg(uint8_t dst, uint8_t length, uint8_t *payload)
{
  if (dst == 0 || pjonbus_.device_id() == dst)
    pjon_recv_handler(pjon_device_id_, payload, length);
  //hope we did not mangle the payload in recv_handler
  if (dst == 0 || pjonbus_.device_id() != dst)
    pjonbus_.send(dst, (const char*) payload, length);
}

void pjon_inject_broadcast_msg(uint8_t length, uint8_t *payload)
{
  pjon_inject_msg(BROADCAST, length, payload);
}

///////// Chaincasting ///////////////

//Since PJON broadcasts are not acknowleged and have turned out to not be reliable,
//I thought up a nice little scheme I call "Chaincasting"
//(another possible name would be "Laddercasting")
//
//Basically a chaincast message is always sent to or origintates from pjonid 1 (bottom of the ladder)
//On reception, during the first pass, each µC
//1. sets a bit in the message, indiciating he recieved it (actually one bit for each dumper is set)
//2. handles the message (pjon_chaincast_recv_handler)
//3. forwards the message to the next higher message id (ascend ladder)
//
//once all required bits in the msg header are set, the second pass starts
//On reception, during the first pass, each µC
//1. handles the message (pjon_chaincast_recv_handler) in the knowledge that all other µC have now seen the same msg
//2. forwards the message to the next lower message id (descend ladder)
//
//This requires that µC have been give PJON device ids in sequential order
//To ensure this is always the case, a method pjon_become_master_of_ids() was written.
//Basically it talks to every µC on the bus and gives them new id's in sequential order.


//check bitfield if all damper bits are set
bool pjon_chaincast_didreachall(uint8_t bitfield)
{
  #if NUM_DAMPER != 3
    #error "Code assumes NUM_DAMPER == 3. Rewrite as for-loop"
  #endif
  return ((bitfield & _BV(0)) > 0) && ((bitfield & _BV(1)) > 0) && ((bitfield & _BV(2)) > 0);
}

//handle recieved message of type pjon_chaincast_t
//
//1. see if message already reached everyone (second pass, didreachall==true)
//   or if this is the first time we see it (first pass, didreachall==false)
//2. call handler for msg->type
//3. forward message to other µC on pjon-bus
void pjon_chaincast_recv_handler(uint8_t toid, pjon_message_t *msg)
{
  //update reach field
  msg->chaincast.reach |= getInstalledDampersAsBitfield();
  bool didreachall = pjon_chaincast_didreachall(msg->chaincast.reach);

  switch(msg->type)
  {
    case MSG_DAMPERCMD:
      printf("MSG_DAMPERCMD to %d\r\n",toid);
      handle_damper_cmd(didreachall, &(msg->chaincast.dampercmd));
      break;
    case MSG_UPDATESETTINGS:
      printf("MSG_UPDATESETTINGS to %d\r\n",toid);
      updateSettingsFromPacket(&(msg->chaincast.updatesettings));
      break;
    default:
      printf("Unknown MSG type %d\r\n", msg->type);
      break;
  }

  pjon_chaincast_forward(toid, didreachall, msg);
}

//forward chaincast message to µC with next higher (first pass) or next lower (second pass) deviceid
void pjon_chaincast_forward(uint8_t toid, bool didreachall, pjon_message_t* msg)
{
  uint8_t next_id = 0;
  if (toid == 0)
  {
    return; // do not forward broadcasts
  }

  // if pkt has reached µcs controlling all the dampers, forward down-id
  // else forward up-i until bitfield is full
  if (didreachall)
  {
    next_id = pjonbus_.device_id() -1;
  } else {
    next_id = pjonbus_.device_id() +1;
  }

  if (next_id > 0)
  {
    // pjonbus_.send(next_id, (char*) msg, pjon_type_to_msg_length(msg->type));
    pjon_debug_send_msg(next_id, (char*) msg, pjon_type_to_msg_length(msg->type));
  }
}

///////// Message Handler ///////////////

//Handle already received messages saved in roundbuffer
//go through every slot in the roundbuffer and see if
//the msg is new (length > 0)
//then call the appropriate handler after some sanity checks
//and mark the slot as handled by setting length = 0
void pjon_postrecv_handle_msg()
{
  for (uint8_t cc=0; cc < PJON_MSGBUF_LEN; cc++)
  {
    uint8_t c = (pjon_msgbuf_idx_+cc)%PJON_MSGBUF_LEN;

    if (pjon_msgbuf_[c].length == 0)
      continue; //not a message but empty slot: ignore

    pjon_printf_msg(&pjon_msgbuf_[c]);

    uint8_t id = pjon_msgbuf_[c].id;
    uint8_t length = pjon_msgbuf_[c].length;
    pjon_message_t *msg = &(pjon_msgbuf_[c].msg);
    uint8_t typelen = pjon_type_to_msg_length(msg->type);

    //invalidate slot
    pjon_msgbuf_[c].id=0;
    pjon_msgbuf_[c].length=0;

    if (length != typelen)
    {
      printf("got msg with invalid length %d of type %d to id %d which should have had length %d)\r\n", length, msg->type,id,typelen);
      continue; //do not accept msg with wrong length
    }

    switch(msg->type)
    {
      case MSG_DAMPERCMD:
      case MSG_UPDATESETTINGS:
        pjon_chaincast_recv_handler(id, msg);
        break;
      case MSG_PRESSUREINFO:
        printf("MSG_PRESSUREINFO to %d\r\n",id);
        break;
      case MSG_ERROR:
        printf("MSG_ERROR to %d\r\n",id);
        break;
      case MSG_PJONID_DOAUTO:
        printf("MSG_PJONID_DOAUTO to %d\r\n",id);
        pjon_startautoiddiscover();
        break;
      case MSG_PJONID_QUESTION:
        printf("MSG_PJONID_QUESTION to %d\r\n",id);
        //reply to id with our pjon_id
        pjon_identify_myself(1); //send answer to 1 since device 1 is always the one asking this question
        break;
      case MSG_PJONID_INFO:
        printf("MSG_PJONID_INFO to %d\r\n",id);
        //save pjon info somewhere
        pjoinidlist_add(id);
        break;
      case MSG_PJONID_SET:
        printf("MSG_PJONID_SET(%d) to %d\r\n",msg->pjonidsetting.pjon_id,id);
        pjon_change_deviceid(msg->pjonidsetting.pjon_id);
        break;
      case ACQUIRE_ID:
        printf("ACQUIRE_ID id probe to %d\r\n",id);
        break;
      default:
        printf("Unknown MSG type %d to %d\r\n", msg->type, id);
        break;
    }
  }
}

///////// Sending and Handling various types of messages ///////////////

//reply to a MSG_PJONID_QUESTION broadcast msg with our msgid
//@arg toid should usually be 1 since this is the id of the new master
void pjon_identify_myself(uint8_t toid)
{
  pjon_message_t msg;
  msg.type = MSG_PJONID_INFO;
  msg.pjonidsetting.pjon_id = pjonbus_.device_id();
  // pjonbus_.send(toid, (char*) &msg, pjon_type_to_msg_length(msg.type));
  pjon_debug_send_msg(toid, (char*) &msg, pjon_type_to_msg_length(msg.type));
}

//act on a MSG_PJONID_DOAUTO msg
//start acquire_id until we heave an id that is != NOT_ASSIGNEd and != 1
//note that this currently does not actually work, since acquire_id seems to be broken in avr-tools pjon v3
void pjon_startautoiddiscover()
{
  _delay_ms(200); // let the bus settle after receiving the broadcast
  pjonbus_.set_id(NOT_ASSIGNED);
  pjonbus_.acquire_id();
  if (pjonbus_.device_id() == NOT_ASSIGNED || pjonbus_.device_id() == 1)
  {
    printf("try again acquire_id()\r\n");
    _delay_ms(100);
    pjonbus_.acquire_id();
  }
  pjon_device_id_ = pjonbus_.device_id();
  printf("finished pjon acquire_id(), new id: %d\r\n",pjon_device_id_);
}

//broadcast msg MSG_PJONID_DOAUTO to all µC telling them to run pjon_startautoiddiscover
void pjon_broadcast_get_autoid()
{
  pjon_message_t msg;
  msg.type = MSG_PJONID_DOAUTO;
  //tell everybody else to get an autoid
  // pjonbus_.send(BROADCAST, (char*) &msg, pjon_type_to_msg_length(msg.type));
  pjon_debug_send_msg(BROADCAST, (char*) &msg, pjon_type_to_msg_length(msg.type));
}

//become device id 1 and assign every other µC a sequentialy incremential id
void pjon_become_master_of_ids()
{
  pjon_message_t msg;
  msg.type = MSG_PJONID_DOAUTO;
  //become #1
  pjon_change_deviceid(1);
  //tell everybody else to get an autoid
  // pjonbus_.send(BROADCAST, (char*) &msg, pjon_type_to_msg_length(msg.type));
  pjon_debug_send_msg(BROADCAST, (char*) &msg, pjon_type_to_msg_length(msg.type));
  uint16_t w;
  for (w=0; w<UINT16_MAX; w++)
    task_pjon();
  //discover id's of everybody else:
  pjoinidlist_clear();
  msg.type = MSG_PJONID_QUESTION;
  // pjonbus_.send(BROADCAST, (char*) &msg, pjon_type_to_msg_length(msg.type));
  pjon_debug_send_msg(BROADCAST, (char*) &msg, pjon_type_to_msg_length(msg.type));
  for (w=0; w<UINT16_MAX; w++)
    task_pjon();  // wait till all µC replied
  // sort id's numerically
  pjoinidlist_sort();
  // for ids
  // first id with gap of >0 to previous id .. tell it to decrease id by gapsize
  // continue until all id's linear 0,1,2,3,4 without gap
  for (uint8_t ii=0; ii<pjon_id_list_idx_; ii++)
  {
    if (pjon_id_list_[ii] != ii+1)
    {
      msg.type = MSG_PJONID_SET;
      msg.pjonidsetting.pjon_id = ii+1;
      if (pjon_id_list_[ii] != msg.pjonidsetting.pjon_id)
        // pjonbus_.send(pjon_id_list_[ii], (const char*) &msg, pjon_type_to_msg_length(msg.type));
        pjon_debug_send_msg(pjon_id_list_[ii], (const char*) &msg, pjon_type_to_msg_length(msg.type));
    }
  }
}

void pjon_send_pressure_infomsg(uint8_t sensorid, float pressure, float temperature)
{
  pjon_message_t msg;
  msg.type = MSG_PRESSUREINFO;
  msg.pressureinfo.sensorid = sensorid;
  msg.pressureinfo.celsius = temperature;
  msg.pressureinfo.pascal = pressure;
  pjon_debug_send_msg(pjon_sensor_destination_id_, (char*) &msg, pjon_type_to_msg_length(msg.type));
}

//sent if damper_states overflows before reaching endstop. May indicate defect endstop!!
void pjon_senderror_dampertimeout(uint8_t damperid)
{
  pjon_message_t msg;
  msg.type = MSG_ERROR;
  msg.errorinfo.damperid = damperid;
  msg.errorinfo.errortype = DAMPER_CONTROL_TIMEOUT;
  pjon_debug_send_msg(pjon_sensor_destination_id_, (char*) &msg, pjon_type_to_msg_length(msg.type));
}

//for testing, simulation and maybe actual work
void pjon_send_dampercmd(dampercmd_t dcmd)
{
  pjon_message_t msg;
  memcpy(&msg.chaincast.dampercmd.damper,&dcmd.damper,NUM_DAMPER);
  msg.chaincast.dampercmd.fan = dcmd.fan;
  msg.chaincast.reach = 0; //empty bitfield
  msg.type = MSG_DAMPERCMD;
  pjon_inject_msg(1, pjon_type_to_msg_length(msg.type), (uint8_t*) &msg);
}

///////// Initialize PJON bus and data structures ///////////////

void pjon_init()
{
  arduino_init();
  pjonbus_.set_error(pjon_error_handler);
  pjonbus_.set_receiver(pjon_recv_handler);
  pjonbus_.set_pin(PIN_PJON);
  pjonbus_.begin();
  if (pjon_device_id_ != NOT_ASSIGNED)
  {
    pjonbus_.set_id(pjon_device_id_);
  }
  else
  {
    pjonbus_.acquire_id();
    if (pjonbus_.device_id() != NOT_ASSIGNED)
    {
      pjon_device_id_ = pjonbus_.device_id();
      saveSettings2EEPROM();
    }
  }
  for (uint8_t c=0; c<PJON_MSGBUF_LEN; c++)
  {
    pjon_msgbuf_[c].id=0;
    pjon_msgbuf_[c].length=0;
  }
  pjon_msgbuf_idx_ = 0;
}

///////// PJON task, called periodically by main() ///////////////

void task_pjon()
{
    pjonbus_.update();
    pjonbus_.receive(64); //PJON sends ACK in receive after callback
    pjon_postrecv_handle_msg();
}