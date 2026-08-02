#include "buzzer.h"
#include <xc.h>

void init_buzzer(void)
{
    ADCON1 = 0x0F;

    //  RE0 as Output 
    TRISEbits.TRISE0 = 0;

    // Initially OFF 
    LATEbits.LATE0 = 0;
}

void buzzer_on(void)
{
    PORTEbits.RE0 = 1;    
}

void buzzer_off(void)
{
    PORTEbits.RE0 = 0;
}

