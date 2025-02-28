/**
 *******************************************
 * @file    bme280.h
 * @author  Łukasz Juraszek / JuraszekL
 * @date	20.04.2023
 * @brief   Header for BME280 Driver
 * @note 	https://github.com/JuraszekL/BME280_Driver
 *******************************************
*/

/**
 * @addtogroup BME280_Driver
 * @{
 */

//***************************************

#ifndef BME280_H
#define BME280_H

//***************************************

#ifdef __cplusplus /* CPP */
extern "C" {
#endif

//***************************************

#include "bme280_definitions.h"

/**
 * @defgroup BME280_libconf Library Configuration
 * @brief Set library options here
 * @{
 */
/// comment this line if you don't want to use 64bit variables in calculations
#define USE_64BIT
/// comment this line if you don't need to use functions with floating point results
#define USE_FLOATS_RESULTS
/// comment this line if you don't need to use functions with integer results
#define USE_INTEGER_RESULTS
/// comment this line if you don't need to read single setting with any getX function
#define USE_GETTERS
/// comment this line if you don't need to write single setting with any setX function
#define USE_SETTERS
/// comment this line if you don't use functionns to read data in normal mode (BME280_ReadxxxLast/BME280_ReadxxxLast_F)
#define USE_NORMAL_MODE
/// comment this line if you don't use functionns to read data in forced mode (BME280_ReadxxxForce/BME280_ReadxxxForce_F)
#define USE_FORCED_MODE
///@}

float combineToFloat(int32_t integerPart, int32_t fractionalPart);

/**
 * @defgroup BME280_Pubfunc Public functions
 * @brief Use these functions only
 * @{
 */

/**
 * @brief Function to initialize sensor and resources
 * @note This must be a first function usend in code before any other opearion can be performed!
 *
 * Init funtion performs sensor reset and checks #BME280_ID. It doesn't set any sensor's parameters. Calibration
 * data specific for each one sensor are read while Init function. If operation is completed with
 * success function sets "initialized" value in #BME280_t structure.
 * @param[in] *Dev pointer to #BME280_t structure which should be initialized
 * @param[in] *Driver pointer to BME280_Driver_t structure where all platform specific data are stored. This structure
 * MUST exist while program is running - do not use local structures to init sensor!
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_ID_ERR sensor's id doesnt match with #BME280_ID
 */
int8_t BME280_Init(BME280_t *Dev, BME280_Driver_t *Driver);

/**
 * @brief Function to perform sensor's software reset
 *
 * Function sends #BME280_RESET_VALUE to #BME280_RESET_ADDR. To perform this operation #bme280_writeregister
 * function must be set inside #BME280_t *Dev structure. Function sets "sleep_mode" inside *Dev structure after reset.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed or #bme280_writeregister function is not set
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 */
int8_t BME280_Reset(BME280_t *Dev);

int8_t bme280_read_platform_spec(uint8_t reg_addr, uint8_t *rxbuff, uint8_t rxlen, void *driver);
int8_t bme280_write_platform_spec(uint8_t reg_addr, uint8_t value, void *driver);
void bme280_delay_platform_spec(uint32_t delay_time);

#ifdef USE_GETTERS
/**
 * @defgroup BME280_getfunctions Get Functions
 * @brief Read sensor's settings
 * @note #USE_GETTERS in @ref BME280_libconf must be uncommented tu use these functions
 * @{
 */

/**
 * @brief Function gets current @ref BME280_mode from sensor
 *
 * Function updates current operating mode inside *Dev structure as well.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Mode pointer to vartiable where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 */
int8_t BME280_GetMode(BME280_t *Dev, uint8_t *Mode);

/**
 * @brief Function gets current pressure @ref BME280_Ovs from sensor
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *POvs pointer to vartiable where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 */
int8_t BME280_GetPOvs(BME280_t *Dev, uint8_t *POvs);

/**
 * @brief Function gets current temperature @ref BME280_Ovs from sensor
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *TOvs pointer to vartiable where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 */
int8_t BME280_GetTOvs(BME280_t *Dev, uint8_t *TOvs);

/**
 * @brief Function gets current humidity @ref BME280_Ovs from sensor
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *HOvs pointer to vartiable where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 */
int8_t BME280_GetHOvs(BME280_t *Dev, uint8_t *HOvs);

/**
 * @brief Function gets current @ref BME280_tstby from sensor
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *TStby pointer to vartiable where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 */
int8_t BME280_GetTStby(BME280_t *Dev, uint8_t *TStby);

/**
 * @brief Function gets current @ref BME280_filter coeficient from sensor
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Filter pointer to vartiable where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 */
int8_t BME280_GetTFilter(BME280_t *Dev, uint8_t *Filter);

/**
 * @brief Function checks if 3-wire SPI is enabled
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Result pointer to vartiable where result will be stored, 0 - disabled, 1 - enabled
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 */
int8_t BME280_Is3WireSPIEnabled(BME280_t *Dev, uint8_t *Result);
///@}
#endif

/**
 * @brief Function to set all sensor settings at once
 * @note Sensor must be in #BME280_SLEEPMODE to set all parameters. Force #BME280_SLEEPMODE with #BME280_SetMode
 * function before or use it directly after #BME280_Init function only.
 *
 * Function writes all 3 config registers without reading them before. It can be usefull after power-up or reset.
 * It sets current operating mode inside *Dev structure at the end.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[in] *Config pointer to #BME280_Config_t structure which contains all paramaters to be set
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 */
int8_t BME280_ConfigureAll(BME280_t *Dev, BME280_Config_t *Config);

#ifdef USE_SETTERS
/**
 * @defgroup BME280_setfunctions Set Functions
 * @brief change sensor's settings
 * @note #USE_SETTERS in @ref BME280_libconf must be uncommented to use these functions
 * @note Sensor must be in #BME280_SLEEPMODE to change settings. Only #BME280_SetMode function can be used when
 * sensors is in different working mode,
 * @{
 */

/**
 * @brief Function sets sensor's @ref BME280_mode
 *
 * Function reads single register from sensor, and checks if current mode matches mode requested by user.
 * If matches, function skips write operation and returns #BME280_OK. If doesnt, it prepares and sends new register
 * value, then sets correct operating mode inside *Dev structure.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[in] Mode value to be set, must be in range of @ref BME280_mode
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 */
int8_t BME280_SetMode(BME280_t *Dev, uint8_t Mode);

/**
 * @brief Function sets sensor's pressure @ref BME280_Ovs
 * @note Sensor must be in #BME280_SLEEPMODE to set this parameter. Force #BME280_SLEEPMODE with #BME280_SetMode
 * function before or use it directly after #BME280_Init function only.
 *
 * Function reads single register from sensor, and checks if current value matches value requested by user.
 * If matches, function skips write operation and returns #BME280_OK. If doesnt, it prepares and sends new register
 * value.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[in] POvs value to be set, must be in range of @ref BME280_Ovs
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 */
int8_t BME280_SetPOvs(BME280_t *Dev, uint8_t POvs);

/**
 * @brief Function sets sensor's temperature @ref BME280_Ovs
 * @note Sensor must be in #BME280_SLEEPMODE to set this parameter. Force #BME280_SLEEPMODE with #BME280_SetMode
 * function before or use it directly after #BME280_Init function only.
 *
 * Function reads single register from sensor, and checks if current value matches value requested by user.
 * If matches, function skips write operation and returns #BME280_OK. If doesnt, it prepares and sends new register
 * value.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[in] TOvs value to be set, must be in range of @ref BME280_Ovs
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 */
int8_t BME280_SetTOvs(BME280_t *Dev, uint8_t TOvs);

/**
 * @brief Function sets sensor's humidity @ref BME280_Ovs
 * @note Sensor must be in #BME280_SLEEPMODE to set this parameter. Force #BME280_SLEEPMODE with #BME280_SetMode
 * function before or use it directly after #BME280_Init function only.
 *
 * Function reads single register from sensor, and checks if current value matches value requested by user.
 * If matches, function skips write operation and returns #BME280_OK. If doesnt, it prepares and sends new register
 * value.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[in] HOvs value to be set, must be in range of @ref BME280_Ovs
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 */
int8_t BME280_SetHOvs(BME280_t *Dev, uint8_t HOvs);

/**
 * @brief Function sets sensor's @ref BME280_tstby
 * @note Sensor must be in #BME280_SLEEPMODE to set this parameter. Force #BME280_SLEEPMODE with #BME280_SetMode
 * function before or use it directly after #BME280_Init function only.
 *
 * Function reads single register from sensor, and checks if current value matches value requested by user.
 * If matches, function skips write operation and returns #BME280_OK. If doesnt, it prepares and sends new register
 * value.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[in] TStby value to be set, must be in range of @ref BME280_tstby
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 */
int8_t BME280_SetTStby(BME280_t *Dev, uint8_t TStby);

/**
 * @brief Function sets sensor's @ref BME280_filter coeficient
 * @note Sensor must be in #BME280_SLEEPMODE to set this parameter. Force #BME280_SLEEPMODE with #BME280_SetMode
 * function before or use it directly after #BME280_Init function only.
 *
 * Function reads single register from sensor, and checks if current value matches value requested by user.
 * If matches, function skips write operation and returns #BME280_OK. If doesnt, it prepares and sends new register
 * value.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[in] Filter value to be set, must be in range of @ref BME280_filter
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 */
int8_t BME280_SetFilter(BME280_t *Dev, uint8_t Filter);

/**
 * @brief Function enables 3-wire SPI bus
 * @note Sensor must be in #BME280_SLEEPMODE to set this parameter. Force #BME280_SLEEPMODE with #BME280_SetMode
 * function before or use it directly after #BME280_Init function only.
 *
 * Function reads single register from sensor, and checks if current value matches value requested by user.
 * If matches, function skips write operation and returns #BME280_OK. If doesnt, it prepares and sends new register
 * value.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 */
int8_t BME280_Enable3WireSPI(BME280_t *Dev);

/**
 * @brief Function disables 3-wire SPI bus
 * @note Sensor must be in #BME280_SLEEPMODE to set this parameter. Force #BME280_SLEEPMODE with #BME280_SetMode
 * function before or use it directly after #BME280_Init function only.
 *
 * Function reads single register from sensor, and checks if current value matches value requested by user.
 * If matches, function skips write operation and returns #BME280_OK. If doesnt, it prepares and sends new register
 * value.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 */
int8_t BME280_Disable3WireSPI(BME280_t *Dev);
///@}
#endif

#ifdef USE_INTEGER_RESULTS
/**
 * @defgroup BME280_readnofl Read Functions (int)
 * @brief read measured data from sensor as integers
 * @note #USE_INTEGER_RESULTS in @ref BME280_libconf must be uncommented to use these functions
 * @{
 */

#ifdef USE_NORMAL_MODE
/**
 * @defgroup BME280_readnormalmodei Read in normal mode
 * @brief read last measured data from sensor in normal mode
 * @note #USE_NORMAL_MODE in @ref BME280_libconf must be uncommented to use these functions
 * @{
 */

/**
 * @brief Function reads all measured data at once
 * @note Sensor must be in #BME280_NORMALMODE to read last measured values.
 *
 * Function reads all adc values from sensor, converts them into single variables and compensate
 * with use #BME280_calibration_data. Compensated values are then converted into #BME280_Data_t structure.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Data pointer to structure where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_NORMALMODE
 */
int8_t BME280_ReadAllLast(BME280_t *Dev, BME280_Data_t *Data);

/**
 * @brief Function reads last measured temperature
 * @note Sensor must be in #BME280_NORMALMODE to read last measured values.
 *
 * Function reads temperature related adc values from sensor, converts them into single variable and compensate
 * with use #BME280_calibration_data. Compensated value is then converted into integer part and fractial part and
 * stored in external variables
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *TempInt pointer to variable where integer part of temperature will be stored
 * @param[out] *TempFract pointer to variable where fractial part of temperature will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_NORMALMODE
 */
int8_t BME280_ReadTempLast(BME280_t *Dev, int8_t *TempInt, uint8_t *TempFract);

/**
 * @brief Function reads last measured pressure
 * @note Sensor must be in #BME280_NORMALMODE to read last measured values.
 *
 * Function reads temperature and pressure related adc values from sensor, converts them into single variables and compensate
 * with use #BME280_calibration_data. Compensated value is then converted into integer part and fractial part and
 * stored in external variables
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *PressInt pointer to variable where integer part of pressure will be stored
 * @param[out] *PressFract pointer to variable where fractial part of pressure will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_NORMALMODE
 */
int8_t BME280_ReadPressLast(BME280_t *Dev, uint16_t *PressInt, uint16_t *PressFract);

/**
 * @brief Function reads last measured humidity
 * @note Sensor must be in #BME280_NORMALMODE to read last measured values.
 *
 * Function reads temperature and humidity related adc values from sensor, converts them into single variables and compensate
 * with use #BME280_calibration_data. Compensated value is then converted into integer part and fractial part and
 * stored in external variables
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *HumInt pointer to variable where integer part of humidity will be stored
 * @param[out] *HumFract pointer to variable where fractial part of humidity will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_NORMALMODE
 */
int8_t BME280_ReadHumLast(BME280_t *Dev, uint8_t *HumInt, uint16_t *HumFract);
///@}
#endif

#ifdef USE_FORCED_MODE
/**
 * @defgroup BME280_readforcedmodei Read in forced mode
 * @brief force single measure and read data when finished
 * @note #USE_FORCED_MODE in @ref BME280_libconf must be uncommented to use these functions
 * @{
 */

/**
 * @brief Function forces single measure and returns all data as integers
 * @note Sensor must be in #BME280_SLEEPMODE to force a single measurement.
 *
 * Function reads sensor's configuration to check conditions and calculate max. delay time
 * required for measure cycle. Then sends command to force single measurement and calls used-defined
 * delay function. When delay function returns ir reads all adc values from sensor, converts them into
 * single variables and compensate with use #BME280_calibration_data. Compensated values are then
 * converted into #BME280_Data_t structure.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Data pointer to structure where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 * @return #BME280_BUSY_ERR sensor is busy so cannot proceed
 */
int8_t BME280_ReadAllForce(BME280_t *Dev, BME280_Data_t *Data);

/**
 * @brief Function forces single measure and returns temperatrue as integers (forced mode)
 * @note Sensor must be in #BME280_SLEEPMODE to force a single measurement.
 *
 * Function reads sensor's configuration to check conditions and calculate max. delay time
 * required for measure cycle. Then sends command to force single measurement and calls used-defined
 * delay function. When delay function returns ir reads temperature related adc values from sensor, converts them into
 * single variables and compensate with use #BME280_calibration_data. Compensated values are then
 * converted into integer (*TempInt) and fractial part (*TempFract).
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *TempInt pointer to variable where integer part of temperature will be stored
 * @param[out] *TempFract pointer to variable where fractial part of temperature will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 * @return #BME280_BUSY_ERR sensor is busy so cannot proceed
 */
int8_t BME280_ReadTempForce(BME280_t *Dev, int8_t *TempInt, uint8_t *TempFract);

/**
 * @brief Function forces single measure and returns pressure as integers (forced mode)
 * @note Sensor must be in #BME280_SLEEPMODE to force a single measurement.
 *
 * Function reads sensor's configuration to check conditions and calculate max. delay time
 * required for measure cycle. Then sends command to force single measurement and calls used-defined
 * delay function. When delay function returns ir reads temperarure and pressure related adc values from sensor,
 * converts them into single variables and compensate with use #BME280_calibration_data. Compensated values are
 * then converted into integer (*PressInt) and fractial part (*PressFract).
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *PressInt pointer to variable where integer part of pressure will be stored
 * @param[out] *PressFract pointer to variable where fractial part of pressure will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 * @return #BME280_BUSY_ERR sensor is busy so cannot proceed
 */
int8_t BME280_ReadPressForce(BME280_t *Dev, uint16_t *PressInt, uint16_t *PressFract);

/**
 * @brief Function forces single measure and returns humidity as integers (forced mode)
 * @note Sensor must be in #BME280_SLEEPMODE to force a single measurement.
 *
 * Function reads sensor's configuration to check conditions and calculate max. delay time
 * required for measure cycle. Then sends command to force single measurement and calls used-defined
 * delay function. When delay function returns ir reads temperarure and humidity related adc values from sensor,
 * converts them into single variables and compensate with use #BME280_calibration_data. Compensated values are
 * then converted into integer (*HumInt) and fractial part (*HumFract).
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *HumInt pointer to variable where integer part of humidity will be stored
 * @param[out] *HumFract pointer to variable where fractial part of humidity will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 * @return #BME280_BUSY_ERR sensor is busy so cannot proceed
 */
int8_t BME280_ReadHumForce(BME280_t *Dev, uint8_t *HumInt, uint16_t *HumFract);
///@}
#endif
///@}
#endif

#ifdef USE_FLOATS_RESULTS
/**
 * @defgroup BME280_readfl Read Functions (float)
 * @brief read measured data from sensor as floating point values
 * @note #USE_FLOATS_RESULTS in @ref BME280_libconf must be uncommented to use these functions
 * @{
 */

#ifdef USE_NORMAL_MODE
/**
 * @defgroup BME280_readnormalmodef Read in normal mode
 * @brief read last measured data from sensor in normal mode
 * @note #USE_NORMAL_MODE in @ref BME280_libconf must be uncommented to use these functions
 * @{
 */

/**
 * @brief Function reads all measured data at once
 * @note Sensor must be in #BME280_NORMALMODE to read last measured values.
 *
 * Function reads all adc values from sensor, converts them into single variables and compensate
 * with use #BME280_calibration_data. Compensated values are then converted into #BME280_DataF_t structure.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Data pointer to structure where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_NORMALMODE
 */
int8_t BME280_ReadAllLast_F(BME280_t *Dev, BME280_DataF_t *Data);

/**
 * @brief Function reads last measured temperature
 * @note Sensor must be in #BME280_NORMALMODE to read last measured values.
 *
 * Function reads temperature related adc values from sensor, converts them into single variable and compensate
 * with use #BME280_calibration_data. Compensated value is then converted into floating point value and stored in
 * external variable
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Temp pointer to variable where temperature will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_NORMALMODE
 */
int8_t BME280_ReadTempLast_F(BME280_t *Dev, float *Temp);

/**
 * @brief Function reads last measured pressure
 * @note Sensor must be in #BME280_NORMALMODE to read last measured values.
 *
 * Function reads temperature and pressure related adc values from sensor, converts them into single variables and compensate
 * with use #BME280_calibration_data. Compensated value is then converted into floating point value and stored in
 * external variable
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Press pointer to variable where temperature will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_NORMALMODE
 */
int8_t BME280_ReadPressLast_F(BME280_t *Dev, float *Press);

/**
 * @brief Function reads last measured humidity
 * @note Sensor must be in #BME280_NORMALMODE to read last measured values.
 *
 * Function reads temperature and humidity related adc values from sensor, converts them into single variables and compensate
 * with use #BME280_calibration_data. Compensated value is then converted into floating point value and stored in
 * external variable
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Hum pointer to variable where temperature will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_NORMALMODE
 */
int8_t BME280_ReadHumLast_F(BME280_t *Dev, float *Hum);
/// @}
#endif

#ifdef USE_FORCED_MODE
/**
 * @defgroup BME280_readforcedmodef Read in forced mode
 * @brief force single measure and read data when finished
 * @note #USE_FORCED_MODE in @ref BME280_libconf must be uncommented to use these functions
 * @{
 */

/**
 * @brief Function forces single measure and returns all data as floats
 * @note Sensor must be in #BME280_SLEEPMODE to force a single measurement.
 *
 * Function reads sensor's configuration to check conditions and calculate max. delay time
 * required for measure cycle. Then sends command to force single measurement and calls used-defined
 * delay function. When delay function returns ir reads all adc values from sensor, converts them into
 * single variables and compensate with use #BME280_calibration_data. Compensated values are then
 * converted into #BME280_DataF_t structure.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Data pointer to structure where result will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 * @return #BME280_BUSY_ERR sensor is busy so cannot proceed
 */
int8_t BME280_ReadAllForce_F(BME280_t *Dev, BME280_DataF_t *Data);

/**
 * @brief Function forces single measure and returns temperatrue as floats (forced mode)
 * @note Sensor must be in #BME280_SLEEPMODE to force a single measurement.
 *
 * Function reads sensor's configuration to check conditions and calculate max. delay time
 * required for measure cycle. Then sends command to force single measurement and calls used-defined
 * delay function. When delay function returns ir reads temperature related adc values from sensor, converts them into
 * single variables and compensate with use #BME280_calibration_data. Compensated values are then
 * converted into float.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Temp pointer to variable where temperature will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 * @return #BME280_BUSY_ERR sensor is busy so cannot proceed
 */
int8_t BME280_ReadTempForce_F(BME280_t *Dev, float *Temp);

/**
 * @brief Function forces single measure and returns pressure as floats (forced mode)
 * @note Sensor must be in #BME280_SLEEPMODE to force a single measurement.
 *
 * Function reads sensor's configuration to check conditions and calculate max. delay time
 * required for measure cycle. Then sends command to force single measurement and calls used-defined
 * delay function. When delay function returns ir reads temperature and pressure related adc values
 * from sensor, converts them into single variables and compensate with use #BME280_calibration_data.
 * Compensated values are then converted into float.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Press pointer to variable where pressure will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 * @return #BME280_BUSY_ERR sensor is busy so cannot proceed
 */
int8_t BME280_ReadPressForce_F(BME280_t *Dev, float *Press);

/**
 * @brief Function forces single measure and returns humidity as floats (forced mode)
 * @note Sensor must be in #BME280_SLEEPMODE to force a single measurement.
 *
 * Function reads sensor's configuration to check conditions and calculate max. delay time
 * required for measure cycle. Then sends command to force single measurement and calls used-defined
 * delay function. When delay function returns ir reads temperature and humidity related adc values
 * from sensor, converts them into single variables and compensate with use #BME280_calibration_data.
 * Compensated values are then converted into float.
 * @param[in] *Dev pointer to sensor's #BME280_t structure
 * @param[out] *Hum pointer to variable where humidity will be stored
 * @return #BME280_OK success
 * @return #BME280_PARAM_ERR wrong parameter passed
 * @return #BME280_INTERFACE_ERR user defined read/write function returned non-zero value
 * @return #BME280_NO_INIT_ERR sensor was not initialized before
 * @return #BME280_CONDITION_ERR sensor is not in #BME280_SLEEPMODE
 * @return #BME280_BUSY_ERR sensor is busy so cannot proceed
 */
int8_t BME280_ReadHumForce_F(BME280_t *Dev, float *Hum);
///@}
#endif
///@}
#endif
///@}


//***************************************

#ifdef __cplusplus
}
#endif /* CPP */

//***************************************

#endif /* BME280_H */

///@}
