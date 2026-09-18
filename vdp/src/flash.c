// -----------------------------------------------------------------------------
// VoidVDP - A versatile serial terminal (fork of VersaTerm)
// Copyright (C) 2022 David Hansel
// Copyright (C) 2026 turboss
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#include "flash.h"
#include "terminal.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

// RP2040 has 2MB of flash, we use the top 64KB (16 sectors) for data storage
// Upper 5 sectors are used for font storage
#define FLASH_STORAGE_SIZE  65536
#define FLASH_TARGET_OFFSET (2048 * 1024 - (FLASH_STORAGE_SIZE))



uint32_t flash_get_write_offset(uint8_t sector)
{
  return sector<16 ? FLASH_TARGET_OFFSET+sector*4096 : 0;
}


uint8_t *flash_get_read_ptr(uint8_t sector)
{
  return sector<16 ? (uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE*sector) : NULL;
}


size_t flash_get_sector_size()
{
  return FLASH_SECTOR_SIZE;
}


int flash_write_partial(uint8_t sector, const void *data, size_t position, size_t size)
{
  int ok = 0;

  if( position+size <= FLASH_SECTOR_SIZE )
    {
      uint8_t *mem = malloc(FLASH_SECTOR_SIZE);
      if( mem!=NULL )
        {
          memcpy(mem, flash_get_read_ptr(sector), FLASH_SECTOR_SIZE);
          memcpy(mem+position, data, size);
          ok = flash_write(sector, mem, FLASH_SECTOR_SIZE);
          free(mem);
        }
    }

  return ok;
}


// Wrapper struct for flash_safe_execute callbacks
struct flash_op {
  uint32_t offs;
  const uint8_t *data;
  size_t count;
};

static void do_erase(void *param) {
  struct flash_op *op = (struct flash_op *)param;
  flash_range_erase(op->offs, op->count);
}

static void do_program(void *param) {
  struct flash_op *op = (struct flash_op *)param;
  flash_range_program(op->offs, op->data, op->count);
}

int flash_write(uint8_t sector, const void *data, size_t length)
{
  if( sector<16 && length<=FLASH_SECTOR_SIZE )
    {
      uint32_t offs = flash_get_write_offset(sector);
      struct flash_op op;

      // Initialize flash safe execute on core 0
      flash_safe_execute_core_init();

      // Erase sector safely
      op.offs = offs;
      op.count = FLASH_SECTOR_SIZE;
      if (flash_safe_execute(do_erase, &op, 1000) != 0) return 0;

      // Program pages
      size_t offset = 0;
      while( offset+FLASH_PAGE_SIZE<=length )
        {
          op.offs = offs + offset;
          op.data = (const uint8_t*)data + offset;
          op.count = FLASH_PAGE_SIZE;
          if (flash_safe_execute(do_program, &op, 100) != 0) return 0;
          offset += FLASH_PAGE_SIZE;
        }

      if( offset<length )
        {
          static uint8_t buffer[FLASH_PAGE_SIZE];
          memset(buffer, 0, FLASH_PAGE_SIZE);
          memcpy(buffer, (const uint8_t*)data + offset, length - offset);
          op.offs = offs + offset;
          op.data = buffer;
          op.count = FLASH_PAGE_SIZE;
          if (flash_safe_execute(do_program, &op, 100) != 0) return 0;
        }

      return memcmp(data, flash_get_read_ptr(sector), length)==0;
    }
  else
    return 0;
}


void flash_read(uint8_t sector, void *data, size_t length)
{
  uint8_t *ptr = flash_get_read_ptr(sector);
  if( ptr!=NULL ) memmove(data, ptr, length);
}
