/*
 * Copyright (c) 2022 The Chromium OS Authors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/smf.h>
#include <zephyr/usb_c/usbc.h>
#include <zephyr/drivers/usb_c/usbc_pd.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(usbc_stack, CONFIG_USBC_STACK_LOG_LEVEL);

#include "usbc_pe_common_internal.h"
#include "usbc_stack.h"

/**
 * @brief Handle sink-specific DPM requests
 */
bool sink_dpm_requests(const struct device *dev)
{
	struct usbc_port_data *data = dev->data;
	struct policy_engine *pe = data->pe;

	if (pe->dpm_request > REQUEST_TC_END) {
		atomic_set_bit(&pe->flags, PE_FLAGS_DPM_INITIATED_AMS);

		if (pe->dpm_request == REQUEST_PE_GET_SRC_CAPS) {
			pe_set_state(dev, PE_SNK_GET_SOURCE_CAP);
		}
		return true;
	}

	return false;
}

/**
 * @brief PE_SNK_Startup Entry State
 */
void pe_snk_startup_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	LOG_INF("PE_SNK_Startup");

	/* Reset the protocol layer */
	prl_reset(dev);

	/* Set power role to Sink */
	pe->power_role = TC_ROLE_SINK;

	/* Invalidate explicit contract */
	atomic_clear_bit(&pe->flags, PE_FLAGS_EXPLICIT_CONTRACT);

	policy_notify(dev, NOT_PD_CONNECTED);
}

/**
 * @brief PE_SNK_Startup Run State
 */
void pe_snk_startup_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	/*
	 * Once the reset process completes, the Policy Engine Shall
	 * transition to the PE_SNK_Discovery state
	 */
	if (prl_is_running(dev)) {
		pe_set_state(dev, PE_SNK_DISCOVERY);
	}
}

/**
 * @brief PE_SNK_Discovery Entry State
 */
void pe_snk_discovery_entry(void *obj)
{
	LOG_INF("PE_SNK_Discovery");
}

/**
 * @brief PE_SNK_Discovery Run State
 */
void pe_snk_discovery_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	const struct device *vbus = data->vbus;

	/*
	 * Transition to the PE_SNK_Wait_for_Capabilities state when
	 * VBUS has been detected
	 */
	if (usbc_vbus_check_level(vbus, TC_VBUS_PRESENT)) {
		pe_set_state(dev, PE_SNK_WAIT_FOR_CAPABILITIES);
	}
}

/**
 * @brief PE_SNK_Wait_For_Capabilities Entry State
 */
void pe_snk_wait_for_capabilities_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;

	LOG_INF("PE_SNK_Wait_For_Capabilities");

	/* Initialize and start the SinkWaitCapTimer */
	usbc_timer_start(&pe->pd_t_typec_sink_wait_cap);
}

/**
 * @brief PE_SNK_Wait_For_Capabilities Run State
 */
void pe_snk_wait_for_capabilities_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	struct protocol_layer_rx_t *prl_rx = data->prl_rx;
	union pd_header header;

	/*
	 * Transition to the PE_SNK_Evaluate_Capability state when:
	 *  1) A Source_Capabilities Message is received.
	 */
	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_MSG_RECEIVED)) {
		header = prl_rx->emsg.header;
		if (received_data_message(dev, header, PD_DATA_SOURCE_CAP)) {
			pe_set_state(dev, PE_SNK_EVALUATE_CAPABILITY);
			return;
		}
	}

	/* When the SinkWaitCapTimer times out, perform a Hard Reset. */
	if (usbc_timer_expired(&pe->pd_t_typec_sink_wait_cap)) {
		atomic_set_bit(&pe->flags, PE_FLAGS_SNK_WAIT_CAP_TIMEOUT);
		pe_set_state(dev, PE_SNK_HARD_RESET);
	}
}

/**
 * @brief PE_SNK_Wait_For_Capabilities Exit State
 */
void pe_snk_wait_for_capabilities_exit(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;

	/* Stop SinkWaitCapTimer */
	usbc_timer_stop(&pe->pd_t_typec_sink_wait_cap);
}

/**
 * @brief PE_SNK_Evaluate_Capability Entry State
 */
void pe_snk_evaluate_capability_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	struct protocol_layer_rx_t *prl_rx = data->prl_rx;
	union pd_header header;
	uint32_t *pdos = (uint32_t *)prl_rx->emsg.data;
	uint32_t num_pdo_objs = PD_CONVERT_BYTES_TO_PD_HEADER_COUNT(prl_rx->emsg.len);

	LOG_INF("PE_SNK_Evaluate_Capability");

	header = prl_rx->emsg.header;

	/* Reset Hard Reset counter to zero */
	pe->hard_reset_counter = 0;

	/* Set to highest revision supported by both ports */
	prl_set_rev(dev, PD_PACKET_SOP, MIN(PD_REV30, header.specification_revision));

	/* Send source caps to Device Policy Manager for saving */
	policy_set_src_cap(dev, pdos, num_pdo_objs);

	/* Transition to PE_Snk_Select_Capability */
	pe_set_state(dev, PE_SNK_SELECT_CAPABILITY);
}

/**
 * @brief PE_SNK_Select_Capability Entry State
 */
void pe_snk_select_capability_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	uint32_t rdo;

	LOG_INF("PE_SNK_Select_Capability");

	/* Get selected source cap from Device Policy Manager */
	rdo = policy_get_request_data_object(dev);

	/* Send Request */
	pe_send_request_msg(dev, rdo);
	/* Inform Device Policy Manager that we are PD Connected */
	policy_notify(dev, PD_CONNECTED);
}


/**
 * @brief PE_SNK_Select_Capability Run State
 */
void pe_snk_select_capability_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	struct protocol_layer_rx_t *prl_rx = data->prl_rx;
	union pd_header header;

	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_MSG_DISCARDED)) {
		/*
		 * The sent REQUEST message was discarded.  This can be at
		 * the start of an AMS or in the middle. Handle what to
		 * do based on where we came from.
		 * 1) SE_SNK_EVALUATE_CAPABILITY: sends SoftReset
		 * 2) SE_SNK_READY: goes back to SNK Ready
		 */
		if (pe_get_last_state(dev) == PE_SNK_EVALUATE_CAPABILITY) {
			pe_send_soft_reset(dev, PD_PACKET_SOP);
		} else {
			pe_set_state(dev, PE_SNK_READY);
		}
	} else if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_TX_COMPLETE)) {
		/* Start the SenderResponseTimer */
		usbc_timer_start(&pe->pd_t_sender_response);
	}

	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_MSG_RECEIVED)) {
		header = prl_rx->emsg.header;

		/*
		 * Transition to the PE_SNK_Transition_Sink state when:
		 *  1) An Accept Message is received from the Source.
		 *
		 * Transition to the PE_SNK_Wait_for_Capabilities state when:
		 *  1) There is no Explicit Contract in place and
		 *  2) A Reject Message is received from the Source or
		 *  3) A Wait Message is received from the Source.
		 *
		 * Transition to the PE_SNK_Ready state when:
		 *  1) There is an Explicit Contract in place and
		 *  2) A Reject Message is received from the Source or
		 *  3) A Wait Message is received from the Source.
		 *
		 * Transition to the PE_SNK_Hard_Reset state when:
		 *  1) A SenderResponseTimer timeout occurs.
		 */
		/* Only look at control messages */
		if (received_control_message(dev, header, PD_CTRL_ACCEPT)) {
			/* explicit contract is now in place */
			atomic_set_bit(&pe->flags, PE_FLAGS_EXPLICIT_CONTRACT);
			pe_set_state(dev, PE_SNK_TRANSITION_SINK);
		} else if (received_control_message(dev, header, PD_CTRL_REJECT) ||
			   received_control_message(dev, header, PD_CTRL_WAIT)) {
			/*
			 * We had a previous explicit contract, so transition to
			 * PE_SNK_Ready
			 */
			if (atomic_test_bit(&pe->flags, PE_FLAGS_EXPLICIT_CONTRACT)) {
				if (received_control_message(dev, header, PD_CTRL_WAIT)) {
					/*
					 * Inform Device Policy Manager that Sink
					 * Request needs to Wait
					 */
					if (policy_wait_notify(dev, WAIT_SINK_REQUEST)) {
						atomic_set_bit(&pe->flags,
								PE_FLAGS_WAIT_SINK_REQUEST);
						usbc_timer_start(&pe->pd_t_wait_to_resend);
					}
				}

				pe_set_state(dev, PE_SNK_READY);
			}
			/*
			 * No previous explicit contract, so transition
			 * to PE_SNK_Wait_For_Capabilities
			 */
			else {
				pe_set_state(dev, PE_SNK_WAIT_FOR_CAPABILITIES);
			}
		} else {
			pe_send_soft_reset(dev, prl_rx->emsg.type);
		}
		return;
	}

	/* When the SenderResponseTimer times out, perform a Hard Reset. */
	if (usbc_timer_expired(&pe->pd_t_sender_response)) {
		policy_notify(dev, PORT_PARTNER_NOT_RESPONSIVE);
		pe_set_state(dev, PE_SNK_HARD_RESET);
	}
}

/**
 * @brief PE_SNK_Select_Capability Exit State
 */
void pe_snk_select_capability_exit(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;

	/* Stop SenderResponse Timer */
	usbc_timer_stop(&pe->pd_t_sender_response);
}

/**
 * @brief PE_SNK_Transition_Sink Entry State
 */
void pe_snk_transition_sink_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;

	LOG_INF("PE_SNK_Transition_Sink");

	/* Initialize and run PSTransitionTimer */
	usbc_timer_start(&pe->pd_t_ps_transition);
}

/**
 * @brief PE_SNK_Transition_Sink Run State
 */
void pe_snk_transition_sink_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	struct protocol_layer_rx_t *prl_rx = data->prl_rx;
	union pd_header header;

	/*
	 * Transition to the PE_SNK_Ready state when:
	 *  1) A PS_RDY Message is received from the Source.
	 *
	 * Transition to the PE_SNK_Hard_Reset state when:
	 *  1) A Protocol Error occurs.
	 */
	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_MSG_RECEIVED)) {
		header = prl_rx->emsg.header;

		/*
		 * PS_RDY message received
		 */
		if (received_control_message(dev, header, PD_CTRL_PS_RDY)) {
			/*
			 * Inform the Device Policy Manager to Transition
			 * the Power Supply
			 */
			policy_notify(dev, TRANSITION_PS);
			pe_set_state(dev, PE_SNK_READY);
		} else {
			/* Protocol Error */
			pe_set_state(dev, PE_SNK_HARD_RESET);
		}
		return;
	}

	/*
	 * Timeout will lead to a Hard Reset
	 */
	if (usbc_timer_expired(&pe->pd_t_ps_transition)) {
		pe_set_state(dev, PE_SNK_HARD_RESET);
	}
}

/**
 * @brief PE_SNK_Transition_Sink Exit State
 */
void pe_snk_transition_sink_exit(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;

	/* Initialize and run PSTransitionTimer */
	usbc_timer_stop(&pe->pd_t_ps_transition);
}

/**
 * @brief PE_SNK_Ready Entry State
 */
void pe_snk_ready_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;

	LOG_INF("PE_SNK_Ready");

	/* Clear AMS Flags */
	atomic_clear_bit(&pe->flags, PE_FLAGS_INTERRUPTIBLE_AMS);
	atomic_clear_bit(&pe->flags, PE_FLAGS_DPM_INITIATED_AMS);
}

/**
 * @brief PE_SNK_Ready Run State
 */
void pe_snk_ready_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	struct protocol_layer_rx_t *prl_rx = data->prl_rx;

	/*
	 * Handle incoming messages before discovery and DPMs other than hard
	 * reset
	 */
	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_MSG_RECEIVED)) {
		union pd_header header = prl_rx->emsg.header;

		/* Extended Message Request */
		if (header.extended) {
			extended_message_not_supported(dev);
			return;
		}
		/* Data Messages */
		else if (header.number_of_data_objects > 0) {
			switch (header.message_type) {
			case PD_DATA_SOURCE_CAP:
				pe_set_state(dev, PE_SNK_EVALUATE_CAPABILITY);
				break;
			default:
				pe_set_state(dev, PE_SEND_NOT_SUPPORTED);
			}
			return;
		}
		/* Control Messages */
		else {
			switch (header.message_type) {
			case PD_CTRL_GOOD_CRC:
				/* Do nothing */
				break;
			case PD_CTRL_PING:
				/* Do nothing */
				break;
			case PD_CTRL_GET_SINK_CAP:
				pe_set_state(dev, PE_SNK_GIVE_SINK_CAP);
				return;
			case PD_CTRL_DR_SWAP:
				pe_set_state(dev, PE_DRS_EVALUATE_SWAP);
				return;
			case PD_CTRL_NOT_SUPPORTED:
				/* Do nothing */
				break;
			/*
			 * USB PD 3.0 6.8.1:
			 * Receiving an unexpected message shall be responded
			 * to with a soft reset message.
			 */
			case PD_CTRL_ACCEPT:
			case PD_CTRL_REJECT:
			case PD_CTRL_WAIT:
			case PD_CTRL_PS_RDY:
				pe_send_soft_reset(dev, prl_rx->emsg.type);
				return;
			/*
			 * Receiving an unknown or unsupported message
			 * shall be responded to with a not supported message.
			 */
			default:
				pe_set_state(dev, PE_SEND_NOT_SUPPORTED);
				return;
			}
		}
	}

	/*
	 * Check if we are waiting to resend any messages
	 */
	if (usbc_timer_expired(&pe->pd_t_wait_to_resend)) {
		if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_WAIT_SINK_REQUEST)) {
			pe_set_state(dev, PE_SNK_SELECT_CAPABILITY);
			return;
		} else if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_WAIT_DATA_ROLE_SWAP)) {
			pe_set_state(dev, PE_DRS_SEND_SWAP);
			return;
		}
	}

	/*
	 * Handle Device Policy Manager Requests
	 */
	sink_dpm_requests(dev);
}

/**
 * @brief PE_SNK_Hard_Reset Entry State
 */
void pe_snk_hard_reset_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;

	LOG_INF("PE_SNK_Hard_Reset");

	/*
	 * Note: If the SinkWaitCapTimer times out and the HardResetCounter is
	 *       greater than nHardResetCount the Sink Shall assume that the
	 *       Source is non-responsive.
	 */
	if (atomic_test_bit(&pe->flags, PE_FLAGS_SNK_WAIT_CAP_TIMEOUT) &&
	    pe->hard_reset_counter > PD_N_HARD_RESET_COUNT) {
		/* Inform the DPM that the port partner is not responsive */
		policy_notify(dev, PORT_PARTNER_NOT_RESPONSIVE);

		/* Pause the Policy Engine */
		data->pe_enabled = false;
		return;
	}

	/* Set Hard Reset Pending Flag */
	atomic_set_bit(&pe->flags, PE_FLAGS_HARD_RESET_PENDING);

	atomic_clear_bit(&pe->flags, PE_FLAGS_SNK_WAIT_CAP_TIMEOUT);
	atomic_clear_bit(&pe->flags, PE_FLAGS_PROTOCOL_ERROR);

	/* Request the generation of Hard Reset Signaling by the PHY Layer */
	prl_execute_hard_reset(dev);
	/* Increment the HardResetCounter */
	pe->hard_reset_counter++;
}

/**
 * @brief PE_SNK_Hard_Reset Run State
 */
void pe_snk_hard_reset_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	/*
	 * Transition to the PE_SNK_Transition_to_default state when:
	 *  1) The Hard Reset is complete.
	 */
	if (atomic_test_bit(&pe->flags, PE_FLAGS_HARD_RESET_PENDING)) {
		return;
	}

	pe_set_state(dev, PE_SNK_TRANSITION_TO_DEFAULT);
}

/**
 * @brief PE_SNK_Transition_to_default Entry State
 */
void pe_snk_transition_to_default_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	LOG_INF("PE_SNK_Transition_to_default");

	/* Reset flags */
	pe->flags = ATOMIC_INIT(0);
	pe->data_role = TC_ROLE_UFP;

	/*
	 * Indicate to the Device Policy Manager that the Sink Shall
	 * transition to default
	 */
	policy_notify(dev, SNK_TRANSITION_TO_DEFAULT);
	/*
	 * Request the Device Policy Manger that the Port Data Role is
	 * set to UFP
	 */
	policy_notify(dev, DATA_ROLE_IS_UFP);
}

/**
 * @brief PE_SNK_Transition_to_default Run State
 */
void pe_snk_transition_to_default_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	/*
	 * Wait until Device Policy Manager has transitioned the sink to
	 * default level
	 */
	if (policy_is_snk_at_default(dev)) {
		/* Inform the Protocol Layer that the Hard Reset is complete */
		prl_hard_reset_complete(dev);
		pe_set_state(dev, PE_SNK_STARTUP);
	}
}

/**
 * @brief PE_SNK_Get_Source_Cap Entry State
 */
void pe_snk_get_source_cap_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	LOG_INF("PE_SNK_Get_Source_Cap");

	/* Send a Get_Source_Cap Message */
	pe_send_ctrl_msg(dev, PD_PACKET_SOP, PD_CTRL_GET_SOURCE_CAP);
}

/**
 * @brief PE_SNK_Get_Source_Cap Run State
 */
void pe_snk_get_source_cap_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	struct protocol_layer_rx_t *prl_rx = data->prl_rx;

	/* Wait until message is sent or dropped */
	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_TX_COMPLETE)) {
		pe_set_state(dev, PE_SNK_READY);
	} else if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_MSG_DISCARDED)) {
		pe_send_soft_reset(dev, prl_rx->emsg.type);
	}
}

/**
 * @brief PE_SNK_Get_Source_Cap Exit State
 */
void pe_snk_get_source_cap_exit(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;

	usbc_timer_stop(&pe->pd_t_sender_response);
}

/**
 * @brief PE_Send_Soft_Reset Entry State
 */
void pe_send_soft_reset_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	LOG_INF("PE_SNK_Send_Soft_Reset");

	/* Reset Protocol Layer */
	prl_reset(dev);
	atomic_set_bit(&pe->flags, PE_FLAGS_SEND_SOFT_RESET);
}

/**
 * @brief PE_Send_Soft_Reset Run State
 */
void pe_send_soft_reset_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	struct protocol_layer_rx_t *prl_rx = data->prl_rx;
	union pd_header header;

	if (prl_is_running(dev) == false) {
		return;
	}

	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_SEND_SOFT_RESET)) {
		/* Send Soft Reset message */
		pe_send_ctrl_msg(dev, pe->soft_reset_sop, PD_CTRL_SOFT_RESET);
		return;
	}

	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_MSG_DISCARDED)) {
		/* Inform Device Policy Manager that the message was discarded */
		policy_notify(dev, MSG_DISCARDED);
		pe_set_state(dev, PE_SNK_READY);
	} else if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_TX_COMPLETE)) {
		/* Start SenderResponse timer */
		usbc_timer_start(&pe->pd_t_sender_response);
	} else if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_MSG_RECEIVED)) {
		header = prl_rx->emsg.header;

		if (received_control_message(dev, header, PD_CTRL_ACCEPT)) {
			pe_set_state(dev, PE_SNK_WAIT_FOR_CAPABILITIES);
		}
	} else if (atomic_test_bit(&pe->flags, PE_FLAGS_PROTOCOL_ERROR) ||
		   usbc_timer_expired(&pe->pd_t_sender_response)) {
		if (atomic_test_bit(&pe->flags, PE_FLAGS_PROTOCOL_ERROR)) {
			atomic_clear_bit(&pe->flags, PE_FLAGS_PROTOCOL_ERROR);
		} else {
			policy_notify(dev, PORT_PARTNER_NOT_RESPONSIVE);
		}
		pe_set_state(dev, PE_SNK_HARD_RESET);
	}
}

/**
 * @brief PE_Send_Soft_Reset Exit State
 */
void pe_send_soft_reset_exit(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;

	/* Stop Sender Response Timer */
	usbc_timer_stop(&pe->pd_t_sender_response);
}

/**
 * @brief PE_SNK_Soft_Reset Entry State
 */
void pe_soft_reset_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	LOG_INF("PE_SNK_Soft_Reset");

	/* Reset the Protocol Layer */
	prl_reset(dev);
	atomic_set_bit(&pe->flags, PE_FLAGS_SEND_SOFT_RESET);
}

/**
 * @brief PE_SNK_Soft_Reset Run State
 */
void  pe_soft_reset_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	if (prl_is_running(dev) == false) {
		return;
	}

	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_SEND_SOFT_RESET)) {
		/* Send Accept message */
		pe_send_ctrl_msg(dev, PD_PACKET_SOP, PD_CTRL_ACCEPT);
		return;
	}

	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_TX_COMPLETE)) {
		pe_set_state(dev, PE_SNK_WAIT_FOR_CAPABILITIES);
	} else if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_PROTOCOL_ERROR)) {
		pe_set_state(dev, PE_SNK_HARD_RESET);
	}
}

/**
 * @brief PE_Not_Supported Entry State
 */
void pe_send_not_supported_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	LOG_INF("PE_Not_Supported");

	/* Request the Protocol Layer to send a Not_Supported or Reject Message. */
	if (prl_get_rev(dev, PD_PACKET_SOP) > PD_REV20) {
		pe_send_ctrl_msg(dev, PD_PACKET_SOP, PD_CTRL_NOT_SUPPORTED);
	} else {
		pe_send_ctrl_msg(dev, PD_PACKET_SOP, PD_CTRL_REJECT);
	}
}

/**
 * @brief PE_Not_Supported Run State
 */
void pe_send_not_supported_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	if (atomic_test_bit(&pe->flags, PE_FLAGS_TX_COMPLETE) ||
			atomic_test_bit(&pe->flags, PE_FLAGS_MSG_DISCARDED)) {
		atomic_clear_bit(&pe->flags, PE_FLAGS_TX_COMPLETE);
		atomic_clear_bit(&pe->flags, PE_FLAGS_MSG_DISCARDED);
		pe_set_state(dev, PE_SNK_READY);
	}
}

/**
 * @brief PE_Chunk_Received Entry State
 */
void pe_chunk_received_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;

	LOG_INF("PE_SNK_Chunk_Received");

	usbc_timer_start(&pe->pd_t_chunking_not_supported);
}

/**
 * @brief PE_Chunk_Received Run State
 */
void pe_chunk_received_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;

	if (usbc_timer_expired(&pe->pd_t_chunking_not_supported)) {
		pe_set_state(dev, PE_SEND_NOT_SUPPORTED);
	}
}

/**
 * @brief PE_SNK_Give_Sink_Cap Entry state
 */
void pe_snk_give_sink_cap_entry(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	struct protocol_layer_tx_t *prl_tx = data->prl_tx;
	struct pd_msg *msg = &prl_tx->emsg;
	uint32_t *pdos;
	uint32_t num_pdos;

	/* Get present sink capabilities from Device Policy Manager */
	policy_get_snk_cap(dev, &pdos, &num_pdos);

	msg->len = PD_CONVERT_PD_HEADER_COUNT_TO_BYTES(num_pdos);
	memcpy(msg->data, (uint8_t *)pdos, msg->len);
	pe_send_data_msg(dev, PD_PACKET_SOP, PD_DATA_SINK_CAP);
}

/**
 * @brief PE_SNK_Give_Sink_Cap Run state
 */
void pe_snk_give_sink_cap_run(void *obj)
{
	struct policy_engine *pe = (struct policy_engine *)obj;
	const struct device *dev = pe->dev;
	struct usbc_port_data *data = dev->data;
	struct protocol_layer_rx_t *prl_rx = data->prl_rx;

	/* Wait until message is sent or dropped */
	if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_TX_COMPLETE)) {
		pe_set_state(dev, PE_SNK_READY);
	} else if (atomic_test_and_clear_bit(&pe->flags, PE_FLAGS_MSG_DISCARDED)) {
		pe_send_soft_reset(dev, prl_rx->emsg.type);
	}
}