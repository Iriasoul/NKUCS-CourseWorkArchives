	.file	"huibian.cpp"
	.text
	.section .rdata,"dr"
_ZStL19piecewise_construct:
	.space 1
	.text
	.globl	_Z1APKii
	.def	_Z1APKii;	.scl	2;	.type	32;	.endef
	.seh_proc	_Z1APKii
_Z1APKii:
.LFB1647:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	subq	$16, %rsp
	.seh_stackalloc	16
	.seh_endprologue
	movq	%rcx, 16(%rbp)
	movl	%edx, 24(%rbp)
	movl	$0, -4(%rbp)
	movl	$0, -8(%rbp)
	jmp	.L2
.L3:
	movl	-8(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %rdx
	movq	16(%rbp), %rax
	addq	%rdx, %rax
	movl	(%rax), %eax
	addl	%eax, -4(%rbp)
	addl	$1, -8(%rbp)
.L2:
	movl	-8(%rbp), %eax
	cmpl	24(%rbp), %eax
	jl	.L3
	movl	-4(%rbp), %eax
	addq	$16, %rsp
	popq	%rbp
	ret
	.seh_endproc
	.globl	_Z1BPKii
	.def	_Z1BPKii;	.scl	2;	.type	32;	.endef
	.seh_proc	_Z1BPKii
_Z1BPKii:
.LFB1648:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	subq	$16, %rsp
	.seh_stackalloc	16
	.seh_endprologue
	movq	%rcx, 16(%rbp)
	movl	%edx, 24(%rbp)
	movl	$0, -4(%rbp)
	movl	$0, -8(%rbp)
	movl	$0, -12(%rbp)
	jmp	.L6
.L7:
	movl	-12(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %rdx
	movq	16(%rbp), %rax
	addq	%rdx, %rax
	movl	(%rax), %eax
	addl	%eax, -4(%rbp)
	movl	-12(%rbp), %eax
	cltq
	addq	$1, %rax
	leaq	0(,%rax,4), %rdx
	movq	16(%rbp), %rax
	addq	%rdx, %rax
	movl	(%rax), %eax
	addl	%eax, -8(%rbp)
	addl	$2, -12(%rbp)
.L6:
	movl	-12(%rbp), %eax
	cmpl	24(%rbp), %eax
	jl	.L7
	movl	-4(%rbp), %edx
	movl	-8(%rbp), %eax
	addl	%edx, %eax
	addq	$16, %rsp
	popq	%rbp
	ret
	.seh_endproc
	.globl	_Z1CPii
	.def	_Z1CPii;	.scl	2;	.type	32;	.endef
	.seh_proc	_Z1CPii
_Z1CPii:
.LFB1649:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	subq	$16, %rsp
	.seh_stackalloc	16
	.seh_endprologue
	movq	%rcx, 16(%rbp)
	movl	%edx, 24(%rbp)
	movl	24(%rbp), %eax
	movl	%eax, -4(%rbp)
	jmp	.L10
.L13:
	movl	$0, -8(%rbp)
	jmp	.L11
.L12:
	movl	-8(%rbp), %eax
	addl	%eax, %eax
	cltq
	leaq	0(,%rax,4), %rdx
	movq	16(%rbp), %rax
	addq	%rdx, %rax
	movl	(%rax), %ecx
	movl	-8(%rbp), %eax
	addl	%eax, %eax
	cltq
	addq	$1, %rax
	leaq	0(,%rax,4), %rdx
	movq	16(%rbp), %rax
	addq	%rdx, %rax
	movl	(%rax), %edx
	movl	-8(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %r8
	movq	16(%rbp), %rax
	addq	%r8, %rax
	addl	%ecx, %edx
	movl	%edx, (%rax)
	addl	$1, -8(%rbp)
.L11:
	movl	-4(%rbp), %eax
	movl	%eax, %edx
	shrl	$31, %edx
	addl	%edx, %eax
	sarl	%eax
	cmpl	%eax, -8(%rbp)
	jl	.L12
	movl	-4(%rbp), %eax
	movl	%eax, %edx
	shrl	$31, %edx
	addl	%edx, %eax
	sarl	%eax
	movl	%eax, -4(%rbp)
.L10:
	cmpl	$1, -4(%rbp)
	jg	.L13
	nop
	nop
	addq	$16, %rsp
	popq	%rbp
	ret
	.seh_endproc
	.def	__main;	.scl	2;	.type	32;	.endef
	.globl	main
	.def	main;	.scl	2;	.type	32;	.endef
	.seh_proc	main
main:
.LFB1650:
	pushq	%rbp
	.seh_pushreg	%rbp
	movq	%rsp, %rbp
	.seh_setframe	%rbp, 0
	subq	$64, %rsp
	.seh_stackalloc	64
	.seh_endprologue
	call	__main
	movl	$1073741824, -8(%rbp)
	movabsq	$4294967296, %rax
	movq	%rax, %rcx
	call	_Znay
	movq	%rax, -16(%rbp)
	movl	$0, -4(%rbp)
	jmp	.L15
.L16:
	movl	-4(%rbp), %eax
	cltq
	leaq	0(,%rax,4), %rdx
	movq	-16(%rbp), %rax
	addq	%rdx, %rax
	movl	$1, (%rax)
	addl	$1, -4(%rbp)
.L15:
	cmpl	$1073741823, -4(%rbp)
	jle	.L16
	movq	-16(%rbp), %rax
	movl	$1073741824, %edx
	movq	%rax, %rcx
	call	_Z1APKii
	movl	%eax, -20(%rbp)
	movq	-16(%rbp), %rax
	movl	$1073741824, %edx
	movq	%rax, %rcx
	call	_Z1BPKii
	movl	%eax, -24(%rbp)
	movabsq	$4294967296, %rax
	movq	%rax, %rcx
	call	_Znay
	movq	%rax, -32(%rbp)
	movq	-16(%rbp), %rdx
	movq	-32(%rbp), %rax
	movabsq	$4294967296, %r8
	movq	%rax, %rcx
	call	memcpy
	movq	-32(%rbp), %rax
	movl	$1073741824, %edx
	movq	%rax, %rcx
	call	_Z1CPii
	cmpq	$0, -16(%rbp)
	je	.L17
	movq	-16(%rbp), %rax
	movq	%rax, %rcx
	call	_ZdaPv
.L17:
	cmpq	$0, -32(%rbp)
	je	.L18
	movq	-32(%rbp), %rax
	movq	%rax, %rcx
	call	_ZdaPv
.L18:
	movl	$0, %eax
	addq	$64, %rsp
	popq	%rbp
	ret
	.seh_endproc
	.ident	"GCC: (GNU) 13.2.0"
	.def	_Znay;	.scl	2;	.type	32;	.endef
	.def	memcpy;	.scl	2;	.type	32;	.endef
	.def	_ZdaPv;	.scl	2;	.type	32;	.endef
