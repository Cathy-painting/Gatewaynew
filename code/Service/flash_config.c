#include "flash_config.h"
#include "crc16.h"
#include "log.h"
#include "cmsis_os.h"
#include <string.h>
#include "stm32g4xx_hal.h"

/* ========================================================================
   STM32G431RB: 128KB Flash, 64 页 × 2KB
   使用末页 Page 63 作为参数存储区（0x0801F800 — 0x0801FFFF）
   ======================================================================== */
#define FLASH_CFG_ADDR      0x0801F800U
#define FLASH_CFG_PAGE      63U
#define FLASH_CFG_BANK      FLASH_BANK_1

static flash_config_t g_flash_config;

/* 64 位对齐写入缓冲区 */
static uint64_t flash_write_buf[sizeof(flash_config_t) / 8 + 1];

/* 默认配置 */
static void load_defaults(flash_config_t *cfg)
{
    memset(cfg, 0, sizeof(flash_config_t));
    cfg->magic             = FLASH_CONFIG_MAGIC;
    cfg->alarm.temp_high   = 60.0f;
    cfg->alarm.temp_low    = -5.0f;
    cfg->alarm.volt_high   = 3.2f;
    cfg->alarm.volt_low    = 0.3f;
    cfg->modbus_slaves[0]  = 1;
    cfg->modbus_slave_count = 1;
    cfg->sample_period_ms  = 1000;
    cfg->cloud_period_ms   = 500;
    strncpy(cfg->device_id, "GW-G431-001", FLASH_CONFIG_DEV_ID_LEN - 1);
}

/* 计算结构体 CRC（从 alarm 字段开始，跳过 magic 和 crc） */
static uint16_t calc_cfg_crc(const flash_config_t *cfg)
{
    const uint8_t *p = (const uint8_t *)&cfg->alarm;
    uint16_t len = (uint16_t)(sizeof(flash_config_t) - ((uintptr_t)&cfg->alarm - (uintptr_t)cfg));
    return crc16_modbus(p, len);
}

/* 从 Flash 读取配置 */
static uint8_t read_from_flash(flash_config_t *cfg)
{
    const flash_config_t *p = (const flash_config_t *)FLASH_CFG_ADDR;
    uint16_t saved_crc;

    /* 检查魔数 */
    if (p->magic != FLASH_CONFIG_MAGIC) {
        log_info("[FLASH] magic mismatch, use defaults\r\n");
        return 0U;
    }

    /* 复制数据 */
    memcpy(cfg, p, sizeof(flash_config_t));

    /* 校验 CRC */
    saved_crc = cfg->crc;
    cfg->crc = 0U;
    if (calc_cfg_crc(cfg) != saved_crc) {
        log_info("[FLASH] CRC mismatch, use defaults\r\n");
        return 0U;
    }
    cfg->crc = saved_crc;

    log_info("[FLASH] config loaded OK\r\n");
    return 1U;
}

/* 擦除参数页 */
static uint8_t erase_page(void)
{
    HAL_StatusTypeDef status;
    uint32_t page_error = 0U;
    FLASH_EraseInitTypeDef erase_init = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks     = FLASH_CFG_BANK,
        .Page      = FLASH_CFG_PAGE,
        .NbPages   = 1U
    };

    /* Flash 擦除期间会禁用中断（约 20-25ms），
       临时提升当前任务优先级，防止被抢占后进入不可中断的 Flash 操作 */
    osThreadId_t self = osThreadGetId();
    osPriority_t old_prio = osThreadGetPriority(self);
    osThreadSetPriority(self, osPriorityHigh);

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        log_info("[FLASH] unlock failed\r\n");
        osThreadSetPriority(self, old_prio);
        return 0U;
    }

    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    if (status != HAL_OK) {
        log_infof("[FLASH] erase failed: status=%d page_err=%lu\r\n", status, (unsigned long)page_error);
        HAL_FLASH_Lock();
        osThreadSetPriority(self, old_prio);
        return 0U;
    }

    HAL_FLASH_Lock();
    osThreadSetPriority(self, old_prio);
    return 1U;
}

/* 写一个双字到 Flash */
static uint8_t write_doubleword(uint32_t addr, uint64_t data)
{
    HAL_StatusTypeDef status;

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) return 0U;

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, data);
    if (status != HAL_OK) {
        log_infof("[FLASH] program failed at 0x%08lX: %d\r\n", (unsigned long)addr, status);
        HAL_FLASH_Lock();
        return 0U;
    }

    HAL_FLASH_Lock();
    return 1U;
}

/* ========================================================================
   公共接口
   ======================================================================== */

void flash_config_init(void)
{
    if (!read_from_flash(&g_flash_config)) {
        load_defaults(&g_flash_config);
        flash_config_save(&g_flash_config);
    }
    log_infof("[FLASH] dev_id=%s tempH=%.1f tempL=%.1f vH=%.2f vL=%.2f "
              "slaves=%u sample=%ums cloud=%ums\r\n",
              g_flash_config.device_id,
              g_flash_config.alarm.temp_high, g_flash_config.alarm.temp_low,
              g_flash_config.alarm.volt_high, g_flash_config.alarm.volt_low,
              g_flash_config.modbus_slave_count,
              g_flash_config.sample_period_ms, g_flash_config.cloud_period_ms);
}

void flash_config_get(flash_config_t *cfg)
{
    if (cfg != NULL) {
        memcpy(cfg, &g_flash_config, sizeof(flash_config_t));
    }
}

void flash_config_load_defaults(flash_config_t *cfg)
{
    if (cfg != NULL) {
        load_defaults(cfg);
    }
}

uint8_t flash_config_save(const flash_config_t *cfg)
{
    flash_config_t tmp;
    uint32_t addr;
    const uint64_t *src;
    uint16_t count;

    if (cfg == NULL) return 0U;

    /* 拷贝并计算 CRC */
    memcpy(&tmp, cfg, sizeof(flash_config_t));
    tmp.magic = FLASH_CONFIG_MAGIC;
    tmp.crc = 0U;
    tmp.crc = calc_cfg_crc(&tmp);

    /* 擦除参数页 */
    if (!erase_page()) {
        return 0U;
    }

    /* 按双字写入 */
    memcpy(flash_write_buf, &tmp, sizeof(flash_config_t));
    addr  = FLASH_CFG_ADDR;
    src   = flash_write_buf;
    count = (sizeof(flash_config_t) + 7U) / 8U;

    for (uint16_t i = 0; i < count; i++) {
        if (!write_doubleword(addr, src[i])) {
            return 0U;
        }
        addr += 8U;
    }

    /* 更新内存镜像 */
    memcpy(&g_flash_config, &tmp, sizeof(flash_config_t));

    log_info("[FLASH] config saved OK\r\n");
    return 1U;
}

const alarm_threshold_t *flash_config_get_alarm_thresholds(void)
{
    return &g_flash_config.alarm;
}
