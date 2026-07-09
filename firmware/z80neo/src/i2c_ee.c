// i2c_ee.c — I2C EEPROM emulation (SPI-flash backed)
// Intercepts bit-bang I2C on PIO port 0xD1 (virtual SCL=bit1, SDA=bit0, SDA_IN=bit2)

#include <stdbool.h>
#include <stdint.h>
#include <hardware/flash.h>
#include <hardware/sync.h>

#define EE_ADDR     0x50
#define EE_SIZE     8192
#define EE_FLASH_OFF (1024*1024)

static uint8_t  scl=1, sda=1, scl_p=1, sda_p=1;
static uint8_t  bits=0, byte=0, state=0;
static uint8_t  dev=0, is_rd=0;
static uint16_t addr=0;
static uint8_t  tx_byte=0;
static int      tx_bit=0;     // 0-7 = driving data, 8 = ACK
static bool     ack=0;
static bool     ack_phase=0;  // true when next SCL rising is ACK
static uint8_t  ram[EE_SIZE];

// Called on every write to port 0xD1
void i2c_ee_write(uint8_t d) {
    scl_p=scl; sda_p=sda;
    scl=(d>>1)&1; sda=d&1;

    // START: SDA fall while SCL high
    if(scl && scl_p && !sda && sda_p) {
        state=1; bits=0; byte=0; ack_phase=0; addr=0;
    }
    // STOP
    else if(scl && scl_p && sda && !sda_p) {
        state=0; ack=0; ack_phase=0;
    }
    // SCL rising edge
    else if(scl && !scl_p && state) {
        if(ack_phase) {
            // ACK bit — don't count as data
            ack_phase = 0;
            bits = 0;
            if(state==1) {
                dev=byte>>1; is_rd=byte&1;
                if(dev==EE_ADDR) { ack=1;
                    if(is_rd) { state=3; tx_byte=ram[addr]; tx_bit=0; }
                    else { state=2; }
                } else { state=0; ack=0; }
            } else if(state==2) {
                addr = (uint16_t)byte << 8; ack=1;
            } else if(state==3 && !is_rd) {
                if(addr<EE_SIZE) ram[addr]=byte;
                addr++; ack=1;
            } else if(state==4) {
                addr |= byte; ack=1; // stays in state 3? No, the next byte is data.
                state=3; // ready for data writes or reads
            }
            byte=0;
        } else if(state==1 || state==2 || state==4 || (state==3 && !is_rd)) {
            // Receiving data bit
            if(bits < 8) {
                byte = (byte<<1) | (sda?1:0);
                bits++;
                if(bits == 8) ack_phase = 1;  // next clock is ACK
            }
        }
    }
    // SCL falling: prepare next bit for read
    else if(!scl && scl_p && state==3 && is_rd) {
        if(tx_bit < 8) {
            tx_byte <<= 1;
            tx_bit++;
        } else {
            // ACK bit done, prepare next byte
            addr++;
            tx_byte = (addr<EE_SIZE) ? ram[addr] : 0xFF;
            tx_bit = 0;
        }
    }
}

// Called on every read from port 0xD1
uint8_t i2c_ee_read(void) {
    uint8_t v = 0xFF;
    if(!state || dev!=EE_ADDR) return v;

    if(state==3 && is_rd && tx_bit<8) {
        // Driving data bit: SDA_IN low if tx_byte bit 7 is 0
        if(!(tx_byte & 0x80)) v &= ~(1<<2);
    } else if(ack_phase && ack) {
        // ACK: slave pulls SDA low
        v &= ~(1<<2);
    }
    return v;
}

void i2c_ee_init(void) {
    for(int i=0; i<EE_SIZE; i++) ram[i] = 0xFF;
}
