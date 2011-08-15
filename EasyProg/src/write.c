/*
 * EasyProg - write.c - Write cartridge image to flash
 *
 * (c) 2009 Thomas Giesel
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Thomas Giesel skoe@directbox.com
 */

#include <conio.h>
#include <string.h>
#include <stdlib.h>

#include "cart.h"
#include "screen.h"
#include "texts.h"
#include "cart.h"
#include "easyprog.h"
#include "flash.h"
#include "startupbin.h"
#include "write.h"
#include "filedlg.h"
#include "sprites.h"
#include "progress.h"
#include "timer.h"
#include "util.h"
#include "eload.h"

/******************************************************************************/
/* Static variables */

/* static to save some function call overhead */
static uint8_t  m_nBank;
static uint16_t m_nAddress;
static uint16_t m_nSize;
static BankHeader bankHeader;

/******************************************************************************/
/**
 * Print a status line, read the next bank header from the currently active
 * input and calculate m_nBank, m_nAddress and m_nSize.
 *
 * return CART_RV_OK, CART_RV_ERR or CART_RV_EOF
 */
static uint8_t readNextHeader()
{
    uint8_t rv;

    rv = readNextBankHeader(&bankHeader);

    m_nBank = bankHeader.bank[1] & FLASH_BANK_MASK;

    m_nAddress = 256 * bankHeader.loadAddr[0] + bankHeader.loadAddr[1];
    m_nSize = 256 * bankHeader.romLen[0] + bankHeader.romLen[1];

    return rv;
}


/******************************************************************************/
/**
 * Show an error dialog because writing a CRT image failed.
 * Return CART_RV_ERR
 */
static uint8_t writeCRTError(void)
{
    screenPrintSimpleDialog(apStrWriteCRTFailed);
    return CART_RV_ERR;
}


/******************************************************************************/
/**
 * Write the startup code to 00:1:xxxx. Patch it to use the right memory
 * configuration for the present cartridge type.
 *
 * Put the bank offset to be used in *pBankOffset. This offset must be added
 * to all banks of this cartridge. This is done to keep space on bank 00:1
 * for the start up code.
 *
 * return CART_RV_OK or CART_RV_ERR
 */
static uint8_t __fastcall__ writeStartUpCode(uint8_t* pBankOffset)
{
    uint8_t  nConfig;
    uint8_t* pBuffer;
    uint8_t  rv;

    // most CRT types are put on bank 1
    *pBankOffset = 1;

    switch (internalCartType)
    {
    case INTERNAL_CART_TYPE_NORMAL_8K:
        nConfig = EASYFLASH_IO_8K;
        break;

    case INTERNAL_CART_TYPE_NORMAL_16K:
        nConfig = EASYFLASH_IO_16K;
        break;

    case INTERNAL_CART_TYPE_ULTIMAX:
        nConfig = EASYFLASH_IO_ULTIMAX;
        break;

    case INTERNAL_CART_TYPE_OCEAN1:
        nConfig = EASYFLASH_IO_16K;
        *pBankOffset = 0;
        break;

    case INTERNAL_CART_TYPE_EASYFLASH:
        *pBankOffset = 0;
        return CART_RV_OK; // nothing to do

    case INTERNAL_CART_TYPE_EASYFLASH_XBANK:
        nConfig = nXbankConfig;
        break;

    default:
        screenPrintSimpleDialog(apStrUnsupportedCRTType);
        return CART_RV_ERR;
    }

    pBuffer = malloc(0x100);

    // btw: the buffer must always be 256 bytes long
    // !!! keep this crap in sync with startup.s - especially the code size !!!
    // copy the startup code to the buffer and patch the start bank and config
    memcpy(pBuffer, startUpStart, 0x100);

    // the 1st byte of this code is the start bank to be used - patch it
    pBuffer[0] = *pBankOffset;

    // the 2nd byte of this code is the memory config to be used - patch it
    pBuffer[1] = nConfig | EASYFLASH_IO_BIT_LED;

    // write the startup code to bank 0, always write 2 * 256 bytes
    if (!flashWriteBlock(0, 1, 0x1e00, pBuffer) ||
        !flashWriteBlock(0, 1, 0x1f00, startUpStart + 0x100))
    	goto err;


    // write the sprites to 00:1:1800
    // keep this in sync with sprites.s
    if (!flashWriteBlock(0, 1, 0x1800, pSprites) ||
        !flashWriteBlock(0, 1, 0x1900, pSprites + 0x100))
    	goto err;

	free(pBuffer);
    return CART_RV_OK;
err:
	free(pBuffer);
	return CART_RV_ERR;
}


/******************************************************************************/
/**
 * Write a cartridge image from the currently active input (file) to flash.
 *
 * return CART_RV_OK or CART_RV_ERR
 */
static uint8_t writeCrtImage(void)
{
    uint8_t rv;
    uint8_t nBankOffset;

    setStatus("Reading CRT image");
    if (!readCartHeader())
    {
        screenPrintSimpleDialog(apStrHeaderReadError);
        return CART_RV_ERR;
    }

    // this will show the cartridge type from the header
    refreshMainScreen();

    if (writeStartUpCode(&nBankOffset) != CART_RV_OK)
        return CART_RV_ERR;

    do
    {
        rv = readNextHeader();
        if (rv == CART_RV_ERR)
        {
            screenPrintSimpleDialog(apStrChipReadError);
            return CART_RV_ERR;
        }

        if (rv == CART_RV_OK)
        {
            m_nBank += nBankOffset;

            if ((m_nAddress == (uint16_t) ROM0_BASE) && (m_nSize <= 0x4000))
            {
                if (m_nSize > 0x2000)
                {
                    if (!flashWriteBankFromFile(m_nBank, 0, 0x2000) ||
                        !flashWriteBankFromFile(m_nBank, 1, m_nSize - 0x2000))
                    {
                        return CART_RV_ERR;
                    }
                }
                else
                {
                    if (!flashWriteBankFromFile(m_nBank, 0, m_nSize))
                        return CART_RV_ERR;
                }
            }
            else if (((m_nAddress == (uint16_t) ROM1_BASE) ||
                      (m_nAddress == (uint16_t) ROM1_BASE_ULTIMAX)) &&
                     (m_nSize <= 0x2000))
            {
                if (!flashWriteBankFromFile(m_nBank, 1, m_nSize))
                    return CART_RV_ERR;
            }
            else
            {
                screenPrintSimpleDialog(apStrUnsupportedCRTData);
                return CART_RV_ERR;
            }
        }

        // rv == CART_RV_EOF is the normal way to leave this loop
    }
    while (rv == CART_RV_OK);

    return CART_RV_OK;
}


/******************************************************************************/
/**
 * Write a BIN image from the given file to flash, either LOROM or HIROM,
 * beginning at m_nBank.
 *
 * return CART_RV_OK or CART_RV_ERR
 */
static uint8_t writeBinImage(uint8_t nChip)
{
    uint16_t nOffset;
    int      nBytes;
    uint8_t* pBuffer;
    uint8_t  rv;

    // this will show the cartridge type from the header
    refreshMainScreen();

    pBuffer = malloc(0x100);
    nOffset = 0;
    rv = CART_RV_ERR;
    do
    {
        nBytes = utilRead(pBuffer, 0x100);

        if (nBytes >= 0)
        {
            // the last block may be smaller than 265 bytes, then we write padding
            if (!flashWriteBlock(m_nBank, nChip, nOffset, pBuffer))
                goto ret;

            if (!flashVerifyBlock(m_nBank, nChip, nOffset, pBuffer))
                goto ret;

            nOffset += 0x100;
            if (nOffset == 0x2000)
            {
                nOffset = 0;
                ++m_nBank;
            }
        }
    }
    while (nBytes == 0x100);

    if (nOffset || m_nBank)
        rv = CART_RV_OK;
ret:
	free(pBuffer);
	return rv;
}


/******************************************************************************/
/**
 * Write an image file to the flash.
 *
 * imageType must be one of IMAGE_TYPE_CRT, IMAGE_TYPE_LOROM, IMAGE_TYPE_HIROM.
 */
static void checkWriteImage(uint8_t imageType)
{
    uint8_t  rv;

    checkFlashType();

    do
    {
        rv = fileDlg(imageType == IMAGE_TYPE_CRT ? "CRT" : "BIN");
        if (!rv)
            return;

        rv = utilOpenFile(0);
        if (rv == 1)
            screenPrintSimpleDialog(apStrFileOpenError);
    }
    while (rv != OPEN_FILE_OK);

    if (screenAskEraseDialog() != BUTTON_ENTER)
    {
        eload_close();
        return;
    }

    refreshMainScreen();
    setStatus("Checking file");

    // make sure the right areas of the chip are erased
    progressInit();
    timerStart();

    if (imageType == IMAGE_TYPE_CRT)
        rv = writeCrtImage();
    else if (imageType == IMAGE_TYPE_KERNAL)
    {
        m_nBank = FLASH_8K_SECTOR_BIT + 1;
        rv = writeBinImage(0);
    }
    else
    {
        m_nBank = 0;
        rv = writeBinImage(imageType == IMAGE_TYPE_HIROM);
    }
    eload_close();

    timerStop();

    if (rv == CART_RV_OK)
        screenPrintSimpleDialog(apStrWriteComplete);
}


/******************************************************************************/
/**
 * Write a CRT image file to the flash.
 */
void checkWriteCRTImage(void)
{
    checkWriteImage(IMAGE_TYPE_CRT);
}


/******************************************************************************/
/**
 * Write a KERNAL image file to the flash.
 */
void checkWriteKERNALImage(void)
{
    checkWriteImage(IMAGE_TYPE_KERNAL);
}


/******************************************************************************/
/**
 * Write a BIN image file to the LOROM flash.
 */
void checkWriteLOROMImage(void)
{
    checkWriteImage(IMAGE_TYPE_LOROM);
}


/******************************************************************************/
/**
 * Write a BIN image file to the HIROM flash.
 */
void checkWriteHIROMImage(void)
{
    checkWriteImage(IMAGE_TYPE_HIROM);
}
