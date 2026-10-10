// ============================================================================
// ULPI PHY link interface (register read/write)
// ============================================================================
//
// Minimal link-side ULPI controller for the USB3317 on the Tang Primer 20K
// Dock.  Commands follow the ULPI spec: TXD[7:6]=10 (0x80) = register write,
// 11 (0xC0) = register read.  The register-transfer FSM mirrors Luna's
// ULPIRegisterWindow: wait for the bus idle (~DIR) before driving, wait for
// NXT to accept the command, then a one-cycle bus turnaround before sampling
// the read data.
//
// All signals are synchronous to the PHY-provided ulpi_clk.  This board's
// ULPI clock is inverted (per the Luna/Sipeed port notes), so the top drives
// this module from ~ulpi_clk.
// ============================================================================

module ulpi_phy (
    input            clk,          // ulpi_clk-derived (~60 MHz, inverted)
    input            rst_n,
    input            start,        // pulse: begin a register op
    input            reg_write,    // 1 = write, 0 = read
    input      [5:0] addr,
    input      [7:0] wdata,
    output reg [7:0] rdata,
    output reg       done,         // 1-cycle pulse when the op completes
    output reg       busy,
    output reg       stp,
    input      [7:0] ulpi_data_in,
    output     [7:0] ulpi_data_out,
    output           ulpi_data_oe,
    input            ulpi_dir,
    input            ulpi_nxt
);

    localparam [7:0] COMMAND_REG_READ  = 8'hC0;
    localparam [7:0] COMMAND_REG_WRITE = 8'h80;

    localparam [3:0]
        IDLE               = 4'd0,
        START_READ         = 4'd1,
        SEND_READ_ADDRESS  = 4'd2,
        READ_TURNAROUND    = 4'd3,
        READ_COMPLETE      = 4'd4,
        START_WRITE        = 4'd5,
        SEND_WRITE_ADDRESS = 4'd6,
        HOLD_WRITE         = 4'd7,
        STOPPING           = 4'd8;

    reg [3:0] state;
    reg [7:0] data_out;
    reg       out_req;

    assign ulpi_data_oe  = out_req;
    assign ulpi_data_out = data_out;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state    <= IDLE;
            data_out <= 8'h00;
            out_req  <= 1'b0;
            stp      <= 1'b0;
            done     <= 1'b0;
            rdata    <= 8'h00;
            busy     <= 1'b0;
        end else begin
            done <= 1'b0;
            stp  <= 1'b0;
            busy <= (state != IDLE);

            case (state)
            IDLE: begin
                out_req  <= 1'b0;
                data_out <= 8'h00;
                if (start)
                    state <= reg_write ? START_WRITE : START_READ;
            end

            START_READ: begin
                if (!ulpi_dir) begin
                    data_out <= COMMAND_REG_READ | {2'b00, addr};
                    out_req  <= 1'b1;
                    state    <= SEND_READ_ADDRESS;
                end
            end

            SEND_READ_ADDRESS: begin
                out_req <= 1'b1;
                if (ulpi_dir) begin
                    out_req <= 1'b0;
                    state   <= START_READ;
                end else if (ulpi_nxt) begin
                    out_req <= 1'b0;
                    data_out <= 8'h00;
                    state   <= READ_TURNAROUND;
                end
            end

            READ_TURNAROUND: state <= READ_COMPLETE;

            READ_COMPLETE: begin
                rdata <= ulpi_data_in;
                done  <= 1'b1;
                state <= IDLE;
            end

            START_WRITE: begin
                if (!ulpi_dir) begin
                    data_out <= COMMAND_REG_WRITE | {2'b00, addr};
                    out_req  <= 1'b1;
                    state    <= SEND_WRITE_ADDRESS;
                end
            end

            SEND_WRITE_ADDRESS: begin
                out_req <= 1'b1;
                if (ulpi_dir) begin
                    out_req <= 1'b0;
                    state   <= START_WRITE;
                end else if (ulpi_nxt) begin
                    data_out <= wdata;
                    state    <= HOLD_WRITE;
                end
            end

            HOLD_WRITE: begin
                out_req <= 1'b1;
                if (ulpi_dir) begin
                    out_req <= 1'b0;
                    state   <= START_WRITE;
                end else if (ulpi_nxt) begin
                    data_out <= 8'h00;
                    stp      <= 1'b1;
                    state    <= STOPPING;
                end
            end

            STOPPING: begin
                stp     <= 1'b0;
                out_req <= 1'b0;
                done    <= 1'b1;
                state   <= IDLE;
            end

            default: state <= IDLE;
            endcase
        end
    end

endmodule
