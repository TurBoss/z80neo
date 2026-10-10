// ============================================================================
// GFX Engine - ESC g drawing sequencer for the 3-bitplane bitmap layer
//
// A drawing command reduces to "plot a sequence of pixels, all in one colour":
//   shape 0 : single pixel at (ax,ay)
//   shape 1 : Bresenham line (ax,ay)-(bx,by)
//   shape 2 : filled rectangle x=ax y=ay w=bx h=by
// Clear / mode-on are issued by the handler as a full-screen fill.
//
// Because the bitmap is stored as one bit per plane, plotting a pixel is a
// plain word write to each plane: no read-modify-write is needed.
//
// The handler holds the console (rx_ready low) while busy is asserted.
// ============================================================================

module gfx_engine (
    input             clk,
    input             rst_n,

    // command interface (system clock domain)
    input             start,
    input      [1:0]  shape,
    input      [2:0]  colour,
    input      [10:0] ax,
    input      [8:0]  ay,
    input      [10:0] bx,
    input      [8:0]  by,
    output            busy,

    // bitmap write port
    output reg [2:0]  we,
    output reg [17:0] waddr,
    output reg [2:0]  wdata
);

    localparam [1:0] S_IDLE = 2'd0, S_WORK = 2'd1;

    reg [1:0] state;
    reg [1:0] kind;
    reg [2:0] col;

    reg [10:0] cx, lx1, fx0, fx1;
    reg [8:0]  cy, ly1, fy0, fy1;
    reg signed [15:0] dx, dy, err;
    reg sx, sy;

    // ---- next-pixel computation --------------------------------------------
    reg signed [15:0] e2, nerr;
    reg [10:0] ncx;
    reg [8:0]  ncy;
    reg        done;

    always @(*) begin
        e2   = err + err;
        ncx  = cx;
        ncy  = cy;
        nerr = err;
        done = 1'b0;
        case (kind)
            2'd0: done = 1'b1;                                   // single pixel
            2'd1: begin                                          // Bresenham line
                if (cx == lx1 && cy == ly1)
                    done = 1'b1;
                else begin
                    if (e2 >= dy) begin nerr = nerr + dy;
                                         ncx = sx ? (cx + 11'd1) : (cx - 11'd1); end
                    if (e2 <= dx) begin nerr = nerr + dx;
                                         ncy = sy ? (cy + 9'd1) : (cy - 9'd1); end
                end
            end
            2'd2: begin                                          // filled rect
                if (cx == fx1 && cy == fy1)      done = 1'b1;
                else if (cx == fx1) begin ncx = fx0; ncy = cy + 9'd1; end
                else                       ncx = cx + 11'd1;
            end
            default: done = 1'b1;
        endcase
    end

    wire [17:0] addr    = cy * 11'd640 + cx;
    wire        inbounds = (cx <= 11'd639) && (cy <= 9'd255);

    // ---- write setup for the next command ----------------------------------
    wire [11:0] fx1_w = (ax + bx > 12'd640) ? 12'd639 : (ax + bx - 12'd1);
    wire [9:0]  fy1_w = (ay + by > 10'd256) ? 10'd255 : (ay + by - 10'd1);
    wire [15:0] adx_w = (bx >= ax) ? (bx - ax) : (ax - bx);
    wire [15:0] ady_w = (by >= ay) ? (by - ay) : (ay - by);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= S_IDLE;
            kind  <= 2'd0;
            col   <= 3'd0;
            we    <= 3'd0;
            waddr <= 18'd0;
            wdata <= 3'd0;
            cx <= 11'd0; cy <= 9'd0;
            lx1 <= 11'd0; ly1 <= 9'd0;
            fx0 <= 11'd0; fx1 <= 11'd0;
            fy0 <= 9'd0;  fy1 <= 9'd0;
            dx <= 16'd0; dy <= 16'd0; err <= 16'd0;
            sx <= 1'b0; sy <= 1'b0;
        end else begin
            we <= 3'b000;

            case (state)
            S_IDLE: begin
                if (start) begin
                    kind <= shape;
                    col  <= colour;
                    cx   <= ax;
                    cy   <= ay;

                    if (shape == 2'd2) begin           // fill / clear
                        fx0 <= ax;
                        fy0 <= ay;
                        fx1 <= fx1_w[10:0];
                        fy1 <= fy1_w[8:0];
                        if (bx == 11'd0 || by == 9'd0 ||
                            ax > 11'd639 || ay > 9'd255)
                            state <= S_IDLE;           // nothing to draw
                        else
                            state <= S_WORK;
                    end else if (shape == 2'd1) begin  // line
                        lx1 <= bx;
                        ly1 <= by;
                        dx  <= adx_w;
                        dy  <= -ady_w;
                        err <= adx_w - ady_w;
                        sx  <= (ax < bx);
                        sy  <= (ay < by);
                        state <= S_WORK;
                    end else                           // pixel
                        state <= S_WORK;
                end
            end

            S_WORK: begin
                we    <= inbounds ? 3'b111 : 3'b000;
                waddr <= addr;
                wdata <= col;
                if (done)
                    state <= S_IDLE;
                else begin
                    cx  <= ncx;
                    cy  <= ncy;
                    err <= nerr;
                end
            end

            default: state <= S_IDLE;
            endcase
        end
    end

    assign busy = start | (state != S_IDLE);

endmodule
