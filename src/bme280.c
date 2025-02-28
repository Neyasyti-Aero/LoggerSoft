/**
 *******************************************
 * @file    bme280.c
 * @author  Łukasz Juraszek / JuraszekL
 * @date	20.04.2023
 * @brief   Source code for BME280 Driver
 * @note 	https://github.com/JuraszekL/BME280_Driver
 *******************************************
*/

/**
 * @addtogroup BME280_Driver
 * @{
 */

//***************************************

#include "main.h"

#ifdef USE_BME280_SPI

#include <stdint.h>
#include <stddef.h>
#include "bme280.h"
#include "gd32e23x_hal_basetick.h"

/**
 * @defgroup BME280_priv Private Resources
 * @brief only for internal library purposes
 * @{
 */


/**
 * @defgroup BME280_privmacros Macros
 * @{
 */
	/// concatenate two bytes into signed half-word
#define CAT_I16T(msb, lsb) ((int16_t)(((int16_t)msb << 8) | (int16_t)lsb))

	/// concatenate two bytes into unsigned half-word
#define CAT_UI16T(msb, lsb) ((uint16_t)(((uint16_t)msb << 8) | (uint16_t)lsb))

	/// check if x is null
#define IS_NULL(x)	((NULL == x))
///@}

/**
 * @defgroup BME280_privenums Enums
 * @{
 */
	/// type of read, used as parameter to call #bme280_read_compensate function
enum { read_all = 0, read_temp, read_press, read_hum};

	/// possible value of "mode" variable inside #BME280_t structure
enum { sleep_mode = 0x00, forced_mode = 0x01, normal_mode = 0x03 };

	/// possible value of "initialized" variabie inside #BME280_t structure
enum { not_initialized = 0x00, initialized };
///@}

/**
 * @struct adc_regs
 * @brief keeps raw adc values from sensor, used in #bme280_read_compensate function
 * @{
 */
struct adc_regs {

	uint8_t press_raw[BME280_PRESS_ADC_LEN];	///< keeps raw adc value of pressure
	uint8_t temp_raw[BME280_TEMP_ADC_LEN];	///< keeps raw adc value of temperature
	uint8_t hum_raw[BME280_HUM_ADC_LEN];	///< keeps raw adc value of humidity

} __attribute__((aligned(1))) ;
///@}

float combineToFloat(int32_t integerPart, int32_t fractionalPart)
{
    int32_t fractionalDigits = 0;
    int32_t temp = fractionalPart;

    while (temp > 0) {
        temp /= 10;
        fractionalDigits++;
    }

    float result = integerPart + fractionalPart / pow(10, fractionalDigits);

    return result;
}

/**
 *@defgroup BME280_privfunct Functions
 *@{
 */
int8_t bme280_read_platform_spec(uint8_t reg_addr, uint8_t *rxbuff, uint8_t rxlen, void *driver){

	/* check parameters */
	if((NULL == rxbuff) || (NULL == driver)) return -1;

	/* prepare local pointers */
	BME280_Driver_t *drv = (BME280_Driver_t *)driver;
	struct spi_bus_data *spi = (struct spi_bus_data *)drv->env_spec_data;

	/* set chip select low */
	hal_gpio_bit_reset(spi->NCS_gpio, spi->NCS_pin);

	/* send register's address to be read */
	if(hal_spi_transmit_poll(spi->spi_handle, &reg_addr, 1U, 200) != HAL_ERR_NONE) return -1;

	/* read data */
	if(hal_spi_receive_poll(spi->spi_handle, rxbuff, rxlen, 200) != HAL_ERR_NONE) return -1;

	/* set chip select high */
	hal_gpio_bit_set(spi->NCS_gpio, spi->NCS_pin);

	return 0;
}

int8_t bme280_write_platform_spec(uint8_t reg_addr, uint8_t value, void *driver){

	/* check parameter */
	if(NULL == driver) return -1;

	/* prepare local pointers */
	BME280_Driver_t *drv = (BME280_Driver_t *)driver;
	struct spi_bus_data *spi = (struct spi_bus_data *)drv->env_spec_data;

	/* local buffer to store data to send */
	uint8_t buff[2];

	buff[0] = reg_addr & 0x7F;	// first element keeps address to be written (MSB must be reset in write mode!)
	buff[1] = value;			// second element keeps value to bw written

	/* set chip select low */
	hal_gpio_bit_reset(spi->NCS_gpio, spi->NCS_pin);

	/* send register's address and value at once */
	if(hal_spi_transmit_poll(spi->spi_handle, buff, 2U, 200) != HAL_ERR_NONE) return -1;

	/* set chip select high */
	hal_gpio_bit_set(spi->NCS_gpio, spi->NCS_pin);

	return 0;
}

void bme280_delay_platform_spec(uint32_t delay_time){

	hal_basetick_delay_ms(delay_time);
}

/**
 * @brief read compensation data
 *
 * Function reads individual compensation data from sensor and stores them into #BME280_calibration_data
 * inside *Dev structure
 */
static int8_t bme280_read_compensation_parameters(BME280_t *Dev);

/**
 * @brief read and compensate measured values
 *
 * Function reads selected adc values from sensor, converts them into single variables and
 * compensate with calibration data stored in #BME280_calibration_data inside *Dev structure. These
 * variales are then returned as single integer values.
 */
static int8_t bme280_read_compensate(uint8_t read_type, BME280_t *Dev, BME280_S32_t *temp,
	BME280_U32_t *press, BME280_U32_t *hum);

/**
 * @brief convert buffer to single variable
 *
 * Function converts raw adc values of temperature or pressure to single #BME280_S32_t variable
 */
static BME280_S32_t bme280_parse_press_temp_s32t(uint8_t *raw);

/**
 * @brief convert buffer to single variable
 *
 * Function converts raw adc values of humidity to single #BME280_S32_t variable
 */
static BME280_S32_t bme280_parse_hum_s32t(uint8_t *raw);

/**
 * @brief compensate temperature value
 *
 * Function returns compensated temperature in DegC, resolution is 0.01 DegC. Output value of “5123”
 * equals 51.23 DegC. It calculates t_fine variable stored inside *Dev structure as well.
 */
static BME280_S32_t bme280_compensate_t_s32t(BME280_t *Dev, BME280_S32_t adc_T);

/**
 * @brief compensate pressure value
 *
 * Function returns compensated pressure in Pa as unsigned 32 bit integer. Output value depends of #USE_64BIT
 * configuration.
 */
static BME280_U32_t bme280_compensate_p_u32t(BME280_t *Dev, BME280_S32_t adc_P);

/**
 * @brief compensate humidity value
 *
 * Function returns compensated humidity in %RH as unsigned 32bit integer. Output value of "47445"
 * represents 47445/1000 = 47.445 %RH
 */
static BME280_U32_t bme280_compensate_h_u32t(BME280_t *Dev, BME280_S32_t adc_H);

#ifdef USE_INTEGER_RESULTS
/**
 * @brief convert temperature to structure
 *
 * Function converts temperature stored in #BME280_S32_t to #BME280_Data_t structure
 */
static void bme280_convert_t_S32_struct(BME280_S32_t temp, BME280_Data_t *data);
#endif

#ifdef USE_FLOATS_RESULTS
/**
 * @brief convert temperature to float
 *
 * Function converts temperature stored in #BME280_S32_t to float variable
 */
static void bme280_convert_t_S32_float(BME280_S32_t temp_in, float *temp_out);
#endif

#ifdef USE_INTEGER_RESULTS
/**
 * @brief convert pressure to structure
 *
 * Function converts pressure stored in #BME280_U32_t to #BME280_Data_t structure
 */
static void bme280_convert_p_U32_struct(BME280_U32_t press, BME280_Data_t *data);
#endif

#ifdef USE_FLOATS_RESULTS
/**
 * @brief convert pressure to structure
 *
 * Function converts pressure stored in #BME280_U32_t to float variable
 */
static void bme280_convert_p_U32_float(BME280_U32_t press_in, float *press_out);
#endif

#ifdef USE_INTEGER_RESULTS
/**
 * @brief convert humidity to structure
 *
 * Function converts humidity stored in #BME280_U32_t to #BME280_Data_t structure
 */
static void bme280_convert_h_U32_struct(BME280_U32_t hum, BME280_Data_t *data);
#endif

#ifdef USE_FLOATS_RESULTS
/**
 * @brief convert humidity to structure
 *
 * Function converts humidity stored in #BME280_U32_t to float variable
 */
static void bme280_convert_h_U32_float(BME280_U32_t hum_in, float *hum_out);
#endif

#ifdef USE_NORMAL_MODE
/**
 * @brief check for normal mode
 *
 * Function checks if device is initializes and if it's status in *Dev structure
 * is set as "normal_mode"
 */
static int8_t bme280_is_normal_mode(BME280_t *Dev);
#endif

/**
 * @brief check for sleep mode
 *
 * Function checks if device is initializes and if it's status in *Dev structure
 * is set as "sleep_mode"
 */
static int8_t bme280_is_sleep_mode(BME280_t *Dev);

#ifdef USE_FORCED_MODE
/**
 * @brief check and set forced mode
 *
 * Function checks if conditions to set forced mode are met, calculates and returns required
 * delay time (via *delay pointer) then sets forced mode
 */
static int8_t bme280_set_forced_mode(BME280_t *Dev, uint8_t *delay);

/**
 * @brief change osrs_x reg value to oversampling
 *
 * Function converts register value to oversampling value
 * f.e. osrs_p = 0x04 then pressure oversampling = x8
 */
static void bme280_osrs_to_oversampling(uint8_t *osrs);

/**
 * @brief check if sensor is busy
 *
 * Function reads "status" register and checks value of two its bits
 * returns #BME280_BUSY_ERR if any of them is set
 */
static int8_t bme280_busy_check(BME280_t *Dev);
#endif
///@}
///@}

//***************************************
/* public functions */
//***************************************

	/* function that initiates minimum required parameters to operate with a sensor */
int8_t BME280_Init(BME280_t *Dev, BME280_Driver_t *Driver){

	int8_t res = BME280_OK;
	uint8_t id = 0;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Driver) || IS_NULL(Driver->write) || IS_NULL(Driver->read) ||
			IS_NULL(Driver->delay) ) return BME280_PARAM_ERR;

	/* attach the driver to main structure */
	Dev->driver = Driver;

	/* perform sensor reset */
	res = BME280_Reset(Dev);
	if(BME280_OK != res) return res;

	/* Start-up time = 2ms */
	Dev->driver->delay(4);

	/* read and check chip ID */
	res = Dev->driver->read(BME280_ID_ADDR, &id, 1, Dev->driver);
	if (BME280_OK != res) return BME280_INTERFACE_ERR;

	//if(BME280_ID != id) return BME280_ID_ERR;

	/* read, parse and store compensation data */
	res = bme280_read_compensation_parameters(Dev);

	if(BME280_OK == res) Dev->initialized = initialized;
	return res;
}

	/* Function  configures all sensor parameters at once */
int8_t BME280_ConfigureAll(BME280_t *Dev, BME280_Config_t *Config){

	int8_t res = BME280_OK;
	uint8_t ctrl_hum = 0, ctrl_meas = 0, config = 0;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Config) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* set the data from Config structure to the right positions in
	 * sensor registers */
	ctrl_hum = Config->oversampling_h & 0x07;	//0x07 - 0b00000111

	ctrl_meas |= (Config->oversampling_t << 5) & 0xE0; 	//0xE0 - 0b11100000
	ctrl_meas |= (Config->oversampling_p << 2) & 0x1C;	//0x1C - 0b00011100
	ctrl_meas |= Config->mode & 0x03;					//0x03 - 0b00000011

	config |= (Config->t_stby << 5) & 0xE0;	//0xE0 - 0b11100000
	config |= (Config->filter << 2) & 0x1C;	//0x1C - 0b00011100
	config |= Config->spi3w_enable & 0x01;	//0x01 - 0b00000001

	/* send three config bytes to the device */
	res = Dev->driver->write(BME280_CTRL_HUM_ADDR, ctrl_hum, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;
	res = Dev->driver->write(BME280_CTRL_MEAS_ADDR, ctrl_meas, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;
	res = Dev->driver->write(BME280_CONFIG_ADDR, config, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* set oparing mode inside Dev structure */
	if(BME280_SLEEPMODE == Config->mode){

		Dev->mode = sleep_mode;
	}
	else if(BME280_FORCEDMODE == Config->mode){

		Dev->mode = forced_mode;
	}
	else if(BME280_NORMALMODE == Config->mode){

		Dev->mode = normal_mode;
	}

	return res;
}

	/* function performs power-on reset procedure for sensor */
int8_t BME280_Reset(BME280_t *Dev){

	int8_t res = BME280_OK;

	/* check parameter */
	if( IS_NULL(Dev) || IS_NULL(Dev->driver->write) ) return BME280_PARAM_ERR;

	/* write reset commad to reset register */
	res = Dev->driver->write(BME280_RESET_ADDR, BME280_RESET_VALUE, Dev->driver);

	/* set mode to default */
	Dev->mode = sleep_mode;

	return res;
}

#ifdef USE_GETTERS
	/* Function reads current operation mode from sensor */
int8_t BME280_GetMode(BME280_t *Dev, uint8_t *Mode){

	int8_t res = BME280_OK;
	uint8_t ctrl_meas = 0;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Mode) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_NO_INIT_ERR == res) return res;

	/* read value of ctrl_meas register from sensor */
	res = Dev->driver->read(BME280_CTRL_MEAS_ADDR, &ctrl_meas, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* parse mode values from ctrl_meas */
	ctrl_meas &= 0x03;
	if(0x02 == ctrl_meas) ctrl_meas = BME280_FORCEDMODE;

	/* update value inside Dev structure */
	Dev->mode = ctrl_meas;

	/* set output pointer */
	*Mode = ctrl_meas;

	return res;
}

	/* function gets current pressure oversampling value from sensor */
int8_t BME280_GetPOvs(BME280_t *Dev, uint8_t *POvs){

	int8_t res = BME280_OK;
	uint8_t ctrl_meas = 0;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(POvs) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_NO_INIT_ERR == res) return res;

	/* read value of ctrl_meas register from sensor */
	res = Dev->driver->read(BME280_CTRL_MEAS_ADDR, &ctrl_meas, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* parse pressure oversampling value from ctrl_meas */
	ctrl_meas = (ctrl_meas >> 2) & 0x07;

	/* set output pointer */
	*POvs = ctrl_meas;

	return res;
}

	/* function gets current temperature oversampling value from sensor */
int8_t BME280_GetTOvs(BME280_t *Dev, uint8_t *TOvs){

	int8_t res = BME280_OK;
	uint8_t ctrl_meas = 0;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(TOvs) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_NO_INIT_ERR == res) return res;

	/* read value of ctrl_meas register from sensor */
	res = Dev->driver->read(BME280_CTRL_MEAS_ADDR, &ctrl_meas, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* parse temperature oversampling value from ctrl_meas */
	ctrl_meas = (ctrl_meas >> 5) & 0x07;

	/* set output pointer */
	*TOvs = ctrl_meas;

	return res;
}


	/* function gets current humidity oversampling value from sensor */
int8_t BME280_GetHOvs(BME280_t *Dev, uint8_t *HOvs){

	int8_t res = BME280_OK;
	uint8_t ctrl_hum = 0;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(HOvs) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_NO_INIT_ERR == res) return res;

	/* read value of ctrl_hum register from sensor */
	res = Dev->driver->read(BME280_CTRL_HUM_ADDR, &ctrl_hum, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* parse humidity oversampling value from ctrl_hum */
	ctrl_hum = ctrl_hum & 0x07;

	/* set output pointer */
	*HOvs = ctrl_hum;

	return res;
}

	/* function gets current standby time for normal mode */
int8_t BME280_GetTStby(BME280_t *Dev, uint8_t *TStby){

	int8_t res = BME280_OK;
	uint8_t config = 0;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(TStby) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_NO_INIT_ERR == res) return res;

	/* read value of config register from sensor */
	res = Dev->driver->read(BME280_CONFIG_ADDR, &config, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* parse standby time value from config */
	config = (config >> 5) & 0x07;

	/* set output pointer */
	*TStby = config;

	return res;
}

	/* function gets current value of IIR filter */
int8_t BME280_GetTFilter(BME280_t *Dev, uint8_t *Filter){

	int8_t res = BME280_OK;
	uint8_t config = 0;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Filter) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_NO_INIT_ERR == res) return res;

	/* read value of config register from sensor */
	res = Dev->driver->read(BME280_CONFIG_ADDR, &config, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* parse filter value from config */
	config = (config >> 2) & 0x07;

	/* set output pointer */
	*Filter = config;

	return res;
}

	/* function reads current 3-wire SPI setup (0 = 3w SPI disabled, 1 - 3w SPI enabled) */
int8_t BME280_Is3WireSPIEnabled(BME280_t *Dev, uint8_t *Result){

	int8_t res = BME280_OK;
	uint8_t config = 0;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Result) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_NO_INIT_ERR == res) return res;

	/* read value of ctrl_meas register from sensor */
	res = Dev->driver->read(BME280_CONFIG_ADDR, &config, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* parse mode values from ctrl_meas */
	config &= 0x01;

	/* set output pointer */
	*Result = config;

	return res;
}
#endif

#ifdef USE_SETTERS
	/* Function sets sensor operation mode */
int8_t BME280_SetMode(BME280_t *Dev, uint8_t Mode){

	int8_t res = BME280_OK;
	uint8_t ctrl_meas = 0, tmp = 0;

	/* check parameters */
	if( IS_NULL(Dev) || (Mode > BME280_NORMALMODE) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_NO_INIT_ERR == res) return res;

	/* read value of ctrl_meas register from sensor */
	res = Dev->driver->read(BME280_CTRL_MEAS_ADDR, &ctrl_meas, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* check if current mode differs from requested */
	tmp = ctrl_meas & 0x03;
	if(0x02 == tmp) tmp = BME280_FORCEDMODE;
	if(Mode == tmp) return BME280_OK;

	/* send new ctrl_meas value to sensor if required */
	ctrl_meas &= 0xFC;	//0xFC - 0b11111100
	ctrl_meas |= Mode;
	res = Dev->driver->write(BME280_CTRL_MEAS_ADDR, ctrl_meas, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* update value inside Dev structure */
	Dev->mode = Mode;

	return res;
}

	/* function sets pressure oversampling value  */
int8_t BME280_SetPOvs(BME280_t *Dev, uint8_t POvs){

	int8_t res = BME280_OK;
	uint8_t ctrl_meas = 0, tmp = 0;

	/* check parameters */
	if( IS_NULL(Dev) || (POvs > BME280_OVERSAMPLING_X16) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* read value of ctrl_meas register from sensor */
	res = Dev->driver->read(BME280_CTRL_MEAS_ADDR, &ctrl_meas, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* check if current value differs from requested */
	tmp = (ctrl_meas >> 2) & 0x07;
	if(POvs == tmp) return BME280_OK;

	/* send new ctrl_meas value to sensor if required */
	ctrl_meas &= 0xE3;	//0xE3 - 0b11100011
	ctrl_meas |= (POvs << 2);
	res = Dev->driver->write(BME280_CTRL_MEAS_ADDR, ctrl_meas, Dev->driver);

	return res;
}

	/* function sets temperature oversampling value  */
int8_t BME280_SetTOvs(BME280_t *Dev, uint8_t TOvs){

	int8_t res = BME280_OK;
	uint8_t ctrl_meas = 0, tmp = 0;

	/* check parameters */
	if( IS_NULL(Dev) || (TOvs > BME280_OVERSAMPLING_X16) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* read value of ctrl_meas register from sensor */
	res = Dev->driver->read(BME280_CTRL_MEAS_ADDR, &ctrl_meas, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* check if current value differs from requested */
	tmp = (ctrl_meas >> 5) & 0x07;
	if(TOvs == tmp) return BME280_OK;

	/* send new ctrl_meas value to sensor if required */
	ctrl_meas &= 0x1F;	//0x1F - 0b00011111
	ctrl_meas |= (TOvs << 5);
	res = Dev->driver->write(BME280_CTRL_MEAS_ADDR, ctrl_meas, Dev->driver);

	return res;
}

	/* function sets humidity oversampling value  */
int8_t BME280_SetHOvs(BME280_t *Dev, uint8_t HOvs){

	int8_t res = BME280_OK;
	uint8_t tmp = 0;

	/* check parameters */
	if( IS_NULL(Dev) || (HOvs > BME280_OVERSAMPLING_X16) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* send requested value to sensor */
	res = Dev->driver->write(BME280_CTRL_HUM_ADDR, HOvs, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* to make the change effective we need to write ctrl_meas register,
	 * check documentation */
	res = Dev->driver->read(BME280_CTRL_MEAS_ADDR, &tmp, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;
	res = Dev->driver->write(BME280_CTRL_MEAS_ADDR, tmp, Dev->driver);

	return res;
}

	/* function sets standby time */
int8_t BME280_SetTStby(BME280_t *Dev, uint8_t TStby){

	int8_t res = BME280_OK;
	uint8_t config = 0, tmp = 0;

	/* check parameters */
	if( IS_NULL(Dev) || (TStby > BME280_STBY_20MS) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* read value of config register from sensor */
	res = Dev->driver->read(BME280_CONFIG_ADDR, &config, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* check if current value differs from requested */
	tmp = (config >> 5) & 0x07;
	if(TStby == tmp) return BME280_OK;

	/* send new config value to sensor if required */
	config &= 0x1F;	//0x1F - 0b00011111
	config |= (TStby << 5);
	res = Dev->driver->write(BME280_CONFIG_ADDR, config, Dev->driver);

	return res;
}

	/* function sets IIR filter value */
int8_t BME280_SetFilter(BME280_t *Dev, uint8_t Filter){

	int8_t res = BME280_OK;
	uint8_t config = 0, tmp = 0;

	/* check parameters */
	if( IS_NULL(Dev) || (Filter > BME280_FILTER_16) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* read value of config register from sensor */
	res = Dev->driver->read(BME280_CONFIG_ADDR, &config, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* check if current value differs from requested */
	tmp = (config >> 2) & 0x07;
	if(Filter == tmp) return BME280_OK;

	/* send new config value to sensor if required */
	config &= 0xE3;	//0xE3 - 0b11100011
	config |= (Filter << 2);
	res = Dev->driver->write(BME280_CONFIG_ADDR, config, Dev->driver);

	return res;
}

	/* function enables 3-wire SPI interface */
int8_t BME280_Enable3WireSPI(BME280_t *Dev){

	int8_t res = BME280_OK;
	uint8_t config = 0, tmp = 0;

	/* check parameter */
	if( IS_NULL(Dev) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* read value of config register from sensor */
	res = Dev->driver->read(BME280_CONFIG_ADDR, &config, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* check if current value differs from requested */
	tmp = config & 0x01;
	if(0x01 == tmp) return BME280_OK;

	/* send new config value to sensor if required */
	config &= 0xFE;	//0xFE - 0b11111110
	config |= 0x01;
	res = Dev->driver->write(BME280_CONFIG_ADDR, config, Dev->driver);

	return res;
}

/* function disables 3-wire SPI interface */
int8_t BME280_Disable3WireSPI(BME280_t *Dev){

	int8_t res = BME280_OK;
	uint8_t config = 0, tmp = 0;

	/* check parameter */
	if( IS_NULL(Dev) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* read value of config register from sensor */
	res = Dev->driver->read(BME280_CONFIG_ADDR, &config, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* check if current value differs from requested */
	tmp = config & 0x01;
	if(0x00 == tmp) return BME280_OK;

	/* send new config value to sensor if required */
	config &= 0xFE;	//0xFE - 0b11111110
	res = Dev->driver->write(BME280_CONFIG_ADDR, config, Dev->driver);

	return res;
}
#endif

#ifdef USE_INTEGER_RESULTS
#ifdef USE_NORMAL_MODE
	/* function reads last measured values from sensor in normal mode (no floats) */
int8_t BME280_ReadAllLast(BME280_t *Dev, BME280_Data_t *Data){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t press, hum;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Data) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in normal mode */
	res = bme280_is_normal_mode(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_all, Dev, &temp, &press, &hum);
	if(BME280_OK != res) return res;

	/* convert 32bit values to Data structure */
	bme280_convert_t_S32_struct(temp, Data);
	bme280_convert_p_U32_struct(press, Data);
	bme280_convert_h_U32_struct(hum, Data);

	return res;
}

	/* function reads last measured temperature from sensor in normal mode (no floats) */
int8_t BME280_ReadTempLast(BME280_t *Dev, int8_t *TempInt, uint8_t *TempFract){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_Data_t data;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(TempInt) || IS_NULL(TempFract) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in normal mode */
	res = bme280_is_normal_mode(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_temp, Dev, &temp, 0, 0);
	if(BME280_OK != res) return res;

	/* convert 32bit values to local data structure */
	bme280_convert_t_S32_struct(temp, &data);

	/* set values of external variables */
	*TempInt = data.temp_int;
	*TempFract = data.temp_fract;

	return res;
}

	/* function reads last measured pressure from sensor in normal mode (no floats) */
int8_t BME280_ReadPressLast(BME280_t *Dev, uint16_t *PressInt, uint16_t *PressFract){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t press;
	BME280_Data_t data;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(PressInt) || IS_NULL(PressFract) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in normal mode */
	res = bme280_is_normal_mode(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_press, Dev, &temp, &press, 0);
	if(BME280_OK != res) return res;

	/* convert 32bit value to local data structure */
	bme280_convert_p_U32_struct(press, &data);

	/* set values of external variables */
	*PressInt = data.pressure_int;
	*PressFract = data.pressure_fract;

	return res;
}

	/* function reads last measured humidity from sensor in normal mode (no floats) */
int8_t BME280_ReadHumLast(BME280_t *Dev, uint8_t *HumInt, uint16_t *HumFract){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t hum;
	BME280_Data_t data;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(HumInt) || IS_NULL(HumFract) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in normal mode */
	res = bme280_is_normal_mode(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_hum, Dev, &temp, 0, &hum);
	if(BME280_OK != res) return res;

	/* convert 32bit value to local data structure */
	bme280_convert_h_U32_struct(hum, &data);

	/* set values of external variables */
	*HumInt = data.humidity_int;
	*HumFract = data.humidity_fract;

	return res;
}
#endif

#ifdef USE_FORCED_MODE
	/* function forces single measurement and reads all data (no floats) */
int8_t BME280_ReadAllForce(BME280_t *Dev, BME280_Data_t *Data){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t press, hum;
	uint8_t delay;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Data) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* force single measure */
	res = bme280_set_forced_mode(Dev, &delay);
	if(BME280_OK != res) return res;

	/* wait until it ends */
	Dev->driver->delay(delay);

	/* check if measure is completed */
	res = bme280_busy_check(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_all, Dev, &temp, &press, &hum);
	if(BME280_OK != res) return res;

	/* convert 32bit values to Data structure */
	bme280_convert_t_S32_struct(temp, Data);
	bme280_convert_p_U32_struct(press, Data);
	bme280_convert_h_U32_struct(hum, Data);

	return res;
}

	/* function forces single measurement and reads the temperature (no floats) */
int8_t BME280_ReadTempForce(BME280_t *Dev, int8_t *TempInt, uint8_t *TempFract){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_Data_t data;
	uint8_t delay;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(TempInt) || IS_NULL(TempFract) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* force single measure */
	res = bme280_set_forced_mode(Dev, &delay);
	if(BME280_OK != res) return res;

	/* wait until it ends */
	Dev->driver->delay(delay);

	/* check if measure is completed */
	res = bme280_busy_check(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_temp, Dev, &temp, 0, 0);
	if(BME280_OK != res) return res;

	/* convert 32bit values to local data structure */
	bme280_convert_t_S32_struct(temp, &data);

	/* set values of external variables */
	*TempInt = data.temp_int;
	*TempFract = data.temp_fract;

	return res;
}

	/* function forces single measurement and reads the pressure (no floats) */
int8_t BME280_ReadPressForce(BME280_t *Dev, uint16_t *PressInt, uint16_t *PressFract){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t press;
	BME280_Data_t data;
	uint8_t delay;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(PressInt) || IS_NULL(PressFract) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* force single measure */
	res = bme280_set_forced_mode(Dev, &delay);
	if(BME280_OK != res) return res;

	/* wait until it ends */
	Dev->driver->delay(delay);

	/* check if measure is completed */
	res = bme280_busy_check(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_press, Dev, &temp, &press, 0);
	if(BME280_OK != res) return res;

	/* convert 32bit value to local data structure */
	bme280_convert_p_U32_struct(press, &data);

	/* set values of external variables */
	*PressInt = data.pressure_int;
	*PressFract = data.pressure_fract;

	return res;
}

	/* function forces single measurement and reads the humidity (no floats) */
int8_t BME280_ReadHumForce(BME280_t *Dev, uint8_t *HumInt, uint16_t *HumFract){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t hum;
	BME280_Data_t data;
	uint8_t delay;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(HumInt) || IS_NULL(HumFract) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* force single measure */
	res = bme280_set_forced_mode(Dev, &delay);
	if(BME280_OK != res) return res;

	/* wait until it ends */
	Dev->driver->delay(delay);

	/* check if measure is completed */
	res = bme280_busy_check(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_hum, Dev, &temp, 0, &hum);
	if(BME280_OK != res) return res;

	/* convert 32bit value to local data structure */
	bme280_convert_h_U32_struct(hum, &data);

	/* set values of external variables */
	*HumInt = data.humidity_int;
	*HumFract = data.humidity_fract;

	return res;
}
#endif
#endif

#ifdef USE_FLOATS_RESULTS
#ifdef USE_NORMAL_MODE
	/* function reads last measured values from sensor in normal mode (with floats) */
int8_t BME280_ReadAllLast_F(BME280_t *Dev, BME280_DataF_t *Data){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t press, hum;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Data) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in normal mode */
	res = bme280_is_normal_mode(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_all, Dev, &temp, &press, &hum);
	if(BME280_OK != res) return res;

	/* convert 32bit values to Data structure */
	bme280_convert_t_S32_float(temp, &Data->temp);
	bme280_convert_p_U32_float(press, &Data->press);
	bme280_convert_h_U32_float(hum, &Data->hum);

	return res;
}

	/* function reads last measured temperature from sensor in normal mode (with floats) */
int8_t BME280_ReadTempLast_F(BME280_t *Dev, float *Temp){

	int8_t res = BME280_OK;
	BME280_S32_t temp;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Temp) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in normal mode */
	res = bme280_is_normal_mode(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_temp, Dev, &temp, 0, 0);
	if(BME280_OK != res) return res;

	/* convert 32bit value to external float */
	bme280_convert_t_S32_float(temp, Temp);

	return res;
}

	/* function reads last measured pressure from sensor in normal mode (with floats) */
int8_t BME280_ReadPressLast_F(BME280_t *Dev, float *Press){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t press;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Press) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in normal mode */
	res = bme280_is_normal_mode(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_press, Dev, &temp, &press, 0);
	if(BME280_OK != res) return res;

	/* convert 32bit value to external float */
	bme280_convert_p_U32_float(press, Press);

	return res;
}

	/* function reads last measured humidity from sensor in normal mode (with floats) */
int8_t BME280_ReadHumLast_F(BME280_t *Dev, float *Hum){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t hum;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Hum) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in normal mode */
	res = bme280_is_normal_mode(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_hum, Dev, &temp, 0, &hum);
	if(BME280_OK != res) return res;

	/* convert 32bit value to external float */
	bme280_convert_h_U32_float(hum, Hum);

	return res;
}
#endif

#ifdef USE_FORCED_MODE
	/* function forces single measurement and reads the data (with floats) */
int8_t BME280_ReadAllForce_F(BME280_t *Dev, BME280_DataF_t *Data){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t press, hum;
	uint8_t delay;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Data) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	res = bme280_set_forced_mode(Dev, &delay);
	if(BME280_OK != res) return res;

	Dev->driver->delay(delay);

	res = bme280_busy_check(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_all, Dev, &temp, &press, &hum);
	if(BME280_OK != res) return res;

	/* convert 32bit values to Data structure */
	bme280_convert_t_S32_float(temp, &Data->temp);
	bme280_convert_p_U32_float(press, &Data->press);
	bme280_convert_h_U32_float(hum, &Data->hum);

	return res;
}

	/* function forces single measurement and reads the temperature (with floats) */
int8_t BME280_ReadTempForce_F(BME280_t *Dev, float *Temp){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	uint8_t delay;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Temp) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* force single measure */
	res = bme280_set_forced_mode(Dev, &delay);
	if(BME280_OK != res) return res;

	/* wait until it ends */
	Dev->driver->delay(delay);

	/* check if measure is completed */
	res = bme280_busy_check(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_temp, Dev, &temp, 0, 0);
	if(BME280_OK != res) return res;

	/* convert 32bit value to external float */
	bme280_convert_t_S32_float(temp, Temp);

	return res;
}

	/* function forces single measurement and reads the pressure (with floats) */
int8_t BME280_ReadPressForce_F(BME280_t *Dev, float *Press){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t press;
	uint8_t delay;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Press) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* force single measure */
	res = bme280_set_forced_mode(Dev, &delay);
	if(BME280_OK != res) return res;

	/* wait until it ends */
	Dev->driver->delay(delay);

	/* check if measure is completed */
	res = bme280_busy_check(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_press, Dev, &temp, &press, 0);
	if(BME280_OK != res) return res;

	/* convert 32bit value to external float */
	bme280_convert_p_U32_float(press, Press);

	return res;
}

	/* function forces single measurement and reads the humidity (with floats) */
int8_t BME280_ReadHumForce_F(BME280_t *Dev, float *Hum){

	int8_t res = BME280_OK;
	BME280_S32_t temp;
	BME280_U32_t hum;
	uint8_t delay;

	/* check parameters */
	if( IS_NULL(Dev) || IS_NULL(Hum) ) return BME280_PARAM_ERR;

	/* check if sensor is initialized and in sleep mode */
	res = bme280_is_sleep_mode(Dev);
	if(BME280_OK != res) return res;

	/* force single measure */
	res = bme280_set_forced_mode(Dev, &delay);
	if(BME280_OK != res) return res;

	/* wait until it ends */
	Dev->driver->delay(delay);

	/* check if measure is completed */
	res = bme280_busy_check(Dev);
	if(BME280_OK != res) return res;

	/* read the data from sensor */
	res = bme280_read_compensate(read_hum, Dev, &temp, 0, &hum);
	if(BME280_OK != res) return res;

	/* convert 32bit value to external float */
	bme280_convert_h_U32_float(hum, Hum);

	return res;
}
#endif
#endif

//***************************************
/* static functions */
//***************************************

	/* private function to read compensation parameters from sensor and
	 * parse them inside  BME280_t structure */
static int8_t bme280_read_compensation_parameters(BME280_t *Dev){

	uint8_t tmp_buff[32];
	int8_t res;

	/* read two calibration data's areas from sensor */
	res = Dev->driver->read(BME280_CALIB_DATA1_ADDR, &tmp_buff[0], BME280_CALIB_DATA1_LEN,
			Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;
	res = Dev->driver->read(BME280_CALIB_DATA2_ADDR, &tmp_buff[25], BME280_CALIB_DATA2_LEN,
			Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	// parse data to the structure inside Dev
	Dev->trimm.dig_T1 = CAT_UI16T(tmp_buff[1], tmp_buff[0]);
	Dev->trimm.dig_T2 = CAT_I16T(tmp_buff[3], tmp_buff[2]);
	Dev->trimm.dig_T3 = CAT_I16T(tmp_buff[5], tmp_buff[4]);

	Dev->trimm.dig_P1 = CAT_UI16T(tmp_buff[7], tmp_buff[6]);
	Dev->trimm.dig_P2 = CAT_I16T(tmp_buff[9], tmp_buff[8]);
	Dev->trimm.dig_P3 = CAT_I16T(tmp_buff[11], tmp_buff[10]);
	Dev->trimm.dig_P4 = CAT_I16T(tmp_buff[13], tmp_buff[12]);
	Dev->trimm.dig_P5 = CAT_I16T(tmp_buff[15], tmp_buff[14]);
	Dev->trimm.dig_P6 = CAT_I16T(tmp_buff[17], tmp_buff[16]);
	Dev->trimm.dig_P7 = CAT_I16T(tmp_buff[19], tmp_buff[18]);
	Dev->trimm.dig_P8 = CAT_I16T(tmp_buff[21], tmp_buff[20]);
	Dev->trimm.dig_P9 = CAT_I16T(tmp_buff[23], tmp_buff[22]);

	Dev->trimm.dig_H1 = tmp_buff[24];
	Dev->trimm.dig_H2 = CAT_I16T(tmp_buff[26], tmp_buff[25]);
	Dev->trimm.dig_H3 = tmp_buff[27];
						/*       	MSB              				LSB			       */
	Dev->trimm.dig_H4 = ( ((int16_t)tmp_buff[28] << 4) | ((int16_t)tmp_buff[29] & 0x0F) );

						/*       	MSB              				LSB			       */
	Dev->trimm.dig_H5 = ( ((int16_t)tmp_buff[30] << 4) | ((int16_t)tmp_buff[29] >> 4) );
	Dev->trimm.dig_H6 = (int8_t)tmp_buff[31];

	return BME280_OK;
}

	/* private function to read and compensate selected adc
	 * data from sensor  */
static int8_t bme280_read_compensate(uint8_t read_type, BME280_t *Dev, BME280_S32_t *temp,
		BME280_U32_t *press, BME280_U32_t *hum){

	int8_t res = BME280_OK;
	BME280_S32_t adc_T, adc_P, adc_H;
	struct adc_regs adc_raw;

	/* read selected adc data from sensor */
	switch(read_type){

	case read_temp:
		res = Dev->driver->read(BME280_TEMP_ADC_ADDR, (uint8_t *)&adc_raw.temp_raw, BME280_TEMP_ADC_LEN,
				Dev->driver);
		break;

	case read_press:
		res = Dev->driver->read(BME280_PRESS_ADC_ADDR, (uint8_t *)&adc_raw.press_raw, (BME280_PRESS_ADC_LEN +
				BME280_TEMP_ADC_LEN), Dev->driver);
		break;

	case read_hum:
		res = Dev->driver->read(BME280_TEMP_ADC_ADDR, (uint8_t *)&adc_raw.temp_raw, (BME280_TEMP_ADC_LEN +
				BME280_HUM_ADC_LEN), Dev->driver);
		break;

	case read_all:
		res = Dev->driver->read(BME280_PRESS_ADC_ADDR, (uint8_t *)&adc_raw.press_raw, (BME280_PRESS_ADC_LEN +
				BME280_TEMP_ADC_LEN + BME280_HUM_ADC_LEN), Dev->driver);
		break;

	default:
		return BME280_PARAM_ERR;
		break;
	}
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* parse  and compensate data from adc_raw structure to variables */
	adc_T = bme280_parse_press_temp_s32t((uint8_t *)&adc_raw.temp_raw);
	*temp = bme280_compensate_t_s32t(Dev, adc_T);

	if((read_press == read_type) || (read_all == read_type)){

		adc_P = bme280_parse_press_temp_s32t((uint8_t *)&adc_raw.press_raw);
		*press = bme280_compensate_p_u32t(Dev, adc_P);
	}

	if((read_hum == read_type) || (read_all == read_type)){

		adc_H = bme280_parse_hum_s32t((uint8_t *)&adc_raw.hum_raw);
		*hum = bme280_compensate_h_u32t(Dev, adc_H);
	}

	return res;
}

	/* private function that parses raw adc pressure or temp values
	 * from sensor into a single BME280_S32_t variable */
static BME280_S32_t bme280_parse_press_temp_s32t(uint8_t *raw){

	BME280_S32_t res;

	res = ((BME280_S32_t)raw[0] << 12U) | ((BME280_S32_t)raw[1] << 4U) | ((BME280_S32_t)raw[2] >> 4U);
	res &= 0xFFFFF;

	return res;
}

	/* private function that parses raw adc humidity values
	 * from sensor into a single BME280_S32_t variable */
static BME280_S32_t bme280_parse_hum_s32t(uint8_t *raw){

	BME280_S32_t res;

	res = ((BME280_S32_t)raw[0] << 8U) | ((BME280_S32_t)raw[1]);
	res &= 0xFFFF;

	return res;
}

	/* Returns temperature in DegC, resolution is 0.01 DegC. Output value of “5123”
	 * equals 51.23 DegC. t_fine carries fine temperature as global value */
static BME280_S32_t bme280_compensate_t_s32t(BME280_t *Dev, BME280_S32_t adc_T){

	BME280_S32_t var1;
	BME280_S32_t var2;
	BME280_S32_t temperature;

    var1 = (BME280_S32_t)((adc_T / 8) - ((BME280_S32_t)Dev->trimm.dig_T1 * 2));
    var1 = (var1 * ((BME280_S32_t)Dev->trimm.dig_T2)) / 2048;
    var2 = (BME280_S32_t)((adc_T / 16) - ((BME280_S32_t)Dev->trimm.dig_T1));
    var2 = (((var2 * var2) / 4096) * ((BME280_S32_t)Dev->trimm.dig_T3)) / 16384;
    Dev->t_fine = var1 + var2;
    temperature = (Dev->t_fine * 5 + 128) / 256;

    return temperature;
}

	/* Returns pressure in Pa as unsigned 32 bit integer in Q24.8 format
	 * (24 integer bits and 8 fractional bits). Output value of “24674867”
	 * represents 24674867/256 = 96386.2 Pa = 963.862 hPa */
static BME280_U32_t bme280_compensate_p_u32t(BME280_t *Dev, BME280_S32_t adc_P){

#ifdef USE_64BIT
	BME280_S64_t var1;
	BME280_S64_t var2;
	BME280_S64_t var3;
	BME280_S64_t var4;
	BME280_U32_t pressure;

    var1 = ((BME280_S64_t)Dev->t_fine) - 128000;
    var2 = var1 * var1 * (BME280_S64_t)Dev->trimm.dig_P6;
    var2 = var2 + ((var1 * (BME280_S64_t)Dev->trimm.dig_P5) * 131072);
    var2 = var2 + (((BME280_S64_t)Dev->trimm.dig_P4) * 34359738368);
    var1 = ((var1 * var1 * (BME280_S64_t)Dev->trimm.dig_P3) / 256) + ((var1 * ((BME280_S64_t)Dev->trimm.dig_P2) * 4096));
    var3 = ((BME280_S64_t)1) * 140737488355328;
    var1 = (var3 + var1) * ((BME280_S64_t)Dev->trimm.dig_P1) / 8589934592;

    /* To avoid divide by zero exception */
    if (var1 != 0)
    {
        var4 = 1048576 - adc_P;
        var4 = (((var4 * INT64_C(2147483648)) - var2) * 3125) / var1;
        var1 = (((BME280_S64_t)Dev->trimm.dig_P9) * (var4 / 8192) * (var4 / 8192)) / 33554432;
        var2 = (((BME280_S64_t)Dev->trimm.dig_P8) * var4) / 524288;
        var4 = ((var4 + var1 + var2) / 256) + (((BME280_S64_t)Dev->trimm.dig_P7) * 16);
        pressure = (BME280_U32_t)(((var4 / 2) * 100) / 128);

    }
    else
    {
        pressure = 0;
    }

#else
	BME280_S32_t var1, var2;
	BME280_U32_t pressure;

	var1 = (((BME280_S32_t)Dev->t_fine)>>1) - (BME280_S32_t)64000;
	var2 = (((var1>>2) * (var1>>2)) >> 11 ) * ((BME280_S32_t)Dev->trimm.dig_P6);
	var2 = var2 + ((var1*((BME280_S32_t)Dev->trimm.dig_P5))<<1);
	var2 = (var2>>2)+(((BME280_S32_t)Dev->trimm.dig_P4)<<16);
	var1 = (((Dev->trimm.dig_P3 * (((var1>>2) * (var1>>2)) >> 13 )) >> 3) + ((((BME280_S32_t)Dev->trimm.dig_P2) *
	var1)>>1))>>18;
	var1 =((((32768+var1))*((BME280_S32_t)Dev->trimm.dig_P1))>>15);
	if (var1 == 0)
	{
	return 0; // avoid exception caused by division by zero
	}
	pressure = (((BME280_U32_t)(((BME280_S32_t)1048576)-adc_P)-(var2>>12)))*3125;
	if (pressure < 0x80000000)
	{
	pressure = (pressure << 1) / ((BME280_U32_t)var1);
	}
	else
	{
	pressure = (pressure / (BME280_U32_t)var1) * 2;
	}
	var1 = (((BME280_S32_t)Dev->trimm.dig_P9) * ((BME280_S32_t)(((pressure>>3) * (pressure>>3))>>13)))>>12;
	var2 = (((BME280_S32_t)(pressure>>2)) * ((BME280_S32_t)Dev->trimm.dig_P8))>>13;
	pressure = (BME280_U32_t)((BME280_S32_t)pressure + ((var1 + var2 + Dev->trimm.dig_P7) >> 4));
#endif

    return pressure;
}

	/* Returns humidity in %RH as unsigned 32bit integer in Q22.10 format (22 integer
	 * and 10 fractional bits). Output value of "47445" represents 47445/1024 = 46.333 %RH */
static BME280_U32_t bme280_compensate_h_u32t(BME280_t *Dev, BME280_S32_t adc_H){

	BME280_S32_t var1;
	BME280_S32_t var2;
	BME280_S32_t var3;
	BME280_S32_t var4;
	BME280_S32_t var5;
	BME280_U32_t humidity;

    var1 = Dev->t_fine - ((BME280_S32_t)76800);
    var2 = (BME280_S32_t)(adc_H * 16384);
    var3 = (BME280_S32_t)(((BME280_S32_t)Dev->trimm.dig_H4) * 1048576);
    var4 = ((BME280_S32_t)Dev->trimm.dig_H5) * var1;
    var5 = (((var2 - var3) - var4) + (BME280_S32_t)16384) / 32768;
    var2 = (var1 * ((BME280_S32_t)Dev->trimm.dig_H6)) / 1024;
    var3 = (var1 * ((BME280_S32_t)Dev->trimm.dig_H3)) / 2048;
    var4 = ((var2 * (var3 + (BME280_S32_t)32768)) / 1024) + (BME280_S32_t)2097152;
    var2 = ((var4 * ((BME280_S32_t)Dev->trimm.dig_H2)) + 8192) / 16384;
    var3 = var5 * var2;
    var4 = ((var3 / 32768) * (var3 / 32768)) / 128;
    var5 = var3 - ((var4 * ((BME280_S32_t)Dev->trimm.dig_H1)) / 16);
    var5 = (var5 < 0 ? 0 : var5);
    var5 = (var5 > 419430400 ? 419430400 : var5);
    humidity = (BME280_U32_t)(var5 / 4096);

    return humidity;
}

#ifdef USE_INTEGER_RESULTS
	/* function converts BME280_S32_t temperature to BME280_Data_t structure */
static void bme280_convert_t_S32_struct(BME280_S32_t temp, BME280_Data_t *data){

	data->temp_int = (BME280_S32_t)temp / 100;
	data->temp_fract = (BME280_S32_t)temp % 100;
}
#endif

#ifdef USE_FLOATS_RESULTS
	/* function converts BME280_S32_t temperature to float */
static void bme280_convert_t_S32_float(BME280_S32_t temp_in, float *temp_out){

	*temp_out= (float)temp_in / 100.0F;
}
#endif

#ifdef USE_INTEGER_RESULTS
	/* function converts BME280_U32_t pressure to BME280_Data_t structure */
static void bme280_convert_p_U32_struct(BME280_U32_t press, BME280_Data_t *data){

#ifdef USE_64BIT
	data->pressure_int = press / (BME280_U32_t)10000;
	data->pressure_fract = (press % (BME280_U32_t)10000) / (BME280_U32_t)10;
#else
	data->pressure_int = press / (BME280_U32_t)100;
	data->pressure_fract = press % (BME280_U32_t)100;
#endif
}
#endif

#ifdef USE_FLOATS_RESULTS
	/* function converts BME280_U32_t pressure to float */
static void bme280_convert_p_U32_float(BME280_U32_t press_in, float *press_out){

#ifdef USE_64BIT
	*press_out = (float)press_in / 10000.0F;
#else
	*press_out  = (float)press_in / 100.0F;
#endif
}
#endif

#ifdef USE_INTEGER_RESULTS
	/* function converts BME280_U32_t humidity to BME280_Data_t structure */
static void bme280_convert_h_U32_struct(BME280_U32_t hum, BME280_Data_t *data){

	data->humidity_int = hum / (BME280_U32_t)1000;
	data->humidity_fract = hum % (BME280_U32_t)1000;
}
#endif

#ifdef USE_FLOATS_RESULTS
	/* function converts BME280_U32_t humidity to float */
static void bme280_convert_h_U32_float(BME280_U32_t hum_in, float *hum_out){

	*hum_out = (float)hum_in / 1000.0F;
}
#endif

#ifdef USE_NORMAL_MODE
	/* function checks if device was initialized and is in normal mode */
static int8_t bme280_is_normal_mode(BME280_t *Dev){

	if(not_initialized == Dev->initialized) {return BME280_NO_INIT_ERR;}

	if(normal_mode != Dev->mode) return BME280_CONDITION_ERR;

	return BME280_OK;
}
#endif

	/* function checks if device was initialized and is in sleep mode */
static int8_t bme280_is_sleep_mode(BME280_t *Dev){

	if(not_initialized == Dev->initialized) {return BME280_NO_INIT_ERR;}

	if(sleep_mode != Dev->mode) return BME280_CONDITION_ERR;

	return BME280_OK;
}

#ifdef USE_FORCED_MODE
	/* function checks and sets forced mode if possible + calculates delay */
static int8_t bme280_set_forced_mode(BME280_t *Dev, uint8_t *delay){

	int8_t res = BME280_OK;
	uint8_t buff[3];
	uint8_t mode, osrs_t, osrs_p, osrs_h;

	/* read ctrl_hum, status and ctrl_meas registers */
	res = Dev->driver->read(BME280_CTRL_HUM_ADDR, buff, 3, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* check if sensor is not busy */
	buff[1] &= 0x09; // mask bits "measuring" - bit 0 and "im_update" - bit 3 only (0x09 = 0b00001001)
	if(0 != buff[1]) return BME280_BUSY_ERR;

	/* check if sensor is in sleep mode */
	mode = buff[2] & 0x03;
	if(BME280_SLEEPMODE != mode) return BME280_CONDITION_ERR;

	/* parse oversampling values from ctrl_meas */
	osrs_p = (buff[2] >> 2) & 0x07;
	osrs_t = (buff[2] >> 5) & 0x07;
	osrs_h = buff[0] & 0x07;

	/* covert osrs_x reg values into oversampling values */
	bme280_osrs_to_oversampling(&osrs_p);
	bme280_osrs_to_oversampling(&osrs_t);
	bme280_osrs_to_oversampling(&osrs_h);

	/* calculate delay */
	*delay = ((125U + (230U * osrs_t) + ((230U * osrs_p) + 58U) + ((230U * osrs_h) + 58U)) / 100U) + 1U;

	/* set forced mode */
	buff[2] &= 0xFC;	///0xFC - 0b11111100
	buff[2] |= BME280_FORCEDMODE;
	res = Dev->driver->write(BME280_CTRL_MEAS_ADDR, buff[2], Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	return res;
}

	/* convers osrs_x register value to oversampling value */
static void bme280_osrs_to_oversampling(uint8_t *osrs){

	if(*osrs <= BME280_OVERSAMPLING_X2) return;				// here reg value = ovs value, no need to change
	else if(BME280_OVERSAMPLING_X4 == *osrs) *osrs = 4U;	// set ovs = 4
	else if(BME280_OVERSAMPLING_X8 == *osrs) *osrs = 8U;	// set ovs = 8
	else *osrs = 16U;										// any other case set ovs max = 16
}

	/* checks sensor's status */
static int8_t bme280_busy_check(BME280_t *Dev){

	int8_t res = BME280_OK;
	uint8_t status;

	/* read status register */
	res = Dev->driver->read(BME280_STATUS_ADDR, &status, 1, Dev->driver);
	if(BME280_OK != res) return BME280_INTERFACE_ERR;

	/* check if both bits are not set */
	status &= 0x09; // mask bits "measuring" - bit 0 and "im_update" - bit 3 only (0x09 = 0b00001001)
	if(0 != status) return BME280_BUSY_ERR;

	return res;
}
#endif // USE_FORCED_MODE
///@}

#endif // USE_BME280_SPI
