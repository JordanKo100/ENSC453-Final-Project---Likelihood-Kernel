// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
`timescale 1ns/1ps
module likelihood_kernel_control_s_axi
#(parameter
    C_S_AXI_ADDR_WIDTH = 7,
    C_S_AXI_DATA_WIDTH = 32
)(
    input  wire                          ACLK,
    input  wire                          ARESET,
    input  wire                          ACLK_EN,
    input  wire [C_S_AXI_ADDR_WIDTH-1:0] AWADDR,
    input  wire                          AWVALID,
    output wire                          AWREADY,
    input  wire [C_S_AXI_DATA_WIDTH-1:0] WDATA,
    input  wire [C_S_AXI_DATA_WIDTH/8-1:0] WSTRB,
    input  wire                          WVALID,
    output wire                          WREADY,
    output wire [1:0]                    BRESP,
    output wire                          BVALID,
    input  wire                          BREADY,
    input  wire [C_S_AXI_ADDR_WIDTH-1:0] ARADDR,
    input  wire                          ARVALID,
    output wire                          ARREADY,
    output wire [C_S_AXI_DATA_WIDTH-1:0] RDATA,
    output wire [1:0]                    RRESP,
    output wire                          RVALID,
    input  wire                          RREADY,
    output wire                          interrupt,
    output wire [31:0]                   Nparticles,
    output wire [31:0]                   countOnes,
    output wire [31:0]                   IszY,
    output wire [31:0]                   Nfr,
    output wire [31:0]                   k,
    output wire [63:0]                   max_size,
    output wire [63:0]                   arrayX,
    output wire [63:0]                   arrayY,
    output wire [63:0]                   objxy,
    output wire [63:0]                   I,
    output wire [63:0]                   likelihood,
    output wire                          ap_start,
    input  wire                          ap_done,
    input  wire                          ap_ready,
    input  wire                          ap_idle
);
//------------------------Address Info-------------------
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read)
//        bit 7  - auto_restart (Read/Write)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0  - enable ap_done interrupt (Read/Write)
//        bit 1  - enable ap_ready interrupt (Read/Write)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0  - ap_done (COR/TOW)
//        bit 1  - ap_ready (COR/TOW)
//        others - reserved
// 0x10 : Data signal of Nparticles
//        bit 31~0 - Nparticles[31:0] (Read/Write)
// 0x14 : reserved
// 0x18 : Data signal of countOnes
//        bit 31~0 - countOnes[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of IszY
//        bit 31~0 - IszY[31:0] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of Nfr
//        bit 31~0 - Nfr[31:0] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of k
//        bit 31~0 - k[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of max_size
//        bit 31~0 - max_size[31:0] (Read/Write)
// 0x3c : Data signal of max_size
//        bit 31~0 - max_size[63:32] (Read/Write)
// 0x40 : reserved
// 0x44 : Data signal of arrayX
//        bit 31~0 - arrayX[31:0] (Read/Write)
// 0x48 : Data signal of arrayX
//        bit 31~0 - arrayX[63:32] (Read/Write)
// 0x4c : reserved
// 0x50 : Data signal of arrayY
//        bit 31~0 - arrayY[31:0] (Read/Write)
// 0x54 : Data signal of arrayY
//        bit 31~0 - arrayY[63:32] (Read/Write)
// 0x58 : reserved
// 0x5c : Data signal of objxy
//        bit 31~0 - objxy[31:0] (Read/Write)
// 0x60 : Data signal of objxy
//        bit 31~0 - objxy[63:32] (Read/Write)
// 0x64 : reserved
// 0x68 : Data signal of I
//        bit 31~0 - I[31:0] (Read/Write)
// 0x6c : Data signal of I
//        bit 31~0 - I[63:32] (Read/Write)
// 0x70 : reserved
// 0x74 : Data signal of likelihood
//        bit 31~0 - likelihood[31:0] (Read/Write)
// 0x78 : Data signal of likelihood
//        bit 31~0 - likelihood[63:32] (Read/Write)
// 0x7c : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

//------------------------Parameter----------------------
localparam
    ADDR_AP_CTRL           = 7'h00,
    ADDR_GIE               = 7'h04,
    ADDR_IER               = 7'h08,
    ADDR_ISR               = 7'h0c,
    ADDR_NPARTICLES_DATA_0 = 7'h10,
    ADDR_NPARTICLES_CTRL   = 7'h14,
    ADDR_COUNTONES_DATA_0  = 7'h18,
    ADDR_COUNTONES_CTRL    = 7'h1c,
    ADDR_ISZY_DATA_0       = 7'h20,
    ADDR_ISZY_CTRL         = 7'h24,
    ADDR_NFR_DATA_0        = 7'h28,
    ADDR_NFR_CTRL          = 7'h2c,
    ADDR_K_DATA_0          = 7'h30,
    ADDR_K_CTRL            = 7'h34,
    ADDR_MAX_SIZE_DATA_0   = 7'h38,
    ADDR_MAX_SIZE_DATA_1   = 7'h3c,
    ADDR_MAX_SIZE_CTRL     = 7'h40,
    ADDR_ARRAYX_DATA_0     = 7'h44,
    ADDR_ARRAYX_DATA_1     = 7'h48,
    ADDR_ARRAYX_CTRL       = 7'h4c,
    ADDR_ARRAYY_DATA_0     = 7'h50,
    ADDR_ARRAYY_DATA_1     = 7'h54,
    ADDR_ARRAYY_CTRL       = 7'h58,
    ADDR_OBJXY_DATA_0      = 7'h5c,
    ADDR_OBJXY_DATA_1      = 7'h60,
    ADDR_OBJXY_CTRL        = 7'h64,
    ADDR_I_DATA_0          = 7'h68,
    ADDR_I_DATA_1          = 7'h6c,
    ADDR_I_CTRL            = 7'h70,
    ADDR_LIKELIHOOD_DATA_0 = 7'h74,
    ADDR_LIKELIHOOD_DATA_1 = 7'h78,
    ADDR_LIKELIHOOD_CTRL   = 7'h7c,
    WRIDLE                 = 2'd0,
    WRDATA                 = 2'd1,
    WRRESP                 = 2'd2,
    WRRESET                = 2'd3,
    RDIDLE                 = 2'd0,
    RDDATA                 = 2'd1,
    RDRESET                = 2'd2,
    ADDR_BITS                = 7;

//------------------------Local signal-------------------
    reg  [1:0]                    wstate = WRRESET;
    reg  [1:0]                    wnext;
    reg  [ADDR_BITS-1:0]          waddr;
    wire [C_S_AXI_DATA_WIDTH-1:0] wmask;
    wire                          aw_hs;
    wire                          w_hs;
    reg  [1:0]                    rstate = RDRESET;
    reg  [1:0]                    rnext;
    reg  [C_S_AXI_DATA_WIDTH-1:0] rdata;
    wire                          ar_hs;
    wire [ADDR_BITS-1:0]          raddr;
    // internal registers
    reg                           int_ap_idle;
    reg                           int_ap_ready;
    reg                           int_ap_done = 1'b0;
    reg                           int_ap_start = 1'b0;
    reg                           int_auto_restart = 1'b0;
    reg                           int_gie = 1'b0;
    reg  [1:0]                    int_ier = 2'b0;
    reg  [1:0]                    int_isr = 2'b0;
    reg  [31:0]                   int_Nparticles = 'b0;
    reg  [31:0]                   int_countOnes = 'b0;
    reg  [31:0]                   int_IszY = 'b0;
    reg  [31:0]                   int_Nfr = 'b0;
    reg  [31:0]                   int_k = 'b0;
    reg  [63:0]                   int_max_size = 'b0;
    reg  [63:0]                   int_arrayX = 'b0;
    reg  [63:0]                   int_arrayY = 'b0;
    reg  [63:0]                   int_objxy = 'b0;
    reg  [63:0]                   int_I = 'b0;
    reg  [63:0]                   int_likelihood = 'b0;

//------------------------Instantiation------------------


//------------------------AXI write fsm------------------
assign AWREADY = (wstate == WRIDLE);
assign WREADY  = (wstate == WRDATA);
assign BRESP   = 2'b00;  // OKAY
assign BVALID  = (wstate == WRRESP);
assign wmask   = { {8{WSTRB[3]}}, {8{WSTRB[2]}}, {8{WSTRB[1]}}, {8{WSTRB[0]}} };
assign aw_hs   = AWVALID & AWREADY;
assign w_hs    = WVALID & WREADY;

// wstate
always @(posedge ACLK) begin
    if (ARESET)
        wstate <= WRRESET;
    else if (ACLK_EN)
        wstate <= wnext;
end

// wnext
always @(*) begin
    case (wstate)
        WRIDLE:
            if (AWVALID)
                wnext = WRDATA;
            else
                wnext = WRIDLE;
        WRDATA:
            if (WVALID)
                wnext = WRRESP;
            else
                wnext = WRDATA;
        WRRESP:
            if (BREADY)
                wnext = WRIDLE;
            else
                wnext = WRRESP;
        default:
            wnext = WRIDLE;
    endcase
end

// waddr
always @(posedge ACLK) begin
    if (ACLK_EN) begin
        if (aw_hs)
            waddr <= AWADDR[ADDR_BITS-1:0];
    end
end

//------------------------AXI read fsm-------------------
assign ARREADY = (rstate == RDIDLE);
assign RDATA   = rdata;
assign RRESP   = 2'b00;  // OKAY
assign RVALID  = (rstate == RDDATA);
assign ar_hs   = ARVALID & ARREADY;
assign raddr   = ARADDR[ADDR_BITS-1:0];

// rstate
always @(posedge ACLK) begin
    if (ARESET)
        rstate <= RDRESET;
    else if (ACLK_EN)
        rstate <= rnext;
end

// rnext
always @(*) begin
    case (rstate)
        RDIDLE:
            if (ARVALID)
                rnext = RDDATA;
            else
                rnext = RDIDLE;
        RDDATA:
            if (RREADY & RVALID)
                rnext = RDIDLE;
            else
                rnext = RDDATA;
        default:
            rnext = RDIDLE;
    endcase
end

// rdata
always @(posedge ACLK) begin
    if (ACLK_EN) begin
        if (ar_hs) begin
            rdata <= 'b0;
            case (raddr)
                ADDR_AP_CTRL: begin
                    rdata[0] <= int_ap_start;
                    rdata[1] <= int_ap_done;
                    rdata[2] <= int_ap_idle;
                    rdata[3] <= int_ap_ready;
                    rdata[7] <= int_auto_restart;
                end
                ADDR_GIE: begin
                    rdata <= int_gie;
                end
                ADDR_IER: begin
                    rdata <= int_ier;
                end
                ADDR_ISR: begin
                    rdata <= int_isr;
                end
                ADDR_NPARTICLES_DATA_0: begin
                    rdata <= int_Nparticles[31:0];
                end
                ADDR_COUNTONES_DATA_0: begin
                    rdata <= int_countOnes[31:0];
                end
                ADDR_ISZY_DATA_0: begin
                    rdata <= int_IszY[31:0];
                end
                ADDR_NFR_DATA_0: begin
                    rdata <= int_Nfr[31:0];
                end
                ADDR_K_DATA_0: begin
                    rdata <= int_k[31:0];
                end
                ADDR_MAX_SIZE_DATA_0: begin
                    rdata <= int_max_size[31:0];
                end
                ADDR_MAX_SIZE_DATA_1: begin
                    rdata <= int_max_size[63:32];
                end
                ADDR_ARRAYX_DATA_0: begin
                    rdata <= int_arrayX[31:0];
                end
                ADDR_ARRAYX_DATA_1: begin
                    rdata <= int_arrayX[63:32];
                end
                ADDR_ARRAYY_DATA_0: begin
                    rdata <= int_arrayY[31:0];
                end
                ADDR_ARRAYY_DATA_1: begin
                    rdata <= int_arrayY[63:32];
                end
                ADDR_OBJXY_DATA_0: begin
                    rdata <= int_objxy[31:0];
                end
                ADDR_OBJXY_DATA_1: begin
                    rdata <= int_objxy[63:32];
                end
                ADDR_I_DATA_0: begin
                    rdata <= int_I[31:0];
                end
                ADDR_I_DATA_1: begin
                    rdata <= int_I[63:32];
                end
                ADDR_LIKELIHOOD_DATA_0: begin
                    rdata <= int_likelihood[31:0];
                end
                ADDR_LIKELIHOOD_DATA_1: begin
                    rdata <= int_likelihood[63:32];
                end
            endcase
        end
    end
end


//------------------------Register logic-----------------
assign interrupt  = int_gie & (|int_isr);
assign ap_start   = int_ap_start;
assign Nparticles = int_Nparticles;
assign countOnes  = int_countOnes;
assign IszY       = int_IszY;
assign Nfr        = int_Nfr;
assign k          = int_k;
assign max_size   = int_max_size;
assign arrayX     = int_arrayX;
assign arrayY     = int_arrayY;
assign objxy      = int_objxy;
assign I          = int_I;
assign likelihood = int_likelihood;
// int_ap_start
always @(posedge ACLK) begin
    if (ARESET)
        int_ap_start <= 1'b0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_AP_CTRL && WSTRB[0] && WDATA[0])
            int_ap_start <= 1'b1;
        else if (ap_ready)
            int_ap_start <= int_auto_restart; // clear on handshake/auto restart
    end
end

// int_ap_done
always @(posedge ACLK) begin
    if (ARESET)
        int_ap_done <= 1'b0;
    else if (ACLK_EN) begin
        if (ap_done)
            int_ap_done <= 1'b1;
        else if (ar_hs && raddr == ADDR_AP_CTRL)
            int_ap_done <= 1'b0; // clear on read
    end
end

// int_ap_idle
always @(posedge ACLK) begin
    if (ARESET)
        int_ap_idle <= 1'b0;
    else if (ACLK_EN) begin
            int_ap_idle <= ap_idle;
    end
end

// int_ap_ready
always @(posedge ACLK) begin
    if (ARESET)
        int_ap_ready <= 1'b0;
    else if (ACLK_EN) begin
            int_ap_ready <= ap_ready;
    end
end

// int_auto_restart
always @(posedge ACLK) begin
    if (ARESET)
        int_auto_restart <= 1'b0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_AP_CTRL && WSTRB[0])
            int_auto_restart <=  WDATA[7];
    end
end

// int_gie
always @(posedge ACLK) begin
    if (ARESET)
        int_gie <= 1'b0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_GIE && WSTRB[0])
            int_gie <= WDATA[0];
    end
end

// int_ier
always @(posedge ACLK) begin
    if (ARESET)
        int_ier <= 1'b0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_IER && WSTRB[0])
            int_ier <= WDATA[1:0];
    end
end

// int_isr[0]
always @(posedge ACLK) begin
    if (ARESET)
        int_isr[0] <= 1'b0;
    else if (ACLK_EN) begin
        if (int_ier[0] & ap_done)
            int_isr[0] <= 1'b1;
        else if (w_hs && waddr == ADDR_ISR && WSTRB[0])
            int_isr[0] <= int_isr[0] ^ WDATA[0]; // toggle on write
    end
end

// int_isr[1]
always @(posedge ACLK) begin
    if (ARESET)
        int_isr[1] <= 1'b0;
    else if (ACLK_EN) begin
        if (int_ier[1] & ap_ready)
            int_isr[1] <= 1'b1;
        else if (w_hs && waddr == ADDR_ISR && WSTRB[0])
            int_isr[1] <= int_isr[1] ^ WDATA[1]; // toggle on write
    end
end

// int_Nparticles[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_Nparticles[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_NPARTICLES_DATA_0)
            int_Nparticles[31:0] <= (WDATA[31:0] & wmask) | (int_Nparticles[31:0] & ~wmask);
    end
end

// int_countOnes[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_countOnes[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_COUNTONES_DATA_0)
            int_countOnes[31:0] <= (WDATA[31:0] & wmask) | (int_countOnes[31:0] & ~wmask);
    end
end

// int_IszY[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_IszY[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_ISZY_DATA_0)
            int_IszY[31:0] <= (WDATA[31:0] & wmask) | (int_IszY[31:0] & ~wmask);
    end
end

// int_Nfr[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_Nfr[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_NFR_DATA_0)
            int_Nfr[31:0] <= (WDATA[31:0] & wmask) | (int_Nfr[31:0] & ~wmask);
    end
end

// int_k[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_k[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_K_DATA_0)
            int_k[31:0] <= (WDATA[31:0] & wmask) | (int_k[31:0] & ~wmask);
    end
end

// int_max_size[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_max_size[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_MAX_SIZE_DATA_0)
            int_max_size[31:0] <= (WDATA[31:0] & wmask) | (int_max_size[31:0] & ~wmask);
    end
end

// int_max_size[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_max_size[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_MAX_SIZE_DATA_1)
            int_max_size[63:32] <= (WDATA[31:0] & wmask) | (int_max_size[63:32] & ~wmask);
    end
end

// int_arrayX[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_arrayX[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_ARRAYX_DATA_0)
            int_arrayX[31:0] <= (WDATA[31:0] & wmask) | (int_arrayX[31:0] & ~wmask);
    end
end

// int_arrayX[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_arrayX[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_ARRAYX_DATA_1)
            int_arrayX[63:32] <= (WDATA[31:0] & wmask) | (int_arrayX[63:32] & ~wmask);
    end
end

// int_arrayY[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_arrayY[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_ARRAYY_DATA_0)
            int_arrayY[31:0] <= (WDATA[31:0] & wmask) | (int_arrayY[31:0] & ~wmask);
    end
end

// int_arrayY[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_arrayY[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_ARRAYY_DATA_1)
            int_arrayY[63:32] <= (WDATA[31:0] & wmask) | (int_arrayY[63:32] & ~wmask);
    end
end

// int_objxy[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_objxy[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OBJXY_DATA_0)
            int_objxy[31:0] <= (WDATA[31:0] & wmask) | (int_objxy[31:0] & ~wmask);
    end
end

// int_objxy[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_objxy[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OBJXY_DATA_1)
            int_objxy[63:32] <= (WDATA[31:0] & wmask) | (int_objxy[63:32] & ~wmask);
    end
end

// int_I[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_I[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_I_DATA_0)
            int_I[31:0] <= (WDATA[31:0] & wmask) | (int_I[31:0] & ~wmask);
    end
end

// int_I[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_I[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_I_DATA_1)
            int_I[63:32] <= (WDATA[31:0] & wmask) | (int_I[63:32] & ~wmask);
    end
end

// int_likelihood[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_likelihood[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_LIKELIHOOD_DATA_0)
            int_likelihood[31:0] <= (WDATA[31:0] & wmask) | (int_likelihood[31:0] & ~wmask);
    end
end

// int_likelihood[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_likelihood[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_LIKELIHOOD_DATA_1)
            int_likelihood[63:32] <= (WDATA[31:0] & wmask) | (int_likelihood[63:32] & ~wmask);
    end
end


//------------------------Memory logic-------------------

endmodule
