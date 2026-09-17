.data
    .align 2
arrayA: .word 1, 2, 3, 4, 5
arrayB: .word 2, 3, 4, 5, 6
length: .word 5

    .text
    .align 2
    .global _start          // 程序入口

_start:
    // --- 初始化寄存器 ---
    LDR r0, =arrayA         // R0: 数组A基地址
    LDR r1, =arrayB         // R1: 数组B基地址
    LDR r3, =length
    LDR r3, [r3]            // R3: 循环计数器 (length)
    MOV r2, #0              // R2: 累加器 (sum) 初始化为 0

dot_product_loop:
    CMP r3, #0              // 检查计数器
    BLE end_loop            // 为0则跳出循环

    // 核心差异点：ARM 后序自增寻址，一条指令干两件事
    LDR r4, [r0], #4        // 取数并自动将数组A指针+4
    LDR r5, [r1], #4        // 取数并自动将数组B指针+4
    
    MUL r6, r4, r5          // 乘法运算
    ADD r2, r2, r6          // 累加
    
    SUB r3, r3, #1          // 计数器-1
    B dot_product_loop      // 跳转回循环

end_loop:
    MOV r0, r2              // 将我们计算出的点积结果(70)放入 r0 作为退出状态码
    MOV r7, #1              // Linux ARM EABI 中 exit 的系统调用号为 1
    SVC #0                  // 触发 Supervisor Call 陷入内核

