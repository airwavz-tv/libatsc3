/*
 * atsc3_lls_alc_tools.h
 *
 *  Created on: Feb 6, 2019
 *      Author: jjustman
 */


#ifndef __LLS_SESSION_RELAX_SOURCE_IP_CHECK__
#define __LLS_SESSION_RELAX_SOURCE_IP_CHECK__ true
#endif

#ifndef ATSC3_LLS_ALC_TOOLS_H_
#define ATSC3_LLS_ALC_TOOLS_H_
#include <assert.h>
#include <stdbool.h>

#include "atsc3_lls.h"
#include "atsc3_lls_types.h"
#include "atsc3_alc_session.h"
#include "atsc3_lls_slt_parser.h"
#include "atsc3_lls_sls_parser.h"
#include "atsc3_logging_externs.h"

#if defined (__cplusplus)
extern "C" {
#endif

lls_sls_alc_monitor_t* lls_sls_alc_monitor_create(void);
lls_sls_alc_session_t* lls_slt_alc_session_find_or_create(lls_slt_monitor_t* lls_slt_monitor, atsc3_lls_slt_service_t* atsc3_lls_slt_service);
lls_sls_alc_monitor_t* atsc3_lls_sls_alc_monitor_find_from_udp_packet(lls_slt_monitor_t* lls_slt_monitor, uint32_t src_ip_addr, uint32_t dst_ip_addr, uint16_t dst_port);

//jjustman-202-03-28 - internal methods for lls_sls_alc_session creation without adding to a monitor
lls_sls_alc_session_t* lls_slt_alc_session_create(atsc3_lls_slt_service_t* atsc3_lls_slt_service);

//jjustman-2020-03-25: TODO - deprecate single lls_sls_alc_session resolution, instead use lls_sls_alc_montitor_t for alc session extraction
lls_sls_alc_session_t* lls_slt_alc_session_find(lls_slt_monitor_t* lls_slt_monitor, atsc3_lls_slt_service_t* atsc3_lls_slt_service);
lls_sls_alc_session_t* lls_slt_alc_session_find_from_udp_packet(lls_slt_monitor_t* lls_slt_monitor, uint32_t src_ip_addr, uint32_t dst_ip_addr, uint16_t dst_port);
lls_sls_alc_session_t* lls_slt_alc_session_find_from_service_id(lls_slt_monitor_t* lls_slt_monitor, uint16_t service_id);

void lls_sls_alc_update_s_tsid_RS_dIpAddr_dPort_if_missing(udp_flow_t* udp_flow, lls_sls_alc_monitor_t* lls_sls_alc_monitor, atsc3_route_s_tsid_t* atsc3_route_s_tsid);

void lls_sls_alc_update_tsi_toi_from_route_s_tsid(lls_sls_alc_monitor_t* lls_sls_alc_monitor, atsc3_route_s_tsid_t* atsc3_route_s_tsid);

void lls_slt_alc_session_remove(lls_slt_monitor_t* lls_slt_monitor, atsc3_lls_slt_service_t* atsc3_lls_slt_service);


lls_sls_alc_monitor_t* lls_monitor_sls_alc_session_create(atsc3_lls_slt_service_t* atsc3_lls_slt_service);


void lls_sls_alc_session_free(lls_sls_alc_session_t** lls_session_ptr);


#if defined (__cplusplus)
}
#endif



#define _ATSC3_LLS_ALC_UTILS_ERROR(...)  __LIBATSC3_TIMESTAMP_ERROR(__VA_ARGS__);
#define _ATSC3_LLS_ALC_UTILS_WARN(...)   __LIBATSC3_TIMESTAMP_WARN(__VA_ARGS__);
#define _ATSC3_LLS_ALC_UTILS_INFO(...)   if(_LLS_ALC_UTILS_INFO_ENABLED)  { __LIBATSC3_TIMESTAMP_INFO(__VA_ARGS__);  }
#define _ATSC3_LLS_ALC_UTILS_DEBUG(...)  if(_LLS_ALC_UTILS_DEBUG_ENABLED) { __LIBATSC3_TIMESTAMP_DEBUG(__VA_ARGS__); }
#define _ATSC3_LLS_ALC_UTILS_TRACE(...)  if(_LLS_ALC_UTILS_TRACE_ENABLED) { __LIBATSC3_TIMESTAMP_TRACE(__VA_ARGS__); }

#endif /* ATSC3_LLS_ALC_TOOLS_H_ */
