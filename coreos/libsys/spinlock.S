// int SysTestAndSet(int new_value, int *lock_pointer);

.globl _SysTestAndSet
_SysTestAndSet:
    movl 4(%esp),%eax  # get new_value into %eax
    movl 8(%esp),%edx  # get lock_pointer into %edx
    lock               # next instruction is locked
    xchgl %eax,(%edx)  # swap %eax with what is stored in (%edx)
                       # ... and don't let any other cpu touch that 
                       # ... memory location while you're swapping
    ret                # return the old value that's in %eax

	.align 8

// decrement *refcount, return whether or not it went zero
// int SysDecrementAndTest(int *refcount);

.globl _SysDecrementAndTest
_SysDecrementAndTest:
    xorl %eax,%eax    # clear all of %eax
    movl 4(%esp),%edx # get pointer to refcount
    lock              # keep other cpu's out of next instruction
    decl (%edx)       # decrement refcount, keeping other cpu's out of refcount
    setz %al          # set %eax=1 if result is zero, set %eax=0 otherwise
    ret               # return %eax as the result

	.align 8

// void SysIncrement(int *refcount);

/*.globl _SysIncrement
_SysIncrement:
    movl 4(%esp),%edx
    lock
    incl (%edx)
    ret*/
    
// int SysIncrement(int *count);

.globl _SysIncrement
_SysIncrement:
	movl 4(%esp),%ecx
	movl $1,%eax
	lock
	xadd %eax,(%ecx)
	ret
