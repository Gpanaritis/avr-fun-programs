#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

volatile uint8_t pin_5_counter = 0;

void TCA0_init(int t1_value, int t1_duty_cycle, int t2_value, int t2_duty_cycle);
void TCA0_hardReset(void);
void PIN_init(void);
void ADC0_init(int threshold);

int main(void)
{
	PIN_init();
	sei(); // Enable global interrupts

	while(1)
	{
		// Main loop
	}
}

// Blade
ISR(TCA0_LUNF_vect)
{
	TCA0.SPLIT.INTFLAGS = TCA_SPLIT_LUNF_bm;
	PORTD.OUTTGL = PIN0_bm;
}

// Base
ISR(TCA0_HUNF_vect)
{
	TCA0.SPLIT.INTFLAGS = TCA_SPLIT_HUNF_bm;
	PORTD.OUTTGL = PIN1_bm;
}

ISR(PORTF_PORT_vect)
{
    cli();

    // Clear the interrupt flag
	PORTF.INTFLAGS = PORT_INT5_bm;


    if(pin_5_counter == 0){

        // Enable the tca0 and adc
        TCA0_init(20,10,40,16);
        ADC0_init(10);
    }
    if(pin_5_counter == 1){
        // Turn off LED2
        PORTD.OUTSET = PIN2_bm;

        // Re-enable the tca
        TCA0_init(20,10,40,16);
    }
    if(pin_5_counter == 2)
    {
        // Disable and re-enable tca
        TCA0.SPLIT.CTRLA &= ~TCA_SPLIT_ENABLE_bm;
        TCA0_init(40,20,40,16);

        pin_5_counter = 3;
    }
    if(pin_5_counter == 3)
    {
        // Disable tca and adc
        TCA0.SPLIT.CTRLA &= ~TCA_SPLIT_ENABLE_bm;
        ADC0.CTRLA &= ~ADC_ENABLE_bm;

        pin_5_counter = 0;
    }

    sei();
}

ISR(ADC0_WCOMP_vect) {
    cli();
    
    int intflags = ADC0.INTFLAGS;
    ADC0.INTFLAGS = intflags;

    // Disable tca0 split
    TCA0.SPLIT.CTRLA &= ~TCA_SPLIT_ENABLE_bm;

    // Turn on LED2
    PORTD.OUTCLR = PIN2_bm;

    sei();
}

/* must be used when switching from single mode to split mode */
void TCA0_hardReset(void)
{
	/* stop timer */
	TCA0.SINGLE.CTRLA &= ~(TCA_SINGLE_ENABLE_bm);
	
	/* force a hard reset */
	TCA0.SINGLE.CTRLESET = TCA_SINGLE_CMD_RESET_gc;
}

void TCA0_init(int t1_value, int t1_duty_cycle, int t2_value, int t2_duty_cycle)
{
	// Initialize TCA0 timer for split mode operation
	// Perform hard reset to ensure correct initial state
	TCA0_hardReset();

	// Enable split mode
	TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;

	// Set prescaler to 256 and enable timer
    // Should be 1024 for the real results, but for some reason it is too slow on the simulator
    // Helpful: https://eleccelerator.com/avr-timer-calculator/
	TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV256_gc | TCA_SPLIT_ENABLE_bm;

    // Set the bitmasks for enabling the low and high compare channels, and enable both channels
    TCA0.SPLIT.CTRLB |= TCA_SPLIT_LCMP0EN_bm | TCA_SPLIT_HCMP0EN_bm;

	/* set the PWM frequencies and duty cycles */
	TCA0.SPLIT.LPER = t1_value; //1 ms
	TCA0.SPLIT.LCMP0 = t1_duty_cycle; // 50% duty cycle (20 * 0.5)
	TCA0.SPLIT.HPER = t2_value; // 2 ms
	TCA0.SPLIT.HCMP0 = t2_duty_cycle; // 40% duty cycle (40 * 0.4)

	// Check for rising edge and enable corresponding interrupt
	TCA0.SPLIT.INTCTRL = TCA_SPLIT_HUNF_bm | TCA_SPLIT_LUNF_bm;
	TCA0.SPLIT.INTFLAGS = TCA_SPLIT_HUNF_bm | TCA_SPLIT_LUNF_bm;

}

void PIN_init(void)
{
    // Configure PORTF
    // Enable pull-up resistor and set pin 5 of PORTF to detect both rising and falling edges
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;

    // Configure PORTD
    PORTD.DIRSET = PIN0_bm | PIN1_bm | PIN2_bm; // Set pins 0, 1 and 2 of PORTD as output

}

void ADC0_init(int threshold)
{
    // Initialize the ADC for Free-Running mode
	ADC0.CTRLA |= ADC_RESSEL_10BIT_gc; // 10-bit resolution
	ADC0.CTRLA |= ADC_ENABLE_bm; // Enable ADC
	ADC0.MUXPOS |= ADC_MUXPOS_AIN7_gc; // Select AIN7 as positive input
	ADC0.DBGCTRL |= ADC_DBGRUN_bm; // Enable Debug Mode
	ADC0.WINLT |= threshold; // Set threshold
	ADC0.INTCTRL |= ADC_WCMP_bm; // Enable Interrupts for WCM
	ADC0.CTRLE |= ADC_WINCM0_bm; // Interrupt when RESULT < WINLT
	ADC0.CTRLA |= ADC_FREERUN_bm; // Enable Free-Running mode
	ADC0.COMMAND |= ADC_STCONV_bm; // Start Conversion
}
