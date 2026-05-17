// 1. 引入自己写的头文件
#include "terminal_service.h"
// 2. 引入系统标准库：用来做「内存清零/拷贝」的工具
#include <string.h>

// 3. 定义一个「私有的全局小本子」
// static = 只有这个文件能碰它，外面的代码改不了
// g_terminal_data = 全局变量（Global）
static terminal_data_t g_terminal_data;


// 4. 函数功能：初始化小本子（把本子擦干净）
void terminal_init(void)
{
    // 5. 把全局小账本的所有内容，全部清零
    memset(&g_terminal_data, 0, sizeof(g_terminal_data));
}


// 6. 函数功能：往小本子里写数字 + 记录写了几次
// 参数：value = 你要存的数字
void terminal_set_local_value(uint16_t value)
{
    // 7. 把传入的数字，存到小本子的「数据位」
    g_terminal_data.local_value = value;
    // 8. 存完一次，计数+1（记录我一共存了多少次数据）
    g_terminal_data.sample_count++;
}


// 9. 函数功能：把小本子的内容，「复印一份」给外面的人
// 参数：data = 外面的人准备好的「复印纸」
void terminal_get_snapshot(terminal_data_t *data)
{
    // 10. 安全检查：如果外面的人没带复印纸（空指针），直接返回，不干活
    if (data == NULL) {
        return;
    }

    // 11. 把我们的私人小账本，完整复制一份到外面的复印纸上
    *data = g_terminal_data;
}
