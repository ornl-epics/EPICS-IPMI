/**
 * 
 * 
 * 
 * 
*/

#include "IpmiSdrRec.h"
#include <stdexcept>
#include <cmath>

IpmiSdrRec::IpmiSdrRec(uint16_t record_id, uint8_t record_type)
    :record_id(record_id), record_type(record_type)
{

}

IpmiSdrRec::~IpmiSdrRec() {
}

uint16_t IpmiSdrRec::get_record_id() const {
    return this->record_id;
}

uint8_t IpmiSdrRec::get_record_type() const {
    return this->record_type;
}

std::string IpmiSdrRec::get_device_id_string() const {
    return this->device_id_string;
}

double IpmiSdrRec::scale_threshold(ipmi_sdr_ctx_t sdr, uint64_t rawVal) const
{
    /*
    * Get the format type and scaling values from the SDR file.
    */
    const common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> &data = record_data;
    uint8_t sensor_units_percentage = 0;
    uint8_t sensor_units_modifier = 0;
    uint8_t sensor_units_rate = 0;
    uint8_t sensor_base_unit_type = 0;
    uint8_t sensor_modifier_unit_type = 0;

    int rv = ipmi_sdr_parse_sensor_units (sdr, data.data, data.size,
        &sensor_units_percentage, &sensor_units_modifier, &sensor_units_rate,
        &sensor_base_unit_type, &sensor_modifier_unit_type);

    if(rv < 0)
    {
        throw std::runtime_error("Can't parse sensor units in SDR to scale threshold");
    }

    int8_t r_exponent = 0;
    int8_t b_exponent = 0;
    int16_t m = 0;
    int16_t b = 0;
    uint8_t linearization = 0;
    uint8_t analog_data_format = 0;
    rv = ipmi_sdr_parse_sensor_decoding_data (sdr, data.data, data.size,
        &r_exponent, &b_exponent, &m, &b,
        &linearization, &analog_data_format);

    if(rv < 0)
    {
        throw std::runtime_error("Can't parse sensor decoding data in SDR to scale threshold");
    }

    double result = 0;

    if (analog_data_format == IPMI_SDR_ANALOG_DATA_FORMAT_UNSIGNED)
        result = (double) rawVal;
    else if (analog_data_format == IPMI_SDR_ANALOG_DATA_FORMAT_1S_COMPLEMENT)
    {
        if (rawVal & 0x80)
            rawVal++;
        result = (double)((char) rawVal);
    }
    else /* analog_data_format == IPMI_SDR_ANALOG_DATA_FORMAT_2S_COMPLEMENT */
        result = (double)((char) rawVal);

    result *= (double) m;
    result += (b * pow (10, b_exponent));
    result *= pow (10, r_exponent);

    switch (linearization)
    {
        case IPMI_SDR_LINEARIZATION_LN:
        result = log (result);
        break;
        case IPMI_SDR_LINEARIZATION_LOG10:
        result = log10 (result);
        break;
        case IPMI_SDR_LINEARIZATION_LOG2:
        result = log2 (result);
        break;
        case IPMI_SDR_LINEARIZATION_E:
        result = exp (result);
        break;
        case IPMI_SDR_LINEARIZATION_EXP10:
        result = exp10 (result);
        break;
        case IPMI_SDR_LINEARIZATION_EXP2:
        result = exp2 (result);
        break;
        case IPMI_SDR_LINEARIZATION_INVERSE:
        if (result != 0.0)
            result = 1.0 / result;
        break;
        case IPMI_SDR_LINEARIZATION_SQR:
        result = pow (result, 2.0);
        break;
        case IPMI_SDR_LINEARIZATION_CUBE:
        result = pow (result, 3.0);
        break;
        case IPMI_SDR_LINEARIZATION_SQRT:
        result = sqrt (result);
        break;
        case IPMI_SDR_LINEARIZATION_CUBERT:
        result = cbrt (result);
        break;
    }
    
    return result;
}

///TODO: Combine the two scaling functions into a single local function.
/*
* So in the IPMI specification, table 43, Full Sensor Record, row where \'byte\' = 43,
* says the following about scaling the hysteresis values:
*
* "Hysteresis values are given as raw counts. That is, to find the degree of hysteresis
* in units, the value must be converted using the ‘y=Mx+B’ formula."
*
* But when you apply the 'B' it produces the wrong value. So as a workaround
* we have two scaling functions.
*/
double IpmiSdrRec::scale_hysteresis(ipmi_sdr_ctx_t sdr, uint64_t rawVal) const
{
    /*
    * Get the format type and scaling values from the SDR file.
    */
    const common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> &data = record_data;
    uint8_t sensor_units_percentage = 0;
    uint8_t sensor_units_modifier = 0;
    uint8_t sensor_units_rate = 0;
    uint8_t sensor_base_unit_type = 0;
    uint8_t sensor_modifier_unit_type = 0;

    int rv = ipmi_sdr_parse_sensor_units (sdr, data.data, data.size,
        &sensor_units_percentage, &sensor_units_modifier, &sensor_units_rate,
        &sensor_base_unit_type, &sensor_modifier_unit_type);

    if(rv < 0)
    {
        throw std::runtime_error("Can't parse sensor units in SDR to scale threshold");
    }

    int8_t r_exponent = 0;
    int8_t b_exponent = 0;
    int16_t m = 0;
    int16_t b = 0;
    uint8_t linearization = 0;
    uint8_t analog_data_format = 0;
    rv = ipmi_sdr_parse_sensor_decoding_data (sdr, data.data, data.size,
        &r_exponent, &b_exponent, &m, &b,
        &linearization, &analog_data_format);

    if(rv < 0)
    {
        throw std::runtime_error("Can't parse sensor decoding data in SDR to scale threshold");
    }

    double result = 0;

    if(analog_data_format == IPMI_SDR_ANALOG_DATA_FORMAT_UNSIGNED)
    {
        result = (double) rawVal;
    }
    else if(analog_data_format == IPMI_SDR_ANALOG_DATA_FORMAT_1S_COMPLEMENT)
    {
        /* we don't support this type yet.*/
        throw std::runtime_error("Can't scale threshold because analog data type read from SDR is 1s-complement");
    }
    else if(analog_data_format == IPMI_SDR_ANALOG_DATA_FORMAT_2S_COMPLEMENT)
    {
        /* Is the value negative or positive. Check the sign bit.*/
        const uint8_t SIGN_BIT = 0x80;
        if(rawVal & SIGN_BIT)
        {
            uint8_t x = ((~rawVal) + 1);
            result = x * (-1.0);
        }
        else
            result = rawVal;
    }
    else if(analog_data_format == IPMI_SDR_ANALOG_DATA_FORMAT_NOT_ANALOG)
    {
        /* Not sure what to do with this one. Nothing?*/
        throw std::runtime_error("Can't scale threshold because analog data type in SDR is not analog (numeric) reading");
    }
    else
    {
        /* Not sure what to do with this one. Nothing?*/
        throw std::runtime_error("Can't scale threshold because of unrecognized analog data type format");
    }
    
    return result * m;
}