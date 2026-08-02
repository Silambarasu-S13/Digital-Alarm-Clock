#include <xc.h>
#include "i2c.h"

static void i2c_wait(void)
{
    while ((SSPCON2 & 0x1F) || R_W);
}

void init_i2c(void)
{
    TRISC3 = 1;    // SCL
    TRISC4 = 1;    // SDA

    SSPCON1 = 0x28;   // I2C Master mode, SSPEN=1
    SSPCON2 = 0x00;

    SSPADD = 49;      // 100 kHz @ Fosc = 20 MHz

    SMP = 1;
    CKE = 0;

    SSPIF = 0;
    BCLIF = 0;
}

void i2c_start(void)
{
    i2c_wait();

    SSPIF = 0;
    SEN = 1;

    while(SEN);
}

void i2c_repeated_start(void)
{
    i2c_wait();

    SSPIF = 0;
    RSEN = 1;

    while(RSEN);
}

void i2c_stop(void)
{
    i2c_wait();

    SSPIF = 0;
    PEN = 1;

    while(PEN);
}

unsigned char i2c_write(unsigned char data)
{
    i2c_wait();

    SSPIF = 0;
    SSPBUF = data;

    while(!SSPIF);
    SSPIF = 0;

    return ACKSTAT;     // 0 = ACK, 1 = NACK
}

unsigned char i2c_read(unsigned char ack)
{
    unsigned char data;

    i2c_wait();

    SSPIF = 0;
    RCEN = 1;

    while(!SSPIF);
    SSPIF = 0;

    data = SSPBUF;

    i2c_wait();

    ACKDT = (ack) ? 0 : 1;   // ack=1 -> ACK, ack=0 -> NACK
    ACKEN = 1;

    while(ACKEN);

    return data;
}








