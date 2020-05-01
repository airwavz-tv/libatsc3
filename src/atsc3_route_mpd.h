/*
 * atsc3_route_mpd.h
 *
 *  Created on: Apr 2, 2019
 *      Author: jjustman
 *
 *
 * jjustman-2020-03-11 - TODO: add support for EXT_ROUTE_PRESENTATION_TIME - SCT
 * From ATSC A/331:2020 - Section 8.1.1.3 -
 *
 * 8.1.1.3 Synchronization and Time
 *
 * ROUTE/DASH requires accurate wall clock for synchronization.
 * Network servers for both broadcast and broadband components of a Service shall synchronize  to a common wall clock (UTC) source.
 * GPS or other source with similar accuracy and stability shall be utilized.
 *
 * Several use cases of receiver presentation time are supported for MPD@type='dynamic'. These are defined as:
 *
 *      Earliest MDE ROUTE presentation time,
 *      Earliest Segment level DASH Media Presentation time,
 *      and Synchronized Segment level DASH Media Presentation timeline.
 *
 *      The receiver is expected to calculate the offset of presentation timeline relative to receiver wall clock for two use cases as follows:
 *      1) Earliest MDE ROUTE presentation time:
 *          MDE data block with T-RAP ROUTE reception time plus (EXT_ROUTE_PRESENTATION_TIME - SCT).
 *          Initial media request based on adjusted segment availability start time per Section 5.3.9.5.3 of [57].
 *
 *      2) Segment level DASH Media Presentation time:
 *          Segment request based on DASH Segment “availability start time” per the DASH-IF [12] profile
 *          in which the receiver calculates the presentation timeline based on UTC timing given in the MPD.
 *
 *  Note that DASH provides a mode supporting synchronized playback among different receivers using a parameter called MPD@suggestedPresentationDelay.
 *  Broadcast emission of MPD@suggestedPresentationDelay is optional and can be used by receivers when synchronized playback is desired.
 *
 *  This mode is not described here as there is no ATSC 3.0 requirement for synchronized playback across multiple devices.
 *
 *  Note that this mode causes an additional delay in channel change times.
 *
 *
 *      <?xml version="1.0" encoding="UTF-8"?>
<MPD availabilityStartTime="2019-02-19T21:40:50Z" maxSegmentDuration="PT2S" minBufferTime="PT2S" minimumUpdatePeriod="PT10S" profiles="urn:mpeg:dash:profile:isoff-live:2011" publishTime="2019-02-19T21:40:50Z" timeShiftBufferDepth="PT20S" type="dynamic" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemalocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">
   <Period id="P21" start="PT2155S" xlink:actuate="onLoad" xlink:href="tag:atsc.org,2016:xlinkhttps://atsc3vod.npgco.com/crengine?clientid=%clientid%&amp;uniqueid=775&amp;thisname=SNPR55030&amp;nextid=774&amp;nextname=SKPR60825" xmlns:xlink="http://www.w3.org/1999/xlink">
      <AdaptationSet contentType="video" id="0" maxFrameRate="30000/1001" maxHeight="1080" maxWidth="1920" mimeType="video/mp4" minFrameRate="30000/1001" minHeight="1080" minWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
         <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
         <Representation bandwidth="7000000" codecs="hev1.2.4.L120.9" frameRate="30000/1001" height="1080" id="0" sar="1:1" width="1920">
            <SegmentTemplate duration="2002000" initialization="test-$RepresentationID$-init.mp4v" media="test-$RepresentationID$-$Number$.mp4v" presentationTimeOffset="2155019000" startNumber="1088" timescale="1000000"/>
         </Representation>
      </AdaptationSet>
      <AdaptationSet contentType="audio" id="1" lang="eng" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
         <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
         <Representation audioSamplingRate="48000" bandwidth="387000" codecs="mp4a.40.2" id="1">
            <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            <SegmentTemplate duration="2002000" initialization="test-$RepresentationID$-init.mp4a" media="test-$RepresentationID$-$Number$.mp4a" presentationTimeOffset="2155019000" startNumber="1088" timescale="1000000"/>
         </Representation>
      </AdaptationSet>
   </Period>
   <Period id="P22" start="PT2185S" xlink:actuate="onLoad" xlink:href="tag:atsc.org,2016:xlinkhttps://atsc3vod.npgco.com/crengine?clientid=%clientid%&amp;uniqueid=776&amp;thisname=SKPR60825&amp;nextid=775&amp;nextname=SNPR55030" xmlns:xlink="http://www.w3.org/1999/xlink">
      <AdaptationSet contentType="video" id="0" maxFrameRate="30000/1001" maxHeight="1080" maxWidth="1920" mimeType="video/mp4" minFrameRate="30000/1001" minHeight="1080" minWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
         <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
         <Representation bandwidth="7000000" codecs="hev1.2.4.L120.9" frameRate="30000/1001" height="1080" id="0" sar="1:1" width="1920">
            <SegmentTemplate duration="2002000" initialization="test-$RepresentationID$-init.mp4v" media="test-$RepresentationID$-$Number$.mp4v" presentationTimeOffset="2185049000" startNumber="1103" timescale="1000000"/>
         </Representation>
      </AdaptationSet>
      <AdaptationSet contentType="audio" id="1" lang="eng" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
         <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
         <Representation audioSamplingRate="48000" bandwidth="387000" codecs="mp4a.40.2" id="1">
            <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            <SegmentTemplate duration="2002000" initialization="test-$RepresentationID$-init.mp4a" media="test-$RepresentationID$-$Number$.mp4a" presentationTimeOffset="2185049000" startNumber="1103" timescale="1000000"/>
         </Representation>
      </AdaptationSet>
   </Period>
   <Period id="P23" start="PT2215S" xlink:actuate="onLoad" xlink:href="tag:atsc.org,2016:xlinkhttps://atsc3vod.npgco.com/crengine?clientid=%clientid%&amp;uniqueid=777&amp;thisname=SNPR60093&amp;nextid=776&amp;nextname=SKPR60825" xmlns:xlink="http://www.w3.org/1999/xlink">
      <AdaptationSet contentType="video" id="0" maxFrameRate="30000/1001" maxHeight="1080" maxWidth="1920" mimeType="video/mp4" minFrameRate="30000/1001" minHeight="1080" minWidth="1920" par="16:9" segmentAlignment="true" startWithSAP="1">
         <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
         <Representation bandwidth="7000000" codecs="hev1.2.4.L120.9" frameRate="30000/1001" height="1080" id="0" sar="1:1" width="1920">
            <SegmentTemplate duration="2002000" initialization="test-$RepresentationID$-init.mp4v" media="test-$RepresentationID$-$Number$.mp4v" presentationTimeOffset="2215112000" startNumber="1119" timescale="1000000"/>
         </Representation>
      </AdaptationSet>
      <AdaptationSet contentType="audio" id="1" lang="eng" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
         <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
         <Representation audioSamplingRate="48000" bandwidth="387000" codecs="mp4a.40.2" id="1">
            <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            <SegmentTemplate duration="2002000" initialization="test-$RepresentationID$-init.mp4a" media="test-$RepresentationID$-$Number$.mp4a" presentationTimeOffset="2215112000" startNumber="1119" timescale="1000000"/>
         </Representation>
      </AdaptationSet>
   </Period>
</MPD>

ATSC3_VECTOR_BUILDER_STRUCT
 *
 */

#ifndef ATSC3_ROUTE_MPD_H_
#define ATSC3_ROUTE_MPD_H_

#include "atsc3_utils.h"
#include "xml.h"
#include "atsc3_vector_builder.h"
#include "atsc3_logging_externs.h"
/**
 *  xlink:href="tag:atsc.org,2016:xlinkhttps://atsc3vod.npgco.com/crengine?clientid=%clientid%&amp;uniqueid=775&amp;thisname=SNPR55030&amp;nextid=774&amp;nextname=SKPR60825"
 *    xmlns:xlink="http://www.w3.org/1999/xlink">
 */

typedef struct atsc3_xlink {

	char* actuate;
	char* href;
	char* xlink;

} atsc3_xlink_t;

typedef struct atsc3_frame_rate {
	uint32_t numerator;
	uint32_t denom;

} atsc3_frame_rate_t;


/*
 *  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
 */

typedef struct atsc3_route_role {
	char* scheme_id_uri;
	char* value;
} atsc3_route_role_t;

/*
<Representation audioSamplingRate="48000" bandwidth="387000" codecs="mp4a.40.2" id="1">
*/


/*
<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
*/

typedef struct atsc3_route_audio_channel_configuration {
	char* scheme_id_uri;
	uint32_t value;
} atsc3_route_audio_channel_configuration_t;

/*
<SegmentTemplate duration="2002000" initialization="test-$RepresentationID$-init.mp4a"
media="test-$RepresentationID$-$Number$.mp4a" presentationTimeOffset="2185049000" startNumber="1103" timescale="1000000"/>
 */

typedef struct atsc3_route_segment_template {
	uint32_t 	duration;
	char*		initialization;
	char*		media;
	uint32_t	presentation_time_offset;
	uint32_t	start_number;
	uint32_t	timescale;

} atsc3_route_segment_template_t;


/**
 *       <AdaptationSet
 *       contentType="video"
 *       id="0"
 *       maxFrameRate="30000/1001"
 *
 *       maxHeight="1080"
 *       maxWidth="1920"
 *       mimeType="video/mp4"
 *       minFrameRate="30000/1001"
 *       minHeight="1080"
 *       minWidth="1920"
 *       par="16:9"
 *       segmentAlignment="true"
 *       startWithSAP="1">
 *
 */


typedef struct atsc3_route_representation {
	uint32_t		audio_sampling_rate;
	uint32_t		bandwidth;
	char*			codecs;
	uint32_t 		id;

	atsc3_route_audio_channel_configuration_t	atsc3_route_audio_channel_configuration;
	atsc3_route_segment_template_t				atsc3_route_segment_template;

} atsc3_route_representation_t;


typedef struct atsc3_route_adaptation_set {
	char*										content_type;
	char*										id;
	char*										max_frame_rate_str;
	atsc3_frame_rate_t 							max_frame_rate;
	uint32_t									max_height;
	uint32_t									max_width;
	char*										mime_type;
	char*										min_frame_str;

	atsc3_frame_rate_t							min_frame_rate;
	uint32_t									min_height;
	uint32_t									min_width;
	char*										par;
	bool										segment_alignment;
	bool										start_with_sap;
	atsc3_route_role_t							atsc3_route_role;
	atsc3_route_representation_t 				atsc3_route_representation;

} atsc3_route_adaptation_set_t;

/*
 *    <Period id="P21" start="PT2155S" xlink:actuate="onLoad"
 *    xlink:href="tag:atsc.org,2016:xlinkhttps://atsc3vod.npgco.com/crengine?clientid=%clientid%&amp;uniqueid=775&amp;thisname=SNPR55030&amp;nextid=774&amp;nextname=SKPR60825"
 *    xmlns:xlink="http://www.w3.org/1999/xlink">
 *
 */
typedef struct atsc3_route_period {
	char* 			id;
	char*			start;
	atsc3_xlink_t* 	atsc3_xlink;
	ATSC3_VECTOR_BUILDER_STRUCT(atsc3_route_adaptation_set)

} atsc3_route_period_t;




ATSC3_VECTOR_BUILDER_METHODS_INTERFACE(atsc3_route_period, atsc3_route_adaptation_set)

//<MPD availabilityStartTime="2019-02-19T21:40:50Z" maxSegmentDuration="PT2S" minBufferTime="PT2S" minimumUpdatePeriod="PT10S"
//profiles="urn:mpeg:dash:profile:isoff-live:2011" publishTime="2019-02-19T21:40:50Z" timeShiftBufferDepth="PT20S" type="dynamic"
//xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
//xsi:schemalocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">

typedef struct atsc3_route_mpd {
	char* availability_start_time;
	char* max_segment_duration;
	char* min_buffer_time;
	char* minimum_update_period;
	char* profiles;
	char* publish_time;
	char* time_shift_buffer_depth;
	char* type;

	ATSC3_VECTOR_BUILDER_STRUCT(atsc3_route_period)

} atsc3_route_mpd_t;

ATSC3_VECTOR_BUILDER_METHODS_INTERFACE(atsc3_route_mpd, atsc3_route_period)


atsc3_route_mpd_t* atsc3_route_mpd_parse_from_payload(char* payload, char* content_location);
atsc3_route_mpd_t* atsc3_route_mpd_parse_period(xml_node_t* period_node, atsc3_route_mpd_t* atsc3_route_mpd);
atsc3_route_period_t* atsc3_route_mpd_parse_adaption_set(xml_node_t* xml_period_node, atsc3_route_period_t* atsc3_route_period);
atsc3_route_adaptation_set_t* atsc3_route_mpd_parse_role_set(xml_node_t*  xml_node, atsc3_route_adaptation_set_t* atsc3_route_adaptation_set);
atsc3_route_adaptation_set_t* atsc3_route_mpd_parse_representation_set(xml_node_t* xml_node,  atsc3_route_adaptation_set_t* atsc3_route_adaptation_set);

atsc3_route_adaptation_set_t* atsc3_route_mpd_parse_audio_channel_configuration_set(xml_node_t*  xml_node, atsc3_route_adaptation_set_t* atsc3_route_adaptation_set);
atsc3_route_adaptation_set_t* atsc3_route_mpd_parse_segment_template_set(xml_node_t* xml_node,  atsc3_route_adaptation_set_t* atsc3_route_adaptation_set);

void atsc3_route_mpd_dump(atsc3_route_mpd_t* atsc3_route_mpd);


#define _ATSC3_ROUTE_MPD_PARSER_ERROR(...)  printf("%s:%d:ERROR:",__FILE__,__LINE__);_ATSC3_UTILS_PRINTLN(__VA_ARGS__);
#define _ATSC3_ROUTE_MPD_PARSER_WARN(...)   printf("%s:%d:WARN:",__FILE__,__LINE__);_ATSC3_UTILS_PRINTLN(__VA_ARGS__);
#define _ATSC3_ROUTE_MPD_PARSER_INFO(...)   if(_ROUTE_MPD_PARSER_INFO_ENABLED)  { printf("%s:%d:INFO :",__FILE__,__LINE__);_ATSC3_UTILS_PRINTLN(__VA_ARGS__); }
#define _ATSC3_ROUTE_MPD_PARSER_DEBUG(...)  if(_ROUTE_MPD_PARSER_DEBUG_ENABLED) { printf("%s:%d:DEBUG:",__FILE__,__LINE__);_ATSC3_UTILS_PRINTLN(__VA_ARGS__); }


#endif /* ATSC3_ROUTE_MPD_H_ */
