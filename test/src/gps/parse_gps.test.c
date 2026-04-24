#include <unity.h>
#include <stdio.h>
#include <string.h>


#include "gps/parse_gps.c"

void setUp(void) {}
void tearDown(void) {}

/* ===== priv__validate_rmc_sentence tests ===== */

void test_validate_valid_rmc(void) {
    const uint8_t *sentence = (const uint8_t *)"$GNRMC,123456.00,A,4807.038,N,01131.000,E,0.0,0.0,230525,,,A*68";
    TEST_ASSERT_TRUE(priv__validate_rmc_sentence(sentence));
}

void test_validate_void_fix_is_invalid(void) {
    /* 'V' = void/invalid fix */
    const uint8_t *sentence = (const uint8_t *)"$GNRMC,123456.00,V,,,,,,,230525,,,N*77";
    TEST_ASSERT_FALSE(priv__validate_rmc_sentence(sentence));
}

void test_validate_wrong_sentence_type(void) {
    const uint8_t *sentence = (const uint8_t *)"$GNGGA,123456.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47";
    TEST_ASSERT_FALSE(priv__validate_rmc_sentence(sentence));
}

void test_validate_missing_dollar(void) {
    const uint8_t *sentence = (const uint8_t *)"GNRMC,123456.00,A,4807.038,N,01131.000,E,0.0,0.0,230525,,,A*68";
    TEST_ASSERT_FALSE(priv__validate_rmc_sentence(sentence));
}

void test_validate_empty_string(void) {
    const uint8_t *sentence = (const uint8_t *)"";
    TEST_ASSERT_FALSE(priv__validate_rmc_sentence(sentence));
}

void test_validate_truncated_after_talker(void) {
    const uint8_t *sentence = (const uint8_t *)"$GNRMC";
    TEST_ASSERT_FALSE(priv__validate_rmc_sentence(sentence));
}

void test_validate_missing_fields(void) {
    /* only has time field, no validity field */
    const uint8_t *sentence = (const uint8_t *)"$GNRMC,123456.00";
    TEST_ASSERT_FALSE(priv__validate_rmc_sentence(sentence));
}

/* ===== priv__parse_rmc tests ===== */

void test_parse_valid_rmc_northern_eastern(void) {
    /*                    time       lat       lon         spd crs date        */
    const uint8_t *sentence = (const uint8_t *)
        "$GNRMC,134523.00,A,4807.0380,N,01131.0000,E,0.0,0.0,230525,,,A*6E";

    GpsCoord result = priv__parse_rmc(sentence);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT8(13, result.hours);
    TEST_ASSERT_EQUAL_UINT8(45, result.minutes);
    TEST_ASSERT_EQUAL_UINT8(23, result.seconds);
    TEST_ASSERT_EQUAL_UINT8(0, result.latitude_sign);   /* N */
    TEST_ASSERT_EQUAL_UINT8(0, result.longitude_sign);  /* E */
    /* 48 deg 07.0380' = 48 + 7.038/60 = 48.1173 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 48.117f, result.latitude);
    /* 011 deg 31.0000' = 11 + 31.0/60 = 11.5167 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.517f, result.longitude);
    TEST_ASSERT_EQUAL_UINT8(23, result.day);
    TEST_ASSERT_EQUAL_UINT8(5, result.month);
    TEST_ASSERT_EQUAL_UINT8(25, result.year);
}

void test_parse_valid_rmc_southern_western(void) {
    const uint8_t *sentence = (const uint8_t *)
        "$GPRMC,092750.00,A,5321.6802,S,00630.3372,W,0.02,31.66,280511,,,A*43";

    GpsCoord result = priv__parse_rmc(sentence);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT8(9, result.hours);
    TEST_ASSERT_EQUAL_UINT8(27, result.minutes);
    TEST_ASSERT_EQUAL_UINT8(50, result.seconds);
    TEST_ASSERT_EQUAL_UINT8(1, result.latitude_sign);   /* S */
    TEST_ASSERT_EQUAL_UINT8(1, result.longitude_sign);  /* W */
    /* 53 deg 21.6802' = 53 + 21.6802/60 = 53.3613 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 53.361f, result.latitude);
    /* 006 deg 30.3372' = 6 + 30.3372/60 = 6.5056 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.506f, result.longitude);
    TEST_ASSERT_EQUAL_UINT8(28, result.day);
    TEST_ASSERT_EQUAL_UINT8(5, result.month);
    TEST_ASSERT_EQUAL_UINT8(11, result.year);
}

void test_parse_void_fix_returns_invalid(void) {
    const uint8_t *sentence = (const uint8_t *)
        "$GNRMC,123456.00,V,,,,,,,230525,,,N*77";

    GpsCoord result = priv__parse_rmc(sentence);
    TEST_ASSERT_FALSE(result.valid);
}

void test_parse_gga_sentence_returns_invalid(void) {
    const uint8_t *sentence = (const uint8_t *)
        "$GNGGA,123456.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47";

    GpsCoord result = priv__parse_rmc(sentence);
    TEST_ASSERT_FALSE(result.valid);
}

void test_parse_empty_returns_invalid(void) {
    const uint8_t *sentence = (const uint8_t *)"";
    GpsCoord result = priv__parse_rmc(sentence);
    TEST_ASSERT_FALSE(result.valid);
}

void test_parse_null_returns_invalid(void) {
    GpsCoord result = priv__parse_rmc(NULL);
    TEST_ASSERT_FALSE(result.valid);
}

void test_parse_truncated_before_date_returns_invalid(void) {
    /* valid through longitude sign, but missing speed/course/date */
    const uint8_t *sentence = (const uint8_t *)
        "$GNRMC,134523.00,A,4807.0380,N,01131.0000,E";

    GpsCoord result = priv__parse_rmc(sentence);
    TEST_ASSERT_FALSE(result.valid);
}

void test_parse_midnight_time(void) {
    const uint8_t *sentence = (const uint8_t *)
        "$GNRMC,000000.00,A,4807.0380,N,01131.0000,E,0.0,0.0,010125,,,A*00";

    GpsCoord result = priv__parse_rmc(sentence);
    TEST_ASSERT_TRUE(result.valid);
    TEST_ASSERT_EQUAL_UINT8(0, result.hours);
    TEST_ASSERT_EQUAL_UINT8(0, result.minutes);
    TEST_ASSERT_EQUAL_UINT8(0, result.seconds);
    TEST_ASSERT_EQUAL_UINT8(1, result.day);
    TEST_ASSERT_EQUAL_UINT8(1, result.month);
    TEST_ASSERT_EQUAL_UINT8(25, result.year);
}

void test_parse_no_subminutes_in_coords(void) {
    /* latitude/longitude with no decimal portion */
    const uint8_t *sentence = (const uint8_t *)
        "$GNRMC,120000.00,A,4807,N,01131,E,0.0,0.0,150625,,,A*00";

    GpsCoord result = priv__parse_rmc(sentence);
    TEST_ASSERT_TRUE(result.valid);
    /* 48 deg 07' = 48 + 7/60 = 48.1167 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 48.117f, result.latitude);
    /* 011 deg 31' = 11 + 31/60 = 11.5167 */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.517f, result.longitude);
}

int main(void) {
    UNITY_BEGIN();

    /* validation */
    RUN_TEST(test_validate_valid_rmc);
    RUN_TEST(test_validate_void_fix_is_invalid);
    RUN_TEST(test_validate_wrong_sentence_type);
    RUN_TEST(test_validate_missing_dollar);
    RUN_TEST(test_validate_empty_string);
    RUN_TEST(test_validate_truncated_after_talker);
    RUN_TEST(test_validate_missing_fields);

    /* parsing */
    RUN_TEST(test_parse_valid_rmc_northern_eastern);
    RUN_TEST(test_parse_valid_rmc_southern_western);
    RUN_TEST(test_parse_void_fix_returns_invalid);
    RUN_TEST(test_parse_gga_sentence_returns_invalid);
    RUN_TEST(test_parse_empty_returns_invalid);
    RUN_TEST(test_parse_null_returns_invalid);
    RUN_TEST(test_parse_truncated_before_date_returns_invalid);
    RUN_TEST(test_parse_midnight_time);
    RUN_TEST(test_parse_no_subminutes_in_coords);

    return UNITY_END();
}
