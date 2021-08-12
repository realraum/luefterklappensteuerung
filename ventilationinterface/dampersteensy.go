package main

import (
  "time"

  "github.com/btittelbach/pubsub"
)

type SerialLine []byte

const (
	damperteensy_start_tx           byte  = '>'
	damperteensy_pjonid_1           uint8 = 1
	damperteensy_type_dampercmd     uint8 = 0
	damperteensy_cmd_damperclosed   uint8 = 0
	damperteensy_cmd_damperopen     uint8 = 1
	damperteensy_cmd_damperhalfopen uint8 = 2
	damperteensy_cmd_fanon          uint8 = 1
	damperteensy_cmd_fanoff         uint8 = 0
)

var damperteensy_cmdmap map[string]uint8 = map[string]uint8{ws_damper_state_closed: damperteensy_cmd_damperclosed, ws_damper_state_open: damperteensy_cmd_damperopen, ws_damper_state_half: damperteensy_cmd_damperhalfopen, ws_fan_state_off: damperteensy_cmd_fanoff, ws_fan_state_on: damperteensy_cmd_fanon}

func mkDamperCmdMsg(newstate wsChangeVent) []byte {
	buf := make([]byte, 16)
	i := 0
	buf[i] = damperteensy_start_tx //serial start tx
	i++
	buf[i] = damperteensy_pjonid_1 //send to pjon id 1
	i++
	insert_length_here := i
	i++
	inmap := false
	buf[i] = damperteensy_type_dampercmd //msg type
	i++
	buf[i] = 0 //reach
	i++
	buf[i], inmap = damperteensy_cmdmap[newstate.Damper1] //Damper[0]
	if inmap == false {
		return nil
	}
	i++
	buf[i], inmap = damperteensy_cmdmap[newstate.Damper2] //Damper[1]
	if inmap == false {
		return nil
	}
	i++
	buf[i], inmap = damperteensy_cmdmap[newstate.Damper3] //Damper[2]
	if inmap == false {
		return nil
	}
	i++
	buf[i] |= damperteensy_cmdmap[newstate.Fan] // Fan
	if inmap == false {
		return nil
	}
	i++
	buf[insert_length_here] = byte(i - insert_length_here - 1)
	return buf[0:i]
}

//TODO: decode and handle error msg if damper did not reach endstop in time
//      --> repeat cmd for that damper

// if vent position changed, we may want to way a bit until sending the next vent position change command
// in order to not overtax the 12V power supply. Should really be implemented in the µC
func didVentPositionChange(a, b wsChangeVent) bool {
	return a.Damper1 != b.Damper1 || a.Damper2 != b.Damper2 || a.Damper3 != b.Damper3
}

func goChangeDampers(ps *pubsub.PubSub, min_cmd_send_interval time.Duration) {
	newstate_c := ps.Sub(PS_DAMPERSCHANGED)
	shutdown_c := ps.SubOnce("shutdown")
	defer ps.Unsub(newstate_c, PS_DAMPERSCHANGED)
	teensytty_wr, teensytty_rd, teensytty_err := OpenAndHandleSerial(TeensyTTY_, 9600)
	if teensytty_err != nil {
		panic(teensytty_err)
	}
	var last_cmd_time time.Time
	var last_state wsChangeVent

	for {
		select {
		case <-shutdown_c:
			return
		case newstate := <-newstate_c:
			vent_position_changed := didVentPositionChange(last_state, newstate.(wsChangeVent))
			LogVent_.Print("goChangeDampers", "Changing Ventpositions to:", newstate.(wsChangeVent))
			if newstate.(wsChangeVent) == last_state {
				continue
			}
			last_state = newstate.(wsChangeVent)
			cmdbytes := mkDamperCmdMsg(last_state)
			if cmdbytes != nil && len(cmdbytes) > 0 {
				if vent_position_changed && time.Now().Sub(last_cmd_time) < min_cmd_send_interval {
					LogVent_.Print("goChangeDampers", "cmds too fast, delaying..", cmdbytes)
					time.Sleep(min_cmd_send_interval - time.Now().Sub(last_cmd_time))
				}
				LogVent_.Print("goChangeDampers", "ToPJON:", cmdbytes)
				teensytty_wr <- cmdbytes
				last_cmd_time = time.Now()
			}
		case line := <-teensytty_rd:
			LogVent_.Print("goChangeDampers", "FromPJON:", line)
		}
	}
}
