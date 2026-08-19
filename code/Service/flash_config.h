#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#include <stdint.h>

/* ========================================================================
   Flash 参数持久化 — 使用 STM32G4 内部 Flash 末页
   页面大小 2KB，地址 0x0801F800
   ======================================================================== */

/* 参数结构体：必须对齐到 8 字节（Flash 双字写入要求） */
#define FLASH_CONFIG_MAGIC      0xA5F0U
#define FLASH_CONFIG_MAX_SLAVES 4U
#define FLASH_CONFIG_DEV_ID_LEN 16U

/* 告警阈值 */
typedef struct {
    float temp_high;    /* 过温阈值（℃），默认 60.0 */
    float temp_low;     /* 欠温阈值（℃），默认 -5.0 */
    float volt_high;    /* 过压阈值（V），默认 3.2 */
    float volt_low;     /* 欠压阈值（V），默认 0.3 */
} alarm_threshold_t;

/* 持久化参数 */
typedef struct {
    uint16_t magic;                             /* 魔数校验 */
    uint16_t crc;                               /* 结构体 CRC16（不含 magic 和 crc 自身） */
    alarm_threshold_t alarm;                    /* 告警阈值 */
    uint8_t  modbus_slaves[FLASH_CONFIG_MAX_SLAVES]; /* Modbus 从站地址列表 */
    uint8_t  modbus_slave_count;                /* 实际从站数量 */
    uint16_t sample_period_ms;                  /* 采样周期（ms），默认 1000 */
    uint16_t cloud_period_ms;                   /* 云端上报周期（ms），默认 500 */
    char     device_id[FLASH_CONFIG_DEV_ID_LEN]; /* 设备 ID 字符串 */
    uint8_t  reserved[16];                      /* 预留扩展 */
} flash_config_t;

/* API */
void flash_config_init(void);
void flash_config_get(flash_config_t *cfg);
uint8_t flash_config_save(const flash_config_t *cfg);
void flash_config_load_defaults(flash_config_t *cfg);

/* 获取告警阈值（方便其他模块直接读取） */
const alarm_threshold_t *flash_config_get_alarm_thresholds(void);

#endif
