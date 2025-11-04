// -------------------------------------------------------------------------- //
//! @file   previpass.c
//! @brief  Satellite pass prediction algorithms
//! @author Kineis
//! @date   2020-01-14
// -------------------------------------------------------------------------- //


// -------------------------------------------------------------------------- //
// Includes
// -------------------------------------------------------------------------- //

#include <math.h>
#include "previpass.h"
#include "previpass_util.h"
#include <stdio.h>


// -------------------------------------------------------------------------- //
//! @addtogroup ARGOS-PASS-PREDICTION-LIBS
//! @{
// -------------------------------------------------------------------------- //

//! Only ever predicting single path for a single satellite at a give time
#define MY_MALLOC_MAX_BYTES 16

// -------------------------------------------------------------------------- //
// Defines values
// -------------------------------------------------------------------------- //

//! Maximum number of configuration. Should be greater than satellite number.
//! Should not be greater than 64 (uint64_t bitmap).
#define MAX_SATELLITES_HANDLED_IN_COVISI 64

//! Memory pool size. Used for linked list. 16 bytes per pass. 35 pass per day.
//! Min value for function 'PREVIPASS_compute_next_pass' : 
//! 	sizeof(struct SatPassLinkedListElement_t) * nb_sat
#ifndef MY_MALLOC_MAX_BYTES
#define MY_MALLOC_MAX_BYTES 2500
#endif

//! Check satellite ID is valid or not
#define IS_SATID_VALID(satID) (satID != HEXID_INVALID)


// -------------------------------------------------------------------------- //
//! Holds configuration of a satellite: IDs and uplink/downlink information.
// -------------------------------------------------------------------------- //

struct SatelliteConfiguration_t {
	// Information on satellite ID
	uint16_t satHexId       : 8 ; //!< Hexadecimal satellite id

	// Configuration type
	uint16_t downlinkStatus : 3 ; //!< 4 maximum, see SatDownlinkStatus_t
	uint16_t uplinkStatus   : 3 ; //!< 5 maximum, see SatUplinkStatus_t

	// Padding
	uint16_t unused         : 2 ; //!< Spare bits
};


// -------------------------------------------------------------------------- //
// Static variables for prepas function
// -------------------------------------------------------------------------- //

//! Memory pool
#ifndef UNIT_TEST
static
#endif
uint8_t  __mallocBytesPool[MY_MALLOC_MAX_BYTES];

//! Memory pool index
static uint16_t __mallocIdx;

// Fill up max value of each field of a satellite pass prediction.
// As fields are unsigned types, maximum found with -1
static const struct SatelliteNextPassPrediction_t SatPassMaxValues = {
	.epoch = -1,
	.duration = -1,
	.elevationMax = -1,
	.satHexId = -1,
	.downlinkStatus = -1,
	.uplinkStatus = -1
};



// -------------------------------------------------------------------------- //
//! \brief Geometric computation of passes
//!
//! Satellite passes prediction. Circular method taking into account drag
//! coefficient (friction effect) and J2 term of earth potential.
//!
//! Passes are detected by comparing current beacon to satellite distance to a
//! minimum distance based on satellite altitude.
//!
//! Analysis is done each computationStep seconds. When leaving a visibility, a
//! fixed amount of time is added to faster the computation and jump closed to
//! the next pass.
//!
//! Satellites are filtered on their downlink and uplink capacities.
//!
//! A linear time margin is added in order to compensate potential satellite
//! derivation when AOP are old. It is added at the beginning and the end of
//! each pass.
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
//!    Donwlink capacity to be selected
//! \param[in] uplinkStatus
//!    Minimum uplink capacity
//! \param[out] previsionPassesList
//!    Pointer to linked list pointer. NULL means there is no pass during the configured period.
//!
//! \return computation status,
//!         * true means: ok prediction is fine. previsionPassesList equals NULL means there is no
//!           prediction found in interval, this is not an error.
//!         * false means error such as:
//!           * config start date is older than config end date
//!           * detection of incompatible bulletin. All birthday is in the future
//!           compared to config.start, the prediction cannot be done
//!           * memory pool overflow
// -------------------------------------------------------------------------- //

bool PREVIPASS_estimate_next_pass_with_status
(
	const struct PredictionPassConfiguration_stu90_t *config,
	const struct AopSatelliteEntry_t           *aopTable,
	struct SatelliteNextPassPrediction_t  *passPtr
)
{
	bool b_retval = false;

	// Beacon position in cartesian coordinates
	float xBeaconCartesian = cosf(config->beaconLatitude  * C_MATH_DEG_TO_RAD)
		* cosf(config->beaconLongitude * C_MATH_DEG_TO_RAD);
	float yBeaconCartesian = cosf(config->beaconLatitude  * C_MATH_DEG_TO_RAD)
		* sinf(config->beaconLongitude * C_MATH_DEG_TO_RAD);
	float zBeaconCartesian = sinf(config->beaconLatitude  * C_MATH_DEG_TO_RAD);

	if ( config->start_stu90 >= config->end_stu90)
		return false;

	// Main loop : computation for each satellite
	if (!IS_SATID_VALID(aopTable->satHexId))
		return false;

//		fprintf(stdout, "[DBG]elt.ID=0x%x\n", aopTable->satHexId);

	// Detection of invalid satellite
	if (aopTable->bulletin.year == 0)
		return false;

	// Number of seconds since bulletin epoch
	uint32_t bullSec90;

	PREVIPASS_UTIL_date_calendar_stu90(&aopTable->bulletin, &bullSec90);

	// Detection of incompatible bulletin
	// @note When the AOP birthday is in the future compared to config.start,
	// the prediction should not be done on the current satellite.
	if (config->start_stu90 < bullSec90)
		return false;
	else
	b_retval = true;

	// Detection of offline satellite: SAT is fully OFF on DL and UL status
	if (aopTable->downlinkStatus == SAT_DNLK_OFF
			&& aopTable->uplinkStatus == SAT_UPLK_OFF)
		return true;

	// Unique computation of cos and sin of inclination
	float sinInclination = sinf(aopTable->inclinationDeg * C_MATH_DEG_TO_RAD);
	float cosInclination = cosf(aopTable->inclinationDeg * C_MATH_DEG_TO_RAD);

	// Conversion from m per day to km per day
	float semiMajorAxisDriftKmPerDay = aopTable->semiMajorAxisDriftMeterPerDay /
						1000;

	// Computation of minimum squared distance
	float visibilityMinDistance2 = PREVIPASS_UTIL_sat_elevation_distance2(
					config->minElevation,
					aopTable->semiMajorAxisKm);

	// Conversions
	float orbitPeriodSec = aopTable->orbitPeriodMin * 60.f;
	float meanMotionBaseRevPerSec = C_MATH_TWO_PI / orbitPeriodSec;
	float ascNodeRad = aopTable->ascNodeLongitudeDeg * C_MATH_DEG_TO_RAD;
	float ascNodeDriftRad = aopTable->ascNodeDriftDeg * C_MATH_DEG_TO_RAD;
	float earthRevPerSec = ascNodeDriftRad / orbitPeriodSec;
	uint32_t secondsSinceBulletin = (uint32_t)config->start_stu90 - (uint32_t)bullSec90;
	uint32_t computationEndSecondsSinceBulletin =  (uint32_t)config->end_stu90 - (uint32_t)bullSec90;

	// Mean motion computation
	float numberOfRevSinceBulletin = ((uint32_t)secondsSinceBulletin) / orbitPeriodSec;
	float meanMotionDriftRevPerSec = -.75f
		* semiMajorAxisDriftKmPerDay
		/ aopTable->semiMajorAxisKm
		/ 86400.0f
		* C_MATH_TWO_PI
		* numberOfRevSinceBulletin;
	float meanMotionRevPerSec = meanMotionBaseRevPerSec + meanMotionDriftRevPerSec;

	// Use current pass
	float distance2 = PREVIPASS_UTIL_sat_point_distance2(secondsSinceBulletin,
			xBeaconCartesian,
			yBeaconCartesian,
			zBeaconCartesian,
			meanMotionRevPerSec,
			sinInclination,
			cosInclination,
			ascNodeRad,
			earthRevPerSec);
	if (distance2 < visibilityMinDistance2) {
		// Go back of at least one pass duration
		// TODO MJT Add analytic computation of this value
		secondsSinceBulletin -= (uint32_t)1200;
	}

	// Start computation loop for current satellite
	uint8_t isInPass = 0;
	uint32_t passDurationSec = 0;
	float elevationMax = 0;
	uint32_t passNumber = 0;

	while (secondsSinceBulletin < computationEndSecondsSinceBulletin) {
	
		// Stop computation if maximum number of passes per satellite is reached
		if (passNumber >= config->maxPasses)
			break;


		// Compute current cartesian distance
		distance2 = PREVIPASS_UTIL_sat_point_distance2(secondsSinceBulletin,
				xBeaconCartesian,
				yBeaconCartesian,
				zBeaconCartesian,
				meanMotionRevPerSec,
				sinInclination,
				cosInclination,
				ascNodeRad,
				earthRevPerSec);

		// A new pass starts or a pass continue.
		// Compute as long as distance is fine and duration does not reach maximum.
		if ((distance2 < visibilityMinDistance2) &&
			(passDurationSec <= SatPassMaxValues.duration)) {
			// Set the pass flag
			isInPass = 1;

			// Add step to pass time
			passDurationSec += (uint32_t)config->computationStepSecond;

			// Compute current satellite elevation and keep current pass maximum
			float v = 2.f * asinf(sqrtf(distance2) / 2.f);
			float elevation = (aopTable->semiMajorAxisKm
						* sinf(v))
						/ sqrtf(C_MATH_EARTH_RADIUS
						* C_MATH_EARTH_RADIUS
						+ aopTable->semiMajorAxisKm
						* aopTable->semiMajorAxisKm
						- 2
						* C_MATH_EARTH_RADIUS
						* aopTable->semiMajorAxisKm
						* cosf(v));
			elevation = C_MATH_RAD_TO_DEG * acosf(elevation);
			if (elevation > elevationMax)
				elevationMax = elevation;


			// Go to next time by one step
			secondsSinceBulletin += (uint32_t)config->computationStepSecond;
			continue;
		}

		// Leave a current pass for the first time
		if (isInPass == 1) {
			// Keep pass when elevation is not too high and duration is enough
			if (elevationMax <= config->maxElevation &&
				config->minPassDurationMinute * 60 <= passDurationSec) {
				// Count this pass
				passNumber = (uint32_t)passNumber + 1;

				// Fill sat configuration
				struct SatelliteNextPassPrediction_t ppItem;

				ppItem.satHexId       = aopTable->satHexId;
				ppItem.downlinkStatus = aopTable->downlinkStatus;
				ppItem.uplinkStatus   = aopTable->uplinkStatus;

				// Compute time margin. Ensure final duration does not exceed maximum size of duration field.
				uint32_t timeMarginSecMax = ((uint32_t)SatPassMaxValues.duration - (uint32_t)passDurationSec) >> 1 ;
				uint32_t timeMarginSec =
						(uint32_t)(config->timeMarginMinPer6months
						* (uint32_t)60
						* (uint32_t)secondsSinceBulletin
						/ (86400 * 365 / 2.f));
				timeMarginSec = (timeMarginSec > timeMarginSecMax)? timeMarginSecMax : timeMarginSec;
				// Fill structure to convert fields in a raw epoch from 1970
				ppItem.duration = (uint32_t)passDurationSec + 2 * (uint32_t)timeMarginSec;
				ppItem.epoch = (uint32_t)bullSec90
					+ (uint32_t)secondsSinceBulletin
					- (uint32_t)passDurationSec
					- (uint32_t)timeMarginSec
					+ (uint32_t)EPOCH_90_TO_70_OFFSET;
				ppItem.elevationMax = (uint32_t)elevationMax;

				// check maximum values are not reached
				if ( ppItem.epoch > SatPassMaxValues.epoch)
					return false;
				if ( ppItem.elevationMax >= 90)
					return false;

				// Add pass to list
				*passPtr = ppItem;
				return true;
			}

			// Next pass will be at in..
			//! \todo MJT : add analytic computation of this value
			secondsSinceBulletin += (uint32_t)4500 ; // 75 min

			// Continue outside visibility
			isInPass = 0;
			elevationMax = 0;
			passDurationSec = 0;
		}

		// Continue outside of a visibility
		// Step is defined according to distance
		//! \todo MJT : add analytic computation of this value
		if (distance2 < 4 * visibilityMinDistance2)
			secondsSinceBulletin += (uint32_t)config->computationStepSecond;

		if ((distance2 >= 4 * visibilityMinDistance2)
				&& (distance2 <= (16 * visibilityMinDistance2)))
			secondsSinceBulletin += (uint32_t)4 * config->computationStepSecond;
		if (distance2 > 16 * visibilityMinDistance2)
			secondsSinceBulletin += (uint32_t)20 * config->computationStepSecond;
	}

	return b_retval;
}


// -------------------------------------------------------------------------- //
//! @} (end addtogroup ARGOS-PASS-PREDICTION-LIBS)
// -------------------------------------------------------------------------- //
