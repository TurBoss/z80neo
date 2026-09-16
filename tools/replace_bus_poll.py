#!/usr/bin/env python3
with open("/home/turboss/Dev/PICO/z80neo/firmware/z80neo/src/memory.c", "r") as f:
    content = f.read()

# Find bus_poll function and replace it with minimal version
old_start = "void bus_poll(void) {"
new_bus_poll = """void bus_poll(void) {
    // ---- I/O request ----
    if (!gpio_get(IORQ_INPUT)) {
        if (bus_active) return;
        bus_active = true;
        io_op++;

        gpio_put(SEL3_OUT, 1); gpio_put(SEL1_OUT, 1); gpio_put(SEL2_OUT, 1);
        gpio_set_dir_masked(bus_mask, 0);
        busy_wait_us_32(2);

        int timeout = 5000;
        while (gpio_get(WR_INPUT) && gpio_get(RD_INPUT) && --timeout) tight_loop_contents();

        gpio_put(SEL2_OUT, 1); gpio_put(SEL1_OUT, 0);
        busy_wait_us_32(2);
        uint8_t pl = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
        uint32_t mask = (1u << SEL1_OUT) | (1u << SEL2_OUT);
        gpio_put_masked(mask, (1u << SEL1_OUT));
        busy_wait_us_32(2);
        uint8_t ph = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
        gpio_put(SEL2_OUT, 1);
        uint8_t port = pl;

        if (!gpio_get(WR_INPUT)) {
            gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 0);
            busy_wait_us_32(3);
            io_w_op = ((gpio_get_all() & bus_mask) >> BUS_GPIO_START) & 0xFF;
            gpio_put(SEL3_OUT, 1);
            bus_active = false;
            io_write_port(port, io_w_op);
            while (!gpio_get(IORQ_INPUT)) tight_loop_contents();
            // Spin for next MREQ, but with timeout (Z80 might HALT)
            int mreq_tmo = 50000;
            while (gpio_get(MREQ_INPUT) && --mreq_tmo) tight_loop_contents();
            if (mreq_tmo == 0) { bus_active = false; return; }
            goto handle_mem;
        } else if (!gpio_get(RD_INPUT)) {
            io_r_op = io_read_port(port);
            set_bus_dir(1);
            gpio_put_masked(bus_mask, (uint32_t)io_r_op << BUS_GPIO_START);
            busy_wait_us_32(2);
            gpio_put(SEL3_OUT, 0); gpio_put(DIR3_OUT, 1);
            while (!gpio_get(IORQ_INPUT)) tight_loop_contents();
            gpio_set_dir_masked(bus_mask, 0);
            gpio_put(DIR3_OUT, 0); gpio_put(SEL3_OUT, 1);
        }
        bus_active = false;
        return;
    }

    // ---- Memory request ----
    if (!gpio_get(MREQ_INPUT)) {
        if (bus_active) return;
        if (gpio_get(RD_INPUT) && gpio_get(WR_INPUT)) return;
        bus_active = true;
    } else {
        return;
    }

handle_mem:
    bus_active = false;
    gpio_set_dir_masked(bus_mask, 0);
    gpio_put(SEL3_OUT, 1);

    uint16_t addr = read_addr_bus();
    handle_mem_cycle(addr);

    // Handle consecutive cycles without re-checking MREQ state
    // (Z80 has back-to-back cycles during LDIR, PUSH, etc.)
    while (!gpio_get(MREQ_INPUT)) {
        if (gpio_get(RD_INPUT) && gpio_get(WR_INPUT)) break;
        addr = read_addr_bus();
        handle_mem_cycle(addr);
    }
}"""

idx = content.index(old_start)
# Find end of function
depth = 0
end_idx = idx
for i in range(idx, len(content)):
    if content[i] == "{":
        depth += 1
    elif content[i] == "}":
        depth -= 1
        if depth == 0:
            end_idx = i + 1
            break

new_content = content[:idx] + new_bus_poll + content[end_idx:]
with open("/home/turboss/Dev/PICO/z80neo/firmware/z80neo/src/memory.c", "w") as f:
    f.write(new_content)
print(f"Replaced bus_poll ({end_idx - idx} chars -> {len(new_bus_poll)} chars)")
