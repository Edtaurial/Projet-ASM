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
extern "C" int VerifierAccesASM(int id, void* addr);

extern "C" long long GetRamUsageASM();
extern "C" long long GetRamTotalASM();
extern "C" long long GetMvUsageASM();
extern "C" long long GetMvTotalASM();
extern "C" void* GetPtrRAM();
extern "C" void* GetPtrMV();
extern "C" int GetActiveProgIDASM();
extern "C" void SetActiveProgIDASM(int id);

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
    std::cout << " Etat: " << std::right << std::setw(6) << utilise << " / " << total << " octets \n";
    /*std::cout << "   [";
    int width = 50;
    int pos = width * pct / 100;
    for (int i = 0; i < width; ++i) { if (i < pos) std::cout << "#"; else std::cout << "."; }
    std::cout << "]\n\n";*/
    std::cout << "\n\n";
}

void AfficherTableauProcessus() {
    int actifID = GetActiveProgIDASM();
    std::cout << " LISTE DES PROCESSUS\n";
    std::cout << " +----+----------+---------------------+------------------+------------------+\n";
    std::cout << " | ID | TAILLE   | ETAT                | ADRESSE DE DEBUT | ADRESSE DE FIN   |\n";
    std::cout << " +----+----------+---------------------+------------------+------------------+\n";

    int nb = GetNbProgsASM();
    for (int i = 0; i < nb; i++) {
        int id, taille, etat;
        void* addrDebut;
        GetProcessInfoASM(i, &id, &taille, &etat, &addrDebut);

        std::string sEtat = "FERME";
        void* addrFin = nullptr;

        if (etat == 1) {
            if (id == actifID) sEtat = ">> EN EXECUTION <<";
            else sEtat = "OUVERT (EN RAM)";
        }
        if (etat == 2) sEtat = "SWAPPE (MV)";

        if (etat != 0 && addrDebut != nullptr) addrFin = reinterpret_cast<char*>(addrDebut) + taille;
        else { addrDebut = 0; addrFin = 0; }

        std::cout << " | " << std::left << std::setw(2) << id
            << " | " << std::setw(8) << taille/1024
            << " | " << std::setw(19) << sEtat
            << " | " << std::setw(16) << addrDebut
            << " | " << std::setw(16) << addrFin << " |\n";
    }
    std::cout << " +----+----------+---------------------+------------------+------------------+\n";
}

void AfficherProgrammeActif() {
    int actifID = GetActiveProgIDASM();
    std::cout << "\n CPU (PROGRAMME EN COURS D'EXECUTION)\n";
    std::cout << " ====================================\n";
    if (actifID > 0) {
        if (g_CodeSource.count(actifID)) {
            std::cout << " > PROGRAMME ID " << actifID << " execute actuellement :\n";
            std::cout << "   --------------------------------------\n";
            std::cout << "   IP -> ";
            for (const auto& ligne : g_CodeSource[actifID]) std::cout << ligne << "\n         ";
        }
    }
    else {
        std::cout << " (IDLE - Aucun programme actif)\n";
    }
    std::cout << " ====================================\n";
}

int main() {
    try { InitNoyauASM(); }
    catch (...) { return -1; }
    InitialiserProgrammes();
    std::string messageNotification = "Systeme initialise.";

    while (true) {
        system("cls");

        // --- EN-TÊTE ---
        std::cout << "======================================================================\n";
        std::cout << "              SIMULATEUR DE MEMOIRE VIRTUELLE (OS)                    \n";
        std::cout << "======================================================================\n\n";

        // --- AFFICHAGE ETAT ---
        DessinerBarreAvecPlage("RAM PHYSIQUE", GetPtrRAM(), GetRamTotalASM(), 1);
        DessinerBarreAvecPlage("MEMOIRE VIRTUELLE", GetPtrMV(), GetMvTotalASM(), 2);

        AfficherTableauProcessus();
        AfficherProgrammeActif();

        // --- ZONE DE NOTIFICATION (Déplacée en bas) ---
        std::cout << "\n";
        std::cout << " >>> DERNIER MESSAGE : " << messageNotification << "\n";
        std::cout << " ----------------------------------------------------------------------\n";

        // --- MENU ---
        std::cout << " ACTIONS :\n";
        std::cout << "  1. Charger (Lancer)\n";
        std::cout << "  2. Fermer (Liberer)\n";
        std::cout << "  5. Utiliser / Activer (Mettre en Execution)\n";
        std::cout << "  6. Test Acces Memoire\n";
        std::cout << "  9. Quitter\n";
        std::cout << "\n Choix > ";

        int choix;
        if (!(std::cin >> choix)) {
            std::cin.clear(); std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            continue;
        }

        if (choix == 9) break;

        // Reset message par défaut si pas d'action
        messageNotification = "En attente...";

        if (choix == 1) {
            std::cout << "ID a charger > "; int id; std::cin >> id;
            void* res = LancerProcessusASM(id);
            if (res) messageNotification = "[SUCCES] Prog " + std::to_string(id) + " est maintenant EN EXECUTION.";
            else messageNotification = "[ERREUR] Echec chargement ID " + std::to_string(id);
        }
        else if (choix == 2) {
            std::cout << "ID a fermer > "; int id; std::cin >> id;
            if (FermerProcessusASM(id)) messageNotification = "[SUCCES] Prog " + std::to_string(id) + " ferme.";
            else messageNotification = "[ERREUR] ID introuvable.";
        }
        else if (choix == 5) {
            std::cout << "ID a activer > "; int id; std::cin >> id;
            void* res = UtiliserProcessusASM(id);
            if (res) messageNotification = "[ACTIF] Prog " + std::to_string(id) + " a pris le controle (EXECUTION).";
            else messageNotification = "[ERREUR] Impossible d'activer ID " + std::to_string(id);
        }
        /*else if (choix == 6) {
            std::cout << "ID Prog > "; int id; std::cin >> id;
            std::cout << "Adresse Hex > "; size_t addrVal; std::cin >> std::hex >> addrVal; std::cin >> std::dec;
            int res = VerifierAccesASM(id, (void*)addrVal);
            if (res == 1) messageNotification = "[ACCES OK] Adresse valide pour ce programme.";
            else messageNotification = "[ACCES REFUSE] Violation d'acces (SegFault).";
        }*/
        else if (choix == 6) {
            std::cout << "ID Prog > "; int id; std::cin >> id;

            std::cout << "Adresse Hex (ex: 14000) > ";
            size_t addrVal;
            std::cin >> std::hex >> addrVal;
            std::cin >> std::dec;

            int res = VerifierAccesASM(id, (void*)addrVal);

            // --- CORRECTION DE L'AFFICHAGE DES ERREURS ---
            if (res == 1) {
                messageNotification = "[ACCES OK] Adresse valide pour ce programme.";
            }
            else if (res == 0) {
                messageNotification = "[ACCES REFUSE] SEGMENTATION FAULT (Adresse hors limites).";
            }
            else if (res == -1) {
                messageNotification = "[ACCES REFUSE] PAGE FAULT (Le programme n'est pas en RAM).";
            }
            else {
                messageNotification = "[ERREUR] ID Programme inconnu.";
            }
            // ---------------------------------------------
        }
    }
    LibererMemoireASM();
    return 0;
}