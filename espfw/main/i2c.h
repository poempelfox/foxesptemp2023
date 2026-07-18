
#ifndef _I2C_H_
#define _I2C_H_

extern i2c_master_bus_handle_t i2c_bushandles[2];

void i2c_port_init();
uint32_t i2c_settingtoi2cclock(uint8_t s);

#endif /* _I2C_H_ */

