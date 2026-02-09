




extern void cdc_task(void);
extern void usb_cdc_init(void);
extern void custom_cdc_task(void);
extern void tud_cdc_rx_cb(uint8_t itf);

extern void send_char(uint8_t c);
extern uint8_t read_char(void);