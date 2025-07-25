#define MEMORY_MAX (1<<16)
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <Windows.h>
#include <conio.h>

uint16_t memory[MEMORY_MAX]; /* 65536 locations */

enum Registers{
	R_R0 = 0,
	R_R1,
	R_R2,
	R_R3,
	R_R4,
	R_R5,
	R_R6,
	R_R7,
	R_PC, /*Program Counter*/
	R_COND,
	R_COUNT
};
uint16_t reg[R_COUNT];

enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};

HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOldMode;

void disable_input_buffering()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode); /* save old mode */
    fdwMode = fdwOldMode
            ^ ENABLE_ECHO_INPUT  /* no input echo */
            ^ ENABLE_LINE_INPUT; /* return when one or
                                    more characters are available */
    SetConsoleMode(hStdin, fdwMode); /* set new mode */
    FlushConsoleInputBuffer(hStdin); /* clear buffer */
}

void restore_input_buffering()
{
    SetConsoleMode(hStdin, fdwOldMode);
}

uint16_t check_key()
{
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

enum Op
{
	OP_BR = 0, /* branch */
	OP_ADD,    /* add  */
	OP_LD,     /* load */
	OP_ST,     /* store */
	OP_JSR,    /* jump register */
	OP_AND,    /* bitwise and */
	OP_LDR,    /* load register */
	OP_STR,    /* store register */
	OP_RTI,    /* unused */
	OP_NOT,    /* bitwise not */
	OP_LDI,    /* load indirect */
	OP_STI,    /* store indirect */
	OP_JMP,    /* jump */
	OP_RES,    /* reserved (unused) */
	OP_LEA,    /* load effective address */
	OP_TRAP    /* execute trap */
};

enum Flag
{
	FL_POS = 1 << 0, /* P */
	FL_ZRO = 1 << 1, /* Z */
	FL_NEG = 1 << 2, /* N */
};

enum
{
	TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
	TRAP_OUT = 0x21,   /* output a character */
	TRAP_PUTS = 0x22,  /* output a word string */
	TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
	TRAP_PUTSP = 0x24, /* output a byte string */
	TRAP_HALT = 0x25   /* halt the program */
};

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}

void mem_write(uint16_t Dst, uint16_t val)
{
	memory[Dst] = val;
}

uint16_t sign_extend(uint16_t x, int bit_count)
{
	if ((x >> (bit_count - 1)) & 1) {
		x |= (0xFFFF << bit_count);
	}
	return x;
}

uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

void update_flags(uint16_t r)
{
	if (reg[r] == 0)
	{
		reg[R_COND] = FL_ZRO;
	}
	else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
	{
		reg[R_COND] = FL_NEG;
	}
	else
	{
		reg[R_COND] = FL_POS;
	}
}

void read_image_file(FILE* file)
{
    /* the origin tells us where in memory to place the image */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    /* we know the maximum file size so we only need one fread */
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    /* swap to little endian */
    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}

int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    read_image_file(file);
    fclose(file);
    return 1;
}

void ADD(uint16_t instruction)
{
	uint16_t DR = (instruction >> 9) & 0x0007;
	uint16_t SR = (instruction >> 6) & 0x0007;
	uint16_t mode = (instruction >> 5) & 0x0001;
	if(mode)
	{
		uint16_t imm5 = instruction & 0x001F;
		imm5 = sign_extend(imm5, 5);
		reg[DR] = reg[SR] + imm5;
	}
	else
	{
		uint16_t SR_2 = instruction & 0x0007;
		reg[DR] =reg[SR] + reg[SR_2];
	}
	update_flags(DR);
}

void LDI(uint16_t instruction)
{
	uint16_t DR = (instruction >> 9) & 0x0007;
	uint16_t offset = instruction & 0x01FF;
	offset = sign_extend(offset, 9);
	reg[DR] = mem_read(mem_read(reg[R_PC] + offset));
	update_flags(DR);
}

void bit_AND(uint16_t instruction)
{
	uint16_t DR = (instruction >> 9) & 0x0007;
	uint16_t SR = (instruction >> 6) & 0x0007;
	uint16_t mode = (instruction >> 5) & 0x0001;
	if(mode)
	{
		uint16_t imm5 = instruction & 0x001F;
		imm5 = sign_extend(imm5, 5);
		reg[DR] = reg[SR] & imm5;
	}
	else
	{
		uint16_t SR_2 = instruction & 0x0007;
		reg[DR] = reg[SR] & reg[SR_2];
	}
	update_flags(DR);
}

void bit_NOT(uint16_t instruction)
{
	uint16_t DR = (instruction >> 9) & 0x0007;
	uint16_t SR = (instruction >> 6) & 0x0007;
	reg[DR] = ~reg[SR];
	update_flags(DR);
}

void BRANCH(uint16_t instruction)
{
	uint16_t cond = (instruction >> 9) & 0x0007;
	uint16_t offset = instruction & 0x01FF;
	offset = sign_extend(offset, 9);
	if(cond & reg[R_COND])
	{
		reg[R_PC] = reg[R_PC] + offset;
	}
}

void JUMP(uint16_t instruction)
{
	uint16_t SR = (instruction >> 6) & 0x0007;
	reg[R_PC] = reg[SR];
}

void JSR(uint16_t instruction)
{
	uint16_t mode = (instruction >> 11) & 0x0001;
	reg[R_R7] = reg[R_PC];
	if(mode)
	{
		uint16_t offset = instruction & 0x07FF;
		offset = sign_extend(offset, 11);
		
		reg[R_PC] += offset;
	}
	else
	{
		uint16_t SR = (instruction >> 6) & 0x0007;
		reg[R_PC] = reg[SR];
	}
}

void LD(uint16_t instruction)
{
	uint16_t DR = (instruction >> 9) & 0x0007;
	uint16_t offset = instruction & 0x01FF;
	offset = sign_extend(offset, 9);
	reg[DR] = mem_read(reg[R_PC] + offset);
	update_flags(DR);
}

void LDR(uint16_t instruction)
{
	uint16_t DR = (instruction >> 9) & 0x0007;
	uint16_t SR = (instruction >> 6) & 0x0007;
	uint16_t offset = instruction & 0x003F;
	offset = sign_extend(offset, 6);
	reg[DR] = mem_read(reg[SR] + offset);
	update_flags(DR);
}

void LEA(uint16_t instruction)
{
	uint16_t DR = (instruction >> 9) & 0x0007;
	uint16_t offset = instruction & 0x01FF;
	offset = sign_extend(offset, 9);
	reg[DR] = reg[R_PC] + offset;
	update_flags(DR);
}

void ST(uint16_t instruction)
{
	uint16_t SR = (instruction >> 9) & 0x0007;
	uint16_t offset = instruction & 0x01FF;
	offset = sign_extend(offset, 9);
	mem_write(reg[R_PC] + offset, reg[SR]);
}

void STI(uint16_t instruction)
{
	uint16_t SR = (instruction >> 9) & 0x0007;
	uint16_t offset = instruction & 0x01FF;
	offset = sign_extend(offset, 9);
	mem_write(mem_read(reg[R_PC] + offset), reg[SR]);
}

void STR(uint16_t instruction)
{
	uint16_t SR = (instruction >> 9) & 0x0007;
	uint16_t DR = (instruction >> 6) & 0x0007;
	uint16_t offset = instruction & 0x003F;
	offset = sign_extend(offset, 6);
	mem_write(reg[DR] + offset, reg[SR]);
}

void PUTS(void)
{
	uint16_t addr = reg[R_R0];
	while(memory[addr])
	{
		putc((char)memory[addr], stdout);
		addr++;
	}
	fflush(stdout);
}

void TRAPIN(void)
{
    printf("Enter a single character:\n");
    char temp = getchar();           // Reads one character from stdin

    putc(temp, stdout);                   // Echo the character back to the screen
	fflush(stdout);
    reg[R_R0] = (uint16_t)temp;      // Store the ASCII value into R0
	update_flags(R_R0);
}

void TRAP_GET_C(void)
{
	printf("Enter a single character:\n");
    char temp = getchar();           // Reads one character from stdin
    reg[R_R0] = (uint16_t)temp;      // Store the ASCII value into R0
	update_flags(R_R0);
}

void TRAPOUT(void)
{
	char temp = (char)reg[R_R0];
	putc(temp, stdout);
	fflush(stdout);
}

void TRAP_PUT_SP(void)
{
    uint16_t addr = reg[R_R0];
    while (memory[addr] != 0)
    {
        char a = memory[addr] & 0x00FF;         // lower byte
        putc(a, stdout);                        // print first character

        char b = (memory[addr] >> 8) & 0x00FF;  // upper byte
        if (b != 0)
        {
            putc(b, stdout);                    // print second character if not null
        }
        addr++; // move to next word
    }
    fflush(stdout); // flush output to ensure all characters are printed
}

void TRAPHALT(int *running)
	{
		puts("HALT");
		fflush(stdout);
		*(running) = 0;
	}
int main(int argc, const char* argv[])
	{
		if (argc < 2)
		{
			/* show usage string */
			printf("lc3 [image-file1] ...\n");
			exit(2);
		}

		for (int j = 1; j < argc; ++j)
		{
			if (!read_image(argv[j]))
			{
				printf("failed to load image: %s\n", argv[j]);
				exit(1);
			}
		}
		signal(SIGINT, handle_interrupt);
		disable_input_buffering();
		reg[R_COND] = FL_ZRO;
		enum { PC_START = 0x3000 };
		reg[R_PC] = PC_START;
		int running = 1;
		while(running)
		{
			uint16_t instruction = mem_read(reg[R_PC]++);
			uint16_t op = instruction >> 12;
			switch (op)
			{
				case OP_ADD:
					ADD(instruction);
					break;
				case OP_AND:
					bit_AND(instruction);
					break;
				case OP_NOT:
					bit_NOT(instruction);
					break;
				case OP_BR:
					BRANCH(instruction);
					break;
				case OP_JMP:
					JUMP(instruction);
					break;
				case OP_JSR:
					JSR(instruction);
					break;
				case OP_LD:
					LD(instruction);
					break;
				case OP_LDI:
					LDI(instruction);
					break;
				case OP_LDR:
					LDR(instruction);
					break;
				case OP_LEA:
					LEA(instruction);
					break;
				case OP_ST:
					ST(instruction);
					break;
				case OP_STI:
					STI(instruction);
					break;
				case OP_STR:
					STR(instruction);
					break;
				case OP_TRAP:
					reg[R_R7] = reg[R_PC];
					switch (instruction & 0xFF)
					{
						case TRAP_GETC:
							TRAP_GET_C();
							break;
						case TRAP_OUT:
							TRAPOUT();
							break;
						case TRAP_PUTS:
							PUTS();
							break;
						case TRAP_IN:
							TRAPIN();
							break;
						case TRAP_PUTSP:
							TRAP_PUT_SP();
							break;
						case TRAP_HALT:
							TRAPHALT(&running);
							break;
					}
					break;
				case OP_RES:
				case OP_RTI:
				default:
					abort();
					break;
			}
		}
		restore_input_buffering();
	}