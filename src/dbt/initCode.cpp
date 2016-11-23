#include <lib/ac_int.h>
#include <lib/endianness.h>
#include <isa/vexISA.h>

#define JUMP_RESOLVE 4


#ifdef __NIOS
unsigned int getInitCode(unsigned int *binaries, int start){
#endif

#ifdef __USE_AC
unsigned int getInitCode(ac_int<128, false> *binaries, int start){
#endif

	int cycle = start;
	writeInt(binaries, cycle*16+0, assembleIInstruction(VEX_CALL, 0, 0));
	writeInt(binaries, cycle*16+4, 0); //OLD : assembleIInstruction(VEX_MOVI,0xffff,29)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, 0);
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleIInstruction(VEX_STOP, 0, 0));
	writeInt(binaries, cycle*16+4, 0);		//STOP
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, 0);
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleRiInstruction(VEX_ADDi, 4, 0, 0x700)); 	//r4 = 0x700
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_STW, 4, 29, 0));		//stw r4 0(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleRiInstruction(VEX_SLLi, 4, 4, 16));		//r4 = r4 << 16
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_STW, 5, 29, 4));		//stw r5 4(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, assembleIInstruction(VEX_MOVI, 0x28008, 5));	//r5 = 0xa0025 FIXME param

	cycle++;
	writeInt(binaries, cycle*16+0, assembleRiInstruction(VEX_SLLi, 5, 5, 14));		//r5 = r5 << 14
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_STW, 6, 29, 8));		//stw r6 8(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleRiInstruction(VEX_ADDi, 5, 5, 0x40));	//r5 = r5 + 0x40 -> r5 = 0xa0020040
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_STW, 7, 29, 12));		//stw r7 12(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleRiInstruction(VEX_ADDi, 4, 4, 4));		//r4 = r4 + 4 = start
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_STW, 8, 29, 16));		//stw r8 16(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, assembleRInstruction(VEX_SUB, 33, 33, 5));		//r33 = r33 - r5 (=0xa0020040)

	cycle++;
	writeInt(binaries, cycle*16+0, assembleRiInstruction(VEX_SRAi, 33, 33, 2));		//r33 = r33 / 4
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_LDW, 5, 4, -4));		//r5 = ldw -4(r4) = size
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleRiInstruction(VEX_SLLi, 5, 5, 2));		//r5 = r5 * 4
	writeInt(binaries, cycle*16+4, assembleRInstruction(VEX_ADD, 6, 0, 33));		//r6 = r33
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	int instrWithPlaceCode = cycle;
	writeInt(binaries, cycle*16+0, assembleRInstruction(VEX_ADD, 5, 5, 4));			//r5 = r5 + r4 = end
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	//bcl
	cycle++;
	int bcl = cycle;

	writeInt(binaries, cycle*16+0, assembleRInstruction(VEX_CMPEQ, 7, 4, 5));
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleIInstruction(VEX_BR, 8, 7));
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, 0);
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, 0);
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_LDW, 8, 4, 0));
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleRInstruction(VEX_CMPGT, 7, 8, 33));
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleIInstruction(VEX_BR, 4, 7));
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, 0);
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleIInstruction(VEX_GOTO, bcl, 0));
	writeInt(binaries, cycle*16+4, 0);
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleRiInstruction(VEX_ADDi, 33, 33, 1));
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_ADDi, 4, 4, 4));
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	//finBcl
	cycle++;
	writeInt(binaries, cycle*16+0, assembleRiInstruction(VEX_ADDi, 33, 33, 27));
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_LDW, 4, 29, 0));		//ldw r4 0(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, 0);
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_LDW, 5, 29, 4));		//ldw r5 4(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, 0);
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_LDW, 6, 29, 8));		//ldw r6 8(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, assembleIInstruction(VEX_IGOTO, 0, 33));
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_LDW, 7, 29, 12));		//ldw r7 12(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);

	cycle++;
	writeInt(binaries, cycle*16+0, 0);
	writeInt(binaries, cycle*16+4, assembleRiInstruction(VEX_LDW, 8, 29, 16));		//ldw r8 16(sp)
	writeInt(binaries, cycle*16+8, 0);
	writeInt(binaries, cycle*16+12, 0);
	cycle++;

	writeInt(binaries, instrWithPlaceCode*16+4, assembleRiInstruction(VEX_ADDi, 33, 33, 0));
	writeInt(binaries, start*16+0, assembleIInstruction(VEX_CALL, cycle, 0));


	return cycle;

}