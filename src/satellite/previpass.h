// -------------------------------------------------------------------------- //
//! @file   previpass.h
//! @brief  Satellite pass prediction algorithms defines
//! @author Kineis
//! @date   2020-01-14
// -------------------------------------------------------------------------- //

// -------------------------------------------------------------------------- //
//! \page pass_prediction_lib_page Satellite pass prediction library
//!
//! This page is presenting the satellite pass prediction software library for KINEIS constellation.
//!
//!
//! \section pass_prediction_lib_usage Usage
//!
//!
//! \subsection pass_prediction_lib_config Configuration
//!
//!
//! Algorithm configuration is given in \ref PredictionPassConfiguration_t structure
//! with the following fields:
//! - beaconLatitude (float)         : Geodetic latitude of the beacon (deg.) [-90, 90]
//! - beaconLongitude (float)        : Geodetic longitude of the beacon (deg.E) [0, 360]
//! - start (Date and time)          : Beginning of prediction (Y/M/D, hh:mm:ss)
//! - end (Date and time)            : End of prediction (Y/M/D, hh:mm:ss)
//! - minElevation (float)           : Minimum elevation of passes [0, 90] (default 5 deg)
//! - maxElevation (float)           : Maximum elevation of passes [maxElevation >= minElevation]
//!                                    (default 90 deg)
//! - minPassDurationMinute (float)  : Minimum duration (default 5 minutes)
//! - maxPasses (int)                : Maximum number of passes per satellite (default 1000)
//! - timeMarginMinPer6months (float): Linear time margin (in minutes/6months)
//!                                    (default 5 minutes/6months)
//! - computationStepSecond (int)    : Computation step (default 30s)
//!
//! Passes are filtered by their elevation and duration.
//!
//! A maximum number of passes can be set for each satellite.
//!
//! A linear time margin can be added to compensate for the drift of the beacon and/or the satellite
//! orbits. The typical margin to compensate for the satellite drift is +/-5 minutes over 6 months
//! (= 19 ppm), which results in all satellite passes being computed with a duration 10mn greater
//! that normal (i.e. almost double) after 6 months.
//! Note the drift in the MCU oscillator should also be considered, thus increasing the time margin.
//! As a consequence, the beacon should make sure to update the AOP data every few months at maximum
//! to compute the most accurate satellite passes as possible.
//!
//! The computation step defines how accurate the results will be. A smaller value leads
//! to a slower computation since the algorithm is almost linear in time with this parameter.
//!
//!
//! \subsection pass_prediction_lib_orbit_data Orbital data
//!
//!
//! Satellite pass prediction is based on orbital bulletin provided for each satellite.
//! This information should be stored in a \ref AopSatelliteEntry_t structure with
//! following fields:
//! - satHexId (int)                      : Hexadecimal satellite id. Current constellation is:
//!   - NOAA_K  = 0x5
//!   - ANGELS  = 0x6
//!   - NOAA_N  = 0x8
//!   - METOP_B = 0x9
//!   - METOP_A = 0xA
//!   - METOP_C = 0xB
//!   - NOAA_P  = 0xC
//!   - SARAL   = 0xD
//! - satDcsId (int)                      : A-DCS address. Not used by prepas.
//! - downlinkStatus (int)                : Downlink satellite status (generic format:
//!                                         \ref SatDownlinkStatus_t)
//! - uplinkStatus (int)                  : Uplink satellite status (generic format:
//!                                         \ref SatUplinkStatus_t)
//! - bulletin (Date and time)            : Bulletin epoch (year month day hour min sec)
//! - semiMajorAxisKm (float)             : Semi-major axis (km)
//! - inclinationDeg(float)               : Orbit inclination (deg)
//! - ascNodeLongitudeDeg(float)          : Longitude of ascending node (deg)
//! - ascNodeDriftDeg(float)              : Ascending node drift during one revolution (deg)
//! - orbitPeriodMin(float)               : Orbit period (min)
//! - semiMajorAxisDriftMeterPerDay(float): Drift of semi-major axis (m/day)
//!
//! One can find these information at https://argos-system.cls.fr (need to login).
//! Be careful downlink and uplink status are provided with a different format than in prepas
//! (Constellation Format A).
//!
//! Here is what you need to do to convert constellation format A to generic:
//! * "Downlink operating status" conversion
//!     * 0 -> SAT_DNLK_OFF
//!     * 1 -> SAT_DNLK_ON_WITH_A4
//!     * 2 -> SAT_DNLK_OFF
//!     * 3 -> SAT_DNLK_ON_WITH_A3
//!     * If sat ID is NOAA_K (0x5) or NOAA_N (0x8) -> SAT_DNLK_OFF
//!
//! * "Uplink operating status" conversion
//!     * 0 -> If sat ID NOAA_K (0x5) or NOAA_N (0x8), SAT_UPLK_ON_WITH_A2 else SAT_UPLK_ON_WITH_A3
//!     * 1 -> If sat ID NOAA_K (0x5) or NOAA_N (0x8), SAT_UPLK_ON_WITH_A2 else SAT_UPLK_ON_WITH_NEO
//!     * 2 -> If sat ID NOAA_K (0x5) or NOAA_N (0x8), SAT_UPLK_ON_WITH_A2 else SAT_UPLK_ON_WITH_A4
//!     * 3 -> SAT_UPLK_OFF
//!
//!
//! \subsection pass_prediction_lib_call Call sequence
//!
//!
//! \subsubsection pass_prediction_lib_multiple_pass Multiple passes strategy
//!
//!
//! First, one should call \ref PREVIPASS_compute_new_prediction_pass_times. This function
//! build a list of passes corresponding to the configuration described above.
//!
//! Then, \ref PREVIPASS_process_existing_sorted_passes can be called in order to
//! know the current status of the constellation above a beacon. It helps understanding which kind
//! of UL/DL modulation has to be configured on ARGOS transceiver side of the beacon.
//! This function is statefull, it means the returned transceiver transition corresponds at a
//! change since last call.
//! Typically, the beacon can call this function each time new transmission or reception is needed
//! on KINEIS network.
//!
//! Transitions can be:
//! - Constellation is in the same state since last call
//! - RX or TX are now enabled or disabled
//! - Modulation of current transceiver RX and/or TX state has changed (for example if
//!   an A2 generation satellite is now in visibility while an A3 one was already above
//!   the beacon).
//!
//! \ref PREVIPASS_compute_new_prediction_pass_times should be called periodically to avoid an
//! empty prediction list. For example, it could be called once a day, to predict the passes of
//! next incoming 24 hours.
//!
//!
//! \subsubsection pass_prediction_lib_single_pass Single pass strategy
//!
//!
//! \ref PREVIPASS_compute_next_pass provides the next pass, including the current
//! pass when it is in progress. If two passes are in progress at the instant of the
//! call, the first satellite in the AOP table is returned.
//!
//! \ref PREVIPASS_compute_next_pass_with_status provides the next pass corresponding
//! to criteria on downlink and uplink capacities.
//!
//! **The single pass strategy does not handle covisibility.** This can lead to unoptimized
//! configuration of the transceiver for satellites which become visible during the returned
//! pass, for instance:
//! * the beacon keeps only transmitting to a TX-only satellites while a RX-TX-capable
//! satellite becomes visible in the middle of the current pass. Thus the beacon does not
//! take advantages of DL.
//! * The beacon keeps transmitting in A3 to a A3-capable satellite, while an A2-capable
//! satellite becomes vissible in the middle of the current pass. The beacon could have
//! switch to A2 modulation in a way to increase its chances to have its messages received
//! by both satellites.
//!
//!
//! \section Code
//!
//!
//! Refer to following module for a detailled documentation of the code
//! * \ref ARGOS-PASS-PREDICTION-LIBS library files
//!
//! Refer to following module for a detailled documentation of the code
//! * \ref ARGOS-PASS-PREDICTION-LIBS library files
//!
// -------------------------------------------------------------------------- //
#ifdef __cplusplus
extern "C" {
#endif

#pragma once

// -------------------------------------------------------------------------- //
// Includes
// -------------------------------------------------------------------------- //

#include "previpass_defs.h"
#include "previpass_util.h"
#include <stdbool.h>
#include <stdlib.h>

// -------------------------------------------------------------------------- //
//! @addtogroup ARGOS-PASS-PREDICTION-LIBS
//! @brief Satellite pass prediction library  (refer to \ref pass_prediction_lib_page page for
//!        general description).
//! @{
// -------------------------------------------------------------------------- //

// -------------------------------------------------------------------------- //
//! \brief Gives action to perform once the iterative of satellite passes
//!        processing is done.
// -------------------------------------------------------------------------- //

enum TransceiverAction_t {
    UNKNOWN_TRANSCEIVER_ACTION, //!< No action
    ENABLE_TX_ONLY,             //!< Enable TX only
    ENABLE_RX_ONLY,             //!< Enable RX only
    ENABLE_TX_AND_RX,           //!< Enable RX after TX
    DISABLE_TX_RX,              //!< Disable RX/TX
    CHANGE_TX_MOD,              //!< New TX capacity detected
    CHANGE_RX_MOD,              //!< New RX capacity detected
    CHANGE_TX_PLUS_RX_MOD,      //!< New TX and RX capacity detected
    KEEP_TRANSCEIVER_STATE      //!< Keep transceiver state
};

// -------------------------------------------------------------------------- //
//! Computation configuration
// -------------------------------------------------------------------------- //
struct PredictionPassConfiguration_stu90_t {
    float beaconLatitude;  //!< Geodetic latitude of the beacon (deg.) [-90, 90]
    float beaconLongitude; //!< Geodetic longitude of the beacon (deg.E) [0, 360]

    uint32_t start_stu90; //!< Beginning of prediction (Y/M/D, hh:mm:ss)

    uint32_t end_stu90; //!< End of prediction (Y/M/D, hh:mm:ss)

    float minElevation; //!< Minimum elevation of passes [0, 90]
    float maxElevation; //!< Maximum elevation of passes [maxElevation >= minElevation]

    float minPassDurationMinute; //!< Minimum duration (in minutes)

    uint32_t maxPasses; //!< Maximum number of passes per satellite

    float timeMarginMinPer6months; //!< Linear time margin (in minutes/6months)

    uint32_t computationStepSecond; //!< Computation step (in seconds)
};

struct PredictionPassConfiguration_t {
    float beaconLatitude;  //!< Geodetic latitude of the beacon (deg.) [-90, 90]
    float beaconLongitude; //!< Geodetic longitude of the beacon (deg.E) [0, 360]

    struct CalendarDateTime_t start; //!< Beginning of prediction (Y/M/D, hh:mm:ss)

    struct CalendarDateTime_t end; //!< End of prediction (Y/M/D, hh:mm:ss)

    float minElevation; //!< Minimum elevation of passes [0, 90]
    float maxElevation; //!< Maximum elevation of passes [maxElevation >= minElevation]

    float minPassDurationMinute; //!< Minimum duration (in minutes)

    uint32_t maxPasses; //!< Maximum number of passes per satellite

    float timeMarginMinPer6months; //!< Linear time margin (in minutes/6months)

    uint32_t computationStepSecond; //!< Computation step (in seconds)
};

// -------------------------------------------------------------------------- //
//! \brief Compress next pass satellite information in one 64bit word as a
//!    bitfield structure.
// -------------------------------------------------------------------------- //

struct SatelliteNextPassPrediction_t {
    //! Information on satellite pass itself with most important information
    uint64_t epoch : 32;       //!< Bulletin epoch next pass (136 years from 1970/01/01)
    uint64_t duration : 11;    //!< Duration (sec) : 11 bits (max 2047s, 34mins)
    uint64_t elevationMax : 7; //!< Max elevation during pass (deg), max is 90

    //! Information on satellite ID
    uint64_t satHexId : 8; //! Hexadecimal satellite id [0x01..0xFF]

    //! Configuration type
    uint64_t downlinkStatus : 3; //!< 4 maximum, see SatDownlinkStatus_t
    uint64_t uplinkStatus : 3;   //!< 5 maximum, see SatUplinkStatus_t
};

// -------------------------------------------------------------------------- //
//! Linked list element for satellite pass prediction.
// -------------------------------------------------------------------------- //

struct SatPassLinkedListElement_t {
    struct SatelliteNextPassPrediction_t element; //!< Pass prediction values
    struct SatPassLinkedListElement_t *next;      //!< Pointer to next element in list
};

// -------------------------------------------------------------------------- //
//! \brief Info about one pass
//!
//! Structure returned by pass processing function to give opportunity for
//! transceiver to adapt to the satellite(s) present on next pass.
// -------------------------------------------------------------------------- //

struct NextPassTransceiverCapacity_t {
    enum TransceiverAction_t trcvrActionForNextPass; //!< Transition nature
    enum SatDownlinkStatus_t maxDownlinkStatus;      //!< Maximum Downlink status during the pass
    enum SatUplinkStatus_t minUplinkStatus;          //!< Minimum Uplink status during the pass
};

// -------------------------------------------------------------------------- //
//! Values for default AOP entry
// -------------------------------------------------------------------------- //

struct AopSatelliteEntry_t
PREVIPASS_default_aop_satellite_entry(void);

// -------------------------------------------------------------------------- //
//! \brief Conversion between constellation status type A and generic status.
//!
//! This function convert allcast status information of format type A into the
//! generic status format which covers all configurations.
//!
//! * "Downlink operating status" conversion
//!     * 0 -> SAT_DNLK_OFF
//!     * 1 -> SAT_DNLK_ON_WITH_A4
//!     * 2 -> SAT_DNLK_ON_WITH_SPARE
//!     * 3 -> SAT_DNLK_ON_WITH_A3
//!     * over -> SAT_DNLK_ON_WITH_SPARE
//!     * If sat ID is NOAA_K (0x5) or NOAA_N (0x8) -> SAT_DNLK_OFF
//!
//! * "Uplink operating status" conversion
//!     * 0 -> If sat ID NOAA_K (0x5) or NOAA_N (0x8), SAT_UPLK_ON_WITH_A2 else SAT_UPLK_ON_WITH_A3
//!     * 1 -> If sat ID NOAA_K (0x5) or NOAA_N (0x8), SAT_UPLK_ON_WITH_A2 else SAT_UPLK_ON_WITH_NEO
//!     * 2 -> If sat ID NOAA_K (0x5) or NOAA_N (0x8), SAT_UPLK_ON_WITH_A2 else SAT_UPLK_ON_WITH_A4
//!     * 3 -> SAT_UPLK_OFF
//!
//! @note Satellite heaxdecimal Id is expected since NOAA K and NOAA N
//!    satellites have special meaning for these fields (see Arogs messages
//!    specification).
//!
//! \param[in] downlinkStatusFormatA
//!    Format A constellation status downlink info
//! \param[in] uplinkStatusFormatA
//!    Format A constellation status uplink info
//! \param[in] satHexId
//!    Satellite hexadecimal Id
//! \param[out] downlinkStatusGeneric
//!    Generic status downlink info
//! \param[out] uplinkStatusGeneric
//!    Generic status uplink info
// -------------------------------------------------------------------------- //

void PREVIPASS_status_format_a_to_generic(
    enum SatDownlinkStatusFormatA_t downlinkStatusFormatA,
    enum SatUplinkStatusFormatA_t uplinkStatusFormatA,
    SatHexId_t satHexId,
    enum SatDownlinkStatus_t *downlinkStatusGeneric,
    enum SatUplinkStatus_t *uplinkStatusGeneric);

// -------------------------------------------------------------------------- //
//! \brief Conversion between constellation status type B and generic status.
//!
//! This function convert allcast status information of format type B into the
//! generic status format which covers all configurations.
//!
//! \param[in] downlinkStatusFormatB
//!    Format B constellation status downlink info
//! \param[in] operatingStatusFormatB
//!    Format B constellation status uplink info
//! \param[out] downlinkStatusGeneric
//!    Generic status downlink info
//! \param[out] uplinkStatusGeneric
//!    Generic status uplink info
// -------------------------------------------------------------------------- //

void PREVIPASS_status_format_b_to_generic(
    enum SatDownlinkStatusFormatB_t downlinkStatusFormatB,
    enum SatOperatingStatusFormatB_t operatingStatusFormatB,
    enum SatDownlinkStatus_t *downlinkStatusGeneric,
    enum SatUplinkStatus_t *uplinkStatusGeneric);

// -------------------------------------------------------------------------- //
//! \brief Conversion between constellation generic status and status type A.
//!
//! This function convert generic status into allcast status information of
//! format type A.
//!
//! \param[in] downlinkStatusGeneric
//!    Generic status downlink info
//! \param[in] uplinkStatusGeneric
//!    Generic status uplink info
//! \param[out] downlinkStatusFormatA
//!    Format A constellation status downlink info
//! \param[out] uplinkStatusFormatA
//!    Format A constellation status uplink info
// -------------------------------------------------------------------------- //

void PREVIPASS_status_generic_to_format_a(
    enum SatDownlinkStatus_t downlinkStatusGeneric,
    enum SatUplinkStatus_t uplinkStatusGeneric,
    enum SatDownlinkStatusFormatA_t *downlinkStatusFormatA,
    enum SatUplinkStatusFormatA_t *uplinkStatusFormatA);

bool PREVIPASS_estimate_next_pass_with_status(
    const struct PredictionPassConfiguration_stu90_t *config,
    const struct AopSatelliteEntry_t *aopTableEntry,
    struct SatelliteNextPassPrediction_t *passPtrPtr);

// -------------------------------------------------------------------------- //
//! \brief Main Prepas library function
//!
//! Build a sorted linked list of satellites passes above a location.
//! Configuration is given in a dedicated structure. Then passes are analysed
//! in order to extract the various configurations present in the list.
//!
//! Information about satellite orbits are given in a array of type
//! AopSatelliteEntry_t. This structure contains orbit period, semi-major axis,
//! etc.
//!
//! Once the list is built and analyzed, one can use
//! PREVIPASS_process_existing_sorted_passes with a date to known which
//! satellites are currently above the beacon with which downlink and uplink
//! capacities.
//!
//! Linked list is allocated in the static variable __mallocBytesPool.
//! MY_MALLOC_MAX_BYTES should be set to fit number of passes corresponding
//! to the configuration.
//!
//! \see PredictionPassConfiguration_t
//! \see AopSatelliteEntry_t
//! \see PREVIPASS_process_existing_sorted_passes
//!
//! \param[in] config
//!    Configuration of passes computation
//! \param[in] aopTable
//!    Array of info about each satellite
//! \param[in] nbSatsInAopTable
//!    Number of satellite in AOP table
//! \param[out] retStatus
//!    Computation status:
//!         * true means: ok prediction is fine. previsionPassesList equals NULL means there is no
//!           prediction found in interval, this is not an error.
//!         * false means error such as:
//!           * detection of incompatible bulletin. All birthday is in the future
//!           compared to config.start, the prediction cannot be done
//!           * memory pool overflow
//!
//! \return Pointer to the first pass of the linked list. NULL if no pass has
//!    been inserted into the list.
// -------------------------------------------------------------------------- //

struct SatPassLinkedListElement_t *
PREVIPASS_compute_new_prediction_pass_times(
    const struct PredictionPassConfiguration_t *config,
    const struct AopSatelliteEntry_t *aopTable,
    bool *retStatus);

// -------------------------------------------------------------------------- //
//! \brief Main Prepas library function with status filtering
//!
//! Satellites are filtered on their downlink and uplink capacities.
//!
//! \see PREVIPASS_compute_new_prediction_pass_times
//!
//! \param[in] config
//!    Configuration of passes computation
//! \param[in] aopTable
//!    Array of info about each satellite
//! \param[in] nbSatsInAopTable
//!    Number of satellite in AOP table
//! \param[in] downlinkStatus
//!    donwlink capacity to be selected
//! \param[in] uplinkStatus
//!    Minimum uplink capacity
//! \param[out] retStatus
//!    Computation status:
//!         * true means: ok prediction is fine. previsionPassesList equals NULL means there is no
//!           prediction found in interval, this is not an error.
//!         * false means error such as:
//!           * detection of incompatible bulletin. All birthday is in the future
//!           compared to config.start, the prediction cannot be done
//!           * memory pool overflow
//!
//! \return Pointer to the first pass of the linked list. NULL if no pass has
//!    been inserted into the list.
// -------------------------------------------------------------------------- //

struct SatPassLinkedListElement_t *
PREVIPASS_compute_new_prediction_pass_times_with_status(
    const struct PredictionPassConfiguration_t *config,
    const struct AopSatelliteEntry_t *aopTable,
    enum SatDownlinkStatus_t downlinkStatus,
    enum SatUplinkStatus_t uplinkStatus,
    bool *retStatus);

// -------------------------------------------------------------------------- //
//! \brief Define transmitter action based on passes list.
//!
//! Process the sorted list of passes to get the new TX/RX activity information
//! according to pass information and current time.
//!
//! Transition since next call can be:
//! - Enable or disable TX and/or RX
//! - Change current status modulation
//!
//! \note The function shall be executed periodically.
//!
//! \see NextPassTransceiverCapacity_t
//! \see PREVIPASS_compute_new_prediction_pass_times
//!
//! \param[in] currentTime
//!    Current epoch time
//! \param[in] previsionPassesList
//!    Input data of passes to be processed
//!
//! \return Which action to be done on transceiver for next transmit occasion
//!    with some information of supported RX/TX modulations.
// -------------------------------------------------------------------------- //

struct NextPassTransceiverCapacity_t
PREVIPASS_process_existing_sorted_passes(
    uint32_t currentTime,
    struct SatPassLinkedListElement_t *previsionPassesList);

// -------------------------------------------------------------------------- //
//! \brief Get next pass
//!
//! This function returns information about the next pass of a satellite above
//! a beacon location. If a pass is in progress, it is considered to determine
//! the transceiver status.
//!
//! \note Current date is given in the start field of the configuration
//!    structure. The computation searches for a maximum of 24h. If this limit
//!    is reached, it is likely because of a misconfiguration.
//!
//! \param[in] config
//!    Configuration of passes computation
//! \param[in] aopTable
//!    Array of info about each satellite
//! \param[in] nbSatsInAopTable
//!    Number of satellite in AOP table
//! \param[out] nextPass
//!    Pointer to the structure where transceiver status will be stored
//!
//! \return True if a pass has been found.
// -------------------------------------------------------------------------- //

bool PREVIPASS_compute_next_pass(
    const struct PredictionPassConfiguration_t *config,
    const struct AopSatelliteEntry_t *aopTable,
    struct SatelliteNextPassPrediction_t *nextPass);

// -------------------------------------------------------------------------- //
//! \brief Get next pass depending on configuration
//!
//! Satellites are filtered on their downlink and uplink capacities.
//!
//! \see PREVIPASS_compute_next_pass
//!
//! \param[in] config
//!    Configuration of passes computation
//! \param[in] aopTable
//!    Array of info about each satellite
//! \param[in] nbSatsInAopTable
//!    Number of satellite in AOP table
//! \param[in] downlinkStatus
//!    donwlink capacity to be selected
//! \param[in] uplinkStatus
//!    Minimum uplink capacity
//! \param[out] nextPass
//!    Pointer to the structure where transceiver status will be stored
//!
//! \return True if a pass has been found.
// -------------------------------------------------------------------------- //

bool PREVIPASS_compute_next_pass_with_status(
    const struct PredictionPassConfiguration_t *config,
    const struct AopSatelliteEntry_t *aopTable,
    enum SatDownlinkStatus_t downlinkStatus,
    enum SatUplinkStatus_t uplinkStatus,
    struct SatelliteNextPassPrediction_t *nextPass);

// -------------------------------------------------------------------------- //
//! @} (end addtogroup ARGOS-PASS-PREDICTION-LIBS)
// -------------------------------------------------------------------------- //
#ifdef __cplusplus
}
#endif
