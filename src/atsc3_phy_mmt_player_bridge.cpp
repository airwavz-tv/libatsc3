/*
 * atsc3_phy_mmt_player_bridge.cpp
 *
 *  Created on: Oct 3, 2019
 *      Author: jjustman
 *
 * Android MMT MFU Playback with SLS event driven callbacks
 *
 *
 * Note: atsc3NdkClient - Android NDK Binding against Lowasys API are not included
 */


#ifdef __FIXME_REFACTOR_LOWASIS__
#include "At3DrvIntf.h"
At3DrvIntf* atsc3NdkClientSL_ptr;

#else

#include "atsc3NdkClient.h"
atsc3NdkClient* atsc3NdkClientSL_ptr;
#endif

#include "atsc3_phy_mmt_player_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <limits.h>

#include "atsc3_listener_udp.h"
#include "atsc3_utils.h"

#include "atsc3_lls.h"


#include "atsc3_lls_slt_parser.h"
#include "atsc3_lls_sls_monitor_output_buffer_utils.h"

#include "atsc3_mmtp_packet_types.h"
#include "atsc3_mmtp_parser.h"
#include "atsc3_mmtp_ntp32_to_pts.h"
#include "atsc3_mmt_mpu_utils.h"

#include "atsc3_logging_externs.h"

#include "atsc3_mmt_context_mfu_depacketizer.h"

#include "atsc3_hevc_nal_extractor.h"
#include "atsc3_lls_types.h"

#include "atsc3_lls_alc_utils.h"
#include "atsc3_alc_rx.h"
#include "atsc3_alc_utils.h"
#include "atsc3_alp_types.h"


//A/330 LMT management
atsc3_link_mapping_table* atsc3_link_mapping_table_last = NULL;

//commandline stream filtering

uint32_t* dst_ip_addr_filter = NULL;
uint16_t* dst_ip_port_filter = NULL;
uint16_t* dst_packet_id_filter = NULL;

//jjustman-2019-10-03 - context event callbacks...
atsc3_mmt_mfu_context_t* atsc3_mmt_mfu_context = NULL;
lls_slt_monitor_t* lls_slt_monitor = NULL;

void atsc3_lls_sls_alc_on_object_close_flag_s_tsid_content_location_ndk(uint32_t tsi, uint32_t toi, char* content_location);
void atsc3_lls_sls_alc_on_route_mpd_patched_ndk(uint16_t service_id);

//mmtp/sls flow management
mmtp_flow_t* mmtp_flow;
udp_flow_latest_mpu_sequence_number_container_t* udp_flow_latest_mpu_sequence_number_container;
lls_sls_mmt_monitor_t* lls_sls_mmt_monitor = NULL;

//route/alc specific parameters
lls_sls_alc_monitor_t* lls_sls_alc_monitor = NULL;
atsc3_alc_arguments_t* alc_arguments;

std::string atsc3_ndk_cache_temp_folder_path;

//these should actually be referenced from mmt_sls_monitor for proper flow references
uint16_t global_video_packet_id = 0;
uint16_t global_audio_packet_id = 0;
uint16_t global_stpp_packet_id = 0;

uint32_t global_mfu_proccessed_count = 0;

mmtp_packet_header_t*  mmtp_parse_header_from_udp_packet(udp_packet_t* udp_packet) {

    mmtp_packet_header_t* mmtp_packet_header = mmtp_packet_header_parse_from_block_t(udp_packet->data);

    if(!mmtp_packet_header) {
        __ERROR("mmtp_parse_header_from_udp_packet: mmtp_packet_header_parse_from_block_t: raw packet ptr is null, parsing failed for flow: %d.%d.%d.%d:(%-10u):%-5u \t ->  %d.%d.%d.%d:(%-10u):%-5u ",
                __toipandportnonstruct(udp_packet->udp_flow.src_ip_addr, udp_packet->udp_flow.src_port),
                udp_packet->udp_flow.src_ip_addr,
                __toipandportnonstruct(udp_packet->udp_flow.dst_ip_addr, udp_packet->udp_flow.dst_port),
                udp_packet->udp_flow.dst_ip_addr);
        return NULL;
    }

    return mmtp_packet_header;
}

void mmtp_process_sls_from_payload(udp_packet_t *udp_packet, mmtp_signalling_packet_t* mmtp_signalling_packet, lls_sls_mmt_session_t* matching_lls_slt_mmt_session) {

    __INFO("mmtp_process_sls_from_payload: processing mmt flow: %d.%d.%d.%d:(%u) packet_id: %d, signalling message: %p",
           __toipandportnonstruct(udp_packet->udp_flow.dst_ip_addr, udp_packet->udp_flow.dst_port),
           mmtp_signalling_packet->mmtp_packet_id,
           mmtp_signalling_packet);

    mmt_signalling_message_dump(mmtp_signalling_packet);
}

atsc3_lls_slt_service_t* atsc3_phy_mmt_player_bridge_set_single_monitor_a331_service_id(int service_id) {
    __INFO("atsc3_phy_mmt_player_bridge_set_a331_service_id: with service_id: %d", service_id);
    //find our matching LLS service, then assign a monitor reference

    atsc3_lls_slt_service_t* atsc3_lls_slt_service = lls_slt_monitor_find_lls_slt_service_id_group_id_cache_entry(lls_slt_monitor, service_id);
    if(!atsc3_lls_slt_service) {
        __ERROR("atsc3_phy_mmt_player_bridge_set_a331_service_id: unable to find service_id: %d", service_id);
        return NULL;
    }

    //jjustman-2019-10-19: todo: refactor this out
    atsc3_slt_broadcast_svc_signalling_t* atsc3_slt_broadcast_svc_signalling = NULL;
    atsc3_slt_broadcast_svc_signalling_t* atsc3_slt_broadcast_svc_signalling_mmt = NULL;
    atsc3_slt_broadcast_svc_signalling_t* atsc3_slt_broadcast_svc_signalling_route = NULL;

    //broadcast_svc_signalling has cardinality (0..1), any other signalling location is represented by SvcInetUrl (0..N)

    for(int i=0; i < atsc3_lls_slt_service->atsc3_slt_broadcast_svc_signalling_v.count; i++) {
        atsc3_slt_broadcast_svc_signalling = atsc3_lls_slt_service->atsc3_slt_broadcast_svc_signalling_v.data[i];

        if(atsc3_slt_broadcast_svc_signalling->sls_protocol == SLS_PROTOCOL_MMTP || atsc3_slt_broadcast_svc_signalling->sls_protocol == SLS_PROTOCOL_ROUTE) {
            //check if we need to add this PLP based upon our LMT to the phy listener
            //if(atsc3_slt_broadcast_svc_signalling->sls_source_ip_address
            if(atsc3_link_mapping_table_last != NULL) {
                for(int j=0; j < atsc3_link_mapping_table_last->atsc3_link_mapping_table_plp_v.count; j++) {
                    atsc3_link_mapping_table_plp_t* atsc3_link_mapping_table_plp = atsc3_link_mapping_table_last->atsc3_link_mapping_table_plp_v.data[j];
                    for(int k=0; k < atsc3_link_mapping_table_plp->atsc3_link_mapping_table_multicast_v.count; k++) {
                        atsc3_link_mapping_table_multicast_t* atsc3_link_mapping_table_multicast = atsc3_link_mapping_table_plp->atsc3_link_mapping_table_multicast_v.data[k];

                        uint32_t sls_destination_ip_address = parseIpAddressIntoIntval(atsc3_slt_broadcast_svc_signalling->sls_destination_ip_address);
                        uint16_t sls_destination_udp_port = parsePortIntoIntval(atsc3_slt_broadcast_svc_signalling->sls_destination_udp_port);

                        if(atsc3_link_mapping_table_multicast->dst_ip_add == sls_destination_ip_address &&
                            atsc3_link_mapping_table_multicast->dst_udp_port == sls_destination_udp_port) {
                            atsc3NdkClientSL_ptr->LogMsgF("atsc3_phy_mmt_player_bridge_set_single_monitor_a331_service_id: SLS adding PLP_id: %d (%s: %s)",
                                    atsc3_link_mapping_table_plp->PLP_ID,
                                    atsc3_slt_broadcast_svc_signalling->sls_destination_ip_address,
                                    atsc3_slt_broadcast_svc_signalling->sls_destination_udp_port);
// JBC - Function is missing               atsc3NdkClientSL_ptr->ListenPLP1(atsc3_link_mapping_table_plp->PLP_ID);
                        }
                    }
                }
            }


            if(atsc3_slt_broadcast_svc_signalling->sls_protocol == SLS_PROTOCOL_MMTP) {
                atsc3_slt_broadcast_svc_signalling_mmt = atsc3_slt_broadcast_svc_signalling;
            } else if(atsc3_slt_broadcast_svc_signalling->sls_protocol == SLS_PROTOCOL_ROUTE) {
                atsc3_slt_broadcast_svc_signalling_route = atsc3_slt_broadcast_svc_signalling;
            }
        }
    }

    //wire up MMT, watch out for potentally free'd sessions that aren't NULL'd out properly..
    if(atsc3_slt_broadcast_svc_signalling_mmt != NULL) {
        __INFO("atsc3_phy_mmt_player_bridge_set_a331_service_id: service_id: %d - using MMT with flow: sip: %s, dip: %s:%s",
               service_id,
               atsc3_slt_broadcast_svc_signalling_mmt->sls_source_ip_address,
               atsc3_slt_broadcast_svc_signalling_mmt->sls_destination_ip_address,
               atsc3_slt_broadcast_svc_signalling_mmt->sls_destination_udp_port);

        //clear any active SLS monitors
        lls_slt_monitor_clear_lls_sls_mmt_monitor(lls_slt_monitor);

        //TODO - remove this logic to a unified process...
        lls_slt_monitor_clear_lls_sls_alc_monitor(lls_slt_monitor);
        lls_slt_monitor->lls_sls_alc_monitor = NULL;
        lls_sls_alc_monitor = NULL; //make sure to clear out our ref

        lls_sls_mmt_monitor = lls_sls_mmt_monitor_create();
        lls_sls_mmt_monitor->atsc3_lls_slt_service = atsc3_lls_slt_service; //HACK!
        lls_slt_service_id_t* lls_slt_service_id = lls_slt_service_id_new_from_atsc3_lls_slt_service(atsc3_lls_slt_service);

        lls_slt_monitor_add_lls_slt_service_id(lls_slt_monitor, lls_slt_service_id);

        //we may not be initialized yet, so re-check again later
        //this should _never_happen...
        lls_sls_mmt_session_t* lls_sls_mmt_session = lls_slt_mmt_session_find_from_service_id(lls_slt_monitor, atsc3_lls_slt_service->service_id);

        if(!lls_sls_mmt_session) {
            __WARN("lls_slt_mmt_session_find_from_service_id: lls_sls_mmt_session is NULL!");
        }
        lls_sls_mmt_monitor->lls_mmt_session = lls_sls_mmt_session;
        lls_slt_monitor->lls_sls_mmt_monitor = lls_sls_mmt_monitor;

        lls_slt_monitor_add_lls_sls_mmt_monitor(lls_slt_monitor, lls_sls_mmt_monitor);
    } else {
        lls_slt_monitor_clear_lls_sls_mmt_monitor(lls_slt_monitor);
        if(lls_slt_monitor->lls_sls_mmt_monitor) {
            lls_sls_mmt_monitor_free(&lls_slt_monitor->lls_sls_mmt_monitor);
        }
        lls_sls_mmt_monitor = NULL;

    }

    //wire up ROUTE
    if(atsc3_slt_broadcast_svc_signalling_route != NULL) {
        __INFO("atsc3_phy_mmt_player_bridge_set_a331_service_id: service_id: %d - using ROUTE with flow: sip: %s, dip: %s:%s",
               service_id,
               atsc3_slt_broadcast_svc_signalling_route->sls_source_ip_address,
               atsc3_slt_broadcast_svc_signalling_route->sls_destination_ip_address,
               atsc3_slt_broadcast_svc_signalling_route->sls_destination_udp_port);

        lls_slt_monitor_clear_lls_sls_alc_monitor(lls_slt_monitor);

        lls_sls_alc_monitor = lls_sls_alc_monitor_create();
        lls_sls_alc_monitor->atsc3_lls_slt_service = atsc3_lls_slt_service;
        lls_sls_alc_monitor->lls_sls_monitor_output_buffer_mode.file_dump_enabled = true;
        lls_slt_service_id_t* lls_slt_service_id = lls_slt_service_id_new_from_atsc3_lls_slt_service(atsc3_lls_slt_service);
        lls_slt_monitor_add_lls_slt_service_id(lls_slt_monitor, lls_slt_service_id);

        lls_sls_alc_session_t* lls_sls_alc_session = lls_slt_alc_session_find_from_service_id(lls_slt_monitor, atsc3_lls_slt_service->service_id);
        if(!lls_sls_alc_session) {
            __WARN("lls_slt_alc_session_find_from_service_id: lls_sls_alc_session is NULL!");
        }
        lls_sls_alc_monitor->lls_alc_session = lls_sls_alc_session;
        lls_slt_monitor->lls_sls_alc_monitor = lls_sls_alc_monitor;

        lls_slt_monitor_add_lls_sls_alc_monitor(lls_slt_monitor, lls_sls_alc_monitor);

        //wire up event callback for alc close_object notification
        lls_sls_alc_monitor->atsc3_lls_sls_alc_on_object_close_flag_s_tsid_content_location = &atsc3_lls_sls_alc_on_object_close_flag_s_tsid_content_location_ndk;

        //write up event callback for alc MPD patching
        lls_sls_alc_monitor->atsc3_lls_sls_alc_on_route_mpd_patched = &atsc3_lls_sls_alc_on_route_mpd_patched_ndk;


    } else {
        lls_slt_monitor_clear_lls_sls_alc_monitor(lls_slt_monitor);
        if(lls_slt_monitor->lls_sls_alc_monitor) {
            lls_sls_alc_monitor_free(&lls_slt_monitor->lls_sls_alc_monitor);
        }
        lls_sls_alc_monitor = NULL;
    }

    return atsc3_lls_slt_service;
}

atsc3_lls_slt_service_t* atsc3_phy_mmt_player_bridge_add_monitor_a331_service_id(int service_id) {
    __INFO("atsc3_phy_mmt_player_bridge_add_monitor_a331_service_id: with service_id: %d", service_id);

    atsc3_lls_slt_service_t* atsc3_lls_slt_service = lls_slt_monitor_find_lls_slt_service_id_group_id_cache_entry(lls_slt_monitor, service_id);
    if(!atsc3_lls_slt_service) {
        __ERROR("atsc3_phy_mmt_player_bridge_add_monitor_a331_service_id: unable to find service_id: %d", service_id);
        return NULL;
    }

    atsc3_slt_broadcast_svc_signalling_t* atsc3_slt_broadcast_svc_signalling = NULL;
    atsc3_slt_broadcast_svc_signalling_t* atsc3_slt_broadcast_svc_signalling_route_to_add_monitor = NULL;

    for(int i=0; i < atsc3_lls_slt_service->atsc3_slt_broadcast_svc_signalling_v.count; i++) {
        atsc3_slt_broadcast_svc_signalling = atsc3_lls_slt_service->atsc3_slt_broadcast_svc_signalling_v.data[i];
         if(atsc3_slt_broadcast_svc_signalling->sls_protocol == SLS_PROTOCOL_ROUTE) {
             atsc3_slt_broadcast_svc_signalling_route_to_add_monitor = atsc3_slt_broadcast_svc_signalling;
        }
    }

    if(atsc3_slt_broadcast_svc_signalling_route_to_add_monitor == NULL) {
        __WARN("atsc3_phy_mmt_player_bridge_add_monitor_a331_service_id: unable to find ALC service_id: %d",
              service_id);
        return NULL;
    }

    __INFO("atsc3_phy_mmt_player_bridge_add_monitor_a331_service_id: service_id: %d - adding ROUTE with flow: sip: %s, dip: %s:%s",
           service_id,
           atsc3_slt_broadcast_svc_signalling_route_to_add_monitor->sls_source_ip_address,
           atsc3_slt_broadcast_svc_signalling_route_to_add_monitor->sls_destination_ip_address,
           atsc3_slt_broadcast_svc_signalling_route_to_add_monitor->sls_destination_udp_port);

    lls_sls_alc_monitor = lls_sls_alc_monitor_create();
    lls_sls_alc_monitor->atsc3_lls_slt_service = atsc3_lls_slt_service;

    lls_sls_alc_session_t* lls_sls_alc_session = lls_slt_alc_session_find_from_service_id(lls_slt_monitor, atsc3_lls_slt_service->service_id);
    if(!lls_sls_alc_session) {
        __WARN("lls_slt_alc_session_find_from_service_id: %d, lls_sls_alc_session is NULL!", service_id);
        return NULL;
    }
    lls_sls_alc_monitor->lls_alc_session = lls_sls_alc_session;
    lls_slt_monitor_add_lls_sls_alc_monitor(lls_slt_monitor, lls_sls_alc_monitor);

    //add in supplimentary callback hook for additional ALC emissions
    lls_sls_alc_monitor->atsc3_lls_sls_alc_on_object_close_flag_s_tsid_content_location = &atsc3_lls_sls_alc_on_object_close_flag_s_tsid_content_location_ndk;


    //add a supplimentry sls_alc monitor
    // TODO: fix me? NOTE: do not replace the primary lls_slt_monitor->lls_sls_alc_monitor entry if set
    if(!lls_slt_monitor->lls_sls_alc_monitor) {
        lls_slt_monitor->lls_sls_alc_monitor = lls_sls_alc_monitor;
    }

    return atsc3_lls_slt_service;
}

//TODO: jjustman-2019-11-07: mutex
atsc3_lls_slt_service_t* atsc3_phy_mmt_player_bridge_remove_monitor_a331_service_id(int service_id) {
    if(!lls_slt_monitor->lls_sls_alc_monitor || !lls_slt_monitor->lls_sls_alc_monitor_v.count) {
        __ERROR("atsc3_phy_mmt_player_bridge_remove_monitor_a331_service_id: unable to remove service_id: %d, lls_slt_monitor->lls_sls_alc_monitor is: %p, lls_slt_monitor->lls_sls_alc_monitor_v.count is: %d",
                service_id,
                lls_slt_monitor->lls_sls_alc_monitor,
                lls_slt_monitor->lls_sls_alc_monitor_v.count);
        return NULL;
    }

    atsc3_lls_slt_service_t* lls_service_removed_lls_sls_alc_monitor = NULL;
    lls_sls_alc_monitor_t* my_lls_sls_alc_monitor_entry_to_release = NULL;
    if(lls_slt_monitor->lls_sls_alc_monitor && lls_slt_monitor->lls_sls_alc_monitor->atsc3_lls_slt_service->service_id == service_id) {
        my_lls_sls_alc_monitor_entry_to_release = lls_slt_monitor->lls_sls_alc_monitor;
        lls_slt_monitor->lls_sls_alc_monitor = NULL;
    }

    //remove lls_sls_alc_monitor entry
    //TODO: jjustman-2019-11-07: hack, move this into atsc3_vector_builder.h
    for(int i=0; i < lls_slt_monitor->lls_sls_alc_monitor_v.count && !my_lls_sls_alc_monitor_entry_to_release; i++) {
        lls_sls_alc_monitor_t* lls_sls_alc_monitor = lls_slt_monitor->lls_sls_alc_monitor_v.data[i];
        if(lls_sls_alc_monitor->atsc3_lls_slt_service->service_id == service_id) {
            my_lls_sls_alc_monitor_entry_to_release = lls_sls_alc_monitor;
            for(int j=i + 1; j < lls_slt_monitor->lls_sls_alc_monitor_v.count; j++) {
                lls_slt_monitor->lls_sls_alc_monitor_v.data[j-1] = lls_slt_monitor->lls_sls_alc_monitor_v.data[j];
            }
            lls_slt_monitor->lls_sls_alc_monitor_v.data[lls_slt_monitor->lls_sls_alc_monitor_v.count-1] = NULL;
            lls_slt_monitor->lls_sls_alc_monitor_v.count--;
            //clear out last entry
        }
    }

    if(my_lls_sls_alc_monitor_entry_to_release) {
        lls_service_removed_lls_sls_alc_monitor = my_lls_sls_alc_monitor_entry_to_release->atsc3_lls_slt_service;
        lls_sls_alc_monitor_free(&my_lls_sls_alc_monitor_entry_to_release);
    }

    return lls_service_removed_lls_sls_alc_monitor;
}

lls_sls_alc_monitor_t* atsc3_lls_sls_alc_monitor_get_from_service_id(int service_id) {
    if(!lls_slt_monitor->lls_sls_alc_monitor || !lls_slt_monitor->lls_sls_alc_monitor_v.count) {
        __ERROR("atsc3_lls_sls_alc_monitor_get_from_service_id: error searching for service_id: %d, alc_monitor is null: lls_slt_monitor->lls_sls_alc_monitor is: %p, lls_slt_monitor->lls_sls_alc_monitor_v.count is: %d",
                service_id,
                lls_slt_monitor->lls_sls_alc_monitor,
                lls_slt_monitor->lls_sls_alc_monitor_v.count);
        return NULL;
    }

    lls_sls_alc_monitor_t* lls_sls_alc_monitor_to_return = NULL;
    for(int i=0; i < lls_slt_monitor->lls_sls_alc_monitor_v.count && !lls_sls_alc_monitor_to_return; i++) {
        lls_sls_alc_monitor_t* lls_sls_alc_monitor = lls_slt_monitor->lls_sls_alc_monitor_v.data[i];
        if(lls_sls_alc_monitor->atsc3_lls_slt_service->service_id == service_id) {
            lls_sls_alc_monitor_to_return = lls_sls_alc_monitor;
            continue;
        }
    }

    if(!lls_sls_alc_monitor_to_return) {
        __ERROR("atsc3_lls_sls_alc_monitor_get_from_service_id: service_id: %d, lls_sls_alc_monitor_to_return is null!",
                service_id);
        return NULL;
    }

    __INFO("atsc3_lls_sls_alc_monitor_get_from_service_id: %d, returning lls_sls_alc_monitor_to_return: %p",
           service_id,
           lls_sls_alc_monitor_to_return);

    return lls_sls_alc_monitor_to_return;
}

atsc3_sls_metadata_fragments_t* atsc3_slt_alc_get_sls_metadata_fragments_from_monitor_service_id(int service_id) {
    lls_sls_alc_monitor_t* lls_sls_alc_monitor = atsc3_lls_sls_alc_monitor_get_from_service_id(service_id);

    if(!lls_sls_alc_monitor || !lls_sls_alc_monitor->atsc3_sls_metadata_fragments) {
        __ERROR("atsc3_slt_alc_get_sls_metadata_fragments_from_monitor_service_id: service_id: %d, alc_monitor or fragments were null, lls_sls_alc_monitor: %p", service_id, lls_sls_alc_monitor);
        return NULL;
    }

    __INFO("atsc3_slt_alc_get_sls_metadata_fragments_from_monitor_service_id: %d, returning atsc3_sls_metadata_fragments: %p",
           service_id,
           lls_sls_alc_monitor->atsc3_sls_metadata_fragments);

    return lls_sls_alc_monitor->atsc3_sls_metadata_fragments;
}

atsc3_route_s_tsid_t* atsc3_slt_alc_get_sls_route_s_tsid_from_monitor_service_id(int service_id) {
    lls_sls_alc_monitor_t* lls_sls_alc_monitor = atsc3_lls_sls_alc_monitor_get_from_service_id(service_id);

    if(!lls_sls_alc_monitor || !lls_sls_alc_monitor->atsc3_sls_metadata_fragments || !lls_sls_alc_monitor->atsc3_sls_metadata_fragments->atsc3_route_s_tsid) {
        __ERROR("atsc3_slt_alc_get_sls_route_s_tsid_from_monitor_service_id: service_id: %d, alc_monitor or fragments or route_s_tsid were null, lls_sls_alc_monitor: %p", service_id, lls_sls_alc_monitor);
        return NULL;
    }

    __INFO("atsc3_slt_alc_get_sls_route_s_tsid_from_monitor_service_id: %d, returning atsc3_route_s_tsid: %p",
           service_id,
           lls_sls_alc_monitor->atsc3_sls_metadata_fragments->atsc3_route_s_tsid);

    return lls_sls_alc_monitor->atsc3_sls_metadata_fragments->atsc3_route_s_tsid;
}

/**
 * else {
        lls_slt_monitor_clear_lls_sls_alc_monitor(lls_slt_monitor);
        if(lls_slt_monitor->lls_sls_alc_monitor) {
            lls_sls_alc_monitor_free(&lls_slt_monitor->lls_sls_alc_monitor);
        }

 * @param packet
 */
//void process_packet(u_char *user, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
void atsc3_phy_mmt_player_bridge_process_packet_phy(block_t* packet) {

    mmtp_packet_header_t* mmtp_packet_header = NULL;
    lls_sls_mmt_session_t* matching_lls_sls_mmt_session = NULL;


    alc_packet_t* alc_packet = NULL;
    lls_sls_alc_session_t* matching_lls_slt_alc_session = NULL;

    //lowasys hands off the ip packet header, not phy eth frame

    //udp_packet_t* udp_packet = udp_packet_process_from_ptr_raw_ethernet_packet(block_Get(packet), packet->p_size);
    udp_packet_t* udp_packet = udp_packet_process_from_ptr(block_Get(packet), packet->p_size);

    if(!udp_packet) {
        __ERROR("after udp_packet_process_from_ptr: unable to extract packet size: %d, i_pos: %d, 0x%02x 0x%02x",
                packet->p_size,
                packet->i_pos,
                packet->p_buffer[0],
                packet->p_buffer[1]);

        return;
    }

    //drop mdNS
    if(udp_packet->udp_flow.dst_ip_addr == UDP_FILTER_MDNS_IP_ADDRESS && udp_packet->udp_flow.dst_port == UDP_FILTER_MDNS_PORT) {
        goto cleanup;
    }

    //don't auto-select service here, let the lls_slt_monitor->atsc3_lls_on_sls_table_present event callback trigger in a service selection
    if(udp_packet->udp_flow.dst_ip_addr == LLS_DST_ADDR && udp_packet->udp_flow.dst_port == LLS_DST_PORT) {
        //at3DrvIntf_ptr->LogMsgF("atsc3_phy_mmt_player_bridge_process_packet_phy got packet: LLS, %p", udp_packet);
        __DEBUG("LLS packet: dst_ip_addr: %u.%u.%u.%u:%u",
                __toipnonstruct(udp_packet->udp_flow.dst_ip_addr),
                udp_packet->udp_flow.dst_port);
        lls_table_t* lls_table = lls_table_create_or_update_from_lls_slt_monitor(lls_slt_monitor, udp_packet->data);
        if(lls_table) {
            if(lls_table->lls_table_id == SLT) {
                //capture our SLT services into alc and mmt session flows
                int retval = lls_slt_table_perform_update(lls_table, lls_slt_monitor);

                if(!retval) {
                    lls_dump_instance_table(lls_table);
                }
            }
        }

        goto cleanup;
    }

//    __ERROR("ALP packet: dst_ip_addr: %u.%u.%u.%u:%u",
//            __toipnonstruct(udp_packet->udp_flow.dst_ip_addr),
//            udp_packet->udp_flow.dst_port);
    //ALC: Find a matching SLS service from this packet flow, and if the selected atsc3_lls_slt_service is monitored, write MBMS/MPD and MDE's out to disk
    matching_lls_slt_alc_session = lls_slt_alc_session_find_from_udp_packet(lls_slt_monitor, udp_packet->udp_flow.src_ip_addr, udp_packet->udp_flow.dst_ip_addr, udp_packet->udp_flow.dst_port);

    if((lls_sls_alc_monitor && matching_lls_slt_alc_session && lls_sls_alc_monitor->atsc3_lls_slt_service &&  (lls_sls_alc_monitor->atsc3_lls_slt_service->service_id == matching_lls_slt_alc_session->atsc3_lls_slt_service->service_id))  ||
       ((dst_ip_addr_filter != NULL && dst_ip_port_filter != NULL) && (udp_packet->udp_flow.dst_ip_addr == *dst_ip_addr_filter && udp_packet->udp_flow.dst_port == *dst_ip_port_filter))) {

        //parse and process ALC flow
        int retval = alc_rx_analyze_packet_a331_compliant((char*)block_Get(udp_packet->data), block_Remaining_size(udp_packet->data), &alc_packet);
        if(!retval) {
            //__DEBUG("atsc3_phy_mmt_player_bridge_process_packet_phy: alc_packet: %p, tsi: %d, toi: %d", alc_packet, alc_packet->def_lct_hdr->tsi, alc_packet->def_lct_hdr->toi);

            //check our alc_packet for a wrap-around TOI value, if it is a monitored TSI, and re-patch the MBMS MPD for updated availabilityStartTime and startNumber with last closed TOI values
            atsc3_alc_packet_check_monitor_flow_for_toi_wraparound_discontinuity(alc_packet, lls_slt_monitor->lls_sls_alc_monitor);

            //keep track of our EXT_FTI and update last_toi as needed for TOI length and manual set of the close_object flag
            atsc3_alc_persist_route_ext_attributes_per_lls_sls_alc_monitor_essence(alc_packet, lls_slt_monitor->lls_sls_alc_monitor);

            //persist to disk, process sls mbms and/or emit ROUTE media_delivery_event complete to the application tier if
            //the full packet has been recovered (e.g. no missing data units in the forward transmission)
            atsc3_alc_packet_persist_to_toi_resource_process_sls_mbms_and_emit_callback(&udp_packet->udp_flow, &alc_packet, lls_slt_monitor->lls_sls_alc_monitor);
        } else {
            __ERROR("Error in ALC decode: %d", retval);
        }
        goto cleanup;
    }


    //MMT: Find a matching SLS service from this packet flow, and if the selected atsc3_lls_slt_service is monitored, enqueue for MFU DU re-constituion and emission
    matching_lls_sls_mmt_session = lls_slt_mmt_session_find_from_udp_packet(lls_slt_monitor, udp_packet->udp_flow.src_ip_addr, udp_packet->udp_flow.dst_ip_addr, udp_packet->udp_flow.dst_port);
    __TRACE("Checking matching_lls_sls_mmt_session: %p,", matching_lls_sls_mmt_session);

	if(matching_lls_sls_mmt_session && lls_slt_monitor && lls_slt_monitor->lls_sls_mmt_monitor && matching_lls_sls_mmt_session->atsc3_lls_slt_service->service_id == lls_slt_monitor->lls_sls_mmt_monitor->atsc3_lls_slt_service->service_id) {

        mmtp_packet_header = mmtp_packet_header_parse_from_block_t(udp_packet->data);

        //at3DrvIntf_ptr->LogMsgF("mmtp_packet_header: %p", mmtp_packet_header);

        if(!mmtp_packet_header) {
            goto cleanup;
        }

        //for filtering MMT flows by a specific packet_id
        if(dst_packet_id_filter && *dst_packet_id_filter != mmtp_packet_header->mmtp_packet_id) {
            goto cleanup;
        }

        mmtp_packet_header_dump(mmtp_packet_header);

        //dump header, then dump applicable packet type
        if(mmtp_packet_header->mmtp_payload_type == 0x0) {
            //mmtp_mpu_packet_t* mmtp_mpu_packet = mmtp_mpu_packet_parse_from_block_t();
            mmtp_mpu_packet_t* mmtp_mpu_packet = mmtp_mpu_packet_parse_and_free_packet_header_from_block_t(&mmtp_packet_header, udp_packet->data);
            if(!mmtp_mpu_packet) {
                goto error;
            }

            if(mmtp_mpu_packet->mpu_timed_flag == 1) {
                mmtp_mpu_dump_header(mmtp_mpu_packet);

                //TODO: jjustman-2019-10-03 - handle context parameters better
                // mmtp_flow, lls_slt_monitor, , udp_flow_latest_mpu_sequence_number_container, matching_lls_sls_mmt_session);

                atsc3_mmt_mfu_context->mmtp_flow = mmtp_flow;
                atsc3_mmt_mfu_context->udp_flow_latest_mpu_sequence_number_container = udp_flow_latest_mpu_sequence_number_container;
                atsc3_mmt_mfu_context->lls_slt_monitor = lls_slt_monitor;
                atsc3_mmt_mfu_context->matching_lls_sls_mmt_session = matching_lls_sls_mmt_session;

                __TRACE("process_packet: mmtp_mfu_process_from_payload_with_context with udp_packet: %p, mmtp_mpu_packet: %p, atsc3_mmt_mfu_context: %p,",
                        udp_packet, mmtp_mpu_packet, atsc3_mmt_mfu_context);

               // at3DrvIntf_ptr->LogMsgF("process_packet: mmtp_mfu_process_from_payload_with_context with udp_packet: %p, mmtp_mpu_packet: %p, atsc3_mmt_mfu_context: %p,", udp_packet, mmtp_mpu_packet, atsc3_mmt_mfu_context);
                mmtp_mfu_process_from_payload_with_context(udp_packet, mmtp_mpu_packet, atsc3_mmt_mfu_context);

            } else {
                //non-timed
                __ATSC3_WARN("process_packet: mmtp_packet_header_parse_from_block_t - non-timed payload: packet_id: %u", mmtp_packet_header->mmtp_packet_id);
            }
        } else if(mmtp_packet_header->mmtp_payload_type == 0x2) {

            mmtp_signalling_packet_t* mmtp_signalling_packet = mmtp_signalling_packet_parse_and_free_packet_header_from_block_t(&mmtp_packet_header, udp_packet->data);
            uint8_t parsed_count = mmt_signalling_message_parse_packet(mmtp_signalling_packet, udp_packet->data);
            if(parsed_count) {
                mmt_signalling_message_dump(mmtp_signalling_packet);

                __TRACE("process_packet: calling mmt_signalling_message_process_with_context with udp_packet: %p, mmtp_signalling_packet: %p, atsc3_mmt_mfu_context: %p,",
                        udp_packet,
                        mmtp_signalling_packet,
                        atsc3_mmt_mfu_context);

                mmt_signalling_message_process_with_context(udp_packet, mmtp_signalling_packet, atsc3_mmt_mfu_context);


                //internal hacks below


                //TODO: jjustman-2019-10-03 - if signalling_packet == MP_table, set atsc3_mmt_mfu_context->mp_table_last;
                mmtp_asset_flow_t* mmtp_asset_flow = mmtp_flow_find_or_create_from_udp_packet(mmtp_flow, udp_packet);
                mmtp_asset_t* mmtp_asset = mmtp_asset_flow_find_or_create_asset_from_lls_sls_mmt_session(mmtp_asset_flow, matching_lls_sls_mmt_session);

                //TODO: FIX ME!!! HACK - jjustman-2019-09-05
                mmtp_mpu_packet_t* mmtp_mpu_packet = mmtp_mpu_packet_new();
                mmtp_mpu_packet->mmtp_packet_id = mmtp_signalling_packet->mmtp_packet_id;

                mmtp_packet_id_packets_container_t* mmtp_packet_id_packets_container = mmtp_asset_find_or_create_packets_container_from_mmt_mpu_packet(mmtp_asset, mmtp_mpu_packet);
                mmtp_packet_id_packets_container_add_mmtp_signalling_packet(mmtp_packet_id_packets_container, mmtp_signalling_packet);

                //TODO: FIX ME!!! HACK - jjustman-2019-09-05
                mmtp_mpu_packet_free(&mmtp_mpu_packet);

                //update our sls_mmt_session info
                mmt_signalling_message_update_lls_sls_mmt_session(mmtp_signalling_packet, matching_lls_sls_mmt_session);

                //TODO - remap this
                //add in flows 				lls_sls_mmt_session_t* lls_sls_mmt_session = lls_slt_mmt_session_find_from_service_id(lls_slt_monitor, lls_sls_mmt_monitor->lls_mmt_session->service_id);

                if(lls_sls_mmt_monitor && lls_sls_mmt_monitor->lls_mmt_session && matching_lls_sls_mmt_session) {
                	__TRACE("mmt_signalling_information: from atsc3 service_id: %u, patching: seting audio_packet_id/video_packet_id/stpp_packet_id: %u, %u, %u",
                								matching_lls_sls_mmt_session->atsc3_lls_slt_service->service_id,
                								matching_lls_sls_mmt_session->audio_packet_id,
                								matching_lls_sls_mmt_session->video_packet_id,
                								matching_lls_sls_mmt_session->stpp_packet_id);

                    if(matching_lls_sls_mmt_session->audio_packet_id) {
                        lls_sls_mmt_monitor->audio_packet_id = matching_lls_sls_mmt_session->audio_packet_id;
                    }

                    if(matching_lls_sls_mmt_session->video_packet_id) {
                        lls_sls_mmt_monitor->video_packet_id = matching_lls_sls_mmt_session->video_packet_id;
                    }
					if(matching_lls_sls_mmt_session->stpp_packet_id) {
						lls_sls_mmt_monitor->stpp_packet_id = matching_lls_sls_mmt_session->stpp_packet_id;
					}

                }
            }

        } else {
            __ATSC3_WARN("process_packet: mmtp_packet_header_parse_from_block_t - unknown payload type of 0x%x", mmtp_packet_header->mmtp_payload_type);
            goto cleanup;
        }
    }

cleanup:

	if(mmtp_packet_header) {
		mmtp_packet_header_free(&mmtp_packet_header);
	}
	if(alc_packet) {
	    alc_packet_free(&alc_packet);
	}

	if(udp_packet) {
        udp_packet_free(&udp_packet);
    }
    return;

error:
    __ATSC3_WARN("process_packet: error, bailing loop!");
    return;
}

/**
 * NDK to JNI bridiging methods defined here
 * @param lls_table
 */


void atsc3_lls_on_sls_table_present_ndk(lls_table_t* lls_table) {
    if(!lls_table) {
        atsc3NdkClientSL_ptr->LogMsg("E: atsc3_lls_on_sls_table_present_ndk: no lls_table for SLS!");
        return;
    }
    if(!lls_table->raw_xml.xml_payload || !lls_table->raw_xml.xml_payload_size) {
        atsc3NdkClientSL_ptr->LogMsg("E: atsc3_lls_on_sls_table_present_ndk: no raw_xml.xml_payload for SLS!");
        return;
    }

    //copy over our xml_payload.size +1 with a null
    int len_aligned = lls_table->raw_xml.xml_payload_size + 1;
    len_aligned += 8-(len_aligned%8);
    char* xml_payload_copy = (char*)calloc(len_aligned , sizeof(char));
    strncpy(xml_payload_copy, (char*)lls_table->raw_xml.xml_payload, lls_table->raw_xml.xml_payload_size);

    atsc3NdkClientSL_ptr->atsc3_onSlsTablePresent((const char*)xml_payload_copy);

    free(xml_payload_copy);
}

//TODO: jjustman-2019-11-08: wire up the service_id in which this alc_emission originated from in addition to tsi/toi
void atsc3_lls_sls_alc_on_object_close_flag_s_tsid_content_location_ndk(uint32_t tsi, uint32_t toi, char* content_location) {
    atsc3NdkClientSL_ptr->atsc3_lls_sls_alc_on_object_close_flag_s_tsid_content_location_jni(tsi, toi, content_location);
}

void atsc3_lls_sls_alc_on_route_mpd_patched_ndk(uint16_t service_id) {
    atsc3NdkClientSL_ptr->atsc3_lls_sls_alc_on_route_mpd_patched_jni(service_id);
}
/*
 *
note for Android MediaCodec:

https://developer.android.com/reference/android/media/MediaCodec

Android uses the following codec-specific data buffers.
These are also required to be set in the track format for proper MediaMuxer track configuration.
Each parameter set and the codec-specific-data sections marked with (*) must start with a start code of "\x00\x00\x00\x01".

CSD buffer #0:

H.265 HEVC	VPS (Video Parameter Sets*) +
SPS (Sequence Parameter Sets*) +
PPS (Picture Parameter Sets*)

*/
block_t* __INTERNAL_LAST_NAL_PACKET_TODO_FIXME = NULL;

void atsc3_mmt_mpu_on_sequence_mpu_metadata_present_ndk(uint16_t packet_id, uint32_t mpu_sequence_number, block_t* mmt_mpu_metadata) {
    atsc3_hevc_nals_record_dump("mmt_mpu_metadata", mmt_mpu_metadata);

    if (global_video_packet_id && global_video_packet_id == packet_id) {
        //manually extract our NALs here
        video_decoder_configuration_record_t *video_decoder_configuration_record = atsc3_avc1_hevc_nal_extractor_parse_from_mpu_metadata_block_t(
                mmt_mpu_metadata);

        //we will get either avc1 (avcC) NAL or hevc (hvcC) nals back
        if (video_decoder_configuration_record) {
            //set width/height to player
            if(video_decoder_configuration_record->width && video_decoder_configuration_record->height) {
                atsc3NdkClientSL_ptr->atsc3_setVideoWidthHeightFromTrak(video_decoder_configuration_record->width, video_decoder_configuration_record->height);
            }

            if (video_decoder_configuration_record->hevc_decoder_configuration_record) {

                block_t* hevc_nals_combined = atsc3_hevc_extract_extradata_nals_combined_ffmpegImpl(video_decoder_configuration_record->hevc_decoder_configuration_record->box_data_original);

                if(hevc_nals_combined->p_size) {
                    //todo - jjustman-2019-10-12 - lock this for race conditions and allocate per flow
                    block_Rewind(hevc_nals_combined);
                    if(__INTERNAL_LAST_NAL_PACKET_TODO_FIXME) {
                        block_Destroy(&__INTERNAL_LAST_NAL_PACKET_TODO_FIXME);
                    }
                    __INTERNAL_LAST_NAL_PACKET_TODO_FIXME = block_Duplicate(hevc_nals_combined);

                    atsc3NdkClientSL_ptr->atsc3_onInitHEVC_NAL_Extracted(packet_id, mpu_sequence_number,  block_Get(hevc_nals_combined), hevc_nals_combined->p_size);
                } else {
                    atsc3NdkClientSL_ptr->LogMsg("atsc3_mmt_mpu_on_sequence_mpu_metadata_present_ndk - error, no NALs returned!");

                }
            }
        }
    } else {
        //if audio, dump the ESDS box for android MediaCodec

        /*
         *
         *  [mp4a] size=8+67
              data_reference_index = 1
              channel_count = 2
              sample_size = 16
              sample_rate = 48000
              [esds] size=12+27
                [ESDescriptor] size=2+25
                  es_id = 0
                  stream_priority = 0
                  [DecoderConfig] size=2+17
                    stream_type = 5
                    object_type = 64
                    up_stream = 0
                    buffer_size = 8192
                    max_bitrate = 128000
                    avg_bitrate = 128000
                    DecoderSpecificInfo = 11 90
                  [Descriptor:06] size=2+1

                  https://wiki.multimedia.cx/index.php/Understanding_AAC
                  https://developer.android.com/reference/android/media/MediaCodecs
         */
    }
}


void atsc3_mmt_signalling_information_on_video_packet_id_with_mpu_timestamp_descriptor_ndk(uint16_t video_packet_id, uint32_t mpu_sequence_number, uint64_t mpu_presentation_time_ntp64, uint32_t mpu_presentation_time_seconds, uint32_t mpu_presentation_time_microseconds) {
    uint64_t last_mpu_timestamp = mpu_presentation_time_seconds * 1000000 + mpu_presentation_time_microseconds;
    global_video_packet_id = video_packet_id;

    atsc3NdkClientSL_ptr->atsc3_signallingContext_notify_video_packet_id_and_mpu_timestamp_descriptor(video_packet_id, mpu_sequence_number, mpu_presentation_time_ntp64, mpu_presentation_time_seconds, mpu_presentation_time_microseconds);
}

void atsc3_mmt_signalling_information_on_audio_packet_id_with_mpu_timestamp_descriptor_ndk(uint16_t audio_packet_id, uint32_t mpu_sequence_number, uint64_t mpu_presentation_time_ntp64, uint32_t mpu_presentation_time_seconds, uint32_t mpu_presentation_time_microseconds) {
    uint64_t last_mpu_timestamp = mpu_presentation_time_seconds * 1000000 + mpu_presentation_time_microseconds;
    global_audio_packet_id = audio_packet_id;

    atsc3NdkClientSL_ptr->atsc3_signallingContext_notify_audio_packet_id_and_mpu_timestamp_descriptor(audio_packet_id, mpu_sequence_number, mpu_presentation_time_ntp64, mpu_presentation_time_seconds, mpu_presentation_time_microseconds);
}

void atsc3_mmt_signalling_information_on_stpp_packet_id_with_mpu_timestamp_descriptor_ndk(uint16_t stpp_packet_id, uint32_t mpu_sequence_number, uint64_t mpu_presentation_time_ntp64, uint32_t mpu_presentation_time_seconds, uint32_t mpu_presentation_time_microseconds) {
    uint64_t last_mpu_timestamp = mpu_presentation_time_seconds * 1000000 + mpu_presentation_time_microseconds;
    global_stpp_packet_id = stpp_packet_id;

    atsc3NdkClientSL_ptr->atsc3_signallingContext_notify_stpp_packet_id_and_mpu_timestamp_descriptor(stpp_packet_id, mpu_sequence_number, mpu_presentation_time_ntp64, mpu_presentation_time_seconds, mpu_presentation_time_microseconds);
}

void atsc3_mmt_mpu_mfu_on_sample_complete_ndk(uint16_t packet_id, uint32_t mpu_sequence_number, uint32_t sample_number, block_t* mmt_mfu_sample, uint32_t mfu_fragment_count_rebuilt) {
    //    void onMfuPacket(bool is_video, uint8_t* buffer, uint32_t bufferLen, uint64_t presentationUs);
//    at3DrvIntf_ptr->LogMsgF("atsc3_mmt_mpu_mfu_on_sample_complete_ndk: packet_id: %u, mpu_sequence_number: %u, sample_number: %u, size: %u, last_mpu_timestamp: %llu",
//                packet_id,
//                mpu_sequence_number,
//                sample_number,
//                mmt_mfu_sample->p_size,
//                last_mpu_timestamp);

    //cant process MFU's without the NAL... we should ALWAYS listen for at least mpu metadata
    //in as many MMT flows as possible

    atsc3_mmt_mfu_mpu_timestamp_descriptor_t* atsc3_mmt_mfu_mpu_timestamp_descriptor = atsc3_mmt_mfu_context->get_mpu_timestamp_from_packet_id_mpu_sequence_number(atsc3_mmt_mfu_context, packet_id, mpu_sequence_number);

    uint64_t mpu_timestamp_descriptor = 0;
    if(atsc3_mmt_mfu_mpu_timestamp_descriptor) {
        mpu_timestamp_descriptor = atsc3_mmt_mfu_mpu_timestamp_descriptor->mpu_presentation_time_as_us_value;
    } else {
        __WARN("atsc3_mmt_mpu_mfu_on_sample_complete_ndk: mmt_mfu_sample: %p: returned null atsc3_mmt_mfu_mpu_timestamp_descriptor for packet_id: %d, mpu_sequence_number: %d", mmt_mfu_sample, packet_id, mpu_sequence_number);
        //__ERROR("atsc3_mmt_mpu_mfu_on_sample_complete_ndk: mmt_mfu_sample: %p: returned null atsc3_mmt_mfu_mpu_timestamp_descriptor for packet_id: %d, mpu_sequence_number: %d", mmt_mfu_sample, packet_id, mpu_sequence_number);
        //return;
    }

    if(__INTERNAL_LAST_NAL_PACKET_TODO_FIXME && packet_id == global_video_packet_id) {
        block_t *mmt_mfu_sample_rbsp = atsc3_hevc_extract_mp4toannexb_filter_ffmpegImpl(mmt_mfu_sample, __INTERNAL_LAST_NAL_PACKET_TODO_FIXME);
        uint8_t *block_ptr = block_Get(mmt_mfu_sample_rbsp);
        uint32_t block_len = block_Len(mmt_mfu_sample_rbsp);

//        at3DrvIntf_ptr->LogMsgF(
//                "atsc3_mmt_mpu_mfu_on_sample_complete_ndk: NAL, packet_id: %d, mpu_sequence_number: %d, block: %p, len: %d, pts_us: %lu",
//                packet_id, mpu_sequence_number, block_ptr, block_len, mpu_timestamp_descriptor);

        if((global_mfu_proccessed_count++ % 600) == 0) {
            atsc3NdkClientSL_ptr->LogMsgF("atsc3_mmt_mpu_mfu_on_sample_complete_ndk: total mfu count: %d, packet_id: %d, mpu: %d, sample: %d, orig len: %d, len: %d",
                                    global_mfu_proccessed_count,
                                    packet_id, mpu_sequence_number, sample_number, block_Len(mmt_mfu_sample),
                                    block_len);

        }
        atsc3NdkClientSL_ptr->atsc3_onMfuPacket(packet_id, mpu_sequence_number, sample_number, block_ptr, block_len, mpu_timestamp_descriptor, mfu_fragment_count_rebuilt);

        block_Release(&mmt_mfu_sample_rbsp);
    } else {
        block_Rewind(mmt_mfu_sample);
        uint8_t *block_ptr = block_Get(mmt_mfu_sample);
        uint32_t block_len = block_Len(mmt_mfu_sample);

        if(packet_id == 19) {
            atsc3NdkClientSL_ptr->LogMsgF(
                    "atsc3_mmt_mpu_mfu_on_sample_complete_ndk: non NAL, packet_id: %d, mpu_sequence_number: %d, block: %p, len: %d, char: %c %c %c %c",
                    packet_id, mpu_sequence_number, block_ptr, block_len, block_ptr[0], block_ptr[1], block_ptr[2], block_ptr[3]);
        }
        //audio and stpp don't need NAL start codes
        atsc3NdkClientSL_ptr->atsc3_onMfuPacket(packet_id, mpu_sequence_number, sample_number, block_ptr, block_len, mpu_timestamp_descriptor, mfu_fragment_count_rebuilt);
    }
}

void atsc3_mmt_mpu_mfu_on_sample_corrupt_ndk(uint16_t packet_id, uint32_t mpu_sequence_number, uint32_t sample_number, block_t* mmt_mfu_sample, uint32_t mfu_fragment_count_expected, uint32_t mfu_fragment_count_rebuilt) {
    //    at3DrvIntf_ptr->LogMsgF("atsc3_mmt_mpu_mfu_on_sample_corrupt_ndk: packet_id: %u, mpu_sequence_number: %u, sample_number: %u, size: %u, last_mpu_timestamp: %llu",
    //                packet_id,
    //                mpu_sequence_number,
    //                sample_number,
    //                mmt_mfu_sample->p_size,
    //                last_mpu_timestamp);

    //cant process MFU's without the NAL... we should ALWAYS listen for at least mpu metadata
    //in as many MMT flows as possible

    atsc3_mmt_mfu_mpu_timestamp_descriptor_t* atsc3_mmt_mfu_mpu_timestamp_descriptor = atsc3_mmt_mfu_context->get_mpu_timestamp_from_packet_id_mpu_sequence_number(atsc3_mmt_mfu_context, packet_id, mpu_sequence_number);
    uint64_t mpu_timestamp_descriptor = 0;
    if(atsc3_mmt_mfu_mpu_timestamp_descriptor) {
        mpu_timestamp_descriptor = atsc3_mmt_mfu_mpu_timestamp_descriptor->mpu_presentation_time_as_us_value;
    } else {
        __WARN("atsc3_mmt_mpu_mfu_on_sample_corrupt_ndk: mmt_mfu_sample: %p: returned null atsc3_mmt_mfu_mpu_timestamp_descriptor for packet_id: %d, mpu_sequence_number: %d", mmt_mfu_sample, packet_id, mpu_sequence_number);
    }

    if(!block_Len(mmt_mfu_sample)) {
        __WARN("atsc3_mmt_mpu_mfu_on_sample_corrupt_ndk: mmt_mfu_sample: %p: block len is 0 for packet_id: %d, mpu_sequence_number: %d", mmt_mfu_sample, packet_id, mpu_sequence_number);
        return;
    }

    if(__INTERNAL_LAST_NAL_PACKET_TODO_FIXME && packet_id == global_video_packet_id) {
        block_t *mmt_mfu_sample_rbsp = atsc3_hevc_extract_mp4toannexb_filter_ffmpegImpl(mmt_mfu_sample, __INTERNAL_LAST_NAL_PACKET_TODO_FIXME);
        if(mmt_mfu_sample_rbsp && block_Len(mmt_mfu_sample_rbsp)) {
            uint8_t *block_ptr = block_Get(mmt_mfu_sample_rbsp);
            uint32_t block_len = block_Len(mmt_mfu_sample_rbsp);

            //if((global_mfu_proccessed_count++ % 600) == 0) {
                __INFO("atsc3_mmt_mpu_mfu_on_sample_corrupt_ndk: total mfu count: %d, packet_id: %d, mpu: %d, sample: %d, sample ptr: %p, orig len: %d (i_pos: %d, p_size: %d), nal ptr: %p, len: %d (i_pos: %d, p_size: %d)",
                                        global_mfu_proccessed_count,
                                        packet_id,
                                        mpu_sequence_number,
                                        sample_number,
                                        mmt_mfu_sample,
                       mmt_mfu_sample->i_pos,

                       mmt_mfu_sample->p_size,
                                        block_Len(mmt_mfu_sample),
                                        block_ptr,
                                        block_len,
                       mmt_mfu_sample_rbsp->i_pos,
                                        mmt_mfu_sample_rbsp->p_size);

            //}

            atsc3NdkClientSL_ptr->atsc3_onMfuPacketCorrupt(packet_id, mpu_sequence_number, sample_number, block_ptr, block_len, mpu_timestamp_descriptor, mfu_fragment_count_expected, mfu_fragment_count_rebuilt);

            block_Release(&mmt_mfu_sample_rbsp);
         } else {
            __ERROR("atsc3_mmt_mpu_mfu_on_sample_corrupt_ndk: mmt_mfu_sample: %p (len: %d) - returned null mmt_mfu_sample_rbsp!", mmt_mfu_sample, mmt_mfu_sample ? mmt_mfu_sample->p_size : -1);
        }
    } else {
        uint8_t *block_ptr = block_Get(mmt_mfu_sample);
        uint32_t block_len = block_Len(mmt_mfu_sample);

        //audio and stpp don't need NAL start codes
        __INFO("atsc3_mmt_mpu_mfu_on_sample_corrupt_ndk: non NAL, packet_id: %d, mpu_sequence_number: %d, sample_number: %d, block: %p, len: %d, char: %c %c %c %c",
                    packet_id, mpu_sequence_number, sample_number,  block_ptr, block_len, block_ptr[0], block_ptr[1], block_ptr[2], block_ptr[3]);

        atsc3NdkClientSL_ptr->atsc3_onMfuPacketCorrupt(packet_id, mpu_sequence_number, sample_number, block_ptr, block_len, mpu_timestamp_descriptor, mfu_fragment_count_expected, mfu_fragment_count_rebuilt);
    }
}


void atsc3_mmt_mpu_mfu_on_sample_corrupt_mmthsample_header_ndk(uint16_t packet_id, uint32_t mpu_sequence_number, uint32_t sample_number, block_t* mmt_mfu_sample, uint32_t mfu_fragment_count_expected, uint32_t mfu_fragment_count_rebuilt) {
    //    void onMfuPacket(bool is_video, uint8_t* buffer, uint32_t bufferLen, uint64_t presentationUs);
//    at3DrvIntf_ptr->LogMsgF("atsc3_mmt_mpu_mfu_on_sample_complete_ndk: packet_id: %u, mpu_sequence_number: %u, sample_number: %u, size: %u, last_mpu_timestamp: %llu",
//                packet_id,
//                mpu_sequence_number,
//                sample_number,
//                mmt_mfu_sample->p_size,
//                last_mpu_timestamp);

    atsc3_mmt_mfu_mpu_timestamp_descriptor_t* atsc3_mmt_mfu_mpu_timestamp_descriptor = atsc3_mmt_mfu_context->get_mpu_timestamp_from_packet_id_mpu_sequence_number(atsc3_mmt_mfu_context, packet_id, mpu_sequence_number);
    uint64_t mpu_timestamp_descriptor = 0;
    if(atsc3_mmt_mfu_mpu_timestamp_descriptor) {
        mpu_timestamp_descriptor = atsc3_mmt_mfu_mpu_timestamp_descriptor->mpu_presentation_time_as_us_value;
    } else {
        __WARN("atsc3_mmt_mpu_mfu_on_sample_corrupt_mmthsample_header_ndk: mmt_mfu_sample: %p: returned null atsc3_mmt_mfu_mpu_timestamp_descriptor for packet_id: %d, mpu_sequence_number: %d", mmt_mfu_sample, packet_id, mpu_sequence_number);
    }
    //TODO: jjustman-2019-10-23: determine if we can still extract NAL's from this payload...

    if(__INTERNAL_LAST_NAL_PACKET_TODO_FIXME && packet_id == global_video_packet_id) {
        block_t *mmt_mfu_sample_rbsp = atsc3_hevc_extract_mp4toannexb_filter_ffmpegImpl(mmt_mfu_sample, __INTERNAL_LAST_NAL_PACKET_TODO_FIXME);
        if(mmt_mfu_sample_rbsp) {
            uint8_t *block_ptr = block_Get(mmt_mfu_sample_rbsp);
            uint32_t block_len = block_Len(mmt_mfu_sample_rbsp);

            //if((global_mfu_proccessed_count++ % 600) == 0) {
            __INFO("atsc3_mmt_mpu_mfu_on_sample_corrupt_mmthsample_header_ndk: total mfu count: %d, packet_id: %d, mpu: %d, sample: %d, sample ptr: %p, orig len: %d (i_pos: %d, p_size: %d), nal ptr: %p, len: %d (i_pos: %d, p_size: %d)",
                                        global_mfu_proccessed_count,
                                        packet_id,
                                        mpu_sequence_number,
                                        sample_number,
                                        mmt_mfu_sample,
                                        block_Len(mmt_mfu_sample),
                   mmt_mfu_sample->i_pos,
                   mmt_mfu_sample->p_size,
                                        block_ptr,
                                        block_len,
                   mmt_mfu_sample_rbsp->i_pos,
                   mmt_mfu_sample_rbsp->p_size);
            //}

            atsc3NdkClientSL_ptr->atsc3_onMfuPacketCorruptMmthSampleHeader(packet_id, mpu_sequence_number, sample_number, block_ptr, block_len, mpu_timestamp_descriptor, mfu_fragment_count_expected, mfu_fragment_count_rebuilt);

            block_Release(&mmt_mfu_sample_rbsp);
        }
    } else {
        uint8_t *block_ptr = block_Get(mmt_mfu_sample);
        uint32_t block_len = block_Len(mmt_mfu_sample);

        __INFO("atsc3_mmt_mpu_mfu_on_sample_corrupt_mmthsample_header_ndk: non NAL, packet_id: %d, mpu_sequence_number: %d, sample: %d, block: %p, len: %d, char: %c %c %c %c",
               packet_id, mpu_sequence_number, sample_number, block_ptr, block_len, block_ptr[0], block_ptr[1], block_ptr[2], block_ptr[3]);

        //audio and stpp don't need NAL start codes
        atsc3NdkClientSL_ptr->atsc3_onMfuPacketCorruptMmthSampleHeader(packet_id, mpu_sequence_number, sample_number, block_ptr, block_len, mpu_timestamp_descriptor, mfu_fragment_count_expected, mfu_fragment_count_rebuilt);
    }
}

void atsc3_mmt_mpu_mfu_on_sample_missing_ndk(uint16_t packet_id, uint32_t mpu_sequence_number, uint32_t sample_number) {
    atsc3NdkClientSL_ptr->atsc3_onMfuSampleMissing(packet_id, mpu_sequence_number, sample_number);
}

void atsc3_mmt_mpu_on_sequence_movie_fragment_metadata_present_ndk(uint16_t packet_id, uint32_t mpu_sequence_number, block_t* mmt_movie_fragment_metadata) {
    if(!mmt_movie_fragment_metadata || !mmt_movie_fragment_metadata->p_size) {
        __WARN("atsc3_mmt_mpu_on_sequence_movie_fragment_metadata_present_ndk: packet_id: %d, mpu_sequence_number: %d, mmt_movie_fragment_metadata: %p: returned null or no length!",
                packet_id, mpu_sequence_number, mmt_movie_fragment_metadata);
        return;
    }

    uint32_t extracted_sample_duration_us = atsc3_mmt_movie_fragment_extract_sample_duration(mmt_movie_fragment_metadata);

    atsc3NdkClientSL_ptr->atsc3_onExtractedSampleDuration(packet_id, mpu_sequence_number, extracted_sample_duration_us);
}


atsc3_link_mapping_table_t* atsc3_phy_mmt_player_bridge_notify_link_mapping_table(atsc3_link_mapping_table_t* atsc3_link_mapping_table_pending) {
    atsc3_link_mapping_table_t* atsc3_link_mapping_table_to_free = NULL;

    //no last link mapping table, so take ownership of pending ptr
    if(!atsc3_link_mapping_table_last) {
        atsc3_link_mapping_table_last = atsc3_link_mapping_table_pending;
    } else {
        //if we have a pending table version that matches our last table version, so return our pending version to be freed (discarded)
        if(atsc3_link_mapping_table_pending->alp_additional_header_for_signaling_information_signaling_version ==
            atsc3_link_mapping_table_last->alp_additional_header_for_signaling_information_signaling_version) {
            atsc3_link_mapping_table_to_free = atsc3_link_mapping_table_pending;
        } else {
            //pending table version is not the saemm as our last version, so discard our last ptr ref and update pending to last
            atsc3_link_mapping_table_to_free = atsc3_link_mapping_table_last;
            atsc3_link_mapping_table_last = atsc3_link_mapping_table_pending;
        }
    }


    return atsc3_link_mapping_table_to_free;
}



#ifdef __FIXME_REFACTOR_LOWASIS__
void atsc3_phy_mmt_player_bridge_init(At3DrvIntf* atsc3NdkClientSL_ptr_l) {
#else
void atsc3_phy_mmt_player_bridge_init(atsc3NdkClient* atsc3NdkClientSL_ptr_l) {
#endif
    atsc3NdkClientSL_ptr = atsc3NdkClientSL_ptr_l;

    atsc3NdkClientSL_ptr->LogMsgF("atsc3_phy_mmt_player_bridge_init - client ptr: %p", atsc3NdkClientSL_ptr_l);

    //set global logging levels
    _MMT_CONTEXT_MPU_DEBUG_ENABLED = 0;
    _ALC_UTILS_IOTRACE_ENABLED = 0;
    _ROUTE_SLS_PROCESSOR_INFO_ENABLED=1;
    _ALC_UTILS_IOTRACE_ENABLED = 0;

#ifdef __SIGNED_MULTIPART_LLS_DEBUGGING__
    _LLS_TRACE_ENABLED = 1;
    _LLS_DEBUG_ENABLED = 1;

    _LLS_SLT_PARSER_DEBUG_ENABLED = 1;
    _LLS_SLT_PARSER_TRACE_ENABLED = 1;
#endif
    _LLS_ALC_UTILS_DEBUG_ENABLED = 1;
    _ALC_UTILS_DEBUG_ENABLED = 1;
    _ALC_RX_TRACE_ENABLED = 0;

    if(!lls_slt_monitor) {
        lls_slt_monitor = lls_slt_monitor_create();
        //wire up a lls event for SLS table
        lls_slt_monitor->atsc3_lls_on_sls_table_present = &atsc3_lls_on_sls_table_present_ndk;

        mmtp_flow = mmtp_flow_new();
        udp_flow_latest_mpu_sequence_number_container = udp_flow_latest_mpu_sequence_number_container_t_init();


        //MMT/MFU callback contexts
        atsc3_mmt_mfu_context = atsc3_mmt_mfu_context_new();

        //wire up atsc3_mmt_mpu_on_sequence_mpu_metadata_present to parse out our NALs as needed for android MediaCodec init
        atsc3_mmt_mfu_context->atsc3_mmt_mpu_on_sequence_mpu_metadata_present = &atsc3_mmt_mpu_on_sequence_mpu_metadata_present_ndk;

        atsc3_mmt_mfu_context->atsc3_mmt_mpu_mfu_on_sample_complete = &atsc3_mmt_mpu_mfu_on_sample_complete_ndk;
        atsc3_mmt_mfu_context->atsc3_mmt_mpu_mfu_on_sample_corrupt = &atsc3_mmt_mpu_mfu_on_sample_corrupt_ndk;
        //todo: search thru NAL's as needed here and discard anything that intra-NAL..
        atsc3_mmt_mfu_context->atsc3_mmt_mpu_mfu_on_sample_corrupt_mmthsample_header = &atsc3_mmt_mpu_mfu_on_sample_corrupt_mmthsample_header_ndk;
        atsc3_mmt_mfu_context->atsc3_mmt_mpu_mfu_on_sample_missing = &atsc3_mmt_mpu_mfu_on_sample_missing_ndk;

        /*
         * TODO: jjustman-2019-10-20 - extend context callback interface with service_id
         */
        atsc3_mmt_mfu_context->atsc3_mmt_signalling_information_on_video_packet_id_with_mpu_timestamp_descriptor = &atsc3_mmt_signalling_information_on_video_packet_id_with_mpu_timestamp_descriptor_ndk;
        atsc3_mmt_mfu_context->atsc3_mmt_signalling_information_on_audio_packet_id_with_mpu_timestamp_descriptor = &atsc3_mmt_signalling_information_on_audio_packet_id_with_mpu_timestamp_descriptor_ndk;
        atsc3_mmt_mfu_context->atsc3_mmt_signalling_information_on_stpp_packet_id_with_mpu_timestamp_descriptor  = &atsc3_mmt_signalling_information_on_stpp_packet_id_with_mpu_timestamp_descriptor_ndk;

        //extract out one trun sampleduration for essence timing
        atsc3_mmt_mfu_context->atsc3_mmt_mpu_on_sequence_movie_fragment_metadata_present = &atsc3_mmt_mpu_on_sequence_movie_fragment_metadata_present_ndk;

        atsc3_ndk_cache_temp_folder_path = atsc3NdkClientSL_ptr->get_android_temp_folder();
        chdir(atsc3_ndk_cache_temp_folder_path.c_str());
        atsc3NdkClientSL_ptr->LogMsgF("atsc3_phy_mmt_player_bridge_init - completed, temp folder path: %s", atsc3_ndk_cache_temp_folder_path.c_str());
    } else {
        atsc3NdkClientSL_ptr->LogMsgF("atsc3_phy_mmt_player_bridge_init - ignoring init as we are already configured");
    }


    /**
     * additional SLS monitor related callbacks wired up in
     *
        lls_sls_alc_monitor->atsc3_lls_sls_alc_on_object_close_flag_s_tsid_content_location = &atsc3_lls_sls_alc_on_object_close_flag_s_tsid_content_location_ndk;
        //write up event callback for alc MPD patching
        lls_sls_alc_monitor->atsc3_lls_sls_alc_on_route_mpd_patched = &atsc3_lls_sls_alc_on_route_mpd_patched_ndk;
     */

}

string atsc3_ndk_cache_temp_folder_path_get() {
    return atsc3_ndk_cache_temp_folder_path;
}


string atsc3_route_service_context_temp_folder_name(int service_id) {
    return __ALC_DUMP_OUTPUT_PATH__ + to_string(service_id) + "/";
}




