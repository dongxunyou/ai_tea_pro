/* 新建文件：ExtMemLoader/X-CUBE-AI/App/network_compat.h
 *
 * 作用：network.c 由旧版 ATONN 生成器生成，使用了带 __LL_ 前缀的旧宏名，
 * 而当前运行库（ll_aton）里这些宏改了名字。本文件把旧名映射到新名。
 *
 * 已通过 findstr 确认运行库中真实定义：
 *   ll_aton_platform.h:383  #define ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR(address) (address)
 *   ll_aton_osal.h:298      #define LL_ATON_OSAL_LOCK_MCU_CACHE()
 * 前者展开为常量 (address)，所以可安全用于静态初始化器。
 */
#ifndef NETWORK_COMPAT_H
#define NETWORK_COMPAT_H

/* 必须显式包含定义这些宏的头文件 */
#include "ll_aton_platform.h"   /* 定义 ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR */
#include "ll_aton_osal.h"       /* 定义 LL_ATON_OSAL_LOCK/UNLOCK_MCU_CACHE  */
#include "ll_aton_lib.h"
#include "ll_aton.h"

/* ---- 旧宏名 -> 新宏名 映射 ---- */

/* 地址转换：旧名带 __LL_ 前缀，新名不带。新宏展开为 (address)，是编译期常量 */
#ifndef __LL_ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR
#define __LL_ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR(addr)  ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR(addr)
#endif

/* MCU 缓存锁：旧名 -> OSAL 版（当前配置下为空宏） */
#ifndef LL_ATON_LOCK_MCU_CACHE
#define LL_ATON_LOCK_MCU_CACHE()    LL_ATON_OSAL_LOCK_MCU_CACHE()
#endif

#ifndef LL_ATON_UNLOCK_MCU_CACHE
#define LL_ATON_UNLOCK_MCU_CACHE()  LL_ATON_OSAL_UNLOCK_MCU_CACHE()
#endif

#endif /* NETWORK_COMPAT_H */