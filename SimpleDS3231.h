/* 
 * Simple Arduino driver for DS3231 RTC module.
 *
 * Written by Victor Gabriel Costin.
 * Licensed under the MIT license.
 */

#ifndef __SIMPLEDS3231_H__
#define __SIMPLEDS3231_H__

#include <Arduino.h>
#include "include/SimpleI2CInterface.h"

class SimpleDS3231 : public SimpleI2CInterface {
private:
    uint8_t             _sec = 0;
    uint8_t             _min = 0;
    uint8_t             _hou = 0;
    uint8_t             _dat = 0;
    uint8_t             _mon = 0;
    int                 _year = 0;

    uint8_t             _raw_data[7] = {0};                 

    bool                _is_pm = false;
    bool                _12_format = false;
    char                _time_str[12] = "xx:xx:xx xx";
    char                _date_str[11] = "xx.xx.xxxx";

    void                _get_data_reg(uint8_t reg, uint8_t n_regs);

    void                _format_sec_data();
    void                _format_min_data();
    void                _format_hou_data();
    void                _format_time_string();
    void                _format_time_data();

    void                _format_dat_data();
    void                _format_mon_data();
    void                _format_year_data();
    void                _format_date_string(); 
    void                _format_date_data();

public:
    SimpleDS3231();

    uint8_t             get_sec();
    uint8_t             get_min();
    uint8_t             get_hou();
    const char*         get_time_str();

    uint8_t             get_dat();
    uint8_t             get_mon();
    int                 get_year();
    const char*         get_date_str();
};

#endif /* __SIMPLEDS3231_H__ */
