/*
 * buildTraces.cpp
 *
 *  Created on: 22 mai 2017
 *      Author: simon
 */
#include <isa/irISA.h>
#include <dbt/dbtPlateform.h>
#include <lib/endianness.h>
#include <lib/log.h>

IRBlock* ifConversion(IRBlock *entryBlock, IRBlock *thenBlock, IRBlock *elseBlock){

}
bool VERBOSE = 1;
IRBlock* superBlock(IRBlock *entryBlock, IRBlock *secondBlock){

	/*****
	 * The goal of this function is to merge two block into one.
	 * There are two kinds of merging supported by this function
	 * -> The merging where the entry blocks has two successors. In this case, the execution may leave the block before its end.
	 * 		For this reason we have to write into new register which will be allocated by the scheduler. We then use the conditional
	 * 		instruction to ensure that we do modify the behavior of the program.
	 * -> The merge of two block when there is no escape. In such a case we do not have to take care of conditions,
	 *
	 *******************************************************
	 *******************************************************
	 *******************************************************
	 *
	 * Here are the steps done by the function:
	 * 0. We compute some parameters for the transformation:
	 * 		-> isEscapable is set to true if the first block is a conditional block (so if the second block may not be executed)
	 *
	 * 1. We go through the instructions of the first block. Doing so, we keep track of
	 * 		-> The last instruction that wrote in a given register
	 * 		-> The last 4 memory accesses (read/store)
	 * 		-> The last 4 conditional instruction
	 * 		-> The jump and its condition
	 *   All these instructions are inserted in the block unmodified.
	 *
	 * 2. We go through the instructions from the second block. Doing this we still keep track of:
	 * 		-> Last writer on registers from this second block
	 * 	  We also do the following modifications:
	 * 	  	-> If isEscapable we transform memory stores into conditional memory stores, we say writing instruction to allocate
	 * 	  		a new register instead of using the normal one.
	 * 	  	-> For each operand of a given instruction, if the operand is global (>256) then we check if the said register has not
	 * 	  		been modified by the block 1. If so we change the reference by a non global one and we add a data dependency.
	 * 	  		Otherwise, we just change the local reference by adding an offset to the operand (the offset will be the size of
	 * 	  		block 1).
	 * 	  	-> For each dependencies of a given instruction, we add an offset in order to refer to the correct instruction index
	 * 	  		in the newly created block. Once again, this offset will be the size of block 1.
	 * 	  	-> The first memory operation found will depend on the last memory operation found on block 1 to ensure the memory coherence
	 *
	 * 3. If the block is escapable, then we have to add also some conditional instructions:
	 * 		-> One setCond instruction which will set the flag by looking at the condition (found during step 1).
	 * 			Note that this instruction will depend (control dependency) from the conditional instruction found during step one.
	 * 		-> For each register whose value was written on block 2, we insert a setCond which will commit the new value if and only if
	 * 			the condition was fulfilled. This instruction will have a data dependency from the instruction from block 2 that
	 * 			generated the value and a control dependency from the SETCOND instruction and from any previous write from the block 1
	 * 			(to ensure that there is no WAR dependency violated).
	 *
	 * 4. Finally, the new block is placed in the procedure, taking the place of the block 1.
	 */

	char isEscape = (entryBlock->nbSucc > 1);
	char useSetc = entryBlock->nbInstr<8;

	Log::printf(LOG_SCHEDULE_PROC, "***********Merging blocks*******************\n");

	for (int i=0; i<entryBlock->nbInstr; i++){
		Log::printf(LOG_SCHEDULE_PROC, "%s ", printBytecodeInstruction(i, readInt(entryBlock->instructions, i*16+0), readInt(entryBlock->instructions, i*16+4), readInt(entryBlock->instructions, i*16+8), readInt(entryBlock->instructions, i*16+12)).c_str());
	}
	Log::printf(LOG_SCHEDULE_PROC, "\n");

	for (int i=0; i<secondBlock->nbInstr; i++){
		Log::printf(LOG_SCHEDULE_PROC, "%s ", printBytecodeInstruction(i, readInt(secondBlock->instructions, i*16+0), readInt(secondBlock->instructions, i*16+4), readInt(secondBlock->instructions, i*16+8), readInt(secondBlock->instructions, i*16+12)).c_str());
	}


	short sizeofEntryBlock = entryBlock->nbInstr;
	char hasJump = (entryBlock->nbSucc >= 1) && entryBlock->successor1 != secondBlock; //True if there is a way to exit the block in the middle
	char jumpId = entryBlock->nbInstr-1; //We suppose the jump is always the last instruction

	//We declare the result block:
	IRBlock *result = new IRBlock(0,0,0);
	result->instructions = (uint32*) malloc(256*4*sizeof(uint32));

	short lastWriteReg[64];
	short lastWriteRegForSecond[64];

	for (int oneReg = 0; oneReg < 64; oneReg++){
		lastWriteReg[oneReg] = -1;
		lastWriteRegForSecond[oneReg] = -1;
	}

	//Last conditional instr
	short lastCondInstr[4];
	char nbLastCondInstr = 0;
	unsigned char placeLastCondInstr = 0;

	//Last memory accesses
	short lastMemInstr[4];
	char nbLastMemInstr = 0;
	unsigned char placeLastMemInstr = 0;

	//****************************************************************************************************
	//We go through the first block to find all written register
	//Because of this we will be able to build dependencies correctly
	short indexOfJump = -1;
	unsigned short indexOfCondition = -1;
	for (int oneInstr = 0; oneInstr<sizeofEntryBlock; oneInstr++){
		short writtenReg = getDestinationRegister(entryBlock->instructions, oneInstr);
		if (writtenReg >= 0)
			lastWriteReg[writtenReg-256] = oneInstr;


		result->instructions[4*oneInstr+0] = entryBlock->instructions[4*oneInstr+0];
		result->instructions[4*oneInstr+1] = entryBlock->instructions[4*oneInstr+1];
		result->instructions[4*oneInstr+2] = entryBlock->instructions[4*oneInstr+2];
		result->instructions[4*oneInstr+3] = entryBlock->instructions[4*oneInstr+3];

		char opcode = getOpcode(entryBlock->instructions, oneInstr);
		if (entryBlock == secondBlock){
			if (opcode == VEX_BR)
				setOpcode(result->instructions, oneInstr, VEX_BRF);
			else if (opcode == VEX_BRF)
				setOpcode(result->instructions, oneInstr, VEX_BR);
		}

		if (opcode == VEX_BR || opcode == VEX_BRF){
			indexOfJump = oneInstr;
			short operands[2];
			char nbOperands = getOperands(entryBlock->instructions, oneInstr, operands);

			if (operands[0] >= 256){
				if (lastWriteReg[operands[0]-256] != -1)
					indexOfCondition = lastWriteReg[operands[0]-256];
				else
					indexOfCondition = operands[0];
			}
			else
				indexOfCondition = operands[0];
		}

		//We keep track of last cond instruction
		if (opcode == VEX_STDc || opcode == VEX_STWc || opcode == VEX_STHc || opcode == VEX_STBc || opcode == VEX_SETFc || opcode == VEX_SETc){
			lastCondInstr[placeLastCondInstr] = oneInstr;
			placeLastCondInstr = (placeLastCondInstr+1) & 0x3;

			if (nbLastCondInstr<4)
				nbLastCondInstr++;
		}

		if (opcode == VEX_SETCOND || opcode == VEX_SETCONDF){
			nbLastCondInstr = 0;
		}

		//We keep track of last store/load instructions
		char shiftOpcode = opcode >> 3;
		if (shiftOpcode == (VEX_STD >> 3)){
			lastMemInstr[0] = oneInstr;
			lastMemInstr[1] = oneInstr;
			lastMemInstr[2] = oneInstr;
			lastMemInstr[3] = oneInstr;
			nbLastMemInstr = 4;
		}

		if (shiftOpcode == (VEX_LDD >> 3)){
			lastMemInstr[placeLastMemInstr] = oneInstr;
			placeLastMemInstr = (placeLastMemInstr+1) & 0x3;

			if (nbLastMemInstr<4)
				nbLastMemInstr++;
		}
	}
	result->nbInstr = entryBlock->nbInstr;


	//****************************************************************************************************
	//We insert instructions from second block
	short indexOfSecondJump = -1;
	char hasStores = 0;

	for (int oneInstr = 0; oneInstr<secondBlock->nbInstr; oneInstr++){

		//We copy the instruction into the new block
		result->instructions[(sizeofEntryBlock+oneInstr)*4+0] = secondBlock->instructions[oneInstr*4+0];
		result->instructions[(sizeofEntryBlock+oneInstr)*4+1] = secondBlock->instructions[oneInstr*4+1];
		result->instructions[(sizeofEntryBlock+oneInstr)*4+2] = secondBlock->instructions[oneInstr*4+2];
		result->instructions[(sizeofEntryBlock+oneInstr)*4+3] = secondBlock->instructions[oneInstr*4+3];



		short operands[2];
		char nbOperand = getOperands(secondBlock->instructions, oneInstr, operands);

		//The new instruction will write into an allocated register (if we are in a case with escape)
		if (isEscape)
			setAlloc(result->instructions, sizeofEntryBlock + oneInstr, 1);

		//We update dependencies
		addOffsetToDep(result->instructions, sizeofEntryBlock+oneInstr, sizeofEntryBlock);

		//For each operand register, there are two possibilities:
		// -> The operand is lower than 256, then it is a reference to another instruction from the block: we correct it
		// -> The operand is greater than 256 then it is an access to global register and thus we need to check if there is a dep to add
		for (int oneOperand = 0; oneOperand<nbOperand; oneOperand++){
			if (lastWriteReg[operands[oneOperand]] < sizeofEntryBlock){
				if (operands[oneOperand] < 256){
					operands[oneOperand] += sizeofEntryBlock;
				}
				else if (lastWriteReg[operands[oneOperand]-256] != -1){
					addDataDep(result->instructions, lastWriteReg[operands[oneOperand]-256], sizeofEntryBlock+oneInstr);
					operands[oneOperand] = lastWriteReg[operands[oneOperand]-256];
				}
			}
		}
		setOperands(result->instructions, sizeofEntryBlock + oneInstr, operands);

		char opcode = getOpcode(secondBlock->instructions, oneInstr);
		char shiftedOpcode = opcode>>3;


		/***************************************************************
		 * We handle memory accesses from the second block:
		 *  -> Stores are turned into conditional store if needed
		 *  -> Dependencies to ensure memory coherence are added to the first memory accesses met
		 */

		if (shiftedOpcode == (VEX_STD>>3)){
			hasStores = 1;

			//If we are in a escapable block, we need to add a dependency from the first jump
			if (isEscape)
				addControlDep(result->instructions, indexOfJump, sizeofEntryBlock+oneInstr);

			//In any case, we have to ensure a dependency from last mem instruction from previous block (this is only needed for first memory store)
			for (int oneLastMemInstr = 0; oneLastMemInstr<nbLastMemInstr; oneLastMemInstr++)
				addControlDep(result->instructions, lastMemInstr[oneLastMemInstr], sizeofEntryBlock+oneInstr);

			nbLastMemInstr = 0;
		}

		if (nbLastMemInstr > 0 && shiftedOpcode == (VEX_LDD>>3)){
			//For the first 4 load instr we add a dependency to ensure the correctness
			addControlDep(result->instructions, lastMemInstr[placeLastMemInstr], sizeofEntryBlock+oneInstr);

			nbLastMemInstr--;
			placeLastMemInstr = (placeLastMemInstr-1) & 0x3;
		}

		/***************************************************************
		 * We handle cond instructions from the second block:
		 *  -> Stores are turned into conditional store if needed
		 *  -> Dependencies to ensure memory coherence are added to the first memory accesses met
		 */

		if (nbLastCondInstr > 0 && (opcode == VEX_SETCOND || opcode == VEX_SETCONDF)){
			for (int onePreviousCond = 0; onePreviousCond<nbLastCondInstr; onePreviousCond++){
				placeLastCondInstr = (placeLastCondInstr-1) & 0x3;
				addControlDep(result->instructions, lastCondInstr[placeLastCondInstr], sizeofEntryBlock+oneInstr);
			}
			nbLastCondInstr = 0;
		}

		short writtenReg = getDestinationRegister(secondBlock->instructions, oneInstr);
		if (writtenReg >= 0)
			lastWriteRegForSecond[writtenReg-256] = oneInstr+sizeofEntryBlock;

		//We check if there is a jump in the second block
		if (opcode == VEX_GOTO || opcode == VEX_GOTOR || opcode == VEX_BRF || opcode == VEX_BR || opcode == VEX_CALL || opcode == VEX_CALLR){
			if (indexOfSecondJump == -1 && indexOfJump != -1){
				addControlDep(result->instructions, indexOfJump, oneInstr+sizeofEntryBlock);
				fprintf(stderr, "indexOfJump is %d\n",indexOfJump);
			}
			indexOfSecondJump = oneInstr+sizeofEntryBlock;


		}
	}
	result->nbInstr = entryBlock->nbInstr + secondBlock->nbInstr;


	//We insert a SETCOND depending on the value of the condition
	//This instruction will depend from all previous cond instrucion (if any)
	if (isEscape && useSetc){

		nbLastCondInstr = 0;

//		if (indexOfSecondJump != -1){
//			result->instructions[indexOfSecondJump*4+1] = result->instructions[indexOfSecondJump*4+1] & 0xffffffc0;
//			result->instructions[indexOfSecondJump*4+2] = 0;
//			result->instructions[indexOfSecondJump*4+3] = 0;
//		}

		for (int oneReg = 1; oneReg < 64; oneReg++){
			if (lastWriteRegForSecond[oneReg]>=0){

				char opcodeJump = getOpcode(entryBlock->instructions, indexOfJump);

				result->instructions[indexOfSecondJump*4+2] = 0;
				result->instructions[indexOfSecondJump*4+3] = 0;
				result->instructions[indexOfSecondJump*4+1] = result->instructions[indexOfSecondJump*4+1] & 0xffffffc0;

				char opcodeCond;
				if (opcodeJump == VEX_BR)
					opcodeCond = VEX_SETc;
				else
					opcodeCond = VEX_SETFc;

				uint128 bytecodeInstr = assembleRBytecodeInstruction(2, 0, VEX_SETc, indexOfCondition, lastWriteRegForSecond[oneReg], oneReg+256, 0);
				result->instructions[(result->nbInstr)*4+0] = bytecodeInstr.slc<32>(96);
				result->instructions[(result->nbInstr)*4+1] = bytecodeInstr.slc<32>(64);
				result->instructions[(result->nbInstr)*4+2] = bytecodeInstr.slc<32>(32);
				result->instructions[(result->nbInstr)*4+3] = bytecodeInstr.slc<32>(0);
				addDataDep(result->instructions, lastWriteRegForSecond[oneReg], result->nbInstr);
				if (lastWriteReg[oneReg]>= 0)
					addControlDep(result->instructions, lastWriteReg[oneReg], result->nbInstr);

				//We add a control dependency from one of the last four cond instruction (note: this array is initialized with four times the setcond instruction)
				if (indexOfCondition<256){
					if (nbLastCondInstr<2){

						addDataDep(result->instructions, indexOfCondition, result->nbInstr);
						nbLastCondInstr++;
						lastCondInstr[placeLastCondInstr] = result->nbInstr;
						placeLastCondInstr = (placeLastCondInstr+1) & 0x3;
					}
					else{
						addControlDep(result->instructions, lastCondInstr[placeLastCondInstr], result->nbInstr);
						lastCondInstr[placeLastCondInstr] = result->nbInstr;
						placeLastCondInstr = (placeLastCondInstr + 1) & 0x3;
					}
				}
				lastWriteRegForSecond[oneReg] = result->nbInstr;

				result->nbInstr++;
			}
		}

		//If there is a jump in the second block, we add dependencies with the setcond
		if (indexOfSecondJump != -1){
			for (int oneLastSet = 0; oneLastSet < nbLastCondInstr; oneLastSet++){
				placeLastCondInstr = (placeLastCondInstr - 1) & 0x3;
				addControlDep(result->instructions, lastCondInstr[placeLastCondInstr], indexOfSecondJump);
			}
			nbLastCondInstr = 0;
		}

		//We correct the jump register if any
		char jumpopcode = getOpcode(result->instructions, indexOfSecondJump);
		if (jumpopcode == VEX_BR || jumpopcode == VEX_BRF){
			short operands[2];
			short nbOperand = getOperands(result->instructions, indexOfSecondJump, operands);
			if (operands[0] < 256){
				char physicalDest = getDestinationRegister(result->instructions, operands[0]);
				operands[0] = lastWriteRegForSecond[physicalDest];
				setOperands(result->instructions, indexOfSecondJump, operands);
				addDataDep(result->instructions, operands[0], indexOfSecondJump);
			}
		}

		if (indexOfSecondJump != -1){
	#ifndef IR_SUCC
			result->instructions[result->nbInstr*4+0] = result->instructions[indexOfSecondJump*4+0];
			result->instructions[result->nbInstr*4+1] = result->instructions[indexOfSecondJump*4+1];
			result->instructions[result->nbInstr*4+2] = result->instructions[indexOfSecondJump*4+2];
			result->instructions[result->nbInstr*4+3] = result->instructions[indexOfSecondJump*4+3];

			result->instructions[indexOfSecondJump*4+0] = 0;
			result->instructions[indexOfSecondJump*4+1] = 0;
			result->instructions[indexOfSecondJump*4+2] = 0;
			result->instructions[indexOfSecondJump*4+3] = 0;

			indexOfSecondJump = result->nbInstr;
			result->nbInstr++;

	#endif

		}



		for (int oneLastCond = 0; oneLastCond<nbLastCondInstr; oneLastCond++){
			addControlDep(result->instructions, lastCondInstr[placeLastCondInstr], indexOfSecondJump);
			placeLastCondInstr = (placeLastCondInstr - 1) & 0x3;
		}


	}
	else if (isEscape){

		nbLastCondInstr = 0;
		for (int oneReg = 63; oneReg >= 1; oneReg--){
					if (lastWriteRegForSecond[oneReg]>=0){


						setAlloc(result->instructions, lastWriteRegForSecond[oneReg], 0);
						//We add a control dependency from one of the last four cond instruction (note: this array is initialized with four times the setcond instruction)
						if (indexOfCondition<256){
							if (nbLastCondInstr<2){
								addControlDep(result->instructions, indexOfJump, lastWriteRegForSecond[oneReg]);
								nbLastCondInstr++;
								lastCondInstr[placeLastCondInstr] = lastWriteRegForSecond[oneReg];
								placeLastCondInstr = (placeLastCondInstr+1) & 0x1;
							}
							else{
								if (lastCondInstr[placeLastCondInstr] < lastWriteRegForSecond[oneReg])
									addControlDep(result->instructions, lastCondInstr[placeLastCondInstr], lastWriteRegForSecond[oneReg]);
								else
									addControlDep(result->instructions, lastWriteRegForSecond[oneReg], lastCondInstr[placeLastCondInstr]);

								lastCondInstr[placeLastCondInstr] = lastWriteRegForSecond[oneReg];
								placeLastCondInstr = (placeLastCondInstr + 1) & 0x1;
							}
						}
					}
				}
	}
	result->nbSucc = 0;

	for (int oneJump = 0; oneJump<entryBlock->nbJumps; oneJump++){
		char jumpOpcode = getOpcode(entryBlock->instructions, entryBlock->jumpIds[oneJump]);
		if (entryBlock == secondBlock){
			result->addJump(entryBlock->jumpIds[oneJump], -1);
			result->successors[result->nbSucc] = entryBlock->successors[oneJump+1];
			result->nbSucc++;
		}
		else if (jumpOpcode != VEX_GOTO && jumpOpcode != VEX_GOTOR && (entryBlock->successors[oneJump] != secondBlock || (entryBlock->successors[oneJump] == secondBlock && entryBlock == secondBlock))){
			result->addJump(entryBlock->jumpIds[oneJump], -1);
			result->successors[result->nbSucc] = entryBlock->successors[oneJump];
			result->nbSucc++;
		}
	}

	if (useSetc){
		for (int oneJump = 0; oneJump<secondBlock->nbJumps; oneJump++){
			char jumpOpcode = getOpcode(secondBlock->instructions, secondBlock->jumpIds[oneJump]);
			if (jumpOpcode != VEX_GOTO && jumpOpcode != VEX_GOTOR){
				result->addJump(indexOfSecondJump, -1);
				result->successors[result->nbSucc] = secondBlock->successors[oneJump];
				result->nbSucc++;
			}
		}
	}
	else{
		for (int oneJump = 0; oneJump<secondBlock->nbJumps; oneJump++){
			char jumpOpcode = getOpcode(secondBlock->instructions, secondBlock->jumpIds[oneJump]);
			if (jumpOpcode != VEX_GOTO && jumpOpcode != VEX_GOTOR){
				result->addJump(secondBlock->jumpIds[oneJump]+entryBlock->nbInstr, -1);
				result->successors[result->nbSucc] = secondBlock->successors[oneJump];
				result->nbSucc++;
			}
		}
	}

	if (secondBlock->nbJumps < secondBlock->nbSucc){
		result->successors[result->nbSucc] = secondBlock->successors[secondBlock->nbSucc - 1];
		result->nbSucc++;
	}



	Log::printf(LOG_SCHEDULE_PROC, "\n Resulting block is: \n");
	for (int i=0; i<result->nbInstr; i++){
		Log::printf(LOG_SCHEDULE_PROC, "%s ", printBytecodeInstruction(i, readInt(result->instructions, i*16+0), readInt(result->instructions, i*16+4), readInt(result->instructions, i*16+8), readInt(result->instructions, i*16+12)).c_str());
	}


	result->vliwStartAddress = entryBlock->vliwStartAddress;
	result->vliwEndAddress = secondBlock->vliwEndAddress;
	result->blockState = entryBlock->blockState;

	return result;


}

void buildTraces(DBTPlateform *platform, IRProcedure *procedure){

	/******************************************************************
	 * Optimization that merges blocks:
	 *
	 * The idea of this optimization is to merge several blocks in order to build bigger one which may lead to more
	 * parallelism. We define several kinds of merging:
	 *
	 * -> Merging three blocks corresponding to a if: in this case, you will work the following way:
	 * 		-> insert all instructions of the two conditionnal blocks working as allocs and with dependencies from
	 * 		last writes in predecessor block.
	 * 		-> If there are memory stores on those blocks, insert a dependency from the br instruction to them and
	 *
	 * -> Merging two blocks to obtain something with more than one output but only one input
	 *
	 */

	int nbBlock = 0;
	for (int oneBlock = 0; oneBlock<procedure->nbBlock; oneBlock++){
		if (procedure->blocks[oneBlock]->nbInstr != 0){
			nbBlock++;
		}
	}
	char changeMade = 1;
	while (changeMade){
		changeMade = 0;
		for (int oneBlock=0; oneBlock<procedure->nbBlock; oneBlock++){
			IRBlock *block = procedure->blocks[oneBlock];

			bool elligible = false;
			IRBlock *firstPredecessor;
			int firstPredecessorId;
			bool predecessorFound = false;

			if (block->nbInstr>0){
				for (int oneOtherBlock = 0; oneOtherBlock<procedure->nbBlock; oneOtherBlock++){
					IRBlock *otherBlock = procedure->blocks[oneOtherBlock];
					for (int oneSuccesor = 0; oneSuccesor < otherBlock->nbSucc; oneSuccesor++){
						if (otherBlock->successors[oneSuccesor] == block){
							if (predecessorFound){
								//We cannot build a trace
								elligible = false;
							}
							else if (block->sourceEndAddress == otherBlock->sourceStartAddress && otherBlock +1==block && otherBlock != block){
								firstPredecessor = otherBlock;
								predecessorFound = true;
								elligible = true;
								firstPredecessorId = oneOtherBlock;
							}
						}
					}

				}
			}

			if (elligible){


				IRBlock *oneSuperBlock = superBlock(firstPredecessor, block);

				if (oneSuperBlock == NULL){
					block->blockState = IRBLOCK_UNROLLED;
					break;
				}


				firstPredecessor->nbInstr = oneSuperBlock->nbInstr;
				firstPredecessor->instructions = oneSuperBlock->instructions;

				firstPredecessor->jumpID = oneSuperBlock->jumpID;
				if (firstPredecessor->nbJumps>0){
					firstPredecessor->nbJumps = 0;
					free(firstPredecessor->jumpIds);
					free(firstPredecessor->jumpPlaces);
				}

				for (int oneJump=0; oneJump<oneSuperBlock->nbJumps; oneJump++){
					firstPredecessor->addJump(oneSuperBlock->jumpIds[oneJump], oneSuperBlock->jumpPlaces[oneJump]);
				}

				firstPredecessor->nbSucc = oneSuperBlock->nbSucc;
				for (int oneSuccessor = 0; oneSuccessor<10; oneSuccessor++)
					firstPredecessor->successors[oneSuccessor] = oneSuperBlock->successors[oneSuccessor];

				firstPredecessor->sourceEndAddress = block->sourceEndAddress;


				block->nbSucc = 0;
				block->nbInstr = 0;
				block->vliwStartAddress = 0;

				nbBlock--;


//					delete block;
//					continue;

				if (firstPredecessor->nbJumps == 2 && firstPredecessor->jumpIds[0] == firstPredecessor->jumpIds[1])
					exit(-1);
				changeMade=1;
				break;


			}
			else if (block->blockState < IRBLOCK_UNROLLED && block->nbSucc == 2 && block->successor1 == block && block->nbInstr>10 && block->nbInstr<128){
					/**************************************************************************************************
					 * In this situation, we are unrolling a loop.
					 **************************************************************************************************
					 * The block CFG information should remain unchanged : only the number of instruction changes.
					 * The place location will also be modified...
					 **************************************************************************************************/
				for (int oneSuccessor = 0; oneSuccessor<block->nbSucc; oneSuccessor++)

					block->blockState = IRBLOCK_UNROLLED;

					IRBlock *oneSuperBlock = superBlock(block, block->successor1);

					fprintf(stderr, "UNROLLING!!!\n");
					block->nbInstr = oneSuperBlock->nbInstr;

					uint32 *oldInstruction = block->instructions;
					block->instructions = oneSuperBlock->instructions;
					oneSuperBlock->instructions = oldInstruction;

					block->jumpID = oneSuperBlock->jumpID;
					block->jumpIds = oneSuperBlock->jumpIds;
					block->nbJumps = oneSuperBlock->nbJumps;
					block->jumpPlaces = oneSuperBlock->jumpPlaces;
					block->nbSucc = oneSuperBlock->nbSucc;
					for (int oneSuccessor = 0; oneSuccessor<10; oneSuccessor++)
						block->successors[oneSuccessor] = oneSuperBlock->successors[oneSuccessor];


					delete oneSuperBlock;

					fprintf(stderr, "after : \n");
					changeMade=1;
					break;

				}
//				else if (isElligible && secondBlock->nbSucc == 1 && (block->nbSucc == 1 || secondBlock->nbInstr<20)){
//
//					/**************************************************************************************************
//					 * In this situation, we merge two blocks
//					 **************************************************************************************************
//					 *TODO
//					 **************************************************************************************************/
//					fprintf(stderr, "TRACING!!!\n");
//
//					IRBlock *oneSuperBlock = superBlock(block, secondBlock);
//					if (oneSuperBlock == NULL){
//						secondBlock->blockState = IRBLOCK_UNROLLED;
//						break;
//					}
//
//
//					//If the two blocks merged have the same successor, or if first block only had second block as successor,
//					// it is no longer needed to maintain a jump...
//					if (block->successor1 == secondBlock->successor1 || block->nbSucc == 1){
//						oneSuperBlock->nbSucc = 1;
//						oneSuperBlock->successor1 = secondBlock->successor1;
//						oneSuperBlock->sourceDestination = -1;
//					}
//					else{
//						oneSuperBlock->nbSucc = 2;
//						oneSuperBlock->successor1 = block->successor1;
//						oneSuperBlock->successor2 = secondBlock->successor1;
//						oneSuperBlock->sourceDestination = block->sourceDestination;
//					}
//					oneSuperBlock->sourceStartAddress = block->sourceStartAddress;
//					oneSuperBlock->sourceEndAddress = secondBlock->sourceEndAddress;
//
//
//					secondBlock->nbSucc = 0;
//					secondBlock->nbInstr = 0;
//					nbBlock--;
//
//
//					for (int oneOtherBlock = 0; oneOtherBlock<procedure->nbBlock; oneOtherBlock++){
//						IRBlock *otherBlock = procedure->blocks[oneOtherBlock];
//						if (otherBlock->successor1 == block)
//							otherBlock->successor1 = oneSuperBlock;
//
//						if (otherBlock->successor2 == block)
//							otherBlock->successor2 = oneSuperBlock;
//
//						procedure->blocks[oneBlock] = oneSuperBlock;
//
//					}
//
//					if (procedure->entryBlock == block)
//						procedure->entryBlock = oneSuperBlock;
//
////					delete block;
////					continue;
//					changeMade=1;
//					break;
//				}

		}

	}
	IRBlock **newBlocks = (IRBlock**)  malloc(nbBlock*sizeof(IRBlock*));
	int index = 0;
	for (int oneBlock = 0; oneBlock<procedure->nbBlock; oneBlock++){
		if (procedure->blocks[oneBlock]->nbInstr != 0){
			newBlocks[index] = procedure->blocks[oneBlock];
			if (procedure->entryBlock == procedure->blocks[oneBlock])
				procedure->entryBlock = newBlocks[index];
			index++;
		}
	}
	procedure->blocks = newBlocks;
	procedure->nbBlock = nbBlock;
}

