#include <set>
#include <iomanip>
#include <string.h>
#include <fstream>
#include <algorithm>
#include "../inc/tableStructs.hpp"

using namespace std;

map<string, LinkerSTRow*>* linkerSymbolTable = new map<string, LinkerSTRow*>(); 
map<string, unsigned int>* placeRequests = new map<string, unsigned int>(); //Place komande idu ovde
deque<string>* unplacedSections = new deque<string>(); //Sekcije koje nemaju place komandu
deque<FileTracker*>* fileSectionTrackers = new deque<FileTracker*>(); //Iz kog fajla potice koja sekcija (za pomeraje, relok zapise)
set<string> undefinedSymbSet; //Proverava nedefinisane simbole
set<string> definedSymbSet; //Proverava dvostruke definicije simbola

unsigned int linkerIndexCtr = 1; //Brojac za Rbr u tabeli simbola

bool cmp(pair<string, LinkerSTRow*>& a, pair<string, LinkerSTRow*>& b) { //Za sortiranje sekcija po adresi
    return a.second->value < b.second->value;
}

void freeMem() {
    for(auto iter : *linkerSymbolTable) {
        delete iter.second;
    }
    delete linkerSymbolTable;
    delete placeRequests;
    delete unplacedSections;
    for(auto iter : *fileSectionTrackers) {
        delete iter;
    }
    delete fileSectionTrackers;
}

unsigned long litToUL(const string literal) { //Pretvara literal u unsigned long 
  int base = 10;
  if(literal[1] == 'x') base = 16;
  else if (literal[0] == '0') base = 8;
  return std::stoul(literal, nullptr, base);
}

int main(int argc, char* argv[]) {

    int inFilesStart = 1;
    string outFileName = "default.hex";
    bool hexCommand = false, relocCommand = false;

    //Obrada komandi iz terminala
    while(inFilesStart < argc){
        if (strcmp(argv[inFilesStart], "-o") == 0) {
          outFileName = argv[inFilesStart + 1];
          inFilesStart += 2;
        }
        else if(strcmp(argv[inFilesStart], "-hex") == 0){
            hexCommand = true;
            inFilesStart += 1;
        }
        else if(strcmp(argv[inFilesStart], "-relocatable") == 0){
            relocCommand = true;
            inFilesStart += 1;
        }
        else if(strstr(argv[inFilesStart], "place") != nullptr) {
            string placeCommand = argv[inFilesStart]; //Olaksava zivot
            unsigned int equalPos = placeCommand.find('='); 
            unsigned int atPos = placeCommand.find('@'); //Za razdvajanje stringa
            string sectionName = placeCommand.substr(equalPos + 1, atPos - equalPos - 1); //Ime sekcije izvuceno iz opcije
            unsigned int startAddr = litToUL(placeCommand.substr(atPos + 1)); //Pocetna adresa izvucena iz opcije
            placeRequests->insert({sectionName, startAddr});
            inFilesStart+=1;
        }
        else break;
    }
    if (!(hexCommand || relocCommand)) {
        freeMem();
        cerr << "Greska - nije navedena ni -hex ni -relocatable opcija!\n";
        exit(-1);
    }
    else if (hexCommand && relocCommand) {
        freeMem();
        cerr << "Greska - navedene i -hex i -relocatable opcije!\n";
        exit(-1);
    }
    else if (inFilesStart == argc) {
        freeMem();
        cerr << "Greska - nijedan ulazni fajl nije naveden!\n";
        exit(-1);
    }

    vector<ifstream>* inFiles = new vector<ifstream>();
    if(hexCommand) { //Ako je -hex opcija
        unsigned int unplacedStartAddr = 0; //Odakle se smestaju sekcije koje nemaju place komandu
        for(int i = 0; i < argc - inFilesStart; i++) {

            //Obrada ulaznih fajlova
            ifstream inFile(argv[i + inFilesStart], ios::binary);
            if(inFile.is_open()) { //Citamo ulazne fajlove redom prateci format, prvo citamo sve sekcije za mapiranje

                size_t numSections;
                inFile.read(reinterpret_cast<char*>(&numSections), sizeof(size_t)); //Broj sekcija
                for(int j = 0; j < numSections; j++) {

                    size_t strSize;
                    string sectionName;
                    inFile.read(reinterpret_cast<char*>(&strSize), sizeof(size_t)); //Velicina stringa
                    sectionName.resize(strSize);
                    inFile.read(&sectionName[0], strSize); 

                    unsigned int inFileSymbIndex, sectionSize;
                    inFile.read(reinterpret_cast<char*>(&inFileSymbIndex), sizeof(unsigned int));  
                    inFile.read(reinterpret_cast<char*>(&sectionSize), sizeof(unsigned int));
                    
                    unsigned int sectionStartAddr; //Odakle pocinje novi dodatak (za fileTracker)
                    auto linkerstrow = linkerSymbolTable->find(sectionName); 
                    if(linkerstrow != linkerSymbolTable->end()) {//Vidi da li sekcija vec postoji
                        sectionStartAddr = linkerstrow->second->value + linkerstrow->second->size; //Pocetak novog dodatka se spaja na kraj postojece sekcije
                        if(linkerstrow->second->placed) { //Ako se dodaje na sekciju sa -place opcijom
                            auto sectionIter = linkerSymbolTable->begin(); 
                            for (sectionIter; sectionIter != linkerSymbolTable->end(); sectionIter++) {
                                if(sectionIter->first != sectionName) { //Ako nisu iste sekcije, proveri preklapanje
                                    if(sectionIter->second->placed && 
                                    sectionStartAddr + sectionSize > sectionIter->second->value && 
                                    sectionStartAddr <= sectionIter->second->value) {
                                        //Dve -place sekcije se preklapaju, gasi linker
                                        freeMem();
                                        for (int k = 0; k < inFiles->size(); k++) (*inFiles)[k].close();
                                        inFile.close();
                                        delete inFiles;
                                        cerr << "Greska tokom linkovanja - sekcije " << linkerstrow->first << " i " << sectionIter->first << " se preklapaju!\n";
                                        exit(-1); 
                                    }
                                }
                            }
                            if(sectionStartAddr + sectionSize > unplacedStartAddr) unplacedStartAddr = sectionStartAddr + sectionSize; //Azuriraj adresu pocetka unplaced sekcija
                        }
                        linkerstrow->second->size += sectionSize;
                        linkerstrow->second->contents->resize(linkerstrow->second->size);
                        inFile.read(reinterpret_cast<char*>(linkerstrow->second->contents->data() + linkerstrow->second->size - sectionSize), sectionSize);                  
                    }
                    else { //Sekcija ne postoji
                        auto placeRequest = placeRequests->find(sectionName); //Proveri da li postoji -place opcija za ovu sekciju
                        bool placeFlag = false;
                        if (placeRequest != placeRequests->end()) {
                            placeFlag = true;
                            sectionStartAddr = placeRequest->second; //Pocetak novog dodatka je odredjen -place opcijom
                            auto sectionIter = linkerSymbolTable->begin(); 
                            for (sectionIter; sectionIter != linkerSymbolTable->end(); sectionIter++) {
                                if(sectionIter->second->placed && 
                                ((sectionStartAddr + sectionSize > sectionIter->second->value && sectionStartAddr <= sectionIter->second->value) ||
                                (sectionIter->second->value + sectionIter->second->size > sectionStartAddr) && sectionIter->second->value <= sectionStartAddr)) {
                                    //Dve -place sekcije se preklapaju, gasi linker
                                    freeMem();
                                    for (int k = 0; k < inFiles->size(); k++) (*inFiles)[k].close();
                                    inFile.close();
                                    delete inFiles;
                                    cerr << "Greska tokom linkovanja - sekcije " << placeRequest->first << " i " << sectionIter->first << " se preklapaju!\n";
                                    exit(-1); 
                                }
                            }
                            if(sectionStartAddr + sectionSize > unplacedStartAddr) unplacedStartAddr = sectionStartAddr + sectionSize; //Azuriraj adresu pocetka ostalih sekcija
                            if(unplacedStartAddr >= 0xffffff00) { //Ako sadrzaj zalazi u rezervisan prostor - greska
                                freeMem();
                                for (int k = 0; k < inFiles->size(); k++) (*inFiles)[k].close();
                                inFile.close();
                                delete inFiles;
                                cerr << "Greska tokom linkovanja - sadrzaj sekcije " << placeRequest->first << " zalazi u prostor rezervisan za mem. mapirane registre!\n";
                                exit(-1);
                            }
                        }
                        else {
                            sectionStartAddr = 0; //Ne postoji -place, ovo ce se prepraviti kasnije kada se mapiraju sve -place sekcije
                            unplacedSections->push_back(sectionName); //Zapamti da mora da se prepravi
                        }
                        LinkerSTRow* newRow = new LinkerSTRow(sectionStartAddr, symbtype::LCL, sectionSize, placeFlag);
                        newRow->contents = new vector<unsigned char>();
                        newRow->contents->resize(sectionSize);
                        inFile.read(reinterpret_cast<char*>(newRow->contents->data()), sectionSize);
                        linkerSymbolTable->insert({sectionName, newRow});

                    }
                    FileTracker* newRow = new FileTracker(sectionName, inFileSymbIndex, i, sectionSize, sectionStartAddr); //Dodaj sekciju iz ovog fajla u fileTracker
                    fileSectionTrackers->push_back(newRow);  
                }
                inFiles->push_back(move(inFile));
            }
        }
        //Mapiranje sekcija koje nisu -place
        unsigned int unplacedMemoryCounter = unplacedStartAddr; //Odakle krecu da se smestaju
        for(auto unplacedSectionName : *unplacedSections) {
            auto linkerstrow = linkerSymbolTable->find(unplacedSectionName); //Nadji sekciju koja treba da se mapira
            linkerstrow->second->value = unplacedMemoryCounter; //Pocetak sekcije u memoriji
            for(auto iter : *fileSectionTrackers) {
                if(iter->sectionName == unplacedSectionName) iter->start += unplacedMemoryCounter; //Pomeri pocetke razdvojenih sekcija u fileTrackeru
            }
            unplacedMemoryCounter += linkerstrow->second->size;
            if(unplacedMemoryCounter >= 0xffffff00) { //Ako sadrzaj zalazi u rezervisan prostor - greska
                freeMem();
                for (int k = 0; k < inFiles->size(); k++) (*inFiles)[k].close();
                delete inFiles;
                cerr << "Greska tokom linkovanja - sadrzaj sekcije " << unplacedSectionName << " zalazi u prostor rezervisan za mem. mapirane registre!\n";
                exit(-1);
            }
        }

        //Izmapirano sve, sada redom cita ulazne fajlove i obradjuje tabele simbola
        for(int i = 0; i < argc - inFilesStart; i++) {
            if((*inFiles)[i].is_open()) {

                size_t symbsNum;
                (*inFiles)[i].read(reinterpret_cast<char*>(&symbsNum), sizeof(size_t)); //Broj simbola
                for(int j = 0; j < symbsNum; j++) {

                    string symbName;
                    size_t stringSize;
                    (*inFiles)[i].read(reinterpret_cast<char*>(&stringSize), sizeof(size_t)); //Velicina stringa
                    symbName.resize(stringSize);
                    (*inFiles)[i].read(&symbName[0], stringSize); 

                    unsigned int index, section, value;
                    symbtype type;
                    (*inFiles)[i].read(reinterpret_cast<char*>(&index), sizeof(unsigned int));
                    (*inFiles)[i].read(reinterpret_cast<char*>(&section), sizeof(unsigned int));
                    (*inFiles)[i].read(reinterpret_cast<char*>(&value), sizeof(unsigned int));
                    (*inFiles)[i].read(reinterpret_cast<char*>(&type), sizeof(symbtype));

                    //-hex opcija - samo globalni simboli se dodaju u tabelu, vrsi se provera visestruke definicije i nedostatka definicije
                    switch(type) {
                        case symbtype::GLBL: { //Nasao globalni simbol
                            if(definedSymbSet.count(symbName)){ //Ako je simbol vec definisan, u ovom je setu
                                freeMem();
                                for (int k = 0; k < inFiles->size(); k++) (*inFiles)[k].close();
                                delete inFiles;
                                cerr << "Greska tokom linkovanja - dvostruka definicija simbola: "<< symbName <<endl;
                                exit(-1);
                            }
                            definedSymbSet.insert(symbName);
                            //Ako je bio obelezen kao nedef preko nekog ranijeg externa, obrisi ga iz skupa nedef
                            if(undefinedSymbSet.count(symbName)) undefinedSymbSet.erase(symbName);
                            break;
                        }
                        case symbtype::EXTRN: { //Nasao extern
                            //Ako dosad nije bio definisan, dodaj ga kao nedefinisanog
                            if(!definedSymbSet.count(symbName)) undefinedSymbSet.insert(symbName);
                            break;
                        }
                        default: break;
                    }
                    if(type == symbtype::GLBL) { //Ako je globalni simbol, ubaci ga u tabelu
                        unsigned int newSection; //U kojoj (novoj) sekciji je definisan
                        for(auto iter : *fileSectionTrackers) { //Trazi iz kog fajla je potekao da bi odredili novu vrednost
                            if(iter->inFileIndex == i && iter->inSymbIndex == section) {
                                value += iter->start;

                                auto newSectionRow = linkerSymbolTable->find(iter->sectionName); //Nadji rbr nove sekcije
                                if(newSectionRow != linkerSymbolTable->end()) newSection = newSectionRow->second->index;
                            }
                        }
                        LinkerSTRow* newRow = new LinkerSTRow(value, type, newSection);
                        linkerSymbolTable->insert({symbName, newRow});
                    }
                }
            }
        }

        //Ucitani svi simboli iz svih tabela
        if(!undefinedSymbSet.empty()) { //Da li je ostao neki nedef simbol
            freeMem();
            for (int k = 0; k < inFiles->size(); k++) (*inFiles)[k].close();
            delete inFiles;
            cerr << "Greska tokom linkovanja - postoje nerazreseni simboli: ";
            for(auto symb : undefinedSymbSet) {
                cerr << symb << " ";
            }
            exit(-1);
        }

        //Sada se citaju i obradjuju relokacioni zapisi iz ulaznih fajlova
        for(int i = 0; i < argc - inFilesStart; i++) {
            if((*inFiles)[i].is_open()) {

                size_t relocTableNum;
                (*inFiles)[i].read(reinterpret_cast<char*>(&relocTableNum), sizeof(size_t)); //Broj relok tabela
                for(int j = 0; j < relocTableNum; j++) {

                    size_t relocRowNum;
                    (*inFiles)[i].read(reinterpret_cast<char*>(&relocRowNum), sizeof(size_t)); //Broj relokacija za ovu tabelu
                    for(int k = 0; k < relocRowNum; k++) {
                        
                        string symbName;
                        size_t strSize;
                        (*inFiles)[i].read(reinterpret_cast<char*>(&strSize), sizeof(size_t)); //Velicina stringa
                        symbName.resize(strSize);
                        (*inFiles)[i].read(&symbName[0], strSize);
                        unsigned int index, section, location, addend;
                        reloctype type;
                        (*inFiles)[i].read(reinterpret_cast<char*>(&index), sizeof(unsigned int)); 
                        (*inFiles)[i].read(reinterpret_cast<char*>(&section), sizeof(unsigned int)); 
                        (*inFiles)[i].read(reinterpret_cast<char*>(&location), sizeof(unsigned int)); 
                        (*inFiles)[i].read(reinterpret_cast<char*>(&addend), sizeof(unsigned int)); 
                        (*inFiles)[i].read(reinterpret_cast<char*>(&type), sizeof(reloctype)); 
                        
                        unsigned int oldSecStartAddr, newSecStartAddr, patchAddr, patchValue;
                        vector<unsigned char>* contentsToPatch;
                        for(auto iter : *fileSectionTrackers) {
                            if(iter->inFileIndex == i && iter->inSymbIndex == section) { //Potrazi pocetak stare sekcije u novom fajlu
                                oldSecStartAddr = iter->start;
                                auto linkerstrow = linkerSymbolTable->find(iter->sectionName); //Potrazi pocetak cele sekcije u novom fajlu
                                if(linkerstrow != linkerSymbolTable->end()) { 
                                    //Dodeli adresu pocetka cele sekcije kao i sadrzaj koji se prepravlja
                                    newSecStartAddr = linkerstrow->second->value;
                                    contentsToPatch = linkerstrow->second->contents;
                                }
                            }
                        }
                        patchAddr = oldSecStartAddr + location - newSecStartAddr; //Ovako dobijamo koje tacno bajtove u vektoru prepravljamo
                        if(type == reloctype::RGBLA) { //Globalna aps relokacija
                            auto targetSymb = linkerSymbolTable->find(symbName);
                            if(targetSymb != linkerSymbolTable->end()) patchValue = targetSymb->second->value; 
                        }
                        else { //Lokalna aps relokacija
                            for(auto iter : *fileSectionTrackers) {//Potrazi pocetak stare sekcije u novom fajlu
                                if(iter->inFileIndex == i && iter->inSymbIndex == section) patchValue = iter->start + addend; 
                            }
                        }
                        (*contentsToPatch)[patchAddr] = (unsigned char)(patchValue & 0xff); //Izvrsi prepravku
                        (*contentsToPatch)[patchAddr + 1] = (unsigned char)((patchValue & 0xff00) >> 8);
                        (*contentsToPatch)[patchAddr + 2] = (unsigned char)((patchValue & 0xff0000) >> 16);
                        (*contentsToPatch)[patchAddr + 3] = (unsigned char)((patchValue & 0xff000000) >> 24); 
                    }
                }
            }
        }

        //Kraj obrade
        for(int i = 0; i < inFiles->size(); i++) (*inFiles)[i].close(); //Zatvori ulazne fajlove

        ofstream outFile;
        outFile.open(outFileName);

        vector<pair<string, linkerSTRow*>> sortedSections;
        for(auto iter : *linkerSymbolTable) {
            if(iter.second->type == symbtype::LCL) sortedSections.push_back(iter);
        }        

        sort(sortedSections.begin(), sortedSections.end(), cmp); //Sortiraj sekcije po adresi pocetka sekcije

        unsigned int prevSectionEndAddr = 0; //Adresu kraja prethodne sekcije
        unsigned int addr = 0;
        unsigned int sectionEndAddrAlign = 0; //Poravnanje za fill karaktere (nule) na kraju sekcije

        for (size_t i = 0; i < sortedSections.size(); i++) { //Boga pitaj kako sam ovo smislio
            auto iter = sortedSections[i];

            unsigned int sectionStartAddr = iter.second->value;
            unsigned int sectionEndAddr = iter.second->value + iter.second->size;
            addr = sectionStartAddr - (sectionStartAddr % 8); //Poravnaj na 8

            if (i > 0) {
                //Zapamti adresu kraja prethodne sekcije
                prevSectionEndAddr = sortedSections[i-1].second->value + sortedSections[i-1].second->size;
            }

            if (addr != sectionStartAddr && sectionStartAddr != prevSectionEndAddr) {
                //Ispisi fill karatktere na pocetku sekcije, AKO pocetak nije poravnat na 8 I ne nadovezuje se na proslu sekciju
                outFile << hex << setfill('0') << setw(8) << addr << ": ";
                while (addr < sectionStartAddr) {
                    outFile << hex << setfill('0') << setw(2) << 0 << " ";
                    addr++;
                }
            }

            else if (addr != sectionStartAddr && sectionStartAddr == prevSectionEndAddr) {
                //Ako se nadovezuje na prethodnu sekciju, fill nije neophodan
            }
            
            else {
                //Inace, ispisi newline ako nije pocetak fajla i adresu 
                if(i != 0) outFile << "\n";
                outFile << hex << setfill('0') << setw(8) << addr << ": ";
            }

            addr = sectionStartAddr; 
            for (auto c : *iter.second->contents) {
                //Ispisuje sadrzaj sekcije
                outFile << hex << setfill('0') << setw(2) << static_cast<unsigned int>(c) << " ";
                addr++;
                if (addr % 8 == 0 && addr != sectionEndAddr) {
                    outFile << "\n";
                    outFile << hex << setfill('0') << setw(8) << addr << ": ";
                }
            }
            
            sectionEndAddrAlign = (addr + 7) & ~7; //Poravnanje za kraj sekcije
            unsigned int nextSectionStartAddr = (i + 1 < sortedSections.size()) ? sortedSections[i + 1].second->value : 0;
            while (addr < sectionEndAddrAlign && addr < nextSectionStartAddr) {
                //Ispisuj fill karaktere dok ne naidje na prvu adresu deljivu sa 8 ili na pocetak sledece sekcije
                outFile << hex << setfill('0') << setw(2) << 0 << " ";
                addr++;
            }
        }
        while(addr < sectionEndAddrAlign) {
            //Posebno za poslednju sekciju ispisi fill karaktere do prve adrese deljive sa 8
            outFile << hex << setfill('0') << setw(2) << 0 << " ";
            addr++;
        }
    }


    else { //Ako je -relocatable opcija
        unsigned int sectionsNum = 0; //Brojac sekcija
        for(int i = 0; i < argc - inFilesStart; i++) {
            ifstream inFile(argv[i + inFilesStart], ios::binary);
            if(inFile.is_open()) { //Citamo ulazne fajlove redom prateci format, prvo citamo sve sekcije za mapiranje

                size_t numSections;
                inFile.read(reinterpret_cast<char*>(&numSections), sizeof(size_t)); //Broj sekcija
                for(int j = 0; j < numSections; j++) {

                    size_t strSize;
                    string sectionName;
                    inFile.read(reinterpret_cast<char*>(&strSize), sizeof(size_t)); //Velicina stringa
                    sectionName.resize(strSize);
                    inFile.read(&sectionName[0], strSize); 

                    unsigned int inFileSymbIndex, sectionSize;
                    inFile.read(reinterpret_cast<char*>(&inFileSymbIndex), sizeof(unsigned int)); 
                    inFile.read(reinterpret_cast<char*>(&sectionSize), sizeof(unsigned int));
                    
                    //Ovaj deo dosta slican kao kod -hex opcije samo se ne gleda uopste -place opcija i ne vrsi se mapiranje, nego sve sekcije pocinju od 0

                    unsigned int sectionStartAddr; //Odakle pocinje novi dodatak ( za fileTracker)
                    auto linkerstrow = linkerSymbolTable->find(sectionName); //Vidi da li sekcija vec postoji
                    if(linkerstrow != linkerSymbolTable->end()) { //Postoji, treba nadovezati
                        sectionStartAddr = linkerstrow->second->value + linkerstrow->second->size; 
                        linkerstrow->second->size += sectionSize;
                        linkerstrow->second->contents->resize(linkerstrow->second->size);
                        inFile.read(reinterpret_cast<char*>(linkerstrow->second->contents->data() + linkerstrow->second->size - sectionSize), sectionSize); //Spoji sekcije                   
                    }
                    else { //Sekcija ne postoji
                        sectionsNum++; //Povecaj broj sekcija
                        sectionStartAddr = 0; //Sve sekcije pocinju od adrese 0 jer pravimo -relocatable
                        LinkerSTRow* newRow = new LinkerSTRow(sectionStartAddr, symbtype::LCL, sectionSize, false);
                        newRow->contents = new vector<unsigned char>();
                        newRow->relocTable = new deque<RelocTRow*>();
                        newRow->contents->resize(sectionSize);
                        inFile.read(reinterpret_cast<char*>(newRow->contents->data()), sectionSize);
                        linkerSymbolTable->insert({sectionName, newRow});
                    }
                    FileTracker* newRow = new FileTracker(sectionName, inFileSymbIndex, i, sectionSize, sectionStartAddr); //Dodaj sekciju iz ovog fajla u fileTracker
                    fileSectionTrackers->push_back(newRow);  
                }
                inFiles->push_back(move(inFile));
            }
        }

        //Sve sekcije ubacene i spojene, sada se obradjuju tabele simbola
        for(int i = 0; i < argc - inFilesStart; i++) {
            if((*inFiles)[i].is_open()) {

                size_t symbsNum;
                (*inFiles)[i].read(reinterpret_cast<char*>(&symbsNum), sizeof(size_t));//Broj simbola
                for(int j = 0; j < symbsNum; j++) {

                    string symbName;
                    size_t stringSize;
                    (*inFiles)[i].read(reinterpret_cast<char*>(&stringSize), sizeof(size_t)); //Velicina stringa
                    symbName.resize(stringSize);
                    (*inFiles)[i].read(&symbName[0], stringSize);

                    unsigned int index, section, value;
                    symbtype type;
                    (*inFiles)[i].read(reinterpret_cast<char*>(&index), sizeof(unsigned int));
                    (*inFiles)[i].read(reinterpret_cast<char*>(&section), sizeof(unsigned int));
                    (*inFiles)[i].read(reinterpret_cast<char*>(&value), sizeof(unsigned int));
                    (*inFiles)[i].read(reinterpret_cast<char*>(&type), sizeof(symbtype));

                    //-relocatable opcija - Globalni simboli se dodaju u tabelu, proverava se dvostruka definicija, nedefinisani simboli se dodaju kao extern na kraju
                    switch(type) {
                        case symbtype::GLBL: { //Nasao globalni simbol
                            if(definedSymbSet.count(symbName)){ //Ako je simbol vec definisan, u ovom je setu
                                freeMem();
                                for (int k = 0; k < inFiles->size(); k++) (*inFiles)[k].close();
                                delete inFiles;
                                cerr << "Greska tokom linkovanja - dvostruka definicija simbola: "<< symbName <<endl;
                                exit(-1);
                            }
                            definedSymbSet.insert(symbName);
                            //Ako je bio obelezen kao nedef preko nekog ranijeg externa, obrisi ga iz skupa nedef
                            if(undefinedSymbSet.count(symbName)) undefinedSymbSet.erase(symbName);
                            break;
                        }
                        case symbtype::EXTRN: { //Nasao extern
                            //Ako dosad nije bio definisan, dodaj ga kao nedefinisanog
                            if(!definedSymbSet.count(symbName)) undefinedSymbSet.insert(symbName);
                            break;
                        }
                        default: break;
                    }
                    if(type == symbtype::GLBL) { //Ako je globalni simbol, ubaci ga u tabelu
                        unsigned int newSection; //U kojoj (novoj) sekciji je definisan
                        for(auto iter : *fileSectionTrackers) { //Trazi iz kog fajla je potekao da bi odredili novu vrednost
                            if(iter->inFileIndex == i && iter->inSymbIndex == section) {
                                value += iter->start;
                                auto newSectionRow = linkerSymbolTable->find(iter->sectionName); //Nadji rbr nove sekcije
                                if(newSectionRow != linkerSymbolTable->end()) newSection = newSectionRow->second->index;
                            }
                        }
                        LinkerSTRow* newRow = new LinkerSTRow(value, type, newSection);
                        linkerSymbolTable->insert({symbName, newRow});
                    }
                }
            }
        }

        //Ucitani svi simboli iz svih tabela
        for(auto externSymb : undefinedSymbSet) {
            linkerSTRow* newRow = new LinkerSTRow(0, symbtype::EXTRN, 0); //Ubaci nedef simbole kao extern
            linkerSymbolTable->insert({externSymb, newRow});
        }

        //Sada se citaju i obradjuju relokacioni zapisi iz ulaznih fajlova
        for(int i = 0; i < argc - inFilesStart; i++) {
            if((*inFiles)[i].is_open()) {

                size_t relocTableNum;
                (*inFiles)[i].read(reinterpret_cast<char*>(&relocTableNum), sizeof(size_t)); //Broj relok tabela
                for(int j = 0; j < relocTableNum; j++) {

                    size_t relocRowNum;
                    (*inFiles)[i].read(reinterpret_cast<char*>(&relocRowNum), sizeof(size_t)); //Broj relokacija za ovu tabelu
                    for(int k = 0; k < relocRowNum; k++) {
                        
                        string symbName;
                        size_t strSize;
                        (*inFiles)[i].read(reinterpret_cast<char*>(&strSize), sizeof(size_t)); //Velicina stringa
                        symbName.resize(strSize);
                        (*inFiles)[i].read(&symbName[0], strSize);
                        unsigned int index, section, location, addend;
                        reloctype type;
                        (*inFiles)[i].read(reinterpret_cast<char*>(&index), sizeof(unsigned int)); 
                        (*inFiles)[i].read(reinterpret_cast<char*>(&section), sizeof(unsigned int)); 
                        (*inFiles)[i].read(reinterpret_cast<char*>(&location), sizeof(unsigned int)); 
                        (*inFiles)[i].read(reinterpret_cast<char*>(&addend), sizeof(unsigned int)); 
                        (*inFiles)[i].read(reinterpret_cast<char*>(&type), sizeof(reloctype)); 
                        
                        unsigned int oldSecStartAddr, newLocation, newAddend, newIndex, newSection;
                        string sectionName;
                        for(auto iter : *fileSectionTrackers) {
                            //Potrazi pocetak stare sekcije u novom fajlu, i indeks sekcije u novom fajlu
                            if(iter->inFileIndex == i && iter->inSymbIndex == section) {
                                oldSecStartAddr = iter->start;
                                auto linkerstrow = linkerSymbolTable->find(iter->sectionName); 
                                //Potrazi indeks cele sekcije u novom fajlu
                                if(linkerstrow != linkerSymbolTable->end()) {
                                    newSection = linkerstrow->second->index; 
                                    sectionName = linkerstrow->first;
                                }
                            }
                        }

                        newLocation = oldSecStartAddr + location; //Nova lokacija prepravke
                        auto targetSymb = linkerSymbolTable->find(symbName);
                        if(targetSymb != linkerSymbolTable->end()) newIndex = targetSymb->second->index; //Nov indeks simbola koji se relocira
                        if(type == reloctype::RGBLA) newAddend = 0; //Globalna aps relok
                        else {
                            for(auto iter : *fileSectionTrackers) {//Potrazi pocetak stare sekcije u novom fajlu
                                if(iter->inFileIndex == i && iter->inSymbIndex == section) newAddend = iter->start + addend; //Novi addend
                            }
                        }
                        //Dodaj prepravljen relok zapis
                        RelocTRow* newReloc = new RelocTRow(symbName, newIndex, newLocation, newSection, newAddend, type);
                        auto linkerstrow = linkerSymbolTable->find(sectionName);
                        if(linkerstrow != linkerSymbolTable->end()) linkerstrow->second->relocTable->push_back(newReloc);
                    }
                }
            }
        }

        //Kraj obrade
        for(int i = 0; i < inFiles->size(); i++) (*inFiles)[i].close(); //Zatvori ulazne fajlove

        if(outFileName == "default.hex") outFileName = "default.o";
        ofstream outFile;
        outFile.open(outFileName, ios::binary);
        if(outFile.is_open()) { //Pocni pisanje u izlazni fajl, isti format kao i izlaz iz asemblera

            auto tableIter = linkerSymbolTable->begin();

            size_t size = sectionsNum;
            outFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); //Upisi broj sekcija u fajl
            for(tableIter; tableIter != linkerSymbolTable->end(); tableIter++) {
                if(tableIter->second->type == symbtype::LCL) { //Ako je sekcija
                    size = tableIter->first.size();
                    outFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); //Upisi velicinu stringa
                    outFile.write(tableIter->first.data(), size); //Upisi string
                    outFile.write(reinterpret_cast<char*>(&tableIter->second->index), sizeof(unsigned int)); 
                    outFile.write(reinterpret_cast<char*>(&tableIter->second->size), sizeof(unsigned int)); 
                    outFile.write(reinterpret_cast<char*>(tableIter->second->contents->data()), tableIter->second->size); 
                }
            }

            size = linkerSymbolTable->size();
            outFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); //Upisi broj simbola u fajl
            for(tableIter = linkerSymbolTable->begin(); tableIter != linkerSymbolTable->end(); tableIter++) {
                size = tableIter->first.size();
                outFile.write(reinterpret_cast<char*>(&(size)), sizeof(size_t)); //Upisi velicinu stringa
                outFile.write(tableIter->first.data(), size); //Upisi string
                outFile.write(reinterpret_cast<char*>(&tableIter->second->index), sizeof(unsigned int)); 
                outFile.write(reinterpret_cast<char*>(&tableIter->second->section), sizeof(unsigned int));
                outFile.write(reinterpret_cast<char*>(&tableIter->second->value), sizeof(unsigned int)); 
                outFile.write(reinterpret_cast<char*>(&tableIter->second->type), sizeof(symbtype));
            }

            size = sectionsNum;
            //Upisi broj relokacionih tabela u fajl, za svaku sekciju po jedna pa makar i bila prazna
            outFile.write(reinterpret_cast<char*>(&size), sizeof(size_t));
            for(tableIter = linkerSymbolTable->begin(); tableIter != linkerSymbolTable->end(); tableIter++) { 
                if(tableIter->second->type == symbtype::LCL) { //Ako je sekcija
                    size = tableIter->second->relocTable->size();
                    outFile.write(reinterpret_cast<char*>(&size), sizeof(size_t)); //Upisi broj relokacija u tabeli
                    for(auto relocIter : *(tableIter->second->relocTable)) {
                        size = relocIter->symbName.size();
                        outFile.write(reinterpret_cast<char*>(&(size)), sizeof(size_t)); //Upisi velicinu stringa 
                        outFile.write(relocIter->symbName.data(), size); 
                        outFile.write(reinterpret_cast<char*>(&relocIter->index), sizeof(unsigned int)); 
                        outFile.write(reinterpret_cast<char*>(&relocIter->section), sizeof(unsigned int)); 
                        outFile.write(reinterpret_cast<char*>(&relocIter->location), sizeof(unsigned int)); 
                        outFile.write(reinterpret_cast<char*>(&relocIter->addend), sizeof(unsigned int)); 
                        outFile.write(reinterpret_cast<char*>(&relocIter->type), sizeof(reloctype)); 
                    }
                }
            }
            outFile.close();
        }
    }

    freeMem(); //Oslobadjanje zauzete memorije
    return 0;
}