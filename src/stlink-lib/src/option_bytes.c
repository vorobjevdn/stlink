/*
 * File: option_bytes.c
 *
 * Read and write option bytes and option control registers
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <stlink/stlink.h>
#include <stlink/option_bytes.h>

#include <stlink/common_flash.h>
#include <stlink/flash_loader.h>
#include <stlink/logging.h>
#include <stlink/map_file.h>
#include <stlink/md5.h>
#include <stlink/read_write.h>

/**
 * Read option control register C0
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_read_option_control_register_c0(stlink_t *sl, uint32_t *option_byte) {
  return stlink_read_debug32(sl, FLASH_C0_OPTR, option_byte);
}

/**
 * Read option bytes C0
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_read_option_bytes_c0(stlink_t *sl, uint32_t *option_byte) {
  return stlink_read_option_control_register_c0(sl, option_byte);
}

/**
 * Write option control register C0
 * @param sl
 * @param option_cr
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_control_register_c0(stlink_t *sl, uint32_t option_cr) {
  int32_t ret = 0;

  clear_flash_error(sl);

  if ((ret = stlink_write_debug32(sl, FLASH_C0_OPTR, option_cr)))
    return ret;

  wait_flash_busy(sl);

  uint32_t cr_reg = (1 << FLASH_C0_CR_OPTSTRT);
  if ((ret = stlink_write_debug32(sl, FLASH_C0_CR, cr_reg)))
    return ret;

  wait_flash_busy(sl);

  if ((ret = check_flash_error(sl)))
    return ret;

  // trigger the load of option bytes into option registers
  cr_reg = (1 << FLASH_C0_CR_OBL_LAUNCH);
  stlink_write_debug32(sl, FLASH_C0_CR, cr_reg);

  return ret;
}

/**
 * Write option bytes C0
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_c0(stlink_t *sl, stm32_addr_t addr, uint8_t *base, uint32_t len) {
  (void)addr;
  (void)len;

  return stlink_write_option_control_register_c0(sl, *(uint32_t*)base);
}

/**
 * Read option control register F0
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_control_register_f0(stlink_t *sl, uint32_t *option_byte) {
  DLOG("@@@@ Read option control register byte from %#10x\n", FLASH_OBR);
  return stlink_read_debug32(sl, FLASH_OBR, option_byte);
}

/**
 * Write option bytes F0
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_f0(stlink_t *sl, stm32_addr_t addr, uint8_t* base, uint32_t len) {
  int32_t ret = 0;

  if (len < 12 || addr != STM32_F0_OPTION_BYTES_BASE) {
    WLOG("Only full write of option bytes area is supported\n");
    return -1;
  }

  clear_flash_error(sl);

  WLOG("Erasing option bytes\n");

  /* erase option bytes */
  stlink_write_debug32(sl, FLASH_CR, (1 << FLASH_CR_OPTER) | (1 << FLASH_CR_OPTWRE));
  ret = stlink_write_debug32(sl, FLASH_CR, (1 << FLASH_CR_OPTER) | (1 << FLASH_CR_STRT) | (1 << FLASH_CR_OPTWRE));
  if (ret) {
    return ret;
  }

  wait_flash_busy(sl);

  ret = check_flash_error(sl);
  if (ret) {
    return ret;
  }

  WLOG("Writing option bytes to %#10x\n", addr);

  /* Set the Option PG bit to enable programming */
  stlink_write_debug32(sl, FLASH_CR, (1 << FLASH_CR_OPTPG) | (1 << FLASH_CR_OPTWRE));

  /* Use flash loader for write OP
   * because flash memory writable by half word */
  flash_loader_t fl;
  ret = stlink_flash_loader_init(sl, &fl);
  if (ret) {
    return ret;
  }
  ret = stlink_flash_loader_run(sl, &fl, addr, base, len);
  if (ret) {
    return ret;
  }

  /* Reload option bytes */
  stlink_write_debug32(sl, FLASH_CR, (1 << FLASH_CR_OBL_LAUNCH));

  return check_flash_error(sl);
}

/**
 * Write option control register F0
 * @param sl
 * @param option_cr
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_control_register_f0(stlink_t *sl, uint32_t option_cr) {
  int32_t ret = 0;
  uint16_t opt_val[8];
  uint32_t protection, optiondata;
  uint16_t user_options, user_data, rdp;
  uint32_t option_offset, user_data_offset;

  ILOG("Asked to write option control register %#10x to %#010x.\n", option_cr, FLASH_OBR);

  /* Clear errors */
  clear_flash_error(sl);

  /* Retrieve current values */
  ret = stlink_read_debug32(sl, FLASH_OBR, &optiondata);
  if (ret) {
    return ret;
  }
  ret = stlink_read_debug32(sl, FLASH_WRPR, &protection);
  if (ret) {
    return ret;
  }

  /* Translate OBR value to flash store structure
   * F0: RM0091, Option byte description, pp. 75-78
   * F1: PM0075, Option byte description, pp. 19-22
   * F3: RM0316, Option byte description, pp. 85-87 */
  switch(sl->chip_id)
  {
  case 0x422: /* STM32F30x */
  case 0x432: /* STM32F37x */
  case 0x438: /* STM32F303x6/8 and STM32F328 */
  case 0x446: /* STM32F303xD/E and STM32F398xE */
  case 0x439: /* STM32F302x6/8 */
  case 0x440: /* STM32F05x */
  case 0x444: /* STM32F03x */
  case 0x445: /* STM32F04x */
  case 0x448: /* STM32F07x */
  case 0x442: /* STM32F09x */
    option_offset = 6;
    user_data_offset = 16;
    rdp = 0x55AA;
    break;
  default:
    option_offset = 0;
    user_data_offset = 10;
    rdp = 0x5AA5;
    break;
  }

  user_options = (option_cr >> option_offset >> 2) & 0xFFFF;
  user_data = (option_cr >> user_data_offset) & 0xFFFF;

#define VAL_WITH_COMPLEMENT(v) (uint16_t) (((v)&0xFF) | (((~(v))<<8)&0xFF00))

  opt_val[0] = (option_cr & (1 << 1/*OPT_READOUT*/)) ? 0xFFFF : rdp;
  opt_val[1] = VAL_WITH_COMPLEMENT(user_options);
  opt_val[2] = VAL_WITH_COMPLEMENT(user_data);
  opt_val[3] = VAL_WITH_COMPLEMENT(user_data >> 8);
  opt_val[4] = VAL_WITH_COMPLEMENT(protection);
  opt_val[5] = VAL_WITH_COMPLEMENT(protection >> 8);
  opt_val[6] = VAL_WITH_COMPLEMENT(protection >> 16);
  opt_val[7] = VAL_WITH_COMPLEMENT(protection >> 24);

#undef VAL_WITH_COMPLEMENT

  /* Write bytes and check errors */
  ret = stlink_write_option_bytes_f0(sl, STM32_F0_OPTION_BYTES_BASE, (uint8_t*)opt_val, sizeof(opt_val));
  if (ret)
    return ret;

  ret = check_flash_error(sl);
  if (!ret) {
    ILOG("Wrote option bytes %#010x to %#010x!\n", option_cr,
         FLASH_OBR);
  }

  return ret;
}

/**
 * Read option control register F2
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_control_register_f2(stlink_t *sl, uint32_t *option_byte) {
  return stlink_read_debug32(sl, FLASH_F2_OPT_CR, option_byte);
}

/**
 * Read option bytes F2
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_bytes_f2(stlink_t *sl, uint32_t *option_byte) {
  return stlink_read_option_control_register_f2(sl, option_byte);
}

/**
 * Read option control register F4
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_control_register_f4(stlink_t *sl, uint32_t *option_byte) {
  return stlink_read_debug32(sl, FLASH_F4_OPTCR, option_byte);
}

/**
 * Read option bytes F4
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_bytes_f4(stlink_t *sl, uint32_t *option_byte) {
  return stlink_read_option_control_register_f4(sl, option_byte);
}

/**
 * Write option bytes F4
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_f4(stlink_t *sl, stm32_addr_t addr, uint8_t *base, uint32_t len) {
  uint32_t option_byte;
  int32_t ret = 0;
  (void)addr;
  (void)len;

  // Clear errors
  clear_flash_error(sl);

  write_uint32((unsigned char *)&option_byte, *(uint32_t *)(base));

  // write option byte, ensuring we dont lock opt, and set strt bit
  stlink_write_debug32(sl, FLASH_F4_OPTCR,
                       (option_byte & ~(1 << FLASH_F4_OPTCR_LOCK)) |
                           (1 << FLASH_F4_OPTCR_START));

  wait_flash_busy(sl);
  ret = check_flash_error(sl);

  // option bytes are reloaded at reset only, no obl. */
  return (ret);
}

/**
 * Read option bytes F7
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
// Since multiple bytes can be read, we read and print32_t all, but one here
// and then return the last one just like other devices.
int32_t stlink_read_option_bytes_f7(stlink_t *sl, uint32_t *option_byte) {
  int32_t err = -1;
  for (uint32_t counter = 0; counter < (sl->option_size / 4 - 1); counter++) {
    err = stlink_read_debug32(sl, sl->option_base + counter * sizeof(uint32_t), option_byte);
    if (err == -1) {
      return err;
    } else {
      printf("%08x\n", *option_byte);
    }
  }

  return stlink_read_debug32(
      sl,
      sl->option_base + (uint32_t) (sl->option_size / 4 - 1) * sizeof(uint32_t),
      option_byte);
}

/**
 * Write option bytes F7
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_f7(stlink_t *sl, stm32_addr_t addr, uint8_t *base, uint32_t len) {
  uint32_t option_byte;
  int32_t ret = 0;

  // Clear errors
  clear_flash_error(sl);

  ILOG("Asked to write option byte %#10x to %#010x.\n", *(uint32_t *)(base), addr);
  write_uint32((unsigned char *)&option_byte, *(uint32_t *)(base));
  ILOG("Write %d option bytes %#010x to %#010x!\n", len, option_byte, addr);

  if (addr == 0) {
    addr = FLASH_F7_OPTCR;
    ILOG("No address provided, using %#10x\n", addr);
  }

  if (addr == FLASH_F7_OPTCR) {
    /* write option byte, ensuring we dont lock opt, and set strt bit */
    stlink_write_debug32(sl, FLASH_F7_OPTCR,
                         (option_byte & ~(1 << FLASH_F7_OPTCR_LOCK)) |
                             (1 << FLASH_F7_OPTCR_START));
  } else if (addr == FLASH_F7_OPTCR1) {
    // Read FLASH_F7_OPTCR
    uint32_t oldvalue;
    stlink_read_debug32(sl, FLASH_F7_OPTCR, &oldvalue);
    /* write option byte */
    stlink_write_debug32(sl, FLASH_F7_OPTCR1, option_byte);
    // Write FLASH_F7_OPTCR lock and start address
    stlink_write_debug32(sl, FLASH_F7_OPTCR,
                         (oldvalue & ~(1 << FLASH_F7_OPTCR_LOCK)) |
                             (1 << FLASH_F7_OPTCR_START));
  } else {
    WLOG("WIP: write %#010x to address %#010x\n", option_byte, addr);
    stlink_write_debug32(sl, addr, option_byte);
  }

  wait_flash_busy(sl);

  ret = check_flash_error(sl);
  if (!ret)
    ILOG("Wrote %d option bytes %#010x to %#010x!\n", len, *(uint32_t *)base, addr);

  /* option bytes are reloaded at reset only, no obl. */

  return ret;
}

/**
 * Read option control register F7
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_control_register_f7(stlink_t *sl, uint32_t *option_byte) {
  DLOG("@@@@ Read option control register byte from %#10x\n", FLASH_F7_OPTCR);
  return stlink_read_debug32(sl, FLASH_F7_OPTCR, option_byte);
}

/**
 * Write option control register F7
 * @param sl
 * @param option_cr
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_control_register_f7(stlink_t *sl, uint32_t option_cr) {
  int32_t ret = 0;

  // Clear errors
  clear_flash_error(sl);

  ILOG("Asked to write option control register 1 %#10x to %#010x.\n",
       option_cr, FLASH_F7_OPTCR);

  /* write option byte, ensuring we dont lock opt, and set strt bit */
  stlink_write_debug32(sl, FLASH_F7_OPTCR,
                       (option_cr & ~(1 << FLASH_F7_OPTCR_LOCK)) |
                           (1 << FLASH_F7_OPTCR_START));

  wait_flash_busy(sl);

  ret = check_flash_error(sl);
  if (!ret)
    ILOG("Wrote option bytes %#010x to %#010x!\n", option_cr,
         FLASH_F7_OPTCR);

  return ret;
}

/**
 * Read option control register1 F7
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_control_register1_f7(stlink_t *sl, uint32_t *option_byte) {
  DLOG("@@@@ Read option control register 1 byte from %#10x\n",
       FLASH_F7_OPTCR1);
  return stlink_read_debug32(sl, FLASH_F7_OPTCR1, option_byte);
}

/**
 * Write option control register1 F7
 * @param sl
 * @param option_cr1
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_control_register1_f7(stlink_t *sl, uint32_t option_cr1) {
  int32_t ret = 0;

  // Clear errors
  clear_flash_error(sl);

  ILOG("Asked to write option control register 1 %#010x to %#010x.\n",
       option_cr1, FLASH_F7_OPTCR1);

  /* write option byte, ensuring we dont lock opt, and set strt bit */
  uint32_t current_control_register_value;
  stlink_read_debug32(sl, FLASH_F7_OPTCR, &current_control_register_value);

  /* write option byte */
  stlink_write_debug32(sl, FLASH_F7_OPTCR1, option_cr1);
  stlink_write_debug32(
      sl, FLASH_F7_OPTCR,
      (current_control_register_value & ~(1 << FLASH_F7_OPTCR_LOCK)) |
          (1 << FLASH_F7_OPTCR_START));

  wait_flash_busy(sl);

  ret = check_flash_error(sl);
  if (!ret)
    ILOG("Wrote option bytes %#010x to %#010x!\n", option_cr1, FLASH_F7_OPTCR1);

  return ret;
}

/**
 * Read option bytes boot address F7
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_bytes_boot_add_f7(stlink_t *sl, uint32_t *option_byte) {
  DLOG("@@@@ Read option byte boot address\n");
  return stlink_read_option_control_register1_f7(sl, option_byte);
}

/**
 * Write option bytes boot address F7
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_boot_add_f7(stlink_t *sl, uint32_t option_byte_boot_add) {
  ILOG("Asked to write option byte boot add %#010x.\n", option_byte_boot_add);
  return stlink_write_option_control_register1_f7(sl, option_byte_boot_add);
}

/**
 * Read option control register Gx
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_control_register_gx(stlink_t *sl, uint32_t *option_byte) {
  return stlink_read_debug32(sl, FLASH_Gx_OPTR, option_byte);
}

/**
 * Read option bytes Gx
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_bytes_gx(stlink_t *sl, uint32_t *option_byte) {
  return stlink_read_option_control_register_gx(sl, option_byte);
}

/**
 * Write option bytes Gx
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_gx(stlink_t *sl, stm32_addr_t addr, uint8_t *base, uint32_t len) {
  /* Write options bytes */
  uint32_t val;
  int32_t ret = 0;
  (void)len;
  uint32_t data;

  clear_flash_error(sl);

  write_uint32((unsigned char *)&data, *(uint32_t *)(base));
  WLOG("Writing option bytes %#10x to %#10x\n", data, addr);
  stlink_write_debug32(sl, FLASH_Gx_OPTR, data);

  // Set Options Start bit
  stlink_read_debug32(sl, FLASH_Gx_CR, &val);
  val |= (1 << FLASH_Gx_CR_OPTSTRT);
  stlink_write_debug32(sl, FLASH_Gx_CR, val);

  wait_flash_busy(sl);

  ret = check_flash_error(sl);

  // Reload options
  stlink_read_debug32(sl, FLASH_Gx_CR, &val);
  val |= (1 << FLASH_Gx_CR_OBL_LAUNCH);
  stlink_write_debug32(sl, FLASH_Gx_CR, val);

  return (ret);
}

/**
 * Write option bytes H7
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_h7(stlink_t *sl, stm32_addr_t addr, uint8_t *base, uint32_t len) {
  uint32_t val;
  uint32_t data;

  // Wait until previous flash option has completed
  wait_flash_busy(sl);

  // Clear previous error
  stlink_write_debug32(sl, FLASH_H7_OPTCCR,
                       1 << FLASH_H7_OPTCCR_CLR_OPTCHANGEERR);

  while (len != 0) {
    switch (addr) {
    case FLASH_H7_REGS_ADDR + 0x20: // FLASH_OPTSR_PRG
    case FLASH_H7_REGS_ADDR + 0x2c: // FLASH_PRAR_PRG1
    case FLASH_H7_REGS_ADDR + 0x34: // FLASH_SCAR_PRG1
    case FLASH_H7_REGS_ADDR + 0x3c: // FLASH_WPSN_PRG1
    case FLASH_H7_REGS_ADDR + 0x44: // FLASH_BOOT_PRG
      /* Write to FLASH_xxx_PRG registers */
      write_uint32((unsigned char *)&data, *(uint32_t *)(base)); // write options bytes

      WLOG("Writing option bytes %#10x to %#10x\n", data, addr);

      /* Skip if the value in the CUR register is identical */
      stlink_read_debug32(sl, addr - 4, &val);
      if (val == data) {
        break;
      }

      /* Write new option byte values and start modification */
      stlink_write_debug32(sl, addr, data);
      stlink_read_debug32(sl, FLASH_H7_OPTCR, &val);
      val |= (1 << FLASH_H7_OPTCR_OPTSTART);
      stlink_write_debug32(sl, FLASH_H7_OPTCR, val);

      /* Wait for the option bytes modification to complete */
      do {
        stlink_read_debug32(sl, FLASH_H7_OPTSR_CUR, &val);
      } while ((val & (1 << FLASH_H7_OPTSR_OPT_BUSY)) != 0);

      /* Check for errors */
      if ((val & (1 << FLASH_H7_OPTSR_OPTCHANGEERR)) != 0) {
        stlink_write_debug32(sl, FLASH_H7_OPTCCR, 1 << FLASH_H7_OPTCCR_CLR_OPTCHANGEERR);
        return -1;
      }
      break;

    default:
      /* Skip non-programmable registers */
      break;
    }

    len -= 4;
    addr += 4;
    base += 4;
  }

  return 0;
}

/**
 * Write option bytes L0
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_l0(stlink_t *sl, stm32_addr_t addr, uint8_t *base, uint32_t len) {
  uint32_t flash_base = get_stm32l0_flash_base(sl);
  uint32_t val;
  uint32_t data;
  int32_t ret = 0;

  // Clear errors
  clear_flash_error(sl);

  while (len != 0) {
    write_uint32((unsigned char *)&data,
                 *(uint32_t *)(base)); // write options bytes

    WLOG("Writing option bytes %#10x to %#10x\n", data, addr);
    stlink_write_debug32(sl, addr, data);
    wait_flash_busy(sl);

    if ((ret = check_flash_error(sl))) {
      break;
    }

    len -= 4;
    addr += 4;
    base += 4;
  }

  // Reload options
  stlink_read_debug32(sl, flash_base + FLASH_PECR_OFF, &val);
  val |= (1 << FLASH_L0_OBL_LAUNCH);
  stlink_write_debug32(sl, flash_base + FLASH_PECR_OFF, val);

  return (ret);
}

/**
 * Write option bytes L4
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_l4(stlink_t *sl, stm32_addr_t addr, uint8_t *base, uint32_t len) {

  uint32_t val;
  int32_t ret = 0;
  (void)addr;
  (void)len;

  // Clear errors
  clear_flash_error(sl);

  // write options bytes
  uint32_t data;
  write_uint32((unsigned char *)&data, *(uint32_t *)(base));
  WLOG("Writing option bytes 0x%04x\n", data);
  stlink_write_debug32(sl, FLASH_L4_OPTR, data);

  // set options start bit
  stlink_read_debug32(sl, FLASH_L4_CR, &val);
  val |= (1 << FLASH_L4_CR_OPTSTRT);
  stlink_write_debug32(sl, FLASH_L4_CR, val);

  wait_flash_busy(sl);
  ret = check_flash_error(sl);

  // apply options bytes immediate
  stlink_read_debug32(sl, FLASH_L4_CR, &val);
  val |= (1 << FLASH_L4_CR_OBL_LAUNCH);
  stlink_write_debug32(sl, FLASH_L4_CR, val);

  return (ret);
}

/**
 * Write option bytes WB
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_bytes_wb(stlink_t *sl, stm32_addr_t addr, uint8_t *base, uint32_t len) {
  /* Write options bytes */
  uint32_t val;
  int32_t ret = 0;
  (void)len;
  uint32_t data;

  clear_flash_error(sl);

  while (len != 0) {
    write_uint32((unsigned char *)&data, *(uint32_t *)(base)); // write options bytes

    WLOG("Writing option bytes %#10x to %#10x\n", data, addr);
    stlink_write_debug32(sl, addr, data);
    wait_flash_busy(sl);

    if ((ret = check_flash_error(sl))) {
      break;
    }

    len -= 4;
    addr += 4;
    base += 4;
  }

  // Set Options Start bit
  stlink_read_debug32(sl, FLASH_WB_CR, &val);
  val |= (1 << FLASH_WB_CR_OPTSTRT);
  stlink_write_debug32(sl, FLASH_WB_CR, val);

  wait_flash_busy(sl);

  ret = check_flash_error(sl);

  // Reload options
  stlink_read_debug32(sl, FLASH_WB_CR, &val);
  val |= (1 << FLASH_WB_CR_OBL_LAUNCH);
  stlink_write_debug32(sl, FLASH_WB_CR, val);

  return (ret);
}

/**
 * Read option control register WB
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_control_register_wb(stlink_t *sl, uint32_t *option_byte) {
  DLOG("@@@@ Read option control register byte from %#10x\n", FLASH_WB_OPTR);
  return stlink_read_debug32(sl, FLASH_WB_OPTR, option_byte);
}

/**
 * Write option control register WB
 * @param sl
 * @param option_cr
 * @return 0 on success, -ve on failure.
 */
static int32_t stlink_write_option_control_register_wb(stlink_t *sl, uint32_t option_cr) {
  int32_t ret = 0;

  // Clear errors
  clear_flash_error(sl);

  ILOG("Asked to write option control register 1 %#10x to %#010x.\n",
       option_cr, FLASH_WB_OPTR);

  /* write option byte, ensuring we dont lock opt, and set strt bit */
  stlink_write_debug32(sl, FLASH_WB_OPTR, option_cr);

  wait_flash_busy(sl);

  // Set Options Start bit
  uint32_t val = (1 << FLASH_WB_CR_OPTSTRT);
  stlink_write_debug32(sl, FLASH_WB_CR, val);

  wait_flash_busy(sl);

  ret = check_flash_error(sl);
  if (!ret)
    ILOG("Wrote option bytes %#010x to %#010x!\n", option_cr, FLASH_WB_OPTR);

  return ret;
}

/**
 * Read option bytes generic
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_bytes_generic(stlink_t *sl, uint32_t *option_byte) {
  DLOG("@@@@ Read option bytes boot address from %#10x\n", sl->option_base);
  return stlink_read_debug32(sl, sl->option_base, option_byte);
}

/**
 * Write option bytes
 * @param sl
 * @param addr of the memory mapped option bytes
 * @param base option bytes
 * @param len of option bytes
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_write_option_bytes(stlink_t *sl, stm32_addr_t addr, uint8_t *base, uint32_t len) {
  int32_t ret = -1;

  if (sl->option_base == 0) {
    ELOG("Option bytes writing is currently not supported for connected chip\n");
    return (-1);
  }

  if ((addr < sl->option_base) || addr > sl->option_base + sl->option_size) {
    ELOG("Option bytes start address out of Option bytes range\n");
    return (-1);
  }

  if (addr + len > sl->option_base + sl->option_size) {
    ELOG("Option bytes data too long\n");
    return (-1);
  }

  wait_flash_busy(sl);

  if (unlock_flash_if(sl)) {
    ELOG("Flash unlock failed! System reset required to be able to unlock it again!\n");
    return (-1);
  }

  if (unlock_flash_option_if(sl)) {
    ELOG("Flash option unlock failed!\n");
    return (-1);
  }

  switch (sl->flash_type) {
  case STM32_FLASH_TYPE_C0:
    ret = stlink_write_option_bytes_c0(sl, addr, base, len);
    break;
  case STM32_FLASH_TYPE_F0_F1_F3:
  case STM32_FLASH_TYPE_F1_XL:
    ret = stlink_write_option_bytes_f0(sl, addr, base, len);
    break;
  case STM32_FLASH_TYPE_F2_F4:
    ret = stlink_write_option_bytes_f4(sl, addr, base, len);
    break;
  case STM32_FLASH_TYPE_F7:
    ret = stlink_write_option_bytes_f7(sl, addr, base, len);
    break;
  case STM32_FLASH_TYPE_L0_L1:
    ret = stlink_write_option_bytes_l0(sl, addr, base, len);
    break;
  case STM32_FLASH_TYPE_L4:
    ret = stlink_write_option_bytes_l4(sl, addr, base, len);
    break;
  case STM32_FLASH_TYPE_G0:
  case STM32_FLASH_TYPE_G4:
    ret = stlink_write_option_bytes_gx(sl, addr, base, len);
    break;
  case STM32_FLASH_TYPE_H7:
    ret = stlink_write_option_bytes_h7(sl, addr, base, len);
    break;
  case STM32_FLASH_TYPE_WB_WL:
    ret = stlink_write_option_bytes_wb(sl, addr, base, len);
    break;
  default:
    ELOG("Option bytes writing is currently not implemented for connected chip\n");
    break;
  }

  if (ret) {
    ELOG("Flash option write failed!\n");
  } else {
    ILOG("Wrote %d option bytes to %#010x!\n", len, addr);
  }

  /* Re-lock flash. */
  lock_flash_option(sl);
  lock_flash(sl);

  return ret;
}

/**
 * Write the given binary file with option bytes
 * @param sl
 * @param path readable file path, should be binary image
 * @param addr of the memory mapped option bytes
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_fwrite_option_bytes(stlink_t *sl, const char *path, stm32_addr_t addr) {
  /* Write the file in flash at addr */
  int32_t err;
  mapped_file_t mf = MAPPED_FILE_INITIALIZER;

  if (map_file(&mf, path) == -1) {
    ELOG("map_file() == -1\n");
    return (-1);
  }

  printf("file %s ", path);
  md5_calculate(&mf);
  stlink_checksum(&mf);

  err = stlink_write_option_bytes(sl, addr, mf.base, (uint32_t) mf.len);
  stlink_fwrite_finalize(sl, addr);
  unmap_file(&mf);

  return (err);
}

/**
 * Read option control register 32
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_control_register32(stlink_t *sl, uint32_t *option_byte) {
  if (sl->option_base == 0) {
    ELOG("Option bytes read is currently not supported for connected chip\n");
    return -1;
  }

  switch (sl->flash_type) {
  case STM32_FLASH_TYPE_C0:
    return stlink_read_option_control_register_c0(sl, option_byte);
  case STM32_FLASH_TYPE_F0_F1_F3:
  case STM32_FLASH_TYPE_F1_XL:
    return stlink_read_option_control_register_f0(sl, option_byte);
  case STM32_FLASH_TYPE_F7:
    return stlink_read_option_control_register_f7(sl, option_byte);
  case STM32_FLASH_TYPE_WB_WL:
    return stlink_read_option_control_register_wb(sl, option_byte);
  default:
    return -1;
  }
}

/**
 * Write option control register 32
 * @param sl
 * @param option_cr
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_write_option_control_register32(stlink_t *sl, uint32_t option_cr) {
  int32_t ret = -1;

  wait_flash_busy(sl);

  if (unlock_flash_if(sl)) {
    ELOG("Flash unlock failed! System reset required to be able to unlock it again!\n");
    return -1;
  }

  if (unlock_flash_option_if(sl)) {
    ELOG("Flash option unlock failed!\n");
    return -1;
  }

  switch (sl->flash_type) {
  case STM32_FLASH_TYPE_C0:
    ret = stlink_write_option_control_register_c0(sl, option_cr);
    break;
  case STM32_FLASH_TYPE_F0_F1_F3:
  case STM32_FLASH_TYPE_F1_XL:
    ret = stlink_write_option_control_register_f0(sl, option_cr);
    break;
  case STM32_FLASH_TYPE_F7:
    ret = stlink_write_option_control_register_f7(sl, option_cr);
    break;
  case STM32_FLASH_TYPE_WB_WL:
    ret = 
        stlink_write_option_control_register_wb(sl, option_cr);
    break;
  default:
    ELOG("Option control register writing is currently not implemented for connected chip\n");
    break;
  }

  if (ret)
    ELOG("Flash option write failed!\n");
  else
    ILOG("Wrote option control register %#010x!\n", option_cr);

  /* Re-lock flash. */
  lock_flash_option(sl);
  lock_flash(sl);

  return ret;
}

/**
 * Read option control register1 32
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_control_register1_32(stlink_t *sl, uint32_t *option_byte) {
  if (sl->option_base == 0) {
    ELOG("Option bytes read is currently not supported for connected chip\n");
    return -1;
  }

  switch (sl->flash_type) {
  case STM32_FLASH_TYPE_F7:
    return stlink_read_option_control_register1_f7(sl, option_byte);
  default:
    return -1;
    // return stlink_read_option_control_register1_generic(sl, option_byte);
  }
}

/**
 * Write option control register1 32
 * @param sl
 * @param option_cr
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_write_option_control_register1_32(stlink_t *sl, uint32_t option_cr1) {
  int32_t ret = -1;

  wait_flash_busy(sl);

  if (unlock_flash_if(sl)) {
    ELOG("Flash unlock failed! System reset required to be able to unlock it again!\n");
    return -1;
  }

  if (unlock_flash_option_if(sl)) {
    ELOG("Flash option unlock failed!\n");
    return -1;
  }

  switch (sl->flash_type) {
  case STM32_FLASH_TYPE_F7:
    ret =
        stlink_write_option_control_register1_f7(sl, option_cr1);
    break;
  default:
    ELOG("Option control register 1 writing is currently not implemented for "
         "connected chip\n");
    break;
  }

  if (ret)
    ELOG("Flash option write failed!\n");
  else
    ILOG("Wrote option control register 1 %#010x!\n", option_cr1);

  lock_flash_option(sl);
  lock_flash(sl);

  return (ret);
}

/**
 * Read option bytes 32
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_bytes32(stlink_t *sl, uint32_t *option_byte) {
  if (sl->option_base == 0) {
    ELOG("Option bytes read is currently not supported for connected chip\n");
    return (-1);
  }

  switch (sl->chip_id) {
  case STM32_CHIPID_C011xx:
  case STM32_CHIPID_C031xx:
    return stlink_read_option_bytes_c0(sl, option_byte);
  case STM32_CHIPID_F2:
    return stlink_read_option_bytes_f2(sl, option_byte);
  case STM32_CHIPID_F4:
  case STM32_CHIPID_F446:
    return stlink_read_option_bytes_f4(sl, option_byte);
  case STM32_CHIPID_F76xxx:
    return stlink_read_option_bytes_f7(sl, option_byte);
  case STM32_CHIPID_G0_CAT1:
  case STM32_CHIPID_G0_CAT2:
  case STM32_CHIPID_G4_CAT2:
  case STM32_CHIPID_G4_CAT3:
  case STM32_CHIPID_G4_CAT4:
    return stlink_read_option_bytes_gx(sl, option_byte);
  default:
    return stlink_read_option_bytes_generic(sl, option_byte);
  }
}

/**
 * Write option bytes 32
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_write_option_bytes32(stlink_t *sl, uint32_t option_byte) {
  WLOG("About to write option byte %#10x to %#10x.\n", option_byte,
       sl->option_base);
  return stlink_write_option_bytes(sl, sl->option_base, (uint8_t *)&option_byte, 4);
}

/**
 * Read option bytes boot address 32
 * @param sl
 * @param option_byte
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_read_option_bytes_boot_add32(stlink_t *sl, uint32_t *option_byte) {
  if (sl->option_base == 0) {
    ELOG("Option bytes boot address read is currently not supported for connected chip\n");
    return -1;
  }

  switch (sl->flash_type) {
  case STM32_FLASH_TYPE_F7:
    return stlink_read_option_bytes_boot_add_f7(sl, option_byte);
  default:
    return -1;
    // return stlink_read_option_bytes_boot_add_generic(sl, option_byte);
  }
}

/**
 * Write option bytes boot address 32
 * @param sl
 * @param option_bytes_boot_add
 * @return 0 on success, -ve on failure.
 */
int32_t stlink_write_option_bytes_boot_add32(stlink_t *sl, uint32_t option_bytes_boot_add) {
  int32_t ret = -1;

  wait_flash_busy(sl);

  if (unlock_flash_if(sl)) {
    ELOG("Flash unlock failed! System reset required to be able to unlock it again!\n");
    return -1;
  }

  if (unlock_flash_option_if(sl)) {
    ELOG("Flash option unlock failed!\n");
    return -1;
  }

  switch (sl->flash_type) {
  case STM32_FLASH_TYPE_F7:
    ret = stlink_write_option_bytes_boot_add_f7(sl, option_bytes_boot_add);
    break;
  default:
    ELOG("Option bytes boot address writing is currently not implemented for connected chip\n");
    break;
  }

  if (ret)
    ELOG("Flash option write failed!\n");
  else
    ILOG("Wrote option bytes boot address %#010x!\n", option_bytes_boot_add);

  /* Re-lock flash. */
  lock_flash_option(sl);
  lock_flash(sl);

  return ret;
}
