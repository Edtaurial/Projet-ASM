#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

extern "C" void InitNoyauASM();
extern "C" void LibererMemoireASM();
extern "C" void EnregistrerProcessusASM(int id, int taille);
extern "C" int GetNbProgsASM();
extern "C" int GetProcessInfoASM(int index, int* id, int* taille, int* etat, void** addr);
extern "C" void* LancerProcessusASM(int id);

// Getters RAM
extern "C" long long GetRamUsageASM();
extern "C" long long GetRamTotalASM();
// Getters MV (Nouveaux)
extern "C" long long GetMvUsageASM();
extern "C" long long GetMvTotalASM();

void InitialiserProgrammes() {
    std::string dossier = "FichiersProgs/";
    std::string noms[] = { "prog1.txt", "prog2.txt", "prog3.txt" };
    int id = 1;
    for (const auto& nom : noms) {
        std::ifstream f(dossier + nom);
        if (f.is_open()) {
            int taille = 0;
            if (f >> taille) EnregistrerProcessusASM(id++, taille);
            f.close();
        }
    }
}

// Fonction helper pour dessiner une barre
void DessinerBarre(long long utilise, long long total, std::string nom) {
    double pct = 0;
    if (total > 0) pct = (double)utilise / total * 100.0;

    std::cout << nom << " : " << utilise << " / " << total << " octets ("
        << std::fixed << std::setprecision(1) << pct << "%)\n";
    std::cout << "[";
    int width = 40;
    int pos = width * pct / 100;
    for (int i = 0; i < width; ++i) {
        if (i < pos) std::cout << "#"; else std::cout << ".";
    }
    std::cout << "]\n";
}

void AfficherEtat() {
    system("cls");
    std::cout << "=== GESTIONNAIRE MEMOIRE (INF22107) ===\n\n";

    // 1. Barre RAM
    DessinerBarre(GetRamUsageASM(), GetRamTotalASM(), "RAM Simulee      ");

    // 2. Barre MV
    DessinerBarre(GetMvUsageASM(), GetMvTotalASM(), "Memoire Virtuelle");

    std::cout << "\n";
    std::cout << "ID   Taille       Etat            Adresse (Phy/Virt)\n";
    std::cout << "---------------------------------------------------\n";

    int nb = GetNbProgsASM();
    for (int i = 0; i < nb; i++) {
        int id, taille, etat;
        void* addr;
        GetProcessInfoASM(i, &id, &taille, &etat, &addr);

        std::string sEtat = "FERME";
        if (etat == 1) sEtat = "EN RAM";
        if (etat == 2) sEtat = "SWAPPE (MV)";

        std::cout << std::left << std::setw(5) << id
            << std::setw(13) << taille
            << std::setw(16) << sEtat
            << addr << "\n";
    }
    std::cout << "---------------------------------------------------\n";
}

int main() {
    try { InitNoyauASM(); }
    catch (...) { return -1; }
    InitialiserProgrammes();

    while (true) {
        AfficherEtat();
        std::cout << "\nCOMMANDES:\n[1-3] Lancer ID\n[9] Quitter\n> ";
        int choix;
        std::cin >> choix;

        if (choix == 9) break;
        if (choix >= 1 && choix <= 3) {
            void* res = LancerProcessusASM(choix);
            // On peut ajouter un message ici si on veut, mais l'affichage suffit
        }
    }
    LibererMemoireASM();
    return 0;
}