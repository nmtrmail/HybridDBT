/*
 * riscvSimulator.cpp
 *
 *  Created on: 2 déc. 2016
 *      Author: Simon Rokicki
 */


#include <isa/riscvISA.h>
#include <simulator/riscvSimulator.h>

#include <types.h>

const unsigned int shiftMask[32] = { 0xffffffff, 0x7fffffff, 0x3fffffff, 0x1fffffff,
							0xfffffff, 	0x7ffffff, 	0x3ffffff, 	0x1ffffff,
							0xffffff, 	0x7fffff, 	0x3fffff, 	0x1fffff,
							0xfffff, 	0x7ffff, 	0x3ffff, 	0x1ffff,
							0xffff, 	0x7fff, 	0x3fff, 	0x1fff,
							0xfff, 		0x7ff, 		0x3ff, 		0x1ff,
							0xff, 		0x7f, 		0x3f, 		0x1f,
							0xf, 		0x7, 		0x3, 		0x1};

//Mips simulator
int RiscvSimulator::doSimulation(int start){
	long long hilo;
	ac_int<64, true> reg[32];

	ac_int<64, true> pc = start;


	ac_int<32, 0> ins, next_instr;

	while (1){
		int i;
		int n_inst;
		unsigned int ivalue;
		float fvalue;



		//We initialize instruction counter
		n_inst = 0;

		//We initialize registers
		for (i = 0; i < 32; i++)
			reg[i] = 0;
		reg[2] = 0x70000;




		do{
			ins = this->ldw(pc);;
			fprintf(stderr,"%d;%x;%x", (int)n_inst, (int)pc, (int) ins);
			std::cerr << printDecodedInstrRISCV(ins);

			pc = pc + 4;

			//Applying xor to every fields of the function
			ac_int<7, false> opcode = ins.slc<7>(0);
			ac_int<5, false> rs1 = ins.slc<5>(15);
			ac_int<5, false> rs2 = ins.slc<5>(20);
			ac_int<5, false> rd = ins.slc<5>(7);
			ac_int<7, false> funct7 = ins.slc<7>(25);
			ac_int<3, false> funct3 = ins.slc<3>(12);
			ac_int<12, false> imm12_I = ins.slc<12>(20);
			ac_int<12, false> imm12_S = 0;
			imm12_S.set_slc(5, ins.slc<7>(25));
			imm12_S.set_slc(0, ins.slc<5>(7));

			ac_int<12, true> imm12_I_signed = ins.slc<12>(20);
			ac_int<12, true> imm12_S_signed = 0;
			imm12_S_signed.set_slc(0, imm12_S.slc<12>(0));


			ac_int<13, false> imm13 = 0;
			imm13[12] = ins[31];
			imm13.set_slc(5, ins.slc<6>(25));
			imm13.set_slc(1, ins.slc<4>(8));
			imm13[11] = ins[7];

			ac_int<13, true> imm13_signed = 0;
			imm13_signed.set_slc(0, imm13);

			ac_int<32, true> imm31_12 = 0;
			imm31_12.set_slc(12, ins.slc<20>(12));

			ac_int<21, false> imm21_1 = 0;
			imm21_1.set_slc(12, ins.slc<8>(12));
			imm21_1[11] = ins[20];
			imm21_1.set_slc(1, ins.slc<10>(21));
			imm21_1[20] = ins[31];

			ac_int<21, true> imm21_1_signed = 0;
			imm21_1_signed.set_slc(0, imm21_1);

			ac_int<6, false> shamt = ins.slc<6>(20);


			ac_int<64, false> unsignedReg1 = 0;
			ac_int<64, false> unsignedReg2 = 0;

			ac_int<64, true> valueRegA;

			ac_int<128, true> longResult;


			ac_int<32, true> localDataa, localDatab;
			ac_int<64, true> localLongResult;
			ac_int<32, false> localDataaUnsigned, localDatabUnsigned;
			ac_int<32, true> localResult;

			switch (opcode)
			{
			case RISCV_LUI:
				reg[rd] = imm31_12;
			break;
			case RISCV_AUIPC:
				reg[rd] = pc -4 + imm31_12;
			break;
			case RISCV_JAL:
				reg[rd] = pc;
				pc = pc - 4 + imm21_1_signed;
			break;
			case RISCV_JALR:
				reg[rd] = pc;
				pc = (reg[rs1] + imm12_I_signed) & 0xfffffffe;
			break;

			//******************************************************************************************
			//Treatment for: BRANCH INSTRUCTIONS
			case RISCV_BR:
				switch(funct3)
				{
				case RISCV_BR_BEQ:
					if (reg[rs1] == reg[rs2])
						pc = pc + (imm13_signed) - 4;
				break;
				case RISCV_BR_BNE:
					if (reg[rs1] != reg[rs2])
						pc = pc + (imm13_signed) - 4;
				break;
				case RISCV_BR_BLT:
					if (reg[rs1] < reg[rs2])
						pc = pc + (imm13_signed) - 4;
				break;
				case RISCV_BR_BGE:
					if (reg[rs1] >= reg[rs2])
						pc = pc + (imm13_signed) - 4;
				break;
				case RISCV_BR_BLTU:
					unsignedReg1.set_slc(0, reg[rs1].slc<32>(0));
					unsignedReg2.set_slc(0, reg[rs2].slc<32>(0));

					if (unsignedReg1 < unsignedReg2)
						pc = pc + (imm13_signed) - 4;
				break;
				case RISCV_BR_BGEU:
					unsignedReg1.set_slc(0, reg[rs1].slc<32>(0));
					unsignedReg2.set_slc(0, reg[rs2].slc<32>(0));

					if (unsignedReg1 >= unsignedReg2)
						pc = pc + (imm13_signed) - 4;
				break;
				default:
					printf("In BR switch case, this should never happen... Instr was %x\n", (int)ins);
				break;
				}
			break;

			//******************************************************************************************
			//Treatment for: LOAD INSTRUCTIONS
			case RISCV_LD:
				switch(funct3)
				{
				case RISCV_LD_LB:
					reg[rd] = this->ldb(reg[rs1] + imm12_I_signed);
				break;
				case RISCV_LD_LH:
					reg[rd] = this->ldh(reg[rs1] + imm12_I_signed);
				break;
				case RISCV_LD_LW:
					reg[rd] = this->ldw(reg[rs1] + imm12_I_signed);
				break;
				case RISCV_LD_LD:
					reg[rd] = this->ldd(reg[rs1] + imm12_I_signed);
				break;
				case RISCV_LD_LBU:
					valueRegA = reg[rs1];
					reg[rd] = 0;
					reg[rd].set_slc(0, this->ldb(valueRegA + imm12_I_signed));
				break;
				case RISCV_LD_LHU:
					valueRegA = reg[rs1];
					reg[rd] = 0;
					reg[rd].set_slc(0, this->ldh(valueRegA + imm12_I_signed));
				break;
				case RISCV_LD_LWU:
					valueRegA = reg[rs1];
					reg[rd] = 0;
					reg[rd].set_slc(0, this->ldw(valueRegA + imm12_I_signed));
				break;
				default:
					printf("In LD switch case, this should never happen... Instr was %x\n", (int)ins);
				break;
				}
			break;

			//******************************************************************************************
			//Treatment for: STORE INSTRUCTIONS
			case RISCV_ST:
				switch(funct3)
				{
				case RISCV_ST_STB:
					this->stb(reg[rs1] + imm12_S_signed, reg[rs2]);
				break;
				case RISCV_ST_STH:
					this->sth(reg[rs1] + imm12_S_signed, reg[rs2]);
				break;
				case RISCV_ST_STW:
					this->stw(reg[rs1] + imm12_S_signed, reg[rs2]);
				break;
				case RISCV_ST_STD:
					this->std(reg[rs1] + imm12_S_signed, reg[rs2]);
				break;
				default:
					printf("In ST switch case, this should never happen... Instr was %x\n", (int)ins);
				break;
				}
			break;

			//******************************************************************************************
			//Treatment for: OPI INSTRUCTIONS
			case RISCV_OPI:
				switch(funct3)
				{
				case RISCV_OPI_ADDI:
					reg[rd] = reg[rs1] + imm12_I_signed;
				break;
				case RISCV_OPI_SLTI:
					reg[rd] = (reg[rs1] < imm12_I_signed) ? 1 : 0;
				break;
				case RISCV_OPI_SLTIU:
					unsignedReg1.set_slc(0, reg[rs1].slc<32>(0));

					reg[rd] = (unsignedReg1 < imm12_I) ? 1 : 0;
				break;
				case RISCV_OPI_XORI:
					reg[rd] = reg[rs1] ^ imm12_I_signed;
				break;
				case RISCV_OPI_ORI:
					reg[rd] = reg[rs1] | imm12_I_signed;
				break;
				case RISCV_OPI_ANDI:
					reg[rd] = reg[rs1] & imm12_I_signed;
				break;
				case RISCV_OPI_SLLI:
					reg[rd] = reg[rs1] << shamt;
				break;
				case RISCV_OPI_SRI:
					if (funct7 == RISCV_OPI_SRI_SRLI)
						reg[rd] = (reg[rs1] >> shamt) & shiftMask[shamt];
					else //SRAI
						reg[rd] = reg[rs1] >> shamt;
				break;
				default:
					printf("In OPI switch case, this should never happen... Instr was %x\n", (int)ins);
				break;
				}
			break;

			//******************************************************************************************
			//Treatment for: OPIW INSTRUCTIONS
			case RISCV_OPIW:
				localDataa = reg[rs1];
				localDatab = imm12_I_signed;
				switch(funct3)
				{
				case RISCV_OPIW_ADDIW:
					localResult = localDataa + localDatab;
					reg[rd] = localResult;
				break;
				case RISCV_OPIW_SLLIW:
					localResult = localDataa << rs2;
					reg[rd] = localResult;
				break;
				case RISCV_OPIW_SRW:
					if (funct7 == RISCV_OPIW_SRW_SRLIW)
						localResult = (localDataa >> rs2) & shiftMask[rs2];
					else //SRAI
						localResult = localDataa >> rs2;

					reg[rd] = localResult;
				break;
				default:
					printf("In OPI switch case, this should never happen... Instr was %x\n", (int)ins);
				break;
				}
			break;

			//******************************************************************************************
			//Treatment for: OP INSTRUCTIONS
			case RISCV_OP:
				if (funct7 == 1){
					//Switch case for multiplication operations (in standard extension RV32M)
					switch (funct3)
					{
					case RISCV_OP_M_MUL:
						longResult = reg[rs1] * reg[rs2];
						reg[rd] = longResult.slc<64>(0);
					break;
					case RISCV_OP_M_MULH:
						longResult = reg[rs1] * reg[rs2];
						reg[rd] = longResult.slc<64>(64);
					break;
					case RISCV_OP_M_MULHSU:
						unsignedReg2 = reg[rs2];
						longResult = reg[rs1] * unsignedReg2;
						reg[rd] = longResult.slc<64>(64);
					break;
					case RISCV_OP_M_MULHU:
						unsignedReg1 = reg[rs1];
						unsignedReg2 = reg[rs2];
						longResult = unsignedReg1 * unsignedReg2;
						reg[rd] = longResult.slc<64>(64);
					break;
					case RISCV_OP_M_DIV:
						reg[rd] = (reg[rs1] / reg[rs2]);
					break;
					case RISCV_OP_M_DIVU:
						unsignedReg1 = reg[rs1];
						unsignedReg2 = reg[rs2];
						reg[rd] = unsignedReg1 / unsignedReg2;
					break;
					case RISCV_OP_M_REM:
						reg[rd] = (reg[rs1] % reg[rs2]);
					break;
					case RISCV_OP_M_REMU:
						unsignedReg1 = reg[rs1];
						unsignedReg2 = reg[rs2];
						reg[rd] = unsignedReg1 % unsignedReg2;
					break;
					}

				}
				else{

					//Switch case for base OP operation
					switch(funct3)
					{
					case RISCV_OP_ADD:
						if (funct7 == RISCV_OP_ADD_ADD)
							reg[rd] = reg[rs1] + reg[rs2];
						else
							reg[rd] = reg[rs1] - reg[rs2];
					break;
					case RISCV_OP_SLL:
						reg[rd] = reg[rs1] << (reg[rs2] & 0x3f);
					break;
					case RISCV_OP_SLT:
						reg[rd] = (reg[rs1] < reg[rs2]) ? 1 : 0;
					break;
					case RISCV_OP_SLTU:
						unsignedReg1.set_slc(0, reg[rs1].slc<32>(0));
						unsignedReg2.set_slc(0, reg[rs2].slc<32>(0));

						reg[rd] = (unsignedReg1 < unsignedReg2) ? 1 : 0;
					break;
					case RISCV_OP_XOR:
						reg[rd] = reg[rs1] ^ reg[rs2];
					break;
					case RISCV_OP_SR:
						if (funct7 == RISCV_OP_SR_SRL)
							reg[rd] = (reg[rs1] >> (reg[rs2] & 0x3f)) & shiftMask[reg[rs2] & 0x3f];
						else //SRA
							reg[rd] = reg[rs1] >> (reg[rs2] & 0x3f);
					break;
					case RISCV_OP_OR:
						reg[rd] = reg[rs1] | reg[rs2];
					break;
					case RISCV_OP_AND:
						reg[rd] = reg[rs1] & reg[rs2];
					break;
					default:
						printf("In OP switch case, this should never happen... Instr was %x\n", (int)ins);
					break;
					}
				}
				break;

			//******************************************************************************************
			//Treatment for: OPW INSTRUCTIONS
			case RISCV_OPW:
				if (funct7 == 1){
					localDataa = reg[rs1].slc<32>(0);
					localDataa = reg[rs2].slc<32>(0);
					localDataaUnsigned = reg[rs1].slc<32>(0);
					localDataaUnsigned = reg[rs2].slc<32>(0);

					//Switch case for multiplication operations (in standard extension RV32M)
					switch (funct3)
					{
					case RISCV_OPW_M_MULW:
						localLongResult = localDataa * localDatab;
						reg[rd] = localLongResult.slc<32>(0);
					break;
					case RISCV_OPW_M_DIVW:
						reg[rd] = (localDataa / localDatab);
					break;
					case RISCV_OPW_M_DIVUW:
						reg[rd] = localDataaUnsigned / localDatabUnsigned;
					break;
					case RISCV_OPW_M_REMW:
						reg[rd] = (localDataa % localDatab);
					break;
					case RISCV_OPW_M_REMUW:
						reg[rd] = localDataaUnsigned % localDatabUnsigned;
					break;
					}

				}
				else{
					localDataa = reg[rs1].slc<32>(0);
					localDataa = reg[rs2].slc<32>(0);

					//Switch case for base OP operation
					switch(funct3)
					{
					case RISCV_OPW_ADDSUBW:
						if (funct7 == RISCV_OPW_ADDSUBW_ADDW){
							localResult = localDataa + localDatab;
							reg[rd] = localResult;
						}
						else{ //SUBW
							localResult = localDataa - localDatab;
							reg[rd] = localResult;
						}
					break;
					case RISCV_OPW_SLLW:
						localResult = localDataa << (localDatab & 0x1f);
						reg[rd] = localResult;
					break;
					case RISCV_OPW_SRW:
						if (funct7 == RISCV_OPW_SRW_SRLW){
							localResult = (localDataa >> (localDatab & 0x1f)) & shiftMask[localDatab & 0x1f];
							reg[rd] = localResult;
						}
						else{ //SRAW
							localResult = localDataa >> (localDatab & 0x1f);
							reg[rd] = localResult;
						}
					break;
					default:
						printf("In OPW switch case, this should never happen... Instr was %x\n", (int)ins);
					break;
					}
				}
				break;
				//END of OP treatment

			//******************************************************************************************
			//Treatment for: SYSTEM INSTRUCTIONS
			case RISCV_SYSTEM:
				if (funct3 == 0 && funct7 == 0)
					pc = 0; //Currently we break on ECALL
			break;
			default:
				printf("In default part of switch opcode, instr %x is not handled yet\n", (int) ins);
			break;

			}
			reg[0] = 0;
			n_inst = n_inst + 1;
			for (int i=0; i<32; i++){
				fprintf(stderr,";%x", (int) reg[i]);
			}
			fprintf(stderr, "\n");

		}
		while (pc != 0 && n_inst<100000000); //Apparently, return from main function is always at address 0x4000074
		fprintf(stderr,"Simulation finished in %d cycles\n",n_inst);

		return 0;
	}
}

void RiscvSimulator::stb(ac_int<64, false> addr, ac_int<8, true> value){
	if (addr == 0x10009000){
		printf("%c",(char) value&0xff);

	}
	else
		this->memory[addr] = value & 0xff;

//	fprintf(stderr, "memwrite %x %x\n", addr, value);

}



void RiscvSimulator::sth(ac_int<64, false> addr, ac_int<16, true> value){
	this->stb(addr+1, value.slc<8>(8));
	this->stb(addr+0, value.slc<8>(0));
}

void RiscvSimulator::stw(ac_int<64, false> addr, ac_int<32, true> value){
	this->stb(addr+3, value.slc<8>(24));
	this->stb(addr+2, value.slc<8>(16));
	this->stb(addr+1, value.slc<8>(8));
	this->stb(addr+0, value.slc<8>(0));
}

void RiscvSimulator::std(ac_int<64, false> addr, ac_int<64, true> value){
	this->stb(addr+7, value.slc<8>(56));
	this->stb(addr+6, value.slc<8>(48));
	this->stb(addr+5, value.slc<8>(40));
	this->stb(addr+4, value.slc<8>(32));
	this->stb(addr+3, value.slc<8>(24));
	this->stb(addr+2, value.slc<8>(16));
	this->stb(addr+1, value.slc<8>(8));
	this->stb(addr+0, value.slc<8>(0));
}


ac_int<8, true> RiscvSimulator::ldb(ac_int<64, false> addr){

	ac_int<8, true> result = 0;
	if (addr == 0x10009000){
		result = getchar();
	}
	else if (this->memory.find(addr) != this->memory.end())
		result = this->memory[addr];
	else
		result= 0;

//	fprintf(stderr, "memread %x %x\n", addr, result);

	return result;
}


//Little endian version
ac_int<16, true> RiscvSimulator::ldh(ac_int<64, false> addr){

	ac_int<16, true> result = 0;
	result.set_slc(8, this->ldb(addr+1));
	result.set_slc(0, this->ldb(addr));
	return result;
}

ac_int<32, false> RiscvSimulator::ldw(ac_int<64, false> addr){

	ac_int<32, false> result = 0;
	result.set_slc(24, this->ldb(addr+3));
	result.set_slc(16, this->ldb(addr+2));
	result.set_slc(8, this->ldb(addr+1));
	result.set_slc(0, this->ldb(addr));
	return result;
}

ac_int<64, false> RiscvSimulator::ldd(ac_int<64, false> addr){

	ac_int<64, false> result = 0;
	result.set_slc(56, this->ldb(addr+7));
	result.set_slc(48, this->ldb(addr+6));
	result.set_slc(40, this->ldb(addr+5));
	result.set_slc(32, this->ldb(addr+4));
	result.set_slc(24, this->ldb(addr+3));
	result.set_slc(16, this->ldb(addr+2));
	result.set_slc(8, this->ldb(addr+1));
	result.set_slc(0, this->ldb(addr));
	return result;
}

void RiscvSimulator::doStep(){

}
