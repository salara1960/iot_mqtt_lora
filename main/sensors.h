#ifndef __SENSORS_H__
#define __SENSORS_H__

#include "hdr.h"
#ifdef SET_I2C

#pragma once

    #ifdef SET_BH1750
        /*  GY-302 (BH1750)  */
        #define BH1750_ADDR            0x23 //!< slave address for BH1750 sensor (0x23 addr=L, 0x5C addr=H)
        #define BH1750_POWER_DOWN      0x00
        #define BH1750_POWER_ON        0x01
        #define BH1750_RESET_VALUE     0x07
        #define BH1750_CON_HRES_MODE   0x10 //120ms 1lx resolution
        #define BH1750_CON_HRES_MODE2  0x11 //120ms 0.5lx resolution
        #define BH1750_CON_LRES_MODE   0x13 //16ms 4lx resolution
        #define BH1750_OT_HRES_MODE    0x20 //120ms 1lx resolution
        #define BH1750_OT_HRES_MODE2   0x21 //120ms 0.5lx resolution
        #define BH1750_OT_LRES_MODE    0x23 //16ms 4lx resolution
    #endif


    #ifdef SET_BMP
	/*  BMP280/BME280 registers  */
        #define BMP280_ADDR            0x76 //!< slave address for BMP280 sensor
        #define BMP280_SENSOR          0x58
        #define BME280_SENSOR          0x60
        #define BMP280_REG_TEMP_XLSB   0xFC // bits: 7-4
        #define BMP280_REG_TEMP_LSB    0xFB
        #define BMP280_REG_TEMP_MSB    0xFA
        #define BMP280_REG_TEMP        (BMP280_REG_TEMP_MSB)
        #define BMP280_REG_PRESS_XLSB  0xF9 // bits: 7-4
        #define BMP280_REG_PRESS_LSB   0xF8
        #define BMP280_REG_PRESS_MSB   0xF7
        #define BMP280_REG_PRESSURE    (BMP280_REG_PRESS_MSB)
        #define BMP280_REG_CONFIG      0xF5 // bits: 7-5 t_sb; 4-2 filter; 0 spi3w_en
        #define BMP280_REG_CTRL        0xF4 // bits: 7-5 osrs_t; 4-2 osrs_p; 1-0 mode
        #define BMP280_REG_STATUS      0xF3 // bits: 3 measuring; 0 im_update
        #define BME280_REG_CTRL_HUM    0xF2 // bits: 2-0 osrs_h;
        #define BMP280_REG_RESET       0xE0
        #define BMP280_REG_ID          0xD0
        #define BMP280_REG_CALIB       0x88
        #define BMP280_NORMAL_MODE     0x3
        #define BMP280_FORCED1_MODE    0x1
        #define BMP280_FORCED2_MODE    0x2
        #define BMP280_OSRS_T          0x20
        #define BMP280_OSRS_P          0x04
        #define BMP280_CONF_T_SB       0x40
        #define BMP280_CONF_FILTER     0x00
        #define BMP280_CONF_SPI3W      0x00
        #define BME280_OSRS_H          0x01
        #define BMP280_RESET_VALUE     0xB6

	typedef struct bmp280_calib_t {
	    uint16_t dig_T1;
	    int16_t dig_T2;
	    int16_t dig_T3;
	    uint16_t dig_P1;
	    int16_t dig_P2;
	    int16_t dig_P3;
	    int16_t dig_P4;
	    int16_t dig_P5;
	    int16_t dig_P6;
	    int16_t dig_P7;
	    int16_t dig_P8;
	    int16_t dig_P9;
	    int8_t  dig_H1;
	    int16_t dig_H2;
	    int8_t  dig_H3;
	    int16_t dig_H4;
	    int16_t dig_H5;
	    int8_t  dig_H6;
	} bmx280_calib_t;

	extern bmx280_calib_t calib;
    #endif

    //    /* TSL2561 */
    //    #define TSL2561_ADDR           0x39 //!< slave address for TSL2561 sensor (0x29 addr=L, 0x49 addr=H, 0x39 addr=float)

    #if defined(SET_BMP) || defined(SET_BH1750) || defined(SET_SI7021)
        #pragma pack(push,1)
        typedef struct {
	    double temp;// DegC
	    double pres;// mmHg
	    double humi;// %rH
	    float lux;//lx
	    float si_humi;
	    float si_temp;
	} result_t;
	#pragma pack(pop)

	extern xSemaphoreHandle sensors_mutex;
	extern result_t sensors;
    #endif

    #ifdef SET_APDS
	#include "esp_types.h"
	#include "stdatomic.h"

	/* APDS-9960 I2C address */
	#define APDS9960_I2C_ADDR       0x39
	/* Gesture parameters */
	#define GESTURE_THRESHOLD_OUT   10
	#define GESTURE_SENSITIVITY_1   50
	#define GESTURE_SENSITIVITY_2   20
	/* Error code for returned values */
	#define ERROR                   0xFF
	/* Acceptable device IDs */
	#define APDS9960_ID_1           0xAB
	#define APDS9960_ID_2           0x9C
	/* Misc parameters */
	#define FIFO_PAUSE_TIME         30      // Wait period (ms) between FIFO reads
	/* APDS-9960 register addresses */
	#define APDS9960_ENABLE         0x80
	#define APDS9960_ATIME          0x81
	#define APDS9960_WTIME          0x83
	#define APDS9960_AILTL          0x84
	#define APDS9960_AILTH          0x85
	#define APDS9960_AIHTL          0x86
	#define APDS9960_AIHTH          0x87
	#define APDS9960_PILT           0x89
	#define APDS9960_PIHT           0x8B
	#define APDS9960_PERS           0x8C
	#define APDS9960_CONFIG1        0x8D
	#define APDS9960_PPULSE         0x8E
	#define APDS9960_CONTROL        0x8F
	#define APDS9960_CONFIG2        0x90
	#define APDS9960_ID             0x92
	#define APDS9960_STATUS         0x93
	#define APDS9960_CDATAL         0x94
	#define APDS9960_CDATAH         0x95
	#define APDS9960_RDATAL         0x96
	#define APDS9960_RDATAH         0x97
	#define APDS9960_GDATAL         0x98
	#define APDS9960_GDATAH         0x99
	#define APDS9960_BDATAL         0x9A
	#define APDS9960_BDATAH         0x9B
	#define APDS9960_PDATA          0x9C
	#define APDS9960_POFFSET_UR     0x9D
	#define APDS9960_POFFSET_DL     0x9E
	#define APDS9960_CONFIG3        0x9F
	#define APDS9960_GPENTH         0xA0
	#define APDS9960_GEXTH          0xA1
	#define APDS9960_GCONF1         0xA2
	#define APDS9960_GCONF2         0xA3
	#define APDS9960_GOFFSET_U      0xA4
	#define APDS9960_GOFFSET_D      0xA5
	#define APDS9960_GOFFSET_L      0xA7
	#define APDS9960_GOFFSET_R      0xA9
	#define APDS9960_GPULSE         0xA6
	#define APDS9960_GCONF3         0xAA
	#define APDS9960_GCONF4         0xAB
	#define APDS9960_GFLVL          0xAE
	#define APDS9960_GSTATUS        0xAF
	#define APDS9960_IFORCE         0xE4
	#define APDS9960_PICLEAR        0xE5
	#define APDS9960_CICLEAR        0xE6
	#define APDS9960_AICLEAR        0xE7
	#define APDS9960_GFIFO_U        0xFC
	#define APDS9960_GFIFO_D        0xFD
	#define APDS9960_GFIFO_L        0xFE
	#define APDS9960_GFIFO_R        0xFF
	/* Bit fields */
	#define APDS9960_PON            1//0b00000001
	#define APDS9960_AEN            2//0b00000010
	#define APDS9960_PEN            4//0b00000100
	#define APDS9960_WEN            8//0b00001000
	#define APSD9960_AIEN           0x10//0b00010000
	#define APDS9960_PIEN           0x20//0b00100000
	#define APDS9960_GEN            0x40//0b01000000
	#define APDS9960_GVALID         1//0b00000001
	/* On/Off definitions */
	#define OFF                     0
	#define ON                      1
	/* Acceptable parameters for setMode */
	#define POWER                   0
	#define AMBIENT_LIGHT           1
	#define PROXIMITY               2
	#define WAIT                    3
	#define AMBIENT_LIGHT_INT       4
	#define PROXIMITY_INT           5
	#define GESTURE                 6
	#define ALL                     7
	/* LED Drive values */
	#define LED_DRIVE_100MA         0
	#define LED_DRIVE_50MA          1
	#define LED_DRIVE_25MA          2
	#define LED_DRIVE_12_5MA        3
	/* Proximity Gain (PGAIN) values */
	#define PGAIN_1X                0
	#define PGAIN_2X                1
	#define PGAIN_4X                2
	#define PGAIN_8X                3
	/* ALS Gain (AGAIN) values */
	#define AGAIN_1X                0
	#define AGAIN_4X                1
	#define AGAIN_16X               2
	#define AGAIN_64X               3
	/* Gesture Gain (GGAIN) values */
	#define GGAIN_1X                0
	#define GGAIN_2X                1
	#define GGAIN_4X                2
	#define GGAIN_8X                3
	/* LED Boost values */
	#define LED_BOOST_100           0
	#define LED_BOOST_150           1
	#define LED_BOOST_200           2
	#define LED_BOOST_300           3
	/* Gesture wait time values */
	#define GWTIME_0MS              0
	#define GWTIME_2_8MS            1
	#define GWTIME_5_6MS            2
	#define GWTIME_8_4MS            3
	#define GWTIME_14_0MS           4
	#define GWTIME_22_4MS           5
	#define GWTIME_30_8MS           6
	#define GWTIME_39_2MS           7
	/* Default values */
	#define DEFAULT_ATIME           219     // 103ms
	#define DEFAULT_WTIME           246     // 27ms
	#define DEFAULT_PROX_PPULSE     0x87    // 16us, 8 pulses
	#define DEFAULT_GESTURE_PPULSE  0x89    // 16us, 10 pulses
	#define DEFAULT_POFFSET_UR      0       // 0 offset
	#define DEFAULT_POFFSET_DL      0       // 0 offset      
	#define DEFAULT_CONFIG1         0x60    // No 12x wait (WTIME) factor
	#define DEFAULT_PGAIN           PGAIN_4X
	#define DEFAULT_AGAIN           AGAIN_4X
	#define DEFAULT_PILT            0       // Low proximity threshold
	#define DEFAULT_PIHT            50      // High proximity threshold
	#define DEFAULT_AILT            0xFFFF  // Force interrupt for calibration
	#define DEFAULT_AIHT            0
	#define DEFAULT_PERS            0x11    // 2 consecutive prox or ALS for int.
	#define DEFAULT_CONFIG2         0x01    // No saturation interrupts or LED boost  
	#define DEFAULT_CONFIG3         0       // Enable all photodiodes, no SAI
	#define DEFAULT_GPENTH          40      // Threshold for entering gesture mode
	#define DEFAULT_GEXTH           30      // Threshold for exiting gesture mode    
	#define DEFAULT_GCONF1          0x40    // 4 gesture events for int., 1 for exit
	#define DEFAULT_GGAIN           GGAIN_4X
	#define DEFAULT_GWTIME          GWTIME_2_8MS
	#define DEFAULT_GOFFSET         0       // No offset scaling for gesture mode
	#define DEFAULT_GPULSE          0xC9    // 32us, 10 pulses
	#define DEFAULT_GCONF3          0       // All photodiodes active during gesture
	#define DEFAULT_GIEN            0       // Disable gesture interrupts
	#define DEFAULT_LDRIVE          LED_DRIVE_12_5MA //LED_DRIVE_100MA
	#define DEFAULT_GLDRIVE         LED_DRIVE_12_5MA //LED_DRIVE_100MA

	#define abs(x) ((x) > 0 ? (x) : -(x))

	/* Direction definitions */
	enum {
	    DIR_NONE,
	    DIR_LEFT,
	    DIR_RIGHT,
	    DIR_UP,
	    DIR_DOWN,
	    DIR_NEAR,
	    DIR_FAR,
	    DIR_ALL
	};
	/* State definitions */
	enum {
	    NA_STATE,
	    NEAR_STATE,
	    FAR_STATE,
	    ALL_STATE
	};

	/* Container for gesture data */
	typedef struct gesture_data_type {
	    uint8_t u_data[16];
	    uint8_t d_data[16];
	    uint8_t l_data[16];
	    uint8_t r_data[16];
	    uint8_t index;
	    uint8_t total_gestures;
	    uint8_t in_threshold;
	    uint8_t out_threshold;
	} gesture_data_type;

	#define GPIO_INPUT_I2C          5
	#define GPIO_INPUT_PIN_SEL_I2C  (1<<GPIO_INPUT_I2C)
	#define ESP_INTR_FLAG_DEFAULT   0
	//extern xQueueHandle apds_evt_queue;
	extern void IRAM_ATTR apds_isr_handler(void *arg);
    #endif

    #ifdef SET_SI7021
        #define  SI7021_ADDRESS             0x40
        #define  SN_CMD_START               0xFA
        #define  SN_CMD2                    0x0F
    #endif

    #if defined(SET_BMP) || defined(SET_BH1750) || defined(SET_APDS) || defined(SET_SI7021)

	#define I2C_MASTER_SCL_IO           19         //!< gpio number for I2C master clock
	#define I2C_MASTER_SDA_IO           18         //!< gpio number for I2C master data
	#define I2C_MASTER_NUM              I2C_NUM_0  //!< I2C port number for master dev
        #define I2C_MASTER_TX_BUF_DISABLE   0          //!< I2C master do not need buffer
        #define I2C_MASTER_RX_BUF_DISABLE   0          //!< I2C master do not need buffer
        #define I2C_MASTER_FREQ_HZ          800000     //!< I2C master clock frequency

        #define WRITE_BIT                   0x00       //I2C_MASTER_WRITE /*!< I2C master write
        #define READ_BIT                    0x01       //I2C_MASTER_READ  /*!< I2C master read
        #define ACK_CHECK_EN                0x01       //!< I2C master will check ack from slave
        #define ACK_CHECK_DIS               0x00       //!< I2C master will not check ack from slave
        #define ACK_VAL                     0x00       //!< I2C ack value
        #define NACK_VAL                    0x01       //!< I2C nack value

	#define DATA_LENGTH                 256        //!<Data buffer length for test buffer
	#define stx_len                     256

	extern const char *TAGI;
	extern void i2c_master_init();
	extern void i2c_task(void* arg);
    #endif

    #define TickDef 3000
    #define TickErr 2000
    extern uint8_t i2c_prn;

#endif

#endif

