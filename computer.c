#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "computer.h"
#include "string.h"
#include <stdint.h>
#undef mips			/* gcc already has a def for mips */

unsigned int endianSwap(unsigned int);

void PrintInfo (int changedReg, int changedMem);
unsigned int Fetch (int);
void Decode (unsigned int, DecodedInstr*, RegVals*);
int Execute (DecodedInstr*, RegVals*);
int Mem(DecodedInstr*, int, int *);
void RegWrite(DecodedInstr*, int, int *);
void UpdatePC(DecodedInstr*, int);
void PrintInstruction (DecodedInstr*);
unsigned createMask(unsigned a, unsigned b);
//macro provided at: https://stackoverflow.com/questions/523724/c-c-check-if-one-bit-is-set-in-i-e-int-variable
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define false 0
#define true 1
typedef int bool;

int32_t signExtension(int32_t instr) {
    int16_t value = (int16_t)instr;
    return (int32_t)value;
}
signed complement(int immed){
 	//Performing ~ operator will not give us the correct value
	//Perform XOR operation to give us the "correct complement"
	 signed value = immed ^ 0xffffffff;
	//If the complement is "0"it is -1
	if(value == 0){
	   value = ~value;
	}
	//If not, then take 2's complement
	else
	{
	   value = ~value + 1 ;
	}
	return value;	 
}
bool checkSigned(int immed){
        unsigned first_bit = immed & (1 << (31));
	first_bit = first_bit >> 31; 
	//Checking if the value is signed
	if(first_bit == 1){
	   return true;
	}
	else{
	   return false; 
	}
}

/*Globally accessible Computer variable*/
Computer mips;
RegVals rVals;

/*
 *  Return an initialized computer with the stack pointer set to the
 *  address of the end of data memory, the remaining registers initialized
 *  to zero, and the instructions read from the given file.
 *  The other arguments govern how the program interacts with the user.
 */
void InitComputer (FILE* filein, int printingRegisters, int printingMemory,
  int debugging, int interactive) {
    int k;
    unsigned int instr;

    /* Initialize registers and memory */

    for (k=0; k<32; k++) {
        mips.registers[k] = 0;
    }
    
    /* stack pointer - Initialize to highest address of data segment */
    mips.registers[29] = 0x00400000 + (MAXNUMINSTRS+MAXNUMDATA)*4;

    for (k=0; k<MAXNUMINSTRS+MAXNUMDATA; k++) {
        mips.memory[k] = 0;
    }
    
    k = 0;
    while (fread(&instr, 4, 1, filein)) {
	/*swap to big endian, convert to host byte order. Ignore this.*/
        mips.memory[k] = ntohl(endianSwap(instr));
        k++;
        if (k>MAXNUMINSTRS) {
            fprintf (stderr, "Program too big.\n");
            exit (1);
        }
    }

    mips.printingRegisters = printingRegisters;
    mips.printingMemory = printingMemory;
    mips.interactive = interactive;
    mips.debugging = debugging;
}

unsigned int endianSwap(unsigned int i) {
    return (i>>24)|(i>>8&0x0000ff00)|(i<<8&0x00ff0000)|(i<<24);
}

/*
 *  Run the simulation.
 */
void Simulate () {
    char s[40];  /* used for handling interactive input */
    unsigned int instr;
    int changedReg=-1, changedMem=-1, val;
    DecodedInstr d;
    
    /* Initialize the PC to the start of the code section */
    mips.pc = 0x00400000;
    while (1) {
        if (mips.interactive) {
            printf ("> ");
            fgets (s,sizeof(s),stdin);
            if (s[0] == 'q') {
                return;
            }
        }

        /* Fetch instr at mips.pc, returning it in instr */
        instr = Fetch (mips.pc);

	/*if(instr == 0x00000000){
	   exit(0);
	}*/
        printf ("Executing instruction at %8.8x: %8.8x\n", mips.pc, instr);

        /* 
	 * Decode instr, putting decoded instr in d
	 * Note that we reuse the d struct for each instruction.
	 */
        Decode (instr, &d, &rVals);

        /*Print decoded instruction*/
        PrintInstruction(&d);

        /* 
	 * Perform computation needed to execute d, returning computed value 
	 * in val 
	 */
        val = Execute(&d, &rVals);

		UpdatePC(&d,val);

        /* 
	 * Perform memory load or store. Place the
	 * address of any updated memory in *changedMem, 
	 * otherwise put -1 in *changedMem. 
	 * Return any memory value that is read, otherwise return -1.
         */
        val = Mem(&d, val, &changedMem);

        /* 
	 * Write back to register. If the instruction modified a register--
	 * (including jal, which modifies $ra) --
         * put the index of the modified register in *changedReg,
         * otherwise put -1 in *changedReg.
         */
        RegWrite(&d, val, &changedReg);

        PrintInfo (changedReg, changedMem);

	
    }
}

/*
 *  Print relevant information about the state of the computer.
 *  changedReg is the index of the register changed by the instruction
 *  being simulated, otherwise -1.
 *  changedMem is the address of the memory location changed by the
 *  simulated instruction, otherwise -1.
 *  Previously initialized flags indicate whether to print all the
 *  registers or just the one that changed, and whether to print
 *  all the nonzero memory or just the memory location that changed.
 */
void PrintInfo ( int changedReg, int changedMem) {
    int k, addr;
    printf ("New pc = %8.8x\n", mips.pc);
    if (!mips.printingRegisters && changedReg == -1) {
        printf ("No register was updated.\n");
    } else if (!mips.printingRegisters) {
        printf ("Updated r%2.2d to %8.8x\n",
        changedReg, mips.registers[changedReg]);
    } else {
        for (k=0; k<32; k++) {
            printf ("r%2.2d: %8.8x  ", k, mips.registers[k]);
            if ((k+1)%4 == 0) {
                printf ("\n");
            }
        }
    }
    if (!mips.printingMemory && changedMem == -1) {
        printf ("No memory location was updated.\n");
    } else if (!mips.printingMemory) {
        printf ("Updated memory at address %8.8x to %8.8x\n",
        changedMem, Fetch (changedMem));
    } else {
        printf ("Nonzero memory\n");
        printf ("ADDR	  CONTENTS\n");
        for (addr = 0x00400000+4*MAXNUMINSTRS;
             addr < 0x00400000+4*(MAXNUMINSTRS+MAXNUMDATA);
             addr = addr+4) {
            if (Fetch (addr) != 0) {
                printf ("%8.8x  %8.8x\n", addr, Fetch (addr));
            }
        }
    }
}

/*
 *  Return the contents of memory at the given address. Simulates
 *  instruction fetch. 
 */
unsigned int Fetch ( int addr) {
    return mips.memory[(addr-0x00400000)/4];
}
/*Retrieves n specific bits from point a to point b*/
unsigned createMask(unsigned a, unsigned b){
   unsigned r = 0; 
   for(unsigned i = a; i <= b; i++){
      r |= 1 << i; 
   }
   return r; 
}

/* Decode instr, returning decoded instruction. */
void Decode ( unsigned int instr, DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */

    //opcode 
    char o[6];
    //funct
    char f[6];
    unsigned r = createMask(26, 31); 
    //perfom & operation to get the bits for the opcode
    unsigned result = r & instr; 
    //retrieve opcode
    result = result >> 26; 
    r = createMask(0, 5);

    //perform & operation to retrieve the bits for the function
    unsigned funct = r & instr;    

    //store the calculated values into char* 
    sprintf(o, "%2.2x", result);
    sprintf(f, "%2.2x", funct);
    //printf("The instruction opcode value is %s\n", o);
    //printf("The funct/immediate value is %s\n", f);*/

    /*
     Using Mips Green Sheet for opcode and funct
     All R formats have 0 opcode, but different funct values
    */

    //R-format conditions
    if(strcmp(o, "00") == 0 && strcmp(f, "21") == 0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        //printf("Rd:%u\n", rd); 
	unsigned shamt = createMask(6, 10); 
	shamt = shamt & instr; 
	shamt = shamt >> 6; 
	//funct was retrieved earlier so ....

        //Setting rs register
	d->regs.r.rs = rs; 
	//Setting rt register
	d->regs.r.rt = rt;
	//Setting rd register
	d->regs.r.rd = rd;
	//Setting shamt register
	d->regs.r.shamt = shamt; 
	//Setting funct register
	d->regs.r.funct = funct; 
        //Testing if decoded properly
	/*printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);*/

	//Need to get rVals updated
	rVals->R_rd = d->regs.r.rd;
	rVals->R_rs = d->regs.r.rs; 
	rVals->R_rt = d->regs.r.rt; 


	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);

        //printf("addu\n");
        return; 
    }
    else if(strcmp(o, "00") == 0 && strcmp(f, "24") == 0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        //printf("Rd:%u\n", rd); 
	unsigned shamt = createMask(6, 10); 
	shamt = shamt & instr; 
	shamt = shamt >> 6; 
	//funct was retrieved earlier so ....

        //Setting rs register
	d->regs.r.rs = rs; 
	//Setting rt register
	d->regs.r.rt = rt;
	//Setting rd register
	d->regs.r.rd = rd;
	//Setting shamt register
	d->regs.r.shamt = shamt; 
	//Setting funct register
	d->regs.r.funct = funct; 
        //Testing if decoded properly
	/*printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);*/

	//Need to get rVals updated
	rVals->R_rd = d->regs.r.rd;
	rVals->R_rs = d->regs.r.rs; 
	rVals->R_rt = d->regs.r.rt;

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


        //printf("and\n");
        return; 
    }
    else if(strcmp(o, "00") == 0 && strcmp(f, "08") == 0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        //printf("Rd:%u\n", rd); 
	unsigned shamt = createMask(6, 10); 
	shamt = shamt & instr; 
	shamt = shamt >> 6; 
	//funct was retrieved earlier so ....

        //Setting rs register
	d->regs.r.rs = rs; 
	//Setting rt register
	d->regs.r.rt = rt;
	//Setting rd register
	d->regs.r.rd = rd;
	//Setting shamt register
	d->regs.r.shamt = shamt; 
	//Setting funct register
	d->regs.r.funct = funct; 
        //Testing if decoded properly
	/*printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);*/

	//Need to get rVals updated
	rVals->R_rd = d->regs.r.rd;
	rVals->R_rs = d->regs.r.rs; 
	rVals->R_rt = d->regs.r.rt;

	//printf("RegsVals: rd: %u rs:%8.8x rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


        //printf("jr\n");
        return;
    }
    else if(strcmp(o, "00") == 0 && strcmp(f, "25")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        //printf("Rd:%u\n", rd); 
	unsigned shamt = createMask(6, 10); 
	shamt = shamt & instr; 
	shamt = shamt >> 6; 
	//funct was retrieved earlier so ....

        //Setting rs register
	d->regs.r.rs = rs; 
	//Setting rt register
	d->regs.r.rt = rt;
	//Setting rd register
	d->regs.r.rd = rd;
	//Setting shamt register
	d->regs.r.shamt = shamt; 
	//Setting funct register
	d->regs.r.funct = funct; 
        //Testing if decoded properly
	/*printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);*/

	//Need to get rVals updated
	rVals->R_rd = d->regs.r.rd;
	rVals->R_rs = d->regs.r.rs; 
	rVals->R_rt = d->regs.r.rt; 

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);

        //printf("or\n");
        return;
    }
    else if(strcmp(o, "00") == 0 && strcmp(f, "2a")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        //printf("Rd:%u\n", rd); 
	unsigned shamt = createMask(6, 10); 
	shamt = shamt & instr; 
	shamt = shamt >> 6; 
	//funct was retrieved earlier so ....

        //Setting rs register
	d->regs.r.rs = rs; 
	//Setting rt register
	d->regs.r.rt = rt;
	//Setting rd register
	d->regs.r.rd = rd;
	//Setting shamt register
	d->regs.r.shamt = shamt; 
	//Setting funct register
	d->regs.r.funct = funct; 
        //Testing if decoded properly
	/*printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);*/
        //printf("slt\n");

	//Need to get rVals updated
	rVals->R_rd = d->regs.r.rd;
	rVals->R_rs = d->regs.r.rs; 
	rVals->R_rt = d->regs.r.rt; 

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


        return;
    }
    else if(strcmp(o, "00") == 0 && strcmp(f, "00")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        //printf("Rd:%u\n", rd); 
	unsigned shamt = createMask(6, 10); 
	shamt = shamt & instr; 
	shamt = shamt >> 6; 
	//funct was retrieved earlier so ....

        //Setting rs register
	d->regs.r.rs = rs; 
	//Setting rt register
	d->regs.r.rt = rt;
	//Setting rd register
	d->regs.r.rd = rd;
	//Setting shamt register
	d->regs.r.shamt = shamt; 
	//Setting funct register
	d->regs.r.funct = funct; 
        //Testing if decoded properly
	/*printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);*/
        //printf("sll\n");

	//Need to get rVals updated
	rVals->R_rd = d->regs.r.rd;
	rVals->R_rs = d->regs.r.rs; 
	rVals->R_rt = d->regs.r.rt; 

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


        return;
    }
    else if(strcmp(o, "00") == 0 && strcmp(f, "02")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        //printf("Rd:%u\n", rd); 
	unsigned shamt = createMask(6, 10); 
	shamt = shamt & instr; 
	shamt = shamt >> 6; 
	//funct was retrieved earlier so ....

        //Setting rs register
	d->regs.r.rs = rs; 
	//Setting rt register
	d->regs.r.rt = rt;
	//Setting rd register
	d->regs.r.rd = rd;
	//Setting shamt register
	d->regs.r.shamt = shamt; 
	//Setting funct register
	d->regs.r.funct = funct; 
        //Testing if decoded properly
	/*printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);*/
        //printf("srl\n");

	//Need to get rVals updated
	rVals->R_rd = d->regs.r.rd;
	rVals->R_rs = d->regs.r.rs; 
	rVals->R_rt = d->regs.r.rt; 

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);
 

        return;
    }
    else if(strcmp(o, "00") == 0 && strcmp(f, "23")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        //printf("Rd:%u\n", rd); 
	unsigned shamt = createMask(6, 10); 
	shamt = shamt & instr; 
	shamt = shamt >> 6; 
	//funct was retrieved earlier so ....

        //Setting rs register
	d->regs.r.rs = rs; 
	//Setting rt register
	d->regs.r.rt = rt;
	//Setting rd register
	d->regs.r.rd = rd;
	//Setting shamt register
	d->regs.r.shamt = shamt; 
	//Setting funct register
	d->regs.r.funct = funct; 
        //Testing if decoded properly
	/*printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);*/

	//Need to get rVals updated
	rVals->R_rd = d->regs.r.rd;
	rVals->R_rs = d->regs.r.rs; 
	rVals->R_rt = d->regs.r.rt; 

        //printf("subu\n");

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


        return;
    }
    /*
      Using Mips Green Sheet, 
      I-format depends on the opcode
    */
    //I-format conditions
    else if(strcmp(o, "09") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15);
	/*unsigned first_bit = createMask(15,15);
	first_bit = first_bit & addr_or_immed;  
	printf("The first bit: %x\n", first_bit);*/ 
        addr_or_immed = addr_or_immed & instr; 
	
	//Sign extend to preserve value
	int32_t value = signExtension((int32_t)(int16_t)addr_or_immed);
	addr_or_immed = value;
	
        //printf("Addr or Immed:%x\n", (addr_or_immed)); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
	//Checking if the value is signed
	if(checkSigned(d->regs.i.addr_or_immed)){
	 	 
	   signed value = complement(d->regs.i.addr_or_immed);
	   //Update the immed value
	   d->regs.i.addr_or_immed = value;
	}
        //Testing if decoded properly
	//printf("%u %u %x\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	//printf("addiu\n");
	
	//Need to get rVals updated
	rVals->R_rd = d->regs.i.rt;
	rVals->R_rs = d->regs.i.rs; 
	rVals->R_rt = d->regs.i.addr_or_immed; 
	
	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);

        return;
    }
    else if(strcmp(o, "0c") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 

	//Sign extend to preserve value
	int32_t value = signExtension((int32_t)(int16_t)addr_or_immed);
	addr_or_immed = value;

        //printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
	//Checking if the value is signed
	if(checkSigned(d->regs.i.addr_or_immed)){
	 	 
	   signed value = complement(d->regs.i.addr_or_immed);
	   //Update the immed value
	   d->regs.i.addr_or_immed = value;
	}
        //Testing if decoded properly
	//printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	//printf("andi\n");

	//Need to get rVals updated
	rVals->R_rd = d->regs.i.rt;
	rVals->R_rs = d->regs.i.rs; 
	rVals->R_rt = d->regs.i.addr_or_immed; 

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


        return;
    }
    else if(strcmp(o, "04") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 

	//Extracting immediate or address from instruction
	signed addr_or_immed = createMask(0,15);
        addr_or_immed = addr_or_immed & instr; 
	
	//Sign extend to preserve value
	int32_t value = signExtension((int32_t)(int16_t)addr_or_immed);
	addr_or_immed = value;
	
	//Multiply branch target by 4
	addr_or_immed = addr_or_immed << 2;
	//Add value to PC+4;
	addr_or_immed = addr_or_immed + (mips.pc + 4);
        //printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	//printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	//printf("beq\n");

	//Need to get rVals updated
	rVals->R_rd = d->regs.i.rs;
	rVals->R_rs = d->regs.i.rt; 
	rVals->R_rt = d->regs.i.addr_or_immed; 
	
	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


        return;
    }
    else if(strcmp(o, "05") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	signed addr_or_immed = createMask(0,15);
        addr_or_immed = addr_or_immed & instr; 

	int32_t value = signExtension((int32_t)(int16_t)addr_or_immed);
	//Sign extend to preserve value
	addr_or_immed = value;
	//Multiply branch target by 4
	addr_or_immed = addr_or_immed << 2;
	//Add value to PC+4;
	addr_or_immed = addr_or_immed + (mips.pc + 4);


        //printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	//printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	//printf("bne\n");

	//Need to get rVals updated
	rVals->R_rd = d->regs.i.rs;
	rVals->R_rs = d->regs.i.rt; 
	rVals->R_rt = d->regs.i.addr_or_immed; 
	
	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


        return;
    }
    else if(strcmp(o, "0f") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
	
	//Sign extend to preserve value
	int32_t value = signExtension((int32_t)(int16_t)addr_or_immed);
	addr_or_immed = value;

        //printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
	//Checking if the value is signed
	if(checkSigned(d->regs.i.addr_or_immed)){
	 	 
	   signed value = complement(d->regs.i.addr_or_immed);
	   //Update the immed value
	   d->regs.i.addr_or_immed = value;
	}
        //Testing if decoded properly
	//printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	//printf("lui\n");
	
	//Need to get rVals updated
	rVals->R_rd = d->regs.i.rt;
	rVals->R_rs = d->regs.i.rs; 
	rVals->R_rt = d->regs.i.addr_or_immed; 	
	
	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


	return;
    }
    else if(strcmp(o, "23") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
	
	//Sign extend to preserve value
	int32_t value = signExtension((int32_t)(int16_t)addr_or_immed);
	
	addr_or_immed = value;
        //printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	//printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	//printf("lw\n");
	
	//Need to get rVals updated
	rVals->R_rd = d->regs.i.rt;
	rVals->R_rs = d->regs.i.rs; 
	rVals->R_rt = d->regs.i.addr_or_immed; 

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);


        return;
    }
    else if(strcmp(o, "0d") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 

	//Sign extend to preserve value
	int32_t value = signExtension((int32_t)(int16_t)addr_or_immed);
	addr_or_immed = value;
	
        //printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
	//Checking if the value is signed
	if(checkSigned(d->regs.i.addr_or_immed)){
	 	 
	   signed value = complement(d->regs.i.addr_or_immed);
	   //Update the immed value
	   d->regs.i.addr_or_immed = value;
	}
        //Testing if decoded properly
	//printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	//printf("ori\n");

	//Need to get rVals updated
	rVals->R_rd = d->regs.i.rt;
	rVals->R_rs = d->regs.i.rs; 
	rVals->R_rt = d->regs.i.addr_or_immed; 

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);

	
	
        return;
    }
   
    else if(strcmp(o, "2b") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        //printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        //printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 

	//Sign extend to preserve value
	int32_t value = signExtension((int32_t)(int16_t)addr_or_immed);
	
	addr_or_immed = value;
        //printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	//printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	//printf("sw\n");

	//Need to get rVals updated
	rVals->R_rd = d->regs.i.rt;
	rVals->R_rs = d->regs.i.rs; 
	rVals->R_rt = d->regs.i.addr_or_immed; 

	//printf("RegsVals: rd: %u rs:%u rt:%x\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);

        return;
    }
    /*
     Using Mips Green Sheet, 
     There are only 2-J format instructions
    */ 

    //J-Format
    else if(strcmp(o, "02") == 0){
       //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = J;
        //Extracting address from instruction 
	//Calculation comes from this source: https://stackoverflow.com/questions/6950230/how-to-calculate-jump-target-address-and-branch-target-address
        unsigned address = createMask(0, 25);
	address = address & instr; 
	//Multiply jump address by 4
	address = address << 2;
	//Extracting first four bits of pc
	unsigned first_four_bits = createMask(28, 31); 
	first_four_bits = first_four_bits & mips.pc; 
	//Concatenating the bits
	address = address | first_four_bits;
        printf("Address: %8.8x\n", address);
	//Setting target
	d->regs.j.target = address; 
	//printf("%u %8.8x\n", d->op, d->regs.j.target);
        //printf("j\n"); 
	
	//Need to get rVals updated
	rVals->R_rd = d->regs.j.target;

	//printf("RegsVals: rd: %8.8x\n", rVals->R_rd);

        return;
    }
    else if(strcmp(o, "03") == 0){
       //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = J;
        //Extracting address from instruction 
	//Calculation comes from this source: https://stackoverflow.com/questions/6950230/how-to-calculate-jump-target-address-and-branch-target-address
        unsigned address = createMask(0, 25);
	address = address & instr; 
	//Multiply jump address by 4
	address = address << 2;
	//Extracting first four bits of pc
	unsigned first_four_bits = createMask(28, 31); 
	first_four_bits = first_four_bits & mips.pc; 
	//Concatenating the bits
	address = address | first_four_bits;
        //printf("Address: %8.8x\n", address);
	//Setting target
	d->regs.j.target = address; 
	//printf("%u %8.8x\n", d->op, d->regs.j.target);
        //printf("jal\n");

	//Need to get rVals updated
	rVals->R_rd = d->regs.j.target;
	//jal jumps to another registes and saves the return dress, $31
	//This needs to be done in writeback, keep the purposes of the functions separated
	//mips.registers[31] = mips.pc; 

	//printf("RegsVals: rd: %8.8x\n", rVals->R_rd);

        return;
    }
    else{
	 d->type = NONE; //an unsupported instruction
    }
    
    //printf("Unsupported instruction\n");
    return; 
}

/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction ( DecodedInstr* d) {
    /* Your code goes here */
    char o[6]; 
    sprintf(o, "%2.2x", d->op);
    
    if(d->type == R){
      //All R-format instructions have an opcode with value 0
      //Therefore, only need to check the function
      char f[6]; 
      sprintf(f, "%2.2x", d->regs.r.funct);
      if(strcmp(f, "21") == 0){
	printf("addu $%u, $%u, $%u\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
	return;
      }
      if(strcmp(f, "24") == 0){
	printf("and $%u, $%u, $%u\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
	return;
      }
      if(strcmp(f, "08") == 0){
	printf("jr $%u\n", d->regs.r.rs);
	return;
      }
      if(strcmp(f, "25") == 0){
	printf("or $%u, $%u, $%u\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
	return;
      }
      if(strcmp(f, "2a") == 0){
	printf("slt $%u, $%u, $%u\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
	return;
      }
      if(strcmp(f, "00") == 0){
	printf("sll $%u, $%u, $%u\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
	return;
      }
      if(strcmp(f, "02") == 0){
	printf("srl $%u, $%u, $%u\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
	return;
      }
      if(strcmp(f, "23") == 0){
	printf("subu $%u, $%u, $%u\n", d->regs.r.rd, d->regs.r.rs, d->regs.r.rt);
	return;
      }	
     
    }
    else if(d->type == I){
      if(strcmp(o, "09") == 0){
	
	printf("addiu $%u, $%u, $%i\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
	return;
      }
      if(strcmp(o, "0c") == 0){
	
	printf("andi $%u, $%u, $%i\n", d->regs.i.rt, d->regs.i.rs, d->regs.i.addr_or_immed);
	return;
      }
      if(strcmp(o, "04") == 0){
	
	printf("beq $%u, $%u, $0x%8.8x\n", d->regs.i.rs, d->regs.i.rt, 
					d->regs.i.addr_or_immed);
	return;
      }
      if(strcmp(o, "05") == 0){
	
	printf("bne $%u, $%u, $0x%8.8x\n", d->regs.i.rs, d->regs.i.rt, 
					d->regs.i.addr_or_immed);
	return;
      }
      if(strcmp(o, "0f") == 0){
	
	printf("lui $%u, $%u, $0x%8.8x\n", d->regs.i.rt, d->regs.i.rs, 
					d->regs.i.addr_or_immed);
	return;
      }
      if(strcmp(o, "23") == 0){
	
	printf("lw $%u, $%u, $0x%8.8x\n", d->regs.i.rt, d->regs.i.rs, 
					d->regs.i.addr_or_immed);
	return;
      }
      if(strcmp(o, "0d") == 0){
	
	printf("ori $%u, $%u, $0x%8.8x\n", d->regs.i.rt, d->regs.i.rs, 
					d->regs.i.addr_or_immed);
	return;
      }
      if(strcmp(o, "2b") == 0){
	
	printf("sw $%u, $%u, $0x%8.8x\n", d->regs.i.rt, d->regs.i.rs, 
					d->regs.i.addr_or_immed);
	return;
      }
	
      return;
    }
    else if(d->type == J){

      if(strcmp(o, "02") == 0){
	printf("j 0x%8.8x\n", d->regs.j.target);
	return;
      }
      if(strcmp(o, "03") == 0){
	printf("jal 0x%8.8x\n", d->regs.j.target);
	return;
      }
      return;
    }
    else{
	//There's an unsupported instruction, so terminate as stated in the example
	exit(0);
	return;
    }
    
}

/* Perform computation needed to execute d, returning computed value */
int Execute ( DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */
    //this train of thought is most needed for execute
    char o[6]; 
    sprintf(o, "%2.2x", d->op);
    
    if(d->type == R){
      //All R-format instructions have an opcode with value 0
      //Therefore, only need to check the function
      char f[6]; 
      sprintf(f, "%2.2x", d->regs.r.funct);
      if(strcmp(f, "21") == 0){
	
	//Return the computed value
	//mips.registers[changedReg] = rVals->R_rs + rVals->R_rt; 
	int a = 0;
	int b = 0;
	a = rVals->R_rs;
	b = rVals->R_rt;
	
	
	////printf("Rs:%u, Rt:%u\n", rVals->R_rs, rVals->R_rt);
	
	return mips.registers[a] + mips.registers[b]; 
      }
      if(strcmp(f, "24") == 0){
	
	
	//Return the computed value
	int a = 0;
	int b = 0;
	a = rVals->R_rs;
	b = rVals->R_rt;
	////printf("Rs:%u, Rt:%u\n", rVals->R_rs, rVals->R_rt);
	return mips.registers[a] & mips.registers[b]; 
      }
      if(strcmp(f, "08") == 0){
	/*Not sure if this needs to be updated, but the pc will get updated to whatever 
	  source*/
	/*No, doesn't need to be updated*, just need to return the value stored in 
	  the register at R_rs*/
	//The Value returned is the source or address
	int a = 0;
	
	a = rVals->R_rs;
	
	////printf("Rs:%u, Rt:%u\n", rVals->R_rs, rVals->R_rt);
	return mips.registers[31];  
      }
      if(strcmp(f, "25") == 0){
	//printf("Rs:%u, Rt:%u\n", rVals->R_rs, rVals->R_rt);
	return mips.registers[rVals->R_rs] | mips.registers[rVals->R_rt];  
      }
      if(strcmp(f, "29") == 0){
	int a = 0;
	int b = 0;
	a = rVals->R_rs;
	b = rVals->R_rt;
	//printf("Rs:%u, Rt:%u\n", rVals->R_rs, rVals->R_rt);
	return (mips.registers[a] < mips.registers[b] ? 1 : 0); 
      }
      if(strcmp(f, "00") == 0){
	int a = 0;
	int b = 0;
	a = rVals->R_rs;
	b = rVals->R_rt;
	//printf("Rs:%u, Rt:%u\n", rVals->R_rs, rVals->R_rt);
	return mips.registers[a] << mips.registers[b]; 
      }
      if(strcmp(f, "02") == 0){
	int a = 0;
	int b = 0;
	a = rVals->R_rs;
	b = rVals->R_rt;
	//printf("Rs:%u, Rt:%u\n", rVals->R_rs, rVals->R_rt);
	return mips.registers[a] >> mips.registers[b];
      }	
      if(strcmp(f, "23") == 0){
	int a = 0;
	int b = 0;
	a = rVals->R_rs;
	b = rVals->R_rt;
	//printf("Rs:%u, Rt:%u\n", rVals->R_rs, rVals->R_rt);
	return mips.registers[a] - mips.registers[b];
      }		
     
    }
    else if(d->type == I){
       if(strcmp(o, "09") == 0){
	 int a = 0;
	 int imm = 0;
	 a = rVals->R_rd;
	 imm = rVals->R_rt;
	 //printf("Rs:%i, Rt:%i\n", rVals->R_rs, rVals->R_rt); 
	 return mips.registers[a] +imm;
	}
	if(strcmp(o, "0c") == 0){
	 int a = 0;
	 int imm = 0;
	 a = rVals->R_rd;
	 imm = rVals->R_rt;
	 //printf("Rs:%i, Rt:%i", rVals->R_rs, rVals->R_rt);
	 return mips.registers[a] & imm; 
	}
	if(strcmp(o, "04") == 0){
	 int a = 0;
	 int b = 0;
	 a = rVals->R_rd;
	 b = rVals->R_rs;
	 if((mips.registers[a] - mips.registers[b] == 0))
	 {
		 //printf("Rd:%i, Rs:%i, result:%i\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);
		return rVals->R_rt;
	 }
	 else{
		// printf("Rd:%i, Rs:%i, result:%i\n", rVals->R_rd, rVals->R_rs, mips.pc);
		return (mips.pc + 4);
	 }
	 
	
	
	}
	if(strcmp(o, "05") == 0){
	 int a = 0;
	 int b = 0;
	 a = rVals->R_rd;
	 b = rVals->R_rs;
	 
	 if((mips.registers[a] - mips.registers[b] != 0))
	 {
		 //printf("Rd:%i, Rs:%i, result:%i\n", rVals->R_rd, rVals->R_rs, rVals->R_rt);
		return rVals->R_rt;
	 }
	 else{
		// printf("Rd:%i, Rs:%i, result:%i\n", rVals->R_rd, rVals->R_rs, mips.pc);
		return mips.pc;
	 }
	}
	if(strcmp(o, "0f") == 0){
	
	 //printf("Rs:%i, Rt:%i\n", rVals->R_rs, rVals->R_rt);
	 return rVals->R_rt << 16; 
	}
	if(strcmp(o, "23") == 0){
	 int pointer = 0;
	 int offset = 0;
	 pointer = rVals->R_rs;
	 offset = rVals->R_rt;
	 //printf("Rs:%i, Rt:%i\n", rVals->R_rs, rVals->R_rt);
	 return mips.registers[pointer] + offset; 
	}
	if(strcmp(o, "0d") == 0){
	 int a = 0;
	 int b = 0;
	 a = rVals->R_rs;
	 b = rVals->R_rt;
	 //printf("Rs:%i, Rt:%i\n", rVals->R_rs, rVals->R_rt);
	 return mips.registers[a] | b;
	}
	if(strcmp(o, "2b") == 0){
	 int pointer = 0;
	 int offset = 0;
	 //pointer stores an address 
	 pointer = rVals->R_rs;
	 offset = rVals->R_rt;

	 //printf("Address: %8.8x\n", mips.registers[pointer]);
	 
	 //printf("Rs:%i, Rt:%i\n", rVals->R_rs, rVals->R_rt);
	 return mips.registers[pointer] + offset; 
	}

    }
    else if(d->type == J){
		// j
      if(strcmp(o, "02") == 0  || d->op == 0x02){
		//return the target address
		//printf("%8.8x\n", rVals->R_rd);
		return rVals->R_rd;
      }
	  //jal
      if(d->op == 0x03 || strcmp(o, "03") == 0){
		//update return address
		mips.registers[31] = mips.pc;
		// printf("Register 31 during execute%8.8x\n", mips.registers[31]);
		//return the target address
		//printf("%8.8x\n", rVals->R_rd);
		return rVals->R_rd;
      }
    }
  return 0;
}

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC ( DecodedInstr* d, int val) {
    
    /* Your code goes here */
    char o[6];
    char f[6];
    sprintf(o, "%2.2x", d->op); 
    sprintf(f, "%2.2x", d->regs.r.funct);

	//printf("%s\n", o);
    if(strcmp(o, "04") == 0 || d->op == 0x04){
	//beq
	//printf("Rs:%i, Rt:%i", rVals->R_rs, rVals->R_rt);
	mips.pc = val; 
	return;
    }
    else if(strcmp(o, "05") == 0 || d->op == 0x05){
	//bne
	//printf("pc = %8.8x", val);
	mips.pc = val + 4; 
	return;
    }
    else if(strcmp(o, "02") == 0 || d->op == 0x02){
	//j
	//printf("Rs:%i, Rt:%i", rVals->R_rs, rVals->R_rt);
	mips.pc = val; 
	return;
    }
    else if(strcmp(o, "03") == 0 || d->op == 0x03){
	//jal
	//printf("Rs:%i, Rt:%i", rVals->R_rs, rVals->R_rt);
	//printf("hello %d\n", val);
	//mips.registers[31] = mips.pc;
	mips.pc = val;
	return;
    }
    else if(strcmp(o, "00") == 0 && strcmp(f, "08") == 0 || d->op == 0x00 && d->regs.r.funct == 0x08){
	//jr
	//printf("Rs:%i, Rt:%i", rVals->R_rs, rVals->R_rt);
	//printf("r31 is %8.8x\n", val); 
	mips.pc = val; 

	return;
    }
    else{
	mips.pc+=4;
	return;
    }
    
   
}

/*
 * Perform memory load or store. Place the address of any updated memory 
 * in *changedMem, otherwise put -1 in *changedMem. Return any memory value 
 * that is read, otherwise return -1. 
 *
 * Remember that we're mapping MIPS addresses to indices in the mips.memory 
 * array. mips.memory[0] corresponds with address 0x00400000, mips.memory[1] 
 * with address 0x00400004, and so forth.
 *
 */
int Mem( DecodedInstr* d, int val, int *changedMem) {
    /* Your code goes here */

  //lw
  if(d->op == 0x23){
	//just need to return the value from memory. 
	//val is the calculated index
	//lw doesn't update any values in memory, therefore changedMem is not updated
	*changedMem = -1;
	int index = (val - 0x00400000)/4;
	printf("%d\n", index);
	return mips.memory[index];
  }
  //sw
  if(d->op == 0x2b){
	
	//val is the calculated index
	//sw updates values in memory, therefore changedMem is updated
	//stores the value found in the specified mips register
	//returns 0 because sw doesn't update any registers; it updates memory
	//Just need to increment the location of mips.memory with val

	//*changedMem = (val - 0x00401000)/4;
	*changedMem = val;
	//printf("CHANGED MEM%8.8x", *changedMem);
	if(*changedMem >= 0x00400000 && *changedMem <= 0x00404000){
		//printf("Memory location of mips.memory: %8.8x, Memory location of changedMem: %8.8x\n", 0x00401000, *changedMem);
		int index = (*changedMem - 0x00400000)/4;
		//printf("val: %i\n", index);
		mips.memory[index] = mips.registers[d->regs.i.rt];
	}
	else{
		*changedMem = -1;
	}
	
	return 0; 
  }
 
  //if no MEM cycle is needed, just return the val
  *changedMem = -1;
  return val;
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite( DecodedInstr* d, int val, int *changedReg) {
    /* Your code goes here */
   //Checks if the format is R
   if(d->type == R){
	//If so, we know the opcode is, thus we just need to check the value of funct
	if(d->regs.r.funct == 0x21){
		//addu
		*changedReg = d->regs.r.rd;
		mips.registers[*changedReg] = val;
		return;
	}
	if(d->regs.r.funct == 0x24){
		//and
		*changedReg = d->regs.r.rd;
		mips.registers[*changedReg] = val;
		return;
	}
	if(d->regs.r.funct == 0x25){
		//or
		*changedReg = d->regs.r.rd;
		mips.registers[*changedReg] = val;
		return;
	}
	if(d->regs.r.funct == 0x2a){
		//slt
		*changedReg = d->regs.r.rd;
		mips.registers[*changedReg] = val;
		return;
	}
	if(d->regs.r.funct == 0x00){
		//sll
		*changedReg = d->regs.r.rd;
		mips.registers[*changedReg] = val;
		return;
	}
	if(d->regs.r.funct == 0x02){
		//srl
		*changedReg = d->regs.r.rd;
		mips.registers[*changedReg] = val;
		return;
	}
	if(d->regs.r.funct == 0x23){
		//subu
		*changedReg = d->regs.r.rd;
		mips.registers[*changedReg] = val;
		return;
	}
	
	//jr was executed, and doesn't update any registers
	*changedReg = -1;
	return;
	
   }
   //Checking if the format is I-format
   if(d->type == I){
	//If so, we just need to check the opcode
	//Not all I-format instructions writeback to a register
	if(d->op == 0x09){
		*changedReg = d->regs.i.rt;
		//printf("val: %i\n", val);
		mips.registers[*changedReg] = val;
		return; 
	}
	if(d->op == 0x0c){
		*changedReg = d->regs.i.rt;
		mips.registers[*changedReg] = val;
		return; 
	}
	if(d->op == 0x0f){
		*changedReg = d->regs.i.rt;
		mips.registers[*changedReg] = val;
		return; 
	}
	if(d->op == 0x23){
		*changedReg = d->regs.i.rt;
		mips.registers[*changedReg] = val;
		return; 
	}
	if(d->op == 0x0d){
		*changedReg = d->regs.i.rt;
		mips.registers[*changedReg] = val;
		return; 
	}
	if(d->op == 0x23){
		*changedReg = d->regs.i.rt;
		mips.registers[*changedReg] = val;
		return;
	}
	//executed beq, bne, or sw
	*changedReg = -1;
	return;
   }
   if(d->type == J){
	//If there is a jump, no register is changed
	//printf("Opcode %x", d->op);
	if(d->op == 0x03){
		*changedReg = 31;
		//printf("R31 %8.8x\n", mips.registers[*changedReg]);
		mips.registers[*changedReg] = mips.registers[*changedReg] + 4;
		return;
	}
	*changedReg = -1;
	return;
   }
}
