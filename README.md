# Arduino-Jtagger

## example:

```
void loop()
{
	uint32_t idcode = 0;
	uint8_t dr_out[MAX_DR_LEN] = {0};
	uint8_t dr_in[MAX_DR_LEN] = {0};
	uint8_t ir_len = 0;
	current_state = TEST_LOGIC_RESET;
	
	Serial.println("Insert 's' to start");
	serialEvent('s');

	// detect chain and read idcode
	ir_len = detect_chain();
	Serial.println("\nIR length: "); Serial.print(ir_len, DEC);

	uint8_t ir_in[ir_len] = {0};
	uint8_t ir_out[ir_len] = {0};

	reset_tap();
	
	
    /* addtional code here */
    
    
    while(1);
}
```
