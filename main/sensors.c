#include "hdr.h"
#ifdef SET_I2C

#include "mqtt.h"

uint8_t i2c_prn = 0;
const char *TAGI = "I2C";

#ifdef SET_BMP
    bmx280_calib_t calib;
#endif

#if defined(SET_BMP) || defined(SET_BH1750) || defined(SET_SI7021)
    xSemaphoreHandle sensors_mutex;
    result_t sensors = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
#endif

#ifdef SET_APDS
    gesture_data_type gesture_data_;
    int gesture_ud_delta_;
    int gesture_lr_delta_;
    int gesture_ud_count_;
    int gesture_lr_count_;
    int gesture_near_count_;
    int gesture_far_count_;
    int gesture_state_;
    int gesture_motion_;

    static uint32_t apds_count = 0;
    atomic_bool apds_flag;

    void IRAM_ATTR apds_isr_handler(void *arg)
    {
        apds_count++;
        atomic_store(&apds_flag, 1);
    }
#endif

//******************************************************************************************

#ifdef SET_BMP
esp_err_t i2c_master_read_sensor(uint8_t reg, uint8_t *data_rd, size_t size)
{
    if (!size) return ESP_OK;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BMP280_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( BMP280_ADDR << 1 ) | READ_BIT, ACK_CHECK_EN);
    if (size > 1) i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 2000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;
}
//-----------------------------------------------------------------------------
esp_err_t i2c_master_reset_sensor(uint8_t *chip_id)
{

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BMP280_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BMP280_REG_RESET, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BMP280_RESET_VALUE, ACK_CHECK_EN);//reset value = 0xB6
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ret;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BMP280_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BMP280_REG_ID, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BMP280_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, chip_id, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    return ret;

}
//-----------------------------------------------------------------------------
esp_err_t i2c_master_test_sensor(uint8_t *stat, uint8_t *mode, uint8_t *conf, uint8_t chip_id)
{

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BMP280_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BMP280_REG_CTRL, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BMP280_OSRS_T | BMP280_OSRS_P | BMP280_FORCED1_MODE, ACK_CHECK_EN);//force mode (1 or 2), 3- normal

    i2c_master_write_byte(cmd, BMP280_REG_CONFIG, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BMP280_CONF_T_SB | BMP280_CONF_FILTER | BMP280_CONF_SPI3W, ACK_CHECK_EN);// bits: 7-5 t_sb; 4-2 filter; 0 spi3w_en */

    if (chip_id == BME280_SENSOR) {
    	i2c_master_write_byte(cmd, BME280_REG_CTRL_HUM, ACK_CHECK_EN);
    	i2c_master_write_byte(cmd, BME280_OSRS_H, ACK_CHECK_EN);//bits: 2-0 osrs_h;
    }
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ESP_FAIL;

    vTaskDelay(30 / portTICK_RATE_MS);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BMP280_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BMP280_REG_STATUS, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BMP280_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, stat, ACK_VAL);
    i2c_master_read_byte(cmd, mode, ACK_VAL);
    i2c_master_read_byte(cmd, conf, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ESP_FAIL;

    return ESP_OK;
}
//-----------------------------------------------------------------------------
esp_err_t readCalibrationData(uint8_t chip_id)
{
esp_err_t err = ESP_FAIL;
uint8_t data[24] = {0};

    if (i2c_master_read_sensor(BMP280_REG_CALIB, &data[0], 24) == ESP_OK) {
	memset(&calib, 0, sizeof(bmx280_calib_t));
	calib.dig_T1 = (data[1] << 8) | data[0];
	calib.dig_T2 = (data[3] << 8) | data[2];
	calib.dig_T3 = (data[5] << 8) | data[4];
	calib.dig_P1 = (data[7] << 8) | data[6];
	calib.dig_P2 = (data[9] << 8) | data[8];
	calib.dig_P3 = (data[11] << 8) | data[10];
	calib.dig_P4 = (data[13] << 8) | data[12];
	calib.dig_P5 = (data[15] << 8) | data[14];
	calib.dig_P6 = (data[17] << 8) | data[16];
	calib.dig_P7 = (data[19] << 8) | data[18];
	calib.dig_P8 = (data[21] << 8) | data[20];
	calib.dig_P9 = (data[23] << 8) | data[22];

	if (chip_id == BME280_SENSOR) {//humidity
		// Read section 0xA1
		if (i2c_master_read_sensor(0xA1, &data[0], 1) != ESP_OK) goto outm;
		calib.dig_H1 = data[0];
		// Read section 0xE1
		if (i2c_master_read_sensor(0xE1, &data[0], 7) != ESP_OK) goto outm;
		calib.dig_H2 = (data[1] << 8) | data[0];
		calib.dig_H3 = data[2];
		calib.dig_H4 = (data[3] << 4) | (0x0f & data[4]);
		calib.dig_H5 = (data[5] << 4) | ((data[4] >> 4) & 0x0F);
		calib.dig_H6 = data[6];
	}
	err = ESP_OK;
    }

outm:

    return err;
}
//-----------------------------------------------------------------------------
void CalcAll(int32_t chip_id, int32_t tp, int32_t pp, int32_t hh)
{
double var1, var2, p, var_H;
double t1, p1, h1 = 0.0;

    //Temp // Returns temperature in DegC, double precision. Output value of “51.23” equals 51.23 DegC.
    var1 = (((double) tp) / 16384.0 - ((double)calib.dig_T1)/1024.0) * ((double)calib.dig_T2);
    var2 = ((((double) tp) / 131072.0 - ((double)calib.dig_T1)/8192.0) * (((double)tp)/131072.0 - ((double) calib.dig_T1)/8192.0)) * ((double)calib.dig_T3);
    // t_fine carries fine temperature as global value
    int32_t t_fine = (int32_t)(var1 + var2);
    t1 = (var1 + var2) / 5120.0;

    //Press // Returns pressure in Pa as double. Output value of “96386.2” equals 96386.2 Pa = 963.862 hPa
    var1 = ((double)t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * ((double) calib.dig_P6) / 32768.0;
    var2 = var2 + var1 * ((double) calib.dig_P5) * 2.0;
    var2 = (var2 / 4.0) + (((double) calib.dig_P4) * 65536.0);
    var1 = (((double) calib.dig_P3) * var1 * var1 / 524288.0 + ((double) calib.dig_P2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * ((double) calib.dig_P1);
    if (var1 == 0.0) p = 0;
    else {
	p = 1048576.0 - (double)pp;
	p = (p - (var2 / 4096.0)) * 6250.0 / var1;
	var1 = ((double) calib.dig_P9) * p * p / 2147483648.0;
	var2 = p * ((double) calib.dig_P8) / 32768.0;
	p = p + (var1 + var2 + ((double) calib.dig_P7)) / 16.0;
    }
    p1 = (p/100) * 0.75006375541921;//convert hPa to mmHg

    if (chip_id == BME280_SENSOR) {// Returns humidity in %rH as as double. Output value of “46.332” represents 46.332 %rH
	var_H = (((double)t_fine) - 76800.0);
	var_H = (hh - (((double)calib.dig_H4) * 64.0 + ((double)calib.dig_H5) / 16384.0 * var_H)) *
		(((double)calib.dig_H2) / 65536.0 * (1.0 + ((double)calib.dig_H6) / 67108864.0 * var_H *
			(1.0 + ((double)calib.dig_H3) / 67108864.0 * var_H)));
	var_H = var_H * (1.0 - ((double)calib.dig_H1) * var_H / 524288.0);
	if (var_H > 100.0) var_H = 100.0;
	else 
	if (var_H < 0.0) var_H = 0.0;
        h1 = var_H;
    }

    if (xSemaphoreTake(sensors_mutex, portMAX_DELAY) == pdTRUE) {
	sensors.temp = t1;
	sensors.pres = p1;
	sensors.humi = h1;
	xSemaphoreGive(sensors_mutex);
    }

}
#endif
//-----------------------------------------------------------------------------
#ifdef SET_BH1750
esp_err_t bh1750_off()
{
    //POWER_DOWN
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BH1750_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BH1750_POWER_DOWN, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}
//-----------------------------------------------------------------------------
esp_err_t bh1750_proc(uint16_t *lux)
{
    //POWER_ON
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BH1750_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BH1750_POWER_ON, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ret;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, BH1750_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, BH1750_CON_HRES_MODE, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ret;

    vTaskDelay(200 / portTICK_RATE_MS);

    uint16_t tmp;
    uint8_t st, ml;
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BH1750_ADDR << 1) | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &st, ACK_VAL);
    i2c_master_read_byte(cmd, &ml, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ret;
    tmp = st; tmp <<= 8; tmp |= ml;
    *lux = tmp;

    return ESP_OK;
}
//-----------------------------------------------------------------------------
#endif

//*****************************************************************************
#ifdef SET_SI7021
//-----------------------------------------------------------------------------
esp_err_t i2c_7021_reset(i2c_port_t i2c_num)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SI7021_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd,  0xFE, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    vTaskDelay(50 / portTICK_RATE_MS);

    return ret;
}
//-----------------------------------------------------------------------------
esp_err_t i2c_7021_read_raw_rh(i2c_port_t i2c_num, uint8_t *data_h, uint8_t *data_l)
{
//uint8_t try = 0;
uint8_t csum = 0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SI7021_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
//    i2c_master_write_byte(cmd,  0xE5, ACK_CHECK_EN);
    i2c_master_write_byte(cmd,  0xF5, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ret;

    vTaskDelay(50 / portTICK_RATE_MS);

    //ret = ESP_FAIL;
    //while ((ret != ESP_OK) && (try++ < 15) ) {
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, SI7021_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
        i2c_master_read_byte(cmd, data_h, ACK_VAL);
        i2c_master_read_byte(cmd, data_l, ACK_VAL);
        i2c_master_read_byte(cmd, &csum, NACK_VAL);//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
    //}

//printf("%s: times_tried = %u\n", __func__, times_tried);

    return ret;
}
//----------------------------------------------------------------------------------
esp_err_t i2c_7021_read_raw_temp(i2c_port_t i2c_num, uint8_t *data_h, uint8_t *data_l)
{
//uint8_t try = 0;
uint8_t csum = 0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SI7021_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
//    i2c_master_write_byte(cmd,  0xE3, ACK_CHECK_EN);
    i2c_master_write_byte(cmd,  0xF3, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ret;

    vTaskDelay(50 / portTICK_RATE_MS);

    //ret = ESP_FAIL;
    //while ((ret != ESP_OK) && (try++ < 15) ) {
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, SI7021_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
        i2c_master_read_byte(cmd, data_h, ACK_VAL);
        i2c_master_read_byte(cmd, data_l, ACK_VAL);
        i2c_master_read_byte(cmd, &csum, NACK_VAL);//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);
    //}

//printf("%s: times_tried = %u\n", __func__, times_tried);


    return ret;
}
//----------------------------------------------------------------------------------
float i2c_7021_read_rh()
{
uint8_t dataH, dataL;

   if (i2c_7021_read_raw_rh(I2C_MASTER_NUM, &dataH, &dataL) != ESP_OK) {
       ESP_LOGE(TAGI, "SI7021: Failed reading humidity");
       return 0.0;
   }

   float val = dataH * 256 + dataL;
   return ( ((val * 125.0)/65536.0) - 6.0 );

}
//----------------------------------------------------------------------------------
float i2c_7021_read_temp()
{
uint8_t dataH, dataL;

   if (i2c_7021_read_raw_temp(I2C_MASTER_NUM, &dataH, &dataL) != ESP_OK) {
       ESP_LOGE(TAGI, "SI7021: Failed reading temp");
       return 0.0;
   }

   float val = dataH * 256 + dataL;
   return ( ((val * 175.72) / 65536.0) - 46.85 );

}
#endif
//*****************************************************************************

#ifdef SET_APDS
//-----------------------------------------------------------------------------
esp_err_t wireReadDataByte(uint8_t reg, uint8_t *val)
{

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, APDS9960_I2C_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, APDS9960_I2C_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, val, NACK_VAL);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ESP_FAIL;

    return ESP_OK;
}
//-----------------------------------------------------------------------------
esp_err_t wireWriteDataByte(uint8_t reg, uint8_t val)
{

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, APDS9960_I2C_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, val, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) return ESP_FAIL;

    return ESP_OK;
}
//-----------------------------------------------------------------------------
int wireReadDataBlock (uint8_t reg, uint8_t *vals, unsigned int len)
{
int ret = 0;

    if (!len) return ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, APDS9960_I2C_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, APDS9960_I2C_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    if (len > 1) i2c_master_read(cmd, vals, len - 1, ACK_VAL);
    i2c_master_read_byte(cmd, vals + len - 1, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 2000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_OK) ret = len;

    return ret;
}
//-----------------------------------------------------------------------------
//Reads and returns the contents of the ENABLE register
esp_err_t getMode(uint8_t *eval)
{
esp_err_t rt = ESP_FAIL;
uint8_t ev;

    rt = wireReadDataByte(APDS9960_ENABLE, &ev);
    if (rt == ESP_OK) *eval = ev;

    return rt;
}
//-----------------------------------------------------------------------------
//Enables or disables a feature in the APDS-9960
esp_err_t setMode(uint8_t mode, uint8_t enable)
{
uint8_t reg_val = ERROR;

    // Read current ENABLE register
    if (getMode(&reg_val) != ESP_OK) return ESP_FAIL;

    // Change bit(s) in ENABLE register
    enable = enable & 0x01;
    if (mode <= 6) {
        if (enable) {
            reg_val |= (1 << mode);
        } else {
            reg_val &= ~(1 << mode);
        }
    } else if (mode == ALL) {
        if (enable) {
            reg_val = 0x7F;
        } else {
            reg_val = 0x00;
        }
    }

    // Write value back to ENABLE register
    return (wireWriteDataByte(APDS9960_ENABLE, reg_val));
}
//-----------------------------------------------------------------------------
esp_err_t setCtrlDrive(uint8_t drive, uint8_t who)
{
uint8_t val;
esp_err_t rt = ESP_FAIL;

    // Read value from CONTROL register
    if (wireReadDataByte(APDS9960_CONTROL, &val) == ESP_OK) {
	drive &= 3;//0b00000011;
	// Set bits in register to given value
	switch (who) {
		case 0://setLEDDrive - Sets the LED drive strength for proximity and ambient light sensor (ALS)
			drive <<= 6;
			val &= 0x3F;//0b00111111;
			val |= drive;
		break;
		case 1://setProximityGain - Sets the receiver gain for proximity detection
			drive <<= 2;
			val &= 0xF3;//0b11110011;
			val |= drive;
		break;
		case 2://setAmbientLightGain - Sets the receiver gain for the ambient light sensor (ALS)
			val &= 0xFC;//0b11111100;
			val |= drive;
		break;
	}
	// Write register value back into CONTROL register
	rt = wireWriteDataByte(APDS9960_CONTROL, val);
    }

    return rt;
}
//-----------------------------------------------------------------------------
//Sets the lower threshold for proximity detection
esp_err_t setProxIntLowThresh(uint8_t threshold)
{
    return (wireWriteDataByte(APDS9960_PILT, threshold));
}
//-----------------------------------------------------------------------------
//Sets the high threshold for proximity detection
esp_err_t setProxIntHighThresh(uint8_t threshold)
{
    return (wireWriteDataByte(APDS9960_PIHT, threshold));
}
//-----------------------------------------------------------------------------
//Sets the low threshold for ambient light interrupts
esp_err_t setLightIntLowThreshold(uint16_t threshold)
{
uint8_t val_low, val_high;

    // Break 16-bit threshold into 2 8-bit values
    val_low = threshold & 0x00FF;
    val_high = (threshold & 0xFF00) >> 8;

    // Write low byte
    if (wireWriteDataByte(APDS9960_AILTL, val_low) == ESP_OK) {
        // Write high byte
        return (wireWriteDataByte(APDS9960_AILTH, val_high));
    } else return ESP_FAIL;
}
//-----------------------------------------------------------------------------
//Sets the high threshold for ambient light interrupts
esp_err_t setLightIntHighThreshold(uint16_t threshold)
{
uint8_t val_low, val_high;

    // Break 16-bit threshold into 2 8-bit values
    val_low = threshold & 0x00FF;
    val_high = (threshold & 0xFF00) >> 8;

    // Write low byte
    if (wireWriteDataByte(APDS9960_AIHTL, val_low) == ESP_OK) {
        // Write high byte
        return (wireWriteDataByte(APDS9960_AIHTH, val_high));
    } else return ESP_FAIL;

}
//-----------------------------------------------------------------------------
//Sets the entry proximity threshold for gesture sensing
esp_err_t setGestureEnterThresh(uint8_t threshold)
{
    return (wireWriteDataByte(APDS9960_GPENTH, threshold));
}
//-----------------------------------------------------------------------------
//Sets the exit proximity threshold for gesture sensing
esp_err_t setGestureExitThresh(uint8_t threshold)
{
    return (wireWriteDataByte(APDS9960_GEXTH, threshold));
}
//-----------------------------------------------------------------------------
esp_err_t setGestureGCONF2(uint8_t param, uint8_t who)
{
uint8_t val;
esp_err_t rt = ESP_FAIL;

    if (wireReadDataByte(APDS9960_GCONF2, &val) == ESP_OK) {
	switch (who) {
		case 0://setGestureGain - Sets the gain of the photodiode during gesture mode
			param &= 3;//0b00000011;
			param = param << 5;
			val &= 0x9F;//0b10011111;
			val |= param;
		break;
		case 1://setGestureLEDDrive - Sets the LED drive current during gesture mode
			param &= 3;//0b00000011;
			param = param << 3;
			val &= 0xE7;//0b11100111;
			val |= param;
		break;
		case 2://setGestureWaitTime - Sets the time in low power mode between gesture detections
			param &= 7;//0b00000111;
			val &= 0xF8;//0b11111000;
			val |= param;
		break;
	}
	rt = wireWriteDataByte(APDS9960_GCONF2, val);
    }

    return rt;
}
//-----------------------------------------------------------------------------
// Turns gesture-related interrupts on or off
esp_err_t setGestureIntEnable(uint8_t enable)
{
uint8_t val;
esp_err_t rt = ESP_FAIL;

    if (wireReadDataByte(APDS9960_GCONF4, &val) == ESP_OK) {
        // Set bits in register to given value
        uint8_t en = enable;
        en &= 1;//0b00000001;
        en <<= 1;
        val &= 0xFD;//0b11111101;
        val |= en;
        rt = wireWriteDataByte(APDS9960_GCONF4, val);
    }
//    ESP_LOGW(TAGI, "setGestureIntEnable(%u): set APDS9960_GCONF4(0x%02x) to 0x%02x ret=%d", enable, APDS9960_GCONF4, val, rt);

    return rt;
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
esp_err_t apds9960_init()
{
uint8_t id = 0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, APDS9960_I2C_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, APDS9960_ID, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, APDS9960_I2C_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &id, NACK_VAL);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 500 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) goto err_lab;
    if ( !(id == APDS9960_ID_1 || id == APDS9960_ID_2 ) ) goto err_lab;

    // Set ENABLE register to 0 (disable all features)
    if (setMode(ALL, OFF) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_ATIME, DEFAULT_ATIME) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_WTIME, DEFAULT_WTIME) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_PPULSE, DEFAULT_PROX_PPULSE) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_POFFSET_UR, DEFAULT_POFFSET_UR) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_POFFSET_DL, DEFAULT_POFFSET_DL) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_CONFIG1, DEFAULT_CONFIG1) != ESP_OK) goto err_lab;

    //setLEDDrive - Sets the LED drive strength for proximity and ambient light sensor (ALS)
    if (setCtrlDrive(DEFAULT_LDRIVE, 0) != ESP_OK) goto err_lab;
    //setProximityGain - Sets the receiver gain for proximity detection
    if (setCtrlDrive(DEFAULT_PGAIN, 1) != ESP_OK) goto err_lab;
    //setAmbientLightGain - Sets the receiver gain for the ambient light sensor (ALS)
    if (setCtrlDrive(DEFAULT_AGAIN, 2) != ESP_OK) goto err_lab;

    if (setProxIntLowThresh(DEFAULT_PILT) != ESP_OK) goto err_lab;
    if (setProxIntHighThresh(DEFAULT_PIHT) != ESP_OK) goto err_lab;

    if (setLightIntLowThreshold(DEFAULT_AILT) != ESP_OK) goto err_lab;
    if (setLightIntHighThreshold(DEFAULT_AIHT) != ESP_OK) goto err_lab;

    if (wireWriteDataByte(APDS9960_PERS, DEFAULT_PERS) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_CONFIG2, DEFAULT_CONFIG2) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_CONFIG3, DEFAULT_CONFIG3) != ESP_OK) goto err_lab;

    // Set default values for gesture sense registers
    if (setGestureEnterThresh(DEFAULT_GPENTH) != ESP_OK) goto err_lab;
    if (setGestureExitThresh(DEFAULT_GEXTH) != ESP_OK) goto err_lab;

    if (wireWriteDataByte(APDS9960_GCONF1, DEFAULT_GCONF1) != ESP_OK) goto err_lab;

    if (setGestureGCONF2(DEFAULT_GGAIN, 0) != ESP_OK) goto err_lab;//setGestureGain
    if (setGestureGCONF2(DEFAULT_GLDRIVE, 1) != ESP_OK) goto err_lab;//setGestureLEDDrive
    if (setGestureGCONF2(DEFAULT_GWTIME, 2) != ESP_OK) goto err_lab;//setGestureWaitTime

    if (wireWriteDataByte(APDS9960_GOFFSET_U, DEFAULT_GOFFSET) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_GOFFSET_D, DEFAULT_GOFFSET) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_GOFFSET_L, DEFAULT_GOFFSET) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_GOFFSET_R, DEFAULT_GOFFSET) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_GPULSE, DEFAULT_GPULSE) != ESP_OK) goto err_lab;
    if (wireWriteDataByte(APDS9960_GCONF3, DEFAULT_GCONF3) != ESP_OK) goto err_lab;

    if (setGestureIntEnable(DEFAULT_GIEN) != ESP_OK) goto err_lab;

//    ESP_LOGW(TAGI, "APDS9960: (id=0x%02x) init OK (INT disable) | FreeMem %u", id, xPortGetFreeHeapSize());

    return ESP_OK;

err_lab:

    ESP_LOGE(TAGI, "APDS9960: (id=0x%02x) init ERROR | FreeMem %u", id, xPortGetFreeHeapSize());

    return ESP_FAIL;
}
//-----------------------------------------------------------------------------
//Resets all the parameters in the gesture data member
void resetGestureParameters()
{
    gesture_data_.index = 0;
    gesture_data_.total_gestures = 0;

    gesture_ud_delta_ = gesture_lr_delta_ = 0;
    gesture_ud_count_ = gesture_lr_count_ = 0;
    gesture_near_count_ = gesture_far_count_ = 0;

    gesture_state_ = 0;
    gesture_motion_ = DIR_NONE;
}
//-----------------------------------------------------------------------------
/*Sets the LED current boost value
 *
 * Value  Boost Current
 *   0        100%
 *   1        150%
 *   2        200%
 *   3        300%
 */
esp_err_t setLEDBoost(uint8_t boost)
{
uint8_t val;
esp_err_t rt = ESP_FAIL;

    if (wireReadDataByte(APDS9960_CONFIG2, &val) == ESP_OK) {
        // Set bits in register to given value
        boost &= 3;//0b00000011;
        boost <<= 4;
        val &= 0xCF;//0b11001111;
        val |= boost;
        // Write register value back into CONFIG2 register
        rt = wireWriteDataByte(APDS9960_CONFIG2, val);
    }

    return rt;
}
//-----------------------------------------------------------------------------
//Tells the state machine to either enter or exit gesture state machine
esp_err_t setGestureMode(uint8_t mode)
{
uint8_t val;
esp_err_t rt = ESP_FAIL;

    if (wireReadDataByte(APDS9960_GCONF4, &val) == ESP_OK) {
        // Set bits in register to given value
        mode &= 1;//0b00000001;
        val &= 0xFE;//0b11111110;
        val |= mode;
        // Write register value back into GCONF4 register
        rt = wireWriteDataByte(APDS9960_GCONF4, val);
    }

    return rt;
}
//-----------------------------------------------------------------------------
//Turn the APDS-9960 on
esp_err_t enablePower()
{
    return (setMode(POWER, 1));
}
//-----------------------------------------------------------------------------
//Determines if there is a gesture available for reading
bool isGestureAvailable()
{
uint8_t val;
bool ret=false;

    if (wireReadDataByte(APDS9960_GSTATUS, &val) == ESP_OK) {
        // Shift and mask out GVALID bit
        if ((val & APDS9960_GVALID) == 1) ret = true;
    }
//    ESP_LOGW(TAGI, "APDS: isGestureAvailable(): ret=%d reg_gstatus(0x%02x)=0x%02x", ret, APDS9960_GSTATUS, val);

    return ret;
}
//-----------------------------------------------------------------------------
//Starts the gesture recognition engine on the APDS-9960
esp_err_t enableGestureSensor(bool interrupts)
{
esp_err_t rt = ESP_FAIL;

    /* Enable gesture mode
       Set ENABLE to 0 (power off)
       Set WTIME to 0xFF
       Set AUX to LED_BOOST_300
       Enable PON, WEN, PEN, GEN in ENABLE */
    resetGestureParameters();
    if (wireWriteDataByte(APDS9960_WTIME, 0xFF) != ESP_OK) goto lerr;
    if (wireWriteDataByte(APDS9960_PPULSE, DEFAULT_GESTURE_PPULSE) != ESP_OK) goto lerr;

    if (setLEDBoost(LED_BOOST_300) != ESP_OK) goto lerr;

/*  if (interrupts) {
        rt = setGestureIntEnable(1);
    } else {
        rt = setGestureIntEnable(0);
    } */
    rt = setGestureIntEnable((uint8_t)interrupts);
    if (rt != ESP_OK) goto lerr;

    if (setGestureMode(1) != ESP_OK) goto lerr;

    if (enablePower() != ESP_OK) goto lerr;

    if (setMode(WAIT, 1) != ESP_OK) goto lerr;

    if (setMode(PROXIMITY, 1) != ESP_OK) goto lerr;

    if (setMode(GESTURE, 1) != ESP_OK) goto lerr;

    ets_printf("[%s] APDS9960: Enable gesture sensor OK (INT enable = %u) | FreeMem %u\n", TAGI, interrupts, xPortGetFreeHeapSize());

    return ESP_OK;

lerr:

    ESP_LOGE(TAGI, "APDS9960: Enable gesture sensor Error | FreeMem %u", xPortGetFreeHeapSize());
    return ESP_FAIL;
}
//-----------------------------------------------------------------------------
//Processes the raw gesture data to determine swipe direction
bool processGestureData()
{
uint8_t u_first = 0;
uint8_t d_first = 0;
uint8_t l_first = 0;
uint8_t r_first = 0;

uint8_t u_last = 0;
uint8_t d_last = 0;
uint8_t l_last = 0;
uint8_t r_last = 0;

int ud_ratio_first, lr_ratio_first;
int ud_ratio_last, lr_ratio_last;
int ud_delta, lr_delta, i;

    // If we have less than 4 total gestures, that's not enough
    if (gesture_data_.total_gestures <= 4) return false;

    // Check to make sure our data isn't out of bounds
    if ( (gesture_data_.total_gestures <= 32) && (gesture_data_.total_gestures > 0) ) {
        // Find the first value in U/D/L/R above the threshold
        for (i = 0; i < gesture_data_.total_gestures; i++) {
            if ((gesture_data_.u_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.d_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.l_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.r_data[i] > GESTURE_THRESHOLD_OUT)) {

                u_first = gesture_data_.u_data[i];
                d_first = gesture_data_.d_data[i];
                l_first = gesture_data_.l_data[i];
                r_first = gesture_data_.r_data[i];
                break;
            }
        }
        // If one of the _first values is 0, then there is no good data
        if ( (u_first == 0) || (d_first == 0) || (l_first == 0) || (r_first == 0) ) return false;
        // Find the last value in U/D/L/R above the threshold
        for (i = gesture_data_.total_gestures - 1; i >= 0; i-- ) {
            if ((gesture_data_.u_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.d_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.l_data[i] > GESTURE_THRESHOLD_OUT) &&
                (gesture_data_.r_data[i] > GESTURE_THRESHOLD_OUT) ) {

                u_last = gesture_data_.u_data[i];
                d_last = gesture_data_.d_data[i];
                l_last = gesture_data_.l_data[i];
                r_last = gesture_data_.r_data[i];
                break;
            }
        }
    }

    // Calculate the first vs. last ratio of up/down and left/right
    ud_ratio_first = ((u_first - d_first) * 100) / (u_first + d_first);
    lr_ratio_first = ((l_first - r_first) * 100) / (l_first + r_first);
    ud_ratio_last = ((u_last - d_last) * 100) / (u_last + d_last);
    lr_ratio_last = ((l_last - r_last) * 100) / (l_last + r_last);
    // Determine the difference between the first and last ratios
    ud_delta = ud_ratio_last - ud_ratio_first;
    lr_delta = lr_ratio_last - lr_ratio_first;
    // Accumulate the UD and LR delta values
    gesture_ud_delta_ += ud_delta;
    gesture_lr_delta_ += lr_delta;

    // Determine U/D gesture
    if (gesture_ud_delta_ >= GESTURE_SENSITIVITY_1) {
        gesture_ud_count_ = 1;
    } else if (gesture_ud_delta_ <= -GESTURE_SENSITIVITY_1) {
        gesture_ud_count_ = -1;
    } else {
        gesture_ud_count_ = 0;
    }

    // Determine L/R gesture
    if (gesture_lr_delta_ >= GESTURE_SENSITIVITY_1) {
        gesture_lr_count_ = 1;
    } else if (gesture_lr_delta_ <= -GESTURE_SENSITIVITY_1) {
        gesture_lr_count_ = -1;
    } else {
        gesture_lr_count_ = 0;
    }

    // Determine Near/Far gesture
    if ( (gesture_ud_count_ == 0) && (gesture_lr_count_ == 0) ) {
        if ( (abs(ud_delta) < GESTURE_SENSITIVITY_2) && (abs(lr_delta) < GESTURE_SENSITIVITY_2) ) {
            if ( (ud_delta == 0) && (lr_delta == 0) ) {
                gesture_near_count_++;
            } else if ( (ud_delta != 0) || (lr_delta != 0) ) {
                gesture_far_count_++;
            }
            if ( (gesture_near_count_ >= 10) && (gesture_far_count_ >= 2) ) {
                if ( (ud_delta == 0) && (lr_delta == 0) ) {
                    gesture_state_ = NEAR_STATE;
                } else if ( (ud_delta != 0) && (lr_delta != 0) ) {
                    gesture_state_ = FAR_STATE;
                }
                return true;
            }
        }
    } else {
        if ( (abs(ud_delta) < GESTURE_SENSITIVITY_2) && (abs(lr_delta) < GESTURE_SENSITIVITY_2) ) {
            if ( (ud_delta == 0) && (lr_delta == 0) ) gesture_near_count_++;
            if (gesture_near_count_ >= 10) {
                gesture_ud_count_ = 0;
                gesture_lr_count_ = 0;
                gesture_ud_delta_ = 0;
                gesture_lr_delta_ = 0;
            }
        }
    }

    return false;
}
//-----------------------------------------------------------------------------
//Determines swipe direction or near/far state
bool decodeGesture()
{
    // Return if near or far event is detected
    if (gesture_state_ == NEAR_STATE) {
        gesture_motion_ = DIR_NEAR;
        return true;
    } else if (gesture_state_ == FAR_STATE) {
        gesture_motion_ = DIR_FAR;
        return true;
    }

    // Determine swipe direction
    if ( (gesture_ud_count_ == -1) && (gesture_lr_count_ == 0) ) {
        gesture_motion_ = DIR_UP;
    } else if ( (gesture_ud_count_ == 1) && (gesture_lr_count_ == 0) ) {
        gesture_motion_ = DIR_DOWN;
    } else if ( (gesture_ud_count_ == 0) && (gesture_lr_count_ == 1) ) {
        gesture_motion_ = DIR_RIGHT;
    } else if ( (gesture_ud_count_ == 0) && (gesture_lr_count_ == -1) ) {
        gesture_motion_ = DIR_LEFT;
    } else if ( (gesture_ud_count_ == -1) && (gesture_lr_count_ == 1) ) {
        if ( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) )
            gesture_motion_ = DIR_UP;
        else
            gesture_motion_ = DIR_RIGHT;
    } else if ( (gesture_ud_count_ == 1) && (gesture_lr_count_ == -1) ) {
        if ( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) )
            gesture_motion_ = DIR_DOWN;
        else
            gesture_motion_ = DIR_LEFT;
    } else if ( (gesture_ud_count_ == -1) && (gesture_lr_count_ == -1) ) {
        if ( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) )
            gesture_motion_ = DIR_UP;
        else
            gesture_motion_ = DIR_LEFT;
    } else if ( (gesture_ud_count_ == 1) && (gesture_lr_count_ == 1) ) {
        if ( abs(gesture_ud_delta_) > abs(gesture_lr_delta_) )
            gesture_motion_ = DIR_DOWN;
        else
            gesture_motion_ = DIR_RIGHT;
    } else return false;

    return true;
}
//-----------------------------------------------------------------------------
//Processes a gesture event and returns best guessed gesture
int readGesture()
{
uint8_t fifo_data[64], fifo_data2[64];
uint8_t gstatus, byte, fifo_level = 0;
int motion, i, bytes_read = 0;

    // Make sure that power and gesture is on and data is valid
    if (!isGestureAvailable()) return DIR_NONE;
    if (getMode(&byte) != ESP_OK) return DIR_NONE;
    if (!(byte & 0x41)) return DIR_NONE;

    // Keep looping as long as gesture data is valid
    while (true) {
        // Wait some time to collect next batch of FIFO data
        vTaskDelay(FIFO_PAUSE_TIME / portTICK_RATE_MS);
        // Get the contents of the STATUS register. Is data still valid?
        if (wireReadDataByte(APDS9960_GSTATUS, &gstatus) != ESP_OK) return DIR_NONE;

        // If we have valid data, read in FIFO
        if ( (gstatus & APDS9960_GVALID) == APDS9960_GVALID ) {
            // Read the current FIFO level
            if (wireReadDataByte(APDS9960_GFLVL, &fifo_level) != ESP_OK) return DIR_NONE;
            // If there's stuff in the FIFO, read it into our data block
            if (fifo_level > 0) {
                if (fifo_level < 17)
                    bytes_read = wireReadDataBlock(APDS9960_GFIFO_U, (uint8_t*)fifo_data, (fifo_level<<2));
                else
                    bytes_read = wireReadDataBlock(APDS9960_GFIFO_U, (uint8_t*)fifo_data2, (fifo_level<<2));
                if (bytes_read == -1) return ERROR;
                // If at least 1 set of data, sort the data into U/D/L/R
                if (bytes_read >= 4) {
                    for (i = 0; i < bytes_read; i += 4) {
                        gesture_data_.u_data[gesture_data_.index] = fifo_data[i + 0];
                        gesture_data_.d_data[gesture_data_.index] = fifo_data[i + 1];
                        gesture_data_.l_data[gesture_data_.index] = fifo_data[i + 2];
                        gesture_data_.r_data[gesture_data_.index] = fifo_data[i + 3];
                        gesture_data_.index++;
                        gesture_data_.total_gestures++;
                    }
                    // Filter and process gesture data. Decode near/far state
                    if (processGestureData()) {
                        if (decodeGesture()) {
                            // TODO: U-Turn Gestures
                            //ESP_LOGI(TAGI, "APDS: readGesture(): processGestureData() & decodeGesture() is TRUE | FreeMem %u", xPortGetFreeHeapSize());
                        }
                    }
                    // Reset data
                    gesture_data_.index = 0;
                    gesture_data_.total_gestures = 0;
                }
            }
        } else {
            // Determine best guessed gesture and clean up
            vTaskDelay(FIFO_PAUSE_TIME / portTICK_RATE_MS);
            decodeGesture();
            motion = gesture_motion_;
            resetGestureParameters();
            return motion;
        }

    }
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#endif
//-----------------------------------------------------------------------------
#if defined(SET_BMP) || defined(SET_BH1750) || defined(SET_SI7021) || defined(SET_APDS)
void i2c_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);

    i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}
//-----------------------------------------------------------------------------
void i2c_task(void* arg)
{
total_task++;

char *stx = (char *)calloc(1, stx_len);
if (!stx) goto eoj;


#ifdef SET_BMP
    result_t fl;
    memset((uint8_t *)&fl, 0, sizeof(result_t));
    uint8_t data_rdx[DATA_LENGTH] = {0};
    uint8_t reg_id = 0;
    uint8_t reg_stat = 0;
    uint8_t reg_mode = 0;
    uint8_t reg_conf = 0;
    int ret;
    size_t d_size = 1;
    int32_t temp, pres, humi = 0;
#endif

#ifdef SET_BH1750
    uint16_t lux = 0;
    float lx = -1.0;
#endif

#ifdef SET_SI7021
    float si_temp, si_humi;
    err_t si_rst = i2c_7021_reset(I2C_MASTER_NUM);//ESP_FAIL;
    const uint8_t prc = 0x25;
#endif

uint8_t pr = i2c_prn;
TickType_t tcikl = get_tmr(TickErr);


    ets_printf("%s[%s] Start i2c_task | FreeMem %u%s\n", START_COLOR, TAGI, xPortGetFreeHeapSize(), STOP_COLOR);

#ifdef SET_APDS
    s_led_cmd data;
    atomic_init(&apds_flag, 0);
    int rdg=0;
    esp_err_t apds = ESP_FAIL;
    uint32_t cinit = 0;
    apds = apds9960_init();
    if (apds == ESP_OK) enableGestureSensor(1);
    else {
	cinit++;
	ESP_LOGE(TAGI, "Init APDS9960 failure.(%u)", cinit);
    }
    TickType_t terr = get_tmr(5000);
#endif



    while (!restart_flag) {
#ifdef SET_APDS
	if (apds == ESP_OK) {
	    if (atomic_load(&apds_flag)) {
		atomic_store(&apds_flag, 0);
		setGestureIntEnable(0);

		rdg = readGesture();
		memset(stx,0,stx_len);
		sprintf(stx,"APDS:\t%d -> ", rdg);
		if (rdg>0) {
			memset((uint8_t *)&data, 0, size_led_cmd);
			if (xSemaphoreTake(status_mutex, 25/portTICK_RATE_MS) == pdTRUE) {
				memcpy((uint8_t *)&data, (uint8_t *)&led_cmd, size_led_cmd);
				xSemaphoreGive(status_mutex);
			} else {
				data.size = PIXELS;
			}
			data.mode = RGB_NONE;
		}
		switch (rdg) {
			case DIR_UP://3
				sprintf(stx+strlen(stx), "UP");
				data.mode = 3;//line_up
				rgb_mode = RGB_LINEUP;
			break;
			case DIR_DOWN://4
				sprintf(stx+strlen(stx), "DOWN");
				data.mode = 4;//line_down
				rgb_mode = RGB_LINEDOWN;
			break;
			case DIR_LEFT://1
				sprintf(stx+strlen(stx), "LEFT");
				data.mode = 5;//white_up
				rgb_mode = WHITE_UP;
			break;
			case DIR_RIGHT://2
				sprintf(stx+strlen(stx), "RIGHT");
				data.mode = 6;//white_down
				rgb_mode = WHITE_DOWN;
			break;
			case DIR_NEAR://5
				sprintf(stx+strlen(stx), "NEAR");
				data.mode = 0;//rgb -> off
				rgb_mode = RGB_NONE;
			break;
			case DIR_FAR://6
				sprintf(stx+strlen(stx), "FAR");
				data.mode = 1;//rainbow_on
				rgb_mode = RGB_RAINBOW;
			break;
				default: sprintf(stx+strlen(stx), "NONE");
		}
		sprintf(stx+strlen(stx)," (intr_count=%u)", apds_count);
		print_msg(TAGI, NULL, stx, 1);
		if (rdg > 0) {
			if (xQueueSend(led_cmd_queue, (void *)&data, (TickType_t)0) != pdTRUE) {
				#ifdef SET_ERROR_PRINT
					ESP_LOGE(TAGI, "Error while sending to led_cmd_queue.");
				#endif
			}
		}
		setGestureIntEnable(1);
	    }
	    vTaskDelay(50 / portTICK_RATE_MS);//(50 /
	} else {
	    if (check_tmr(terr)) {
		apds = apds9960_init();
		if (apds == ESP_OK) enableGestureSensor(1);
		else {
		    cinit++;
		    ESP_LOGE(TAGI, "Init APDS9960 failure.(%u)", cinit);
		}
		terr = get_tmr(TickErr);
	    } else vTaskDelay(50 / portTICK_RATE_MS);//(50 /
	}

#endif

	if (check_tmr(tcikl)) {
	    tcikl = get_tmr(TickDef);
	    pr = i2c_prn;
	    memset(stx, 0, stx_len);

#ifdef SET_SI7021
	    if (si_rst != ESP_OK) si_rst = i2c_7021_reset(I2C_MASTER_NUM);
	    if (si_rst == ESP_OK) {
		si_humi = si_temp = 0.0;
		si_temp = i2c_7021_read_temp();
		si_humi = i2c_7021_read_rh();
		sprintf(stx+strlen(stx),"ADDR=0x%02X (SI7021) Humidity=%.2f %c Temp=%.2f DegC ; ", SI7021_ADDRESS, si_humi, prc, si_temp);
		if (xSemaphoreTake(sensors_mutex, 50/portTICK_RATE_MS) == pdTRUE) {
		    sensors.si_humi = si_humi;
		    sensors.si_temp = si_temp;
		    xSemaphoreGive(sensors_mutex);
		}
	    }
#endif

#ifdef SET_BH1750
	    lx = -1.0;
	    if (bh1750_proc(&lux) == ESP_OK) {
		lx = lux / 1.2;
		sprintf(stx+strlen(stx),"ADDR=0x%02X (BH1750) Lux=%.2f lx ; ", BH1750_ADDR, lx);
		if (xSemaphoreTake(sensors_mutex, 50/portTICK_RATE_MS) == pdTRUE) {
		    sensors.lux = lx;
		    xSemaphoreGive(sensors_mutex);
		}
	    }
    #ifdef SET_BMP
	    vTaskDelay(400 / portTICK_RATE_MS);
    #endif
#endif

#ifdef SET_BMP
	    ret = i2c_master_reset_sensor(&reg_id);
	    if (ret != ESP_OK) {
		//ESP_LOGW(TAGI, "No ANSWER FROM SENSOR 0x%02X (reg_id=0x%02x)", BMP280_ADDR, reg_id);
		ets_printf("%s[%s] No ANSWER FROM SENSOR 0x%02X (reg_id=0x%02x)%s\n", BROWN_COLOR, TAGI, BMP280_ADDR, reg_id, STOP_COLOR);
		vTaskDelay(200 / portTICK_RATE_MS);
		tcikl = get_tmr(TickErr);
		continue;
	    }

	    ret = i2c_master_test_sensor(&reg_stat, &reg_mode, &reg_conf, reg_id);
	    reg_stat &= 0x0f;
	    sprintf(stx+strlen(stx),"ADDR=0x%02X ", BMP280_ADDR);
	    if (ret == ESP_OK) {
		switch (reg_id) {
			case BMP280_SENSOR : sprintf(stx+strlen(stx),"(BMP280)"); d_size = 6; break;
			case BME280_SENSOR : sprintf(stx+strlen(stx),"(BME280)"); d_size = 8; break;
			default : sprintf(stx+strlen(stx),"(unknown)");
		}
		//printf("\nid=0x%02x stat: 0x%02x (%1d.%1d), mode: 0x%02x, conf: 0x%02x\n", reg_id, reg_stat, reg_stat>>3, reg_stat&1, reg_mode, reg_conf);
	    } else {
		sprintf(stx+strlen(stx)," No ack...try again. (reg_id=0x%02x)\n", reg_id);
		print_msg(TAGI, NULL, stx, 1);
		vTaskDelay(200 / portTICK_RATE_MS);
		tcikl = get_tmr(TickErr);
		continue;
	    }
	    //---------------------------------------------------
	    memset(data_rdx, 0, DATA_LENGTH);
	    ret = i2c_master_read_sensor(BMP280_REG_PRESSURE, &data_rdx[0], d_size);
	    if (ret == ESP_OK) {
		//--------------------------------------------
		if (readCalibrationData(reg_id) != ESP_OK) {
			sprintf(stx+strlen(stx)," Reading Calibration Data ERROR");
		} else {
			pres = temp = 0;
			pres = (data_rdx[0] << 12) | (data_rdx[1] << 4) | (data_rdx[2] >> 4);
			temp = (data_rdx[3] << 12) | (data_rdx[4] << 4) | (data_rdx[5] >> 4);
			if (reg_id == BME280_SENSOR) humi = (data_rdx[6] << 8) | data_rdx[7];
			CalcAll(reg_id, temp, pres, humi);
			if (xSemaphoreTake(sensors_mutex, 50/portTICK_RATE_MS) == pdTRUE) {
			    memcpy((uint8_t *)&fl, (uint8_t *)&sensors, sizeof(result_t));
			    xSemaphoreGive(sensors_mutex);
			}
			sprintf(stx+strlen(stx)," Press=%.2f mmHg, Temp=%.2f DegC", fl.pres, fl.temp);
			if (reg_id == BME280_SENSOR) sprintf(stx+strlen(stx)," Humidity=%.2f %crH", fl.humi, 0x25);
		}
	    } else sprintf(stx+strlen(stx),"\nMaster read slave error, IO not connected, maybe.");
#endif
	    if ((pr) && (strlen(stx))) {
		sprintf(stx+strlen(stx)," | FreeMem=%u\n", xPortGetFreeHeapSize());
		print_msg(TAGI, NULL, stx, 1);
	    }

	    tcikl = get_tmr(TickDef);

	}

    }//while(!restart_flag)

    if (stx) free(stx);

/**/
#ifdef SET_APDS
    setGestureIntEnable(0);
#endif
#ifdef SET_BH1750
    esp_err_t bh1750_off();
#endif
#ifdef SET_BMP
    i2c_master_reset_sensor(&reg_id);
#endif
#ifdef SET_SI7021
    i2c_7021_reset(I2C_MASTER_NUM);
#endif
/**/

eoj:
    ets_printf("%s[%s] Stop i2c_task | FreeMem %u%s\n", START_COLOR, TAGI, xPortGetFreeHeapSize(), STOP_COLOR);
    if (total_task) total_task--;
    vTaskDelete(NULL);
}
//******************************************************************************************
#endif

#endif
