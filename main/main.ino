/** @file main.ino
 *
 * @brief basic jtagger, built for simple purposes such as:
 *		detecting existance of a scan chain. read idcode, insert ir and dr...
 *
 * @author Michael Vigdorchik, October 2019, Retro
 */


#include "Arduino.h"
#include "ir.h"

/* 
comment out this line if you don't wish to see debug info such as
TAP state transitions.
*/
#define DEBUGTAP


// set the jtag pins
#define TCK 7
#define TMS 8
#define TDI 9
#define TDO 10
#define TRST 11


#define MAX_DR_LEN 1000
#ifndef MAX_DR_LEN
#define MAX_DR_LEN 256    // usually BSR
#endif


// half clock cycle
#define HC delay(50);

// A more precise way to delay a half clock cycle
// #define HC __asm__ __volatile__(
//     		 "nop\t\n
//     		  nop\t\n
//     		  nop\t\n
//     		  nop\t\n" : : );




enum TapStates
{
	TEST_LOGIC_RESET, RUN_TEST_IDLE,
	SELECT_DR, CAPTURE_DR, SHIFT_DR, EXIT1_DR, PAUSE_DR, EXIT2_DR, UPDATE_DR,
	SELECT_IR, CAPTURE_IR, SHIFT_IR, EXIT1_IR, PAUSE_IR, EXIT2_IR, UPDATE_IR
};

enum TapStates current_state;




/**
 * @brief Detects the the existence of a chain and checks the ir length.
 * @return An integer that represents the length of the instructions.
 */
uint8_t detect_chain(void){

	uint8_t id_arr[32] = { 0 };
	uint32_t idcode = 0;
	uint32_t i = 0;
	uint8_t counter = 0;


	reset_tap();

	// try to read IDCODE first and then detect the IR length
	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(CAPTURE_DR);
	advance_tap_state(SHIFT_DR);
	// shift out the id code from the id code register
	for (i = 0; i < 32; ++i)
	{
		advance_tap_state(SHIFT_DR);
		id_arr[i] = digitalRead(TDO);  // LSB first
	}
	// LSB of IDCODe must be 1.
	if (id_arr[0] != 1)
	{
		Serial.println("\n\nBad IDCODE or not implemented, LSB = 0");
	}

	// turn idcode_bits into an unsigned integer
	idcode = arrayToInt(id_arr, 32);
	Serial.print("\nFound IDCODE: "); Serial.print(idcode, HEX);


	// find ir length.
	Serial.println("\n\nAttempting to find IR length of part ...\n\n");
	reset_tap();
	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(SELECT_IR);
	advance_tap_state(CAPTURE_IR);
	advance_tap_state(SHIFT_IR);
	
	// shift in about 100 ones into TDI to clear the register
	// from its previos content. then shift a single zero followed by
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


/**
 * @brief Convert array of bytes into an integer number, where every byte
 * represents a single bit. Together the array represents a binary number.
 * First element of array is the LSB.
 * @param arr Pointer to the array of bits.
 * @param len Integer that represents the length of the binary array.
 * @return An integer that represents the the value of the array bits.
 */
uint32_t arrayToInt(uint8_t * arr, uint8_t len){
	uint32_t sum = 0;
	for (int i = 0; i < len; ++i)
		sum += (arr[i] * pow(2, i));
	return sum;
}


/**
 * @brief Convert an integer number n into a bytes array arr that will represent
 * the binary value of n. each element (byte) in arr represent a bit.
 * First element is the LSB. Largest number is a 32 bit number.
 * @param arr Pointer to the output array with the binary values.
 * @param len Length of the output array. (max size 32)
 * @param n The integer to convert.
 */
void intToArray(uint8_t * arr, uint8_t len, uint32_t n){
	if (len > 32)
	{
		Serial.println("inToArray function, len is larger than 32");
		Serial.println("Bad Conversion");
	}
	
	uint32_t quotient = 0;
	for (int8_t i = 0; i < len; ++i)
	{
		if (quotient % 2)
			arr[i] = 1;
		else
			arr[i] = 0;
		
		quotient /= 2;
	}
}

/**
	@brief Return to test logic reset state of the TAP FSM.
	Invoke 5 TCK cycles accompanied with TMS logic state 1.
*/
void reset_tap(void){
	Serial.println("resetting tap");
	for (uint8_t i = 0; i < 5; ++i)
	{
		digitalWrite(TMS, 1);
		digitalWrite(TCK, 0); HC;
		digitalWrite(TCK, 1); HC;
	}
	current_state = TEST_LOGIC_RESET;
}


/**
	@brief Insert data of length dr_len to DR, and end the interaction
	in the state end_state which can be one of the following:
	TLR, RTI, PauseIR.
	@param dr_in Pointer to the input data array. (bytes array)
	@param dr_len Length of the register currently connected between tdi and tdo.
	@param end_state TAP state after dr inseration.
	@param dr_out Pointer to the output data array. (bytes array)
*/
void insert_dr(uint8_t * dr_in, uint8_t dr_len, uint8_t end_state, uint8_t * dr_out){
	/* Make sure that current state is TLR*/
	uint16_t i = 0;

	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(CAPTURE_DR);
	advance_tap_state(SHIFT_DR);

	// shift data bits into the DR. make sure that first bit is LSB
	for (i = 0; i < dr_len; ++i)
	{
		digitalWrite(TDI, dr_in[i]);
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



/**
	@brief Insert data of length ir_len to IR, and end the interaction
	in the state end_state which can be one of the following:
	TLR, RTI, PauseIR.
	@param ir_in Pointer to the input data array. Bytes array, where each
	@param ir_len Length of the register currently connected between tdi and tdo.
	@param end_state TAP state after dr inseration.
	@param ir_out Pointer to the output data array. (bytes array)
*/
void insert_ir(uint8_t * ir_in, uint8_t ir_len, uint8_t end_state, uint8_t * ir_out){

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
		digitalWrite(TDI, ir_in[i]);
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


/**
 * @brief Fill the register with zeros
 * @param reg Pointer to the register to flush.
 */
void flush_reg(uint8_t * reg){
	size_t size = sizeof(reg) / sizeof(reg[0]);  // find the size of the register

	for (uint16_t i = 0; i < size; i++)
		reg[i] = 0;
}


/**
 * @brief Clean the IR and DR by calling flush_reg function.
 */
void flush_ir_dr(uint8_t * ir_reg, uint8_t * dr_reg){
	flush_reg(ir_reg);
	flush_reg(dr_reg);
}



/**
	@brief Find out the dr length of a specific instruction.
	@param instruction Pointer to the bytes array that contains the instruction.
	@param ir_len The length of the IR. (Needs to be know prior to function call).
	@return Counter that represents the size of the DR. Or 0 if did not find
	a valid size. (DR may not be implemented or some other reason).
*/
uint16_t detect_dr_len(uint8_t * instruction, uint8_t ir_len){
	
	/* Make sure that current state is TLR*/

	uint8_t tmp[ir_len] = {0};  // temporary array to strore the shifted out bits of IR
	uint16_t i = 0;
	uint16_t counter = 0;

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
			++counter;
			return counter;
		}
		counter++;
	}
	Serial.println("\n\nDid not find the current DR length, TDO stuck at: ");
	Serial.print(digitalRead(TDO) ,DEC);
	return 0;
}



/**
	@brief Advance the TAP machine 1 state ahead according to the current state 
	and next state of the IEEE 1149 standard.
*/
void advance_tap_state(uint8_t next_state){

#ifdef DEBUGTAP
	Serial.print("\ntap state: ");
	Serial.print(current_state, HEX);
#endif

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
				// stay in shift ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			else if (next_state == EXIT1_IR){ 
				// go to exit1 ir
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



/**
 * @brief Waits for the incoming of a special character to Serial.
*/
void serialEvent(char character) {
  char inChar;
  while (Serial.available() == 0) {
    // get the new byte:
    inChar = (char)Serial.read();
    // if the incoming character is a newline, 
	// break from while and proceed to main loop
    // do something about it:
    if (inChar == character) {
      break;
    }
  }
  Serial.flush();
}



/**
 * @brief Sends the bytes of an array/buffer via the serial port.
 * @param buf Pointer to the buffer of data to be sent.
 * @param chunk_size Number of bytes to send to host.
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

	/* Initialize serial communication */
	Serial.begin(115200);
	while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
	}
	Serial.println("Ready...");
}



void loop() {

	uint32_t idcode = 0x00000000;
	uint8_t dr_out[MAX_DR_LEN];
	uint8_t dr_in[MAX_DR_LEN];
	uint8_t ir_len = 0;
	current_state = TEST_LOGIC_RESET;
  	
	
	while(1){
		Serial.println("Insert 's' to start");
		serialEvent('s');


		// detect chain and read idcode
		ir_len = detect_chain();
		Serial.println("\nIR length: "); Serial.print(ir_len, DEC);
		

		uint8_t ir_in[ir_len];
		uint8_t ir_out[ir_len];

		reset_tap();
		
		// read user code
		intToArray(ir_in, USERCODE, ir_len);
		insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);
		insert_dr(dr_in, 32, RUN_TEST_IDLE, dr_out);
		Serial.print("\nUSER CODE: "); Serial.print(arrayToInt(dr_out, 32) ,HEX);
		
		
		flush_ir_dr(ir_in, dr_out);
	}
}
