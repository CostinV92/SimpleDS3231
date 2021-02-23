/* 
 * Simple Arduino driver for DS3231 RTC module.
 *
 * Written by Victor Gabriel Costin.
 * Licensed under the MIT license.
 */

#include "SimpleDS3231.h"
#include "include/ds3231.h"

/* Indexes of internal data array */
#define DATA_SEC            0
#define DATA_MIN            1
#define DATA_HOU            2
#define DATA_DAY            3
#define DATA_DAT            4
#define DATA_MON            5
#define DATA_YEAR           6

/* Bit defines for the hour raw data */
#define HOU_TENS_1          4
#define HOU_TENS_2          5
#define HOU_AM_PM           5
#define HOU_FORMAT          6

/* Get macros */
#define GET_SEC_DATA()      _get_data_reg(DS3231_SEC_REG, 1)
#define GET_MIN_DATA()      _get_data_reg(DS3231_MIN_REG, 1);
#define GET_HOU_DATA()      _get_data_reg(DS3231_HOU_REG, 1);
#define GET_DAT_DATA()      _get_data_reg(DS3231_DAT_REG, 1);
#define GET_MON_DATA()      _get_data_reg(DS3231_MON_REG, 1);
#define GET_YEAR_DATA()     _get_data_reg(DS3231_YEAR_REG, 1);
#define GET_ALL_DATA()      _get_data_reg(DS3231_SEC_REG, DS3231_NO_DATA_REG);

/* Decode macros */
#define DECODE_SEC()        do { _sec = _decode_gen(_raw_data[DATA_SEC]); } while (0);
#define DECODE_MIN()        do { _min = _decode_gen(_raw_data[DATA_MIN]); } while (0);
#define DECODE_HOU()        do { _decode_hou(); } while (0);

#define DECODE_DAT()        do { _dat = _decode_gen(_raw_data[DATA_DAT]); } while (0);
#define DECODE_MON()        do { _mon = _decode_gen(_raw_data[DATA_MON]); } while (0);
#define DECODE_YEAR()       do { _year = 2000 + _decode_gen(_raw_data[DATA_YEAR]); } while (0);

/* Miscellaneous */
#define MASK_BIT(bit)       (1 << bit)
#define SET_BIT(x, bit)     (x |= (1 << bit))
#define UNSET_BIT(x, bit)   (x &= (~(1 << bit)))
#define LSB_HALF(x)         (x & 0xF)
#define MSB_HALF(x)         ((x & (0xF << 4)) >> 4)
#define COMBINE(x, y)       ((uint8_t)((x << 4) | (y)))

#define ASCII_OFFSET        48

SimpleI2CInterface::SimpleI2CInterface() {}

void SimpleI2CInterface::begin()
{
    digitalWrite(SDA, HIGH);
    digitalWrite(SCL, HIGH);

    UNSET_BIT(TWSR, TWPS0);
    UNSET_BIT(TWSR, TWPS1);

    TWBR = ((F_CPU / 400000L) - 16) / 2;
}

inline void SimpleI2CInterface::_send_start()
{

    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWSTA) | MASK_BIT(TWEN);

    _wait_int();
}

inline void SimpleI2CInterface::_send_stop()
{
    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWSTO) | MASK_BIT(TWEN);
}

inline void SimpleI2CInterface::_send_byte(uint8_t byte)
{
    TWDR = byte;

    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWEN);

    _wait_int();
}

inline uint8_t SimpleI2CInterface::_recv_byte_ack()
{
    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWEA) | MASK_BIT(TWEN);

    _wait_int();

    return TWDR;
}

inline uint8_t SimpleI2CInterface::_recv_byte_nack()
{
    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWEN);

    _wait_int();

    return TWDR;
}

inline void SimpleI2CInterface::_wait_int()
{
    while (!(TWCR & MASK_BIT(TWINT))) {}
    /* TODO(victor): check status */
}

SimpleDS3231::SimpleDS3231() {}

void SimpleDS3231::_get_data_reg(uint8_t reg, uint8_t n_regs)
{
    int i = 0;

    /* Set starting register */
    _send_start();
    _send_byte(DS3231_WRITE_ADDR);
    _send_byte(reg);

    /* Start receiving data */
    _send_start();
    _send_byte(DS3231_READ_ADDR);

    if (n_regs != 1) {
        for (i = reg; i < reg + n_regs - 1; i++)
            _raw_data[i] =_recv_byte_ack();
        _raw_data[i] = _recv_byte_nack();
    } else {
        _raw_data[reg] = _recv_byte_nack();
    }

    _send_stop();
}

inline void SimpleDS3231::_format_time_string()
{
    /* Format time string */
    _time_str[2] = _time_str[5] = ':';
    _time_str[1] = LSB_HALF(_raw_data[DATA_HOU]) + ASCII_OFFSET;
    _time_str[3] = MSB_HALF(_raw_data[DATA_MIN]) + ASCII_OFFSET;
    _time_str[4] = LSB_HALF(_raw_data[DATA_MIN]) + ASCII_OFFSET;
    _time_str[6] = MSB_HALF(_raw_data[DATA_SEC]) + ASCII_OFFSET;
    _time_str[7] = LSB_HALF(_raw_data[DATA_SEC]) + ASCII_OFFSET;

    if (_12_format) {
        _time_str[0] = MSB_HALF(_raw_data[DATA_HOU] & MASK_BIT(HOU_TENS_1)) + ASCII_OFFSET;
        if (_is_pm)
            _time_str[9] = 'P';
        else
            _time_str[9] = 'A';
        _time_str[10] = 'M';
    } else {
        _time_str[0] = MSB_HALF((_raw_data[DATA_HOU] & MASK_BIT(HOU_TENS_1)) |
                                (_raw_data[DATA_HOU] & MASK_BIT(HOU_TENS_2))) + ASCII_OFFSET;
        _time_str[8] = '\0';
    }
}

inline void SimpleDS3231::_format_date_string()
{
    int year_aux = 0, aux = 0;

    _date_str[2] = _date_str[5] = '.';
    _date_str[10] = '\0';
    _date_str[0] = MSB_HALF(_raw_data[DATA_DAT]) + ASCII_OFFSET;
    _date_str[1] = LSB_HALF(_raw_data[DATA_DAT]) + ASCII_OFFSET;
    _date_str[3] = MSB_HALF(_raw_data[DATA_MON]) + ASCII_OFFSET;
    _date_str[4] = LSB_HALF(_raw_data[DATA_MON]) + ASCII_OFFSET;
    
    year_aux = _year;
    while (year_aux >= 1000) {
        aux++;
        year_aux -= 1000;
    }
    _date_str[6] = aux + ASCII_OFFSET;

    aux = 0;
    while (year_aux >= 100) {
        aux++;
        year_aux -= 100;
    }
    _date_str[7] = aux + ASCII_OFFSET;

    aux = 0;
    while (year_aux >= 10) {
        aux++;
        year_aux -= 10;
    }
    _date_str[8] = aux + ASCII_OFFSET;
    _date_str[9] = year_aux + ASCII_OFFSET;
}

inline uint8_t SimpleDS3231::_decode_gen(uint8_t raw_data)
{
    return MSB_HALF(raw_data) * 10 + LSB_HALF(raw_data);
}

inline void SimpleDS3231::_decode_hou()
{
    _12_format = _raw_data[DATA_HOU] & MASK_BIT(HOU_FORMAT);
    if (_12_format) {
        _is_pm = _raw_data[DATA_HOU] & MASK_BIT(HOU_AM_PM);
        _hou = MSB_HALF(_raw_data[DATA_HOU] & MASK_BIT(HOU_TENS_1)) * 10 +
                LSB_HALF(_raw_data[DATA_HOU]);
    } else {
        _hou = MSB_HALF((_raw_data[DATA_HOU] & MASK_BIT(HOU_TENS_1)) |
                        (_raw_data[DATA_HOU] & MASK_BIT(HOU_TENS_2))) * 10 +
                            LSB_HALF(_raw_data[DATA_HOU]);
    }
}

uint8_t SimpleDS3231::get_sec()
{
    GET_SEC_DATA();
    DECODE_SEC();

    return _sec;
}

uint8_t SimpleDS3231::get_min()
{
    GET_MIN_DATA();
    DECODE_MIN();

    return _min;
}

uint8_t SimpleDS3231::get_hou()
{
    GET_HOU_DATA();
    DECODE_HOU();

    return _hou;
}

const char* SimpleDS3231::get_time_str()
{
    GET_ALL_DATA();
    DECODE_SEC();
    DECODE_MIN();
    DECODE_HOU();
    _format_time_string();

    return _time_str;
}

uint8_t SimpleDS3231::get_dat()
{
    GET_DAT_DATA();
    DECODE_DAT();

    return _dat;
}

uint8_t SimpleDS3231::get_mon()
{
    GET_MON_DATA();
    DECODE_MON();

    return _mon;
}

int SimpleDS3231::get_year()
{
    GET_YEAR_DATA();
    DECODE_YEAR();

    return _year;
}

const char* SimpleDS3231::get_date_str()
{
    GET_ALL_DATA();
    DECODE_DAT();
    DECODE_MON();
    DECODE_YEAR();
    _format_date_string();

    return _date_str;
}
