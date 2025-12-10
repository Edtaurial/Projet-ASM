#define NOMINMAX 
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <limits> 
#include <map> 



// fonctions pour les actions
extern "C" void InitNoyauASM();
extern "C" void LibererMemoireASM();
extern "C" void EnregistrerProcessusASM(int id, int taille);
extern "C" void* LancerProcessusASM(int id);
extern "C" int FermerProcessusASM(int id);
extern "C" void* UtiliserProcessusASM(int id);
extern "C" int VerifierAccesASM(int id, void* addr);

// fonctions pour la consultation (Getters)
extern "C" int GetNbProgsASM();
extern "C" int GetProcessInfoASM(int index, int* id, int* taille, int* etat, void** addr);
extern "C" long long GetRamUsageASM();
extern "C" long long GetRamTotalASM();
extern "C" long long GetMvUsageASM();
extern "C" long long GetMvTotalASM();
extern "C" void* GetPtrRAM();
extern "C" void* GetPtrMV();
extern "C" int GetActiveProgIDASM();
extern "C" void SetActiveProgIDASM(int id);

// =========================================================
// SECTION : DONNÉES LOCALES (SIMULATION VISUELLE)
// =========================================================
// Cette map sert uniquement à afficher le "code source" à l'écran.
// L'ASM gère la mémoire, mais ne stocke pas le texte des instructions "MOV, ADD...".
// Le C++ garde ce texte pour faire joli dans l'interface "CPU".
std::map<int, std::vector<std::string>> g_CodeSource;

void InitialiserProgrammes() {
    std::string dossier = "FichiersProgs/";
    std::string noms[] = { "prog1.txt", "prog2.txt", "prog3.txt" };
    int id = 1;

    for (const auto& nom : noms) {
        std::ifstream f(dossier + nom);
        if (f.is_open()) {
            int taille = 0;
            // 1. Lire la taille pour l'envoyer au Noyau ASM (Allocation réelle)
            if (f >> taille) {
                EnregistrerProcessusASM(id, taille);

                // 2. Lire le reste du texte pour l'affichage C++ (Simulation visuelle)
                std::string ligne;
                std::getline(f, ligne); // Skip ligne taille
                while (std::getline(f, ligne)) {
                    // Nettoyage des lignes vides
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

// Fonction auxiliaire : Le C++ doit demander à l'ASM qui est où
// pour calculer correctement la barre de chargement.
long long CalculerUsageReel(int typeMemoireCible) {
    long long usage = 0;
    int nb = GetNbProgsASM();
    for (int i = 0; i < nb; i++) {
        int id, taille, etat;
        void* addr;
        // On interroge le PCB de chaque processus via l'ASM
        GetProcessInfoASM(i, &id, &taille, &etat, &addr);
        if (etat == typeMemoireCible) usage += taille;
    }
    return usage;
}

// Affiche l'en-tête avec les adresses physiques réelles retournées par VirtualAlloc
void DessinerBarreAvecPlage(std::string titre, void* debut, long long total, int typeMemoire) {
    long long utilise = CalculerUsageReel(typeMemoire);

    // Arithmétique de pointeurs pour l'affichage : Debut + Taille = Fin
    void* fin = reinterpret_cast<char*>(debut) + total;

    std::cout << " " << std::left << std::setw(20) << titre << " [Plage: " << debut << " - " << fin << "]\n";
    std::cout << " Etat: " << std::right << std::setw(6) << utilise << " / " << total << " octets \n";
    std::cout << "\n\n";
}

// Affiche le tableau principal. C'est une "Vue" sur les données du Noyau ASM.
void AfficherTableauProcessus() {
    int actifID = GetActiveProgIDASM(); // Qui est le "focus" actuel ?

    std::cout << " LISTE DES PROCESSUS\n";
    std::cout << " +----+----------+---------------------+------------------+------------------+\n";
    std::cout << " | ID |TAILLE Ko | ETAT                | ADRESSE DE DEBUT | ADRESSE DE FIN   |\n";
    std::cout << " +----+----------+---------------------+------------------+------------------+\n";

    int nb = GetNbProgsASM();
    for (int i = 0; i < nb; i++) {
        int id, taille, etat;
        void* addrDebut;

        // APPEL NOYAU : Récupère l'état actuel (RAM, SWAP, ou FERMÉ)
        GetProcessInfoASM(i, &id, &taille, &etat, &addrDebut);

        std::string sEtat = "FERME";
        void* addrFin = nullptr;

        if (etat == 1) {
            if (id == actifID) sEtat = ">> EN EXECUTION <<";
            else sEtat = "OUVERT (EN RAM)";
        }
        if (etat == 2) sEtat = "SWAPPE (MV)";

        // Calcul adresse fin pour affichage seulement si le prog est chargé
        if (etat != 0 && addrDebut != nullptr)
            addrFin = reinterpret_cast<char*>(addrDebut) + taille;
        else {
            addrDebut = 0; addrFin = 0;
        }

        // Affichage formatté
        std::cout << " | " << std::left << std::setw(2) << id
            << " | " << std::setw(8) << taille / 1024 // Conversion inverse pour affichage Ko
            << " | " << std::setw(19) << sEtat
            << " | " << std::setw(16) << addrDebut
            << " | " << std::setw(16) << addrFin << " |\n";
    }
    std::cout << " +----+----------+---------------------+------------------+------------------+\n";
}

// Simule l'affichage du CPU (Instruction Pointer)
void AfficherProgrammeActif() {
    int actifID = GetActiveProgIDASM();
    std::cout << "\n CPU (PROGRAMME EN COURS D'EXECUTION)\n";
    std::cout << " ====================================\n";
    if (actifID > 0) {
        // On récupère le texte stocké localement dans la map C++
        if (g_CodeSource.count(actifID)) {
            std::cout << " > PROGRAMME ID " << actifID << " execute actuellement :\n";
            std::cout << "   --------------------------------------\n";
            std::cout << "   IP -> ";
            // Affiche les instructions fictives
            for (const auto& ligne : g_CodeSource[actifID]) std::cout << ligne << "\n         ";
        }
    }
    else {
        std::cout << " (IDLE - Aucun programme actif)\n";
    }
    std::cout << " ====================================\n";
}

int main() {
    // 1. Initialisation du Noyau (Allocation mémoire Windows)
    try { InitNoyauASM(); }
    catch (...) { return -1; }

    // 2. Chargement des fichiers textes
    InitialiserProgrammes();
    std::string messageNotification = "Systeme initialise.";

    // 3. Boucle Principale (Shell)
    while (true) {
        system("cls"); // Rafraîchissement écran

        // --- EN-TÊTE ---
        std::cout << "======================================================================\n";
        std::cout << "              SIMULATEUR DE MEMOIRE VIRTUELLE                         \n";
        std::cout << "======================================================================\n\n";

        // --- AFFICHAGE ETAT (Interrogation du Noyau ASM) ---
        DessinerBarreAvecPlage("RAM PHYSIQUE", GetPtrRAM(), GetRamTotalASM(), 1);
        DessinerBarreAvecPlage("MEMOIRE VIRTUELLE", GetPtrMV(), GetMvTotalASM(), 2);

        AfficherTableauProcessus();
        AfficherProgrammeActif();

        // --- ZONE DE NOTIFICATION ---
        std::cout << "\n";
        std::cout << " >>> DERNIER MESSAGE : " << messageNotification << "\n";
        std::cout << " ----------------------------------------------------------------------\n";

        // --- MENU UTILISATEUR ---
        std::cout << " ACTIONS :\n";
        std::cout << "  1. Charger (Lancer)\n";
        std::cout << "  2. Fermer (Liberer)\n";
        std::cout << "  5. Utiliser / Activer (Mettre en Execution)\n";
        std::cout << "  6. Test Acces Memoire (Simulation Protection)\n";
        std::cout << "  9. Quitter\n";
        std::cout << "\n Choix > ";

        int choix;
        // Sécurisation entrée utilisateur
        if (!(std::cin >> choix)) {
            std::cin.clear(); std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            continue;
        }

        if (choix == 9) break;

        messageNotification = "En attente...";

        // --- APPELS SYSTEME (Vers ASM) ---

        if (choix == 1) {
            std::cout << "ID a charger > "; int id; std::cin >> id;
            // Appel ASM : Tente d'allouer en RAM (et Swap si nécessaire)
            void* res = LancerProcessusASM(id);
            if (res) messageNotification = "[SUCCES] Prog " + std::to_string(id) + " est maintenant EN EXECUTION.";
            else messageNotification = "[ERREUR] Echec chargement ID " + std::to_string(id);
        }
        else if (choix == 2) {
            std::cout << "ID a fermer > "; int id; std::cin >> id;
            // Appel ASM : Efface la mémoire et met à jour le PCB
            if (FermerProcessusASM(id)) messageNotification = "[SUCCES] Prog " + std::to_string(id) + " ferme.";
            else messageNotification = "[ERREUR] ID introuvable.";
        }
        else if (choix == 5) {
            std::cout << "ID a activer > "; int id; std::cin >> id;
            // Appel ASM : Swap In si nécessaire pour rendre actif
            void* res = UtiliserProcessusASM(id);
            if (res) messageNotification = "[ACTIF] Prog " + std::to_string(id) + " a pris le controle.";
            else messageNotification = "[ERREUR] Impossible d'activer ID " + std::to_string(id);
        }
        else if (choix == 6) {
            // Test de la protection mémoire (Segmentation Fault simulation)
            std::cout << "ID Prog > "; int id; std::cin >> id;
            std::cout << "Adresse Hex (ex: 14000) > ";
            size_t addrVal;
            std::cin >> std::hex >> addrVal; std::cin >> std::dec;

            // Appel ASM : Vérifie les bornes
            int res = VerifierAccesASM(id, (void*)addrVal);

            if (res == 1) messageNotification = "[ACCES OK] Adresse valide.";
            else if (res == 0) messageNotification = "[ACCES REFUSE] SEGMENTATION FAULT (Hors limites).";
            else if (res == -1) messageNotification = "[ACCES REFUSE] PAGE FAULT (Prog en Swap).";
            else messageNotification = "[ERREUR] ID inconnu.";
        }
    }

    // Libération finale des ressources Windows
    LibererMemoireASM();
    return 0;
}