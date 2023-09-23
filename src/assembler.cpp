#include "../inc/assembler.hpp"
#include <iomanip>
#include <fstream>
using namespace std;

extern map<string, SymbTRow*>* symbolTable;
extern map<int, Section*>* allSections;
extern unsigned int lc;
extern unsigned int lineNum;
extern bool isFirstPass;
extern Section* currentSection;
extern string outputName;

//==============================================Pomocne fje=============================================

long litToL(const string* literal) { //Pretvara literal u long
  int base = 10;
  if((*literal)[1] == 'x') base = 16;
  else if ((*literal)[0] == '0') base = 8;
  return std::stoul(*literal, nullptr, base);
}

void addLit(string* literal) {
  if(currentSection != nullptr) {
    auto litTRow = currentSection->literalTable->find(*literal);
    //Dodaj u tabelu literala sa vrednoscu 0 (prepravice se posle)
    if(litTRow == currentSection->literalTable->end()) {
      currentSection->literalTable->insert({*literal, 0});
    } 
  }
}

int checkSection(string* label) { //Proverava da li je simbol u istoj sekciji kao i instr koja ga koristi
  auto symbolIter = symbolTable->find(*label);
  if(symbolIter == symbolTable->end()) { //Ako simbol nije definisan u tabeli simbola dici gresku
    freeMem();
    cerr << "Greska tokom asembliranja na liniji " << lineNum << "! Koriscenje nedefinisanog simbola " << *label << "!\n";
    exit(-1);
  }
  return (symbolIter->second->section == currentSection->symbolEntry);
}

unsigned int literalOffset(string* literal) {
  auto litTRow = currentSection->literalTable->find(*literal); //Pronadji literal u tabeli literala
  return litTRow->second - lc - 4; //Offset do njegove adrese
}

//==============================================Oslobadjanje mem=============================================

void freeMem() {
  for(auto iter : *symbolTable) {
    delete iter.second;
  }
  delete symbolTable;
  for(auto iter : *allSections) {
    delete iter.second;
  }
  delete allSections;
}

//==============================================Def simbola=============================================

void symbDef(string* label) {
  auto trow = symbolTable->find(*label);
  if(trow != symbolTable->end()) { //Nasao simbol
    if(trow->second->section != 0) { //Simbol vec definisan - asembler baca gresku
      freeMem();
      delete label;
      cerr << "Greska tokom asembliranja na liniji " << lineNum << "! Visestruka definicija simbola!\n";  //cerr ne baferise nista, mada moze i cout vrv ovde
      exit(-1);
    }
    else { //Ako je u tabeli simb preko .global i ili .extern a naknadno se definise
      trow->second->section = currentSection->symbolEntry;
      trow->second->value = lc;
    }
  }
  else { //Nije u tabeli simbola
      symbTRow* newRow = new SymbTRow(lc, currentSection->symbolEntry, symbtype::LCL);
      symbolTable->insert({*label, newRow});
  }
}

//==============================================Direktive=============================================

void dirGlobal(Args* arguments) {
  for(int i = 0; i < arguments->argNames->size(); i++) { //Sve simbole u listi obelezava kao global
    auto trow = symbolTable->find(*(*(arguments->argNames))[i]);
    if(trow != symbolTable->end()) { //Simbol je vec definisan, treba ga obeleziti da je global
      trow->second->type = symbtype::GLBL;
    }
    else { //Simbol nije definisan, dodati u tabelu kao global undefined
      symbTRow* newRow = new symbTRow(0, 0, symbtype::GLBL);
      symbolTable->insert({*(*(arguments->argNames))[i], newRow});
    }
  }
}

void dirExtern(Args* arguments) {
  for(int i = 0; i < arguments->argNames->size(); i++) { //Sve simbole u listi obelezava kao extern
    auto trow = symbolTable->find(*(*(arguments->argNames))[i]);
    if(trow != symbolTable->end()) { //Simbol je vec definisan, treba ga obeleziti da je extern
      trow->second->type = symbtype::EXTRN;
    }
    else { //Simbol nije definisan, dodati u tabelu kao extern
      symbTRow* newRow = new symbTRow(0, 0, symbtype::EXTRN);
      symbolTable->insert({*(*(arguments->argNames))[i], newRow});
    }
  }
}

void dirSection(string* label) {
  if(currentSection != nullptr) { //Sacuvaj velicinu prethodne sekcije
    if(isFirstPass)currentSection->size = lc;
    else currentSection->loc = lc;
  }
  auto symbtrow = symbolTable->find(*label);
  if(symbtrow != symbolTable->end()) { //Ako sekcija vec postoji
    auto sectrow = allSections->find(symbtrow->second->index);
    currentSection = sectrow->second;
    if(isFirstPass) lc = currentSection->size;
    else lc = currentSection->loc;
  }
  else { //Ako ne postoji
    lc = 0;
    SymbTRow* newRow = new SymbTRow(lc, 0, symbtype::LCL);
    newRow->section = newRow->index;
    symbolTable->insert({*label, newRow});
    Section* newSection = new Section(*label, newRow->index);
    currentSection = newSection;
    allSections->insert({newRow->index, newSection});
  }
}

void dirWord(Args* arguments) {
  if(currentSection != nullptr) {
    for(int i = 0; i < arguments->argNames->size(); i++) {
      if((*(arguments->argTypes))[i] == argtype::LITERAL) { //Ako je literal, upisi vrednost u sadrzaj
        long number = litToL((*(arguments->argNames))[i]);
        currentSection->contents->push_back((unsigned char)(number & 0xff));
        currentSection->contents->push_back((unsigned char)((number & 0xff00) >> 8));
        currentSection->contents->push_back((unsigned char)((number & 0xff0000) >> 16));
        currentSection->contents->push_back((unsigned char)((number & 0xff000000) >> 24));
        lc+=4;
      }
      else { //Ako je simbol, upisi nule
        for(int i = 0; i < 4; i++) {
          currentSection->contents->push_back(0x00);
        }
        //Napravi relok zapis
        auto symbTrow = symbolTable->find(*(*(arguments->argNames))[0]);
        if(symbTrow != symbolTable->end()) {
          switch(symbTrow->second->type) {
              case symbtype::EXTRN:
              case symbtype::GLBL: {
                RelocTRow* newReloc = new RelocTRow(symbTrow->first, symbTrow->second->index, lc, currentSection->symbolEntry, 0, reloctype::RGBLA);
                currentSection->relocTable->push_back(newReloc);
              }
              case symbtype::LCL: {
                RelocTRow* newReloc = new RelocTRow(*currentSection->name, symbTrow->second->section, lc, currentSection->symbolEntry, symbTrow->second->value, reloctype::RLCLA);
                currentSection->relocTable->push_back(newReloc);
              }
            }
        }
        else {
          freeMem();
          cerr << "Greska tokom asembliranja na liniji " << lineNum << "! Koriscenje nedefinisanog simbola " << *(*(arguments->argNames))[0] << "!\n";
          exit(-1);
        }
        lc+=4;
      }
    }
  }
}

void dirSkip(string* literal) {
  if(currentSection != nullptr) {
    long bytes = litToL(literal);
    lc += bytes;
    if(!isFirstPass) { //Samo u drugom prolazu se stvara masinski kod
      for(int i = 0; i < bytes; i++) {
        currentSection->contents->push_back(0);
      }
    }
  }
}

void dirAscii(string* str) {
  if(currentSection != nullptr) {
    for(int i = 0; i < str->length(); i++) {
      currentSection->contents->push_back((*str)[i]);
    }
  }
}

void dirEnd() {
  if(currentSection != nullptr && isFirstPass) { //Sacuvaj velicinu poslednje sekcije u prvom prolazu
    currentSection->size = lc;
  }
  if(isFirstPass) { //Posao za prvi prolaz
    for(auto iter : *symbolTable) {
      if(iter.second->type != symbtype::EXTRN && iter.second->section == 0) {
        //Nije definisan simbol koji nije extern, dici gresku
        freeMem();
        cerr << "Greska tokom asembliranja! Nedostaje definicija simbola koji nije obelezen kao extern " << iter.first << endl;
        exit(-1);
      }
    }
    for(auto iter = allSections->begin(); iter != allSections->end(); iter++) {
      //Azuriraj adrese u tabeli literala za ovu sekciju
      for(auto literalIter = iter->second->literalTable->begin(); literalIter != iter->second->literalTable->end();) {
        auto symbolIter = symbolTable->find(literalIter->first);
        if(symbolIter != symbolTable->end() && symbolIter->second->section == iter->second->symbolEntry) {
          literalIter = iter->second->literalTable->erase(literalIter);
        }
        else {
          literalIter->second = iter->second->size;
          iter->second->size += 4;
          literalIter++;
        }
      }
      //Provera da li moze bazen da stane u sekciju
      if(iter->second->size > 2047) {
        freeMem();
        cerr << "Greska tokom asembliranja! Prevelika velicina sekcije - ne moze se napraviti bazen literala!\n";
        exit(-1);
      }
    }
  }
  else { //Posao za drugi prolaz
    for(auto iter = allSections->begin(); iter != allSections->end(); iter++) { 
      //Generisanje bazena literala za svaku sekciju
      for(auto litIter : *iter->second->literalTable) {
        if((isdigit(litIter.first[0]) != 0) || litIter.first[0] == '-') { //Dodaje se literal u bazen
          long number = litToL(&(litIter.first));
          iter->second->contents->push_back((unsigned char)(number & 0xff));
          iter->second->contents->push_back((unsigned char)((number & 0xff00) >> 8));
          iter->second->contents->push_back((unsigned char)((number & 0xff0000) >> 16));
          iter->second->contents->push_back((unsigned char)((number & 0xff000000) >> 24));
        }
        else { 
          //Dodaje se simbol i relok zapis u bazen, ako nije izracunljiv, ako jeste dodaje se njegova vrednost
          for(int i = 0; i < 4; i++) {
            iter->second->contents->push_back(0x00);
          }
          auto symbTrow = symbolTable->find(litIter.first);
          if(symbTrow != symbolTable->end()) {
            switch(symbTrow->second->type) {
              case symbtype::EXTRN:
              case symbtype::GLBL: {
                RelocTRow* newReloc = new RelocTRow(litIter.first, symbTrow->second->index, litIter.second, iter->second->symbolEntry, 0, reloctype::RGBLA);
                iter->second->relocTable->push_back(newReloc);
                break;
              }
              case symbtype::LCL: {
                RelocTRow* newReloc = new RelocTRow(*iter->second->name, symbTrow->second->section, litIter.second, iter->second->symbolEntry, symbTrow->second->value, reloctype::RLCLA);
                iter->second->relocTable->push_back(newReloc);
                break;
              }
            }  
          }
          else {
            freeMem();
            cerr << "Greska tokom asembliranja! Koriscenje nedefinisanog simbola " << litIter.first << "!\n";
            exit(-1);
          }
        }
      } 
    }

    //FORMAT IZLAZNOG FAJLA:
    //1. sadrzaj svih sekcija
    //2. tabela simbola
    //3. sve tabele relokacija
    //Ovaj format je najbolji za linker zato sto prati redosled radnji linkerske skripte
    //Linker moze samo redom da cita fajl i da radi svoj posao
    ofstream outFile;
    outFile.open(outputName, ios::binary);
    if(outFile.is_open()){
      auto sectionIter = allSections->begin();
      auto symbolIter = symbolTable->begin();

      size_t size = allSections->size();
      outFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); //Upisi broj sekcija u fajl
      for(sectionIter ; sectionIter != allSections->end(); sectionIter++) {
        size = sectionIter->second->name->size();
        outFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); //Upisi velicinu stringa - ime sekcije
        outFile.write(sectionIter->second->name->data(), size); //Upisi string
        outFile.write(reinterpret_cast<char*>(&sectionIter->second->symbolEntry), sizeof(unsigned int)); 
        outFile.write(reinterpret_cast<char*>(&sectionIter->second->size), sizeof(unsigned int)); 
        outFile.write(reinterpret_cast<char*>(sectionIter->second->contents->data()), sectionIter->second->size); 
      }

      size = symbolTable->size();
      outFile.write(reinterpret_cast<char*>(&(size)), sizeof(size_t)); //Upisi broj simbola u tabeli simbola
      for(symbolIter; symbolIter != symbolTable->end(); symbolIter++) {
        size = symbolIter->first.size();
        outFile.write(reinterpret_cast<char*>(&(size)), sizeof(size_t)); //Upisi velicinu stringa - ime simbola
        outFile.write(symbolIter->first.data(), size); //Upisi string
        outFile.write(reinterpret_cast<char*>(&symbolIter->second->index), sizeof(unsigned int)); 
        outFile.write(reinterpret_cast<char*>(&symbolIter->second->section), sizeof(unsigned int)); 
        outFile.write(reinterpret_cast<char*>(&symbolIter->second->value), sizeof(unsigned int)); 
        outFile.write(reinterpret_cast<char*>(&symbolIter->second->type), sizeof(symbtype)); 
      }

      size = allSections->size();
      outFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); //Upisi broj relok tabela u fajl (Za svaku sekciju po jedna pa makar bila prazna)
      for(sectionIter = allSections->begin(); sectionIter != allSections->end(); sectionIter++) {
        size = sectionIter->second->relocTable->size();
        outFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); //Upisi broj relokacija u tabeli
        for(auto relocIter : *(sectionIter->second->relocTable)) {
          size = relocIter->symbName.size();
          outFile.write(reinterpret_cast<char*>(&(size)), sizeof(size_t)); //Upisi velicinu stringa - ime simbola
          outFile.write(relocIter->symbName.data(), size); //Upisi string
          outFile.write(reinterpret_cast<char*>(&relocIter->index), sizeof(unsigned int)); 
          outFile.write(reinterpret_cast<char*>(&relocIter->section), sizeof(unsigned int)); 
          outFile.write(reinterpret_cast<char*>(&relocIter->location), sizeof(unsigned int)); 
          outFile.write(reinterpret_cast<char*>(&relocIter->addend), sizeof(unsigned int));
          outFile.write(reinterpret_cast<char*>(&relocIter->type), sizeof(reloctype)); 
        }
      }
      outFile.close();
    }
  }
  currentSection = nullptr;
}

//==============================================Instrukcije=============================================

void instrHalt() {
  for(int i = 0; i < 4; i++) {
    currentSection->contents->push_back(0x00);
  }
}

void instrInt() {
  currentSection->contents->push_back(0x10);
  for(int i = 0; i < 3; i++) {
    currentSection->contents->push_back(0x00);
  }
}

void instrIret() {
  if(!isFirstPass) { //Posao za drugi prolaz
    //prva instrukcija - ld %status, [%sp + 4]
    currentSection->contents->push_back(0x96);
    currentSection->contents->push_back(0x0e);
    currentSection->contents->push_back(0x04);
    currentSection->contents->push_back(0x00);
    //druga instrukcija - ld %pc, [%sp] ; sp+=8
    currentSection->contents->push_back(0x93);
    currentSection->contents->push_back(0xfe);
    currentSection->contents->push_back(0x08);
    currentSection->contents->push_back(0x00); 
  }
  lc+=4;
}

void instrCall(string* jmpAddress) {
  if(isFirstPass) { //Posao za prvi prolaz
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //Ako je literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) addLit(jmpAddress); //Dodaj u bazen ako ne moze da stane na 3B
    }
    else addLit(jmpAddress); //Simbole dodaj
  }
  else { //Drugi prolaz
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //Literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) { //Mora bazen
        unsigned int offset = literalOffset(jmpAddress);
        currentSection->contents->push_back(0x21);
        currentSection->contents->push_back(0xf0);
        currentSection->contents->push_back((unsigned char)(offset & 0x0f));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Ugradi u instr
        currentSection->contents->push_back(0x20);
        currentSection->contents->push_back(0x00);
        currentSection->contents->push_back((unsigned char)(number & 0x0f));
        currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
      }
    }
    else { //Simbol
      if(checkSection(jmpAddress)) { //Ako su u istoj sekciji, ne treba bazen i relok zapis
        auto symbTrow = symbolTable->find(*jmpAddress);
        int offset = symbTrow->second->value - lc - 4; //Offset do simbola u sekciji
        currentSection->contents->push_back(0x20);
        currentSection->contents->push_back(0xf0);
        currentSection->contents->push_back((unsigned char)(offset & 0x0f));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Nisu u istoj sekciji, mora bazen i relok zapis
        unsigned int offset = literalOffset(jmpAddress); //Offset do simbola u tabeli
        currentSection->contents->push_back(0x21);
        currentSection->contents->push_back(0xf0);
        currentSection->contents->push_back((unsigned char)(offset & 0x0f));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
    }
  }
}

void instrRet() { //Isto kao pop %pc
  currentSection->contents->push_back(0x93);
  currentSection->contents->push_back(0xfe);
  currentSection->contents->push_back(0x04);
  currentSection->contents->push_back(0x00);
}

void instrJmp(string* jmpAddress) {
  if(isFirstPass) { //Posao za prvi prolaz
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //Ako je literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) addLit(jmpAddress); //Dodaj u bazen ako ne moze da stane na 3B
    }
    else addLit(jmpAddress); //Simbole dodaj
  }
  else { //Drugi prolaz
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //Literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) { //Mora bazen
        unsigned int offset = literalOffset(jmpAddress);
        currentSection->contents->push_back(0x38);
        currentSection->contents->push_back(0xf0);
        currentSection->contents->push_back((unsigned char)(offset & 0x0f));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Ugradi u instr
        currentSection->contents->push_back(0x30);
        currentSection->contents->push_back(0x00);
        currentSection->contents->push_back((unsigned char)(number & 0x0f));
        currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
      }
    }
    else { //Simbol
      if(checkSection(jmpAddress)) { //Ako su u istoj sekciji, ne treba bazen i relok zapis
        auto symbTrow = symbolTable->find(*jmpAddress);
        int offset = symbTrow->second->value - lc - 4; //Offset do simbola u sekciji
        currentSection->contents->push_back(0x30);
        currentSection->contents->push_back(0xf0);
        currentSection->contents->push_back((unsigned char)(offset & 0x0f));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Nisu u istoj sekciji, mora bazen i relok zapis
        unsigned int offset = literalOffset(jmpAddress); //Offset do simbola u tabeli
        currentSection->contents->push_back(0x38);
        currentSection->contents->push_back(0xf0);
        currentSection->contents->push_back((unsigned char)(offset & 0x0f));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
    }
  }
}

void instrBeq(string* registerSrc, string* registerDst, string* jmpAddress){
  if(isFirstPass) {
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //Ako je literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) addLit(jmpAddress); //Dodaj u bazen ako ne moze da stane na 3B
    }
    else addLit(jmpAddress); //Simbole dodaj
  }
  else { //Drugi prolaz
    unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
    unsigned int regDstNum = stoi((*registerDst).substr(1,2));
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //Literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) { //Mora bazen
        unsigned int offset = literalOffset(jmpAddress);
        currentSection->contents->push_back(0x39);
        currentSection->contents->push_back((unsigned char)(0xf0 | regSrcNum));
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (offset & 0x0f)));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Ugradi u instr
        currentSection->contents->push_back(0x31);
        currentSection->contents->push_back((unsigned char)regSrcNum);
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (number & 0x0f)));
        currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
      }
    }
    else { //Simbol
      if(checkSection(jmpAddress)) { //Ako su u istoj sekciji, ne treba bazen i relok zapis
        auto symbTrow = symbolTable->find(*jmpAddress);
        int offset = symbTrow->second->value - lc - 4; //Offset do simbola u sekciji
        currentSection->contents->push_back(0x31);
        currentSection->contents->push_back((unsigned char)(0xf0 | regSrcNum));
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (offset & 0x0f)));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Nisu u istoj sekciji, mora bazen i relok zapis
        unsigned int offset = literalOffset(jmpAddress); //Offset do simbola u tabeli
        currentSection->contents->push_back(0x39);
        currentSection->contents->push_back((unsigned char)(0xf0 | regSrcNum));
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (offset & 0x0f)));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
    }
  }
}

void instrBne(string* registerSrc, string* registerDst, string* jmpAddress){
  if(isFirstPass) {
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //Ako je literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) addLit(jmpAddress); //Dodaj u bazen ako ne moze da stane na 3B
    }
    else addLit(jmpAddress); //Simbole dodaj
  }
  else { //Drugi prolaz
    unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
    unsigned int regDstNum = stoi((*registerDst).substr(1,2));
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //Literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) { //Mora bazen
        unsigned int offset = literalOffset(jmpAddress);
        currentSection->contents->push_back(0x3a);
        currentSection->contents->push_back((unsigned char)(0xf0 | regSrcNum));
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (offset & 0x0f)));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Ugradi u instr
        currentSection->contents->push_back(0x32);
        currentSection->contents->push_back((unsigned char)regSrcNum);
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (number & 0x0f)));
        currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
      }
    }
    else { //Simbol
      if(checkSection(jmpAddress)) { //Ako su u istoj sekciji, ne treba bazen i relok zapis
        auto symbTrow = symbolTable->find(*jmpAddress);
        int offset = symbTrow->second->value - lc - 4; //Offset do simbola u sekciji
        currentSection->contents->push_back(0x32);
        currentSection->contents->push_back((unsigned char)(0xf0 | regSrcNum));
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (offset & 0x0f)));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Nisu u istoj sekciji, mora bazen i relok zapis
        unsigned int offset = literalOffset(jmpAddress); //Offset do simbola u tabeli
        currentSection->contents->push_back(0x3a);
        currentSection->contents->push_back((unsigned char)(0xf0 | regSrcNum));
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (offset & 0x0f)));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
    }
  }
}

void instrBgt(string* registerSrc, string* registerDst, string* jmpAddress){
  if(isFirstPass) {
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //Ako je literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) addLit(jmpAddress); //Dodaj u bazen ako ne moze da stane na 3B
    }
    else addLit(jmpAddress); //Simbole dodaj 
  }
  else { //Drugi prolaz
    unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
    unsigned int regDstNum = stoi((*registerDst).substr(1,2));
    if((isdigit((*jmpAddress)[0]) != 0) || (*jmpAddress)[0] == '-') { //literal
      long number = litToL(jmpAddress);
      if(number > 2047 || number < -2048) { //Mora bazen
        unsigned int offset = literalOffset(jmpAddress);
        currentSection->contents->push_back(0x3b);
        currentSection->contents->push_back((unsigned char)(0xf0 | regSrcNum));
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (offset & 0x0f)));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Ugradi u instr
        currentSection->contents->push_back(0x33);
        currentSection->contents->push_back((unsigned char)regSrcNum);
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (number & 0x0f)));
        currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
      }
    }
    else { //Simbol
      if(checkSection(jmpAddress)) { //Ako su u istoj sekciji, ne treba bazen i relok zapis
        auto symbTrow = symbolTable->find(*jmpAddress);
        int offset = symbTrow->second->value - lc - 4; //Offset do simbola u sekciji
        currentSection->contents->push_back(0x33);
        currentSection->contents->push_back((unsigned char)(0xf0 | regSrcNum));
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (offset & 0x0f)));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
      else { //Nisu u istoj sekciji, mora bazen i relok zapis
        unsigned int offset = literalOffset(jmpAddress); //Offset do simbola u tabeli
        currentSection->contents->push_back(0x3b);
        currentSection->contents->push_back((unsigned char)(0xf0 | regSrcNum));
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | (offset & 0x0f)));
        currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
      }
    }
  }
}

void instrPush(string* registerSrc) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  currentSection->contents->push_back(0x81);
  currentSection->contents->push_back(0xe0);
  currentSection->contents->push_back((unsigned char)((regSrcNum << 4) | 0x0c));
  currentSection->contents->push_back(0xff);
}

void instrPop(string* registerDst) {
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x93);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | 0x0e));
  currentSection->contents->push_back(0x04);
  currentSection->contents->push_back(0x00);
}

void instrXchg(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x40);
  currentSection->contents->push_back((unsigned char) regSrcNum);
  currentSection->contents->push_back((unsigned char)(regDstNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrAdd(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x50);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
  currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrSub(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x51);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
  currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrMul(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x52);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
  currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrDiv(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x53);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
  currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrNot(string* registerSrc) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  currentSection->contents->push_back(0x60);
  currentSection->contents->push_back((unsigned char)((regSrcNum << 4) | regSrcNum));
  currentSection->contents->push_back(0x00);
  currentSection->contents->push_back(0x00);
}

void instrAnd(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x61);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
  currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrOr(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x62);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
  currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrXor(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x63);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
  currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrShl(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x70);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
  currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrShr(string* registerSrc, string* registerDst) {
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x71);
  currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
  currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
  currentSection->contents->push_back(0x00);
}

void instrLd(Args* arguments, string* reg) {
  if(isFirstPass) { //Posao za prvi prolaz
    switch(arguments->operandAdressMode) {
      case opmode::IMMEDLIT:
      case opmode::MEMDIRLIT: {
        long number = litToL((*(arguments->argNames))[0]);
        if (number > 2047 || number < -2048) addLit((*(arguments->argNames))[0]);
        break;
      }
      case opmode::REGINDPOMLIT: {
        long number = litToL((*(arguments->argNames))[1]);
        if (number > 2047 || number < -2048) { //Ako je prevelik literal dize gresku
          delete arguments;
          delete reg;
          freeMem();
          cerr << "Greska tokom asembliranja na liniji " << lineNum << "! Literal je siri od 12b!\n";
          exit(-1);
        }
        break;
      }
      case opmode::IMMEDSYMB:
      case opmode::MEMDIRSYMB: {
        addLit((*(arguments->argNames))[0]);
        break;
      }
      case opmode::REGINDPOMSYMB: {
        //TODO: Dodaj kada uradis equ, da li je sekcija 1 i da li je vr simbola na 12b
      }
      default: break;
    }
  }
  else { //Posao za drugi prolaz - generisanje relok. zapisa i mas. koda
      switch(arguments->operandAdressMode) {
      case opmode::REGDIR: { //Registarsko direktno
        unsigned int regSrcNum = stoi((*(*(arguments->argNames))[0]).substr(1,2));
        unsigned int regDstNum = stoi((*reg).substr(1,2));
        currentSection->contents->push_back(0x91);
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regSrcNum));
        currentSection->contents->push_back(0x00);
        currentSection->contents->push_back(0x00);
        break;
      }
      case opmode::REGIND: { //Registarsko indirektno
        unsigned int regSrcNum = stoi((*(*(arguments->argNames))[0]).substr(1,2)); 
        unsigned int regDstNum = stoi((*reg).substr(1,2));
        currentSection->contents->push_back(0x92);
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regSrcNum));
        currentSection->contents->push_back(0x00);
        currentSection->contents->push_back(0x00);
        break;
      }
      case opmode::IMMEDLIT: { //Neposredno sa literalom
        long number = litToL((*(arguments->argNames))[0]); //Pretvori literal u long
        unsigned int regDstNum = stoi((*reg).substr(1,2)); 
        if (number > 2047 || number < -2048) { 
          //Ako broj ne moze da stane u instr, mora iz bazena literala da se dohvati
          unsigned int offset = literalOffset((*(arguments->argNames))[0]); //Racuna se offset do literala u bazenu
          currentSection->contents->push_back(0x92);
          currentSection->contents->push_back((unsigned char)((regDstNum << 4) | 0x0f));
          currentSection->contents->push_back((unsigned char)(offset & 0x0f));
          currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
        }
        else { //Broj se ugradjuje u instrukciju ako moze
          currentSection->contents->push_back(0x91);
          currentSection->contents->push_back((unsigned char)(regDstNum << 4));
          currentSection->contents->push_back((unsigned char)(number & 0x0f));
          currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
        }
        break;
      }
      case opmode::IMMEDSYMB: { //Neposredno sa simbolom
        unsigned int regDstNum = stoi((*reg).substr(1,2)); 
        if(checkSection((*(arguments->argNames))[0])) { //Ako su u istoj sekciji, ne treba bazen i relok zapis
          auto symbTrow = symbolTable->find(*(*(arguments->argNames))[0]);
          int offset = symbTrow->second->value - lc - 4; //Offset do simbola u sekciji
          currentSection->contents->push_back(0x91);
          currentSection->contents->push_back((unsigned char)((regDstNum << 4) | 0x0f));
          currentSection->contents->push_back((unsigned char)(offset & 0x0f));
          currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
        }
        else { //Nisu u istoj sekciji, mora bazen i relok zapis
          unsigned int offset = literalOffset((*(arguments->argNames))[0]); //Offset do simbola u tabeli
          currentSection->contents->push_back(0x92);
          currentSection->contents->push_back((unsigned char)((regDstNum << 4) | 0x0f));
          currentSection->contents->push_back((unsigned char)(offset & 0x0f));
          currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
        }
        break;
      }
      case opmode::MEMDIRLIT: { //Memdir sa literalom
        long number = litToL((*(arguments->argNames))[0]); //Pretvori literal u long
        unsigned int regDstNum = stoi((*reg).substr(1,2));
        if(number > 2047 || number < -2048) { 
          //Mora dve instrukcije, prva ucitava adresu iz bazena, druga pristupa sadrzaju na adresi
          unsigned int offset = literalOffset((*(arguments->argNames))[0]); //Offset do literala
          //ucitavamo adresu (literal) u registar
          currentSection->contents->push_back(0x92); 
          currentSection->contents->push_back((unsigned char)((regDstNum << 4) | 0x0f));
          currentSection->contents->push_back((unsigned char)(offset & 0x0f));
          currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
          //Ucitavamo sadrzaj sa te adrese u isti registar
          currentSection->contents->push_back(0x92);
          currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
          currentSection->contents->push_back(0x00);
          currentSection->contents->push_back(0x00);
        }
        else { //Dovoljna jedna instrukcija, adresa moze da se ugradi u instrukciju
          currentSection->contents->push_back(0x92); 
          currentSection->contents->push_back((unsigned char)(regDstNum << 4));
          currentSection->contents->push_back((unsigned char)(number & 0x0f));
          currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
        }
        break;
      }
      case opmode::MEMDIRSYMB: { //Memdir sa simbolom, mora dve instrukcije
        unsigned int regDstNum = stoi((*reg).substr(1,2)); 
        if(checkSection((*(arguments->argNames))[0])) { //Ako su u istoj sekciji, ne treba bazen i relok zapis
          auto symbTrow = symbolTable->find(*(*(arguments->argNames))[0]);
          int offset = symbTrow->second->value - lc - 4; //Offset do simbola u sekciji
          //Ucitaj vrednost simbola
          currentSection->contents->push_back(0x91);
          currentSection->contents->push_back((unsigned char)((regDstNum << 4) | 0x0f));
          currentSection->contents->push_back((unsigned char)(offset & 0x0f));
          currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
          //Ucitavamo sadrzaj sa te adrese u isti registar
          currentSection->contents->push_back(0x92);
          currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
          currentSection->contents->push_back(0x00);
          currentSection->contents->push_back(0x00);
        }
        else { //Nisu u istoj sekciji, mora bazen i relok zapis
          unsigned int offset = literalOffset((*(arguments->argNames))[0]); //Offset do simbola
          //Ucitaj adresu iz bazena
          currentSection->contents->push_back(0x92); 
          currentSection->contents->push_back((unsigned char)((regDstNum << 4) | 0x0f));
          currentSection->contents->push_back((unsigned char)(offset & 0x0f));
          currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
          //Ucitavamo sadrzaj sa te adrese u isti registar
          currentSection->contents->push_back(0x92);
          currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regDstNum));
          currentSection->contents->push_back(0x00);
          currentSection->contents->push_back(0x00);
        } 
        break;
      }
      case opmode::REGINDPOMLIT: { //Literal je sigurno na sirini od 12b, pukao bi asembler ranije da nije
        long number = litToL((*(arguments->argNames))[1]);
        unsigned int regSrcNum = stoi((*(*(arguments->argNames))[0]).substr(1,2));
        unsigned int regDstNum = stoi((*reg).substr(1,2));
        currentSection->contents->push_back(0x92);
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regSrcNum));
        currentSection->contents->push_back((unsigned char)(number & 0x0f));
        currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
        break;
      }
      case opmode::REGINDPOMSYMB: {
        //TODO: Uradi kada zavrsis .equ, sigurno ce biti na 12b i aps jer bi pukao asembler ranije da nije
      }
      default: break;
    }
  }
  //Ovo se radi u oba prolaza
  //Mora se lc povecati za jos 4 jer ova dva adresiranja zahtevaju dodatnu instrukciju
  if(arguments->operandAdressMode == opmode::MEMDIRLIT) {
    long number = litToL((*(arguments->argNames))[0]); //Pretvori literal u long
    if(number > 2047 || number < -2048) lc+=4; //Samo ako literal ne moze da stane u instr, trebace jos jedna
  }
  else if (arguments->operandAdressMode == opmode::MEMDIRSYMB) {
    lc+=4;
  }
}

void instrSt(string* reg, Args* arguments) {
  if(isFirstPass) { //Posao za prvi prolaz
    switch(arguments->operandAdressMode) {
      case opmode::IMMEDLIT:
      case opmode::IMMEDSYMB: {
        //Ako je immed sa st dizi gresku
        delete reg;
        delete arguments;
        freeMem();
        cerr << "Greska tokom asembliranja na liniji " << lineNum << "! St i immed u instrukciji!\n";
        exit(-1);
      }
      case opmode::MEMDIRLIT: {
        long number = litToL((*(arguments->argNames))[0]);
        if (number > 2047 || number < -2048) addLit((*(arguments->argNames))[0]);
        break;
      }
      case opmode::REGINDPOMLIT: {
        long number = litToL((*(arguments->argNames))[1]);
        if (number > 2047 || number < -2048) { //Ako je prevelik literal dize gresku
          delete arguments;
          delete reg;
          freeMem();
          cerr << "Greska tokom asembliranja na liniji " << lineNum << "! Literal je siri od 12b!\n";
          exit(-1);
        }
        break;
      }
      case opmode::MEMDIRSYMB: {
        addLit((*(arguments->argNames))[0]);
        break;
      }
      case opmode::REGINDPOMSYMB: {
        //TODO: Dodaj kada uradis equ, da li je sekcija 1 i da li je vr simbola na 12b
      }
      default: break;
    }
  }
  else {
    switch(arguments->operandAdressMode) {
      case opmode::REGDIR: { //Isto kao kod ld samo zamenjeni izvorisni i odredisni reg
        unsigned int regDstNum = stoi((*(*(arguments->argNames))[0]).substr(1,2)); 
        unsigned int regSrcNum = stoi((*reg).substr(1,2)); 
        currentSection->contents->push_back(0x91);
        currentSection->contents->push_back((unsigned char)((regDstNum << 4) | regSrcNum));
        currentSection->contents->push_back(0x00);
        currentSection->contents->push_back(0x00);
        break;
      }
      case opmode::REGIND: { //Isto kao kod ld samo zamenjeni izvorisni i odredisni reg
        unsigned int regDstNum = stoi((*(*(arguments->argNames))[0]).substr(1,2)); 
        unsigned int regSrcNum = stoi((*reg).substr(1,2));
        currentSection->contents->push_back(0x80);
        currentSection->contents->push_back((unsigned char)(regDstNum << 4));
        currentSection->contents->push_back((unsigned char)(regSrcNum << 4));
        currentSection->contents->push_back(0x00);
        break;
      }
      case opmode::MEMDIRLIT: {
        long number = litToL((*(arguments->argNames))[0]);
        unsigned int regSrcNum = stoi((*reg).substr(1,2)); 
        if(number > 2047 || number < -2048) { //Mora bazen
          unsigned int offset = literalOffset((*(arguments->argNames))[0]);
          currentSection->contents->push_back(0x82);
          currentSection->contents->push_back(0xf0);
          currentSection->contents->push_back((unsigned char)((regSrcNum << 4) | (offset & 0x0f)));
          currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
        }
        else { //Ugradi u instr
          currentSection->contents->push_back(0x80);
          currentSection->contents->push_back(0x00);
          currentSection->contents->push_back((unsigned char)((regSrcNum << 4) | (number & 0x0f)));
          currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
        }
        break;
      }
      case opmode::MEMDIRSYMB: {
        unsigned int regSrcNum = stoi((*reg).substr(1,2));
        if(checkSection((*(arguments->argNames))[0])) { //Ako su u istoj sekciji, ne treba bazen i relok zapis
          auto symbTrow = symbolTable->find(*(*(arguments->argNames))[0]);
          int offset = symbTrow->second->value - lc - 4;
          currentSection->contents->push_back(0x80);
          currentSection->contents->push_back(0xf0);
          currentSection->contents->push_back((unsigned char)((regSrcNum << 4) | (offset & 0x0f)));
          currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
        }
        else { //Nisu u istoj sekciji
          unsigned int offset = literalOffset((*(arguments->argNames))[0]);
          currentSection->contents->push_back(0x82);
          currentSection->contents->push_back(0xf0);
          currentSection->contents->push_back((unsigned char)((regSrcNum << 4) | (offset & 0x0f)));
          currentSection->contents->push_back((unsigned char)((offset & 0xff0) >> 4));
        }
        break;
      }
      case opmode::REGINDPOMLIT: { //Literal je sigurno na sirini od 12b, pukao bi asembler ranije da nije
        long number = litToL((*(arguments->argNames))[1]);
        unsigned int regDstNum = stoi((*(*(arguments->argNames))[0]).substr(1,2)); 
        unsigned int regSrcNum = stoi((*reg).substr(1,2));
        currentSection->contents->push_back(0x80);
        currentSection->contents->push_back((unsigned char)(regDstNum << 4));
        currentSection->contents->push_back((unsigned char)((number & 0x0f) | (regSrcNum << 4)));
        currentSection->contents->push_back((unsigned char)((number & 0xff0) >> 4));
        break;
      }
      case opmode::REGINDPOMSYMB: {
        //TODO: Dodaj kada uradis equ
      }
      default: break;
    }
  }
}

void instrCsrrd(string* sysRegSrc, string* registerDst) {
  unsigned int regSrcNum = 0; //Pretpostavka da je status
  if (*sysRegSrc == "HANDLER") regSrcNum = 1;
  else if (*sysRegSrc == "CAUSE") regSrcNum = 2;
  unsigned int regDstNum = stoi((*registerDst).substr(1,2));
  currentSection->contents->push_back(0x90);
  currentSection->contents->push_back((unsigned char)(regDstNum << 4 | regSrcNum));
  currentSection->contents->push_back(0x00);
  currentSection->contents->push_back(0x00);
}

void instrCsrwr(string* registerSrc, string* sysRegDst) {
  unsigned int regDstNum = 0; //Pretpostavka da je status
  if (*sysRegDst == "HANDLER") regDstNum = 1;
  else if (*sysRegDst == "CAUSE") regDstNum = 2;
  unsigned int regSrcNum = stoi((*registerSrc).substr(1,2));
  currentSection->contents->push_back(0x94);
  currentSection->contents->push_back((unsigned char)(regDstNum << 4 | regSrcNum));
  currentSection->contents->push_back(0x00);
  currentSection->contents->push_back(0x00);
}

//==============================================Za stampanje na cout=============================================

void printSymbs() {
  cout << left << setw(35) << "Labela" << "Rbr\t" << "Tip\t" << "Sekcija\t" << "Vrednost(lc)\n" << endl;
  for(auto iter = symbolTable->begin(); iter != symbolTable->end(); iter++) {
    cout << left << setw(35) << iter->first;
    iter->second->printRow();  
  }
}

void printSections() {
  for(auto iter = allSections->begin(); iter != allSections->end(); iter++) {
    iter->second->printSection();
  }
}