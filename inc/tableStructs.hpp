//Strukture za tabele asemblera i linkera

#include <vector>
#include <deque>
#include <map>
#include <iostream>
#include <string>

enum opmode { //Sintaksne notacije za operand
  REGDIR = 0x00U, //Registarsko direktno
  REGIND = 0x01U, //Registarsko indirektno
  IMMEDLIT = 0x10U, //Neposredno sa literalom
  IMMEDSYMB = 0x11U, //Neposredno sa simbolom
  MEMDIRLIT = 0x20U, //Memorijsko direktno sa literalom
  MEMDIRSYMB = 0x21U, //Memorijsko direktno sa simbolom
  REGINDPOMLIT = 0x30U, //Registrasko indirektno sa pomerajem literal
  REGINDPOMSYMB = 0x31U, //Registarsko indirektno sa pomerajem simbol
  NOTOP = 0xFFU //Nije operand (Koristi se kod direktiva)
};

enum symbtype { //Tip simbola
  LCL = 0b001U, //Lokalni
  GLBL = 0b010U, //Globalni
  EXTRN = 0b100U //Eksterni
};

enum argtype { //Tip argumenta
  REGISTER = 0b00U, //Registar
  LITERAL = 0b01U, //Literal
  SYMBOL = 0b10U //Simbol
};

enum reloctype { //Tip relokacije
  RLCLA = 0b01U, //Lokalna apsolutna
  RGBLA = 0b10U //Globalna apsolutna
};

extern unsigned int linkerIndexCtr; //Brojac za linker broj simbola
extern unsigned int indexCtr; //Brojac za broj simbola

typedef struct args { //Argumenti instrukcije
  opmode operandAdressMode; //Nacin adresiranja operanda u instrukcijama

  //Vise argumenata u instr ili direktivi
  std::deque<std::string*>* argNames; //Imena argumenata instr i direktiva
  std::deque<argtype>* argTypes; //Tipovi argumenata instr i direktiva
  
  args(opmode opam, std::string* argn, argtype argt) {
    this->operandAdressMode = opam;
    this->argNames = new std::deque<std::string*>();
    this->argTypes = new std::deque<argtype>();
    this->argNames->push_back(argn);
    this->argTypes->push_back(argt);
  };

  ~args() {
    for (auto iter : *argNames) {
      delete iter;
    }
    delete argNames;
    delete argTypes;
  }

} Args;

typedef struct symbTRow { //Red u tabeli simbola
  unsigned int index; //Redni broj u tabeli simbola
  unsigned int section; //Koja sekcija - 0 rez za nedef symb; 1 rez za aps symb
  unsigned int value; //Vrednost simbola
  symbtype type; //Tip simbola


  symbTRow(unsigned int lc, unsigned int sec, symbtype type) {
    this->value = lc;
    this->section = sec;
    this->type = type;
    this->index = indexCtr++;
  }

  void printRow() {
    std::cout << index << "\t" << type << "\t" << section << "\t" << value << "\t" << std::endl;
  }

} SymbTRow;

typedef struct relocTRow { //Red u tabeli relokacija
  std::string symbName; //Ime simbola koji se relocira, treba mi za pretragu
  unsigned int index; //Broj simbola koji se relocira
  unsigned int section; //U kojoj sekciji
  unsigned int location; //Na kojoj adresi se radi relokacija
  unsigned int addend; //Addend 
  reloctype type;

  relocTRow(std::string symbName, unsigned int index, unsigned int location, unsigned int section, unsigned int addend, reloctype type) {
    this->symbName = symbName;
    this->index = index;
    this->location = location;
    this->section = section;
    this->addend = addend;
    this->type = type;
  }

  void printRow() {
    std::cout << index << "\t" << section << "\t" << location << "\t" << addend << std::endl;
  }
  
} RelocTRow;

typedef struct section { //Sekcija
  std::string* name; //Ime sekcije (labela)
  unsigned int symbolEntry; //Rbr simbola u tabeli simbola za sekciju
  unsigned int loc; //Za azuriranje LC u drugom prolazu
  unsigned int size; //Velicina sekcije u B
  std::vector<unsigned char>* contents; //Sadrzaj sekcije
  std::deque<RelocTRow*>* relocTable; //Tabela relokacija za ovu sekciju
  std::map<std::string, unsigned int>* literalTable; //Tabela literala za ovu sekciju

  section(std::string name, unsigned int symbolEntry) {
    this->name = new std::string(name);
    this->symbolEntry = symbolEntry;
    this->size = 0;
    this->loc = 0;
    this->contents = new std::vector<unsigned char>();
    this->relocTable = new std::deque<RelocTRow*>();
    this->literalTable = new std::map<std::string, unsigned int>();
  }

  void printSection() {
    int ctr = 0; 
    std::cout << "Ime sekcije: " << *name << std::endl;
    std::cout << "Indeks simbola u tabeli simbola: " << symbolEntry << std::endl;
    std::cout << "Velicina: " << size << std::endl;
    std::cout << "Sadrzaj:" << std::endl;
    for(auto c : *contents) {
      printf("%02x ", c);
      if(++ctr == 8) {
        printf("\n");
        ctr = 0;
      }
    }
    std::cout << "\nTabela literala\n";
    std::cout << "Vrednost\t" << "Adresa\n";
    for(auto iter : *literalTable) {
      std::cout << iter.first << "\t" << iter.second << std::endl;
    }
    std::cout << "\nRelokacioni zapisi za sekciju:\n";
    std::cout << "Indeks\t" << "Sekcija\t" << "Lokacija\t" << "Addend\n";
    for(auto iter : *relocTable) {
      iter->printRow();
    }
  }

  ~section() {
    delete name;
    for(auto iter : *relocTable) {
      delete iter;
    }
    delete contents;
    delete relocTable;
    delete literalTable;
  }

} Section;


typedef struct linkerSTRow { //Red u tabeli simbola u linkeru 
  unsigned int index; //Rbr u tabeli simbola
  unsigned int value; //Virtuelna adresa (ili pomeraj u odnosu na pocetak sekcije zavisi od opcije)
  symbtype type; //Tip simbola
  unsigned int size; //Velicina sekcije - 0 ako nije sekcija
  unsigned int section; //Kojoj sekciji pripada
  bool placed; //Da li se smesta na posebnoj adresi - samo za sekcije
  std::deque<RelocTRow*>* relocTable; //Tabela relokacija za ovu sekciju - treba samo ako se pravi relocatable fajl
  std::vector<unsigned char>* contents; //Sadrzaj sekcije

  linkerSTRow(unsigned int value, symbtype type, unsigned int size, bool placed) { //Za sekcije
    this->index = linkerIndexCtr++;
    this->value = value;
    this->type = type;
    this->size = size;
    this->section = index;
    this->placed = placed;
    this->contents = nullptr;
    this->relocTable = nullptr;
  }

  linkerSTRow(unsigned int value, symbtype type, unsigned int section) { //Za simbole
    this->index = linkerIndexCtr++;
    this->value = value;
    this->type = type;
    this->size = 0;
    this->section = section;
    this->placed = false;
    this->contents = nullptr;
    this->relocTable = nullptr;
  }

  ~linkerSTRow() {
    if(contents != nullptr) delete contents;
    if(relocTable != nullptr) {
      for (auto iter : *relocTable) {
        delete iter;
      }
      delete relocTable;
    }
  }

  void printLinkerSymbol() {
    int ctr = 0;
    std::cout << "Indeks simbola u tabeli simbola: " << index << std::endl;
    std::cout << "Adresa (pocetka sekcije) simbola " << value << std::endl;
    std::cout << "Kojoj sekciji pripada " << section << std::endl;
    std:: cout << "Place komanda? " << placed << std::endl;
    std::cout << "Velicina: " << size << std::endl;
    if(contents != nullptr) {
      std::cout << "Sadrzaj:" << std::endl;
      for(auto c : *contents) {
        printf("%02x ", c);
        if(++ctr == 8) {
          printf("\n");
          ctr = 0;
        }
      }
    }
    std::cout << "\n";
    if(relocTable != nullptr) {
      std::cout << "\nRelokacioni zapisi za sekciju:\n";
      std::cout << "Indeks\t" << "Sekcija\t" << "Lokacija\t" << "Addend\n";
      for(auto iter : *relocTable) {
        iter->printRow();
      }
    }
  }

} LinkerSTRow;

typedef struct fileTracker { //Prati iz kog fajla je koja sekcija (za linker)
  std::string sectionName; //Ime sekcije
  unsigned int inSymbIndex; //Index iz ulaznog fajla
  unsigned int inFileIndex; //Iz kog ulaznog ulaznog fajla
  unsigned int size; //Velicina sekcije
  unsigned int start; //Odakle pocinje ova sekcija u izlaznom fajlu

  fileTracker (std::string sectionName, unsigned int inSymbIndex, unsigned int inFileIndex, unsigned int size, unsigned int start) {
    this->sectionName = sectionName;
    this->inSymbIndex = inSymbIndex;
    this->inFileIndex = inFileIndex;
    this->size = size;
    this->start = start;
  }

  void printTracker() {
    std::cout << sectionName << "\t" << inSymbIndex << "\t" << inFileIndex << "\t" << size << "\t" << start << std::endl;
  }

} FileTracker;