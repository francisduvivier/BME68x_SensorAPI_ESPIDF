// TODO MOVE THIS FILE to top level directory
/**
 * Copyright (C) 2023 Bosch Sensortec GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "bme68x_i2c_helper.h"
#include "esp_log.h"

/******************************************************************************/
static i2c_bus_device_handle_t intf_conf;

/******************************************************************************/
/*!                User interface functions                                   */

/*!
 * I2C read function map to COINES platform
 */
BME68X_INTF_RET_TYPE bme68x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    i2c_bus_device_handle_t intf_info = (i2c_bus_device_handle_t)intf_ptr;
    return i2c_bus_read_bytes(intf_info, reg_addr, (uint16_t)len, reg_data);
}

/*!
 * I2C write function map to COINES platform
 */
BME68X_INTF_RET_TYPE bme68x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    i2c_bus_device_handle_t intf_info = (i2c_bus_device_handle_t)intf_ptr;
    return i2c_bus_write_bytes(intf_info, reg_addr, len, reg_data);
}

/*!
 * SPI read function map to COINES platform
 */
BME68X_INTF_RET_TYPE bme68x_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    return 0; // TODO
}

/*!
 * SPI write function map to COINES platform
 */
BME68X_INTF_RET_TYPE bme68x_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    return 0; // TODO
}

/*!
 * Delay function map to COINES platform
 */
void bme68x_delay_us(uint32_t period, void *intf_ptr)
{
    vTaskDelay(period / 1000 / portTICK_PERIOD_MS);
}

void bme68x_check_rslt(const char api_name[], int8_t rslt)
{
    switch (rslt)
    {
        case BME68X_OK:

            /* Do nothing */
            break;
        case BME68X_E_NULL_PTR:
            printf("API name [%s]  Error [%d] : Null pointer\r\n", api_name, rslt);
            break;
        case BME68X_E_COM_FAIL:
            printf("API name [%s]  Error [%d] : Communication failure\r\n", api_name, rslt);
            break;
        case BME68X_E_INVALID_LENGTH:
            printf("API name [%s]  Error [%d] : Incorrect length parameter\r\n", api_name, rslt);
            break;
        case BME68X_E_DEV_NOT_FOUND:
            printf("API name [%s]  Error [%d] : Device not found\r\n", api_name, rslt);
            break;
        case BME68X_E_SELF_TEST:
            printf("API name [%s]  Error [%d] : Self test error\r\n", api_name, rslt);
            break;
        case BME68X_W_NO_NEW_DATA:
            printf("API name [%s]  Warning [%d] : No new data found\r\n", api_name, rslt);
            break;
        default:
            printf("API name [%s]  Error [%d] : Unknown error code\r\n", api_name, rslt);
            break;
    }
}

/**
 * @brief Function to select the interface between SPI and I2C for BME68x.
 * @param[in] bme      : Structure instance of bme68x_dev
 * @param[in] intf     : Interface selection parameter (BME68X_I2C_INTF or BME68X_SPI_INTF)
 * @param[in] dev_addr : Device address (I2C address or SPI CS pin)
 * @param[in] bus_inst : I2C bus handle (for I2C)
 * @return Status of execution
 * @retval 0 -> Success
 * @retval < 0 -> Failure Info
 */
int8_t bme68x_interface_init(struct bme68x_dev *bme, uint8_t intf, uint8_t dev_addr, i2c_bus_handle_t bus_inst)
{
    int8_t rslt = BME68X_OK;

    if (bme != NULL)
    {
        if (intf == BME68X_I2C_INTF)
        {
            bme->intf = BME68X_I2C_INTF;
            bme->read = bme68x_i2c_read;
            bme->write = bme68x_i2c_write;

            i2c_bus_device_handle_t i2c_device_handle = i2c_bus_device_create(bus_inst, dev_addr, 0);
            if (NULL == i2c_device_handle)
            {
                ESP_LOGE("BME68X", "i2c_bus_device_create failed");
                rslt = BME68X_E_NULL_PTR;
            }
            intf_conf = i2c_device_handle;
            bme->intf_ptr = (void *)intf_conf;
        }
        else if (intf == BME68X_SPI_INTF)
        {
            bme->intf = BME68X_SPI_INTF;
            bme->read = bme68x_spi_read;
            bme->write = bme68x_spi_write;
            ESP_LOGE("BME68X", "SPI Interface not supported yet");
            rslt = BME68X_E_COM_FAIL;
        }
        else
        {
            ESP_LOGE("BME68X", "Unknown interface type");
            rslt = BME68X_E_COM_FAIL;
        }

        bme->delay_us = bme68x_delay_us;
        bme->amb_temp = 25;
    }
    else
    {
        rslt = BME68X_E_NULL_PTR;
    }

    return rslt;
}

esp_err_t bme68x_i2c_deinit(void)
{
    ESP_LOGI("BME68X", "bme68x_i2c_deinit");
    return i2c_bus_device_delete(&intf_conf);
}
