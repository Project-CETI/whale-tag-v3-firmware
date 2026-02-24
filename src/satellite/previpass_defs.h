// -------------------------------------------------------------------------- //
//! @file   previpass_defs.h
//! @brief  Satellite pass prediction definintions
//! @author Kineis
//! @date   2020-10-01
// -------------------------------------------------------------------------- //

#pragma once


// -------------------------------------------------------------------------- //
// Includes
// -------------------------------------------------------------------------- //

#include <stdint.h>
// -------------------------------------------------------------------------- //
//! @addtogroup ARGOS-PASS-PREDICTION-LIBS
//! @{
// -------------------------------------------------------------------------- //

// -------------------------------------------------------------------------- //
// Generic status
// -------------------------------------------------------------------------- //

//! Generic status downlink info (Order matters)
enum SatDownlinkStatus_t {
	SAT_DNLK_OFF           = 0, //!< No downlink capacity
	SAT_DNLK_ON_WITH_A3    = 3, //!< Argos 3 downlink
	SAT_DNLK_ON_WITH_A4    = 4, //!< Argos 4 downlink
	SAT_DNLK_ON_WITH_SPARE = 6, //!< spare future downlink
};

//! Generic status uplink info (Order matters)
enum SatUplinkStatus_t {
	SAT_UPLK_OFF           = 0, //!< Satelite not operational
	SAT_UPLK_ON_WITH_A2    = 2, //!< Argos 2 instrument generation
	SAT_UPLK_ON_WITH_A3    = 3, //!< Argos 3 instrument generation
	SAT_UPLK_ON_WITH_A4    = 4, //!< Argos 4 instrument generation
	SAT_UPLK_ON_WITH_NEO   = 5, //!< Neo instrument generation
	SAT_UPLK_ON_WITH_SPARE = 6  //!< Spare future instrument generation
};


// -------------------------------------------------------------------------- //
// Downlink and uplink status format from allcast
// -------------------------------------------------------------------------- //

//! Format A constellation status downlink info
enum SatDownlinkStatusFormatA_t {
	FMT_A_SAT_DNLK_DOWNLINK_OFF   = 0, //!< Downlink transmitter OFF
	FMT_A_SAT_DNLK_A4_RX_CAPACITY = 1, //!< ARGOS-4 TX ON
	FMT_A_SAT_DNLK_SPARE_INFO     = 2, //!< Spare
	FMT_A_SAT_DNLK_A3_RX_CAPACITY = 3, //!< ARGOS-3 TX ON
};

//! Format A constellation status uplink info
enum SatUplinkStatusFormatA_t {
	FMT_A_SAT_UPLK_A3_CAPACITY    = 0, //!< ARGOS-3 payload
	FMT_A_SAT_UPLK_NEO_CAPACITY   = 1, //!< ARGOS-NEO payload on ANGELS
	FMT_A_SAT_UPLK_A4_CAPACITY    = 2, //!< ARGOS-4 payload
	FMT_A_SAT_UPLK_OUT_OF_SERVICE = 3  //!< Out of service
};

//! Format B constellation status downlink info
enum SatDownlinkStatusFormatB_t {
	FMT_B_SAT_DNLK_DOWNLINK_OFF = 0, //!< Downlink transmitter OFF
	FMT_B_SAT_DNLK_DOWNLINK_ON  = 1  //!< Downlink transmitter ON
};

//! Format B constellation status uplink info
enum SatOperatingStatusFormatB_t {
	FMT_B_SAT_UPLK_A3_CAPACITY    = 0, //!< ARGOS-3 payload
	FMT_B_SAT_UPLK_NEO_CAPACITY   = 1, //!< ARGOS-NEO payload on ANGELS
	FMT_B_SAT_UPLK_A4_CAPACITY    = 2, //!< ARGOS-4 payload
	FMT_B_SAT_UPLK_SPARE_INFO2    = 3, //!< spare
	FMT_B_SAT_UPLK_SPARE_INFO3    = 4, //!< spare
	FMT_B_SAT_UPLK_SPARE_INFO4    = 5, //!< spare
	FMT_B_SAT_UPLK_SPARE_INFO5    = 6, //!< spare
	FMT_B_SAT_UPLK_OUT_OF_SERVICE = 7  //!< Out of service
};


// -------------------------------------------------------------------------- //
// Types for satellites identification
// -------------------------------------------------------------------------- //

//! Satellite identification
typedef uint8_t SatHexId_t;

//! Satellite A-DCS address
typedef uint8_t SatAdcsAddress_t;


//! Identification of satellites
enum KnownSatellitesHexId_t {
	HEXID_INVALID    = 0x00, //!< Invalid id
	HEXID_CDARS      = 0x01, //!< CDARS (TBC) : A-DCS@ = 8 (TBC) PFM1
	HEXID_OCEANSAT_3 = 0x02, //!< OCEANSAT-3 : A-DCS@ = 9 (TBC) FM2
	HEXID_METOP_SG1B = 0x03, //!< METOP-SG1B : A-DCS@ = A (TBC) FM3
	HEXID_METOP_SG2B = 0x04, //!< METOP-SG2B : A-DCS@ = B (TBC) FM4
	HEXID_NOAA_K     = 0x05, //!< NOAA K : A-DCS@ = Not applicable – No downlink
	HEXID_ANGELS     = 0x06, //!< ANGELS : A-DCS@ = Not applicable – No downlink
	HEXID_TBD        = 0x07, //!< TBD : A-DCS@ = TBD
	HEXID_NOAA_N     = 0x08, //!< NOAA N : A-DCS@ = Not applicable – No downlink
	HEXID_METOP_B    = 0x09, //!< METOP B : A-DCS@ = 3
	HEXID_METOP_A    = 0x0A, //!< METOP A : A-DCS@ = 5
	HEXID_METOP_C    = 0x0B, //!< METOP C : A-DCS@ = 7
	HEXID_NOAA_P     = 0x0C, //!< NOAA N’ : A-DCS@ = 6
	HEXID_SARAL      = 0x0D, //!< SARAL : A-DCS@ = 4
	HEXID_TBD2       = 0x0E, //!< TBD : A-DCS@ = TBD
	HEXID_NA         = 0x0F  //!< N/A : A-DCS@ = reserved F for any ARGOS-3, ARGOS-4 instrument
};


// -------------------------------------------------------------------------- //
//! Computation configuration
// -------------------------------------------------------------------------- //

struct CalendarDateTime_t {
	uint16_t year ;   //!< Year ( year > 1900 )
	uint8_t  month ;  //!< Month ( 1 <= month <= 12 )
	uint8_t  day ;    //!< Day ( 1 <= day <= 31 )
	uint8_t  hour ;   //!< Hour ( 0 <= hour <= 23 )
	uint8_t  minute ; //!< Minute ( 0 <= minute <= 59 )
	uint8_t  second ; //!< Second ( 0 <= second <= 59 )
};


// -------------------------------------------------------------------------- //
//! Structure with information about one satellite orbit and status
// -------------------------------------------------------------------------- //

struct AopSatelliteEntry_t {
	// Satellite identification
	SatHexId_t satHexId ;        //!< Hexadecimal satellite id
	SatAdcsAddress_t satDcsId ;  //!< A-DCS address. Not used by prepas

	// Satellite status
	enum SatDownlinkStatus_t downlinkStatus ; //!< Downlink satellite status
	enum SatUplinkStatus_t uplinkStatus ;     //!< Uplink satellite status

	//! Bulletin epoch
	struct CalendarDateTime_t bulletin;

	//! Semi-major axis (km)
	float semiMajorAxisKm;

	//! Orbit inclination (deg)
	float inclinationDeg;

	//! Longitude of ascending node (deg)
	float ascNodeLongitudeDeg;

	//! Asc. node drift during one revolution (deg)
	float ascNodeDriftDeg;

	//! Orbit period (min)
	float orbitPeriodMin;

	//! Drift of semi-major axis (m/day)
	float semiMajorAxisDriftMeterPerDay;
};


// -------------------------------------------------------------------------- //
//! @} (end addtogroup ARGOS-PASS-PREDICTION-LIBS)
// -------------------------------------------------------------------------- //