#include "alarm.h"
#include <xc.h>
#include "main.h"
#include "rtc.h"
#include "time.h"
#include "timer0.h"
#include <string.h>
#include "buzzer.h"
#include "clcd.h"
#include "matrix_keypad.h"
#include "eeprom.h"

// Oscillator frequency used by the __delay_ms()/__delay_us() macros
#define _XTAL_FREQ 20000000

// Current screen/page index, defined and updated elsewhere (main.c)
extern unsigned char screen;
// Current time string ("HH:MM...") kept up to date by the RTC module
extern unsigned char time[9];

// Number of alarms currently stored/active
unsigned char alarm_count = 0;
// Flag used to make sure an alarm only triggers once per match (reset when time no longer matches)
char event_triggerred = 1;

// Software counter/flag used to blink UI elements while editing an alarm
volatile unsigned short blink_count = 0;
volatile unsigned char blink = 0;

// Software timer counter and state flag used to generate the alarm ring/pause pattern
volatile unsigned long count = 0;
volatile unsigned char flag = 0;

// Software timer counter and flag used to auto stop the buzzer after a timeout
volatile unsigned long int beep_count = 0;
volatile unsigned char beep_flag = 0;

// Timer0 interrupt service routine: reloads TMR0 and updates all software
// timing counters/flags used across the module (alarm ring pattern,
// auto buzzer-off timeout, and UI blink toggling)
void __interrupt() isr(void)
{
    if (TMR0IF)
    {
        // Clear interrupt flag and reload timer for the next tick
        TMR0IF = 0;
        TMR0 = TMR0 + 8;

        // Only run the ring pattern timing while at least one alarm exists
        if (alarm_count > 0)
        {
            count++;
            if (count >= 100000 && flag == 0) // 5 second
            {
                // 5 seconds elapsed while "off" -> switch to "on" phase
                count = 0;
                flag = 1;
            }
            else if (count > 40000 && flag == 1) // 2 second
            {
                // 2 seconds elapsed while "on" -> switch back to "off" phase
                count = 0;
                flag = 0;
            }
        }
        else
        {
            // No alarms set, keep the pattern counters reset
            count = 0;
            flag = 0;
        }
        
        // Countdown to automatically silence the buzzer after it has been triggered
        if(beep_flag == 1)
        {
            if (beep_count++ >= 100000) // 5 second
            {           
                beep_count = 0;
                
                beep_flag = 0;
                
                // Timeout reached: stop the buzzer and mark the event as no longer triggering
                buzzer_off();
                event_triggerred = 0;
            }
        }

        // Toggle the blink flag roughly every 0.5 second for blinking UI fields
        if (blink_count++ >= 10000) // 0.5 second blink
        {
            blink_count = 0;
            blink = !blink;
        }

    }
}



// Per-key hold counters, incremented each poll while a key stays pressed
// and used to distinguish short presses from long presses
int up_count = 0;
int down_count = 0;
int left_count = 0;
int right_count = 0;

// Polls the keypad (LEVEL_CHANGE mode) and returns a short-press or
// long-press event code for whichever key is currently active, or 0 if none
int key_detect(void)
{
    char key = read_matrix_keypad(LEVEL_CHANGE);

    // UP
    if (key == UP_KEY)
    {
        // Key held long enough -> report long press and reset the counter
        if (++up_count >= 4500)
        {
            up_count = 0;
            return UP_LONG_PRESS;
        }
    }
    else
    {
        // Key was released before reaching long-press threshold but was
        // held past the debounce threshold -> report short press
        if (up_count > 20)
        {
            up_count = 0;
            return UP_SHORT_PRESS;
        }
        up_count = 0;
    }

    // DOWN - same short/long press logic as UP, using down_count
    if (key == DOWN_KEY)
    {
        if (++down_count >= 4500)
        {
            down_count = 0;
            return DOWN_LONG_PRESS;
        }
    }
    else
    {
        if (down_count > 20)
        {
            down_count = 0;
            return DOWN_SHORT_PRESS;
        }
        down_count = 0;
    }

    // LEFT - same short/long press logic as UP, using left_count
    if (key == LEFT_KEY)
    {
        if (++left_count >= 4500)
        {
            left_count = 0;
            return LEFT_LONG_PRESS;
        }
    }
    else
    {
        if (left_count > 20)
        {
            left_count = 0;
            return LEFT_SHORT_PRESS;
        }
        left_count = 0;
    }

    // RIGHT - same short/long press logic as UP, using right_count
    if (key == RIGHT_KEY)
    {
        if (++right_count >= 4500)
        {
            right_count = 0;
            return RIGHT_LONG_PRESS;
        }
    }
    else
    {
        if (right_count > 20)
        {
            right_count = 0;
            return RIGHT_SHORT_PRESS;
        }
        right_count = 0;
    }

    // No key currently qualifies as a short or long press
    return 0;
}

// Display-ready strings for each stored alarm, e.g. "08:30 AM O" (time, AM/PM, schedule type)
char view_alarm[20][20] = {0};

// Rebuilds the view_alarm[] array from the values persisted in internal EEPROM,
// meant to be called once at startup so alarms survive a power cycle
void restore_event_from_eeprom(void)
{
    // Byte 0x00 stores how many alarms are saved
    alarm_count  = read_internal_eeprom(0x00);

    if(alarm_count == 0)
        return;

    // Safety clamp in case EEPROM holds a corrupted/out-of-range count
    if(alarm_count > 20)
        alarm_count = 0;


    // Lookup tables to translate stored AM/PM and schedule codes into display text
    char *period_arr[3] = {" AM", " PM", "   "};
    char *alarm_schedule[4] = {" O", " D", " W", "  "};
    
    char alarm_time[8];
    char i = 0;
    unsigned short address = 0;

    while(i < alarm_count)
    {
        // Each alarm occupies 4 EEPROM bytes: hour, minute, period, schedule
        address = i * 4;

        char store_hour        = read_internal_eeprom(address + 1);
        char store_minute      = read_internal_eeprom(address + 2);
        char store_period_no   = read_internal_eeprom(address + 3);
        char store_schedule_no = read_internal_eeprom(address + 4);

        // Format as "HH:MM"
        alarm_time[0] = '0' + store_hour / 10;
        alarm_time[1] = '0' + store_hour % 10;
        alarm_time[2] = ':';
        alarm_time[3] = '0' + store_minute / 10;
        alarm_time[4] = '0' + store_minute % 10;
        alarm_time[5] = '\0';

        // Build the full display string: "HH:MM" + " AM/PM" + schedule tag
        strcpy(view_alarm[i], alarm_time);
        strcat(view_alarm[i], period_arr[store_period_no]);
        strcat(view_alarm[i], alarm_schedule[store_schedule_no]);

        i++;
    }
}

// Bubble-sorts the view_alarm[] list in ascending chronological ("HH:MM") order
// so the earliest upcoming alarm is always at index 0
void sort_alarm_event(void)
{
    char temp[20];

    for (int i = 0; i < alarm_count - 1; i++)
    {
        for (int j = 0; j < alarm_count - i - 1; j++)
        {
            // Compare only the "HH:MM" portion (first 5 characters)
            if (strncmp(view_alarm[j], view_alarm[j + 1], 5) > 0)
            {
                // Swap adjacent entries that are out of order
                strcpy(temp, view_alarm[j]);
                strcpy(view_alarm[j], view_alarm[j + 1]);
                strcpy(view_alarm[j + 1], temp);
            }
        }
    }
}

// Lets the user edit an existing alarm (selected by index into view_alarm[])
// using LEFT/RIGHT to move between fields and UP/DOWN to change values,
// then saves the updated alarm back to EEPROM
void edit_alarm(int index)
{
    char hour = 0;
    char minute = 0;
    char period_no = 0;
    char schedule_no = 0;

    char *type_arr[3] = {" AM", " PM", "   "};
    char *alarm_schedule[4] = {" O", " D", " W", "  "};

    // Parse the existing "HH:MM AM/PM O" style string back into numeric fields
    hour = (view_alarm[index][0] - '0') * 10 + (view_alarm[index][1] - '0');
    
    minute = (view_alarm[index][3] - '0') * 10 + (view_alarm[index][4] - '0');
    
    period_no = view_alarm[index][6] == 'P' ? 1 : 0;

    if (view_alarm[index][9] == 'O')
        schedule_no = 0;
    else if (view_alarm[index][9] == 'D')
        schedule_no = 1;
    else if (view_alarm[index][9] == 'W')
        schedule_no = 2;

    // Rebuild the "HH:MM" text used while editing
    char alarm_time[8];
    alarm_time[0] = '0' + hour / 10;
    alarm_time[1] = '0' + hour % 10;
    alarm_time[2] = ':';
    alarm_time[3] = '0' + minute / 10;
    alarm_time[4] = '0' + minute % 10;
    alarm_time[5] = '\0';

    // field selects which value (hour/minute/AM-PM/schedule) is currently editable
    unsigned char field = 0;
    unsigned char key;
    int upkey_count = 0;

    // Initial screen for the edit UI showing the alarm being edited
    CLEAR_DISP_SCREEN;
    clcd_print("# T", LINE1(0));
    clcd_putch(index + '0', LINE2(0));
    clcd_print("D", LINE2(2));

    clcd_print(alarm_time, LINE2(5));
    clcd_print(type_arr[period_no], LINE2(10));
    clcd_print(alarm_schedule[schedule_no], LINE2(13));

    while (1)
    {  
        char exit_key = read_matrix_keypad(LEVEL_CHANGE);

        // Holding UP for ~500 polls exits edit mode and saves current values
        if (exit_key == UP_KEY)
        {
            if (++upkey_count >= 500)
            {
                alarm_time[0] = '0' + hour / 10;
                alarm_time[1] = '0' + hour % 10;
                alarm_time[3] = '0' + minute / 10;
                alarm_time[4] = '0' + minute % 10; 
                break;
            }
        }
        else
        {
            upkey_count = 0;
        }
        
        // Keep the current time display refreshed in the background
        get_time();
        display_time();
        
        key = key_detect();
        
        clcd_print(alarm_time, LINE2(5));

        // Field Change - move the editable cursor left/right between fields
        if (key == LEFT_PRESS)
        {
            if (field > 0)
                field--;
        }
        else if (key == RIGHT_PRESS)
        {
            if (field < 3)
                field++;
        }

        // Hour - active when field == 0
        if (field == 0)
        {
            alarm_time[3] = '0' + minute / 10;
            alarm_time[4] = '0' + minute % 10;
            clcd_print(type_arr[period_no], LINE2(10));
            clcd_print(alarm_schedule[schedule_no], LINE2(13));


            // Increment/decrement hour, wrapping within 1-12 (12-hour format)
            if (key == UP_PRESS)
            {
                hour++;
                if (hour > 12)
                {
                    hour = 1;
                }
            }
            else if(key == DOWN_PRESS)
            {
                hour--;
                if(hour == 0)
                {
                    hour = 12;
                }      
            }
            
            // Blink the hour digits to indicate this field is being edited
            if (blink)
            {
                alarm_time[0] = '0' + hour / 10;
                alarm_time[1] = '0' + hour % 10;
            }
            else
            {
                alarm_time[0] = ' ';
                alarm_time[1] = ' ';
            }
            clcd_print(alarm_time, LINE2(5));
        }
        // Minute - active when field == 1
        else if (field == 1)
        {
            alarm_time[0] = '0' + hour / 10;
            alarm_time[1] = '0' + hour % 10;
            clcd_print(type_arr[period_no], LINE2(10));
            clcd_print(alarm_schedule[schedule_no], LINE2(13));
            
            // Increment/decrement minute, wrapping within 0-59
            if (key == UP_PRESS)
            {
                minute = ++minute % 60;
            }
            else if(key == DOWN_PRESS)
            {
                
                if(minute == 0)
                {
                    minute = 59;
                }
                else
                {
                    --minute;
                }

            }

            // Blink the minute digits to indicate this field is being edited
            if (blink)
            {
                alarm_time[3] = '0' + minute / 10;
                alarm_time[4] = '0' + minute % 10;
            }
            else
            {
                alarm_time[3] = ' ';
                alarm_time[4] = ' ';
            }
            clcd_print(alarm_time, LINE2(5));
        }

        // Type (AM/PM) - active when field == 2
        else if (field == 2)
        {
            alarm_time[0] = '0' + hour / 10;
            alarm_time[1] = '0' + hour % 10;
            alarm_time[3] = '0' + minute / 10;
            alarm_time[4] = '0' + minute % 10;
            clcd_print(alarm_schedule[schedule_no], LINE2(13));
            
            // Toggle AM/PM on UP press
            if (key == UP_PRESS)
            {
                period_no = ++period_no % 2;
            }
                     
            // Blink the AM/PM text to indicate this field is being edited
            if (blink)
            {
                clcd_print(type_arr[period_no], LINE2(10));
            }
            else
            {
                clcd_print("   ", LINE2(10));
            }
        }
        // Schedule (Once/Daily/Weekly) - active when field == 3
        else if (field == 3)
        {
            alarm_time[3] = '0' + minute / 10;
            alarm_time[4] = '0' + minute % 10;
            alarm_time[0] = '0' + hour / 10;
            alarm_time[1] = '0' + hour % 10;
            clcd_print(type_arr[period_no], LINE2(10));
            
            // Cycle through schedule types (O -> D -> W -> O ...) on UP press
            if (key == UP_PRESS)
            {
                schedule_no = ++schedule_no % 3;
            }

            // Blink the schedule tag to indicate this field is being edited
            if (blink)
            {
                clcd_print(alarm_schedule[schedule_no], LINE2(13));
            }
            else
            {
                clcd_print("  ", LINE2(13));
            }
        }

    } // while end

    // saving after edit: rebuild the "HH:MM AM/PM X" display string from the edited fields
    view_alarm[index][0] = (hour / 10) + '0';
    view_alarm[index][1] = (hour % 10) + '0';
    view_alarm[index][2] = ':';
    view_alarm[index][3] = (minute / 10) + '0';
    view_alarm[index][4] = (minute % 10) + '0';

    view_alarm[index][5] = ' ';
    if (period_no)
    { 
        view_alarm[index][6] = 'P';
        view_alarm[index][7] = 'M';
    }
    else
    {
        view_alarm[index][6] = 'A';
        view_alarm[index][7] = 'M';
    }

    view_alarm[index][8] = ' ';
    
    // Encode schedule type as a single character tag
    if (schedule_no == 0)
        view_alarm[index][9] = 'O';
    else if (schedule_no == 1)
        view_alarm[index][9] = 'D';
    else if (schedule_no == 2)
        view_alarm[index][9] = 'W';

    view_alarm[index][10] = '\0';


    // Persist the edited alarm back to its original EEPROM slot
    unsigned short address = 0;
    address = index * 4;

    write_internal_eeprom(address+1, hour);
    write_internal_eeprom(address+2, minute);
    write_internal_eeprom(address+3, period_no);
    write_internal_eeprom(address+4, schedule_no);

    // Confirm the edit to the user
    CLEAR_DISP_SCREEN;
    clcd_print(" ALARM EDITED  ", LINE1(0));
    clcd_print("SUCCESSFULLY...", LINE2(1));
    __delay_ms(1000);
    return;
}

// Guides the user through creating a brand-new alarm (hour, minute, AM/PM,
// schedule type) and appends it to view_alarm[] and EEPROM
void set_alarm()
{
    // Reject new alarms once the fixed-size storage (20 slots) is full
    if (alarm_count >= 20)
    {
        CLEAR_DISP_SCREEN;
        clcd_print("MEMORY FULL", LINE1(0));
        __delay_ms(1000);
        return;
    }

    // Default starting values for a new alarm
    char hour = 1;
    char minute = 0;

    char *type_arr[3] = {" AM", " PM", "   "};
    char period_no = 0;

    char *alarm_schedule[4] = {" O", " D", " W", "  "};
    char schedule_no = 0;

    // Build the initial "HH:MM" text
    char alarm_time[8];
    alarm_time[0] = '0' + hour / 10;
    alarm_time[1] = '0' + hour % 10;
    alarm_time[2] = ':';
    alarm_time[3] = '0' + minute / 10;
    alarm_time[4] = '0' + minute % 10;
    alarm_time[5] = '\0';

    // field selects which value (hour/minute/AM-PM/schedule) is currently editable
    unsigned char field = 0;
    unsigned char key;
    int upkey_count = 0;

    CLEAR_DISP_SCREEN;
    clcd_print("TIME", LINE1(0));
    clcd_print("ALARM", LINE2(0));

    while (1)
    {   
        char exit_key = read_matrix_keypad(LEVEL_CHANGE);

        // Holding UP for ~500 polls exits the setup and saves the new alarm
        if (exit_key == UP_KEY)
        {
            if (++upkey_count >= 500)
            {               
                alarm_time[0] = '0' + hour / 10;
                alarm_time[1] = '0' + hour % 10;
                alarm_time[3] = '0' + minute / 10;
                alarm_time[4] = '0' + minute % 10; 
                break;
            }
        }
        else
        {
            upkey_count = 0;
        }


        // Keep the current time display refreshed in the background
        get_time();
        time_hr_min();
        clcd_putch('$', LINE1(15));
        clcd_print(alarm_time, LINE2(6));

        key = key_detect();
        
        // Field Change - move the editable cursor left/right between fields
        if (key == LEFT_PRESS)
        {
            if (field > 0)
                field--;
        }
        else if (key == RIGHT_PRESS)
        {
            if (field < 3)
                field++;
        }

        // Hour - active when field == 0
        if (field == 0)
        {
            alarm_time[3] = '0' + minute / 10;
            alarm_time[4] = '0' + minute % 10;
            clcd_print(type_arr[period_no], LINE2(11));
            clcd_print(alarm_schedule[schedule_no], LINE2(14));
                
            // Increment/decrement hour, wrapping within 1-12 (12-hour format)
            if (key == UP_PRESS)
            {
                hour++;
                if (hour > 12)
                {
                    hour = 1;
                }
            }
            else if(key == DOWN_PRESS)
            {
                hour--;
                if(hour == 0)
                {
                    hour = 12;
                }      
            }
            // Blink the hour digits to indicate this field is being edited
            if (blink)
            {
                alarm_time[0] = '0' + hour / 10;
                alarm_time[1] = '0' + hour % 10;
            }
            else
            {
                alarm_time[0] = ' ';
                alarm_time[1] = ' ';
            }
            
        }
        // Minute - active when field == 1
        else if (field == 1)
        {     
            alarm_time[0] = '0' + hour / 10;
            alarm_time[1] = '0' + hour % 10;
            clcd_print(type_arr[period_no], LINE2(11));
            clcd_print(alarm_schedule[schedule_no], LINE2(14));
            
            // Increment/decrement minute, wrapping within 0-59
            if (key == UP_PRESS)
            {
                minute = ++minute % 60;
            }
            else if(key == DOWN_PRESS)
            {
                if(minute == 0)
                {
                    minute = 59;
                }
                else
                {
                   --minute; 
                }
            }
            
            // Blink the minute digits to indicate this field is being edited
            if (blink)
            {
                alarm_time[3] = '0' + minute / 10;
                alarm_time[4] = '0' + minute % 10;
            }
            else
            {
                alarm_time[3] = ' ';
                alarm_time[4] = ' ';
            }      
        }

        // Type (AM/PM) - active when field == 2
        else if (field == 2)
        {
            alarm_time[0] = '0' + hour / 10;
            alarm_time[1] = '0' + hour % 10;
            alarm_time[3] = '0' + minute / 10;
            alarm_time[4] = '0' + minute % 10;   
            clcd_print(alarm_schedule[schedule_no], LINE2(14));

            // Toggle AM/PM on UP press
            if (key == UP_PRESS)
            {
                period_no = ++period_no % 2;
            }

            // Blink the AM/PM text to indicate this field is being edited
            if (blink)
            {
                clcd_print(type_arr[period_no], LINE2(11));
            }
            else
            {
                clcd_print("   ", LINE2(11));
            }

        }
        
        // Schedule (Once/Daily/Weekly) - active when field == 3
        else if (field == 3)
        {
            alarm_time[0] = '0' + hour / 10;
            alarm_time[1] = '0' + hour % 10;
            alarm_time[3] = '0' + minute / 10;
            alarm_time[4] = '0' + minute % 10; 
            clcd_print(type_arr[period_no], LINE2(11));

            // Cycle through schedule types (O -> D -> W -> O ...) on UP press
            if (key == UP_PRESS)
            {
                schedule_no = ++schedule_no % 3;
            }

            // Blink the schedule tag to indicate this field is being edited
            if (blink)
            {
                clcd_print(alarm_schedule[schedule_no], LINE2(14));
            }
            else
            {
                clcd_print("  ", LINE2(14));
            }
        }

    } // while end

    // Assemble the final display string and persist the new alarm
    strcpy(view_alarm[alarm_count], alarm_time);
    strcat(view_alarm[alarm_count], type_arr[period_no]);
    strcat(view_alarm[alarm_count], alarm_schedule[schedule_no]);
   

    // Persist the new alarm to the next free EEPROM slot
    unsigned short address = 0;

    address = alarm_count * 4;

    write_internal_eeprom(address+1, hour);
    write_internal_eeprom(address+2, minute);
    write_internal_eeprom(address+3, period_no);
    write_internal_eeprom(address+4, schedule_no);

    // Update and persist the total alarm count
    alarm_count++;
    write_internal_eeprom(0x00, alarm_count);

    // Keep the list chronologically ordered after adding the new alarm
    if (alarm_count > 1)
        sort_alarm_event();

    // Confirm the new alarm to the user
    CLEAR_DISP_SCREEN;
    clcd_print("SET ALARM DONE", LINE1(0));
    clcd_print("SUCCESSFULLY...", LINE2(1));
    __delay_ms(1000);

    return;
}

// Lets the user browse the saved alarm list with UP/DOWN, delete the
// selected alarm with LEFT, or edit it with RIGHT; RIGHT/DOWN held long exits
void view_event()
{
    unsigned char key;
    int rightkey_count = 0;
    int downkey_count = 0;

    CLEAR_DISP_SCREEN;
    clcd_print("# T", LINE1(0));
    clcd_print("  D", LINE2(0));

    int x = 0;
    while (1)
    {   
        char exit_key = read_matrix_keypad(LEVEL_CHANGE);

        // Holding RIGHT for ~500 polls exits the view screen
        if (exit_key == RIGHT_KEY)
        {
            if (++rightkey_count >= 500)
            {
                rightkey_count = 0;
                return;
            }
        }
        else
        {
            rightkey_count = 0;
        }
        
        // Holding DOWN for ~500 polls also exits the view screen
        if (exit_key == DOWN_KEY)
        {
            if (++downkey_count >= 500)
            {
                downkey_count = 0;
                return;
            }
        }
        else
        {
            downkey_count = 0;
        }
        

        // Nothing to browse if there are no alarms saved
        if (alarm_count == 0)
        {
            CLEAR_DISP_SCREEN;
            clcd_print("NO ALARM", LINE1(0));
            __delay_ms(2000);
            return;
        }

        // Keep the current time display refreshed in the background
        get_time();
        display_time();
        
        key = key_detect();
        
        // Show the currently selected alarm entry
        clcd_print(view_alarm[x], LINE2(5));
        clcd_putch(x + '0', LINE2(0));

        // Move selection up/down through the alarm list
        if (key == UP_PRESS)
        {
            if (x > 0)
                x--;
        }
        else if (key == DOWN_PRESS)
        {
            if (x < alarm_count - 1)
                x++;
            else
            {
                // Already at the last alarm: briefly show an end-of-list message
                clcd_print("                ", LINE2(0));
                clcd_print("...THE END...", LINE2(1));
                __delay_ms(500);
                clcd_print("                ", LINE2(0));
                clcd_print("  D", LINE2(0));
            }
        }
        else if (key == LEFT_PRESS) // delete event
        {
            // Shift all later entries in view_alarm[] left to overwrite the deleted one
            for (int i = x; i < alarm_count - 1; i++)
            {
                strcpy(view_alarm[i], view_alarm[i + 1]);
            }

            // Shift the corresponding EEPROM records left as well, so storage stays contiguous
            unsigned short address = 0;
            for (int i = x; i < alarm_count - 1; i++)
            {
                address = (i+1) * 4;
                char hour = read_internal_eeprom(address+1);
                char minute = read_internal_eeprom(address+2);
                char period_no = read_internal_eeprom(address+3);
                char schedule_no = read_internal_eeprom(address+4);

                address = i * 4;
                write_internal_eeprom(address+1, hour);
                write_internal_eeprom(address+2, minute);
                write_internal_eeprom(address+3, period_no);
                write_internal_eeprom(address+4, schedule_no);
            }

            // Update and persist the reduced alarm count
            alarm_count--;
            write_internal_eeprom(0x00, alarm_count);


            // Keep the selection index within the now-shorter list
            if (x >= alarm_count)
                x = alarm_count - 1;
            
            // Notify the user and redraw the view screen header
            CLEAR_DISP_SCREEN;
            clcd_print("EVENT DELETED...", LINE1(0));
            __delay_ms(500);
            CLEAR_DISP_SCREEN;
            clcd_print("# T", LINE1(0));
            clcd_print("  D", LINE2(0));
        }
        else if (key == RIGHT_PRESS)
        {
            // Open the edit screen for the currently selected alarm
            edit_alarm(x);
            CLEAR_DISP_SCREEN;
            clcd_print("# T", LINE1(0));
            clcd_print("  D", LINE2(0));
            // Re-sort in case the edit changed the alarm's time
            if (alarm_count > 1)
                sort_alarm_event();
        }
    }
}

// Home screen: shows the current time, alternating the second line between
// the date and the next alarm (driven by the ISR "flag"), and rings the
// buzzer when the earliest alarm's time matches the current time
void default_page(void)
{
    
    char key;
    
    char change = 0;

    CLEAR_DISP_SCREEN;
    clcd_print("TIME", LINE1(0));
    
    while (1)
    {
        // Always keep the time on line 1 up to date
        get_time();
        display_time();

        // Alternate line 2 between DATE and ALARM based on the ISR-driven "flag"
        if (flag == 0 && change == 0)
        {
            clcd_print("                ", LINE2(0));
            clcd_print("DATE", LINE2(0));
            get_date();
            display_date();
            change = 1;
        }
        else if (alarm_count > 0 && flag == 1)
        {
            clcd_print("                ", LINE2(0));
            clcd_print("ALARM", LINE2(0));
            clcd_print(view_alarm[0], LINE2(6));
            change = 0;
        }

        key = read_matrix_keypad(STATE_CHANGE);
       

        // Check whether the earliest (index 0) alarm matches the current time
        if (alarm_count > 0 )
        {
            char delete_event = 0;
            if (strncmp(view_alarm[0], time, 5) == 0)
            {
                // A "once" (O) type alarm should be removed after it rings
                if (view_alarm[0][9] == 'O')
                    delete_event = 1;

                // First time matching this minute: start ringing the buzzer
                if (event_triggerred == 1)
                {
                    buzzer_on();
                    beep_flag = 1;       
                }
                // Stop ringing once the ISR timeout fires or the user presses LEFT/RIGHT to dismiss
                if (event_triggerred == 0 || key == LEFT_KEY || key == RIGHT_KEY)
                {
                    buzzer_off();

                    // Remove a one-time alarm from the list/EEPROM after it has rung
                    if (delete_event == 1 )
                    {
                        for (int i = 0; i < alarm_count - 1; i++)
                        {
                            strcpy(view_alarm[i], view_alarm[i + 1]);
                        }

                        unsigned short address = 0;
                        for (int i = 0; i < alarm_count - 1; i++)
                        {
                            address = (i+1) * 4;
                            char hour = read_internal_eeprom(address+1);
                            char minute = read_internal_eeprom(address+2);
                            char period_no = read_internal_eeprom(address+3);
                            char schedule_no = read_internal_eeprom(address+4);

                            address = i * 4;
                            write_internal_eeprom(address+1, hour);
                            write_internal_eeprom(address+2, minute);
                            write_internal_eeprom(address+3, period_no);
                            write_internal_eeprom(address+4, schedule_no);
                        }

                        alarm_count--;
                        write_internal_eeprom(0x00, alarm_count);
                    }

                }
                
            }
            else
            {
                // Time no longer matches: re-arm so the alarm can trigger again next match
                event_triggerred = 1;
            }
        }
        
        // UP or DOWN from the home screen switches to the config/menu screen
        if (key == UP_KEY || key == DOWN_KEY)
        {
            screen = 1;
            return;
        }
       
    }
}

// Configuration menu screen: top-level toggles between "SET/VIEW EVENT" and
// "SET TIME/DATE" pages (display), each with its own sub-menu (event),
// and "open" triggers entering the selected sub-screen
void config_page()
{
    // display: which top-level menu page is shown (1 = event menu, 2 = time/date menu)
    char display = 1;
    // event: which sub-menu item is highlighted within the current page
    char event = 0;
    // open: set when a sub-menu item should be opened/executed
    char open = 0;

    // change_detect: prevents redrawing the menu text every loop iteration
    char change_detect = 0;
    CLEAR_DISP_SCREEN;
    clcd_print("CONFIG MODE...", LINE1(0));
    __delay_ms(500);

    while (1)
    {
        // Draw the "SET/VIEW EVENT" vs "SET TIME/DATE" menu page
        if (display == 1 && change_detect == 0)
        {
            if (event == 0)
            {
                CLEAR_DISP_SCREEN;
                clcd_print("->SET/VIEW EVENT", LINE1(0));
                clcd_print("  SET TIME/DATE", LINE2(0));
            }
            else if (event == 1)
            {
                CLEAR_DISP_SCREEN;
                clcd_print("-> SET  EVENT", LINE1(0));
                clcd_print("   VIEW EVENT", LINE2(0));
            }
            else if (event == 2)
            {
                CLEAR_DISP_SCREEN;
                clcd_print("   SET  EVENT", LINE1(0));
                clcd_print("-> VIEW EVENT", LINE2(0));
            }
            change_detect = 1;
        }
        // Draw the "SET TIME" vs "SET DATE" menu page
        else if (display == 2 && change_detect == 0)
        {
            if (event == 0)
            {
                CLEAR_DISP_SCREEN;
                clcd_print("  SET/VIEW EVENT", LINE1(0));
                clcd_print("->SET TIME/DATE", LINE2(0));
            }
            else if (event == 1)
            {
                CLEAR_DISP_SCREEN;
                clcd_print("-> SET TIME", LINE1(0));
                clcd_print("   SET DATE", LINE2(0));
            }
            else if (event == 2)
            {
                CLEAR_DISP_SCREEN;
                clcd_print("   SET TIME", LINE1(0));
                clcd_print("-> SET DATE", LINE2(0));
            }
            change_detect = 1;
        }

        // operation execution: launch the selected sub-screen when "open" is set
        if (display == 1 && open == 1)
        {
            if (event == 1)
            {
                set_alarm();
            }
            else if (event == 2)
            {
                view_event();
            }
            open = 0;
            change_detect = 0;
        }
        else if (display == 2 && open == 1)
        {
            if (event == 1)
            {
                set_time();
            }
            else if (event == 2)
            {
                set_date();
            }
            open = 0;
            change_detect = 0;
        }

        // UP Key handling: long press enters a highlighted sub-menu item, short press navigates
        char key = key_detect();

        if (key == UP_LONG_PRESS)
        {
            // Enter (open) the currently highlighted sub-menu item
            if (event == 1 || event == 2)
                open = 1;

            // From the top level of either page, move into that page's sub-menu
            if ((display == 1 && event == 0) || (display == 2 && event == 0))
                event = 1;

            change_detect = 0;
        }
        else if (key == UP_SHORT_PRESS)
        {
            // Switch back to the event menu page from the time/date page
            if (display == 2 && event == 0)
                display = 1;

            // Move selection up within a sub-menu
            if (event == 2)
                event = 1;

            change_detect = 0;
        }

        // DOWN Key handling: long press exits the current level, short press navigates
        if (key == DOWN_LONG_PRESS)
        {
            // Already at the top level: exit config mode back to the default/home screen
            if (event == 0)
            {
                screen = 0;
                return;
            }

            // Back out of a sub-menu to the top level
            event = 0;

            change_detect = 0;
        }
        else if (key == DOWN_SHORT_PRESS)
        {
            // Switch to the time/date menu page from the event page
            if (display == 1 && event == 0)
                display = 2;

            // Move selection down within a sub-menu
            if (event == 1)
                event = 2;

            change_detect = 0;
        }

    } // while end
}
