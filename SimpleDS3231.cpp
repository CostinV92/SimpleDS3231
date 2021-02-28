/*
 * Simple Arduino driver for DS3231 RTC module.
 *
 * Written by Victor Gabriel Costin.
 * Licensed under the MIT license.
 */

#include <Arduino.h>
#include <util/atomic.h>

#include "SimpleDS3231.h"
#include "include/ds3231.h"

/* Indexes of internal data array */
#define DATA_SEC            0
#define DATA_MIN            1
#define DATA_HOU            2
#define DATA_DOW            3
#define DATA_DAY            4
#define DATA_MON            5
#define DATA_YEAR           6

/* Bit defines for the hour raw data */
#define HOU_TENS_1          4
#define HOU_TENS_2          5
#define HOU_AM_PM           5
#define HOU_FORMAT          6

/* Read macros */
#define READ_SEC_DATA()     _read_data_reg(DS3231_SEC_REG, 1)
#define READ_MIN_DATA()     _read_data_reg(DS3231_MIN_REG, 1);
#define READ_HOU_DATA()     _read_data_reg(DS3231_HOU_REG, 1);
#define READ_DAY_DATA()     _read_data_reg(DS3231_DAY_REG, 1);
#define READ_MON_DATA()     _read_data_reg(DS3231_MON_REG, 1);
#define READ_YEAR_DATA()    _read_data_reg(DS3231_YEAR_REG, 1);
#define READ_ALL_DATA()     _read_data_reg(DS3231_SEC_REG, DS3231_NO_DATA_REG);

/* Set macros */
#define WRITE_SEC_DATA()    _write_data_reg(DS3231_SEC_REG, 1)
#define WRITE_MIN_DATA()    _write_data_reg(DS3231_MIN_REG, 1);
#define WRITE_HOU_DATA()    _write_data_reg(DS3231_HOU_REG, 1);
#define WRITE_TIME_DATA()   _write_data_reg(DS3231_SEC_REG, 3);
#define WRITE_DAY_DATA()    _write_data_reg(DS3231_DAY_REG, 1);
#define WRITE_MON_DATA()    _write_data_reg(DS3231_MON_REG, 1);
#define WRITE_YEAR_DATA()   _write_data_reg(DS3231_YEAR_REG, 1);
#define WRITE_DATE_DATA()   _write_data_reg(DS3231_DAY_REG, 3);

/* Decode macros */
#define DECODE_SEC()        do { _sec = _decode_gen(_data_buffer[DATA_SEC]); } while (0);
#define DECODE_MIN()        do { _min = _decode_gen(_data_buffer[DATA_MIN]); } while (0);
#define DECODE_HOU()        do { _decode_hou(); } while (0);

#define DECODE_DAY()        do { _day = _decode_gen(_data_buffer[DATA_DAY]); } while (0);
#define DECODE_MON()        do { _mon = _decode_gen(_data_buffer[DATA_MON]); } while (0);
#define DECODE_YEAR()       do { _year = 2000 + _decode_gen(_data_buffer[DATA_YEAR]); } while (0);

/* Encode macros */
#define ENCODE_SEC(sec)                     do { _data_buffer[DATA_SEC] = _encode_gen(sec); } while (0);
#define ENCODE_MIN(min)                     do { _data_buffer[DATA_MIN] = _encode_gen(min); } while (0);
#define ENCODE_HOU(hou, format, is_pm)      do { _data_buffer[DATA_HOU] = _encode_hou(hou, format, is_pm); } while (0);

#define ENCODE_DAY(day)                     do { _data_buffer[DATA_DAY] = _encode_gen(day); } while (0);
#define ENCODE_MON(mon)                     do { _data_buffer[DATA_MON] = _encode_gen(mon); } while (0);
#define ENCODE_YEAR(year)                   do { _data_buffer[DATA_YEAR] = _encode_gen(year); } while (0);

/* Miscellaneous */
#define MASK_BIT(bit)       (1 << bit)
#define SET_BIT(x, bit)     (x |= (1 << bit))
#define UNSET_BIT(x, bit)   (x &= (~(1 << bit)))
#define LSB_HALF(x)         (x & 0xF)
#define MSB_HALF(x)         ((x & (0xF << 4)) >> 4)
#define COMBINE(x, y)       ((uint8_t)((x << 4) | (y)))

#define ASCII_OFFSET        48

/*
 * Uncomment the below macro to debug the lib (all functions become public).
 * The same macro must be defined in your sketch before including SimpleDS3231.h.
 */
/* #define SIMPLE_DS3231_DEBUG */

#ifdef SIMPLE_DS3231_DEBUG
    #define INLINE
#else
    #define INLINE      inline
#endif

/*
 * Global variables used by the fine clock.
 */
volatile uint32_t ms_g, us_g, ns_g, ps_g;

SimpleI2CInterface::SimpleI2CInterface() {}

void SimpleI2CInterface::begin()
{
    digitalWrite(SDA, HIGH);
    digitalWrite(SCL, HIGH);

    UNSET_BIT(TWSR, TWPS0);
    UNSET_BIT(TWSR, TWPS1);

    TWBR = ((F_CPU / 400000L) - 16) / 2;
}

INLINE void SimpleI2CInterface::_write_start()
{
    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWSTA) | MASK_BIT(TWEN);

    _wait_int();
}

INLINE void SimpleI2CInterface::_write_stop()
{
    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWSTO) | MASK_BIT(TWEN);
}

INLINE void SimpleI2CInterface::_write_byte(uint8_t byte)
{
    TWDR = byte;

    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWEN);

    _wait_int();
}

INLINE uint8_t SimpleI2CInterface::_read_byte_ack()
{
    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWEA) | MASK_BIT(TWEN);

    _wait_int();

    return TWDR;
}

INLINE uint8_t SimpleI2CInterface::_read_byte_nack()
{
    TWCR = MASK_BIT(TWINT) | MASK_BIT(TWEN);

    _wait_int();

    return TWDR;
}

INLINE void SimpleI2CInterface::_wait_int()
{
    while (!(TWCR & MASK_BIT(TWINT))) {}
    /* TODO(victor): check status */
}

SimpleDS3231::SimpleDS3231() {}

void SimpleDS3231::_read_data_reg(uint8_t reg, uint8_t n_regs)
{
    int i = 0;

    /* Set starting register */
    _write_start();
    _write_byte(DS3231_WRITE_ADDR);
    _write_byte(reg);

    /* Start receiving data */
    _write_start();
    _write_byte(DS3231_READ_ADDR);

    if (n_regs != 1) {
        for (i = reg; i < reg + n_regs - 1; i++)
            _data_buffer[i] =_read_byte_ack();
        _data_buffer[i] = _read_byte_nack();
    } else {
        _data_buffer[reg] = _read_byte_nack();
    }

    _write_stop();
}

void SimpleDS3231::_write_data_reg(uint8_t reg, uint8_t n_regs)
{
    int i = 0;

    /* Set starting register */
    _write_start();
    _write_byte(DS3231_WRITE_ADDR);
    _write_byte(reg);

    /* Send data */
    for (i = reg; i < reg + n_regs; i++)
        _write_byte(_data_buffer[i]);

    _write_stop();
}

INLINE void SimpleDS3231::_format_time_string()
{
    /* Format time string */
    _str_buffer[2] = _str_buffer[5] = ':';
    _str_buffer[1] = LSB_HALF(_data_buffer[DATA_HOU]) + ASCII_OFFSET;
    _str_buffer[3] = MSB_HALF(_data_buffer[DATA_MIN]) + ASCII_OFFSET;
    _str_buffer[4] = LSB_HALF(_data_buffer[DATA_MIN]) + ASCII_OFFSET;
    _str_buffer[6] = MSB_HALF(_data_buffer[DATA_SEC]) + ASCII_OFFSET;
    _str_buffer[7] = LSB_HALF(_data_buffer[DATA_SEC]) + ASCII_OFFSET;

    if (_12_format) {
        _str_buffer[0] = MSB_HALF(_data_buffer[DATA_HOU] & MASK_BIT(HOU_TENS_1)) + ASCII_OFFSET;
        if (_is_pm)
            _str_buffer[9] = 'P';
        else
            _str_buffer[9] = 'A';
        _str_buffer[10] = 'M';
        _str_buffer[11] = '\0';
    } else {
        _str_buffer[0] = MSB_HALF((_data_buffer[DATA_HOU] & MASK_BIT(HOU_TENS_1)) |
                                (_data_buffer[DATA_HOU] & MASK_BIT(HOU_TENS_2))) + ASCII_OFFSET;
        _str_buffer[8] = '\0';
    }
}

INLINE void SimpleDS3231::_format_date_string()
{
    int year_aux = 0, aux = 0;

    _str_buffer[2] = _str_buffer[5] = '.';
    _str_buffer[10] = '\0';
    _str_buffer[0] = MSB_HALF(_data_buffer[DATA_DAY]) + ASCII_OFFSET;
    _str_buffer[1] = LSB_HALF(_data_buffer[DATA_DAY]) + ASCII_OFFSET;
    _str_buffer[3] = MSB_HALF(_data_buffer[DATA_MON]) + ASCII_OFFSET;
    _str_buffer[4] = LSB_HALF(_data_buffer[DATA_MON]) + ASCII_OFFSET;

    year_aux = _year;
    while (year_aux >= 1000) {
        aux++;
        year_aux -= 1000;
    }
    _str_buffer[6] = aux + ASCII_OFFSET;

    aux = 0;
    while (year_aux >= 100) {
        aux++;
        year_aux -= 100;
    }
    _str_buffer[7] = aux + ASCII_OFFSET;

    aux = 0;
    while (year_aux >= 10) {
        aux++;
        year_aux -= 10;
    }
    _str_buffer[8] = aux + ASCII_OFFSET;
    _str_buffer[9] = year_aux + ASCII_OFFSET;
}

INLINE uint8_t SimpleDS3231::_decode_gen(uint8_t raw_data)
{
    return MSB_HALF(raw_data) * 10 + LSB_HALF(raw_data);
}

INLINE void SimpleDS3231::_decode_hou()
{
    _12_format = _data_buffer[DATA_HOU] & MASK_BIT(HOU_FORMAT);
    if (_12_format) {
        _is_pm = _data_buffer[DATA_HOU] & MASK_BIT(HOU_AM_PM);
        _hou = MSB_HALF(_data_buffer[DATA_HOU] & MASK_BIT(HOU_TENS_1)) * 10 +
                LSB_HALF(_data_buffer[DATA_HOU]);
    } else {
        _hou = MSB_HALF((_data_buffer[DATA_HOU] & MASK_BIT(HOU_TENS_1)) |
                        (_data_buffer[DATA_HOU] & MASK_BIT(HOU_TENS_2))) * 10 +
                            LSB_HALF(_data_buffer[DATA_HOU]);
    }
}

INLINE uint8_t SimpleDS3231::_encode_gen(uint8_t data)
{
    uint8_t msbd = 0;

    while (data >= 10) {
        msbd++;
        data -= 10;
    }

    return COMBINE(msbd, data);
}

INLINE uint8_t SimpleDS3231::_encode_hou(uint8_t hou, bool am_pm_format, bool is_pm)
{
    uint8_t rv = 0;

    rv = _encode_gen(hou);

    if (am_pm_format) {
        SET_BIT(rv, HOU_FORMAT);

        if (is_pm)
            SET_BIT(rv, HOU_AM_PM);
    }

    return rv;
}

uint8_t SimpleDS3231::get_temp()
{
    uint8_t temp;

    _write_start();
    _write_byte(DS3231_WRITE_ADDR);
    _write_byte(DS3231_MSB_TMP_REG);

    _write_start();
    _write_byte(DS3231_READ_ADDR);

    temp = _read_byte_nack();
    _write_stop();

    return temp;
}

uint8_t SimpleDS3231::get_sec()
{
    READ_SEC_DATA();
    DECODE_SEC();

    return _sec;
}

uint8_t SimpleDS3231::get_min()
{
    READ_MIN_DATA();
    DECODE_MIN();

    return _min;
}

uint8_t SimpleDS3231::get_hou()
{
    READ_HOU_DATA();
    DECODE_HOU();

    return _hou;
}

const char* SimpleDS3231::get_time_str()
{
    READ_ALL_DATA();
    DECODE_SEC();
    DECODE_MIN();
    DECODE_HOU();
    _format_time_string();

    return _str_buffer;
}

void SimpleDS3231::set_hou(uint8_t hou, bool am_pm_format, bool is_pm)
{
    if (am_pm_format && (hou < 1 || hou > 12) ||
        hou > 23)
        return;

    ENCODE_HOU(hou, am_pm_format, is_pm);
    WRITE_HOU_DATA();
}

void SimpleDS3231::set_min(uint8_t min)
{
    if (min > 59)
        return;

    ENCODE_MIN(min);
    WRITE_MIN_DATA();
}

void SimpleDS3231::set_sec(uint8_t sec)
{
    if (sec > 59)
        return;

    ENCODE_SEC(sec);
    WRITE_SEC_DATA();
}

void SimpleDS3231::set_time(uint8_t hou, uint8_t min, uint8_t sec,
                            bool am_pm_format, bool is_pm)
{
    if (am_pm_format && (hou < 1 || hou > 12) ||
        hou > 23 || min > 59 || sec > 59)
        return;

    ENCODE_HOU(hou, am_pm_format, is_pm);
    ENCODE_MIN(min);
    ENCODE_SEC(sec);
    WRITE_TIME_DATA();
}

uint8_t SimpleDS3231::get_day()
{
    READ_DAY_DATA();
    DECODE_DAY();

    return _day;
}

uint8_t SimpleDS3231::get_mon()
{
    READ_MON_DATA();
    DECODE_MON();

    return _mon;
}

int SimpleDS3231::get_year()
{
    READ_YEAR_DATA();
    DECODE_YEAR();

    return _year;
}

const char* SimpleDS3231::get_date_str()
{
    READ_ALL_DATA();
    DECODE_DAY();
    DECODE_MON();
    DECODE_YEAR();
    _format_date_string();

    return _str_buffer;
}

void SimpleDS3231::set_day(uint8_t day)
{
    if (day < 1 || day > 31)
        return;

    ENCODE_DAY(day);
    Serial.println(_data_buffer[DATA_DAY]);
    WRITE_DAY_DATA();
}

void SimpleDS3231::set_mon(uint8_t mon)
{
    if (mon < 1 || mon > 12)
        return;

    ENCODE_MON(mon);
    WRITE_MON_DATA();
}

void SimpleDS3231::set_year(int year)
{
    if (year < 2000 || year > 2099)
        return;

    ENCODE_YEAR(year - 2000);
    WRITE_YEAR_DATA();
}

void SimpleDS3231::set_date(uint8_t day, uint8_t mon, int year)
{
    if (day < 1 || day > 31 ||
        mon < 1 || mon > 12 ||
        year < 2000 || year > 2099)
        return;

    ENCODE_DAY(day);
    ENCODE_MON(mon);
    ENCODE_YEAR(year - 2000);
    WRITE_DATE_DATA();
}

/*
 * The fine clock uses the DS3231 32.768kHz clock to  count milliseconds (resolution: 976us 562ns 5ps)
 * It enables a compare match interrupt on T1. The interrupt runs at every 32 clock cycles.
 * 1 / 32678 = 0.00003051757 * 32 = 0.0009765625 = 976us 562ns 5ps
 */
void SimpleDS3231::enable_fine_clock()
{
    pinMode(5, INPUT_PULLUP);

    _write_start();
    _write_byte(DS3231_WRITE_ADDR);
    _write_byte(DS3231_CTL_STA_REG);
    _write_byte(1 << 3);

    noInterrupts();
    /*
     * Init control registers.
     */
    TCCR1A = 0;
    TCCR1B = 0;
    /*
     * Init counter register.
     */
    TCNT1  = 0;
    /*
     * Init compare match register.
     */
    OCR1A = 31;
    /*
     * Set CTC mode and clock source as T1 on rising edge.
     */
    TCCR1B |= (1 << WGM12);
    TCCR1B |= ((1 << CS12) | (1 << CS11) | (1 << CS10));
    /*
     * Enable time compare interrupt.
     */
    TIMSK1 |= (1 << OCIE1A);
    interrupts();
}

uint32_t SimpleDS3231::get_millis()
{
    uint32_t ms;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ms = ms_g;
    }
    return ms;
}

ISR(TIMER1_COMPA_vect)
{
    ps_g += 5;
    ns_g += 562;
    us_g += 976;

    while (ps_g >= 1000) {
        ns_g++;
        ps_g -= 1000;
    }

    while (ns_g >= 1000) {
        us_g++;
        ns_g -= 1000;
    }

    while (us_g >= 1000) {
        ms_g++;
        us_g -= 1000;
    }
}
