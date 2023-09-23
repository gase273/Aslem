
#include <fstream>
#include <sys/mman.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include "../inc/emulator.hpp"

using namespace std;

Registers cpu_registers; //Registri
unsigned char* memory; //Memorija
bool writeToTermOut; //Da li je pisano u term_out registar

reg loadFromMemory(unsigned int addr) {
  if(addr > 0xfffffffcU) return -1; //Sprecava citanje sa vrha adresnog prostora
  unsigned char fByte = memory[addr];
  unsigned char sByte = memory[addr+1];
  unsigned char tByte = memory[addr+2];
  unsigned char fourByte = memory[addr+3];
  return (reg)(fByte | (sByte << 8) | (tByte << 16) | (fourByte << 24));
}

void writeToMemory(reg registerSrc, unsigned int addr) {
  if(addr > 0xfffffffcU) return; //Sprecava upise na vrhu adresnog prostora
  if(addr == term_out) writeToTermOut = true;
  memory[addr] = (unsigned char)(registerSrc & 0xff);
  if(addr == term_out) writeToTermOut = true;
  memory[addr+1] = (unsigned char)((registerSrc & 0xff00) >> 8);
  if(addr == term_out) writeToTermOut = true;
  memory[addr+2] =(unsigned char)((registerSrc & 0xff0000) >> 16);
  if(addr == term_out) writeToTermOut = true;
  memory[addr+3] = (unsigned char)((registerSrc & 0xff000000) >> 24);
}

void illegalInterrupt() {
  cout << "Bad instr\n"; //Ispisi na konzolu ako prekidna rutina ne obradjuje nelegalne instr
  cpu_registers.gpr[sp] -= 4;
  writeToMemory(cpu_registers.csr[status], cpu_registers.gpr[sp]); 
  cpu_registers.gpr[sp] -= 4;
  writeToMemory(cpu_registers.gpr[pc], cpu_registers.gpr[sp]); 
  cpu_registers.csr[status] |= statusMasks::allDisable; 
  cpu_registers.csr[cause] = causeMasks::illegal; 
  cpu_registers.gpr[pc] = cpu_registers.csr[handler]; 
}

void printState(Registers registers) {
  cout << "\n-----------------------------------------------------------------\n";
  cout << "Emulated processor executed halt instruction\n";
  cout << "Emulated processor state:\n";
  cout << " r0=0x" << hex << setfill('0') << setw(8) << registers.gpr[0] << "\t";
  cout << " r1=0x" << hex << setfill('0') << setw(8) << registers.gpr[1] << "\t";
  cout << " r2=0x" << hex << setfill('0') << setw(8) << registers.gpr[2] << "\t";
  cout << " r3=0x" << hex << setfill('0') << setw(8) << registers.gpr[3] << "\n";
  cout << " r4=0x" << hex << setfill('0') << setw(8) << registers.gpr[4] << "\t";
  cout << " r5=0x" << hex << setfill('0') << setw(8) << registers.gpr[5] << "\t";
  cout << " r6=0x" << hex << setfill('0') << setw(8) << registers.gpr[6] << "\t";
  cout << " r7=0x" << hex << setfill('0') << setw(8) << registers.gpr[7] << "\n";
  cout << " r8=0x" << hex << setfill('0') << setw(8) << registers.gpr[8] << "\t";
  cout << " r9=0x" << hex << setfill('0') << setw(8) << registers.gpr[9] << "\t";
  cout << "r10=0x" << hex << setfill('0') << setw(8) << registers.gpr[10] << "\t";
  cout << "r11=0x" << hex << setfill('0') << setw(8) << registers.gpr[11] << "\n";
  cout << "r12=0x" << hex << setfill('0') << setw(8) << registers.gpr[12] << "\t";
  cout << "r13=0x" << hex << setfill('0') << setw(8) << registers.gpr[13] << "\t";
  cout << "r14=0x" << hex << setfill('0') << setw(8) << registers.gpr[14] << "\t";
  cout << "r15=0x" << hex << setfill('0') << setw(8) << registers.gpr[15] << "\n";
}

int main(int argc, char* argv[]) {

  //Inicijalizacija memorije
  off_t memSize = 0x100000000;
  void* mapped_memory = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_NORESERVE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (mapped_memory == MAP_FAILED) {
    perror("mmap failed");
    exit(EXIT_FAILURE);
  }
  memory = static_cast<unsigned char*>(mapped_memory);

  ifstream inFile(argv[1]);
  string line;
  string address;
  string chr;
  unsigned long addr;
  unsigned int charToLoad;

  //Ucitaj sadrzaj fajla u memoriju
  if(inFile.is_open()){ 
    while (getline(inFile,line)) {
      address = line.substr(0, 8);
      addr = stoul(address, nullptr, 16);
      int i = 10;
      while (i < line.size()) {
        chr = line.substr(i, 2);
        charToLoad = stoi(chr, nullptr, 16);
        memory[addr++] = charToLoad;
        i += 3;
      }
    }
    inFile.close();
  }
  else {
    cerr << "Nije moguce ucitati memoriju!\n";
    exit(-1);
  }

  //Inicijalizacija registara
  for (int i = 0; i < 15; i++) {
    cpu_registers.gpr[i] = 0;
  }
  cpu_registers.gpr[pc] =  0x40000000U;
  cpu_registers.csr[status] = 0;
  cpu_registers.csr[handler] = 0;
  cpu_registers.csr[cause] = 0;


  //Sacuvaj originalna podesavanja za terminal
  struct termios original_settings;
  tcgetattr(STDIN_FILENO, &original_settings);

  //Iskljuci echo
  struct termios new_settings = original_settings;
  new_settings.c_lflag &= ~(ICANON | ECHO); 
  tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);

  //Stdin neblokirajuci
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  //Iskljuci baferisanje
  setvbuf(stdout, nullptr, _IONBF, 0);

  unsigned char* monitoredLoc = memory + term_out;
  writeToTermOut = false;

  unsigned int instr;
  unsigned char opcode, mode, A, B, C;
  int D;
  bool running = true;

  while(running) {

    //ucitavanje instrukcije
    instr = (memory[cpu_registers.gpr[pc]] << 24);
    instr |= (memory[cpu_registers.gpr[pc]+1] << 16);
    instr |= (memory[cpu_registers.gpr[pc]+2] << 8);
    instr |= memory[cpu_registers.gpr[pc]+3];
    cpu_registers.gpr[pc] += 4; //pc se odmah povecava

    opcode = (instr & 0xf0000000U) >> 28; //Operacioni kod instrukcije
    mode = (instr & 0xf000000U) >> 24; //Mod za instrukcije
    A = (instr & 0xf00000U) >> 20; 
    B = (instr & 0xf0000U) >> 16; 
    C = (instr & 0xf000U) >> 12; 
    D = ((instr & 0xf00U) >> 8) | ((instr & 0xff) << 4);
    if((D & negDispMask) != 0) D |= 0xfffff000; //Prosiri znak D 

    //Sada gledamo koja je instrukcija
    switch(opcode) {

      case oc::halt: { //Zaustavljanje procesora
        //Da li je korektan format instrukcije
        if(instr == 0) running = false; 
        else illegalInterrupt(); //Nelegalna instrukcija
        break;
      }

      case oc::interrupt: { //Softverski prekid
        //Da li je korektan format instrukcije
        if(instr == 0x10000000U) { 
          if((cpu_registers.csr[status] & statusMasks::allDisable) == 0) { 
            //Ako su prekidi omoguceni, skoci na prekidnu rutinu
            cpu_registers.gpr[sp] -= 4;
            writeToMemory(cpu_registers.csr[status], cpu_registers.gpr[sp]); 
            cpu_registers.gpr[sp] -= 4;
            writeToMemory(cpu_registers.gpr[pc], cpu_registers.gpr[sp]); 
            cpu_registers.csr[status] |= statusMasks::allDisable; 
            cpu_registers.csr[cause] = causeMasks::software; 
            cpu_registers.gpr[pc] = cpu_registers.csr[handler]; 

          }
        }
        else illegalInterrupt(); //Nelegalna instrukcija
        break;
      }

      case oc::call: { //Poziv potprograma
        if(C != 0) illegalInterrupt(); //Nelegalna instrukcija, C u formatu je 0
        else { 
          switch(mode) {
            case callModes::noMem: {
              cpu_registers.gpr[sp] -= 4;
              writeToMemory(cpu_registers.gpr[pc], cpu_registers.gpr[sp]); //Push pc
              cpu_registers.gpr[pc] = cpu_registers.gpr[A] + cpu_registers.gpr[B] + D;
              break;
            }
            case callModes::mem: {
              cpu_registers.gpr[sp] -= 4;
              writeToMemory(cpu_registers.gpr[pc], cpu_registers.gpr[sp]); //Push pc
              cpu_registers.gpr[pc] = loadFromMemory(cpu_registers.gpr[A] + cpu_registers.gpr[B] + D);
              break;
            }
            default: { //Negalna instrukcija, mod nije podrzan
              illegalInterrupt();
              break;
            }
          }
        }  
        break;
      }

      case oc::jmp: { //Skokovi na adresu
        switch(mode) {
          case jmpModes::jmpNoMem: {
            cpu_registers.gpr[pc] = cpu_registers.gpr[A] + D;
            break;
          }
          case jmpModes::beqNoMem: {
            if(cpu_registers.gpr[B] == cpu_registers.gpr[C]) cpu_registers.gpr[pc] = cpu_registers.gpr[A] + D;
            break;
          }
          case jmpModes::bneNoMem: {
            if(cpu_registers.gpr[B] != cpu_registers.gpr[C]) cpu_registers.gpr[pc] = cpu_registers.gpr[A] + D;
            break;
          }
          case jmpModes::bgtNoMem: {
            if(cpu_registers.gpr[B] > cpu_registers.gpr[C]) cpu_registers.gpr[pc] = cpu_registers.gpr[A] + D;
            break;
          }
          case jmpModes::jmpMem: {
            cpu_registers.gpr[pc] = loadFromMemory(cpu_registers.gpr[A] + D);
            break;
          }
          case jmpModes::beqMem: {
            if(cpu_registers.gpr[B] == cpu_registers.gpr[C]) cpu_registers.gpr[pc] = loadFromMemory(cpu_registers.gpr[A] + D);
            break;
          }
          case jmpModes::bneMem: {
            if(cpu_registers.gpr[B] != cpu_registers.gpr[C]) cpu_registers.gpr[pc] = loadFromMemory(cpu_registers.gpr[A] + D);
            break;
          }
          case jmpModes::bgtMem: {
            if(cpu_registers.gpr[B] > cpu_registers.gpr[C]) cpu_registers.gpr[pc] = loadFromMemory(cpu_registers.gpr[A] + D);
            break;
          }
          default: { //Negalna instrukcija, mod nije podrzan
            illegalInterrupt();
            break;
          }
        }
        break;
      }

      case oc::xchg: { //Zamena vrednosti
        if(mode != 0 || A != 0 || D != 0) illegalInterrupt(); //Nelegalna instr, u formatu sve ovo je 0
        else {
          int temp = cpu_registers.gpr[B];
          cpu_registers.gpr[B] = cpu_registers.gpr[C];
          cpu_registers.gpr[C] = temp;
        }
        break;
      }

      case oc::arit: { //Aritmeticke instrukcije
        if(D != 0) illegalInterrupt(); //Nelegalna instrukcija, u formatu D je 0
        else {
          switch(mode) {
            case aritModes::add: {
              cpu_registers.gpr[A] = cpu_registers.gpr[B] + cpu_registers.gpr[C];
              break;
            }
            case aritModes::sub: {
              cpu_registers.gpr[A] = cpu_registers.gpr[B] - cpu_registers.gpr[C];
              break;
            }
            case aritModes::mul: {
              cpu_registers.gpr[A] = cpu_registers.gpr[B] * cpu_registers.gpr[C];
              break;
            }
            case aritModes::divide: {
              cpu_registers.gpr[A] = cpu_registers.gpr[B] / cpu_registers.gpr[C];
              break;
            }
            default: { //Nelegalna instrukcija, mod nije podrzan
              illegalInterrupt();
              break;
            }
          }
        }
        break;
      }

      case oc::logic: { //Logicke instrukcije
        if(D != 0) illegalInterrupt(); //Nelegalna instrukcija, u formatu D je 0
        else {
          switch(mode) {
            case logicModes::lnot: {
              cpu_registers.gpr[A] = !cpu_registers.gpr[B];
              break;
            }
            case logicModes::land: {
              cpu_registers.gpr[A] = cpu_registers.gpr[B] & cpu_registers.gpr[C];
              break;
            }
            case logicModes::lor: {
              cpu_registers.gpr[A] = cpu_registers.gpr[B] | cpu_registers.gpr[C];
              break;
            }
            case logicModes::lxor: {
              cpu_registers.gpr[A] = cpu_registers.gpr[B] ^ cpu_registers.gpr[C];
              break;
            }
            default: { //Nelegalna instrukcija, mod nije podrzan
              illegalInterrupt();
              break;
            }
          }
        }
        break;
      }

      case oc::shift: { //Pomeracke instrukcije
        if(D != 0) illegalInterrupt(); //Nelegalna instrukcija, u formatu D je 0
        else {
          switch(mode) {
            case shiftModes::left: {
              cpu_registers.gpr[A] = cpu_registers.gpr[B] << cpu_registers.gpr[C];
              break;
            }
            case shiftModes::right: {
              cpu_registers.gpr[A] = cpu_registers.gpr[B] >> cpu_registers.gpr[C];
              break;
            }
            default: { //Nelegalna instrukcija, mod nije podrzan
              illegalInterrupt();
              break;
            }
          }
        }
        break;
      }

      case oc::store: { //Instrukcije smestanja podatka
        switch(mode) {
          case storeModes::memdir: {
            writeToMemory(cpu_registers.gpr[C], cpu_registers.gpr[A] + cpu_registers.gpr[B] + D);
            break;
          }
          case storeModes::push: {
            cpu_registers.gpr[A] += D;
            writeToMemory(cpu_registers.gpr[C], cpu_registers.gpr[A]);
            break;
          }
          case storeModes::memind: {
            unsigned int addr = loadFromMemory(cpu_registers.gpr[A] + cpu_registers.gpr[B] + D);
            writeToMemory(cpu_registers.gpr[C], addr);
            break;
          }
          default: { //Nelegalna instrukcija, mod nije podrzan
            illegalInterrupt();
            break;
          }
        }
        break;
      }

      case oc::load: { //Insrukcije ucitavanja podatka
        switch(mode) {
          case loadModes::csrrd: {
            if(B > 2) illegalInterrupt(); //Nelegalna instrukcija, najveci indeks za sist. registre je 2
            else cpu_registers.gpr[A] = A != 0 ? cpu_registers.csr[B] : 0;
            break;
          }
          case loadModes::regdirDisp: {
            cpu_registers.gpr[A] = A != 0 ? cpu_registers.gpr[B] + D : 0;
            break;
          }
          case loadModes::regindPom: {
            cpu_registers.gpr[A] = A != 0 ? loadFromMemory(cpu_registers.gpr[B] + cpu_registers.gpr[C] + D) : 0;
            break;
          }
          case loadModes::gprPop: {
            cpu_registers.gpr[A] = loadFromMemory(cpu_registers.gpr[B]);
            cpu_registers.gpr[B] += D;
            break;
          }
          case loadModes::csrwr: {
            if(A > 2) illegalInterrupt(); //Nelegalna instrukcija, najveci indeks za sist. registre je 2
            else cpu_registers.csr[A] = cpu_registers.gpr[B];
            break;
          }
          case loadModes::csrdirOrD: {
            if(A > 2 || B > 2) illegalInterrupt(); //Nelegalna instrukcija, najveci indeks za sist. registre je 2
            else cpu_registers.csr[A] = cpu_registers.csr[B] | D;
            break;
          }
          case loadModes::csrRegindPom: {
            if(A > 2) illegalInterrupt(); //Nelegalna instrukcija, najveci indeks za sist. registre je 2
            else cpu_registers.csr[A] = loadFromMemory(cpu_registers.gpr[B] + cpu_registers.gpr[C] + D);
            break;
          }
          case loadModes::csrPop: {
            if(A > 2) illegalInterrupt(); //Nelegalna instrukcija, najveci indeks za sist. registre je 2
            else {
              cpu_registers.csr[A] = loadFromMemory(cpu_registers.gpr[B]);
              cpu_registers.gpr[B] += D;
            }
            break;
          }
          default: { //Nelegalna instrukcija, mod nije podrzan
            illegalInterrupt();
            break;
          }
        }
        break;
      }
      default: { //Nelegalna instrukcija, mod nije podrzan
        illegalInterrupt();
        break;
      }
    }

    char ch;
    //Proveri da li je pritisnuto dugme
    if ((read(STDIN_FILENO, &ch, 1) == 1) && running) { 

        memory[term_in] = ch; //Upisi karakter u term_in registar
        memory[term_in+1] = 0;
        memory[term_in+2] = 0;
        memory[term_in+3] = 0; 
        if((cpu_registers.csr[status] & (statusMasks::terminalDisable | statusMasks::allDisable)) == 0) { 
          //Ako su prekidi omoguceni, skoci na prekidnu rutinu
          cpu_registers.gpr[sp] -= 4;
          writeToMemory(cpu_registers.csr[status], cpu_registers.gpr[sp]); 
          cpu_registers.gpr[sp] -= 4;
          writeToMemory(cpu_registers.gpr[pc], cpu_registers.gpr[sp]); 
          cpu_registers.csr[status] |= statusMasks::allDisable; 
          cpu_registers.csr[cause] = causeMasks::terminal; 
          cpu_registers.gpr[pc] = cpu_registers.csr[handler]; 

        }
    }

    // Proveri da li je ista upisano u term_out
    if (writeToTermOut && running) {
        putchar(*monitoredLoc);
        writeToTermOut = false;
    }
  }

  // Vrati originalna podesavanja terminala
  setbuf(stdout, nullptr);
  tcsetattr(STDIN_FILENO, TCSANOW, &original_settings);

  printState(cpu_registers);

  //Oslobadjanje memorije
  if (munmap(mapped_memory, memSize) == -1) {
    perror("munmap failed");
    exit(EXIT_FAILURE);
  }

  return 0;
}