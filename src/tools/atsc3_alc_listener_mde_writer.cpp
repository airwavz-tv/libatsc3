/*
 * atsc3_alc_listener_mde_writer.cpp
 *
 *  Created on: Aug 23rd, 2019
 *      Author: jjustman
 *
 * single <ip, udp> flow ALC media delivery event (mde) writer for debugging/analysis
 * ignores SLT service <ip, udp> flows for ROUTE emissions, as to aid in low-level ALC investigation
 *
*/

//#define _ENABLE_TRACE 1
#define _SHOW_PACKET_FLOW 1
int PACKET_COUNTER=0;

#ifdef __MALLOC_DEBUGGING
#include <mcheck.h>
#endif


#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/stat.h>

#include "../atsc3_listener_udp.h"
#include "../atsc3_utils.h"
#include "../atsc3_lls.h"
#include "../atsc3_lls_alc_utils.h"
#include "../atsc3_alc_rx.h"
#include "../atsc3_alc_utils.h"
#include "../atsc3_listener_udp.h"
#include "../atsc3_logging_externs.h"

lls_slt_monitor_t* lls_slt_monitor;
lls_sls_alc_monitor_t* lls_sls_alc_monitor;

uint32_t* dst_ip_addr_filter = NULL;
uint16_t* dst_ip_port_filter = NULL;
uint16_t* dst_service_id_filter = NULL;

atsc3_alc_arguments_t* alc_arguments;
atsc3_alc_session_t* atsc3_alc_session;

void process_packet(u_char *user, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
  udp_packet_t* udp_packet = process_packet_from_pcap(user, pkthdr, packet);
  if(!udp_packet) {
	return;
  }

  alc_packet_t* alc_packet = NULL;
    
    
    //dispatch for LLS extraction and dump
    if(udp_packet->udp_flow.dst_ip_addr == LLS_DST_ADDR && udp_packet->udp_flow.dst_port == LLS_DST_PORT) {
        lls_table_t* lls_table = lls_table_create_or_update_from_lls_slt_monitor(lls_slt_monitor, udp_packet->data);
        //auto-assign our first ROUTE service id here
        if(lls_table && lls_table->lls_table_id == SLT) {
            for(int i=0; i < lls_table->slt_table.atsc3_lls_slt_service_v.count; i++) {
                atsc3_lls_slt_service_t* atsc3_lls_slt_service = lls_table->slt_table.atsc3_lls_slt_service_v.data[i];
                if(atsc3_lls_slt_service->atsc3_slt_broadcast_svc_signalling_v.count &&
                   atsc3_lls_slt_service->atsc3_slt_broadcast_svc_signalling_v.data[0]->sls_protocol == SLS_PROTOCOL_ROUTE) {
					lls_sls_alc_monitor_t* lls_sls_alc_monitor_local = lls_sls_alc_monitor_create();
								
					lls_slt_service_id_t* lls_slt_service_id = lls_slt_service_id_new_from_atsc3_lls_slt_service(atsc3_lls_slt_service);
					lls_slt_monitor_add_lls_slt_service_id(lls_slt_monitor, lls_slt_service_id);

					lls_sls_alc_session_t* lls_sls_alc_session = lls_slt_alc_session_find_from_service_id(lls_slt_monitor, atsc3_lls_slt_service->service_id);
					if(!lls_sls_alc_session) {
						__WARN("lls_slt_alc_session_find_from_service_id: lls_sls_alc_session is NULL!");
					}
					lls_sls_alc_monitor_local->lls_alc_session = lls_sls_alc_session;
					lls_sls_alc_monitor_local->atsc3_lls_slt_service = atsc3_lls_slt_service;
					lls_sls_alc_monitor_local->lls_sls_monitor_output_buffer_mode.file_dump_enabled = true;


					__WARN("process_packet: adding lls_sls_alc_monitor: %p to lls_slt_monitor: %p, service_id: %d",
						   lls_sls_alc_monitor_local, lls_slt_monitor, lls_sls_alc_session->service_id);

					lls_slt_monitor_add_lls_sls_alc_monitor(lls_slt_monitor, lls_sls_alc_monitor_local);

					if(!lls_sls_alc_monitor) {
						lls_slt_monitor->lls_sls_alc_monitor = lls_sls_alc_monitor_local;
						lls_sls_alc_monitor =  lls_sls_alc_monitor_local;

					} else {
						//only swap out this lls_sls_alc_monitor if this alc flow is "retired"
					}
                }
            }
        }
        

            
        return udp_packet_free(&udp_packet);
    }
    
    /*
	 jjustman-2020-03-25 - alternatively, filter out by ServiceID:
	lls_sls_alc_monitor->atsc3_lls_slt_service &&
	lls_sls_alc_monitor->atsc3_lls_slt_service->service_id == matching_lls_slt_alc_session->atsc3_lls_slt_service->service_id
	 
	clang optimized out matching_lls_slt_alc_session in the first conditional, so its added in the 3rd filter test for service_id
    */
	
	lls_sls_alc_session_t* matching_lls_slt_alc_session = lls_slt_alc_session_find_from_udp_packet(lls_slt_monitor, udp_packet->udp_flow.src_ip_addr, udp_packet->udp_flow.dst_ip_addr, udp_packet->udp_flow.dst_port);
    if((lls_sls_alc_monitor && matching_lls_slt_alc_session) &&
	   (dst_service_id_filter == NULL && dst_ip_addr_filter == NULL && dst_ip_port_filter == NULL) ||
	   ((dst_service_id_filter != NULL && matching_lls_slt_alc_session && matching_lls_slt_alc_session->service_id == *dst_service_id_filter ) ||
	    (dst_ip_addr_filter != NULL && dst_ip_port_filter == NULL && udp_packet->udp_flow.dst_ip_addr == *dst_ip_addr_filter) ||
	    (dst_ip_addr_filter != NULL && dst_ip_port_filter != NULL && udp_packet->udp_flow.dst_ip_addr == *dst_ip_addr_filter && udp_packet->udp_flow.dst_port == *dst_ip_port_filter))) {
		
		//process ALC streams
		int retval = alc_rx_analyze_packet_a331_compliant((char*)block_Get(udp_packet->data), block_Remaining_size(udp_packet->data), &alc_packet);

#ifdef __MALLOC_DEBUGGING
		mcheck(0);
#endif
		
		if(!retval) {
			
			//lls_sls_alc_monitor_t* atsc3_lls_sls_alc_monitor_find_from_udp_packet(lls_slt_monitor_t* lls_slt_monitor, uint32_t src_ip_addr, uint32_t dst_ip_addr, uint16_t dst_port) {

			//dump out for fragment inspection
		  atsc3_alc_persist_route_ext_attributes_per_lls_sls_alc_monitor_essence(alc_packet, lls_slt_monitor->lls_sls_alc_monitor);

#ifdef __MALLOC_DEBUGGING
mcheck(0);
#endif
		  atsc3_alc_packet_persist_to_toi_resource_process_sls_mbms_and_emit_callback(
                    &udp_packet->udp_flow, &alc_packet,
                    lls_slt_monitor->lls_sls_alc_monitor);
#ifdef __MALLOC_DEBUGGING
		 mcheck(0);
#endif
		} else {
			__ERROR("Error in ALC decode: %d", retval);
		}
	} else {
		//__ERROR("Discarding packet: lls_sls_alc_monitor: %p, matching_lls_slt_alc_session: %p, ", lls_sls_alc_monitor, matching_lls_slt_alc_session);
	}

udp_packet_free:
	alc_packet_free(&alc_packet);
	alc_packet = NULL;
#ifdef __MALLOC_DEBUGGING

	mcheck(0);
#endif
	
    return udp_packet_free(&udp_packet);
}

int main(int argc,char **argv) {

#ifdef __MALLOC_DEBUGGING
          mcheck(0);
	  mtrace ();
#endif
	_LLS_SLT_PARSER_INFO_ROUTE_ENABLED = 1;
	_LLS_ALC_UTILS_INFO_ENABLED = 1;

    _ALC_UTILS_DEBUG_ENABLED = 1;
    _ALC_UTILS_IOTRACE_ENABLED = 1;
	_ALC_RX_DEBUG_ENABLED = 1;
    _ALC_UTILS_DEBUG_ENABLED = 1;
	
#ifdef __LOTS_OF_DEBUGGING__
	_LLS_INFO_ENABLED = 1;
	_LLS_DEBUG_ENABLED = 1;

	_LLS_SLT_PARSER_INFO_ROUTE_ENABLED = 1;
    _ALC_UTILS_DEBUG_ENABLED = 1;
    _ALC_UTILS_TRACE_ENABLED = 1;

	_ALC_UTILS_IOTRACE_ENABLED=1;
	_ALC_RX_DEBUG_ENABLED = 1;
	_ALC_RX_TRACE_ENABLED = 1;
#endif

	char *dev;

    char *dst_ip = NULL;
    char *dst_port = NULL;
	char *dst_service_id = NULL;
	
	uint16_t dst_service_id_int;
	uint32_t dst_ip_int;
	uint16_t dst_port_int;

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* descr;
    struct bpf_program fp;
    bpf_u_int32 maskp;
    bpf_u_int32 netp;

	//wire up our required lls and sls structs here, if needed for "ad-hoc" IP based ROUTE flow selection without LLS/SLS
	
    mkdir("route", 0777);

    lls_slt_monitor = lls_slt_monitor_create();
	alc_arguments = (atsc3_alc_arguments_t*)calloc(1, sizeof(atsc3_alc_arguments_t));
    
    atsc3_alc_session = atsc3_open_alc_session(alc_arguments);
	

    //listen to all flows
    if(argc==2) {
        dev = argv[1];
        __INFO("listening on dev: %s", dev);
		
    } else if(argc==4) {
		if(argv[2][0] == 's') {
			dev = argv[1];
			dst_service_id = argv[3];

			dst_service_id_int = atoi(dst_service_id);
			dst_service_id_filter = &dst_service_id_int;
		} else {
			//we can't support listening to an ip address ROUTE flow, as we need an ip:port to create our dummy SLS broadcast_svc_signalling
			
			println("Invalid type for filtering: %c (should be empty or s)", argv[2][0]);
			exit(2);
		}
    } else if(argc==5) {
		if(argv[2][0] != 'p') {
			printf("invalid filter type: %c", argv[2][0]);
			exit(2);
		}
    	//listen to a selected flow
		dev = argv[1];
		dst_ip = argv[3];
		dst_port = argv[4];

		dst_ip_int = parseIpAddressIntoIntval(dst_ip);
		dst_ip_addr_filter = &dst_ip_int;

		__INFO("listening on dev: %s, dst_ip: %s, dst_port: %s", dev, dst_ip, dst_port);
		dst_port_int = parsePortIntoIntval(dst_port);
		dst_ip_port_filter = &dst_port_int;
		
		/* jjustman-2020-03-28 - create a dummy lls_sls_alc_monitor_t for ad-hoc SLS management (e.g. LLS is not present in this pcap flow - may be on a non-listening PLP */

		atsc3_lls_slt_service_t* atsc3_lls_slt_service = atsc3_lls_slt_service_new();
		atsc3_lls_slt_service->service_id=31337; //hack
		
		atsc3_slt_broadcast_svc_signalling_t* atsc3_slt_broadcast_svc_signalling = atsc3_slt_broadcast_svc_signalling_new();
		atsc3_slt_broadcast_svc_signalling->sls_destination_ip_address = dst_ip;
		atsc3_slt_broadcast_svc_signalling->sls_destination_udp_port =  dst_port;
		atsc3_slt_broadcast_svc_signalling->sls_protocol = SLS_PROTOCOL_ROUTE;
		atsc3_lls_slt_service_add_atsc3_slt_broadcast_svc_signalling(atsc3_lls_slt_service, atsc3_slt_broadcast_svc_signalling);
		
		lls_slt_alc_session_find_or_create(lls_slt_monitor, atsc3_lls_slt_service);

		lls_slt_service_id_t* lls_slt_service_id = lls_slt_service_id_new_from_atsc3_lls_slt_service(atsc3_lls_slt_service);
		lls_slt_monitor_add_lls_slt_service_id(lls_slt_monitor, lls_slt_service_id);

		lls_sls_alc_session_t* lls_sls_alc_session = lls_slt_alc_session_find_from_service_id(lls_slt_monitor, atsc3_lls_slt_service->service_id);
		if(!lls_sls_alc_session) {
			__WARN("lls_slt_alc_session_find_from_service_id: lls_sls_alc_session is NULL!");
		}
		
		lls_sls_alc_monitor_t* lls_sls_alc_monitor_local = lls_sls_alc_monitor_create();
		lls_sls_alc_monitor_local->lls_alc_session = lls_sls_alc_session;
		lls_sls_alc_monitor_local->atsc3_lls_slt_service = atsc3_lls_slt_service;
		lls_sls_alc_monitor_local->lls_sls_monitor_output_buffer_mode.file_dump_enabled = true;


		__WARN("process_packet: adding lls_sls_alc_monitor: %p to lls_slt_monitor: %p, service_id: %d",
			   lls_sls_alc_monitor_local, lls_slt_monitor, lls_sls_alc_session->service_id);

		lls_slt_monitor_add_lls_sls_alc_monitor(lls_slt_monitor, lls_sls_alc_monitor_local);

		if(!lls_sls_alc_monitor) {
			lls_slt_monitor->lls_sls_alc_monitor = lls_sls_alc_monitor_local;
			lls_sls_alc_monitor =  lls_sls_alc_monitor_local;
		}

    } else {
    	println("%s - a udp mulitcast ALC listener for writing out MDE fragments to ROUTE objects", argv[0]);
    	println("---");
    	println("args: dev (type) (service_id)|(dst_ip dst_port");
    	println(" dev: device to listen for udp multicast");
    	println(" --- optional ---");
    	println(" type: s => service_id, p=> dst_ip (dst_port)");
		println("  s: restrict ALC flow capture to specific service_id");
		println("  -or-");
		println("  p: restrict ALC flow capture to specific ip address and port");
    	println("   dst_ip: restrict ALC flow capture to specific ip address");
    	println("   dst_port: restrict ALC flow capture to specific port");
    	println("");
    	exit(1);
    }


    pcap_lookupnet(dev, &netp, &maskp, errbuf);
    descr = pcap_open_live(dev, MAX_PCAP_LEN, 1, 1, errbuf);

    if(descr == NULL) {
        printf("pcap_open_live(): %s",errbuf);
        exit(1);
    }

    char filter[] = "udp";
    if(pcap_compile(descr,&fp, filter,0,netp) == -1) {
        fprintf(stderr,"Error calling pcap_compile");
        exit(1);
    }

    if(pcap_setfilter(descr,&fp) == -1) {
        fprintf(stderr,"Error setting filter");
        exit(1);
    }

    pcap_loop(descr,-1,process_packet,NULL);

    return 0;
}


