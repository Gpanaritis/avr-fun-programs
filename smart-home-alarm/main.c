#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

uint8_t num_correct_digits = 0;
bool first_enable = true;
uint8_t num_false_tries = 0;
bool is_someone_close = false;
bool alarm_enabled = false;

void Pin_init(void);
void TCA0_init(int t1_value, bool pwm_enable);
void TCA0_hardReset(void);
void ADC0_init(int threshold);

int main(void)
{
	Pin_init();
	sei(); // Enable global interrupts

	while(1)
	{
		if (num_false_tries == 1 && first_enable == false && is_someone_close == true)
		{
			// Enable alarm
			TCA0_init(15,true);
		}
		
	}
}

ISR(TCA0_OVF_vect)
{
	cli();
	// Clear the interrupt flag
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;

	if (first_enable == true && alarm_enabled == false)
	{
		TCA0.SINGLE.CTRLA &= ~(TCA_SINGLE_ENABLE_bm); // Disable timer
		ADC0_init(100); // Enable ADC with threshold of 100
		first_enable = false;
	}
	else if(first_enable == false && alarm_enabled == false){
		// Enable alarm
		TCA0_init(15,true);
	}
	else if(first_enable == false && alarm_enabled == true)
	{
		PORTD.OUTCLR = PIN0_bm; //Turn on led at Pin 0
	}
	
	sei();
	
}

ISR(TCA0_CMP0_vect)
{
	cli();
	
	// Clear the interrupt flag
	TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm;
	
	PORTD.OUTSET = PIN0_bm; //Turn off led at Pin 0
	
	sei();
	
}

ISR(ADC0_WCOMP_vect)
{
	cli();
	
	// Clear the interrupt flag
	ADC0.INTFLAGS = ADC_WCMP_bm;

	TCA0_init(10000,false); // Enable ADC with threshold of 100000
	PORTD.OUTCLR = PIN0_bm; // Turn on LED0
	num_false_tries = 0;
	num_correct_digits = 0;
	is_someone_close = true;
	ADC0.CTRLA &= ~(ADC_ENABLE_bm);
	
	sei();
}

ISR(PORTF_PORT_vect){
	// save the interrupt flag to a variable
	uint8_t intflags = PORTF.INTFLAGS;

	// Check if the interrupt is from pin 5 and no other pin is pressed
	if((intflags & PIN5_bm) && !(intflags & ~(PIN5_bm))){
		if(num_correct_digits == 0 || num_correct_digits == 2){
			num_correct_digits++;
		}
		else{
			num_correct_digits = 0;
			num_false_tries++;
		}
	}
	// Check if the interrupt is from pin 6 and no other pin is pressed
	else if((intflags & PIN6_bm) && !(intflags & ~(PIN6_bm))){
		if(num_correct_digits == 1 || num_correct_digits == 3){
			num_correct_digits++;
		}
		else{
			num_correct_digits = 0;
			num_false_tries++;
		}
	}
	else{
		// This should never happen, but just in case
		num_correct_digits = 0;
		num_false_tries++;
	}

	if (num_correct_digits == 4 && first_enable == true)
	{
		// Turn on LED2
		TCA0_init(10,false);
		num_correct_digits = 0;
		num_false_tries = 0;
	}
	else if(num_correct_digits == 4 && first_enable == false)
	{
		// Disable alarm
		TCA0.SINGLE.CTRLA &= ~(TCA_SINGLE_ENABLE_bm); // Disable timer
		ADC0.CTRLA &= ~(ADC_ENABLE_bm); // Disable ADC
		PORTD.OUTSET = PIN0_bm; // Turn off LED0
		num_correct_digits = 0;
		num_false_tries = 0;
		first_enable = true;
		is_someone_close = false;
	}	

	// Clear the interrupt flag
	PORTF.INTFLAGS = intflags;
}

void Pin_init(void)
{
	// Configure PORTF
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc; // Enable pull-up resistor and set interrupt on both edges
	PORTF.PIN6CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc ; // Enable pull-up resistor and set interrupt on both edges
	PORTD.DIR |= PIN0_bm;

}

void TCA0_hardReset(void)
{
	/* stop timer */
	TCA0.SINGLE.CTRLA &= ~(TCA_SINGLE_ENABLE_bm);
	
	/* force a hard reset */
	TCA0.SINGLE.CTRLESET = TCA_SINGLE_CMD_RESET_gc;
}

void TCA0_init(int t1_value, bool pwm_enable)
{
	// Initialize TCA0 timer for split mode operation
	// Perform hard reset to ensure correct initial state
	TCA0_hardReset();

	if(pwm_enable == false)
	{
		// Set prescaler to 256 and enable timer
		// Should be 1024 for the real results, but for some reason it is too slow on the simulator
		// Helpful: https://eleccelerator.com/avr-timer-calculator/
		TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc | TCA_SINGLE_ENABLE_bm;
		TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc;

		// Set the period
		TCA0.SINGLE.PER = t1_value;

		// Enable overflow interrupt
		TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
	}
	else if(pwm_enable == true)
	{
		alarm_enabled = true;
		num_false_tries = 0;
		num_correct_digits = 0;
		
		TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc | TCA_SINGLE_ENABLE_bm; // Set prescaler to 256 and enable timer
		TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc; // Set waveform generation mode to single slope PWM
		
		TCA0.SINGLE.PER = t1_value; // Set the period
		TCA0.SINGLE.CMP0 = t1_value / 2; // Set the compare value for compare channel to half of the period

		// Enable interrupts for compare channels 0 and 1
		TCA0.SINGLE.INTCTRL = TCA_SINGLE_CMP0_bm | TCA_SINGLE_OVF_bm;
	}

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
