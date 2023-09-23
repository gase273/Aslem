#define negDispMask 0x800U

typedef unsigned int reg; //registri su 32b

typedef struct cpuRegs {
  reg gpr[16];
  reg csr[3];
} Registers;


//Enumi za registre da bude citkije sta se desava
enum {
  r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15
};

enum {
  status, handler, cause,
  sp = 14U,
  pc = 15U
};

enum {
  term_out = 0xFFFFFF00U,
  term_in = 0xFFFFFF04U
};

//Maske status registra
enum statusMasks {
  timerDisable = 1U,
  terminalDisable = 2U,
  allDisable = 4U
};

enum causeMasks {
  illegal = 1, timer, terminal, software
};

//Operacioni kodovi (visih 4bita prvog bajta instrukcije)
enum oc {
  halt, interrupt, call, jmp, xchg, arit, logic, shift, store, load
};

//Modovi za call
enum callModes {
  noMem, mem
};

//Modovi za jmp
enum jmpModes {
  jmpNoMem, beqNoMem, bneNoMem, bgtNoMem,
  jmpMem = 8U,
  beqMem = 9U,
  bneMem = 10U,
  bgtMem = 11U
};

//Modovi za arit
enum aritModes {
  add, sub, mul, divide
};

//Modovi za logic
enum logicModes {
  lnot, land, lor, lxor
};

enum shiftModes {
  left, right
};

enum storeModes {
  memdir, push, memind
};

enum loadModes {
  csrrd, regdirDisp, regindPom, gprPop, csrwr, csrdirOrD, csrRegindPom, csrPop
};
