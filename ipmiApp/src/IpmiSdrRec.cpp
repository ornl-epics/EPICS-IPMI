/**
 * 
 * 
 * 
 * 
*/

#include "IpmiSdrRec.h"

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

double IpmiSdrRec::scale(ipmi_sdr_ctx_t sdr, uint64_t rawVal) const
{
    const common::buffer<uint8_t, IPMI_SDR_MAX_RECORD_LENGTH> &data = record_data;
    uint8_t sensor_units_percentage = 0;
    uint8_t sensor_units_modifier = 0;
    uint8_t sensor_units_rate = 0;
    uint8_t sensor_base_unit_type = 0;
    uint8_t sensor_modifier_unit_type = 0;

    int rv = ipmi_sdr_parse_sensor_units (sdr, data.data, data.size,
        &sensor_units_percentage, &sensor_units_modifier, &sensor_units_rate,
        &sensor_base_unit_type, &sensor_modifier_unit_type);

    int8_t r_exponent = 0;
    int8_t b_exponent = 0;
    int16_t m = 0;
    int16_t b = 0;
    uint8_t linearization = 0;
    uint8_t analog_data_format = 0;
    rv = ipmi_sdr_parse_sensor_decoding_data (sdr, data.data, data.size,
        &r_exponent, &b_exponent, &m, &b,
        &linearization, &analog_data_format);

    double result = 0;

    switch (analog_data_format)
    {
    case IPMI_SDR_ANALOG_DATA_FORMAT_UNSIGNED:
        result = (double) rawVal;
        break;
    case IPMI_SDR_ANALOG_DATA_FORMAT_1S_COMPLEMENT:
        /* we don't support this type yet.*/
        break;
    case IPMI_SDR_ANALOG_DATA_FORMAT_2S_COMPLEMENT:
        /* ... */
        if(rawVal & 0x80)
        {
            uint8_t x = ((~rawVal) + 1);
            result = x * (-1.0);
        }
        else
            result = rawVal;
        break;
    case IPMI_SDR_ANALOG_DATA_FORMAT_NOT_ANALOG:
        /* Not sure what to do with this one. Nothing?*/
        break;
    default:
        break;
    }
    
    /** analog_data_format = IPMI_SDR_ANALOG_DATA_FORMAT_UNSIGNED*/
    return result * m + b;
}
