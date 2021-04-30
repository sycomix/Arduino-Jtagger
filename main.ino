/*
	Author: Michael Vigdorchik, October 2019, Retro

	This is a basic jtagger, built for simple purposes such as:
	detecting existance of a scan chain. read idcode, insert ir and dr ...
*/

#include "Arduino.h"


#define TCK 7
#define TMS 8
#define TDI 9
#define TDO 10
#define TRST 11

#define IDCODE 0000
#define SAMPLE 0001
#define EXTEST 0011
#define BYPASS 1111

#define MAX_DR_LEN 256  // usually BSR

/*
#define HC __asm__ __volatile__( \
    		 "nop\t\n
    		  nop\t\n
    		  nop\t\n
    		  nop\t\n" : : );
// half clock cycle
*/

#define HC delay(1);



enum tap_states
{
	TEST_LOGIC_RESET, RUN_TEST_IDLE,  \
	SELECT_DR, CAPTURE_DR, SHIFT_DR, EXIT1_DR, PAUSE_DR, EXIT2_DR, UPDATE_DR, \
	SELECT_IR, CAPTURE_IR, SHIFT_IR, EXIT1_IR, PAUSE_IR, EXIT2_IR, UPDATE_IR  \
};


enum tap_states current_state;



/*
	the function detects the the existence of a chain and checks the ir length
*/
uint8_t detect_chain(void){

	uint32_t idcode_bits[32] = { 0 };
	uint32_t i = 0;
	uint8_t counter = 0;


	reset_tap_machine();

	// try to read IDCODE first and then detect the IR length
	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(CAPTURE_DR);
	advance_tap_state(SHIFT_DR);

	// shift out the id code from the id code register
	for (i = 0; i < 32; ++i)
	{
		advance_tap_state(SHIFT_DR);
		idcode_bits[i] = digitalRead(TDO);  // LSB first
	}

	
	// LSB of IDCODe must be 1.
	if (idcode_bits[0] != 0x01)
	{
		Serial.println("\n\nBad IDCODE or not implemented, LSB = 0");
	}

	// turn idcode_bits into an unsigned integer
	idcode = 0;
	for (i = 0; i < 32; ++i)
	{
		idcode += (idcode_bits[i] * pow(2, i));
	}
	Serial.print("\nFound IDCODE: ");
	Serial.print(idcode, HEX);


	// find ir length.
	Serial.println("\n\nAttempting to find IR length of part ...\n\n");
	reset_tap_machine();
	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(SELECT_IR);
	advance_tap_state(CAPTURE_IR);
	advance_tap_state(SHIFT_IR);
	
	// shift in about 100 ones into TDI to clear the register
	// from its previos content. and then shift a single zero followed by
	// a bunch of ones and cout the amount of clock cycles from inserting zero
	// till we read it in TDO.
	
	digitalWrite(TDI, 1);
	for (i = 0; i < 100; ++i)
	{
		advance_tap_state(SHIFT_IR);
	}

	digitalWrite(TDI, 0);
	advance_tap_state(SHIFT_IR);

	digitalWrite(TDI, 1);
	for (i = 0; i < 100; ++i)
	{
		advance_tap_state(SHIFT_IR);

		if (digitalRead(TDO) == 0){
			return counter;
		}
		counter++;
	}

	Serial.println("\n\nDid not find valid IR length\n");
	return 0;
}


/*
	Return to teset logic reset state of the TAP FSM.
	Invoke 5 TCK cycles accompanied with TMS logic state 1.
*/
void reset_tap_machine(void){

	for (uint8_t i = 0; i < 5; ++i)
	{
		digitalWrite(TMS, 1);
		
		digitalWrite(TCK, 0);
		HC;
		digitalWrite(TCK, 1);
		HC;
	}
	current_state = TEST_LOGIC_RESET;
}




/*
	the function inserts data of length dr_len to DR, and ends the
	interaction in the state end_state which can be one of the following:
	TLR, RTI, PauseIR
*/
void insert_dr(uint8_t * data, uint8_t dr_len, uint8_t end_state, uint8_t * dr_out){
	/* Make sure that current state is TLR*/
	uint16_t i = 0;

	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(CAPTURE_DR);
	advance_tap_state(SHIFT_DR);

	
	// shift data bits into the DR. make sure that first bit is LSB
	for (i = 0; i < dr_len; ++i)
	{
		digitalWrite(TDI, data[i]);

		digitalWrite(TCK, 0); HC;
		digitalWrite(TCK, 1); HC;

		dr_out[i] = digitalRead(TDO);  // read the shifted out bits . LSB first
	}


	// continue to the end state
	advance_tap_state(EXIT1_DR);
	advance_tap_state(PAUSE_DR);

	if (end_state == RUN_TEST_IDLE){
		advance_tap_state(EXIT2_DR);
		advance_tap_state(UPDATE_DR);
		advance_tap_state(RUN_TEST_IDLE);
	}
	if (end_state == TEST_LOGIC_RESET){
			advance_tap_state(TEST_LOGIC_RESET);
	}

}



/*
	the function inserts data of length ir_len to IR, and ends the
	interaction in the state end_state which can be one of the following:
	TLR, RTI, PauseIR
*/
void insert_ir(uint8_t * data, uint8_t ir_len, uint8_t end_state, uint8_t * ir_out){

	/* Make sure that current state is TLR*/
	uint8_t i = 0;

	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(SELECT_IR);
	advance_tap_state(CAPTURE_IR);
	advance_tap_state(SHIFT_IR);

	
	// shift data bits into the IR. make sure that first bit is LSB
	for (i = 0; i < ir_len; ++i)
	{
		digitalWrite(TDI, data[i]);

		digitalWrite(TCK, 0); HC;
		digitalWrite(TCK, 1); HC;

		ir_out[i] = digitalRead(TDO);  // read the shifted out bits . LSB first
	}


	// continue to the end state
	advance_tap_state(EXIT1_IR);
	advance_tap_state(PAUSE_IR);

	if (end_state == RUN_TEST_IDLE){
		advance_tap_state(EXIT2_IR);
		advance_tap_state(UPDATE_IR);
		advance_tap_state(RUN_TEST_IDLE);
	}
	if (end_state == TEST_LOGIC_RESET){
			advance_tap_state(TEST_LOGIC_RESET);
	}
}


/*
	the function return the dr length of a specific instruction
*/
uint32_t detect_dr_len(uint8_t * instruction, uint8_t ir_len){
	
	/* Make sure that current state is TLR*/

	uint8_t tmp[ir_len];  // temporary array to strore the shifted out bits of IR
	uint16_t i = 0;
	uint32_t counter = 0;

	// insert the instruction we wish to check
	insert_ir(instruction, ir_len, RUN_TEST_IDLE, tmp);

	/* 
		check the length of the DR register between TDI and TDO
		by inserting many ones (MAX_DR_LEN) to clean the register.
		afterwards, insert a single zero and start counting the amount
		of TCK clock cycles till the appearence of that zero in TDO.
	*/
	advance_tap_state(SELECT_DR);
	advance_tap_state(CAPTURE_DR);
	advance_tap_state(SHIFT_DR);

	digitalWrite(TDI, 1);
	for (i = 0; i < MAX_DR_LEN; ++i)
	{
		advance_tap_state(SHIFT_DR);
	}

	digitalWrite(TDI, 0);
	advance_tap_state(SHIFT_DR);

	digitalWrite(TDI, 1);
	for (i = 0; i < MAX_DR_LEN; ++i)
	{
		advance_tap_state(SHIFT_DR);

		if (digitalRead(TDO) == 0){
			return counter;
		}
		counter++;
	}

	Serial.println("\n\nDid not find the current DR lenght, TDO stuck at 1\n");
	return 0;
}





/* 
	Advance the TAP machine 1 state ahead according to the current state and next state
*/
void advance_tap_state(uint8_t next_state){


	Serial.print("\ntap state: ");
	Serial.print(current_state, HEX);

	switch ( current_state ){

		case TEST_LOGIC_RESET:
			if (next_state == RUN_TEST_IDLE){
				// go to run test idle
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = RUN_TEST_IDLE;
			}
			else if (next_state == TEST_LOGIC_RESET){
				// stay in test logic reset
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			break;


		case RUN_TEST_IDLE:
			if (next_state == SELECT_DR){
				// go to select dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SELECT_DR;
			}
			else if (next_state == RUN_TEST_IDLE){ 
				// stay in run test idle
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			break;


		case SELECT_DR:
			if (next_state == CAPTURE_DR){
				// go to capture dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = CAPTURE_DR;
			}
			else if (next_state == SELECT_IR){ 
				// go to select ir
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SELECT_IR;
			}
			break;


		case CAPTURE_DR:
			if (next_state == SHIFT_DR){
				// go to shift dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SHIFT_DR;
			}
			else if (next_state == EXIT1_DR){ 
				// go to exit1 dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT1_DR;
			}
			break;

		case SHIFT_DR:
			if (next_state == SHIFT_DR){
				// stay in shift dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			else if (next_state == EXIT1_DR){ 
				// go to exit1 dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT1_DR;
			}
			break;


		case EXIT1_DR:
			if (next_state == PAUSE_DR){
				// go to pause dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = PAUSE_DR;
			}
			else if (next_state == UPDATE_DR){ 
				// go to update dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = UPDATE_DR;
			}
			break;


		case PAUSE_DR:
			if (next_state == PAUSE_DR){
				// stay in pause dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			else if (next_state == EXIT2_DR){ 
				// go to exit2 dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT2_DR;
			}
			break;


		case EXIT2_DR:
			if (next_state == SHIFT_DR){
				// go to shift dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SHIFT_DR;
			}
			else if (next_state == UPDATE_DR){ 
				// go to update dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = UPDATE_DR;
			}
			break;


		case UPDATE_DR:
			if (next_state == RUN_TEST_IDLE){
				// go to run test idle
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = RUN_TEST_IDLE;
			}
			else if (next_state == SELECT_DR){ 
				// go to select dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SELECT_DR;
			}
			break;


		case SELECT_IR:
			if (next_state == CAPTURE_IR){
				// go to capture ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = CAPTURE_IR;
			}
			else if (next_state == TEST_LOGIC_RESET){ 
				// go to test logic reset
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = TEST_LOGIC_RESET;
			}
			break;


		case CAPTURE_IR:
			if (next_state == SHIFT_IR){
				// go to shift ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SHIFT_IR;
			}
			else if (next_state == EXIT1_IR){ 
				// go to exit1 ir
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT1_IR;
			}
			break;


		case SHIFT_IR:
			if (next_state == SHIFT_IR){
				// stay in shift dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			else if (next_state == EXIT1_IR){ 
				// go to exit1 dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT1_IR;
			}
			break;


		case EXIT1_IR:
			if (next_state == PAUSE_IR){
				// go to pause ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = PAUSE_IR;
			}
			else if (next_state == UPDATE_IR){ 
				// go to update ir
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = UPDATE_IR;
			}
			break;

		
		case PAUSE_IR:
			if (next_state == PAUSE_IR){
				// stay in pause ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			else if (next_state == EXIT2_IR){ 
				// go to exit2 dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT2_IR;
			}
			break;


		case EXIT2_IR:
			if (next_state == SHIFT_IR){
				// go to shift ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SHIFT_IR;
			}
			else if (next_state == UPDATE_IR){ 
				// go to update ir
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = UPDATE_IR;
			}
			break;


		case UPDATE_IR:
			if (next_state == RUN_TEST_IDLE){
				// go to run test idle
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = RUN_TEST_IDLE;
			}
			else if (next_state == SELECT_DR){ 
				// go to select dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SELECT_DR;
			}
			break;


		default:
			Serial.println("Error: incorrent TAP state !");
			break;
	}

}



/*
	waits for a the incoming of a special character to Serial
*/
void serialEvent() {
  char inChar;
  while (Serial.available() == 0) {
    // get the new byte:
    inChar = (char)Serial.read();
    // if the incoming character is a newline, break from while and proceed to main loop
    // do something about it:
    if (inChar == '\n') {
      break;
    }
  }
}


/*
	sends the bytes of an array/buffer via the serial port
*/
void send_data_to_host(uint8_t * buf, uint16_t chunk_size)
{
	for (int i = 0; i < chunk_size; ++i)
	{
		Serial.write(buf[i]);
	}
	Serial.flush();
}




void setup() {
	/* initialize mode for jtag pins */
	pinMode(TCK, OUTPUT);
	pinMode(TMS, OUTPUT);
	pinMode(TDI, OUTPUT);
	pinMode(TDO, INPUT);
	pinMode(TRST, OUTPUT);

	/* initial pins state */
	digitalWrite(TCK, 0);
	digitalWrite(TMS, 1);
	digitalWrite(TDI, 1);
	digitalWrite(TDO, HIGH); // to use PullUp
	digitalWrite(TRST, 1);



	/* serial communication */
	Serial.begin(115200);
	/* wait for user input to start running the program */
  	serialEvent();

}

void loop() {

	uint32_t idcode = 0x00000000;
	uint8_t dr_out[MAX_DR_LEN];
	uint8_t dr_in[MAX_DR_LEN];
	uint8_t ir_len = 0;


	current_state = TEST_LOGIC_RESET;
  	

  // put your main code here, to run repeatedly:
	ir_len = detect_chain();
	Serial.println("\nIR length: (HEX) ");
	Serial.print(ir_len, HEX);
	uint8_t ir_out[ir_len];


	reset_tap_machine();
	insert_ir();

	while ( 1 );

}
