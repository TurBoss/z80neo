// i2c_ee.h
#ifndef I2C_EE_H
#define I2C_EE_H
#include <stdint.h>
void i2c_ee_init(void);
void i2c_ee_write(uint8_t data);
uint8_t i2c_ee_read(void);
#endif
