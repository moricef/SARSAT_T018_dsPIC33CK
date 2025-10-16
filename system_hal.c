/* system_hal.c
 * Hardware Abstraction Layer implementation for dsPIC33CK64MC105
 * Autonomous system functions and utilities
 */

#include "includes.h"
#include "system_definitions.h"
#include "system_debug.h"
#include <libpic30.h>

// Global system timer counter
volatile uint32_t millis_counter = 0;
static uint16_t timer_overflow_count = 0;

void oscillator_init(void);
void ports_init(void);
void timer_init(void);
void uart_init(void);
void uart2_init(void);
void spi_init(void);

// External chip timer callback from system_comms.c
extern void chip_timer_callback(void);

// System initialization
void system_init(void) {
    oscillator_init();
    ports_init();
    timer_init();  // Timer1 now runs at 38.4 kHz for chip timing
    uart_init();
    uart2_init();
    spi_init();

    // Enable global interrupts
    __builtin_enable_interrupts();

    DEBUG_LOG_FLUSH("System initialized\r\n");
}

// System oscillator configuration
void oscillator_init(void) {
    // Configure PLL: FRC=8MHz -> FOSC=200MHz, FCY=100MHz
    // FOSC = FRC × M / (N1 × N2 × N3) = 8MHz × 200 / (1 × 4 × 1) = 200MHz
    CLKDIVbits.PLLPRE = 1;      // N1=1
    PLLFBDbits.PLLFBDIV = 200;  // M=200
    PLLDIVbits.POST1DIV = 4;    // N2=4
    PLLDIVbits.POST2DIV = 1;    // N3=1

    // Initiate Clock Switch to FRC with PLL (NOSC=0b001)
    __builtin_write_OSCCONH(0x01);
    __builtin_write_OSCCONL(OSCCON | 0x01);

    // Wait for Clock switch to occur
    while (OSCCONbits.OSWEN != 0);

    // Wait for PLL to lock
    while (OSCCONbits.LOCK != 1);
    
    // Handle PLL lock failure
    if(!OSCCONbits.LOCK) {
        while(1);
    }
}

// GPIO port initialization
void ports_init(void) {
    // Disable analog functionality on all ports
    ANSELA = 0x0000;
    ANSELB = 0x0000;
    ANSELD = 0x0000;

    // DEBUG: RB0 for oscilloscope measurement of Timer1 ISR rate
    TRISBbits.TRISB0 = 0;     // RB0 output
    LATBbits.LATB0 = 0;       // Initially low

    // DEBUG: RA3 for oscilloscope measurement of SPI transfer time
    TRISAbits.TRISA3 = 0;     // RA3 output
    LATAbits.LATA3 = 0;       // Initially low

    // MCP4922 dual DAC CS pin configuration
    TRISBbits.TRISB2 = 0;     // RB2 output for MCP4922 CS
    LATBbits.LATB2 = 1;       // CS inactive (high)
    
    // RF control pins
    TRISBbits.TRISB15 = 0;    // RF amplifier enable
    LATBbits.LATB15 = 0;      // Initially disabled
    TRISBbits.TRISB11 = 0;    // Power level control
    LATBbits.LATB11 = 0;      // Low power mode
    
    // Status LED
    STATUS_LED_TRIS = 0;      // LED output
    STATUS_LED_LAT = 0;       // Initially off
    
    // SPI chip select pin
    ADF_CS_TRIS = 0;          // CS output
    ADF_CS_LAT = 1;           // CS inactive (high)
    
    // Mode switch configuration (Test/Exercise)
    MODE_SWITCH_TRIS = 1;     // Input
    CNPUCbits.CNPUC0 = 1;     // Enable pull-up (default = TEST mode)
}

// Timer1 initialization - now runs at 38.4 kHz for chip timing
void timer_init(void) {
    // Configure Timer1 for 38.4 kHz interrupts (chip rate)
    T1CONbits.TON = 0;      // Disable timer
    T1CONbits.TCKPS = 0;    // 1:1 prescaler (no division)
    T1CONbits.TCS = 0;      // Internal clock source (FCY = 100 MHz)

    // Set period for 38.4 kHz: FCY/38400 - 1 = 2604 - 1
    // FCY = 100,000,000 Hz, Target = 38,400 Hz
    // Period = 100,000,000 / 38,400 = 2604.17 cycles
    PR1 = 2604 - 1;
    TMR1 = 0;               // Clear counter

    // Configure interrupt
    IPC0bits.T1IP = 5;      // Interrupt priority 5 (high, for chip timing)
    IFS0bits.T1IF = 0;      // Clear interrupt flag
    IEC0bits.T1IE = 1;      // Enable interrupt

    T1CONbits.TON = 1;      // Start timer
}

// Timer2 initialization for T.018 chip clock (38.4 kHz)
// Duplicate function removed - only one timer2_init_chip_clock needed

// Timer1 interrupt service routine - now runs at 38.4 kHz
void __attribute__((__interrupt__, __auto_psv__)) _T1Interrupt(void) {
    static uint16_t chip_tick_counter = 0;
    static uint16_t debug_toggle_counter = 0;

    // DEBUG: Toggle RB0 every 1000 ISR calls = 26.04ms @ 38.4kHz (should be 38.4Hz = 26ms period)
    // Measure with oscilloscope to verify actual ISR rate
    debug_toggle_counter++;
    if(debug_toggle_counter >= 1000) {
        debug_toggle_counter = 0;
        LATBbits.LATB0 = !LATBbits.LATB0;  // Toggle RB0 for scope measurement
    }

    // Call chip timer callback at 38.4 kHz (every interrupt)
    chip_timer_callback();

    // Maintain millis_counter: 38.4 kHz / 38.4 = 1 kHz (1ms)
    chip_tick_counter++;
    if(chip_tick_counter >= 38) {  // ~38.4 ticks = 1ms
        chip_tick_counter = 0;
        millis_counter++;
        timer_overflow_count++;
    }

    IFS0bits.T1IF = 0;  // Clear interrupt flag
}

// UART initialization for debug output
void uart_init(void) {
    // Disable UART during configuration
    U1MODEbits.UARTEN = 0;
    U1MODEbits.UTXEN = 0;

    // Configure PPS for UART1 on RC10 (RP58/TX) and RC11 (RP59/RX)
    __builtin_write_OSCCONL(OSCCON | 0x40);   // Unlock PPS
    _U1RXR = 59;                               // RC11 (RP59) -> U1RX input
    _RP58R = 0x0001;                           // RC10 (RP58) -> U1TX output
    __builtin_write_OSCCONL(OSCCON & ~0x40);  // Lock PPS

    // Configure pins as digital I/O
    TRISCbits.TRISC10 = 0;    // RC10 output (TX)
    TRISCbits.TRISC11 = 1;    // RC11 input (RX)
    LATCbits.LATC10 = 1;      // TX idle high

    // Configure baud rate: FCY / (16 * (BRG + 1))
    U1BRG = (FCY / (16UL * BAUDRATE)) - 1;

    // Enable UART (T001 style)
    U1MODEbits.UARTEN = 1;  // Enable UART module
    U1MODEbits.UTXEN = 1;   // Enable transmitter
}

// UART2 initialization for Trimble GPS
void uart2_init(void) {
    // Disable UART2 during configuration
    U2MODEbits.UARTEN = 0;
    
    // Configure pins for UART2 (RC4=TX2, RC5=RX2)
    _RP52R = 3;               // U2TX on RC4/RP52
    _U2RXR = 53;              // U2RX on RC5/RP53
    
    // Configure baud rate for GPS: FCY / (16 * (BRG + 1))
    U2BRG = (FCY / (16UL * GPS_BAUDRATE)) - 1;
    
    // Enable UART2 (T001 style)
    U2MODEbits.UARTEN = 1;    // Enable UART module
    U2MODEbits.UTXEN = 1;     // Enable transmitter
}

// SPI initialization
void spi_init(void) {
    // Clear SPIx buffers
    SPI1BUFL = 0;
    SPI1BUFH = 0;

    // Configure SPI1 Master mode (SPIxCON1L[5] = 1)
    SPI1CON1L = 0x0000;         // Clear all bits
    SPI1CON1Lbits.MSTEN = 1;    // Master mode

    // Configure SPI baud rate: FCY=100MHz, MCP4922 max=20MHz
    // SPI_CLK = FCY / (2 × (SPI1BRGL + 1))
    // For 12.5MHz: 100MHz / (2 × (SPI1BRGL + 1)) = 12.5MHz → SPI1BRGL = 3
    // For 10MHz:   100MHz / (2 × (SPI1BRGL + 1)) = 10MHz   → SPI1BRGL = 4
    SPI1BRGL = 3;  // 100MHz / (2×4) = 12.5 MHz (safe for MCP4922 @ 20MHz max)
    // Result: 16-bit transfer @ 12.5MHz = 1.28µs (vs 26µs ISR period = OK!)

    // Clear SPIROV bit (SPIxSTATL[6])
    SPI1STATLbits.SPIROV = 0;

    // Enable SPI operation (SPIxCON1L[15])
    SPI1CON1Lbits.SPIEN = 1;

    // Verify SPI configuration (debug)
    char spi_debug[80];
    sprintf(spi_debug, "SPI init: BRGL=%u, MSTEN=%u, SPIEN=%u\r\n",
            SPI1BRGL, SPI1CON1Lbits.MSTEN, SPI1CON1Lbits.SPIEN);
    debug_print_string(spi_debug);
}

// Utility function: Set bit field in byte array
void set_bit_field(uint8_t* buffer, uint16_t start_bit, uint8_t num_bits, uint32_t value) {
    for(uint8_t i = 0; i < num_bits; i++) {
        uint16_t bit_pos = start_bit + i;
        uint16_t byte_index = bit_pos / 8;
        uint8_t bit_index = 7 - (bit_pos % 8);
        
        if(value & (1UL << (num_bits - 1 - i))) {
            buffer[byte_index] |= (1 << bit_index);
        } else {
            buffer[byte_index] &= ~(1 << bit_index);
        }
    }
}

// Utility function: Set bit field in byte array (64-bit version)
void set_bit_field_64(uint8_t* buffer, uint16_t start_bit, uint8_t num_bits, uint64_t value) {
    for(uint8_t i = 0; i < num_bits; i++) {
        uint16_t bit_pos = start_bit + i;
        uint16_t byte_index = bit_pos / 8;
        uint8_t bit_index = 7 - (bit_pos % 8);
        
        if(value & (1ULL << (num_bits - 1 - i))) {
            buffer[byte_index] |= (1 << bit_index);
        } else {
            buffer[byte_index] &= ~(1 << bit_index);
        }
    }
}

// Utility function: Get bit field from byte array
uint32_t get_bit_field(const uint8_t* buffer, uint16_t start_bit, uint8_t num_bits) {
    uint32_t result = 0;
    
    for(uint8_t i = 0; i < num_bits; i++) {
        uint16_t bit_pos = start_bit + i;
        uint16_t byte_index = bit_pos / 8;
        uint8_t bit_index = 7 - (bit_pos % 8);
        
        if(buffer[byte_index] & (1 << bit_index)) {
            result |= (1UL << (num_bits - 1 - i));
        }
    }
    
    return result;
}

// Utility function: Get 64-bit field from byte array
uint64_t get_bit_field_64(const uint8_t* buffer, uint16_t start_bit, uint8_t num_bits) {
    uint64_t result = 0;
    
    for(uint8_t i = 0; i < num_bits; i++) {
        uint16_t bit_pos = start_bit + i;
        uint16_t byte_index = bit_pos / 8;
        uint8_t bit_index = 7 - (bit_pos % 8);
        
        if(buffer[byte_index] & (1 << bit_index)) {
            result |= (1ULL << (num_bits - 1 - i));
        }
    }
    
    return result;
}

// Hardware abstraction functions
void toggle_status_led(void) {
    LED_TOGGLE();
}

uint32_t get_system_time_ms(void) {
    return millis_counter;
}

void system_delay_ms(uint16_t ms) {
    uint32_t start_time = millis_counter;
    while((millis_counter - start_time) < ms) {
        // Wait
    }
}
