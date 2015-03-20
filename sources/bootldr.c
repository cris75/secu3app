/* SECU-3  - An open source, free engine control unit
   Copyright (C) 2007 Alexey A. Shabelnikov. Ukraine, Kiev

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   contacts:
              http://secu-3.org
              email: shabelnikov@secu-3.org
*/

/** \file bootldr.c
 * \author Alexey A. Shabelnikov
 * Contains boot loader's body which inserted into firmware
 * (�������� ���� ���������� ������� ��������� � ��������).
 */

#include "port/pgmspace.h"
#include "port/port.h"
#include "bootldr.h"

/**Helpful macro, builds LDI AVR instruction */
#define LDIINS(imm) (0x80 | (imm & 0xF)), (0xE0 | (imm >> 4))

#if defined(BL_BAUD_RATE)
/**Encode specified baud rate value into AVR instruction (ldi)*/
//We use 20.000 mHz oscillator for ATMega644 and so we have different baud rate constants
#if (BL_BAUD_RATE == 9600)
 #define _LDIINS_L LDIINS(/*0x81*/0x03)
 #define _LDIINS_H LDIINS(/*0x00*/0x01)
#elif (BL_BAUD_RATE == 14400)
 #define _LDIINS_L LDIINS(/*0x56*/0xAD)
 #define _LDIINS_H LDIINS(0x00)
#elif (BL_BAUD_RATE == 19200)
 #define _LDIINS_L LDIINS(/*0x40*/0x81)
 #define _LDIINS_H LDIINS(0x00)
#elif (BL_BAUD_RATE == 28800)
 #define _LDIINS_L LDIINS(/*0x2A*/0x56)
 #define _LDIINS_H LDIINS(0x00)
#elif (BL_BAUD_RATE == 38400)
 #define _LDIINS_L LDIINS(/*0x20*/0x40)
 #define _LDIINS_H LDIINS(0x00)
#elif (BL_BAUD_RATE == 57600)
 #define _LDIINS_L LDIINS(/*0x15*/0x2A)   //note: error is 0.9%
 #define _LDIINS_H LDIINS(0x00)
#elif (BL_BAUD_RATE == 115200)
 #define _LDIINS_L LDIINS(/*0x0A*/0x15)   //note: error is 1.4%
 #define _LDIINS_H LDIINS(0x00)
#elif (BL_BAUD_RATE == 250000)
 #define _LDIINS_L LDIINS(/*0x04*/0x09)
 #define _LDIINS_H LDIINS(0x00)
#elif (BL_BAUD_RATE == 500000)
 #define _LDIINS_L LDIINS(0x04)
 #define _LDIINS_H LDIINS(0x00)
#else
 #error "Invalid baud rate value specified for boot loader!"
#endif

#else
 /**Default baud rate for boot loader (if not specified), low byte*/
 #define _LDIINS_L LDIINS(/*0x15*/0x2A)
 /**Default baud rate for boot loader, high byte*/
 #define _LDIINS_H LDIINS(/*0x00*/0x00)
#endif

/**following object will be inserted into the program memory independently on used it or not
 * (���� ������ ����� � ������ �������� ���������� �� ���� ������������ �� ��� ���).
 */
PGM_FIXED_ADDR_OBJ(uint8_t boot_loader[BOOT_LOADER_SIZE], ".boot_loader") =
{
 0xF8,0x94,0x00,0x24,0x07,0xB8,0x33,0x99,0xFD,0xC0,0xF8,0x94,0x8F,0xEF,0x90,0xE1,
 0x8D,0xBF,0x9E,0xBF,_LDIINS_L,0x80,0x93,0xC4,0x00,_LDIINS_H,0x80,0x93,0xC5,0x00,
 0x88,0xE1,0x80,0x93,0xC1,0x00,0x82,0xE0,0x80,0x93,0xC0,0x00,0x76,0xD0,0x01,0x32,
 0xE9,0xF7,0x73,0xD0,0x00,0x35,0xD9,0xF4,0xAB,0xD0,0xBA,0xD0,0x13,0xE0,0xBE,0xD0,
 0x11,0xE1,0xBC,0xD0,0x44,0x27,0x80,0xE0,0xA3,0xD0,0x00,0x2E,0x40,0x27,0xA0,0xD0,
 0x10,0x2E,0x40,0x27,0x11,0xE0,0xB2,0xD0,0x32,0x96,0x82,0x50,0xA9,0xF7,0xE0,0x50,
 0xF1,0x40,0x15,0xE0,0xAB,0xD0,0x56,0xD0,0x04,0x2F,0x87,0xD0,0xDF,0xCF,0x02,0x35,
 0x79,0xF4,0x8E,0xD0,0x9D,0xD0,0x4E,0xD0,0x11,0xE1,0xA0,0xD0,0x44,0x27,0x80,0xE0,
 0x05,0x91,0x40,0x27,0x7A,0xD0,0x81,0x50,0xD9,0xF7,0x04,0x2F,0x76,0xD0,0xCE,0xCF,
 0x0A,0x34,0x69,0xF4,0x3F,0xD0,0x44,0x27,0xAA,0x27,0xBB,0x27,0x11,0xE0,0x9A,0xD0,
 0x40,0x27,0x6B,0xD0,0xB8,0x30,0xD9,0xF7,0x04,0x2F,0x67,0xD0,0xBF,0xCF,0x07,0x35,
 0x71,0xF4,0x44,0x27,0xAA,0x27,0xBB,0x27,0x16,0xE0,0x6A,0xD0,0x00,0xBD,0x8A,0xD0,
 0x40,0x27,0xB8,0x30,0xD1,0xF7,0x26,0xD0,0x04,0x2F,0x57,0xD0,0xAF,0xCF,0x04,0x35,
 0x91,0xF4,0x20,0xD0,0x00,0xE4,0x29,0xD0,0x00,0x91,0xC0,0x00,0x05,0xFF,0xFC,0xCF,
 0x07,0xB7,0x00,0x00,0x06,0xFF,0x03,0xC0,0x11,0xE1,0x68,0xD0,0xF9,0xCF,0x08,0xE0,
 0x00,0x93,0x60,0x00,0xFF,0xCF,0x09,0x34,0x49,0xF4,0x0C,0xD0,0xEA,0xEE,0xF9,0xEF,
 0x05,0x91,0x00,0x23,0x11,0xF0,0x11,0xD0,0xFB,0xCF,0x90,0xCF,0x03,0xD0,0x0F,0xE3,
 0x0C,0xD0,0x8C,0xCF,0x0C,0xE3,0x09,0xD0,0x08,0x95,0xA8,0x95,0x00,0x91,0xC0,0x00,
 0x07,0xFF,0xFB,0xCF,0x00,0x91,0xC6,0x00,0x08,0x95,0xA8,0x95,0x10,0x91,0xC0,0x00,
 0x15,0xFF,0xFB,0xCF,0x00,0x93,0xC6,0x00,0x08,0x95,0x2F,0x93,0x10,0x2F,0x12,0x95,
 0x1F,0x70,0x1A,0x30,0x10,0xF0,0x27,0xE0,0x12,0x0F,0x20,0xE3,0x12,0x0F,0x0F,0x70,
 0x0A,0x30,0x10,0xF0,0x27,0xE0,0x02,0x0F,0x20,0xE3,0x02,0x0F,0x2F,0x91,0x08,0x95,
 0x1F,0x93,0x0A,0x33,0x08,0xF0,0x07,0x50,0x00,0x53,0x1A,0x33,0x08,0xF0,0x17,0x50,
 0x10,0x53,0x12,0x95,0x01,0x2B,0x1F,0x91,0x08,0x95,0x0F,0x93,0x1F,0x93,0xDD,0xDF,
 0x0F,0x93,0x01,0x2F,0xD2,0xDF,0x0F,0x91,0xD0,0xDF,0x1F,0x91,0x0F,0x91,0x08,0x95,
 0x1F,0x93,0xC3,0xDF,0x01,0x32,0x41,0xF0,0x10,0x2F,0xBF,0xDF,0x01,0x32,0x21,0xF0,
 0x0E,0x94,0xB0,0x7C,0x1F,0x91,0x08,0x95,0x1F,0x91,0x0F,0x91,0x0F,0x91,0x41,0xCF,
 0xF0,0x2F,0x00,0x00,0xEE,0x27,0x00,0x00,0x00,0x00,0x08,0x95,0x07,0xB7,0x00,0x00,
 0xA8,0x95,0x00,0xFD,0xFB,0xCF,0xA8,0x95,0xF9,0x99,0xFD,0xCF,0x17,0xBF,0x00,0x00,
 0xE8,0x95,0x08,0x95,0xA1,0xBD,0xB2,0xBD,0x11,0x96,0x11,0xFD,0xFA,0x9A,0x1F,0xBB,
 0xA8,0x95,0xF9,0x99,0xFD,0xCF,0x00,0xB5,0x08,0x95,0x42,0x6F,0x6F,0x74,0x20,0x6C,
 0x6F,0x61,0x64,0x65,0x72,0x20,0x76,0x31,0x2E,0x36,0x2E,0x5B,0x31,0x31,0x2E,0x31,

 0x33,0x5D,0x00,0x00,0x0C,0x94,0x00,0x80,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,

 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,

 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x36,0x34,0x34,0x20
};
