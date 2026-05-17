// 引入LED板级支持包的头文件
// 头文件里一般放函数声明、宏定义，让其他文件能调用这里的LED函数
#include "bsp_led.h"

// 定义一个"静态全局变量"，专门用来保存8个LED的当前亮灭状态
// static：只有这个.c文件里的函数能访问它，其他文件改不了，更安全
// uint8_t：8位无符号整数（刚好存8个LED的状态，1位对应1个LED）
// 0x00：初始值是二进制00000000，代表所有LED一开始都是熄灭的
static uint8_t led_state = 0x00;


// 【函数2】一次性设置8个LED的全部状态（核心函数）
// 参数value：8位二进制数，每一位对应一个LED的状态（1=亮，0=灭）
// 比如value=0x03（二进制00000011）→ LED1和LED2亮，其他灭
void bsp_led_write8(uint8_t value)
{
    // 先把新的8个LED状态保存到全局变量里
    // 这样其他函数（on/off/toggle）就能知道当前LED是什么状态了
    led_state = value;

    // 拉高PD2，准备给锁存器发数据
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET);

    // 循环8次，依次设置每一个LED的状态
    // i从0到7，对应LED1到LED8
    for (uint8_t i = 0; i < 8; i++)
    {
        // 检查value的第i位是不是1
        // 1 << i：生成一个只有第i位是1，其他位是0的数
        // &：按位与运算，结果非0说明第i位是1
        if (value & (1 << i))
        {
            // 第i位是1 → 对应LED要亮 → 拉低PC8+i引脚
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8 << i, GPIO_PIN_RESET);
        }
        else
        {
            // 第i位是0 → 对应LED要灭 → 拉高PC8+i引脚
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8 << i, GPIO_PIN_SET);
        }
    }

    // 拉低PD2，锁存所有LED的状态
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
}

// 【函数3】点亮指定的某一个LED
// 参数led：要点亮的LED编号（1~8）
void bsp_led_on(uint8_t led)
{
    // 同样先检查LED编号是否合法
    if (led < 1 || led > 8)
    {
        return;
    }

    // 把全局状态变量中对应LED的位设为1，其他位保持不变
    // |=：按位或赋值，只要有一个是1结果就是1
    // 比如原来led_state=0x00（全灭），led=3 → 1<<2=4 → 0x00 | 0x04=0x04 → 第3位变1
    led_state |= (1 << (led - 1));
    
    // 调用核心函数，把新的状态写入硬件，让LED真的亮起来
    bsp_led_write8(led_state);
}

// 【函数4】熄灭指定的某一个LED
// 参数led：要熄灭的LED编号（1~8）
void bsp_led_off(uint8_t led)
{
    // 参数合法性检查
    if (led < 1 || led > 8)
    {
        return;
    }

    // 把全局状态变量中对应LED的位设为0，其他位保持不变
    // ~：按位取反，比如1<<2=0x04 → ~0x04=0xFB（二进制11111011）
    // &=：按位与赋值，只有两个都是1结果才是1，这样就能把对应位清0
    led_state &= ~(1 << (led - 1));
    
    // 写入硬件更新状态
    bsp_led_write8(led_state);
}

// 【函数5】翻转指定LED的状态（亮变灭，灭变亮）
// 参数led：要翻转的LED编号（1~8）
void bsp_led_toggle(uint8_t led)
{
    // 参数合法性检查
    if (led < 1 || led > 8)
    {
        return;
    }

    // 把全局状态变量中对应LED的位取反，其他位不变
    // ^=：按位异或赋值，相同为0，不同为1 → 1变0，0变1
    led_state ^= (1 << (led - 1));
    
    // 写入硬件更新状态
    bsp_led_write8(led_state);
}
