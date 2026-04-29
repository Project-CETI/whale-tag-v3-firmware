#include <unity.h>

#include "util/quaternion.h"

DEFINE_QUATERION_TYPE(float)
DEFINE_EULER_TYPE(float)
IMPLEMENT_QUATERION_TYPE(float)

void test_quaternion_remove_yaw(void) {

}

void test_quaternion_multiplication(void) {
    Quaternion(float) q_real = {.real = 1.0f};
    Quaternion(float) q_i = {.i = 1.0f};
    Quaternion(float) q_j = {.j = 1.0f};
    Quaternion(float) q_k = {.k = 1.0f};
    Quaternion(float) q_expect;
    Quaternion(float) q_result;

    // quaternion multiplication logic table
    // real * real = real
    q_expect = q_real;
    q_result = quaternion_multiplication(float)(q_real, q_real);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // i * i = -real
    q_expect =  (Quaternion(float)){.real = -1.0f};
    q_result = quaternion_multiplication(float)(q_i, q_i);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // j * j = -real
    q_expect =  (Quaternion(float)){.real = -1.0f};
    q_result = quaternion_multiplication(float)(q_j, q_j);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));    
    
    // k * k = -real
    q_expect =  (Quaternion(float)){.real = -1.0f};
    q_result = quaternion_multiplication(float)(q_k, q_k);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // real * i = i
    q_expect = q_i;
    q_result = quaternion_multiplication(float)(q_real, q_i);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // i * real = i
    q_expect =  q_i;
    q_result = quaternion_multiplication(float)(q_i, q_real);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // j * k = i
    q_expect =  q_i;
    q_result = quaternion_multiplication(float)(q_j, q_k);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));    
    
    // k * j = -i
    q_expect =  (Quaternion(float)){.i = -1.0f};
    q_result = quaternion_multiplication(float)(q_k, q_j);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // real * j = j
    q_expect = q_j;
    q_result = quaternion_multiplication(float)(q_real, q_j);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // i * k = -j
    q_expect = (Quaternion(float)){.j = -1.0f};
    q_result = quaternion_multiplication(float)(q_i, q_k);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // j * real = j
    q_expect =  q_j;
    q_result = quaternion_multiplication(float)(q_j, q_real);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));    
    
    // k * i = j
    q_expect = q_j;
    q_result = quaternion_multiplication(float)(q_k, q_i);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // real * k = k
    q_expect = q_k;
    q_result = quaternion_multiplication(float)(q_real, q_k);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // i * j = k
    q_expect = q_k;
    q_result = quaternion_multiplication(float)(q_i, q_j);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // j * i = -k
    q_expect =  (Quaternion(float)){.k = -1.0f};
    q_result = quaternion_multiplication(float)(q_j, q_i);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));    
    
    // k * real = k
    q_expect =  q_k;
    q_result = quaternion_multiplication(float)(q_k, q_real);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    // zero identity
    Quaternion(float) q_zero = {};
    Quaternion(float) q_any = {.real = 1.5f, .i = -0.5f, .j = 0.5f, .k = -0.5f};
    q_result = quaternion_multiplication(float)(q_any, q_zero);
    TEST_ASSERT_EQUAL_MEMORY(&q_zero, &q_result, sizeof(Quaternion(float)));

}

void test_quaternion_addition(void) {
    Quaternion(float) q1 = {};
    Quaternion(float) q2 = {.real = 0.5f, .i = -0.5f, .j = 0.5f, .k = -0.5f};
    Quaternion(float) q3 = {.real = 1.0f};

    //q2 - q1
    Quaternion(float) q_expect = q2;
    Quaternion(float) q_result = quaternion_addition(float)(q2, q1);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));
    
    //q1 - q2
    q_expect = q2;
    q_result = quaternion_addition(float)(q1, q2);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    //q3 - q2
    q_expect = (Quaternion(float)){.real = 1.5f, .i = -0.5f, .j = 0.5f, .k = -0.5f};
    q_result = quaternion_addition(float)(q3, q2);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));
}

void test_quaternion_subtraction(void) {
    Quaternion(float) q1 = {};
    Quaternion(float) q2 = {.real = 0.5f, .i = -0.5f, .j = 0.5f, .k = -0.5f};
    Quaternion(float) q3 = {.real = 1.0f};

    //q2 - q1
    Quaternion(float) q_expect = q2;
    Quaternion(float) q_result = quaternion_subtraction(float)(q2, q1);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));
    
    //q1 - q2
    q_expect = (Quaternion(float)){.real = -0.5f, .i = 0.5f, .j = -0.5f, .k = 0.5f};
    q_result = quaternion_subtraction(float)(q1, q2);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));

    //q3 - q2
    q_expect = (Quaternion(float)){.real = 0.5f, .i = 0.5f, .j = -0.5f, .k = 0.5f};
    q_result = quaternion_subtraction(float)(q3, q2);
    TEST_ASSERT_EQUAL_MEMORY(&q_expect, &q_result, sizeof(Quaternion(float)));
}

void test_quaternion_magnitude(void) {
    TEST_ASSERT_EQUAL_FLOAT(1.0f, quaternion_magnitude(float)( (Quaternion(float)){.real = 0.5f, .i = 0.5f, .j = 0.5f, .k = 0.5f}));
    TEST_ASSERT_EQUAL_FLOAT(1.0f/sqrt(2.0f), quaternion_magnitude(float)( (Quaternion(float)){.real = 0.5f, .i = 0.0f, .j = -0.5f, .k = 0.0f}));
    TEST_ASSERT_EQUAL_FLOAT(2.0f, quaternion_magnitude(float)( (Quaternion(float)){.real = -1.0f, .i = 1.0f, .j = 1.0f, .k = -1.0f}));
    TEST_ASSERT_EQUAL_FLOAT(3.0f, quaternion_magnitude(float)( (Quaternion(float)){.k = 3.0f}));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, quaternion_magnitude(float)( (Quaternion(float)){}));
}




void setUp(void) {
}

void tearDown(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_quaternion_multiplication);
    RUN_TEST(test_quaternion_addition);
    RUN_TEST(test_quaternion_subtraction);
    RUN_TEST(test_quaternion_magnitude);
    return UNITY_END();
}
