/* ###################################################################
**     This component module is generated by Processor Expert. Do not modify it.
**     Filename    : McuOneWire.c
**     CDE edition : Community
**     Project     : FRDM-K64F_Generator
**     Processor   : MK64FN1M0VLL12
**     Component   : OneWire
**     Version     : Component 01.151, Driver 01.00, CPU db: 3.00.000
**     Repository  : Legacy User Components
**     Compiler    : GNU C Compiler
**     Date/Time   : 2017-07-09, 10:44, # CodeGen: 188
**     Abstract    :
**          This is a component implementing the 1-Wire protocol.
**     Settings    :
**          Component Name                                 : McuOneWire
**          Data Pin I/O                                   : SDK_BitIO
**          Write Pin                                      : Disabled
**          Timing                                         : 
**            A: Write 1 Low time (us)                     : 6
**            B: Write 1 High time (us)                    : 64
**            C: Write 0 Low time (us)                     : 60
**            D: Write 0 High time (us)                    : 10
**            E: Read delay time (us)                      : 3
**            A: Read Low time (us)                        : 6
**            F: Read delay time                           : 55
**            H: Reset low time (us)                       : 480
**            I: Reset response time (us)                  : 70
**            J: Reset wait time after reading device presence (us): 410
**            Total slot time (us)                         : 100
**          Buffers                                        : 
**            Input                                        : RBInput
**          Debug                                          : Enabled
**            Debug Read Pin                               : SDK_BitIO
**          CriticalSection                                : McuCriticalSection
**          Utility                                        : McuUtility
**          Wait                                           : McuWait
**          SDK                                            : McuLib
**          RTOS                                           : Enabled
**            RTOS                                         : McuRTOS
**          Shell                                          : Enabled
**            Shell                                        : McuShell
**     Contents    :
**         CalcCRC       - uint8_t McuOneWire_CalcCRC(uint8_t *data, uint8_t dataSize);
**         SendByte      - uint8_t McuOneWire_SendByte(uint8_t data);
**         SendBytes     - uint8_t McuOneWire_SendBytes(uint8_t *data, uint8_t count);
**         Receive       - uint8_t McuOneWire_Receive(uint8_t counter);
**         SendReset     - uint8_t McuOneWire_SendReset(void);
**         Count         - uint8_t McuOneWire_Count(void);
**         GetBytes      - uint8_t McuOneWire_GetBytes(uint8_t *data, uint8_t count);
**         GetByte       - uint8_t McuOneWire_GetByte(uint8_t *data);
**         strcatRomCode - uint8_t McuOneWire_strcatRomCode(uint8_t *buf, size_t bufSize, uint8_t...
**         ReadRomCode   - uint8_t McuOneWire_ReadRomCode(uint8_t *romCodeBuffer);
**         ResetSearch   - void McuOneWire_ResetSearch(void);
**         TargetSearch  - void McuOneWire_TargetSearch(uint8_t familyCode);
**         Search        - bool McuOneWire_Search(uint8_t *newAddr, bool search_mode);
**         ParseCommand  - uint8_t McuOneWire_ParseCommand(const unsigned char* cmd, bool *handled,...
**         Deinit        - void McuOneWire%.Init(void) McuOneWire_Deinit(void);
**         Init          - void McuOneWire%.Init(void) McuOneWire_Init(void);
**
**     * Copyright (c) Original implementation: Omar Isa� Pinales Ayala, 2014, all rights reserved.
**      * Updated and maintained by Erich Styger, 2014-2017
**      * Web:         https://mcuoneclipse.com
**      * SourceForge: https://sourceforge.net/projects/mcuoneclipse
**      * Git:         https://github.com/ErichStyger/McuOnEclipse_PEx
**      * All rights reserved.
**      *
**      * Redistribution and use in source and binary forms, with or without modification,
**      * are permitted provided that the following conditions are met:
**      *
**      * - Redistributions of source code must retain the above copyright notice, this list
**      *   of conditions and the following disclaimer.
**      *
**      * - Redistributions in binary form must reproduce the above copyright notice, this
**      *   list of conditions and the following disclaimer in the documentation and/or
**      *   other materials provided with the distribution.
**      *
**      * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
**      * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
**      * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
**      * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
**      * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
**      * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**      * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
**      * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
**      * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
**      * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
** ###################################################################*/
/*!
** @file McuOneWire.c
** @version 01.00
** @brief
**          This is a component implementing the 1-Wire protocol.
*/         
/*!
**  @addtogroup McuOneWire_module McuOneWire module documentation
**  @{
*/         

/* MODULE McuOneWire. */

#include "McuOneWire.h"
#include "DQ1.h" /* data pin */
#include "InputRB1.h" /* input ring buffer */
#include "McuUtility.h" /* Utility */
#include "McuWait.h" /* Waiting */

/* global search state and information */
static unsigned char ROM_NO[8];
static uint8_t LastDiscrepancy;
static uint8_t LastFamilyDiscrepancy;
static uint8_t LastDeviceFlag;

/* Rom commands */
#define RC_READ_ROM          0x33
#define RC_MATCH_ROM         0x55
#define RC_SKIP_ROM          0xCC
#define RC_SEARCH_COND       0xEC
#define RC_SEARCH            0xF0
#define RC_RELEASE           0xFF

#if McuOneWire_CONFIG_WRITE_PIN /* extra pin only for write bit */
  #define DQ_Init               DQ1_Init(); McuOneWire_CONFIG_WRITE_PIN_INIT
  #define DQ_Deinit             DQ1_Deinit(); McuOneWire_CONFIG_WRITE_PIN_DEINIT
#else
  #define DQ_Init               DQ1_Init()
  #define DQ_Deinit             DQ1_Deinit()
#endif
#if McuOneWire_CONFIG_WRITE_PIN /* using dedicated circuit with separate pin to control the 1-wire write */
  #define DQ_SetLow             McuOneWire_CONFIG_WRITE_PIN_SET_OUTPUT
  #define DQ_Low                McuOneWire_CONFIG_WRITE_PIN_HIGH
  #define DQ_Floating           McuOneWire_CONFIG_WRITE_PIN_LOW
#else
  #define DQ_SetLow             DQ1_ClrVal()
  #define DQ_Low                DQ1_SetOutput()
  #define DQ_Floating           DQ1_SetInput()
#endif
#if McuOneWire_CONFIG_DEBUG_READ_PIN_ENABLED
  #define DBG_Init              McuOneWire_CONFIG_DEBUG_READ_PIN_INIT
  #define DBG_Deinit            McuOneWire_CONFIG_DEBUG_READ_PIN_DEINIT
  #define DQ_Read               (McuOneWire_CONFIG_DEBUG_READ_PIN_TOGGLE, DQ1_GetVal()!=0)
#else
  #define DBG_Init              /* empty */
  #define DBG_Deinit            /* empty */
  #define DQ_Read               (DQ1_GetVal()!=0)
#endif

static uint8_t read_bit(void) {
  uint8_t bit;
  McuCriticalSection_CriticalVariable();

  McuCriticalSection_EnterCritical();
  DQ_Low;
  McuWait_Waitus(McuOneWire_CONFIG_A_READ_LOW_TIME);
  DQ_Floating;
  McuWait_Waitus(McuOneWire_CONFIG_E_BEFORE_READ_DELAY_TIME);
  bit = DQ_Read;
  McuCriticalSection_ExitCritical();
  McuWait_Waitus(McuOneWire_CONFIG_F_AFTER_READ_DELAY_TIME);
  return bit;
}

static void write_bit(uint8_t bit) {
  McuCriticalSection_CriticalVariable();

  if (bit&1) {
    McuCriticalSection_EnterCritical();
    DQ_Low;
    McuWait_Waitus(McuOneWire_CONFIG_A_WRITE_1_LOW_TIME);
    DQ_Floating;
    McuWait_Waitus(McuOneWire_CONFIG_B_WRITE_1_HIGH_TIME);
    McuCriticalSection_ExitCritical();
  } else { /* zero bit */
    McuCriticalSection_EnterCritical();
    DQ_Low;
    McuWait_Waitus(McuOneWire_CONFIG_C_WRITE_0_LOW_TIME);
    DQ_Floating;
    McuWait_Waitus(McuOneWire_CONFIG_D_WRITE_0_HIGH_TIME);
    McuCriticalSection_ExitCritical();
  }
}

#if McuOneWire_CONFIG_PARSE_COMMAND_ENABLED
static uint8_t PrintStatus(const McuShell_StdIOType *io) {
  McuShell_SendStatusStr((unsigned char*)"McuOneWire", (unsigned char*)"\r\n", io->stdOut);
#if McuOneWire_CONFIG_DEBUG_READ_PIN_ENABLED
  McuShell_SendStatusStr((unsigned char*)"  debug pin", (unsigned char*)"yes\r\n", io->stdOut);
#else
  McuShell_SendStatusStr((unsigned char*)"  debug pin", (unsigned char*)"no\r\n", io->stdOut);
#endif
  return ERR_OK;
}

static uint8_t PrintHelp(const McuShell_StdIOType *io) {
  McuShell_SendHelpStr((unsigned char*)"McuOneWire", (unsigned char*)"Group of McuOneWire commands\r\n", io->stdOut);
  McuShell_SendHelpStr((unsigned char*)"  help|status", (unsigned char*)"Print help or status information\r\n", io->stdOut);
  McuShell_SendHelpStr((unsigned char*)"  reset", (unsigned char*)"Send a RESET sequence to the bus\r\n", io->stdOut);
  McuShell_SendHelpStr((unsigned char*)"  read rom", (unsigned char*)"Send a READ ROM (0x33) to the bus\r\n", io->stdOut);
  McuShell_SendHelpStr((unsigned char*)"  search", (unsigned char*)"Search for devices on the bus\r\n", io->stdOut);
  return ERR_OK;
}
#endif /* McuOneWire_CONFIG_PARSE_COMMAND_ENABLED */


/*
** ===================================================================
**     Method      :  McuOneWire_SendReset (component OneWire)
**     Description :
**         Sends a reset to the bus
**     Parameters  : None
**     Returns     :
**         ---             - error code
** ===================================================================
*/
uint8_t McuOneWire_SendReset(void)
{
  uint8_t bit;
  McuCriticalSection_CriticalVariable();

  McuCriticalSection_EnterCritical();
  DQ_Low;
  McuWait_Waitus(McuOneWire_CONFIG_H_RESET_TIME);
  DQ_Floating;
  McuWait_Waitus(McuOneWire_CONFIG_I_RESET_RESPONSE_TIME);
  bit = DQ_Read;
  McuCriticalSection_ExitCritical();
  McuWait_Waitus(McuOneWire_CONFIG_J_RESET_WAIT_TIME);
  if (!bit) { /* a device pulled the data line low: at least one device is present */
    return ERR_OK;
  } else {
    return ERR_BUSOFF; /* no device on the bus? */
  }
}

/*
** ===================================================================
**     Method      :  McuOneWire_SendByte (component OneWire)
**     Description :
**         Sends a single byte
**     Parameters  :
**         NAME            - DESCRIPTION
**         data            - the data byte to be sent
**     Returns     :
**         ---             - error code
** ===================================================================
*/
uint8_t McuOneWire_SendByte(uint8_t data)
{
  int i;

  for(i=0;i<8;i++) {
    write_bit(data&1); /* send LSB first */
    data >>= 1; /* next bit */
  } /* for */
  return ERR_OK;
}

/*
** ===================================================================
**     Method      :  McuOneWire_SendBytes (component OneWire)
**     Description :
**         Sends multiple bytes
**     Parameters  :
**         NAME            - DESCRIPTION
**       * data            - Pointer to the array of bytes
**         count           - Number of bytes to be sent
**     Returns     :
**         ---             - error code
** ===================================================================
*/
uint8_t McuOneWire_SendBytes(uint8_t *data, uint8_t count)
{
  uint8_t res;

  while(count>0) {
    res = McuOneWire_SendByte(*data);
    if (res!=ERR_OK) {
      return res; /* failed */
    }
    data++;
    count--;
  }
  return ERR_OK;
}

/*
** ===================================================================
**     Method      :  McuOneWire_Receive (component OneWire)
**     Description :
**         Programs a read operation after the master send all in
**         output buffer. Don't use a SendReset while the data is
**         coming.
**     Parameters  :
**         NAME            - DESCRIPTION
**         counter         - Number of bytes to receive from
**                           slave
**     Returns     :
**         ---             - error code
** ===================================================================
*/
uint8_t McuOneWire_Receive(uint8_t counter)
{
  int i;
  uint8_t val, mask;

  while(counter>0) {
    val = 0; mask = 1;
    for(i=0;i<8;i++) {
      if (read_bit()) { /* read bits (LSB first) */
        val |= mask;
      }
      mask <<= 1; /* next bit */
    } /* for */
    (void)InputRB1_Put(val); /* put it into the queue so it can be retrieved by GetBytes() */
    counter--;
  }
  return ERR_OK;
}

/*
** ===================================================================
**     Method      :  McuOneWire_Count (component OneWire)
**     Description :
**         Returns the number of elements stored on input buffer that
**         are ready to read.
**     Parameters  : None
**     Returns     :
**         ---             - number of elements
** ===================================================================
*/
uint8_t McuOneWire_Count(void)
{
  return InputRB1_NofElements();
}

/*
** ===================================================================
**     Method      :  McuOneWire_GetByte (component OneWire)
**     Description :
**         Get a single byte from the bus
**     Parameters  :
**         NAME            - DESCRIPTION
**       * data            - Pointer to were to store the data
**     Returns     :
**         ---             - error code
** ===================================================================
*/
uint8_t McuOneWire_GetByte(uint8_t *data)
{
  if (InputRB1_NofElements()==0) {
    return ERR_FAILED;
  }
  (void)InputRB1_Get(data);
  return ERR_OK;
}

/*
** ===================================================================
**     Method      :  McuOneWire_GetBytes (component OneWire)
**     Description :
**         Gets multiple bytes from the bus
**     Parameters  :
**         NAME            - DESCRIPTION
**       * data            - Pointer to where to store the data
**         count           - Number of bytes
**     Returns     :
**         ---             - error code
** ===================================================================
*/
uint8_t McuOneWire_GetBytes(uint8_t *data, uint8_t count)
{
  if(count > InputRB1_NofElements()) {
    return ERR_FAILED;
  }
  for(;count>0;count--) {
    (void)InputRB1_Get(data);
    data++;
  }
  return ERR_OK;
}

/*
** ===================================================================
**     Method      :  McuOneWire_CalcCRC (component OneWire)
**     Description :
**         Calculates the CRC over a number of bytes
**     Parameters  :
**         NAME            - DESCRIPTION
**       * data            - Pointer to data
**         dataSize        - number of data bytes
**     Returns     :
**         ---             - calculated CRC
** ===================================================================
*/
uint8_t McuOneWire_CalcCRC(uint8_t *data, uint8_t dataSize)
{
  uint8_t crc, i, x, y;

  crc = 0;
  for(x=0;x<dataSize;x++){
    y = data[x];
    for(i=0;i<8;i++) { /* go through all bits of the data byte */
      if((crc&0x01)^(y&0x01)) {
        crc >>= 1;
        crc ^= 0x8c;
      } else {
        crc >>= 1;
      }
      y >>= 1;
    }
  }
  return crc;
}

/*
** ===================================================================
**     Method      :  McuOneWire_Init (component OneWire)
**     Description :
**         Initializes this device.
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void McuOneWire_Init(void)
{
#if McuLib_CONFIG_NXP_SDK_USED
  /* using SDK, need to initialize inherited components */
  DQ_Init; /* data pin */
  DBG_Init; /* optional debug pin */
  InputRB1_Init(); /* input ring buffer */
#endif
  DQ_Floating; /* input mode, let the pull-up take the signal high */
  /* load LOW to output register. We won't change that value afterwards, we only switch between output and input/float mode */
  DQ_SetLow;
}

/*
** ===================================================================
**     Method      :  McuOneWire_Deinit (component OneWire)
**     Description :
**         Driver de-initialization
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void McuOneWire_Deinit(void)
{
  DQ_Deinit; /* data pin */
  DQ_Floating; /* input mode, tristate pin */
  DBG_Deinit; /* optional debug pin */
  InputRB1_Deinit(); /* input ring buffer */
}


/*
** ===================================================================
**     Method      :  McuOneWire_ParseCommand (component OneWire)
**     Description :
**         Shell Command Line parser. Method is only available if Shell
**         is enabled in the component properties.
**     Parameters  :
**         NAME            - DESCRIPTION
**         cmd             - command string
**       * handled         - Pointer to variable which tells if
**                           the command has been handled or not
**         io              - Pointer to I/O structure
**     Returns     :
**         ---             - Error code
** ===================================================================
*/
uint8_t McuOneWire_ParseCommand(const unsigned char* cmd, bool *handled, const McuShell_StdIOType *io)
{
#if McuOneWire_CONFIG_PARSE_COMMAND_ENABLED
  uint8_t res = ERR_OK;
  uint8_t buf[32];

  if (McuUtility_strcmp((char*)cmd, McuShell_CMD_HELP) == 0
    || McuUtility_strcmp((char*)cmd, "McuOneWire help") == 0)
  {
    *handled = TRUE;
    return PrintHelp(io);
  } else if (   (McuUtility_strcmp((char*)cmd, McuShell_CMD_STATUS)==0)
             || (McuUtility_strcmp((char*)cmd, "McuOneWire status")==0)
            )
  {
    *handled = TRUE;
    res = PrintStatus(io);
  } else if (McuUtility_strcmp((char*)cmd, "McuOneWire read rom")==0) {
    uint8_t rom[McuOneWire_ROM_CODE_SIZE];

    *handled = TRUE;
    res = McuOneWire_ReadRomCode(&rom[0]);
    if (res!=ERR_OK) {
      McuUtility_strcpy(buf, sizeof(buf), (unsigned char*)"ReadRomCode() ERROR (");
      McuUtility_strcatNum8u(buf, sizeof(buf), res);
      McuUtility_strcat(buf, sizeof(buf), (unsigned char*)")\r\n");
      McuShell_SendStr(buf, io->stdErr);
    } else {
      buf[0] = '\0';
      (void)McuOneWire_strcatRomCode(buf, sizeof(buf), &rom[0]);
      McuUtility_strcat(buf, sizeof(buf), (unsigned char*)"\r\n");
      McuShell_SendStr(buf, io->stdOut);
    }
  } else if (McuUtility_strcmp((char*)cmd, "McuOneWire reset")==0) {
    *handled = TRUE;
    res = McuOneWire_SendReset();
    if (res==ERR_OK) {
      McuShell_SendStr((unsigned char*)"Device present\r\n", io->stdOut);
    } else {
      McuShell_SendStr((unsigned char*)"No device present?\r\n", io->stdErr);
    }
  } else if (McuUtility_strcmp((char*)cmd, "McuOneWire search")==0) {
    uint8_t rom[McuOneWire_ROM_CODE_SIZE];
    bool found;
    int nofFound = 0;

    *handled = TRUE;
    McuOneWire_ResetSearch();
    do {
      found = McuOneWire_Search(&rom[0], TRUE);
      if (found) {
        nofFound++;
        buf[0] = '\0';
        (void)McuOneWire_strcatRomCode(buf, sizeof(buf), &rom[0]);
        McuUtility_strcat(buf, sizeof(buf), (unsigned char*)"\r\n");
        McuShell_SendStr(buf, io->stdOut);
      }
    } while(found);
    if (nofFound==0) {
      McuShell_SendStr((unsigned char*)"No device found!\r\n", io->stdErr);
    }
    return ERR_OK;
  }
  return res;
#else
  (void)cmd;
  (void)handled;
  (void)io;
  return ERR_OK;
#endif
}

/*
** ===================================================================
**     Method      :  McuOneWire_ReadRomCode (component OneWire)
**     Description :
**         Read the ROM code. Only works with one device on the bus.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * romCodeBuffer   - Pointer to a buffer
**                           with 8 bytes where the ROM code gets stored
**     Returns     :
**         ---             - Error code
** ===================================================================
*/
uint8_t McuOneWire_ReadRomCode(uint8_t *romCodeBuffer)
{
  uint8_t res;

  McuOneWire_SendReset();
  McuOneWire_SendByte(RC_READ_ROM);
  McuOneWire_Receive(McuOneWire_ROM_CODE_SIZE); /* 8 bytes for the ROM code */
  McuOneWire_SendByte(RC_RELEASE);
  /* copy ROM code */
  res = McuOneWire_GetBytes(romCodeBuffer, McuOneWire_ROM_CODE_SIZE); /* 8 bytes */
  if (res!=ERR_OK) {
    return res; /* error */
  }
  /* index 0  : family code
     index 1-6: 48bit serial number
     index 7  : CRC
  */
  if (McuOneWire_CalcCRC(&romCodeBuffer[0], McuOneWire_ROM_CODE_SIZE-1)!=romCodeBuffer[McuOneWire_ROM_CODE_SIZE-1]) {
    return ERR_CRC; /* wrong CRC? */
  }
  return ERR_OK; /* ok */
}

/*
** ===================================================================
**     Method      :  McuOneWire_strcatRomCode (component OneWire)
**     Description :
**         Appends the ROM code to a string.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * buf             - Pointer to zero terminated buffer
**         bufSize         - size of buffer
**       * romCode         - Pointer to 8 bytes of ROM Code
**     Returns     :
**         ---             - error code
** ===================================================================
*/
uint8_t McuOneWire_strcatRomCode(uint8_t *buf, size_t bufSize, uint8_t *romCode)
{
  int j;

  for(j=0;j<McuOneWire_ROM_CODE_SIZE;j++) {
    McuUtility_strcatNum8Hex(buf, bufSize, romCode[j]);
    if(j<McuOneWire_ROM_CODE_SIZE-1) {
      McuUtility_chcat(buf, bufSize, '-');
    }
  }
  return ERR_OK;
}

/*
** ===================================================================
**     Method      :  McuOneWire_ResetSearch (component OneWire)
**     Description :
**         Reset the search state
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void McuOneWire_ResetSearch(void)
{
  /* reset the search state */
  int i;

  LastDiscrepancy = 0;
  LastDeviceFlag = FALSE;
  LastFamilyDiscrepancy = 0;
  for(i = 7; ; i--) {
    ROM_NO[i] = 0;
    if (i==0) {
      break;
    }
  }
}

/*
** ===================================================================
**     Method      :  McuOneWire_TargetSearch (component OneWire)
**     Description :
**         
**     Parameters  :
**         NAME            - DESCRIPTION
**         familyCode      - family code to restrict
**                           search for
**     Returns     : Nothing
** ===================================================================
*/
void McuOneWire_TargetSearch(uint8_t familyCode)
{
  /* set the search state to find SearchFamily type devices */
  int i;

  ROM_NO[0] = familyCode;
  for (i = 1; i < 8; i++) {
    ROM_NO[i] = 0;
  }
  LastDiscrepancy = 64;
  LastFamilyDiscrepancy = 0;
  LastDeviceFlag = FALSE;
}

/*
** ===================================================================
**     Method      :  McuOneWire_Search (component OneWire)
**     Description :
**         
**     Parameters  :
**         NAME            - DESCRIPTION
**       * newAddr         - Pointer to 8 bytes of data where
**                           to store the new address
**         search_mode     - 
**     Returns     :
**         ---             - TRUE if new device has been found, FALSE
**                           otherwise.
** ===================================================================
*/
bool McuOneWire_Search(uint8_t *newAddr, bool search_mode)
{
// Version from https://raw.githubusercontent.com/PaulStoffregen/OneWire/master/OneWire.cpp
//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
  uint8_t id_bit_number;
  uint8_t last_zero, rom_byte_number, search_result;
  uint8_t id_bit, cmp_id_bit;
  unsigned char rom_byte_mask, search_direction;
  uint8_t res;

  /* initialize for search */
  id_bit_number = 1;
  last_zero = 0;
  rom_byte_number = 0;
  rom_byte_mask = 1;
  search_result = 0;

  /* if the last call was not the last one */
  if (!LastDeviceFlag) {
    /* 1-Wire reset */
    res = McuOneWire_SendReset();
    if (res!=ERR_OK) {
      /* reset the search */
      LastDiscrepancy = 0;
      LastDeviceFlag = FALSE;
      LastFamilyDiscrepancy = 0;
      return FALSE;
    }
    /* issue the search command */
    if (search_mode) {
      McuOneWire_SendByte(RC_SEARCH);   /* NORMAL SEARCH */
    } else {
      McuOneWire_SendByte(RC_SEARCH_COND);   /* CONDITIONAL SEARCH */
    }
    /* loop to do the search */
    do  {
      /* read a bit and its complement */
      id_bit = read_bit();
      cmp_id_bit = read_bit();

      /* check for no devices on 1-wire */
      if ((id_bit==1) && (cmp_id_bit==1)) {
        break;
      } else {
        /* all devices coupled have 0 or 1 */
        if (id_bit != cmp_id_bit) {
          search_direction = id_bit;  // bit write value for search
        } else {
          /* if this discrepancy if before the Last Discrepancy */
          /* on a previous next then pick the same as last time */
          if (id_bit_number < LastDiscrepancy) {
            search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
          } else {
            /* if equal to last pick 1, if not then pick 0 */
            search_direction = (id_bit_number == LastDiscrepancy);
          }
          /* if 0 was picked then record its position in LastZero */
          if (search_direction == 0) {
            last_zero = id_bit_number;
            /* check for Last discrepancy in family */
            if (last_zero < 9)
               LastFamilyDiscrepancy = last_zero;
            }
          }

          /* set or clear the bit in the ROM byte rom_byte_number */
          /* with mask rom_byte_mask */
          if (search_direction == 1) {
            ROM_NO[rom_byte_number] |= rom_byte_mask;
          } else {
            ROM_NO[rom_byte_number] &= ~rom_byte_mask;
          }
          /* serial number search direction write bit */
          write_bit(search_direction);

          /* increment the byte counter id_bit_number */
          /* and shift the mask rom_byte_mask */
          id_bit_number++;
          rom_byte_mask <<= 1;

          /* if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask */
          if (rom_byte_mask == 0) {
            rom_byte_number++;
            rom_byte_mask = 1;
          }
       }
    }
    while(rom_byte_number < 8);  /* loop until through all ROM bytes 0-7 */
    /* if the search was successful then */
    if (!(id_bit_number < 65)) {
      /* search successful so set LastDiscrepancy,LastDeviceFlag,search_result */
      LastDiscrepancy = last_zero;

      /* check for last device */
      if (LastDiscrepancy == 0) {
        LastDeviceFlag = TRUE;
      }
      search_result = TRUE;
    }
  }
  /* if no device found then reset counters so next 'search' will be like a first */
  if (!search_result || !ROM_NO[0])  {
    LastDiscrepancy = 0;
    LastDeviceFlag = FALSE;
    LastFamilyDiscrepancy = 0;
    search_result = FALSE;
  } else {
    for (int i = 0; i < 8; i++) {
      newAddr[i] = ROM_NO[i];
    }
  }
  return search_result;
}

/* END McuOneWire. */

/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.5 [05.21]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
