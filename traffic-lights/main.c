#include <avr/io.h>
#include <avr/interrupt.h>

#define T1 0x0A;	// Time between Tram passes.
#define T2 0x02;	// Duration of tram and pedestrian passings.
#define T3 0x03;	// Time between button presses.


// Functions declaration
void TCA0_init(void);
void PIN_init(void);
void TCA0_hardReset(void);

int pedestrian_button_enabled = 1; // Indicates whether the pedestrian button can be pressed
int t2_passed=1;			// Internal Flag for servicing T2 or T3.

// Initial State: Cars are passing Traffic Lights: (Green,Red,Red).
int main(void)
{
	// Initialize pins for controlling traffic lights
	// Set pin direction and output for Cars, Tram, Pedestrians
	// Corresponding road lights: 0 = on, 1 = off
	// Bit 5 of PortF is used for button to trigger interrupts
	PIN_init();
	
	// Initialize TCA0 timer for controlling traffic light timings
	TCA0_init();
	
	// Enable interrupts
	sei();
	
	// Enter infinite loop to wait for interrupts
	while (1)
	{
		// Pause execution and wait for an interrupt (Used for button)
		// Only for debugging purposes
		// Should remove on release
		asm("break");
	}
}

// This Interrupt Service Routine (ISR) serves T1
ISR(TCA0_HUNF_vect){
	// Disable Interrupts
	cli();
	
	// Clear the interrupt flag
	uint8_t interruptFlags = TCA0.SPLIT.INTFLAGS;
	TCA0.SPLIT.INTFLAGS = interruptFlags;
	
	// Change the flag to indicate that pedestrian button is disabled
	pedestrian_button_enabled = 0;
	
	// Turn off car lights
	PORTD.OUT |= 0b00000100;
	
	// Turn on pedestrian and tram lights
	PORTD.OUTCLR = 0b00000011;
	
	// Set the interrupt control to enable interrupts from the lower bits and disable interrupts from the high bits.
	TCA0.SPLIT.INTCTRL = TCA_SPLIT_LCMP0_bm;
	
	// Set the LCNT to T2 for the next interrupt
	TCA0.SPLIT.LCNT = T2;
	
	sei(); // Enable interrupts again
}

// Interrupt Service Routine (ISR) to serve T2 and T3
ISR(TCA0_LCMP0_vect){
	// Disable Interrupts
	cli();
	// Check if HUNF (High-Byte Underflow) is active and give it priority
	if ((TCA0.SPLIT.INTFLAGS & 0b00000010) == 0x02){
		// HUNF is active, so clear the flag and return
		TCA0.SPLIT.INTFLAGS = 0b00000010;
		return;
	}

	// Clear the interrupt flag for the LCMP0 (low-byte compare match) event
	uint8_t interruptFlags = TCA0.SPLIT.INTFLAGS;
	TCA0.SPLIT.INTFLAGS = interruptFlags;

	// Check if T2 has ended and T3 preparation is required
	if (t2_passed){
		// Turn off the pedestrian and tram LEDs
		PORTD.OUT |= 0b00000011;
		// Turn on the car LED
		PORTD.OUTCLR = 0b00000100;
		
		// Set LCNT to T3 for the next interrupt timing
		TCA0.SPLIT.LCNT = T3;
		// Reset the t2_passed flag
		t2_passed=0;
	}
	// T3 has ended
	else {
		// Enable the pedestrian button
		pedestrian_button_enabled=1;
		// Set the t2_passed flag to true
		t2_passed=1;
		// Disable interrupts from the low-byte and enable interrupts from the high-byte
		TCA0.SPLIT.INTCTRL = TCA_SPLIT_HUNF_bm;
	}
	// Enable Interrupts
	sei();
}

// ISR triggered when pedestrians press the button
ISR(PORTF_PORT_vect){
	// Disable Interrupts
	cli();
	
	//clear he interrupt flag
	uint8_t interruptFlags = PORTF.INTFLAGS;
	PORTF.INTFLAGS = interruptFlags;
	
	if (pedestrian_button_enabled){
		// Change the value to indicate that the pedestrian button is no longer enabled
		pedestrian_button_enabled = 0;
		
		// Turn off the car lights and turn on the pedestrian lights
		PORTD.OUT |= 0b00000100; // set bit 2 to 1 to turn off car lights
		PORTD.OUTCLR = 0b00000001; // set bit 0 to 0 to turn on pedestrian lights
		
		// Enable interrupts for both T2 and T3 to create a race condition.
		// When both the high and low bits are enabled from INTCTRL to cause
		// interrupts, the ISR that gets executed is randomized.
		// TRAM has a higher priority than the T2 and T3 interrupts.
		TCA0.SPLIT.INTCTRL |= TCA_SPLIT_LCMP0_bm; // enable T2 and T3 interrupts
		
		// Set LCNT to T2 for the next interrupt
		TCA0.SPLIT.LCNT = T2; // set T2 as the next interrupt timing
	}
	
	sei();
}

void PIN_init(void)
{
	PORTD.DIR |= 0b00000111;
	PORTD.OUT |= 0b00000011;
	PORTF.PIN5CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;
}

/* must be used when switching from single mode to split mode */
void TCA0_hardReset(void)
{
	/* stop timer */
	TCA0.SINGLE.CTRLA &= ~(TCA_SINGLE_ENABLE_bm);
	
	/* force a hard reset */
	TCA0.SINGLE.CTRLESET = TCA_SINGLE_CMD_RESET_gc;
}

void TCA0_init(void)
{
	// Initialize TCA0 timer for split mode operation
	// Perform hard reset to ensure correct initial state
	TCA0_hardReset();

	// Enable split mode
	TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;

	// Set prescaler to 16 and enable timer
	TCA0.SPLIT.CTRLA = TCA_SPLIT_CLKSEL_DIV16_gc | TCA_SPLIT_ENABLE_bm;

	// Enable LCMP0 for output compare
	TCA0.SPLIT.CTRLB = TCA_SPLIT_LCMP0EN_bm;

	// Set the period register to T1
	TCA0.SPLIT.HPER = T1;

	// Check for rising edge on HUNF (Tram) and enable corresponding interrupt
	TCA0.SPLIT.INTCTRL = TCA_SPLIT_HUNF_bm;

	// Set the LCMP0 value to 0 and the HCNT to T1
	TCA0.SPLIT.LCMP0 = 0x00;
	TCA0.SPLIT.HCNT = T1;
}

