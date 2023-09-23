#include "tableStructs.hpp"

//Za oslobadjanje memorije
void freeMem();

//Definicija simbola
void symbDef(std::string* label);

//Direktive
void dirGlobal(Args* arguments);
void dirExtern(Args* arguments);
void dirSection(std::string* label);
void dirWord(Args* arguments);
void dirSkip(std::string* literal);
void dirAscii(std::string* str);
void dirEqu(std::string* label, std::string* value);
void dirEnd();


//Instrukcije
void instrHalt();
void instrInt();
void instrIret();
void instrCall(std::string* jmpAddress);
void instrRet();
void instrJmp(std::string* jmpAdress);
void instrBeq(std::string* registerSrc, std::string* registerDst, std::string* jmpAddress);
void instrBne(std::string* registerSrc, std::string* registerDst, std::string* jmpAddress);
void instrBgt(std::string* registerSrc, std::string* registerDst, std::string* jmpAddress);
void instrPush(std::string* registerSrc);
void instrPop(std::string* registerDst);
void instrXchg(std::string* registerSrc, std::string* registerDst);
void instrAdd(std::string* registerSrc, std::string* registerDst);
void instrSub(std::string* registerSrc, std::string* registerDst);
void instrMul(std::string* registerSrc, std::string* registerDst);
void instrDiv(std::string* registerSrc, std::string* registerDst);
void instrNot(std::string* registerSrc);
void instrAnd(std::string* registerSrc, std::string* registerDst);
void instrOr(std::string* registerSrc, std::string* registerDst);
void instrXor(std::string* registerSrc, std::string* registerDst);
void instrShl(std::string* registerSrc, std::string* registerDst);
void instrShr(std::string* registerSrc, std::string* registerDst);
void instrLd(Args* arguments, std::string* reg);
void instrSt(std::string* reg, Args* arguments);
void instrCsrrd(std::string* sysRegSrc, std::string* registerDst);
void instrCsrwr(std::string* registerStc, std::string* sysRegDst);

//Provera da li radi sve, mogu da obrisem posle
void printSymbs();
void printSections();

