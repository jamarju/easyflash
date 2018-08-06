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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ftdi.h>
#include "easp.h"

// ========== functions ==========
// examine whether ch is blank character or not
int is_blank(int ch)
{
  return (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t');
}

// examine whether ch is semicolon or not
int is_semi(int ch)
{
  return (ch == ';');
}

// get a word from FILE* fp
// return 1 if success
int get_word(FILE *fp, char *dst, int *end_semi)
{
  static char ch = 0;
  int semi = 0;

  // pre read
  if (!ch) ch = fgetc(fp);
  SCAN_START:
  if (feof(fp)) return 0;
  // skip blank
  while (is_blank(ch)) {
    if (feof(fp)) return 0;
    ch = fgetc(fp);
  }
  // skip comment
  if (ch == '/') {
    ch = fgetc(fp);
    if (ch == '/') {
      while (ch != '\n') {
        if (feof(fp)) return 0;
        ch = fgetc(fp);
      };
      goto SCAN_START;
    } else *(dst++) = '/';
  }
  // actual read
  while (!(is_blank(ch) || is_semi(ch))) {
    *(dst++) = ch;
    if (feof(fp)) break;
    ch = fgetc(fp);
  }
  *dst = 0;
  // skip blank again
  while (is_blank(ch) || is_semi(ch)) {
    if (!semi && is_semi(ch)) semi = 1;
    if (feof(fp)) break;
    ch = fgetc(fp);
  }
  *end_semi = semi;
  return 1;
}

// skip brace ()
int skip_brace(char *src, char *dst)
{
  if (*(src++) != '(') return 0;
  while (*src != ')') *(dst++) = *(src++);
  *dst = 0;
  return 1;
}

// examine whether ch is a hex character or not
int is_hex_char(int ch)
{
  return ('0' <= ch && ch <= '9') || ('a' <= ch && ch <= 'f');
}

// get the value of the hex character
int value_of_hex_char(int ch)
{
  if ('0' <= ch && ch <= '9') return ch - '0';
  return (ch - 'a' + 10);
}

// get the hex character of the value
int hex_char_of_value(int v)
{
  if (v <= 9) return v + '0';
  return (v - 10 + 'a');
}

// examine whether keyw is ignore keyword or not
int is_ignore(char *keyw)
{
  int i;
  const char ignores[6][10] = {"HIR", "TIR", "HDR", "TDR", "TRST", "FREQUENCY"};

  for (i = 0; i < 6; i++) {
    if (!strncmp(ignores[i], keyw, strlen(keyw))) return 1;
  }
  return 0;
}

// examine whether keyw is SIR or SDR
int sir_sdr(char *keyw)
{
  int i;
  const char coms[2][4] = {"SIR", "SDR"};

  for (i = 0; i < 2; i++)
    if (!strncmp(coms[i], keyw, strlen(keyw))) return 1;
  return 0;
}

// make default mask of w bits
void make_mask(int w, char *dst)
{
  int i, d = 0, n = w / 4, m = w % 4; //, len = (w+3)/4;

  while (m-- > 0) d = d*2+1;
  if (d) *(dst++) = hex_char_of_value(d);
  for (i = 0; i < n; i++) *(dst++) = 'f';
  *dst = 0;
}

// make zero of w bits
void make_zero(int w, char *dst)
{
  int i, len = (w+3)/4;

  for (i=0; i<len; i++) *(dst++) = '0';
  *dst = 0;
}

// address TDI / TDO / SMASK / MASK
int do_param(FILE *fp, char *keyw, char* kind, char* param, int *semi)
{
  char buff[MAX_STR];
  if (!strncmp(keyw, kind, strlen(kind))) {
    if (!get_word(fp, buff, semi)) return 0;
    skip_brace(buff, param);
  }
  return 1;
}

// output bit data
int outBit(struct ftdi_context *p_ftdic, int tms, int tdi, int tdo, int mask,
           int flush, int *no_match, int mode)
{
  static int length = 0, explength[2]={0,0},
  first = 1, index = 0;
  static unsigned char buff[USB_BUFSIZE], expect[USB_BUFSIZE/2][2],
    result[USB_BUFSIZE];
  int written, read;
  int i, j;

  buff[length++] = ((tms << 1) | tdi);
  buff[length++] = (4 | (tms << 1) | tdi);
  if (mode == 1) expect[explength[index]++][index] = (mask ? (tdo | 2) : 0);

  // write read to/from USB
  if ((length == USB_BUFSIZE) || flush) {
    written = ftdi_write_data(p_ftdic, buff, length);
    if (written != length) return 0;
    length = 0;
    if (mode == 1) {
      if (!first) {
        for (j=0; j<(flush+1); j++) {
          int prev_idx = (index?0:1);
          read = ftdi_read_data(p_ftdic, result, explength[prev_idx]*2);
          if (read != (explength[prev_idx]*2)) return 0;
          for (i=0; i<explength[prev_idx]; i++) {
            int exp = (expect[i][prev_idx]&1);
            int msk = (expect[i][prev_idx]&2);
            int res = ((result[i*2+1]&8)?1:0);
            if (msk) {
              if ((exp != res) && no_match) {
                (*no_match)++;
              }
            }
          }
          explength[prev_idx] = 0;
          index = (index?0:1);
        }
        if (flush) first = 1;
      }
      else {
        index = (index?0:1);
        first = 0;
      }
    }
  }
  return 1;
}

// output data
int outData(struct ftdi_context *p_ftdic, int bitw, char *tdi, char *smask,
            char *tdo, char *mask, int *no_match, int mode)
{
  int i, tdi_ = 0, smask_ = 0, tdo_ = 0, mask_ = 0;

  while (*(tdi++)) ; tdi -= 2;
  while (*(smask++)) ; smask -= 2;
  while (*(tdo++)) ; tdo -= 2;
  while (*(mask++)) ; mask -= 2;
  for (i = 0; i < bitw; i++) {
    if ((i % 4) == 0) {
      tdi_ = value_of_hex_char(*(tdi--));
      smask_ = value_of_hex_char(*(smask--));
      tdo_ = value_of_hex_char(*(tdo--));
      mask_ = value_of_hex_char(*(mask--));
    }
    if (!outBit(p_ftdic, (i == (bitw-1)) ? 1 : 0, (tdi_&1), (tdo_&1), (mask_&1), 0, no_match, mode)) {
      return 0;
    }
    tdi_ >>= 1;
    smask_ >>= 1;
    tdo_ >>= 1;
    mask_ >>= 1;
  }
  return 1;
}

// reset TAP machine
int reset_tap(struct ftdi_context *p_ftdic, int *current_state, int mode)
{
  int i;
  for (i = 0; i < 5; i++) {
    if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) {
      return 0;
    }
  }
  outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode);
  *current_state = RUN_TEST;
  return 1;
}

// transit state
int transit(struct ftdi_context *p_ftdic, int *current, int next, int mode)
{
  if (*current == next) return 1;
  switch (*current) {
    case TEST_LOGIC_RESET:
      if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
      *current = RUN_TEST;
      transit(p_ftdic, current, next, mode);
      break;
    case RUN_TEST:
      if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
      *current = SELECT_DR_SCAN;
      transit(p_ftdic, current, next, mode);
      break;
    case SELECT_DR_SCAN:
      if (next == CAPTURE_DR || next == SHIFT_DR || next == EXIT1_DR ||
      next == PAUSE_DR || next == EXIT2_DR || next == UPDATE_DR) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = CAPTURE_DR;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = SELECT_IR_SCAN;
        transit(p_ftdic, current, next, mode);
      }
      break;
    case CAPTURE_DR:
      if (next == SHIFT_DR) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = SHIFT_DR;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = EXIT1_DR;
        transit(p_ftdic, current, next, mode);
      }
      break;
    case SHIFT_DR:
      if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
      *current = EXIT1_DR;
      transit(p_ftdic, current, next, mode);
      break;
    case EXIT1_DR:
      if (next == PAUSE_DR || next == EXIT2_DR || next == SHIFT_DR) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = PAUSE_DR;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = UPDATE_DR;
        transit(p_ftdic, current, next, mode);
      }
      break;
    case PAUSE_DR:
      if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
      *current = EXIT2_DR;
      transit(p_ftdic, current, next, mode);
      break;
    case EXIT2_DR:
      if (next == SHIFT_DR || next == EXIT1_DR || next == PAUSE_DR) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = SHIFT_DR;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = UPDATE_DR;
        transit(p_ftdic, current, next, mode);
      }
      break;
    case UPDATE_DR:
      if (next == RUN_TEST) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = RUN_TEST;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = SELECT_DR_SCAN;
        transit(p_ftdic, current, next, mode);
      }
      break;
    case SELECT_IR_SCAN:
      if (next == CAPTURE_IR || next == SHIFT_IR || next == EXIT1_IR ||
      next == PAUSE_IR || next == EXIT2_IR || next == UPDATE_IR) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = CAPTURE_IR;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = TEST_LOGIC_RESET;
        transit(p_ftdic, current, next, mode);
      }
      break;
    case CAPTURE_IR:
      if (next == SHIFT_IR) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = SHIFT_IR;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = EXIT1_IR;
        transit(p_ftdic, current, next, mode);
      }
      break;
    case SHIFT_IR:
      if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
      *current = EXIT1_IR;
      transit(p_ftdic, current, next, mode);
      break;
    case EXIT1_IR:
      if (next == PAUSE_IR || next == EXIT2_IR || next == SHIFT_IR) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = PAUSE_IR;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = UPDATE_IR;
        transit(p_ftdic, current, next, mode);
      }
      break;
    case PAUSE_IR:
      if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
      *current = EXIT2_IR;
      transit(p_ftdic, current, next, mode);
      break;
    case EXIT2_IR:
      if (next == SHIFT_IR || next == EXIT1_IR || next == PAUSE_IR) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = SHIFT_IR;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = UPDATE_IR;
        transit(p_ftdic, current, next, mode);
      }
      break;
    case UPDATE_IR:
      if (next == RUN_TEST) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = RUN_TEST;
        transit(p_ftdic, current, next, mode);
      }
      else {
        if (!outBit(p_ftdic, 1, 0, 0, 0, 0, NULL, mode)) return 0;
        *current = SELECT_DR_SCAN;
        transit(p_ftdic, current, next, mode);
      }
      break;
  }
  return 1;
}

// get state from state name
int state_of_string(char *n, int *s)
{
  if (!strncmp(n, "IDLE", 4)) *s = RUN_TEST;
  else if (!strncmp(n, "RESET", 5)) *s = TEST_LOGIC_RESET;
  else if (!strncmp(n, "IRPAUSE", 7)) *s = PAUSE_IR;
  else if (!strncmp(n, "DRPAUSE", 7)) *s = PAUSE_DR;
  else return 0;
  return 1;
}

// parse SVF file
int parse_svf(FILE *fp, struct ftdi_context *p_ftdic, int v, int *current_state,
              int *no_match, int mode)
{
  char keyw[MAX_STR], keyw2[MAX_STR], tdi[MAX_STR], tdo[MAX_STR];
  static char smask_sir[MAX_STR], mask_sir[MAX_STR];
  static char smask_sdr[MAX_STR], mask_sdr[MAX_STR];
  static char mask_tmp[MAX_STR];
  char *smask, *mask;
  int semi, ret, bitw, clks, i;
  int end_ir = RUN_TEST, end_dr = RUN_TEST;

  while (get_word(fp, keyw, &semi)) {
    if (is_ignore(keyw)) {
      do {
        ret = get_word(fp, keyw, &semi);
      } while (ret && !semi);
    }
    else if (sir_sdr(keyw)) {
      int next_state;

      if (!strncmp(keyw, "SIR", 3)) {
        next_state = SHIFT_IR;
        mask = mask_sir;
        smask = smask_sir;
      }
      else {
        next_state = SHIFT_DR;
        mask = mask_sdr;
        smask = smask_sdr;
      }
      if (!transit(p_ftdic, current_state, next_state, mode)) return 0;
      if (!get_word(fp, keyw2, &semi)) return 0;
      bitw = atoi(keyw2);
      *tdi = *tdo = 0;
      do {
        if (!get_word(fp, keyw2, &semi)) return 0;
        if (!do_param(fp, keyw2, "TDI", tdi, &semi)) return 0;
        if (!do_param(fp, keyw2, "TDO", tdo, &semi)) return 0;
        if (!do_param(fp, keyw2, "SMASK", smask, &semi)) return 0;
        if (!do_param(fp, keyw2, "MASK", mask, &semi)) return 0;
      } while (!semi);
      if (*tdo == 0) {
        make_zero(bitw, tdo);
        mask = mask_tmp;
        make_zero(bitw, mask);
      }
      if (v) {
        printf("%s %d TDI %s SMASK %s TDO %s MASK %s\n", keyw, bitw, tdi,
          smask, tdo, mask);
      }
      if (!outData(p_ftdic, bitw, tdi, smask, tdo, mask, no_match, mode)) {
        return 0;
      }
      if (!strncmp(keyw, "SIR", 3)) {
        *current_state = EXIT1_IR;
        if (!transit(p_ftdic, current_state, end_ir, mode)) return 0;
      }
      else {
        *current_state = EXIT1_DR;
        if (!transit(p_ftdic, current_state, end_dr, mode)) return 0;
      }
    }
    else if (!strncmp(keyw, "RUNTEST", 7)) {
      if (!transit(p_ftdic, current_state, RUN_TEST, mode)) return 0;
      if (!get_word(fp, keyw, &semi)) return 0;
      clks = atoi(keyw);
      if (!get_word(fp, keyw, &semi)) return 0;
      if (strncmp(keyw, "TCK", 3)) return 0;
      if (v) {
        printf("RUNTEST %d TCK\n", clks); fflush(stdout);
      }
      for (i = 0; i < clks; i++) {
        if (!outBit(p_ftdic, 0, 0, 0, 0, 0, NULL, mode)) {
          return 0;
        }
      }
    }
    else if (!strncmp(keyw, "STATE", 5)) {
      if (v) {
        printf("STATE ");
      }
      do {
        int n;
        if (!get_word(fp, keyw2, &semi)) return 0;
        if (!state_of_string(keyw2, &n)) return 0;
        if (!transit(p_ftdic, current_state, n, mode)) return 0;
        if (v) {
          printf("%s ", keyw2);
        }
      } while (!semi);
      if (v) printf("\n");
    }
    else if (!strncmp(keyw, "ENDIR", 5)) {
      int n;
      if (!get_word(fp, keyw2, &semi)) return 0;
      if (v) {
        printf("ENDIR %s\n", keyw2);
      }
      if (!state_of_string(keyw2, &n)) return 0;
      end_ir = n;
    }
    else if (!strncmp(keyw, "ENDDR", 5)) {
      int n;
      if (!get_word(fp, keyw2, &semi)) return 0;
      if (v) {
        printf("ENDDR %s\n", keyw2);
      }
      if (!state_of_string(keyw2, &n)) return 0;
      end_dr = n;
    } else return 0;
  }
  return 1;
}

int init(struct ftdi_context *p_ftdic)
{
  if (ftdi_init(p_ftdic)) {
    fprintf(stderr, "can't initialize ftdi_context");
    return 1;
  }
  return 0;
}

int fail(struct ftdi_context *p_ftdic)
{
  int rc = 0;
  char *err;
  err = ftdi_get_error_string(p_ftdic);
  if (strncmp(err, "all fine", 8)) {
    fprintf(stderr, "libftdi: %s\n", err);
    rc = 1;
  }
  ftdi_deinit(p_ftdic);
  return rc;
}

int listDevices(struct ftdi_context *p_ftdic,
                struct ftdi_device_list *p_deviceList, int product_id)
{
  struct ftdi_device_list *p_currentDevice;
  char p_manufacturer[128], p_description[128], p_serial[128];
  int i = 0;
  if (ftdi_usb_find_all(p_ftdic, &p_deviceList, 0x0403, product_id) < 0) {
    fprintf(stderr, "There was a problem listing the devices.\n");
    fail(p_ftdic);
    return 1;
  }
  if (p_deviceList == NULL) {
    printf("No devices found.\n Check connection and access permissions!\n");
    return 1;
  }
  p_currentDevice = p_deviceList;
  while (p_currentDevice != NULL) {
    if (ftdi_usb_get_strings(p_ftdic, p_currentDevice->dev,
         p_manufacturer, 128, p_description, 128, p_serial, 128) < 0)
    {
      fprintf(stderr, "Error: %s.\n", ftdi_get_error_string(p_ftdic));
      ftdi_list_free(&p_deviceList);
      ftdi_deinit(p_ftdic);
      return 1;
    }
    printf("Device: %d, Manufacturer: %s, Description: %s, Serial: %s\n",
      i, p_manufacturer, p_description, p_serial);
    p_currentDevice = p_currentDevice->next;
    ++i;
  }
  ftdi_list_free(&p_deviceList);
  return 0;
}

void usage()
{
  printf("easp [options] <svf file>\n");
  printf(" options:\n");
  printf("   -b <kbit/s> set bit rate\n");
  printf("   -c disable verification of TDO outputs\n");
  printf("   -d <device number> use FTDI device <number>\n");
  printf("   -l list FTDI USB devices\n");
  printf("   -p <product_id> use a different USB product ID\n");
  printf("   -v verbose\n");
  printf("   -h help\n");
  return;
}

int main(int argc, char *argv[])
{
  struct ftdi_context ftdic;
  struct ftdi_device_list *p_deviceList = NULL;
  FILE *fp;
  char *fname = NULL;
  int opt;
  int v = 0;
  int current_state;
  int deviceNumber = 0;
  int no_match = 0;
  int mode = 1;
  int op_list = 0;
  int baudrate = 2100;
  int product_id = 0x6001;
  unsigned char scratch;

  if (init(&ftdic)) return 1;

  while ((opt = getopt(argc, argv, "vclhd:p:b:")) != -1)
  {
    switch(opt)
    {
      case 'v':
        v = 1;
        break;
      case 'c':
        mode = 0;
        break;
      case 'l':
        op_list = 1;
        break;
      case 'h':
        usage();
        return 0;
        break;
      case 'd':
        deviceNumber = atoi(optarg);
        break;
      case 'p':
        product_id = (int) strtoul(optarg, NULL, 0);
        break;
      case 'b':
        baudrate = atoi(optarg);
        break;
      default:
        usage();
        return 0;
    }
  }

  if (op_list)
  {
    return (listDevices(&ftdic, p_deviceList, product_id));
  }

  if (optind >= argc)
  {
    usage();
    fprintf(stderr, "specify SVF file\n");
    return 0;
  }
  fname = argv[optind];
  
  if (!(fp = fopen(fname, "r"))) {
    fprintf(stderr, "can't open %s\n", fname);
    return 1;
  }
  if (ftdi_usb_open_desc_index(&ftdic, 0x0403, product_id, NULL, NULL,
    deviceNumber))
    goto ERROR2;
  while (ftdi_usb_reset(&ftdic)) {
    fprintf(stderr, "%s", ftdi_get_error_string(&ftdic));
    usleep(1000000);
  }
  if (ftdi_set_baudrate(&ftdic, baudrate*250)) goto ERROR1;
  // synchronous or asynchronous bit bang mode
  // (d3,d2,d1,d0) = (TDO,TCK,TMS,TDI)
  if (ftdi_set_bitmode(&ftdic, 7,
    (mode == 1) ? BITMODE_SYNCBB : BITMODE_BITBANG))
    goto ERROR1;
  if (ftdi_write_data_set_chunksize(&ftdic, USB_BUFSIZE)) goto ERROR1;
  if (ftdi_read_data_set_chunksize(&ftdic, USB_BUFSIZE)) goto ERROR1;
  if (ftdi_set_latency_timer(&ftdic, 1)) goto ERROR1;
  ftdic.usb_write_timeout = ftdic.usb_read_timeout = 2000;
  if (v) {
    if (ftdi_get_latency_timer(&ftdic, &scratch)) goto ERROR1;
    printf("usb write timeout: %d, usb read timeout: %d, latency: %d\n",
      ftdic.usb_write_timeout, ftdic.usb_read_timeout, scratch);
    printf(" max. packet size: %d, baud rate %d\n", ftdic.max_packet_size,
      ftdic.baudrate);
  }
  if (!reset_tap(&ftdic, &current_state, mode))  goto ERROR1;
  // purge the usb buffers and be sure to empty the rx buffer
  if (ftdi_usb_purge_buffers(&ftdic)) goto ERROR1;
  while (ftdi_read_data(&ftdic, &scratch, 1));
  printf("started programming, please relax...\n");
  if (!parse_svf(fp, &ftdic, v, &current_state, &no_match, mode)) {
    fprintf(stderr, "parse error\n");
    goto ERROR1;
  }
  if (mode == 1) {
    if (no_match > 0) {
      printf("\n   <<< %d TDO outputs didn't match to the expected values... >>>\n\n", no_match);
    }
    else {
      printf("\n   <<< All TDO outputs matched to the expected values!  >>>\n\n");
    }
  }
  // flush USB
  outBit(&ftdic, 0, 0, 0, 0, 1, NULL, mode);
  ftdi_set_bitmode(&ftdic, 0, BITMODE_RESET);
  ERROR1:
  ftdi_usb_close(&ftdic);
  ERROR2:
  fclose(fp);
  fflush(stderr);
  return fail(&ftdic);
}
