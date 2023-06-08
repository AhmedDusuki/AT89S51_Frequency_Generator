#include <reg51.h>

#define MACHINE_CYCLE_US 0.75   // machine cycle in us, 1.085 using 11.0592MHz, 0.75 using 16MHz
#define OVERHEAD 14             // overhead in cycles for delay <= 65536 cycles
#define DISPLAY_UPPER_BYTE 0xFE // upper_byte for timer for display
#define DISPLAY_LOWER_BYTE 0x00 // lower_byte for timer for display
#define DEBOUNCES 6             // times to repeat 65536 delay for debounce

// quad 7 segment connected to port 2 and half of port 0
sbit a = P2 ^ 0;
sbit b = P2 ^ 1;
sbit c = P2 ^ 2;
sbit d = P2 ^ 3;
sbit e = P2 ^ 4;
sbit f = P2 ^ 5;
sbit g = P2 ^ 6;
sbit dp = P2 ^ 7;
sbit seg_en1 = P0 ^ 0;
sbit seg_en2 = P0 ^ 1;
sbit seg_en3 = P0 ^ 2;
sbit seg_en4 = P0 ^ 3;

// keypad connected to port 1
sbit R1 = P1 ^ 0;
sbit R2 = P1 ^ 1;
sbit R3 = P1 ^ 2;
sbit R4 = P1 ^ 3;
sbit C1 = P1 ^ 4;
sbit C2 = P1 ^ 5;
sbit C3 = P1 ^ 6;
sbit C4 = P1 ^ 7;

sbit LED = P3 ^ 6;
sbit FREQ_OUT = P3 ^ 7;

typedef union cycles {
    short unsigned int count;
    char byte[2];
} cycles;

unsigned char upper_byte, lower_byte;

void debounce_delay(void)
{
    unsigned char i;
    IT0 = 0;
    TH0 = 0;
    TL0 = 0;
    TR0 = 1;
    for (i = DEBOUNCES; i != 0; --i)
    {
        while (TF0 == 0)
            ;
        TF0 = 0;
    }
    TR0 = 0;
    IT0 = 1;
}

long int t;
unsigned char delay_count_org, delay_count;
bit delay_remainder;

bit pulse = 0;
bit small_delay = 1;
unsigned char nums[] = {0, 0, 0, 1}; // nums[3] is the units digit (least significant)

void main(void)
{
    unsigned char digit;

    IT0 = 1; // Configure interrupt 0 for falling edge on /INT0 (P3.2)
    EX0 = 1; // Enable EX0 Interrupt
    ET0 = 1; // Enable ET0 Interrupt
    ET1 = 1; // Enable ET1 Interrupt
    EA = 1;  // Enable Global Interrupt Flag
    LED = 0;
    FREQ_OUT = 0;
    TMOD = 0x11;              // Set both timers to mode 1
    TH1 = DISPLAY_UPPER_BYTE; // Set display timer delay
    TL1 = DISPLAY_LOWER_BYTE;
    TR1 = 1;

    while (1)
    {
        if (~pulse)
        {
            digit = -1;
            R1 = R2 = R3 = R4 = 1;
            C1 = C2 = C3 = C4 = 0;
            if (!R1)
            {
                C1 = C2 = C3 = C4 = 1;
                R1 = R2 = R3 = R4 = 0;
                if (!C1)
                {
                    digit = 1;
                }
                else if (!C2)
                {
                    digit = 2;
                }
                else if (!C3)
                {
                    digit = 3;
                }
            }
            else if (!R2)
            {
                C1 = C2 = C3 = C4 = 1;
                R1 = R2 = R3 = R4 = 0;
                if (!C1)
                {
                    digit = 4;
                }
                else if (!C2)
                {
                    digit = 5;
                }
                else if (!C3)
                {
                    digit = 6;
                }
            }
            else if (!R3)
            {
                C1 = C2 = C3 = C4 = 1;
                R1 = R2 = R3 = R4 = 0;
                if (!C1)
                {
                    digit = 7;
                }
                else if (!C2)
                {
                    digit = 8;
                }
                else if (!C3)
                {
                    digit = 9;
                }
            }
            else if (!R4)
            {
                C1 = C2 = C3 = C4 = 1;
                R1 = R2 = R3 = R4 = 0;
                if (!C2)
                {
                    digit = 0;
                }
            }
            if (digit != -1)
            { // shift numbers to the left, and add new digit
                nums[0] = nums[1];
                nums[1] = nums[2];
                nums[2] = nums[3];
                nums[3] = digit;
                debounce_delay();
            }
        }
    }
}

void ex0_isr(void) interrupt 0
{
    if (pulse)
    { // reset signals (LED, FREQ_OUT) to 0 when switched off
        TR0 = 0;
        LED = 0;
        FREQ_OUT = 0;
        nums[0] = 0;
        nums[1] = 0;
        nums[2] = 0;
        nums[3] = 0;
    }
    else
    {
        cycles delay_cycles;
        unsigned short int f = (nums[0] * 1000 + nums[1] * 100 + nums[2] * 10 + nums[3]);
        if (!f)
        {
            return;
        }
        t = 500000 / f;           // half-period time in microseconds
        t = t / MACHINE_CYCLE_US; // number of cycles
        t = t - OVERHEAD;
        if (t < 0)
        {
            t = 0;
        }
        if (t <= 65536)
        {
            t = 65536 - t;
            delay_cycles.count = t;
            upper_byte = delay_cycles.byte[0]; // assuming little endian
            lower_byte = delay_cycles.byte[1];
            TH0 = upper_byte;
            TL0 = lower_byte;
            small_delay = 1;
        }
        else
        {
            TH0 = 0;
            TL0 = 0;
            delay_count = delay_count_org = (t / 65536) - 1; // delay_count - 1
            delay_cycles.count = 65536 - (t - (delay_count_org + 1) * 65536);
            delay_remainder = 1;
            upper_byte = delay_cycles.byte[0]; // assuming little endian
            lower_byte = delay_cycles.byte[1];
            small_delay = 0;
        }
        TR0 = 1;
    }
    pulse = ~pulse;
}

void t0_isr(void) interrupt 1
{ // frequency generator
    TF0 = 0;
    if (small_delay)
    {
        LED = ~LED;
        FREQ_OUT = ~FREQ_OUT;
        TH0 = upper_byte;
        TL0 = lower_byte;
    }
    else
    {
        if (delay_count)
        {
            --delay_count;
        }
        else
        {
            if (delay_remainder)
            {
                delay_remainder = 0;
                TH0 = upper_byte;
                TL0 = lower_byte;
            }
            else
            {
                LED = ~LED;
                FREQ_OUT = ~FREQ_OUT;
                delay_count = delay_count_org;
                delay_remainder = 1;
                TH0 = 0;
                TL0 = 0;
            }
        }
    }
}

char segment_digits[] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90};
bit updated = 0;
unsigned char dig = 0; // current digit to refresh, 0 -> 3

void t1_isr(void) interrupt 3
{ // refresh display
    TH1 = DISPLAY_UPPER_BYTE;
    TL1 = DISPLAY_LOWER_BYTE;
    TF1 = 0;

    if (updated)
    {
        updated = 0;
        switch (dig)
        {
        case 0:
            seg_en1 = 0;
            dig = 1;
            break;
        case 1:
            seg_en2 = 0;
            dig = 2;
            break;
        case 2:
            seg_en3 = 0;
            dig = 3;
            break;
        default:
            seg_en4 = 0;
            dig = 0;
            break;
        }
    }
    else
    {
        updated = 1;
        P2 = segment_digits[nums[dig]];
        switch (dig)
        {
        case 0:
            seg_en1 = 1;
            break;
        case 1:
            seg_en2 = 1;
            break;
        case 2:
            seg_en3 = 1;
            break;
        default:
            seg_en4 = 1;
            break;
        }
    }
}