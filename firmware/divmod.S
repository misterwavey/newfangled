.syntax unified
.cpu cortex-m0
.thumb
.global __aeabi_uidivmod
.type __aeabi_uidivmod,%function
.thumb_func
__aeabi_uidivmod:
  push {r4,lr}
  mov r4,r0
  push {r1,r2}
  bl __aeabi_uidiv
  pop {r1,r2}
  muls r1,r0,r1
  subs r1,r4,r1
  pop {r4,pc}
