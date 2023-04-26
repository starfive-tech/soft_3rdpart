/**
  ******************************************************************************
  * @file  ISP_mod_oecfhm_test.h
  * @author  StarFive Isp Team
  * @version  V1.0
  * @date  12/14/2019
  * @brief
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STARFIVE SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * Copyright (C)  2019 - 2022 StarFive Technology Co., Ltd.
  */
#ifndef __ISP_MOD_OECFHM_TEST_H__
#define __ISP_MOD_OECFHM_TEST_H__


#ifdef __cplusplus
extern "C" {
#endif


#include "stf_include.h"


/* ISPC OECFHM driver test structure */


/* ISPC OECFHM driver test variables */


/* ISPC OECFHM driver test interface */
//-----------------------------------------------------------------------------
extern
STF_RESULT Test_OECFHM_SetReg(
    STF_VOID
    );

#if defined(EXTEND_IOCTL_COMMAND_OECFHM)
//-----------------------------------------------------------------------------
extern
STF_RESULT Test_OECFHM_SetInput(
    STF_VOID
    );

extern
STF_RESULT Test_OECFHM_SetSlope(
    STF_VOID
    );

#endif //#if defined(EXTEND_IOCTL_COMMAND_OECFHM)
//-----------------------------------------------------------------------------
extern
STF_RESULT ISP_Mod_OecfHm_Test(
    STF_VOID
    );

//-----------------------------------------------------------------------------


#ifdef __cplusplus
}
#endif


#endif //__ISP_MOD_OECFHM_TEST_H__

