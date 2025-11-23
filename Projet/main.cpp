#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>

// === LIENS ASM ===
extern "C" void InitNoyauASM();
extern "C" void LibererMemoireASM();
extern "C" void EnregistrerProcessusASM(int id, int taille);
extern "C" int GetNbProgsASM();
extern "C" int GetProcessInfoASM(int index, int* id, int* taille, int* etat, void** addr);
extern "C" void* LancerProcessusASM(int id);

// Getters Infos
extern "C" long long GetRamUsageASM();
extern "C" long long GetRamTotalASM();
extern "C" long long GetMvUsageASM();
extern "C" long long GetMvTotalASM();

// NOUVEAU : Getters Adresses Base
extern "C" void* GetPtrRAM();
extern "C" void* GetPtrMV();

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

// Affiche une plage mémoire "Debut - Fin"
void AfficherPlageMemoire(std::string nom, void* debut, long long taille, long long utilise) {
    // Calcul adresse fin = debut + taille
    // On utilise char* pour l'arithmétique de pointeurs (1 octet)
    void* fin = reinterpret_cast<char*>(debut) + taille;

    double pct = 0;
    if (taille > 0) pct = (double)utilise / taille * 100.0;

    std::cout << "--- " << nom << " ---\n";
    std::cout << "Plage    : " << debut << " a " << fin << "\n";
    std::cout << "Occupation: " << utilise << " / " << taille << " ("
        << std::fixed << std::setprecision(1) << pct << "%)\n";

    // Barre visuelle
    std::cout << "[";
    int width = 30;
    int pos = width * pct / 100;
    for (int i = 0; i < width; ++i) {
        if (i < pos) std::cout << "#"; else std::cout << ".";
    }
    std::cout << "]\n\n";
}

void AfficherEtat() {
    system("cls");
    std::cout << "=== SIMULATION GESTION MEMOIRE (INF22107) ===\n\n";

    // 1. Affichage Global RAM 
    AfficherPlageMemoire("RAM SIMULEE", GetPtrRAM(), GetRamTotalASM(), GetRamUsageASM());

    // 2. Affichage Global MV 
    AfficherPlageMemoire("MEMOIRE VIRTUELLE", GetPtrMV(), GetMvTotalASM(), GetMvUsageASM());

    // 3. Tableau des Programmes 
    // On élargit les colonnes pour les adresses
    std::cout << "ID   Taille     Etat           Debut               Fin\n";
    std::cout << "----------------------------------------------------------------------\n";

    int nb = GetNbProgsASM();
    for (int i = 0; i < nb; i++) {
        int id, taille, etat;
        void* addrDebut;

        GetProcessInfoASM(i, &id, &taille, &etat, &addrDebut);

        std::string sEtat = "FERME";
        void* addrFin = nullptr;

        if (etat == 1) sEtat = "EN RAM";
        if (etat == 2) sEtat = "SWAPPE";

        // Calcul Adresse Fin 
        // Si le programme est chargé (pas FERME), Fin = Debut + Taille
        if (etat != 0 && addrDebut != nullptr) {
            addrFin = reinterpret_cast<char*>(addrDebut) + taille;
        }
        else {
            // Si fermé, on met tout à 0 pour faire propre
            addrDebut = 0;
            addrFin = 0;
        }

        std::cout << std::left << std::setw(5) << id
            << std::setw(11) << taille
            << std::setw(15) << sEtat
            << addrDebut << "    "
            << addrFin << "\n";
    }
    std::cout << "----------------------------------------------------------------------\n";
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
            LancerProcessusASM(choix);
        }
    }
    LibererMemoireASM();
    return 0;
}