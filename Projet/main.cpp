#define NOMINMAX 
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <limits> 
#include <map> 

// LIENS ASM
extern "C" void InitNoyauASM();
extern "C" void LibererMemoireASM();
extern "C" void EnregistrerProcessusASM(int id, int taille);
extern "C" int GetNbProgsASM();
extern "C" int GetProcessInfoASM(int index, int* id, int* taille, int* etat, void** addr);
extern "C" void* LancerProcessusASM(int id);
extern "C" int FermerProcessusASM(int id);
extern "C" void* UtiliserProcessusASM(int id);
// <--- AJOUT ETAPE 7
extern "C" int VerifierAccesASM(int id, void* addr);

extern "C" long long GetRamUsageASM();
extern "C" long long GetRamTotalASM();
extern "C" long long GetMvUsageASM();
extern "C" long long GetMvTotalASM();
extern "C" void* GetPtrRAM();
extern "C" void* GetPtrMV();

std::map<int, std::vector<std::string>> g_CodeSource;

void InitialiserProgrammes() {
    std::string dossier = "FichiersProgs/";
    std::string noms[] = { "prog1.txt", "prog2.txt", "prog3.txt" };
    int id = 1;
    for (const auto& nom : noms) {
        std::ifstream f(dossier + nom);
        if (f.is_open()) {
            int taille = 0;
            if (f >> taille) {
                EnregistrerProcessusASM(id, taille);
                std::string ligne;
                std::getline(f, ligne);
                while (std::getline(f, ligne)) {
                    if (!ligne.empty() && ligne.find_first_not_of(" \t\r\n") != std::string::npos) {
                        g_CodeSource[id].push_back(ligne);
                    }
                }
                id++;
            }
            f.close();
        }
    }
}

long long CalculerUsageReel(int typeMemoireCible) {
    long long usage = 0;
    int nb = GetNbProgsASM();
    for (int i = 0; i < nb; i++) {
        int id, taille, etat;
        void* addr;
        GetProcessInfoASM(i, &id, &taille, &etat, &addr);
        if (etat == typeMemoireCible) usage += taille;
    }
    return usage;
}

void DessinerBarreAvecPlage(std::string titre, void* debut, long long total, int typeMemoire) {
    long long utilise = CalculerUsageReel(typeMemoire);
    void* fin = reinterpret_cast<char*>(debut) + total;
    double pct = 0;
    if (total > 0) pct = (double)utilise / total * 100.0;

    std::cout << " " << std::left << std::setw(20) << titre << " [Plage: " << debut << " - " << fin << "]\n";
    std::cout << "   Usage: " << std::right << std::setw(6) << utilise << " / " << total << " octets (" << std::fixed << std::setprecision(1) << pct << "%)\n";
    std::cout << "   [";
    int width = 50;
    int pos = width * pct / 100;
    for (int i = 0; i < width; ++i) { if (i < pos) std::cout << "#"; else std::cout << "."; }
    std::cout << "]\n\n";
}

void AfficherTableauProcessus() {
    std::cout << " LISTE DES PROCESSUS\n";
    std::cout << " +----+----------+---------------+------------------+------------------+\n";
    std::cout << " | ID | TAILLE   | ETAT          | DEBUT (ADDR)     | FIN (ADDR)       |\n";
    std::cout << " +----+----------+---------------+------------------+------------------+\n";

    int nb = GetNbProgsASM();
    for (int i = 0; i < nb; i++) {
        int id, taille, etat;
        void* addrDebut;
        GetProcessInfoASM(i, &id, &taille, &etat, &addrDebut);

        std::string sEtat = "FERME";
        void* addrFin = nullptr;
        if (etat == 1) sEtat = "EN RAM";
        if (etat == 2) sEtat = "SWAPPE (MV)";
        if (etat != 0 && addrDebut != nullptr) addrFin = reinterpret_cast<char*>(addrDebut) + taille;
        else { addrDebut = 0; addrFin = 0; }

        std::cout << " | " << std::left << std::setw(2) << id << " | " << std::setw(8) << taille << " | " << std::setw(13) << sEtat << " | " << std::setw(16) << addrDebut << " | " << std::setw(16) << addrFin << " |\n";
    }
    std::cout << " +----+----------+---------------+------------------+------------------+\n";
}

void AfficherContenuRAM() {
    std::cout << "\n CONTENU DE LA RAM (Instructions chargees)\n";
    std::cout << " =========================================\n";
    int nb = GetNbProgsASM();
    bool unProgrammeEnRam = false;
    for (int i = 0; i < nb; i++) {
        int id, taille, etat;
        void* addr;
        GetProcessInfoASM(i, &id, &taille, &etat, &addr);
        if (etat == 1) {
            unProgrammeEnRam = true;
            std::cout << " > PROGRAMME ID " << id << " (" << taille << " o) :\n   ---------------------\n";
            if (g_CodeSource.count(id)) { for (const auto& ligne : g_CodeSource[id]) std::cout << "      " << ligne << "\n"; }
            std::cout << "\n";
        }
    }
    if (!unProgrammeEnRam) std::cout << " (Aucun programme en cours d'execution)\n";
    std::cout << " =========================================\n";
}

int main() {
    try { InitNoyauASM(); }
    catch (...) { return -1; }
    InitialiserProgrammes();
    std::string messageNotification = "Systeme initialise. Pret.";

    while (true) {
        system("cls");
        std::cout << "======================================================================\n";
        std::cout << "              SIMULATEUR DE MEMOIRE VIRTUELLE (OS)                    \n";
        std::cout << "======================================================================\n";
        std::cout << " >> MESSAGE : " << messageNotification << "\n";
        std::cout << "======================================================================\n\n";

        DessinerBarreAvecPlage("RAM PHYSIQUE", GetPtrRAM(), GetRamTotalASM(), 1);
        DessinerBarreAvecPlage("FICHIER D'ECHANGE", GetPtrMV(), GetMvTotalASM(), 2);
        AfficherTableauProcessus();
        AfficherContenuRAM();

        std::cout << "\n ACTIONS :\n";
        std::cout << "  1. Charger (Nouveau Lancement)\n";
        std::cout << "  2. Fermer (Liberer)\n";
        std::cout << "  5. Utiliser / Activer (Swap-In si necessaire)\n";
        std::cout << "  6. Demander un Acces Memoire (Test Protection)\n"; // <--- AJOUT ETAPE 7
        std::cout << "  9. Quitter\n";
        std::cout << "\n Choix > ";

        int choix;
        if (!(std::cin >> choix)) {
            std::cin.clear(); std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            continue;
        }

        if (choix == 9) break;

        if (choix == 1) {
            std::cout << "ID a charger > "; int id; std::cin >> id;
            void* res = LancerProcessusASM(id);
            if (res) messageNotification = "[SUCCES] Prog " + std::to_string(id) + " charge.";
            else messageNotification = "[ERREUR] Echec chargement ID " + std::to_string(id);
        }
        else if (choix == 2) {
            std::cout << "ID a fermer > "; int id; std::cin >> id;
            if (FermerProcessusASM(id)) messageNotification = "[SUCCES] Prog " + std::to_string(id) + " ferme.";
            else messageNotification = "[ERREUR] ID introuvable.";
        }
        else if (choix == 5) {
            std::cout << "ID a utiliser > "; int id; std::cin >> id;
            void* res = UtiliserProcessusASM(id);
            if (res) messageNotification = "[ACTIF] Prog " + std::to_string(id) + " est maintenant en RAM.";
            else messageNotification = "[ERREUR] Impossible d'activer ID " + std::to_string(id);
        }
        else if (choix == 6) { // <--- LOGIQUE ETAPE 7
            std::cout << "ID du programme qui demande l'acces > ";
            int id; std::cin >> id;

            std::cout << "Adresse cible (Hex, ex: 000001D...) > ";
            size_t addrVal;
            std::cin >> std::hex >> addrVal; // Lecture hexadécimale
            std::cin >> std::dec; // Remettre en décimal pour la suite

            void* ptrCible = (void*)addrVal;

            int res = VerifierAccesASM(id, ptrCible);

            if (res == 1) {
                messageNotification = "[ACCES AUTORISE] L'adresse appartient bien au programme " + std::to_string(id);
            }
            else if (res == 0) {
                messageNotification = "[ACCES REFUSE] SEGMENTATION FAULT ! Adresse hors limites.";
            }
            else if (res == -1) {
                messageNotification = "[ERREUR] Le programme n'est pas charge en RAM (Page Fault).";
            }
            else {
                messageNotification = "[ERREUR] ID Programme inconnu.";
            }
        }
    }
    LibererMemoireASM();
    return 0;
}