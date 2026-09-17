	.file	"huibian.cpp"
	.text
	.section .rdata,"dr"
_ZStL19piecewise_construct:
	.space 1
	.def	__main;	.scl	2;	.type	32;	.endef
	.text
	.globl	main
	.def	main;	.scl	2;	.type	32;	.endef
	.seh_proc	main
main:
.LFB1647:
	pushq	%rbp
	.seh_pushreg	%rbp
	pushq	%rbx
	.seh_pushreg	%rbx
	subq	$104, %rsp
	.seh_stackalloc	104
	leaq	96(%rsp), %rbp
	.seh_setframe	%rbp, 96
	.seh_endprologue
	call	__main
	movl	$100, -32(%rbp)
	movl	$800, %ecx
	call	_Znay
	movq	%rax, -40(%rbp)
	movl	$0, -4(%rbp)
	jmp	.L2
.L3:
	movl	-4(%rbp), %eax
	cltq
	leaq	0(,%rax,8), %rdx
	movq	-40(%rbp), %rax
	leaq	(%rdx,%rax), %rbx
	movl	$400, %ecx
	call	_Znay
	movq	%rax, (%rbx)
	addl	$1, -4(%rbp)
.L2:
	cmpl	$99, -4(%rbp)
	jle	.L3
	movl	$400, %ecx
	call	_Znay
	movq	%rax, -48(%rbp)
	movl	$400, %ecx
	call	_Znay
	movq	%rax, -56(%rbp)
	movl	$0, -8(%rbp)
	jmp	.L4
.L7:
	movl	-8(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %rdx
	movq	-48(%rbp), %rax
	addq	%rax, %rdx
	movl	-8(%rbp), %eax
	movl	%eax, (%rdx)
	movl	-8(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %rdx
	movq	-56(%rbp), %rax
	addq	%rdx, %rax
	movl	$0, (%rax)
	movl	$0, -12(%rbp)
	jmp	.L5
.L6:
	movl	-8(%rbp), %eax
	cltq
	leaq	0(,%rax,8), %rdx
	movq	-40(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %rax
	movl	-12(%rbp), %edx
	movslq	%edx, %rdx
	salq	$2, %rdx
	addq	%rdx, %rax
	movl	-8(%rbp), %ecx
	movl	-12(%rbp), %edx
	addl	%ecx, %edx
	movl	%edx, (%rax)
	addl	$1, -12(%rbp)
.L5:
	cmpl	$99, -12(%rbp)
	jle	.L6
	addl	$1, -8(%rbp)
.L4:
	cmpl	$99, -8(%rbp)
	jle	.L7
	movl	$0, -16(%rbp)
	jmp	.L8
.L9:
	movl	-16(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %rdx
	movq	-56(%rbp), %rax
	addq	%rdx, %rax
	movl	$0, (%rax)
	addl	$1, -16(%rbp)
.L8:
	cmpl	$99, -16(%rbp)
	jle	.L9
	movl	$0, -20(%rbp)
	jmp	.L10
.L13:
	movl	$0, -24(%rbp)
	jmp	.L11
.L12:
	movl	-24(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %rdx
	movq	-56(%rbp), %rax
	addq	%rdx, %rax
	movl	(%rax), %ecx
	movl	-20(%rbp), %eax
	cltq
	leaq	0(,%rax,8), %rdx
	movq	-40(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %rax
	movl	-24(%rbp), %edx
	movslq	%edx, %rdx
	salq	$2, %rdx
	addq	%rdx, %rax
	movl	(%rax), %edx
	movl	-20(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %r8
	movq	-48(%rbp), %rax
	addq	%r8, %rax
	movl	(%rax), %eax
	imull	%eax, %edx
	movl	-24(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %r8
	movq	-56(%rbp), %rax
	addq	%r8, %rax
	addl	%ecx, %edx
	movl	%edx, (%rax)
	addl	$1, -24(%rbp)
.L11:
	cmpl	$99, -24(%rbp)
	jle	.L12
	addl	$1, -20(%rbp)
.L10:
	cmpl	$99, -20(%rbp)
	jle	.L13
	movl	$0, -28(%rbp)
	jmp	.L14
.L16:
	movl	-28(%rbp), %eax
	cltq
	leaq	0(,%rax,8), %rdx
	movq	-40(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %rax
	testq	%rax, %rax
	je	.L15
	movl	-28(%rbp), %eax
	cltq
	leaq	0(,%rax,8), %rdx
	movq	-40(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %rax
	movq	%rax, %rcx
	call	_ZdaPv
.L15:
	addl	$1, -28(%rbp)
.L14:
	cmpl	$99, -28(%rbp)
	jle	.L16
	cmpq	$0, -40(%rbp)
	je	.L17
	movq	-40(%rbp), %rax
	movq	%rax, %rcx
	call	_ZdaPv
.L17:
	cmpq	$0, -48(%rbp)
	je	.L18
	movq	-48(%rbp), %rax
	movq	%rax, %rcx
	call	_ZdaPv
.L18:
	cmpq	$0, -56(%rbp)
	je	.L19
	movq	-56(%rbp), %rax
	movq	%rax, %rcx
	call	_ZdaPv
.L19:
	movl	$0, %eax
	addq	$104, %rsp
	popq	%rbx
	popq	%rbp
	ret
	.seh_endproc
	.ident	"GCC: (GNU) 13.2.0"
	.def	_Znay;	.scl	2;	.type	32;	.endef
	.def	_ZdaPv;	.scl	2;	.type	32;	.endef
