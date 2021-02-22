/* 
 * Simple driver for DS3231 RTC module.
 *
 * Written by Victor Gabriel Costin.
 * Licensed under the MIT license.
 */

#ifndef __SIMPLEI2CINTERFACE_H__
#define __SIMPLEI2CINTERFACE_H__

class SimpleI2CInterface {
protected:
    void                _send_start();
    void                _send_stop();
    void                _send_byte(uint8_t byte);
    uint8_t             _recv_byte_ack();
    uint8_t             _recv_byte_nack();
    void                _wait_int();

public:
    SimpleI2CInterface();

    void                begin();
};

#endif /* __SIMPLEI2CINTERFACE_H__ */
