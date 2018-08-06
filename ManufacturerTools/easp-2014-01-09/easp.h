/*
easp: Easy SVF Player, a simple USB JTAG programmer for FTDI FT245Rx
Copyright (C) 2014 Tilmann Hentze <0xcafe@directbox.com>
based on:
  prog_cpld: a very simple USB CPLD programmer
  Copyright (C) 2009  Koichi Nishida

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3 
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// libftdi0 has no queue, the FT245R has only 128 bytes receive buffer.
#define USB_BUFSIZE 128

// max string length
#define MAX_STR 256

// TAP STATES
#define TEST_LOGIC_RESET	0
#define RUN_TEST		1
#define SELECT_DR_SCAN		2
#define CAPTURE_DR		3
#define SHIFT_DR		4
#define EXIT1_DR		5
#define PAUSE_DR		6
#define EXIT2_DR		7
#define UPDATE_DR		8
#define SELECT_IR_SCAN		9
#define CAPTURE_IR		10
#define SHIFT_IR		11
#define EXIT1_IR		12
#define PAUSE_IR		13
#define EXIT2_IR		14
#define UPDATE_IR		15

// ========== prototypes ==========
// examine whether ch is blank character or not
int is_blank(int ch);

// examine whether ch is semicolon or not
int is_semi(int ch);

// get word from file FP
int get_word(FILE *fp, char *dst, int *end_semi);

// skip brace ()
int skip_brace(char *src, char *dst);

// examine whether ch is a hex character or not
int is_hex_char(int ch);

// get the value of the hex character
int value_of_hex_char(int ch);

// get the hex character of the value
int hex_char_of_value(int v);

// examine whether keyw is the keyword that can be ignored
int is_ignore(char *keyw);

// examine whether keyw is "SIR" or "SDR"
int sir_sdr(char *keyw);

// make default mask of w bits
void make_mask(int w, char *dst);

// make zero of w bits
void make_zero(int w, char *dst);

// address TDI / TDO / SMASK / MASK
int do_param(FILE *fp, char *keyw, char* kind, char* param, int *semi);

// reset TAP machine
int reset_tap(struct ftdi_context *p_ftdic, int *current_state, int mode);

// transit state
int transit(struct ftdi_context *p_ftdic, int *current, int next, int mode);

// output bit data 
int outBit(struct ftdi_context *p_ftdic, int tms, int tdi, int tdo, int mask,
           int flush, int *no_match, int mode);

// output data
int outData(struct ftdi_context *p_ftdic, int bitw, char *tdi, char *smask,
            char *tdo, char *mask, int *no_match, int mode);

// get state from state name 
int state_of_string(char *n, int *s);

// parse SVF file
int parse_svf(FILE *fp, struct ftdi_context *p_ftdic, int v,
              int *current_state, int *no_match, int mode);

// init ftdi context
int init(struct ftdi_context *p_ftdic);

// print error string and close ftdi context
int fail(struct ftdi_context *p_ftdic);

// list devices
int listDevices(struct ftdi_context *p_ftdic, struct ftdi_device_list *p_deviceList, int product_id);

// print usage
void usage();
