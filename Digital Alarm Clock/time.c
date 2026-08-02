#include "time.h"
#include "rtc.h"
#include "clcd.h"
#include "matrix_keypad.h"
#include "alarm.h"


// Oscillator frequency used by the __delay_ms()/__delay_us() macros
#define _XTAL_FREQ 20000000

// Blink toggle flag (defined in alarm.c) used to flash the field being edited
extern unsigned char blink;

// Raw DS1307 register bytes: [0]=hour, [1]=minute, [2]=second
unsigned char clock_reg[3];
// Display-ready "HH:MM:SS" time string
unsigned char time[9];

// Raw DS1307 register bytes: [0]=year, [1]=month, [2]=date, [3]=day-of-week
unsigned char calender_reg[4];
// Display-ready "DD-MM-YYYY" date string
unsigned char date[11];

// Prints the current date string to line 2 of the LCD
void display_date(void)
{
    clcd_print(date, LINE2(5));
}

// Prints the current time string to line 1 of the LCD, along with AM/PM
// when the RTC is running in 12-hour mode
void display_time(void)
{
    clcd_print(time, LINE1(5));

    // Bit 0x40 of the hour register indicates 12-hour mode is active
    if (clock_reg[0] & 0x40)
    {
        // Bit 0x20 indicates PM when set, AM when clear
        if (clock_reg[0] & 0x20)
        {
            clcd_print("PM", LINE1(14));
        }
        else
        {
            clcd_print("AM", LINE1(14));
        }
    }
}

// Prints only the hour:minute portion (plus AM/PM) of the current time,
// used by screens (e.g. set_alarm) that only need HH:MM shown alongside the clock
void time_hr_min(void)
{
    // Time with hour and minute 
    
    clcd_putch(time[0], LINE1(6));
    clcd_putch(time[1], LINE1(7));
    clcd_putch(time[2], LINE1(8));
    clcd_putch(time[3], LINE1(9));
    clcd_putch(time[4], LINE1(10));
    
    // Bit 0x40 of the hour register indicates 12-hour mode is active
    if (clock_reg[0] & 0x40)
    {
        // Bit 0x20 indicates PM when set, AM when clear
        if (clock_reg[0] & 0x20)
            clcd_print("PM", LINE1(12));
        else
            clcd_print("AM", LINE1(12)); 
    }
}

// Reads the current date/day-of-week from the DS1307 RTC and formats it
// into the display-ready date[] string as "DD-MM-20YY"
void get_date(void)
{
    calender_reg[0] = read_ds1307(YEAR_ADDR);
    calender_reg[1] = read_ds1307(MONTH_ADDR);
    calender_reg[2] = read_ds1307(DATE_ADDR);
    calender_reg[3] = read_ds1307(DAY_ADDR);

    // Each RTC byte is BCD-encoded: high nibble = tens digit, low nibble = units digit
    date[0] = '0' + ((calender_reg[2] >> 4) & 0x0F);
    date[1] = '0' + (calender_reg[2] & 0x0F);
    date[2] = '-';
    date[3] = '0' + ((calender_reg[1] >> 4) & 0x0F);
    date[4] = '0' + (calender_reg[1] & 0x0F);
    date[5] = '-';
    // Century is hardcoded as "20" since the RTC only stores a 2-digit year
    date[6] = '2';
    date[7] = '0';
    date[8] = '0' + ((calender_reg[0] >> 4) & 0x0F);
    date[9] = '0' + (calender_reg[0] & 0x0F);
    date[10] = '\0';
}

// Read current time from DS1307 RTC

void get_time(void)
{
    clock_reg[0] = read_ds1307(HOUR_ADDR);
    clock_reg[1] = read_ds1307(MIN_ADDR);
    clock_reg[2] = read_ds1307(SEC_ADDR);

    // Bit 0x40 of the hour register indicates 12-hour mode
    if (clock_reg[0] & 0x40)
    {
        // In 12-hour mode the hour tens digit only uses 1 bit (0x10), so mask with 0x01
        time[0] = '0' + ((clock_reg[0] >> 4) & 0x01);
        time[1] = '0' + (clock_reg[0] & 0x0F);
    }
    else 
    {
        // In 24-hour mode the hour tens digit can be 0-2, so mask with 0x03
        time[0] = '0' + ((clock_reg[0] >> 4) & 0x03);
        time[1] = '0' + (clock_reg[0] & 0x0F);
    }
    time[2] = ':';
    time[3] = '0' + ((clock_reg[1] >> 4) & 0x0F);
    time[4] = '0' + (clock_reg[1] & 0x0F);
    time[5] = ':';
    time[6] = '0' + ((clock_reg[2] >> 4) & 0x0F);
    time[7] = '0' + (clock_reg[2] & 0x0F);
    time[8] = '\0';
}

// Convert decimal value to BCD format

unsigned char dec_to_bcd(unsigned char val)
{
    // Packs tens digit into the high nibble and units digit into the low nibble
    return ((val / 10) << 4) | (val % 10);
}

// Convert BCD value to decimal format

unsigned char bcd_to_dec(unsigned char val)
{
    // Unpacks tens digit from the high nibble and units digit from the low nibble
    return ((val >> 4) & 0x0F) * 10 + (val & 0x0F);
}

// Same as bcd_to_dec() but masks the tens nibble to 1 bit, for use with the
// DS1307 hour register in 12-hour mode (where hour only ranges 1-12)
unsigned char bcd_to_dec_12hr(unsigned char val)
{
    return ((val >> 4) & 0x01) * 10 + (val & 0x0F);
}

// Initializes the DS1307 RTC with a default time (12:00:00 AM) and date
// (28/07/26), intended for first-time setup or after power loss
void set_default_time_and_date(void)
{

    unsigned char hour = dec_to_bcd(12);
    hour |= 0x40;      // 12-hour mode
    hour &= ~0x20;     // AM (set 0x20 for PM)

    write_ds1307(HOUR_ADDR, hour);
    write_ds1307(MIN_ADDR, dec_to_bcd(0));
    write_ds1307(SEC_ADDR, dec_to_bcd(0));

    write_ds1307(DATE_ADDR, dec_to_bcd(28));
    write_ds1307(MONTH_ADDR, dec_to_bcd(7));
    write_ds1307(YEAR_ADDR, dec_to_bcd(26));
}

// Lets the user set the RTC's hour, minute, second, and AM/PM using
// LEFT/RIGHT to change field and UP/DOWN to change value, then writes
// the new time back to the DS1307 when UP is held to exit
void set_time(void)
{
    // Read current RTC time
    // NOTE: time_meridian is read but not used below (hour is derived via bcd_to_dec_12hr instead)
    unsigned char time_meridian = read_ds1307(HOUR_ADDR);
    unsigned char time_meridian_flag = 0;

    unsigned char hour = bcd_to_dec_12hr(read_ds1307(HOUR_ADDR));
    unsigned char minute = bcd_to_dec(read_ds1307(MIN_ADDR));
    unsigned char second = bcd_to_dec(read_ds1307(SEC_ADDR));

    // Used to select hour/minute/second field
    unsigned char field = 0; // start at hour

    unsigned char key;
    unsigned char flag;
    int upkey_count = 0;

    // Refresh the global time[] string so the screen starts with the current time shown
    get_time();

    CLEAR_DISP_SCREEN;
    clcd_print("HH MM SS    SET", LINE1(0));
    clcd_print("            TIME", LINE2(0));

    while (1)
    {
        char exit_key = read_matrix_keypad(LEVEL_CHANGE);

        // Holding UP for ~500 polls exits the edit loop and saves the new time
        if (exit_key == UP_KEY)
        {
            if (++upkey_count >= 500)
            {
                upkey_count = 0;
                break;
            }
        }
        else
        {
            upkey_count = 0;
        }
        
        clcd_print(time, LINE2(0));
        
        // Show AM/PM indicator except while the AM/PM field itself is being edited (field 3 draws it separately)
        if(field != 3)
        {
           if(time_meridian_flag == 0)
               clcd_print("AM", LINE2(9));
           else 
               clcd_print("PM", LINE2(9));
        }

        key = key_detect();
       

        // Field Change - move between hour(0)/minute(1)/second(2)/AM-PM(3)
        if (key == LEFT_PRESS)
        {
            if (field > 0)
                field--;
        }
        else if (key == RIGHT_PRESS)
        {
            if (field < 4)
                field++;
        }


        // Hour - active when field == 0
        if (field == 0)
        {
            time[3] = '0' + minute / 10;
            time[4] = '0' + minute % 10;
            time[6] = '0' + second / 10;
            time[7] = '0' + second % 10;

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
                time[0] = '0' + hour / 10;
                time[1] = '0' + hour % 10;
            }
            else
            {
                time[0] = ' ';
                time[1] = ' ';
            }
        }

        // Minutes - active when field == 1
        if (field == 1)
        {
            time[0] = '0' + hour / 10;
            time[1] = '0' + hour % 10;
            time[6] = '0' + second / 10;
            time[7] = '0' + second % 10;

            // Increment/decrement minute, wrapping within 0-59
            if (key == UP_PRESS)
            {
                minute = ++minute % 60;
            }
            else if(key == DOWN_PRESS)
            {
                // NOTE: minute is decremented here and again inside the branches below,
                // so a DOWN press decrements the value twice as written
                --minute;
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
                time[3] = '0' + minute / 10;
                time[4] = '0' + minute % 10;
            }
            else
            {
                time[3] = ' ';
                time[4] = ' ';
            }
        }
        // Seconds - active when field == 2
        if (field == 2)
        {
            time[0] = '0' + hour / 10;
            time[1] = '0' + hour % 10;
            time[3] = '0' + minute / 10;
            time[4] = '0' + minute % 10;

            // Increment/decrement second, wrapping within 0-59
            if (key == UP_PRESS)
            {
                second = ++second % 60;
            }
            else if(key == DOWN_PRESS)
            {
                
                if(second == 0)
                {
                    second = 59;
                }
                else
                {
                    --second;
                }
            }

            // Blink the second digits to indicate this field is being edited
            if (blink)
            {
                time[6] = '0' + second / 10;
                time[7] = '0' + second % 10;
            }
            else
            {
                time[6] = ' ';
                time[7] = ' ';
            }
        }
        // AM/PM - active when field == 3
        else if (field == 3)
        {
            time[0] = '0' + hour / 10;
            time[1] = '0' + hour % 10;
            time[3] = '0' + minute / 10;
            time[4] = '0' + minute % 10;
            time[6] = '0' + second / 10;
            time[7] = '0' + second % 10;
            
            // Either UP or DOWN toggles AM/PM in this field
            if (key == UP_PRESS)
            {
                time_meridian_flag = !time_meridian_flag;
            }
            else if(key == DOWN_PRESS)
            {
                time_meridian_flag = !time_meridian_flag;
            }
            
            // Blink the AM/PM text to indicate this field is being edited
            if (time_meridian_flag == 0)
            {
                if (blink)
                    clcd_print("AM", LINE2(9));
                else
                    clcd_print("  ", LINE2(9));
            }
            else if(time_meridian_flag == 1)
            {
                if (blink)
                    clcd_print("PM", LINE2(9));
                else
                    clcd_print("  ", LINE2(9));
            }
        }

    } // while end

    // Pack the edited hour into BCD and re-apply the 12-hour mode / AM-PM bits
    unsigned char hour_reg = dec_to_bcd(hour);

    hour_reg |= 0x40; // 12-hour mode

    if (time_meridian_flag == 1)
    {
        hour_reg |= 0x20; // PM
    }

    // Persist the new time to the DS1307 RTC
    write_ds1307(HOUR_ADDR, hour_reg);
    write_ds1307(MIN_ADDR, dec_to_bcd(minute));
    write_ds1307(SEC_ADDR, dec_to_bcd(second));

    // Confirm to the user
    CLEAR_DISP_SCREEN;
    clcd_print("SET TIME DONE", LINE1(0));
    clcd_print("SUCCESSFULLY", LINE2(1));
    __delay_ms(2000);
}

// Lets the user set the RTC's date, month, and year using LEFT/RIGHT to
// change field and UP/DOWN to change value, then writes the new date back
// to the DS1307 when UP is held to exit
void set_date(void)
{
    // Read current RTC time
    unsigned char dates = bcd_to_dec(read_ds1307(DATE_ADDR));
    unsigned char month = bcd_to_dec(read_ds1307(MONTH_ADDR));
    char year = bcd_to_dec(read_ds1307(YEAR_ADDR));

    // select Date / Month / Year field
    unsigned char field = 0;

    unsigned char key;
    int upkey_count = 0;

    // Refresh the global date[] string so the screen starts with the current date shown
    get_date();

    CLEAR_DISP_SCREEN;
    clcd_print("DD MM YY    SET", LINE1(0));
    clcd_print("            DATE", LINE2(0));

    while (1)
    {
        char exit_key = read_matrix_keypad(LEVEL_CHANGE);

        // Holding UP for ~500 polls exits the edit loop and saves the new date
        if (exit_key == UP_KEY)
        {
            if (++upkey_count >= 500)
            {
                upkey_count = 0;
                break;
            }
        }
        else
        {
            upkey_count = 0;
        }
                
        clcd_print(date, LINE2(0));

        key = key_detect();
        

        // Field Change - move between date(0)/month(1)/year(2)
        if (key == LEFT_PRESS)
        {
            if (field > 0)
                field--;
        }
        else if (key == RIGHT_PRESS)
        {
            if (field < 2)
                field++;
        }

        // Date - active when field == 0
        if (field == 0)
        {
            date[3] = '0' + month / 10;
            date[4] = '0' + month % 10;
            date[8] = '0' + year / 10;
            date[9] = '0' + year % 10;

            // Increment/decrement day-of-month, wrapping within 1-31
            if (key == UP_PRESS)
            {
                dates++;
                if (dates > 31)
                {
                    dates = 1;
                }
            }
            else if(key == DOWN_PRESS)
            {
                dates--;
                if(dates == 0)
                {
                    dates = 31;
                }
            
            }

            // Blink the date digits to indicate this field is being edited
            if (blink)
            {
                date[0] = '0' + dates / 10;
                date[1] = '0' + dates % 10;
            }
            else
            {
                date[0] = ' ';
                date[1] = ' ';
            }
        }

        // Month - active when field == 1
        if (field == 1)
        {
            date[0] = '0' + dates / 10;
            date[1] = '0' + dates % 10;
            date[8] = '0' + year / 10;
            date[9] = '0' + year % 10;

            // Increment/decrement month, wrapping within 1-12
            if (key == UP_PRESS)
            {
                month++;
                if (month > 12)
                {
                    month = 1;
                }
            }
            else if(key == DOWN_PRESS)
            {
                month--;
                if(month == 0)
                {
                    month = 12;
                }
            
            }

            // Blink the month digits to indicate this field is being edited
            if (blink)
            {
                date[3] = '0' + month / 10;
                date[4] = '0' + month % 10;
            }
            else
            {
                date[3] = ' ';
                date[4] = ' ';
            }
        }

        // Year - active when field == 2
        if (field == 2)
        {
            date[0] = '0' + dates / 10;
            date[1] = '0' + dates % 10;
            date[3] = '0' + month / 10;
            date[4] = '0' + month % 10;

            // Increment year, wrapping within 0-99 (2-digit year)
            if (key == UP_PRESS)
            {
                year = ++year % 100;
            }
            else if(key == DOWN_PRESS)
            {
                year--;
                if(year < 0)
                {
                    year = 99;
                }
            }

            // Blink the year digits to indicate this field is being edited
            if (blink)
            {
                date[8] = '0' + year / 10;
                date[9] = '0' + year % 10;
            }
            else
            {
                date[8] = ' ';
                date[9] = ' ';
            }
        }

    } // while end

    // Persist the new date to the DS1307 RTC
    write_ds1307(DATE_ADDR, dec_to_bcd(dates));
    write_ds1307(MONTH_ADDR, dec_to_bcd(month));
    write_ds1307(YEAR_ADDR, dec_to_bcd(year));

    // Confirm to the user
    CLEAR_DISP_SCREEN;
    clcd_print("SET DATE DONE", LINE1(0));
    clcd_print("SUCCESSFULLY", LINE2(1));
    __delay_ms(2000);
}
