/*
 * File: read_write.c
 *
 * Read and write operations
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <stlink/stlink.h>
#include <stlink/read_write.h>

#include <stlink/logging.h>

// Endianness
// https://commandcenter.blogspot.com/2012/04/byte-order-fallacy.html
// These functions encode and decode little endian uint16 and uint32 values.

uint16_t read_uint16(const unsigned char *c, const int32_t pt) {
  return ((uint16_t) c[pt]) | ((uint16_t) c[pt + 1] << 8);
}

void write_uint16(unsigned char *buf, uint16_t ui) {
  buf[0] = (uint8_t) ui;
  buf[1] = (uint8_t) (ui >> 8);
}

uint32_t read_uint32(const unsigned char *c, const int32_t pt) {
  return ((uint32_t) c[pt]) | ((uint32_t) c[pt + 1] << 8) |
         ((uint32_t) c[pt + 2] << 16) | ((uint32_t) c[pt + 3] << 24);
}

void write_uint32(unsigned char *buf, uint32_t ui) {
  buf[0] = ui;
  buf[1] = ui >> 8;
  buf[2] = ui >> 16;
  buf[3] = ui >> 24;
}

int32_t stlink_read_debug32(stlink_t *sl, uint32_t addr, uint32_t *data) {
  int32_t ret;

  ret = sl->backend->read_debug32(sl, addr, data);
  if (!ret)
    DLOG("*** stlink_read_debug32  %#010x at %#010x\n", *data, addr);

  return (ret);
}

int32_t stlink_write_debug32(stlink_t *sl, uint32_t addr, uint32_t data) {
  DLOG("*** stlink_write_debug32 %#010x to %#010x\n", data, addr);
  return sl->backend->write_debug32(sl, addr, data);
}

int32_t stlink_read_mem32(stlink_t *sl, uint32_t addr, uint16_t len) {
  DLOG("*** stlink_read_mem32 ***\n");

  if (len % 4 != 0) { // !!! never ever: fw gives just wrong values
    ELOG("Data length doesn't have a 32 bit alignment: +%d byte.\n", len % 4);
    return (-1);
  }

  return (sl->backend->read_mem32(sl, addr, len));
}

int32_t stlink_write_mem32(stlink_t *sl, uint32_t addr, uint16_t len) {
  DLOG("*** stlink_write_mem32 %u bytes to %#x\n", len, addr);

  if (len % 4 != 0) {
    ELOG("Data length doesn't have a 32 bit alignment: +%d byte.\n", len % 4);
    return (-1);
  }

  return (sl->backend->write_mem32(sl, addr, len));
}

int32_t stlink_write_mem8(stlink_t *sl, uint32_t addr, uint16_t len) {
  DLOG("*** stlink_write_mem8 ***\n");
  return (sl->backend->write_mem8(sl, addr, len));
}

int32_t stlink_read_reg(stlink_t *sl, int32_t r_idx, struct stlink_reg *regp) {
  DLOG("*** stlink_read_reg\n");
  DLOG(" (%d) ***\n", r_idx);

  if (r_idx > 20 || r_idx < 0) {
    fprintf(stderr, "Error: register index must be in [0..20]\n");
    return (-1);
  }

  return (sl->backend->read_reg(sl, r_idx, regp));
}

int32_t stlink_write_reg(stlink_t *sl, uint32_t reg, int32_t idx) {
  DLOG("*** stlink_write_reg\n");
  return (sl->backend->write_reg(sl, reg, idx));
}

int32_t stlink_read_unsupported_reg(stlink_t *sl, int32_t r_idx,
                                struct stlink_reg *regp) {
  int32_t r_convert;

  DLOG("*** stlink_read_unsupported_reg\n");
  DLOG(" (%d) ***\n", r_idx);

  /* Convert to values used by STLINK_REG_DCRSR */
  if (r_idx >= 0x1C &&
      r_idx <= 0x1F) { // primask, basepri, faultmask, or control
    r_convert = 0x14;
  } else if (r_idx == 0x40) { // FPSCR
    r_convert = 0x21;
  } else if (r_idx >= 0x20 && r_idx < 0x40) {
    r_convert = 0x40 + (r_idx - 0x20);
  } else {
    fprintf(stderr, "Error: register address must be in [0x1C..0x40]\n");
    return (-1);
  }

  return (sl->backend->read_unsupported_reg(sl, r_convert, regp));
}

int32_t stlink_write_unsupported_reg(stlink_t *sl, uint32_t val, int32_t r_idx,
                                 struct stlink_reg *regp) {
  int32_t r_convert;

  DLOG("*** stlink_write_unsupported_reg\n");
  DLOG(" (%d) ***\n", r_idx);

  /* Convert to values used by STLINK_REG_DCRSR */
  if (r_idx >= 0x1C &&
      r_idx <= 0x1F) {        /* primask, basepri, faultmask, or control */
    r_convert = r_idx;        // the backend function handles this
  } else if (r_idx == 0x40) { // FPSCR
    r_convert = 0x21;
  } else if (r_idx >= 0x20 && r_idx < 0x40) {
    r_convert = 0x40 + (r_idx - 0x20);
  } else {
    fprintf(stderr, "Error: register address must be in [0x1C..0x40]\n");
    return (-1);
  }

  return (sl->backend->write_unsupported_reg(sl, val, r_convert, regp));
}

int32_t stlink_read_all_regs(stlink_t *sl, struct stlink_reg *regp) {
  DLOG("*** stlink_read_all_regs ***\n");
  return (sl->backend->read_all_regs(sl, regp));
}

int32_t stlink_read_all_unsupported_regs(stlink_t *sl, struct stlink_reg *regp) {
  DLOG("*** stlink_read_all_unsupported_regs ***\n");
  return (sl->backend->read_all_unsupported_regs(sl, regp));
}
