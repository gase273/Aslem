//Ovaj fajl postoji jer parser zeza sa dvostrukim definicijama ovih globalnih promenljivih, ovo je jedno resenje koje zaobilazi to
#include <map>
#include <string.h>
#include "../inc/tableStructs.hpp"
#include <regex>
using namespace std;
  
map<int, Section*> *allSections = new map<int, Section*>(); 
map<string, SymbTRow*> *symbolTable = new map<string, SymbTRow*>(); 
Section* currentSection = nullptr; 
unsigned int indexCtr = 2; //Brojac indeksa - 0 je za nedef simbole, 1 je neiskorisceno (za aps simb bi trebalo)
unsigned int lc = 0;
//prvi prolaz popunjava tabelu simbola i racuna velicine sekcija
//drugi prolaz popunjava masinski kod
bool isFirstPass = true; 
string outputName; //ime izlaznog fajla                       

extern int pMain(int argc, char* argv[]);

int main(int argc, char* argv[]) {
  if(strcmp(argv[1], "-o") == 0) outputName = argv[2];
  else {
    outputName = argv[1];
    outputName = regex_replace(outputName, regex("\\.s"), ".o");
    outputName = outputName.substr(outputName.find_last_of("/")+1);
  }
  return pMain(argc, argv);
}