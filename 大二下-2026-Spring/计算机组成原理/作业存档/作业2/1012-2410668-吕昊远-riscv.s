.data
    .balign 4
arrayA: .word 1, 2, 3, 4, 5
arrayB: .word 2, 3, 4, 5, 6
length: .word 5

    .text
    .balign 4
    .globl _start           # 程序入口

_start:
    # --- 初始化寄存器 ---
    la s0, arrayA           # s0: 数组A基地址
    la s1, arrayB           # s1: 数组B基地址
    la t0, length
    lw s2, 0(t0)            # s2: 循环计数器 (length)
    li s3, 0                # s3: 累加器 (sum) 初始化为 0

dot_product_loop:
    beq s2, zero, end_loop  # 计数器为0则跳出循环

    # 核心差异点：RISC-V 访存与算术指令强制分离
    lw t1, 0(s0)            # 取数 A[i]
    lw t2, 0(s1)            # 取数 B[i]
    
    mul t3, t1, t2          # 乘法运算
    add s3, s3, t3          # 累加
    
    addi s0, s0, 4          # 使用独立指令使数组A指针+4
    addi s1, s1, 4          # 使用独立指令使数组B指针+4
    addi s2, s2, -1         # 计数器-1
    j dot_product_loop      # 跳转回循环

end_loop:
    mv a0, s3               # 将我们计算出的点积结果(70)放入 a0 作为退出状态码
    li a7, 93               # Linux RISC-V 中 exit 的系统调用号为 93
    ecall                   # 触发 Environment Call 陷入内核

