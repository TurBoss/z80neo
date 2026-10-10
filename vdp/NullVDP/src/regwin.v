module register_window(usb_clk, busy, ulpi_data_in, ulpi_dir, ulpi_next, ulpi_data_out, ulpi_stop, done, address, write_data, read_request, write_request, usb_rst);
  reg \$auto$verilog_backend.cc:2083:dump_module$19  = 0;
  wire \$1 ;
  wire [7:0] \$11 ;
  wire \$13 ;
  wire [7:0] \$15 ;
  wire \$17 ;
  wire \$19 ;
  wire \$3 ;
  wire \$5 ;
  wire \$6 ;
  wire \$9 ;
  input [5:0] address;
  wire [5:0] address;
  output busy;
  wire busy;
  reg [5:0] current_address = 6'h00;
  reg [5:0] \current_address$next ;
  reg [7:0] current_write = 8'h00;
  reg [7:0] \current_write$next ;
  output done;
  reg done = 1'h0;
  reg \done$next ;
  reg [3:0] fsm_state = 4'h0;
  reg [3:0] \fsm_state$next ;
  reg [7:0] read_data = 8'h00;
  reg [7:0] \read_data$next ;
  input read_request;
  wire read_request;
  input [7:0] ulpi_data_in;
  wire [7:0] ulpi_data_in;
  output [7:0] ulpi_data_out;
  reg [7:0] ulpi_data_out = 8'h00;
  reg [7:0] \ulpi_data_out$next ;
  input ulpi_dir;
  wire ulpi_dir;
  input ulpi_next;
  wire ulpi_next;
  reg ulpi_out_req = 1'h0;
  reg \ulpi_out_req$next ;
  output ulpi_stop;
  reg ulpi_stop = 1'h0;
  reg \ulpi_stop$next ;
  input usb_clk;
  wire usb_clk;
  input usb_rst;
  wire usb_rst;
  input [7:0] write_data;
  wire [7:0] write_data;
  input write_request;
  wire write_request;
  assign \$9  = ~  ulpi_dir;
  assign \$11  = 8'hc0 |  address;
  assign \$13  = ~  ulpi_dir;
  assign \$15  = 8'h80 |  address;
  assign \$17  = ~  ulpi_dir;
  assign \$1  = ~  ulpi_dir;
  assign \$19  = ~  ulpi_dir;
  always @(posedge usb_clk)
    ulpi_out_req <= \ulpi_out_req$next ;
  always @(posedge usb_clk)
    ulpi_stop <= \ulpi_stop$next ;
  always @(posedge usb_clk)
    done <= \done$next ;
  always @(posedge usb_clk)
    ulpi_data_out <= \ulpi_data_out$next ;
  always @(posedge usb_clk)
    current_address <= \current_address$next ;
  always @(posedge usb_clk)
    current_write <= \current_write$next ;
  always @(posedge usb_clk)
    fsm_state <= \fsm_state$next ;
  always @(posedge usb_clk)
    read_data <= \read_data$next ;
  assign \$3  = ~  ulpi_dir;
  assign \$6  = !  fsm_state;
  assign \$5  = ~  \$6 ;
  always @* begin
    if (\$auto$verilog_backend.cc:2083:dump_module$19 ) begin end
    \ulpi_out_req$next  = 1'h0;
    casez (fsm_state)
      4'h0:
          ;
      4'h1:
          casez (\$1 )
            1'h1:
                \ulpi_out_req$next  = 1'h1;
          endcase
      4'h3:
        begin
          \ulpi_out_req$next  = 1'h1;
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                \ulpi_out_req$next  = 1'h0;
            2'b1?:
                \ulpi_out_req$next  = 1'h0;
          endcase
        end
      4'h4:
          ;
      4'h5:
          ;
      4'h2:
          casez (\$3 )
            1'h1:
                \ulpi_out_req$next  = 1'h1;
          endcase
      4'h6:
        begin
          \ulpi_out_req$next  = 1'h1;
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                \ulpi_out_req$next  = 1'h0;
          endcase
        end
      4'h7:
        begin
          \ulpi_out_req$next  = 1'h1;
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                \ulpi_out_req$next  = 1'h0;
          endcase
        end
    endcase
    casez (usb_rst)
      1'h1:
          \ulpi_out_req$next  = 1'h0;
    endcase
  end
  always @* begin
    if (\$auto$verilog_backend.cc:2083:dump_module$19 ) begin end
    \ulpi_stop$next  = 1'h0;
    casez (fsm_state)
      4'h0:
          ;
      4'h1:
          ;
      4'h3:
          ;
      4'h4:
          ;
      4'h5:
          ;
      4'h2:
          ;
      4'h6:
          ;
      4'h7:
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                ;
            2'b1?:
                \ulpi_stop$next  = 1'h1;
          endcase
      4'h8:
          \ulpi_stop$next  = 1'h0;
    endcase
    casez (usb_rst)
      1'h1:
          \ulpi_stop$next  = 1'h0;
    endcase
  end
  always @* begin
    if (\$auto$verilog_backend.cc:2083:dump_module$19 ) begin end
    \done$next  = 1'h0;
    casez (fsm_state)
      4'h0:
          ;
      4'h1:
          ;
      4'h3:
          ;
      4'h4:
          ;
      4'h5:
          \done$next  = 1'h1;
      4'h2:
          ;
      4'h6:
          ;
      4'h7:
          ;
      4'h8:
          \done$next  = 1'h1;
    endcase
    casez (usb_rst)
      1'h1:
          \done$next  = 1'h0;
    endcase
  end
  always @* begin
    if (\$auto$verilog_backend.cc:2083:dump_module$19 ) begin end
    \ulpi_data_out$next  = ulpi_data_out;
    casez (fsm_state)
      4'h0:
          \ulpi_data_out$next  = 8'h00;
      4'h1:
          casez (\$9 )
            1'h1:
                \ulpi_data_out$next  = \$11 ;
          endcase
      4'h3:
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                ;
            2'b1?:
                \ulpi_data_out$next  = 8'h00;
          endcase
      4'h4:
          ;
      4'h5:
          ;
      4'h2:
          casez (\$13 )
            1'h1:
                \ulpi_data_out$next  = \$15 ;
          endcase
      4'h6:
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                ;
            2'b1?:
                \ulpi_data_out$next  = write_data;
          endcase
      4'h7:
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                ;
            2'b1?:
                \ulpi_data_out$next  = 8'h00;
          endcase
      4'h8:
          \ulpi_data_out$next  = 8'h00;
    endcase
    casez (usb_rst)
      1'h1:
          \ulpi_data_out$next  = 8'h00;
    endcase
  end
  always @* begin
    if (\$auto$verilog_backend.cc:2083:dump_module$19 ) begin end
    \current_address$next  = current_address;
    casez (fsm_state)
      4'h0:
          \current_address$next  = address;
    endcase
    casez (usb_rst)
      1'h1:
          \current_address$next  = 6'h00;
    endcase
  end
  always @* begin
    if (\$auto$verilog_backend.cc:2083:dump_module$19 ) begin end
    \current_write$next  = current_write;
    casez (fsm_state)
      4'h0:
          \current_write$next  = write_data;
    endcase
    casez (usb_rst)
      1'h1:
          \current_write$next  = 8'h00;
    endcase
  end
  always @* begin
    if (\$auto$verilog_backend.cc:2083:dump_module$19 ) begin end
    \fsm_state$next  = fsm_state;
    casez (fsm_state)
      4'h0:
        begin
          casez (read_request)
            1'h1:
                \fsm_state$next  = 4'h1;
          endcase
          casez (write_request)
            1'h1:
                \fsm_state$next  = 4'h2;
          endcase
        end
      4'h1:
          casez (\$17 )
            1'h1:
                \fsm_state$next  = 4'h3;
          endcase
      4'h3:
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                \fsm_state$next  = 4'h1;
            2'b1?:
                \fsm_state$next  = 4'h4;
          endcase
      4'h4:
          \fsm_state$next  = 4'h5;
      4'h5:
          \fsm_state$next  = 4'h0;
      4'h2:
          casez (\$19 )
            1'h1:
                \fsm_state$next  = 4'h6;
          endcase
      4'h6:
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                \fsm_state$next  = 4'h2;
            2'b1?:
                \fsm_state$next  = 4'h7;
          endcase
      4'h7:
          casez ({ ulpi_next, ulpi_dir })
            2'b?1:
                \fsm_state$next  = 4'h2;
            2'b1?:
                \fsm_state$next  = 4'h8;
          endcase
      4'h8:
          \fsm_state$next  = 4'h0;
    endcase
    casez (usb_rst)
      1'h1:
          \fsm_state$next  = 4'h0;
    endcase
  end
  always @* begin
    if (\$auto$verilog_backend.cc:2083:dump_module$19 ) begin end
    \read_data$next  = read_data;
    casez (fsm_state)
      4'h0:
          ;
      4'h1:
          ;
      4'h3:
          ;
      4'h4:
          ;
      4'h5:
          \read_data$next  = ulpi_data_in;
    endcase
    casez (usb_rst)
      1'h1:
          \read_data$next  = 8'h00;
    endcase
  end
  assign busy = \$5 ;
endmodule
