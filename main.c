#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include "constants.h"

/**
 * FUNCTION DECLARATIONS
 */
void configure_ports(void);
void configure_adc(void);
void configure_tmr(void);
void configure_pwm(void);

uint16_t do_adc(void);
uint16_t convert_adc_to_pwm(void);
uint16_t safe_substract(uint16_t a, uint16_t b);
void set_duty(uint16_t duty);


/**
 * VARIABLES
 */
uint8_t do_stuff;
uint16_t new_pwm_val;
uint16_t old_pwm_val;
uint8_t tmr_cnt;

/**
 * MAIN
 */
void main(void) {
    
    configure_ports();
    configure_adc();
    configure_pwm();
    configure_tmr();
    
    set_duty(PWM_STILL);
    do_stuff = 0;
    new_pwm_val = PWM_STILL;
    old_pwm_val = PWM_STILL;

    while(1) {
        
        if (do_stuff) {
            tmr_cnt++;
            uint16_t speed = PWM_STILL;
            if (ENA_Pin == 0) { // Enable is active low
                // Get ADC value
                uint16_t adc = do_adc();
                adc = ((adc >> SPEED_RANGE) & PWM_STILL); // Divide 1/2

                // Calculate speed depending on direction
                if (DIR_Pin) {
                    speed = (PWM_STILL + adc);
                } else {
                    speed = (PWM_STILL - adc);
                }
                
                // Blink led fast
                if (tmr_cnt > 2) {
                    TEST_Pin = !TEST_Pin;
                    tmr_cnt = 0;
                }
            } else {
                // Blink led slow
                if (tmr_cnt > 20) {
                    TEST_Pin = !TEST_Pin;
                    tmr_cnt = 0;
                }
            }
            
            // Ramp to value
            if (speed > old_pwm_val) {
                // Quick ramp or fine ramp
                if (speed > (old_pwm_val + PWM_MAX_STEP)) {
                    new_pwm_val = old_pwm_val + PWM_MAX_STEP;
                } else {
                    new_pwm_val = old_pwm_val + 1;
                }
            } else if (speed == old_pwm_val){
                new_pwm_val = speed;
            } else {
                if (speed < safe_substract(old_pwm_val, PWM_MAX_STEP)) {
                    new_pwm_val = safe_substract(old_pwm_val, PWM_MAX_STEP);
                } else {
                    new_pwm_val = safe_substract(old_pwm_val, 1);
                }
            }
            
            set_duty(new_pwm_val);
            old_pwm_val = new_pwm_val;
            do_stuff = 0;
        }
    }
    return;
}

/**
 * FUNCTIONS
 */
void configure_ports() {
    // Initially all outputs and digital
    ANSEL = 0x00;
    CMCON0 = 0x07;
    
    TRISA = 0x00;
    PORTA = 0x00;
    
    TRISC = 0x00;
    PORTC = 0x00;
    
    // Set directions
    ANSELbits.ANS4 = 1; // Analog pin
    ADC_Dir = 1;
    
    PWM_1_Dir = 0; // P1A output
    PWM_2_Dir = 0; // P1B output
    
    DIR_Dir = 1; // Input direction pin
    ENA_Dir = 1; // Enable is output
    
    TEST_Dir = 0; // Test is output
    TEST_Pin = 0;
}

void configure_adc() {
    // ADCON1 register
    ADCON1bits.ADCS = 0b110; // Tclk = 62.5 ns, Tad should be larger than 2µs => Tclk * 64 = 4µs = Tad
    
    // ADCON0 register
    ADCON0bits.CHS = 0b100; // Channel 0
    ADCON0bits.ADFM = 1; // Right justified
    ADCON0bits.ADON = 1; // Enable ADC
}

void configure_tmr() {
    // Interrupts
    PIR1bits.TMR1IF = 0; // Clear flag
    PIE1bits.TMR1IE = 1; // Enable timer 1 interrupts
    INTCONbits.PEIE = 1; // Peripheral interrupt enable
    INTCONbits.GIE = 1; // Global interrupt enable
    
    // T1CON register
    T1CONbits.T1CKPS = 0b10; // 1:4 Pre-scale value: Fclk/4 = 4MHz -> 4MHz / 4 = 1MHz -> 1MHz / (2 ^ 16)) = 15Hz (65ms)
    T1CONbits.TMR1CS = 0; // FOSC/4 internal clock
    T1CONbits.TMR1ON = 1; // Enable timer 1
}

void configure_pwm() {
    // CCP1CON register
    CCP1CONbits.P1M0 = 0; // Half-bridge output; P1A, P1B modulated with dead 
    CCP1CONbits.P1M1 = 1; // band control.  P1C, P1D assigned as port pins
    CCP1CONbits.CCP1M = 0b1100; //  PWM mode; P1x active-high
    
    // T2CON register
    T2CONbits.TMR2ON = 0; // Wait
    T2CONbits.T2CKPS = 0b00; // 1:1 pre-scale
    PR2 = 0xFF; // Max value
    
    // PWM1CON register
    PWM1CONbits.PRSEN = 1; //  Upon  auto-shutdown,  the  ECCPASE  bit  clears  automatically  once  the  shutdown  event goes away; the PWM restarts automatically.
    PWM1CONbits.PDC = 1; // PWM Delay count bits in TAD
    
    // ECCPAS register
    ECCPASbits.ECCPAS = 0b000; // VIL on INT pin
    ECCPASbits.PSSAC = 0b00; // Drive pins A and C to '0'
    ECCPASbits.PSSBD = 0b01; // Drive pins B and D to '1'
    
    // Enable
    T2CONbits.TMR2ON = 1; // Enable timer 2
}

uint16_t safe_substract(uint16_t a, uint16_t b) {
    if (b < a) {
        return (a-b);
    } else {
        return 0;
    }
}

uint16_t do_adc() {
    uint16_t tmp;

    ADCON0bits.GO_DONE = 1; // Start conversion
    while(ADCON0bits.GO_DONE) {} // Wait for conversion
    
    tmp = ((ADRESH << 8) + ADRESL) & 0x03FF; // Read AD value
    
    __delay_us(10); // Wait for more than 2 TAD
    return tmp;
}

void set_duty(uint16_t duty) {
    // Safety
    if (duty > PWM_MAX) {
        duty = PWM_MAX;
    }
    if (duty < PWM_MIN) {
        duty = PWM_MIN;
    }
   
    // LSB
    CCP1CONbits.DCB = (duty & 0x0003);
    // MSB
    CCPR1L = ((duty>>2) & 0x00FF);
}

void interrupt inter(void) {
    if (PIR1bits.TMR1IF) {
        // Don't do stuff if still busy
        if (do_stuff == 0) {
            do_stuff = 1;
        }
        PIR1bits.TMR1IF = 0;
    }
}

///// OLD RAMPING
//// Ramp PWM duty cycle
//            if (adc > pwm_val) {
//                if (adc > (pwm_val+10)) {
//                    pwm_val += 8; // Rough ramping
//                } else {
//                    pwm_val += 1; // Fine tune
//                }
//                
//                if (pwm_val > 0x03FF) {
//                    pwm_val = 0x03FF;
//                }
//            }
//            if (adc < pwm_val) {
//                if (adc < (pwm_val-10)) {
//                    if (pwm_val >= 8) {
//                        pwm_val -= 8;
//                    } else {
//                        pwm_val -= 1;
//                    }
//                } else {
//                    if (pwm_val > 0) {
//                        pwm_val -= 1;
//                    }
//                }
//            }