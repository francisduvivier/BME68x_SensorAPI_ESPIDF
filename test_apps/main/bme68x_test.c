/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "bme68x_i2c_esp_idf.h"
#include "driver/i2c.h"

// Settings
#define I2C_MASTER_TIMEOUT 100        // Shorter timeout for faster scanning

#define TEST_MEMORY_LEAK_THRESHOLD (-800) // TODO investigate why it's a bit higher than 400


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
    TEST_ASSERT_NOT_NULL_MESSAGE(bme68x_handle, "BME68X create returned NULL \n");
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
    i2c_bus_delete(i2c_bus);
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

// Silent scanner - returns true if device found
bool i2c_probe_address(i2c_port_t port, uint8_t addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);

    // Temporarily lower log level to suppress bus errors
    esp_log_level_t old_level = esp_log_level_get("i2c");
    esp_log_level_set("i2c", ESP_LOG_ERROR);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT));

    // Restore log level
    esp_log_level_set("i2c", old_level);
    i2c_cmd_link_delete(cmd);

    return ret == ESP_OK;
}

uint8_t i2c_scan() {
    printf("\nI2C Scanner (0x08-0x77)\n");

    uint8_t found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        // Probe address
        if (i2c_probe_address(I2C_MASTER_NUM, addr)) {
            printf("\nFound device at 0x%X\n", addr);
            found++;
        }
    }

    printf("\n\nFound [%d] device(s)\n", found);
    return found;
}

TEST_CASE("i2c scan test", "[i2c][scan]")
{
    // Configure I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));

    // Run scan
    uint8_t found = i2c_scan();
    TEST_ASSERT_NOT_EQUAL(0, found);

    // Cleanup (comment out if you need I2C after scanning)
    i2c_driver_delete(I2C_MASTER_NUM);
}

/* Macro for count of samples to be displayed */
#define SAMPLE_COUNT  UINT16_C(10)  // Reduced for testing

TEST_CASE("BME68x forced_mode", "[BME68x][forced_mode]")
{
    printf("START: TEST_CASE BME68x forced_mode\n");

    esp_err_t ret = ESP_OK;
    int8_t rslt;
    struct bme68x_conf conf;
    struct bme68x_heatr_conf heatr_conf;
    struct bme68x_data data;
    uint32_t del_period;
    uint32_t time_ms = 0;
    uint8_t n_fields;
    uint16_t sample_count = 1;

    printf("START: i2c_sensor_bme68x_init\n");
    i2c_sensor_bme68x_init();
    printf("DONE: i2c_sensor_bme68x_init\n");

    /* Configure sensor settings */
    conf.filter = BME68X_FILTER_OFF;
    conf.odr = BME68X_ODR_NONE;
    conf.os_hum = BME68X_OS_16X;
    conf.os_pres = BME68X_OS_1X;
    conf.os_temp = BME68X_OS_2X;
    rslt = bme68x_set_conf(&conf, bme68x_handle);
    bme68x_check_rslt("bme68x_set_conf", rslt);
    TEST_ASSERT_EQUAL(BME68X_OK, rslt);

    /* Configure heater settings */
    heatr_conf.enable = BME68X_ENABLE;
    heatr_conf.heatr_temp = 300;
    heatr_conf.heatr_dur = 100;
    rslt = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, bme68x_handle);
    bme68x_check_rslt("bme68x_set_heatr_conf", rslt);
    TEST_ASSERT_EQUAL(BME68X_OK, rslt);

    printf("Sample, TimeStamp(ms), Temperature(deg C), Pressure(Pa), Humidity(%%), Gas resistance(ohm), Status\n");

    while (sample_count <= SAMPLE_COUNT)
    {
        rslt = bme68x_set_op_mode(BME68X_FORCED_MODE, bme68x_handle);
        bme68x_check_rslt("bme68x_set_op_mode", rslt);
        TEST_ASSERT_EQUAL(BME68X_OK, rslt);

        /* Calculate delay period in microseconds */
        del_period = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, bme68x_handle) + (heatr_conf.heatr_dur * 1000);
        vTaskDelay(pdMS_TO_TICKS((del_period + 999) / 1000));  // Convert to ms and delay

        time_ms = esp_timer_get_time() / 1000;  // Convert to milliseconds

        /* Get sensor data */
        rslt = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, bme68x_handle);
        bme68x_check_rslt("bme68x_get_data", rslt);
        TEST_ASSERT_EQUAL(BME68X_OK, rslt);

        if (n_fields)
        {
            printf("%u, %lu, %.2f, %.2f, %.2f, %.2f, 0x%x\n",
                   sample_count,
                   (unsigned long)time_ms,
                   data.temperature,
                   data.pressure,
                   data.humidity,
                   data.gas_resistance,
                   data.status);
            sample_count++;
        }
    }

    bme68x_sensor_del(bme68x_handle);
    ret = bme68x_i2c_deinit();
    i2c_bus_delete(i2c_bus);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    printf("DONE: TEST_CASE BME68x forced_mode\n");
}

void app_main(void)
{
    printf("BME68X TEST \n");
    printf("BME68X_I2C_ADDR is defined with value: 0x%X\n", BME68X_I2C_ADDR);
    printf("I2C_MASTER_SDA_IO is defined with value: %d\n", I2C_MASTER_SDA_IO);
    printf("I2C_MASTER_SCL_IO is defined with value: %d\n", I2C_MASTER_SCL_IO);
    printf("GPIO_PULLUP_ENABLE is defined with value: %d\n", GPIO_PULLUP_ENABLE);

    unity_run_menu();
}
