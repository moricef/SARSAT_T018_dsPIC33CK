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
void timer2_init_chip_clock(void);
void uart_init(void);
void uart2_init(void);
void spi_init(void);

void __attribute__((__interrupt__, __auto_psv__)) _CCP1Interrupt(void);

// Timer2 initialization for T.018 chip clock (38.4 kHz)
void timer2_init_chip_clock(void) {
    // dsPIC33CK64MC105 : Utilisation CCP1 pour timing précis 38.4 kHz
    // Timer1 déjà utilisé pour system tick, CCP1 indépendant
    
    // Disable CCP1 during configuration
    CCP1CON1Lbits.CCPON = 0;
    
    // Configure CCP1 in Compare mode with Special Event Trigger
    CCP1CON1Lbits.MOD = 0b0101;    // Compare mode, trigger special event
    CCP1CON1Lbits.T32 = 0;         // 16-bit timer mode
    CCP1CON1Lbits.TMRSYNC = 0;     // Timer synchronization disabled
    CCP1CON1Lbits.CLKSEL = 0;      // System clock (FCY) as source
    CCP1CON1Lbits.TMRPS = 0;       // 1:1 prescaler for maximum precision
    
    // Calculate compare value for 38.4 kHz
    // FCY = 100MHz, Target = 38.4kHz
    // Period = FCY / Target = 100,000,000 / 38,400 = 2604.17 cycles
    // Using 2604 gives 38.402kHz (error = +0.005%)
    CCP1PRL = 2604 - 1;            // Set period for 38.4kHz
    CCP1PRH = 0;                   // High word = 0 for 16-bit mode
    
    // Clear timer
    CCP1TMRL = 0;
    CCP1TMRH = 0;
    
    // Configure interrupt for chip timing
    IPC1bits.CCP1IP = 5;           // High priority (above system tick)
    IFS0bits.CCP1IF = 0;           // Clear interrupt flag
    IEC0bits.CCP1IE = 1;           // Enable CCP1 interrupt
    
    // Enable CCP1 module
    CCP1CON1Lbits.CCPON = 1;
    
    DEBUG_LOG_FLUSH("T.018 CCP1 chip clock initialized (38.400 kHz)\r\n");
}

// System initialization
void system_init(void) {
    oscillator_init();
    ports_init();
    timer_init();
    timer2_init_chip_clock();  // T.018 chip rate timer
    uart_init();
    uart2_init();
    spi_init();
    
    // Enable global interrupts
    __builtin_enable_interrupts();
    
    DEBUG_LOG_FLUSH("System initialized\r\n");
}

// System oscillator configuration
void oscillator_init(void) {
    // Select FRC as primary clock source
    __builtin_write_OSCCONH(0x0000);
    __builtin_write_OSCCONL(OSCCON | 0x01);
    while(OSCCONbits.OSWEN);
    
    // Configure PLL for 100MHz operation
    // FRC = 8MHz, Target FCY = 100MHz, FOSC = 200MHz
    // FOSC = FIN × M / (N1 × N2 × N3) = 8 × 200 / (2 × 4 × 1) = 200MHz
    CLKDIVbits.PLLPRE = 1;     // N1 = 2 (Input divider)
    PLLFBD = 199;              // M = 200 (Multiplier, register = M-1)
    PLLDIVbits.POST1DIV = 3;   // N2 = 4 (Post divider 1)
    PLLDIVbits.POST2DIV = 0;   // N3 = 1 (Post divider 2)

    // Activate FRC with PLL
    __builtin_write_OSCCONH(0x01);
    __builtin_write_OSCCONL(OSCCON | 0x01);
    
    // Wait for PLL lock
    while(OSCCONbits.OSWEN);
    while(!OSCCONbits.LOCK);
    
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

// Timer1 initialization for system tick
void timer_init(void) {
    // Configure Timer1 for 1ms interrupts
    T1CONbits.TON = 0;      // Disable timer
    T1CONbits.TCKPS = 2;    // 1:64 prescaler
    T1CONbits.TCS = 0;      // Internal clock source
    
    // Set period for 1ms: FCY/64/1000 - 1
    PR1 = (FCY/64/1000) - 1;
    TMR1 = 0;               // Clear counter
    
    // Configure interrupt
    IPC0bits.T1IP = 4;      // Interrupt priority 4
    IFS0bits.T1IF = 0;      // Clear interrupt flag
    IEC0bits.T1IE = 1;      // Enable interrupt
    
    T1CONbits.TON = 1;      // Start timer
}

// Timer2 initialization for T.018 chip clock (38.4 kHz)
// Duplicate function removed - only one timer2_init_chip_clock needed

// Timer1 interrupt service routine
void __attribute__((__interrupt__, __auto_psv__)) _T1Interrupt(void) {
    millis_counter++;
    timer_overflow_count++;
    
    // Toggle LED every 500ms for heartbeat
    if((millis_counter % 500) == 0) {
        LED_TOGGLE();
    }
    
    IFS0bits.T1IF = 0;  // Clear interrupt flag
}

// CCP1 interrupt service routine - T.018 chip clock à 38.4 kHz précis
void __attribute__((__interrupt__, __auto_psv__)) _CCP1Interrupt(void) {
    // ISR appelée exactement à 38.400 kHz pour sortie chip T.018
    // Gestion des symboles OQPSK avec timing hardware précis
    
    // Signal disponible pour modules OQPSK/transmission
    // Implémentation complète dans system_comms.c chip_timer callback
    
    // Clear CCP1 interrupt flag
    IFS0bits.CCP1IF = 0;
}

// UART initialization for debug output
void uart_init(void) {
    // Disable UART during configuration
    U1MODEbits.UARTEN = 0;
    
    // Use default UART1 pins (no remapping required)
    
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

// SPI initialization for dsPIC33CK (official datasheet DS70005399D)
void spi_init(void) {
    // Clear SPIx buffers
    SPI1BUFL = 0;
    SPI1BUFH = 0;
    
    // Configure SPI1 Master mode (SPIxCON1L[5] = 1)
    SPI1CON1L = 0x0000;         // Clear all bits
    SPI1CON1Lbits.MSTEN = 1;    // Master mode
    
    // Clear SPIROV bit (SPIxSTATL[6])
    SPI1STATLbits.SPIROV = 0;
    
    // Enable SPI operation (SPIxCON1L[15])
    SPI1CON1Lbits.SPIEN = 1;
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
