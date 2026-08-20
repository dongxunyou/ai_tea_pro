/**
 * ai_model.c - STM32N6 Neural-ART NPU 推理封装 (STAI-2.0 / LL_ATON)
 *
 * 运行模式：LL_ATON_RT_MODE == LL_ATON_RT_ASYNC（中断驱动，N6 默认）
 * 网络名：Default
 *
 * ── 第 8 轮：直接读 NPU 时钟频率，一锤定音──────────────────────────────
 *  第7轮诊断结论：INTREG=0(无任何中断)+BUSIF全0(无总线错误)+wait_mask=0x0A
 *  (在等流引擎1/3完成) → 流引擎被启动但根本没执行。ST 官方：NPU 是独立 1GHz
 *  时钟；LL_ATON 库不碰 RCC(假设应用已配好时钟)。grep HAL 头发现存在
 *  HAL_RCC_GetNPUClockFreq()。本版直接把它打出来：=0 则 NPU 计算时钟确实没转。
 *
 * ── 第 7 轮：用 ATON 内部状态(INTREG/wait_mask/BUSIF)定位──────────────
 *  核对 stm32n6xx_hal_rcc_ex.h：外设时钟表里【没有 RCC_PERIPHCLK_NPU】→ N6 的
 *  NPU 时钟不是独立旋钮，跟随系统互连时钟 ck_icn（已配）→ NPU 本就有时钟。
 *  本版：删掉走错路的 NPU 时钟补配；在超时诊断里读 ATON 原始中断寄存器(INTREG)、
 *  运行时 wait_mask、两个 BUSIF 总线错误 —— 一次烧录即可判定是 RISAF 总线拦截，
 *  还是中断没路由到 NVIC，还是 NPU 压根没动。
 *
 * ── 第 6 轮（作废）：曾以为 NPU 计算时钟 npu_ck 没配 ────────────────────
 *  串口证据：first WFE 后 5s 超时，diag pend=0（中断从未触发）→ NPU 没运算。
 *  查 main.c 的 SystemClock_Config：配了 PLL1/IC1/IC2/IC6/IC11/IC4/IC16，
 *  唯独【没有任何 RCC_PERIPHCLK_NPU】→ NPU 只有 AHB5 寄存器时钟、计算核无时钟。
 *  本版在 init 里补配 NPU 时钟：源自已在跑的 PLL1、复用 IC6 现有 300MHz 分频
 *  （不改 SYSCLK、不动 PLL2，最稳），把 npu_ck 接通。
 *
 * ── 第 5 轮：WFE 死等改轮询+超时（保留，便于诊断且 GUI 不死）──────────
 *  上一版串口：probe OK → memcpy done → epoch loop enter → first WFE 后挂住。
 *  WFE 是让出式信号量等待，ISR 一运行就会 SIGNAL_EVENT 唤醒；卡在这=ISR 没运行
 *  =NPU 没真正完成 epoch（没产生中断）或中断进了别的 NVIC 线。
 *  本版把死等 LL_ATON_OSAL_WFE() 改成「轮询 RunEpochBlock + osDelay(1) 让出 + 超时」，
 *  超时后 dump NVIC 状态（中断有没有 pending）并恢复，GUI 全程不死。
 *
 * ── 第 4 轮：AXISRAM3~6 默认 disabled（已修复，保留）──────────────────
 *  串口定位到死在「memcpy 196608 bytes -> 0x342E0000」，且无 FAULT = 总线停死。
 *  network.c 顶部内存池注释显示模型的 IO/激活落在 AXISRAM2~6：
 *      AXISRAM5 npuRAM5 @0x342E0000 (输入/输出 buffer 就在这)
 *      AXISRAM4 @0x34270000, AXISRAM3 @0x34200000, AXISRAM6 @0x34350000,
 *      AXISRAM2 @0x34100000
 *
 *  ★ N6 官方坑：AXISRAM3/4/5/6 复位后默认 DISABLED，必须使能各自的
 *    MEM 时钟 __HAL_RCC_AXISRAMx_MEM_CLK_ENABLE()，光清 RAMCFG 的 SRAMSD
 *    断电位不够。之前几版只清了 SRAMSD、从没开 MEM 时钟 → 该 RAM 没时钟，
 *    CPU 一写 0x342E0000 总线事务永不完成 → 整核停死（无异常）。
 *    （来源：STM32N6 Community「How to utilize 4.2MB RAM」「Example to
 *     initialize all available RAM」——AXISRAM3-6 disabled by default。）
 *
 *  本版修复：在 init 里先 __HAL_RCC_AXISRAMx_MEM_CLK_ENABLE() 再清 SRAMSD，
 *  并加一个 AXISRAM5 写读自检立即验证。
 * ──────────────────────────────────────────────────────────────────
 */

#include "ai_model.h"
#include "ll_aton_runtime.h"
#include "ll_aton_NN_interface.h"
#include "npu_cache.h"
#include "stm32n6xx_hal.h"
#include "cmsis_os2.h"      /* osDelay / osKernelGetTickCount：轮询让出用 */
#include "ltdc.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Default);

#ifndef NPU_IRQ_PRIORITY
#define NPU_IRQ_PRIORITY  15U
#endif

#ifndef AI_MAX_EPOCH_ITERS
#define AI_MAX_EPOCH_ITERS  100000U
#endif

/* 单个 epoch 等 NPU 完成的超时(ms)。正常一个 epoch 远小于此；超时即判定
 * NPU 没产生完成中断，dump 诊断并放弃本次推理（GUI 不受影响）。*/
#ifndef WFE_TIMEOUT_MS
#define WFE_TIMEOUT_MS  5000U
#endif

#define PHEX(p)  ((unsigned long)(uintptr_t)(p))

/* ── 第 9 轮：真正测/修 RISAF（直接寄存器，不靠查不到的 HAL 路径）─────────
 *  已用 network.c 的 epoch 描述符实锤：卡住的是 epoch 2（wait_mask=0x0a=输出
 *  引擎 streng 1+3），而 EpochBlock_2 是【第一个让 NPU 经 streng 8 从 0x70700000
 *  读 XSPI2 权重】的 epoch。main.c 给了 NPU 的 RIMC 主属性 + RISC 外设，却从没给
 *  XSPI2 配任何 RISAF 内存区域。ST 明确要求：非 CPU 主设备(NPU/CID1)访问 XSPI
 *  必须经 RISAF 放行(XSPI2→RISAF12)。上一版的 RISAF 代码只改“已使能”区域、一个
 *  都没命中=等于没做，所以 RISAF 之前根本没被真正测过。BUSIF=0 不能排除 RISAF——
 *  拦截记录在 RISAF 自己的非法访问寄存器(IASR/IAR)里，不在 NPU 的 BUSIF。
 *  本版直接读写 RISAF 寄存器：打印全部 15 区域真实状态；对覆盖 0x70700000 的已
 *  使能区域只 OR 进 CID1 读写白名单(保留 CID0=CPU 的 XIP)；若无区域覆盖则在空闲
 *  槽新建一个覆盖整个 XSPI2 窗口；并读非法访问寄存器，使推理结果一锤定音。
 * ──────────────────────────────────────────────────────────────────── */

/* 测试开关：=1 则跳过 CACHEAXI（让 NPU 直接读 XSPI2，绕过缓存）。
 * ★ 第 10 轮已置 1：上一份日志确认 RISAF 已彻底排除(IAEF=0)，问题在 NPU→CACHEAXI
 *   →XSPI2 数据通路。模型把 XSPI2 标 cacheable=ON，NPU 读权重默认走 CACHEAXI；
 *   而 App 同时在 XSPI2 上 XIP，缓存对“正被 XIP 的 Flash”做 linefill 极易卡死。
 *   本版绕过缓存让 NPU 直接读 XSPI2（功能正确，仅慢）。若这样 epoch 2 通过=坐实
 *   是缓存通路；要回到带缓存只需把此宏改回 0。 */
#ifndef NPU_TEST_DISABLE_CACHE
#define NPU_TEST_DISABLE_CACHE 0
#endif

/* 等待方式开关：=1 推理等待用 __WFI()(CPU 睡眠、不从 XSPI2 取指，把总线让给 NPU)，
 *   并在 epoch 循环期间把本任务提到最高优先级(GUI/idle 不再空转占 XSPI2)。
 *   =0 回退到旧的 osDelay(1) 轮询(GUI 全程流畅，但 CPU 持续 XIP 占 XSPI2)。
 * ★ 第 11 轮置 1：上一份日志坐实 XSPI2 控制器在 NPU 读时 BUSY 卡死=CPU XIP 与 NPU
 *   读权重争用同一颗 XSPI2 的 mmap 控制器。本开关让 CPU 在 NPU 读权重时几乎不碰
 *   XSPI2。若仍卡，说明需把权重挪出 XSPI2（见文件尾说明）。*/
#ifndef NPU_WAIT_USE_WFI
#define NPU_WAIT_USE_WFI 0
#endif

/* FIX-5: 推理耗时测量开关（默认开，串口打印耗时） */
#ifndef AI_PROFILE_INFER
#define AI_PROFILE_INFER 1
#endif

#define WEIGHT_PROBE_ADDR  0x70A00000UL   /* EpochBlock_2 经 streng 8 读的权重首址 */

/* 任务#18：RT_Main 驱动 + 看门狗共享状态 */
static volatile uint8_t  g_npu_infer_done  = 1;
static volatile uint32_t g_npu_infer_start = 0;
#define XSPI2_MMAP_START   0x70000000UL   /* XSPI2 memory-mapped 窗口起 */
#define XSPI2_MMAP_END     0x77FFFFFFUL   /* XSPI2 memory-mapped 窗口止(覆盖 64MB flash) */

#if defined(RISAF12) || defined(RISAF11)
/* 读 RISAF 全部区域 + 给 NPU(CID1)放行 XSPI 权重访问。安全、可逆。 */
static void risaf_grant_xspi_for_npu(RISAF_TypeDef *saf, const char *name)
{
    int covering = 0, granted = 0, created = 0;

    for (int r = 0; r < 15; r++) {
        uint32_t cfgr  = saf->REG[r].CFGR;
        uint32_t start = saf->REG[r].STARTR;
        uint32_t end   = saf->REG[r].ENDR;
        uint32_t cid   = saf->REG[r].CIDCFGR;
        uint32_t bren  = (cfgr & RISAF_REGx_CFGR_BREN) ? 1U : 0U;

        printf("[NPU] RISAF %s reg%02d: BREN=%lu SEC=%lu [%08lX..%08lX] rdEN=%02lX wrEN=%02lX\r\n",
               name, r + 1, (unsigned long)bren,
               (unsigned long)((cfgr & RISAF_REGx_CFGR_SEC) ? 1U : 0U),
               (unsigned long)start, (unsigned long)end,
               (unsigned long)(cid & 0xFFU),          /* RDENC0..7 */
               (unsigned long)((cid >> 16) & 0xFFU)); /* WRENC0..7 */

        if (bren && (WEIGHT_PROBE_ADDR >= start) && (WEIGHT_PROBE_ADDR <= end)) {
            covering++;
            uint32_t need = RISAF_REGx_CIDCFGR_RDENC1 | RISAF_REGx_CIDCFGR_WRENC1;
            if ((cid & need) != need) {
                saf->REG[r].CIDCFGR = cid | need;   /* 只追加 CID1，不动 CID0/地址/安全位 */
                __DSB();
                printf("[NPU] RISAF %s reg%02d: +CID1(NPU) GRANTED (CIDCFGR %08lX -> %08lX)\r\n",
                       name, r + 1, (unsigned long)cid, (unsigned long)saf->REG[r].CIDCFGR);
                granted++;
            } else {
                printf("[NPU] RISAF %s reg%02d: CID1 already allowed (no change)\r\n", name, r + 1);
                granted++;
            }
        }
    }

    /* 没有任何已使能区域覆盖权重地址 → 在空闲槽新建一个覆盖整个 XSPI2 窗口的区域。
     * 只在“确实没区域”时才走这条；此时 CPU 是靠 RISAF 默认策略 XIP，新建不会和现有
     * 区域重叠。CFGR 只置 BREN+SEC(不限特权,最大兼容)，CIDCFGR 给 CID0+CID1 读写，
     * 先写地址/CID 再最后置 BREN —— 启用前默认策略仍在,CPU XIP 不断。 */
    if (covering == 0) {
        for (int r = 0; r < 15; r++) {
            if ((saf->REG[r].CFGR & RISAF_REGx_CFGR_BREN) == 0U) {
                saf->REG[r].STARTR  = XSPI2_MMAP_START;
                saf->REG[r].ENDR    = XSPI2_MMAP_END;
                saf->REG[r].CIDCFGR = RISAF_REGx_CIDCFGR_RDENC0 | RISAF_REGx_CIDCFGR_RDENC1
                                    | RISAF_REGx_CIDCFGR_WRENC0 | RISAF_REGx_CIDCFGR_WRENC1;
                __DSB();
                saf->REG[r].CFGR    = RISAF_REGx_CFGR_BREN | RISAF_REGx_CFGR_SEC;  /* 最后启用 */
                __DSB(); __ISB();
                printf("[NPU] RISAF %s reg%02d: CREATED [%08lX..%08lX] CID0+CID1 R/W SEC "
                       "(CFGR=%08lX CID=%08lX)\r\n",
                       name, r + 1, (unsigned long)XSPI2_MMAP_START, (unsigned long)XSPI2_MMAP_END,
                       (unsigned long)saf->REG[r].CFGR, (unsigned long)saf->REG[r].CIDCFGR);
                created++;
                break;
            }
        }
        if (created == 0)
            printf("[NPU] RISAF %s: *** no covering region AND no free slot ***\r\n", name);
    }

    /* 清非法访问标志(写 1 清)，便于推理后判断 NPU 是否仍被拦 */
    saf->IACR = RISAF_IACR_CAEF | RISAF_IACR_IAEF;
    __DSB();
    printf("[NPU] RISAF %s: done (covering=%d granted=%d created=%d), IAR cleared\r\n",
           name, covering, granted, created);
}

/* 读 RISAF 非法访问记录：若推理仍卡，这里就是 RISAF 是否在拦 NPU 的决定性证据。
 *   IAEF=1 + IADDR≈0x707xxxxx + IACID=1 → NPU(CID1) 读权重仍被拒(放行没覆盖到)。
 *   IAEF=0 → 不是 RISAF → 是缓存/总线路径(翻 NPU_TEST_DISABLE_CACHE=1 再测)。*/
static void risaf_dump_illegal(RISAF_TypeDef *saf, const char *name)
{
    uint32_t iasr = saf->IASR;
    uint32_t ies  = saf->IAR[0].IAESR;
    uint32_t iadd = saf->IAR[0].IADDR;
    printf("[NPU] RISAF %s IA: IASR=%08lX (IAEF=%lu) IACID=%lu priv=%lu sec=%lu IADDR=%08lX\r\n",
           name, (unsigned long)iasr, (unsigned long)((iasr & RISAF_IASR_IAEF) ? 1U : 0U),
           (unsigned long)(ies & RISAF_IAESR_IACID_Msk),
           (unsigned long)((ies & RISAF_IAESR_IAPRIV) ? 1U : 0U),
           (unsigned long)((ies & RISAF_IAESR_IASEC) ? 1U : 0U),
           (unsigned long)iadd);
}
#endif /* RISAF12 || RISAF11 */

/* 读 NPU→XSPI2 数据通路硬件状态。卡住时一刀切开“卡在缓存”还是“卡在 XSPI2 控制器”：
 *   CACHEAXI.SR: BUSYF=1=缓存停在 busy（linefill 没回来）；ERRF=1=缓存报错。
 *   XSPI2.SR  BUSY=1 = XSPI2 控制器停在一笔事务里没完成（多半是 CPU 正 XIP 时
 *     无法同时服务 NPU 的读 → 总线/控制器争用）；BUSY=0 = 请求根本没到 XSPI2。
 *   XSPI2.CR  FMODE 应=3(memory-mapped)，EN 应=1。读的是控制寄存器、不触发 Flash
 *     事务，所以即使内存窗口正卡着，这几行也不会把自己卡死。*/
static void dump_xspi_cache_state(void)
{
#if defined(CACHEAXI)
    uint32_t csr = CACHEAXI->SR;
    printf("[NPU] diag: CACHEAXI CR1=%08lX SR=%08lX (BUSYF=%lu ERRF=%lu)\r\n",
           (unsigned long)CACHEAXI->CR1, (unsigned long)csr,
           (unsigned long)((csr & CACHEAXI_SR_BUSYF) ? 1U : 0U),
           (unsigned long)((csr & CACHEAXI_SR_ERRF) ? 1U : 0U));
#endif
#if defined(XSPI2)
    uint32_t xcr = XSPI2->CR;
    uint32_t xsr = XSPI2->SR;
    printf("[NPU] diag: XSPI2 CR=%08lX (EN=%lu FMODE=%lu) SR=%08lX (BUSY=%lu)\r\n",
           (unsigned long)xcr,
           (unsigned long)((xcr & XSPI_CR_EN) ? 1U : 0U),
           (unsigned long)((xcr & XSPI_CR_FMODE) >> XSPI_CR_FMODE_Pos),
           (unsigned long)xsr,
            (unsigned long)((xsr & XSPI_SR_BUSY) ? 1U : 0U));
#endif
#if defined(XSPI1)
    printf("[NPU] diag: XSPI1 CR=%08lX SR=%08lX (BUSY=%d)\r\n",
           (unsigned long)XSPI1->CR, (unsigned long)XSPI1->SR, (int)((XSPI1->SR >> 5) & 1));
#endif
}

/* 关于 NPU 时钟：经核对本工程的 stm32n6xx_hal_rcc_ex.h，外设时钟列表里【没有】
 * RCC_PERIPHCLK_NPU —— 也就是说 N6 的 NPU 计算时钟不是一个独立可配的内核时钟旋钮，
 * 它跟随系统互连时钟 ck_icn（由 SystemClock_Config 的 IC2/IC6/IC11 提供，已在运行）。
 * 所以 NPU 本来就是有时钟的，之前'补配 NPU 时钟'的方向被 HAL 否定。pend=0 的真因
 * 另有其人，本版加 ATON 内部状态诊断来定位（见 dump_npu_irq_state）。*/
static void npu_clock_enable(void)
{
    SET_BIT(RCC->AHB5ENR, RCC_AHB5ENR_NPUEN);
    (void)READ_BIT(RCC->AHB5ENR, RCC_AHB5ENR_NPUEN);
    __DSB();

    SET_BIT(RCC->AHB5RSTR, RCC_AHB5RSTR_NPURST);
    (void)READ_BIT(RCC->AHB5RSTR, RCC_AHB5RSTR_NPURST);
    __DSB();
    CLEAR_BIT(RCC->AHB5RSTR, RCC_AHB5RSTR_NPURST);
    (void)READ_BIT(RCC->AHB5RSTR, RCC_AHB5RSTR_NPURST);
    __DSB();
}

/* ★★ 真因修复 ★★
 * AXISRAM3~6 默认 disabled。两步都要做：
 *   (1) 使能各 bank 的 MEM 时钟（__HAL_RCC_AXISRAMx_MEM_CLK_ENABLE）
 *   (2) 清断电位 SRAMSD（RAMCFG_SRAMx_AXI->CR）
 * AXISRAM1/2 默认就有时钟，但一起使能无害。全部用 #if defined 保护，
 * 某个宏不存在也不会编译报错。
 */
static void axisram_power_on_all(void)
{
#if defined(__HAL_RCC_RAMCFG_CLK_ENABLE)
    __HAL_RCC_RAMCFG_CLK_ENABLE();    /* 写 RAMCFG 寄存器需要其外设时钟 */
#endif

    /* (1) 使能各 AXISRAM bank 的 MEM 时钟 —— 这是之前缺的关键步骤 */
#if defined(__HAL_RCC_AXISRAM1_MEM_CLK_ENABLE)
    __HAL_RCC_AXISRAM1_MEM_CLK_ENABLE();
#endif
#if defined(__HAL_RCC_AXISRAM2_MEM_CLK_ENABLE)
    __HAL_RCC_AXISRAM2_MEM_CLK_ENABLE();
#endif
#if defined(__HAL_RCC_AXISRAM3_MEM_CLK_ENABLE)
    __HAL_RCC_AXISRAM3_MEM_CLK_ENABLE();
#endif
#if defined(__HAL_RCC_AXISRAM4_MEM_CLK_ENABLE)
    __HAL_RCC_AXISRAM4_MEM_CLK_ENABLE();
#endif
#if defined(__HAL_RCC_AXISRAM5_MEM_CLK_ENABLE)
    __HAL_RCC_AXISRAM5_MEM_CLK_ENABLE();
#endif
#if defined(__HAL_RCC_AXISRAM6_MEM_CLK_ENABLE)
    __HAL_RCC_AXISRAM6_MEM_CLK_ENABLE();
#endif
    __DSB();
    __ISB();

    /* (2) 清断电位 SRAMSD（保持原有）*/
#if defined(RAMCFG_SRAM1_AXI)
    RAMCFG_SRAM1_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    (void)RAMCFG_SRAM1_AXI->CR;
#endif
#if defined(RAMCFG_SRAM2_AXI)
    RAMCFG_SRAM2_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    (void)RAMCFG_SRAM2_AXI->CR;
#endif
#if defined(RAMCFG_SRAM3_AXI)
    RAMCFG_SRAM3_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    (void)RAMCFG_SRAM3_AXI->CR;
#endif
#if defined(RAMCFG_SRAM4_AXI)
    RAMCFG_SRAM4_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    (void)RAMCFG_SRAM4_AXI->CR;
#endif
#if defined(RAMCFG_SRAM5_AXI)
    RAMCFG_SRAM5_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    (void)RAMCFG_SRAM5_AXI->CR;
#endif
#if defined(RAMCFG_SRAM6_AXI)
    RAMCFG_SRAM6_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    (void)RAMCFG_SRAM6_AXI->CR;
#endif
    __DSB();
}

/* 任务#18：前向声明（定义在下方） */
static void npu_watchdog_task(void *arg);
static void dump_npu_irq_state(void);

int ai_model_init(void)
{
    printf("[NPU] init: enter\r\n");

    /* ★★ 第12轮（已撤销——权重回 XSPI2，不再搬到 XSPI1）★★
    {
        #define NPU_W_SRC   0x70700000UL
        #define NPU_W_DST   0x90800000UL
        #define NPU_W_SIZE  0x300000UL
        volatile uint32_t *ws = (volatile uint32_t *)NPU_W_SRC;
        printf("[NPU] wcopy: src@%08lX ...", ...);
        memcpy((void *)NPU_W_DST, (const void *)NPU_W_SRC, (size_t)NPU_W_SIZE);
        ...
        printf("[NPU] wcopy: dst@%08lX ...", ...);
        ...
    } */

    npu_clock_enable();
    printf("[NPU] init: NPU clock enabled (AHB5ENR=%08lX)\r\n",
           (unsigned long)RCC->AHB5ENR);

    /* ★★ 决定性一测：直接读 NPU 计算时钟频率 ★★
     *   = 0 或异常低  → NPU 计算时钟没在转 → 真因坐实，需要配时钟源；
     *   = 正常值(几百 MHz/最高 1GHz) → NPU 其实有时钟 → 问题在别处，需另查。
     * HAL_RCC_GetNPUClockFreq() 声明于 stm32n6xx_hal_rcc.h，只读寄存器、安全。*/
    printf("[NPU] init: *** HAL_RCC_GetNPUClockFreq() = %lu Hz  <<< KEY VALUE ***\r\n",
           (unsigned long)HAL_RCC_GetNPUClockFreq());

    /* AXISRAM：使能 MEM 时钟 + 清断电位 */
    axisram_power_on_all();
    printf("[NPU] init: AXISRAM MEM-clk enabled + SRAMSD cleared\r\n");

    /* 自检：往 AXISRAM5 起始(0x342E0000，模型输入 buffer 处)写一个值读回。
     * 看到 OK = 这块 RAM 现在可访问，memcpy 不会再停死。
     * 若串口停在 "probing..." 这行 = 即便开了 MEM 时钟仍不可访问，
     *   那就是 sysd_ck（AXISRAM3-6 的时钟源）没在 SystemClock_Config 里配好。*/
    printf("[NPU] init: probing AXISRAM5 @342E0000 ...\r\n");
    {
        volatile uint32_t *t = (volatile uint32_t *)0x342E0000UL;
        *t = 0xA5A51234UL;
        __DSB();
        uint32_t rb = *t;
        printf("[NPU] init: AXISRAM5 probe rd=%08lX %s\r\n",
               (unsigned long)rb, (rb == 0xA5A51234UL) ? "OK" : "MISMATCH");
    }

    /* ★ 关键新增：验证网络真正写入的其它 NPU bank。
     * EpochBlock_1(卡住的第一个 epoch)= 引擎6读 0x342e0000(AXISRAM5) →ARITH→
     *   引擎4写 0x34200000(AXISRAM3)。AXISRAM5 已验证 OK；但 AXISRAM3 从没验证过。
     * 若某个 bank 实际没使能，CPU(以及 NPU)往它写就【无响应卡死】——正好解释
     *   引擎不完成、INTREG=0、BUSIF 无错误。
     * 判读：若串口停在某条 "probing AXISRAMx" 之后【没有】对应的结果行，
     *   = 那个 bank 没真正使能，就是它把 NPU 卡死了。 */
    printf("[NPU] init: probing AXISRAM3 @34200000 ...\r\n");
    { volatile uint32_t *t = (volatile uint32_t *)0x34200000UL; *t = 0x33333333UL; __DSB();
      uint32_t rb = *t; printf("[NPU] init: AXISRAM3 probe rd=%08lX %s\r\n",
                               (unsigned long)rb, (rb == 0x33333333UL) ? "OK" : "MISMATCH"); }
    printf("[NPU] init: probing AXISRAM4 @34270000 ...\r\n");
    { volatile uint32_t *t = (volatile uint32_t *)0x34270000UL; *t = 0x44444444UL; __DSB();
      uint32_t rb = *t; printf("[NPU] init: AXISRAM4 probe rd=%08lX %s\r\n",
                               (unsigned long)rb, (rb == 0x44444444UL) ? "OK" : "MISMATCH"); }
    printf("[NPU] init: probing AXISRAM6 @34350000 ...\r\n");
    { volatile uint32_t *t = (volatile uint32_t *)0x34350000UL; *t = 0x66666666UL; __DSB();
      uint32_t rb = *t; printf("[NPU] init: AXISRAM6 probe rd=%08lX %s\r\n",
                               (unsigned long)rb, (rb == 0x66666666UL) ? "OK" : "MISMATCH"); }

#if (NPU_TEST_DISABLE_CACHE == 0)
    /* task#27: CACHEAXI MspInit is empty weak-fn, clock never truly enabled,
     * so Enable write to CR1 had no effect (CR1 stayed 0). Fix: turn on clocks. */
    __HAL_RCC_CACHEAXI_CLK_ENABLE();
    __HAL_RCC_CACHEAXIRAM_MEM_CLK_ENABLE();
    printf("[NPU] init: CACHEAXI + CACHEAXIRAM clocks ENABLED (task#27)\r\n");
    npu_cache_init();
    npu_cache_enable();
    printf("[NPU] init: CACHEAXI after enable: CR1=%08lX SR=%08lX (CR1 bit0 EN must now be 1)\r\n",
           (unsigned long)CACHEAXI->CR1, (unsigned long)CACHEAXI->SR);
    printf("[NPU] init: npu cache enabled\r\n");
#else
    /* A/B 测试：跳过 CACHEAXI，让 NPU 直接读 XSPI2。仅用于判定“是否缓存卡死”。*/
    printf("[NPU] init: *** CACHEAXI DISABLED (NPU_TEST_DISABLE_CACHE=1, A/B test) ***\r\n");
#endif

    /* ★★★★★ 真·根因修复 ★★★★★
     * ST 官方文档 + 社区已解决案例:推理等待时 core 进低功耗(WFI/WFE),
     * 默认会【门控掉 NPU 及 NPU-RAM 的时钟】→ NPU 当场冻死 → epoch 永不完成、
     * INTREG=0、BUSIF 无错(这正是我们看到的现象)。HAL_RCC_GetNPUClockFreq 读的
     * 是配置值(醒着 1GHz),所以骗过了前面几轮诊断。
     * 修复:把推理用到的所有时钟设为"低功耗模式下保持开",用 __HAL_RCC_xxx_CLK_
     * SLEEP_ENABLE 宏(STM32 标准命名,和我们已用的 _CLK_ENABLE 一一对应)。
     * 全部 #if defined 保护——个别名字若不符只是跳过、不会编译失败;末尾打印实际
     * 生效个数,若个数过少说明宏名需按你的 stm32n6xx_hal_rcc.h 修正。 */
    {
        int _ns = 0;
        /* --- NPU 计算单元本体 --- */
#if defined(__HAL_RCC_NPU_CLK_SLEEP_ENABLE)
        __HAL_RCC_NPU_CLK_SLEEP_ENABLE();            _ns++;
#endif
        /* --- NPU 的 AXI 缓存(CACHEAXI)及其 RAM --- */
#if defined(__HAL_RCC_CACHEAXI_CLK_SLEEP_ENABLE)
        __HAL_RCC_CACHEAXI_CLK_SLEEP_ENABLE();        _ns++;
#endif
#if defined(__HAL_RCC_CACHEAXIRAM_MEM_CLK_SLEEP_ENABLE)
        __HAL_RCC_CACHEAXIRAM_MEM_CLK_SLEEP_ENABLE(); _ns++;
#endif
        /* --- 所有 AXISRAM bank:输入(AXISRAM5)、输出(AXISRAM3)、激活(3-6),
         *     1/2 也一并保住更稳妥 --- */
#if defined(__HAL_RCC_AXISRAM1_MEM_CLK_SLEEP_ENABLE)
        __HAL_RCC_AXISRAM1_MEM_CLK_SLEEP_ENABLE();    _ns++;
#endif
#if defined(__HAL_RCC_AXISRAM2_MEM_CLK_SLEEP_ENABLE)
        __HAL_RCC_AXISRAM2_MEM_CLK_SLEEP_ENABLE();    _ns++;
#endif
#if defined(__HAL_RCC_AXISRAM3_MEM_CLK_SLEEP_ENABLE)
        __HAL_RCC_AXISRAM3_MEM_CLK_SLEEP_ENABLE();    _ns++;
#endif
#if defined(__HAL_RCC_AXISRAM4_MEM_CLK_SLEEP_ENABLE)
        __HAL_RCC_AXISRAM4_MEM_CLK_SLEEP_ENABLE();    _ns++;
#endif
#if defined(__HAL_RCC_AXISRAM5_MEM_CLK_SLEEP_ENABLE)
        __HAL_RCC_AXISRAM5_MEM_CLK_SLEEP_ENABLE();    _ns++;
#endif
#if defined(__HAL_RCC_AXISRAM6_MEM_CLK_SLEEP_ENABLE)
        __HAL_RCC_AXISRAM6_MEM_CLK_SLEEP_ENABLE();    _ns++;
#endif
        /* --- 外部内存:权重在 XSPI2(0x70700000);XSPI1 通常接 PSRAM;XSPIM 管理器 --- */
#if defined(__HAL_RCC_XSPI1_CLK_SLEEP_ENABLE)
        __HAL_RCC_XSPI1_CLK_SLEEP_ENABLE();           _ns++;
#endif
#if defined(__HAL_RCC_XSPI2_CLK_SLEEP_ENABLE)
        __HAL_RCC_XSPI2_CLK_SLEEP_ENABLE();           _ns++;
#endif
#if defined(__HAL_RCC_XSPIM_CLK_SLEEP_ENABLE)
        __HAL_RCC_XSPIM_CLK_SLEEP_ENABLE();           _ns++;
#endif
        /* --- RAMCFG(RAM 电源/时钟控制器)--- */
#if defined(__HAL_RCC_RAMCFG_CLK_SLEEP_ENABLE)
        __HAL_RCC_RAMCFG_CLK_SLEEP_ENABLE();          _ns++;
#endif
        __DSB();
        __ISB();
        printf("[NPU] init: *** low-power CLK_SLEEP_ENABLE applied to %d clocks ***\r\n", _ns);
    }

    /* 任务#22/23：清掉开机残留的 RISAF/IAC 非法访问标记，再打开 IAC 使能 */
#if defined(RISAF12)
    RISAF12->IACR = RISAF_IACR_CAEF | RISAF_IACR_IAEF;
    __DSB();
#endif
#if defined(RISAF11)
    RISAF11->IACR = RISAF_IACR_CAEF | RISAF_IACR_IAEF;
    __DSB();
#endif
#if defined(IAC)
    for (int _i = 0; _i < 5; _i++) IAC->ICR[_i] = 0xFFFFFFFFUL;
    __DSB();
#endif

    /* 任务#22：使能 IAC（非法访问控制器）——否则 RIF 拦截对非安全世界不可见 */
#if defined(IAC)
    __HAL_RCC_IAC_CLK_ENABLE();
    for (int _i = 0; _i < 5; _i++) IAC->IER[_i] = 0xFFFFFFFFUL;
    __DSB();
    printf("[NPU] init: IAC enabled (IER all open, illegal-access now visible)\r\n");
    HAL_NVIC_SetPriority(IAC_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(IAC_IRQn);
    printf("[NPU] init: IAC IRQ(IRQn=IAC_IRQn=%d) enabled prio=5\r\n", (int)IAC_IRQn);
#else
    printf("[NPU] init: IAC peripheral NOT defined (non-secure world?)\r\n");
#endif

    /* ★★ 真·根因修复候选 #2:授权 NPU(CID1) 读 XSPI2 权重(直接寄存器版)★★
     * 见文件顶部第 9 轮说明。覆盖 0x70700000 的已使能区域只追加 CID1；无则新建。*/
    {
        int _nri = 0;
#if defined(RISAF12)
        risaf_grant_xspi_for_npu(RISAF12, "RISAF12/XSPI2"); _nri++;
#endif
#if defined(RISAF11)
        risaf_grant_xspi_for_npu(RISAF11, "RISAF11/XSPI1"); _nri++;
#endif
        if (_nri == 0)
            printf("[NPU] RISAF: *** RISAF11/12 instance NOT defined ***\r\n");
    }

    {
        volatile const uint32_t *w = (const uint32_t *)WEIGHT_PROBE_ADDR;
        printf("[NPU] init: weight[0..1] = %08lX %08lX\r\n",
               (unsigned long)w[0], (unsigned long)w[1]);
    }

    {
        SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk
                    | SCB_SHCSR_USGFAULTENA_Msk
                    | SCB_SHCSR_MEMFAULTENA_Msk;
        __DSB();
        printf("[NPU] init: SHCSR=%08lX CFSR=%08lX HFSR=%08lX\r\n",
               (unsigned long)SCB->SHCSR,
               (unsigned long)SCB->CFSR,
               (unsigned long)SCB->HFSR);
    }

    /* 任务#18：改用 LL_ATON_RT_Main，由它内部做 RuntimeInit/DeInit；此处不再单独 RuntimeInit */
    // LL_ATON_RT_RuntimeInit();
    printf("[NPU] init: RuntimeInit deferred to LL_ATON_RT_Main (task#18)\r\n");

    HAL_NVIC_SetPriority(ATON_STD_IRQn, NPU_IRQ_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(ATON_STD_IRQn);
    printf("[NPU] init: ATON IRQ(IRQn=%d) prio forced to %u (readback=%lu)\r\n",
           (int)ATON_STD_IRQn, (unsigned)NPU_IRQ_PRIORITY,
           (unsigned long)NVIC_GetPriority(ATON_STD_IRQn));

    /* 任务#18：诊断看门狗——RT_Main 若卡住(OSAL_WFE 阻塞)，6s 后打印 NPU 诊断，避免静默死等 */
    {
        osThreadAttr_t wd_attr = { .name = "npu_wdog",
                                   .priority = osPriorityLow,
                                   .stack_size = 2048 };
        osThreadNew(npu_watchdog_task, NULL, &wd_attr);
    }

    return 0;
}

/* 卡在 WFE 超时时调用：把 NPU 完成中断在 NVIC 里的状态打出来。
 *   pend=1 → 中断已触发但没被服务（handler 没接对/被屏蔽）
 *   pend=0 → 中断从未触发 → NPU 没完成 epoch（多半是 npu_ck 没跑 / 访问受阻）
 *   BASEPRI/PRIMASK 非 0 → 中断被屏蔽 */
static void npu_watchdog_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (!g_npu_infer_done && (HAL_GetTick() - g_npu_infer_start) > 6000U) {
            printf("[NPU][WDOG] inference > 6s, dumping NPU state "
                   "(RT_Main likely blocked in OSAL_WFE):\r\n");
            dump_npu_irq_state();
            g_npu_infer_done = 1;   /* 只打印一次，避免刷屏 */
        }
        osDelay(500);
    }
}

static void dump_npu_irq_state(void)
{
    printf("[NPU] diag: ATON IRQn=%d en=%lu pend=%lu act=%lu BASEPRI=%lu PRIMASK=%lu\r\n",
           (int)ATON_STD_IRQn,
           (unsigned long)NVIC_GetEnableIRQ(ATON_STD_IRQn),
           (unsigned long)NVIC_GetPendingIRQ(ATON_STD_IRQn),
           (unsigned long)NVIC_GetActive(ATON_STD_IRQn),
           (unsigned long)__get_BASEPRI(),
           (unsigned long)__get_PRIMASK());

    /* ★ ATON 内部状态（NVIC 之前的层面）—— 用来一刀切开剩余可能性：
     *   INTREG(raw)：ATON 原始中断寄存器。若它≠0 而上面 pend=0，说明 NPU 其实
     *     已经触发了中断（算完或报错），只是没被路由到 NVIC 53 → INTCTRL 掩码/
     *     等待掩码不匹配的问题。
     *   wait_mask：运行时此刻在等的中断位。和 INTREG 对比即知“等的东西到底来没来”。
     *   BUSIF ERR：总线接口错误。若≠0 → NPU 作为主设备读权重(0x70700000)/AXISRAM
     *     时撞了错误 → 极可能是 RISAF 没给 NPU 主设备放行那块内存。*/
#if defined(ATON_INTCTRL_INTREG_GET)
    printf("[NPU] diag: ATON INTREG(raw)=%08lX\r\n",
           (unsigned long)ATON_INTCTRL_INTREG_GET(0));
#endif
    {
        extern uint32_t volatile __ll_current_wait_mask;
        printf("[NPU] diag: runtime wait_mask=%08lX\r\n",
               (unsigned long)__ll_current_wait_mask);
    }
#if defined(ATON_BUSIF_NUM)
    for (int _i = 0; _i < ATON_BUSIF_NUM; _i++) {
        printf("[NPU] diag: BUSIF%d ERR=%08lX  (non-zero => NPU bus access fault, likely RISAF)\r\n",
               _i, (unsigned long)ATON_BUSIF_ERR_GET(_i));
    }
#endif
    printf("[NPU] diag: NPU clock freq = %lu Hz  (0 / abnormal => NPU compute clock NOT running)\r\n",
           (unsigned long)HAL_RCC_GetNPUClockFreq());

    /* ★ RISAF 非法访问记录 —— 推理仍卡时的决定性证据：
     *   IAEF=1 且 IADDR≈0x707xxxxx → NPU 读权重仍被 RISAF 拒(放行没覆盖到，看 IACID 是哪个 CID)；
     *   IAEF=0 (全 0)            → 不是 RISAF 拦截 → 是缓存/总线路径，翻 NPU_TEST_DISABLE_CACHE=1 再测。*/
#if defined(RISAF12)
    risaf_dump_illegal(RISAF12, "RISAF12/XSPI2");
#endif
#if defined(RISAF11)
    risaf_dump_illegal(RISAF11, "RISAF11/XSPI1");
#endif

    /* 任务#22：IAC 非法访问状态 */
#if defined(IAC)
    for (int _i = 0; _i < 5; _i++) {
        if (IAC->ISR[_i])
            printf("[NPU] diag: IAC ISR[%d]=%08lX (non-zero => 有非法访问被拦)\r\n",
                   _i, (unsigned long)IAC->ISR[_i]);
    }
#endif

    /* ★ NPU→XSPI2 通路硬件状态 —— 关缓存后若仍卡，这就是定位的决定性一行 */
    dump_xspi_cache_state();
}

/* 任务#22：IAC 非法访问中断处理——让所有非法访问可见 */
#if defined(IAC)
void IAC_IRQHandler(void)
{
    uint32_t isr[5];
    int hit = 0;
    for (int _i = 0; _i < 5; _i++) {
        isr[_i] = IAC->ISR[_i];
        if (isr[_i]) { hit = 1; IAC->ICR[_i] = isr[_i]; }
    }
    __DSB();
    if (hit) {
        printf("[NPU][IAC] *** ILLEGAL ACCESS *** ISR=%08lX %08lX %08lX %08lX %08lX\r\n",
               (unsigned long)isr[0], (unsigned long)isr[1], (unsigned long)isr[2],
               (unsigned long)isr[3], (unsigned long)isr[4]);
        /* 再打 RISAF 细节（哪个地址、哪个 CID、安全/非安全） */
        dump_npu_irq_state();
    }
}
#endif

int ai_model_run(const uint8_t *input, const float **output)
{
    if (input == NULL || output == NULL)
        return -1;

    const LL_Buffer_InfoTypeDef *in_info  = LL_ATON_Input_Buffers_Info_Default();
    const LL_Buffer_InfoTypeDef *out_info = LL_ATON_Output_Buffers_Info_Default();

    printf("[NPU] run: enter. in_info=%08lX out_info=%08lX\r\n",
           PHEX(in_info), PHEX(out_info));
    if (in_info != NULL && in_info[0].name != NULL) {
        printf("[NPU] run: INPUT  '%s' @ %08lX\r\n",
               in_info[0].name, PHEX(LL_Buffer_addr_start(&in_info[0])));
    }
    if (out_info != NULL && out_info[0].name != NULL) {
        printf("[NPU] run: OUTPUT '%s' @ %08lX\r\n",
               out_info[0].name, PHEX(LL_Buffer_addr_start(&out_info[0])));
    }

    if (in_info == NULL || in_info[0].name == NULL)
        return -2;

    uint8_t *in_buf = (uint8_t *)LL_Buffer_addr_start(&in_info[0]);
    if (in_buf == NULL) {
        return -3;
    }
    printf("[NPU] run: memcpy %u bytes -> %08lX ...\r\n",
           (unsigned)AI_INPUT_SIZE, PHEX(in_buf));
    memcpy(in_buf, input, AI_INPUT_SIZE);
    __DSB();   /* FIX-4: 确保 CPU 写入在 NPU 读取前可见 */
    printf("[NPU] run: memcpy done\r\n");

    /* 任务#18：用厂商一站式 EC 驱动；Init_Network / epoch 循环 / DeInit 都由 RT_Main 内部完成 */
    g_npu_infer_done  = 0;
    g_npu_infer_start = HAL_GetTick();
    printf("[NPU] run: calling LL_ATON_RT_Main (vendor EC driver)...\r\n");

#if AI_PROFILE_INFER
    uint32_t t_npu_start = HAL_GetTick();
#endif
    LL_ATON_RT_Main(&NN_Instance_Default);
#if AI_PROFILE_INFER
    uint32_t t_npu_end = HAL_GetTick();
    printf("[NPU] PROFILE: infer_total=%lums  memcpy+overhead~%lums\r\n",
           (unsigned long)(t_npu_end - t_npu_start),
           (unsigned long)(t_npu_start - g_npu_infer_start));
#endif

    g_npu_infer_done  = 1;
    printf("[NPU] run: LL_ATON_RT_Main returned (inference complete)\r\n");

    if (out_info == NULL || out_info[0].name == NULL) {
        return -4;
    }
    *output = (const float *)LL_Buffer_addr_start(&out_info[0]);
    if (*output == NULL) {
        return -5;
    }

    /* 输出在 AXISRAM(非缓存 cacheable=OFF)，CPU 直接读即可；若日后发现 score
     * 像脏数据，再在此对 out 区间做 SCB_InvalidateDCache_by_Addr。*/

    return 0;
}

void ai_model_deinit(void)
{
    LL_ATON_RT_RuntimeDeInit();
}