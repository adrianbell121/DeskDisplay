#include "middle_sensor.h"
#include "qmi8658.h"
#include "shtc3.h"

static const uint32_t SHTC3_I2C_SPEED_HZ = 40000;
static const uint32_t SHTC3_I2C_SCL_WAIT_US = 10000;

static const char *TAG = "middle";
static bool qmi_inited = false;
static bool shtc3_inited = false;
static qmi8658_dev_t qmi_dev;
static i2c_master_dev_handle_t shtc3_dev = NULL;


esp_err_t middle_init_shtc3()
{
    if (shtc3_inited) return ESP_OK;

    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "bsp_i2c_init failed");
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_I2C_ADDR,
        .scl_speed_hz = SHTC3_I2C_SPEED_HZ,
        .scl_wait_us = SHTC3_I2C_SCL_WAIT_US,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &shtc3_dev), TAG, "i2c_master_bus_add_device failed");

    shtc3_inited = true;
    return ESP_OK;
}


esp_err_t middle_init_qmi8658(void)
{
    if (qmi_inited) return ESP_OK;

    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "bsp_i2c_init failed");
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_ERR_INVALID_STATE;

    ESP_RETURN_ON_ERROR(qmi8658_init(&qmi_dev, bus, QMI8658_ADDRESS_HIGH), TAG, "qmi8658_init failed");

    qmi8658_set_accel_unit_mps2(&qmi_dev, true);
    qmi8658_set_gyro_unit_rads(&qmi_dev, true);
    qmi8658_set_display_precision(&qmi_dev, 4);
    ESP_RETURN_ON_ERROR(qmi8658_enable_sensors(&qmi_dev, QMI8658_ENABLE_ACCEL | QMI8658_ENABLE_GYRO),
                        TAG, "qmi8658_enable_sensors failed");

    qmi_inited = true;
    return ESP_OK;
}

esp_err_t middle_read_shtc3(float *temp_c, float *hum_rh)
{
    if (!temp_c || !hum_rh) return ESP_ERR_INVALID_ARG;
    if (!shtc3_inited || !shtc3_dev) return ESP_ERR_INVALID_STATE;
    return shtc3_get_th(shtc3_dev, SHTC3_REG_T_CSE_NM, temp_c, hum_rh);
}

esp_err_t middle_read_qmi(float *ax, float *ay, float *az, float *gx, float *gy, float *gz)
{
    if (!ax || !ay || !az || !gx || !gy || !gz) return ESP_ERR_INVALID_ARG;
    if (!qmi_inited) return ESP_ERR_INVALID_STATE;

    qmi8658_data_t data = {0};
    esp_err_t ret = qmi8658_read_sensor_data(&qmi_dev, &data);
    if (ret != ESP_OK) return ret;

    *ax = data.accelX;
    *ay = data.accelY;
    *az = data.accelZ;
    *gx = data.gyroX;
    *gy = data.gyroY;
    *gz = data.gyroZ;
    return ESP_OK;
}
