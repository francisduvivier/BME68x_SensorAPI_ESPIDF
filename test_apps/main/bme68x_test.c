/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "bme68x_i2c_esp_idf.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)


#define I2C_MASTER_SCL_IO       CONFIG_I2C_MASTER_SCL
#define I2C_MASTER_SDA_IO       CONFIG_I2C_MASTER_SDA

#define I2C_MASTER_NUM          I2C_NUM_0               /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ      100*1000                /*!< I2C master clock frequency */

#ifdef CONFIG_BME68X_SDO_PULLED_DOWN
#define BME68X_I2C_ADDR        BME68X_I2C_ADDR_LOW
#else
#define BME68X_I2C_ADDR        BME68X_I2C_ADDR_HIGH
#endif

static bme68x_handle_t bme68x_handle = NULL;
static i2c_bus_handle_t i2c_bus;

/**
 * @brief i2c master initialization
 */
static void i2c_sensor_bme68x_init(void)
{
    const i2c_config_t i2c_bus_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };

    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &i2c_bus_conf);

    TEST_ASSERT_NOT_NULL_MESSAGE(i2c_bus, "i2c_bus create returned NULL");

    bme68x_i2c_config_t i2c_bme68x_conf = {
        .i2c_handle = i2c_bus,
        .i2c_addr = BME68X_I2C_ADDR,
    };
    bme68x_sensor_create(&i2c_bme68x_conf, &bme68x_handle);
    TEST_ASSERT_NOT_NULL_MESSAGE(bme68x_handle, "BME68X create returned NULL");
}

int8_t run_self_test(bme68x_handle_t bme)
{
    int8_t rslt = bme68x_selftest_check(bme);

    if (rslt != BME68X_OK) {
        ESP_LOGE("BME68X", "Self-test failed with error code: %d", rslt);
    } else {
        ESP_LOGI("BME68X", "Self-test passed successfully");
    }
    return rslt;
}

TEST_CASE("BME68x self_test", "[BME68x][self_test]")
{
    printf("START: TEST_CASE BME68x self_test\n");

    esp_err_t ret = ESP_OK;

    printf("START: i2c_sensor_bme68x_init\n");
    i2c_sensor_bme68x_init();
    printf("DONE: i2c_sensor_bme68x_init\n");

    printf("START: bme68x_selftest_check\n");
    int8_t rslt = bme68x_selftest_check(bme68x_handle);
    printf("DONE: bme68x_selftest_check\n");

    if (rslt != BME68X_OK) {
        ESP_LOGE("BME68X", "Self-test failed with error code: %d", rslt);
    } else {
        ESP_LOGI("BME68X", "Self-test passed successfully");
    }
    bme68x_check_rslt("after_run_self_test", rslt);
    TEST_ASSERT_EQUAL(BME68X_OK, rslt);

    bme68x_sensor_del(bme68x_handle);
    ret = bme68x_i2c_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    printf("DONE: TEST_CASE BME68x self_test\n");

}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    printf("START: check_leak\n");
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
    printf("DONE: check_leak\n");
}

void setUp(void)
{
    printf("START: setUp\n");
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    printf("DONE: setUp\n");
}

void tearDown(void)
{
    printf("START: tearDown\n");
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
    printf("DONE: tearDown\n");
}

void app_main(void)
{
    printf("BME68X TEST \n");
    printf("BME68X_I2C_ADDR is defined with value: 0x%X\n", BME68X_I2C_ADDR);
    printf("I2C_MASTER_SDA_IO is defined with value: %d\n", I2C_MASTER_SDA_IO);
    printf("I2C_MASTER_SCL_IO is defined with value: %d\n", I2C_MASTER_SCL_IO);

    unity_run_menu();
}
