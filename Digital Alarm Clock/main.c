/*Description:
              Author       :  SILAMBARASU S
              Start Date   :  06/07/2026
              End Date     :  12/07/2026
              Project Name :  Digital Timer (Digital Alarm Clock)
              Description  :  A digital clock built with an alarm system is called as an alarm clock.
              Along with showing time it includes a preset timer to remember and trigger an event when
              the timer expires by generating an alarm. Alarms can be configured for different time
              intervals say daily or weekly which can trigger different set of actions. The actions
              can range from blowing a buzzer or switching on a  light. The sound of an alarm can be
              stopped by pressing the button or automatically stop by producing a beep sound in particular time duration.

              EVENTS:-
                    O -> ONCE
                    D -> DAILY
                    W -> WEAKLY
 
              ALARM OFF - LEFT_KEY OR RIGHT_KEY
 
              USER MANUAL:-
                    UP_KEY    -> MK_SW1
                    DOWN_KEY  -> MK_SW2
                    LEFT_KEY  -> MK_SW3
                    RIGHT_KEY -> MK_SW6

                    DEFAULT SCREEN
                    CONFIG MODE         -  UP_KEY
                    CONFIG MODE         -  DOWN_KEY

                    CONFIG SCREEN
                    SCROLL UP           -  UP_KEY
                    SCROLL DOWN         -  DOWN_KEY
                    NEXT SCREEN         -  UP_KEY  (LONG PRESS)
                    PREVIOUS SCREEN     -  DOWN_KEY(LONG PRESS)

                    1.SET EVENT
                    VALUE ROLLOVER      -  UP_KEY
                    SAVE EVENT          -  UP_KEY(LONG PRESS)
                    FIELD CHANGE        -  LEFT_KEY AND RIGHT_KEY

                    2.VIEW EVENT
                    SCROLL UP           -  UP_KEY
                    SCROLL DOWN         -  DOWN_KEY
                    DELETE EVENT        -  LEFT_KEY
                    EDIT EVENT          -  RIGHT_KEY
                    PREVIOUS SCREEN     -  RIGHT_KEY(LONG PRESS)

                    3.SET TIME
                    VALUE ROLLOVER      -  UP_KEY
                    SAVE EVENT          -  UP_KEY(LONG PRESS)
                    FIELD CHANGE        -  LEFT_KEY AND RIGHT_KEY

                    4.SET DATE
                    VALUE ROLLOVER      -  UP_KEY
                    SAVE EVENT          -  UP_KEY(LONG PRESS)
                    FIELD CHANGE        -  LEFT_KEY AND RIGHT_KEY
 
               


*/

#include <xc.h>
#include "main.h"
#include "i2c.h"
#include "clcd.h"
#include "rtc.h"
#include "timer0.h"
#include "matrix_keypad.h"
#include "alarm.h"
#include "buzzer.h"
#include "time.h"


void init_config(void)
{
  init_i2c();
  init_clcd();
  init_rtc();
  init_timer0();
  init_buzzer();
  init_matrix_keypad();
  
  set_default_time_and_date();
  restore_event_from_eeprom();
}

unsigned char screen = 0;

void main(void)
{
  init_config();
  
  while (1)
  {
    // Default Screen
    if (screen == 0)
    {
      default_page();
    }
    // Configuration Screen
    else if (screen == 1)
    {
      config_page();
    }
  }
}
