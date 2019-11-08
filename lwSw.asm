.text
	lui $a0, 0x0040
	addiu $t5, $0, 0x1000
	or $a0, $a0, $t5 
	addu $a1, $a0, $0
	addiu $t0, $0, 1
	addiu $t1, $0, 2
	addu $t2, $t0, $0
	addiu $t3, $0, 10

Loop:
	sll $0,$0,0
	sw $t2, 0($a1)
	addu $t2, $t2, $t0
	addiu $a1, $a1, 4
	bne  $t2, $t3, Loop
	
	addu $t2,$0,$0
	addiu $t3, $t3, -1
Loop2:
	sll $0,$0,0
	lw $t2, 0($a0)
	addiu $a0, $a0, 8
	bne  $t2, $t3, Loop2
	
Done:
	li      $v0, 10    
	syscall