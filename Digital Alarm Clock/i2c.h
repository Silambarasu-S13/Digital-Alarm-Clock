#ifndef I2C_H
#define I2C_H

static void i2c_wait(void);
void init_i2c(void);
void i2c_start(void);
void i2c_repeated_start(void);
void i2c_stop(void);
unsigned char i2c_write(unsigned char data);
unsigned char i2c_read(unsigned char ack);

#endif
