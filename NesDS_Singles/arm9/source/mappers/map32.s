@---------------------------------------------------------------------------------
.section .text,"ax"
@---------------------------------------------------------------------------------
	#include "equates.h"
@---------------------------------------------------------------------------------
	.global mapper32init
	pswitch = mapperdata+0
@---------------------------------------------------------------------------------
mapper32init:	@Irem... Image Fight (J)
@---------------------------------------------------------------------------------
	.word write8000,writeA000,void,void

	mov pc,lr
@---------------------------------------------------------------------------------
write8000:
@---------------------------------------------------------------------------------
	tst addy,#0x1000
	bne write9000
	ldr_ r1,pswitch
	tst r1,#0x02
	beq map89_
	bne mapCD_
write9000:
	str_ r0,pswitch
	tst r0,#0x1
	b mirror2V_
@---------------------------------------------------------------------------------
writeA000:
@---------------------------------------------------------------------------------
	tst addy,#0x1000
	beq mapAB_
	and addy,addy,#7
	ldr r1,=writeCHRTBL
	ldr pc,[r1,addy,lsl#2]
@---------------------------------------------------------------------------------