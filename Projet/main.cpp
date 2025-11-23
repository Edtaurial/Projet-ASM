#define NOMINMAX 
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <limits> 
#include <map> // <--- AJOUT pour stocker le code source

// =============================================================
// LIENS AVEC LE NOYAU ASSEMBLEUR
// =============================================================
extern "C" void InitNoyauASM();
extern "C" void LibererMemoireASM();
extern "C" void EnregistrerProcessusASM(int id, int taille);
extern "C" int GetNbProgsASM();
extern "C" int GetProcessInfoASM(int index, int* id, int* taille, int* etat, void** addr);
extern "C" void* LancerProcessusASM(int id);
extern "C" int FermerProcessusASM(int id);

extern "C" long long GetRamUsageASM();
extern "C" long long GetRamTotalASM();
extern "C" long long GetMvUsageASM();
extern "C" long long GetMvTotalASM();
extern "C" void* GetPtrRAM();
extern "C" void* GetPtrMV();

// =============================================================
// DONNEES GLOBALES C++ (Pour l'affichage texte uniquement)
// =============================================================
// On stocke ici les lignes de code de chaque programme (ID -> Lignes)
std::map<int, std::vector<std::string>> g_CodeSource;

// =============================================================
// OUTILS
// =============================================================

void InitialiserProgrammes() {
    std::string dossier = "FichiersProgs/";
    std::string noms[] = { "prog1.txt", "prog2.txt", "prog3.txt" };
    int id = 1;

    for (const auto& nom : noms) {
        std::ifstream f(dossier + nom);
        if (f.is_open()) {
            int taille = 0;
            // 1. Lire la taille (première ligne)
            if (f >> taille) {
                EnregistrerProcessusASM(id, taille);

                // 2. Lire le reste du fichier (Instructions)
                std::string ligne;
                std::getline(f, ligne); // Consommer le saut de ligne après la taille

                while (std::getline(f, ligne)) {
                    // On ne stocke que les lignes non vides
                    if (!ligne.empty() && ligne.find_first_not_of(" \t\r\n") != std::string::npos) {
                        g_CodeSource[id].push_back(ligne);
                    }
                }
                id++; // Passer à l'ID suivant seulement si lecture OK
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

    std::cout << " " << std::left << std::setw(20) << titre
        << " [Plage: " << debut << " - " << fin << "]\n";
    std::cout << "   Usage: " << std::right << std::setw(6) << utilise << " / " << total << " octets "
        << "(" << std::fixed << std::setprecision(1) << pct << "%)\n";

    std::cout << "   [";
    int width = 50;
    int pos = width * pct / 100;
    for (int i = 0; i < width; ++i) {
        if (i < pos) std::cout << "#"; else std::cout << ".";
    }
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

        if (etat != 0 && addrDebut != nullptr) {
            addrFin = reinterpret_cast<char*>(addrDebut) + taille;
        }
        else {
            addrDebut = 0; addrFin = 0;
        }

        std::cout << " | " << std::left << std::setw(2) << id
            << " | " << std::setw(8) << taille
            << " | " << std::setw(13) << sEtat
            << " | " << std::setw(16) << addrDebut
            << " | " << std::setw(16) << addrFin << " |\n";
    }
    std::cout << " +----+----------+---------------+------------------+------------------+\n";
}

// <--- NOUVELLE FONCTION POUR AFFICHER LE CONTENU
void AfficherContenuRAM() {
    std::cout << "\n CONTENU DE LA RAM (Instructions chargees)\n";
    std::cout << " =========================================\n";

    int nb = GetNbProgsASM();
    bool unProgrammeEnRam = false;

    for (int i = 0; i < nb; i++) {
        int id, taille, etat;
        void* addr;
        GetProcessInfoASM(i, &id, &taille, &etat, &addr);

        // Si le programme est EN RAM (etat == 1)
        if (etat == 1) {
            unProgrammeEnRam = true;
            std::cout << " > PROGRAMME ID " << id << " (" << taille << " o) :\n";
            std::cout << "   ---------------------\n";

            // Afficher les lignes stockées
            if (g_CodeSource.count(id)) {
                for (const auto& ligne : g_CodeSource[id]) {
                    std::cout << "      " << ligne << "\n";
                }
            }
            else {
                std::cout << "      (Pas d'instructions lisibles)\n";
            }
            std::cout << "\n";
        }
    }

    if (!unProgrammeEnRam) {
        std::cout << " (Aucun programme en cours d'execution)\n";
    }
    std::cout << " =========================================\n";
}

// =============================================================
// MAIN
// =============================================================
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

        // --- ETAT ---
        DessinerBarreAvecPlage("RAM PHYSIQUE", GetPtrRAM(), GetRamTotalASM(), 1);
        DessinerBarreAvecPlage("FICHIER D'ECHANGE", GetPtrMV(), GetMvTotalASM(), 2);

        // --- TABLEAU ---
        AfficherTableauProcessus();

        // --- CONTENU RAM (Nouvelle section) ---
        AfficherContenuRAM();

        // --- MENU ---
        std::cout << "\n ACTIONS :\n";
        std::cout << "  1. Charger (Lancer)\n";
        std::cout << "  2. Fermer (Liberer)\n";
        std::cout << "  3. Quitter\n";
        std::cout << "\n Choix > ";

        int choix;
        if (!(std::cin >> choix)) {
            std::cin.clear();
            std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            messageNotification = "ERREUR : Entree invalide.";
            continue;
        }

        if (choix == 3) break;

        if (choix == 1) {
            std::cout << "ID a charger (1-" << GetNbProgsASM() << ") > ";
            int id;
            std::cin >> id;

            void* res = LancerProcessusASM(id);
            if (res != nullptr) {
                messageNotification = "[SUCCES] Programme " + std::to_string(id) + " charge.";
            }
            else {
                messageNotification = "[ERREUR] Echec chargement ID " + std::to_string(id) + ".";
            }
        }
        else if (choix == 2) {
            std::cout << "ID a fermer > ";
            int id;
            std::cin >> id;

            int res = FermerProcessusASM(id);
            if (res == 1) {
                messageNotification = "[SUCCES] Programme " + std::to_string(id) + " ferme.";
            }
            else {
                messageNotification = "[ERREUR] Impossible de fermer l'ID " + std::to_string(id) + ".";
            }
        }
        else {
            messageNotification = "Choix inconnu.";
        }
    }

    LibererMemoireASM();
    std::cout << "\nFermeture du systeme...\n";
    return 0;
}