#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
// #include <stdlib.h>
#include <string.h>

// USB
#include <bsp/board_api.h>
#include <tusb.h>



char serial_rx_buffer[4];


static struct serial_device *con;
static struct acia *acia;

bool p1 = false;
bool p2 = false;


uint8_t read_buffer[4];

// Buffer for received data
uint8_t rx_buffer[CFG_TUD_CDC_RX_BUFSIZE];
uint8_t tx_buffer[CFG_TUD_CDC_TX_BUFSIZE];

uint8_t rx_head = 0;
uint8_t rx_tail = 0;

bool rx_data_available = false;

void usb_cdc_init(void) {
	
	// USB
	board_init();
	tusb_init();


	// TinyUSB board init callback after init
	if (board_init_after_tusb) {
	    board_init_after_tusb();
	}
	
}


void send_char(char c){
		
//	tud_cdc_n_write(1, serial_rx_buffer, 1);
//	tud_cdc_n_write_flush(1);

}
unsigned char read_char(void) {
	
}

void custom_cdc_task(void)
{
    // polling CDC interfaces if wanted

    // Check if CDC interface 0 (for pico sdk stdio) is connected and ready

    if (tud_cdc_n_connected(1)) {
//		if(!p2){
//			con->ready(acia);
//			p2 = true;
//		}
		// serial_status_2 = setBit(serial_status_2, 2);
        // print on CDC 0 some debug message
        // printf("Connected to CDC 0\n");
        // sleep_ms(5000); // wait for 5 seconds
    }
	else{
//		if(p2){
//			// con_ready(acia);
//			p2 = false;
//		}
		// serial_status_2 = clearBit(serial_status_2,2);
	}
	if (tud_cdc_n_connected(0)) {
		// serial_status_1 = setBit(serial_status_1, 2);
	    // print on CDC 0 some debug message
	    // printf("Connected to CDC 1\n");
	    // sleep_ms(5000); // wait for 5 seconds
	}
	else{
		// serial_status_1 = clearBit(serial_status_1, 2);
	}
}



// callback when data is received on a CDC interface
void tud_cdc_rx_cb(uint8_t itf)
{
	rx_head = 0;
    // allocate buffer for the data in the stack
    


    // printf("RX CDC %d\n", itf);
	
	// read the available data 
    // | IMPORTANT: also do this for CDC0 because otherwise
    // | you won't be able to print anymore to CDC0
    // | next time this function is called
    uint32_t count = tud_cdc_n_read(itf, rx_buffer, sizeof(rx_buffer));

    // check if the data was received on the second cdc interface
    
    if (itf == 1) {
        // process the received data
        rx_buffer[count] = 0; // null-terminate the string
		rx_tail = count;
		//strcpy(read_buffer, rx_buffer);
        // now echo data back to the console on CDC 0

		strcpy(read_buffer, rx_buffer);
		printf("RX1: %s %d \n", read_buffer, count);
		for (int i; i < count; i++){
			send_char(read_buffer[i]);
		}
        // and echo back OK on CDC 1
        // tud_cdc_n_write(itf, (uint8_t const *) "OK\r\n", 4);
        // tud_cdc_n_write_flush(itf);

		rx_data_available = true;
		//serial_status_2 = setBit(serial_status_2, 1);
    }
    
    else {

//        rx_buffer[count] = 0;
//		rx_tail = count;
//
//
//		// printf("RX0: %s %d \n", rx_buffer, count);
//
//        rx_data_available = true;
//		serial_status_2 = setBit(serial_status_2, 1);

	}
}

void cdc_task(void){
	
	tud_task();

	// custom tasks
	custom_cdc_task();
	
}
