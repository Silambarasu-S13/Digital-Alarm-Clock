#ifndef ALARM_H
#define ALARM_H

#define UP_PRESS      2
#define DOWN_PRESS    4
#define LEFT_PRESS    6
#define RIGHT_PRESS   8

#define UP_LONG_PRESS       1
#define UP_SHORT_PRESS      2
#define DOWN_LONG_PRESS     3
#define DOWN_SHORT_PRESS    4
#define LEFT_LONG_PRESS     5
#define LEFT_SHORT_PRESS    6
#define RIGHT_LONG_PRESS    7
#define RIGHT_SHORT_PRESS   8


void default_page();
void config_page();
int key_detect(void);
void restore_event_from_eeprom(void);

#endif