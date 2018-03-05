#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "computer.h"
#include "string.h"
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
    sprintf(o, "%x", result);
    sprintf(f, "%x", funct);
    printf("The instruction opcode value is %s\n", o);
    printf("The funct/immediate value is %s\n", f);

    /*
     Using Mips Green Sheet for opcode and funct
     All R formats have 0 opcode, but different funct values
    */

    //R-format conditions
    if(strcmp(o, "0") == 0 && strcmp(f, "20") == 0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
 	printf("add\n");
        return; 
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "21") == 0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("addu\n");
        return; 
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "24") == 0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("and\n");
        return; 
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "08") == 0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("jr\n");
        return;
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "27") == 0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("nor\n");
        return;
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "25")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("or\n");
        return;
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "2a")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("slt\n");
        return;
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "2b")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("sltu\n");
        return;
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "00")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("sll\n");
        return;
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "02")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("srl\n");
        return;
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "22")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("sub\n");
        return;
    }
    if(strcmp(o, "0") == 0 && strcmp(f, "23")==0){
	//Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = R;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned rd = createMask(11,15); 
        rd = rd & instr; 
	rd = rd >> 11; 
        printf("Rd:%u\n", rd); 
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
	printf("%u %u %u %u %u\n", d->regs.r.rs, d->regs.r.rt, 
				d->regs.r.rd, d->regs.r.shamt, d->regs.r.funct);
        printf("subu\n");
        return;
    }
    /*
      Using Mips Green Sheet, 
      I-format depends on the opcode
    */
    //I-format conditions
    if(strcmp(o, "8") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
	d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("addi\n");
        return;
    }
    if(strcmp(o, "9") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("addiu\n");
        return;
    }
    if(strcmp(o, "c") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("andi\n");
        return;
    }
    if(strcmp(o, "4") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("beq\n");
        return;
    }
    if(strcmp(o, "5") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("bne\n");
        return;
    }
    if(strcmp(o, "24") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("lbu\n");
        return;
    }
    if(strcmp(o, "25") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("lhu\n");
        return;
    }
    if(strcmp(o, "30") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("ll\n");
        return;
    }
    if(strcmp(o, "f") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("lui\n");
    }
    if(strcmp(o, "23") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("lw\n");
        return;
    }
    if(strcmp(o, "d") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("ori\n");
        return;
    }
    if(strcmp(o, "a") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("slti\n");
        return;
    }
    if(strcmp(o, "b") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("sltiu\n");
        return;
    }
    if(strcmp(o, "28") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("sb\n");
        return;
    }
    if(strcmp(o, "38") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
       	d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("sc\n");
        return;
    }
    if(strcmp(o, "29") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("sh\n");
        return;
    }
    if(strcmp(o, "2b") == 0){
        //Setting the opcode
 	d->op = result;
	//Setting the Instruction Type
        d->type = I;
        //Extracting rs from instruction 
        unsigned rs = createMask(21, 25); 
	rs = rs & instr; 
        rs = rs >> 21; 
        printf("Rs:%u\n", rs);
	//Extracting rt from instruction
	unsigned rt = createMask(16, 20); 
        rt = rt & instr; 
        rt = rt >> 16; 
        printf("Rt:%u\n", rt); 
	//Extracting immediate or address from instruction
	unsigned addr_or_immed = createMask(0,15); 
        addr_or_immed = addr_or_immed & instr; 
        printf("Addr or Immed:%u\n", addr_or_immed); 
        //Setting rs register
	d->regs.i.rs = rs; 
	//Setting rt register
	d->regs.i.rt = rt;
	//Setting immediate value
	d->regs.i.addr_or_immed = addr_or_immed;
        //Testing if decoded properly
	printf("%u %u %u\n", d->regs.i.rs, d->regs.i.rt, d->regs.i.addr_or_immed);
 	printf("sw\n");
        return;
    }
    /*
     Using Mips Green Sheet, 
     There are only 2-J format instructions
    */ 

    //J-Format
    if(strcmp(o, "2") == 0){
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
	printf("%u %8.8x\n", d->op, d->regs.j.target);
        printf("j\n"); 
        return;
    }
    if(strcmp(o, "3") == 0){
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
	printf("%u %8.8x\n", d->op, d->regs.j.target);
        printf("jal\n");
        return;
    }
    return; 
}

/*
 *  Print the disassembled version of the given instruction
 *  followed by a newline.
 */
void PrintInstruction ( DecodedInstr* d) {
    /* Your code goes here */
}

/* Perform computation needed to execute d, returning computed value */
int Execute ( DecodedInstr* d, RegVals* rVals) {
    /* Your code goes here */
  return 0;
}

/* 
 * Update the program counter based on the current instruction. For
 * instructions other than branches and jumps, for example, the PC
 * increments by 4 (which we have provided).
 */
void UpdatePC ( DecodedInstr* d, int val) {
    mips.pc+=4;
    /* Your code goes here */
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
  return 0;
}

/* 
 * Write back to register. If the instruction modified a register--
 * (including jal, which modifies $ra) --
 * put the index of the modified register in *changedReg,
 * otherwise put -1 in *changedReg.
 */
void RegWrite( DecodedInstr* d, int val, int *changedReg) {
    /* Your code goes here */
}
