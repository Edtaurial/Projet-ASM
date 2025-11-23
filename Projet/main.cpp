#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <iomanip>

// === CONSTANTES ===
const size_t TAILLE_RAM = 4 * 1024 * 1024;
const size_t TAILLE_MV = 10 * 1024 * 1024;

// === STRUCTURES ===
enum Etat { FERME, EN_RAM, SWAPPE };

struct Programme {
    int id;
    std::string nomFichier;
    int tailleRequise;
    std::vector<std::string> instructions;
    Etat etat;
    void* adresseMemoire;
};

// === GLOBALES ===
void* g_RamPtr = nullptr;
void* g_MvPtr = nullptr;
size_t g_OffsetLibreRAM = 0; // Pointeur de suivi de l'utilisation RAM (Simulateur d'allocateur)
std::vector<Programme> g_Programmes;

// === IMPORTATION MASM ===
extern "C" void InitialiserMemoireASM(void* ptrRam);
// Allocateur MASM : retourne l'adresse ou 0
extern "C" void* AllouerMemoireRAM(void* baseRam, size_t* ptrOffset, size_t tailleTotal, int tailleRequise);
// Copieur MASM
extern "C" void CopierDonneesASM(void* dest, const char* source, size_t taille);

// === FONCTIONS ===

void ChargerFichiersProgrammes() {
    g_Programmes.clear();
    std::string fichiers[] = { "prog1.txt", "prog2.txt", "prog3.txt" };
    int idCounter = 1;

    for (const auto& nom : fichiers) {
        std::ifstream file(nom);
        if (!file.is_open()) continue;

        Programme p;
        p.id = idCounter++;
        p.nomFichier = nom;
        p.etat = FERME;
        p.adresseMemoire = nullptr;

        if (!(file >> p.tailleRequise)) p.tailleRequise = 0;

        std::string ligne;
        std::getline(file, ligne);
        while (std::getline(file, ligne)) {
            if (!ligne.empty()) p.instructions.push_back(ligne);
        }
        g_Programmes.push_back(p);
        file.close();
    }
}

void AfficherEtatSysteme() {
    system("cls"); // Nettoyer l'écran pour clarté
    std::cout << "=== GESTIONNAIRE DE MEMOIRE VIRTUELLE ===\n";
    std::cout << "RAM Base: " << g_RamPtr << " | Utilise: " << g_OffsetLibreRAM << " / " << TAILLE_RAM << "\n";
    std::cout << "-------------------------------------------------------------\n";
    std::cout << std::left << std::setw(3) << "ID" << std::setw(12) << "Fichier"
        << std::setw(8) << "Taille" << std::setw(12) << "Etat"
        << "Adresse RAM" << std::endl;
    std::cout << "-------------------------------------------------------------\n";

    for (const auto& p : g_Programmes) {
        std::string etatStr = (p.etat == FERME) ? "FERME" : (p.etat == EN_RAM ? "EN RAM" : "SWAPPE");
        std::cout << std::left << std::setw(3) << p.id
            << std::setw(12) << p.nomFichier
            << std::setw(8) << p.tailleRequise
            << std::setw(12) << etatStr
            << p.adresseMemoire << std::endl;
    }
    std::cout << "-------------------------------------------------------------\n";
}

// Logique "Step 3": Lancer un programme
void LancerProgramme(int id) {
    // 1. Trouver le programme
    Programme* prog = nullptr;
    for (auto& p : g_Programmes) {
        if (p.id == id) { prog = &p; break; }
    }

    if (!prog) { std::cout << "Programme introuvable.\n"; return; }
    if (prog->etat == EN_RAM) { std::cout << "Programme deja en cours.\n"; return; }

    std::cout << "Tentative de chargement de " << prog->nomFichier << " (" << prog->tailleRequise << " octets)...\n";

    // 2. Allocation via MASM
    // On passe l'adresse de g_OffsetLibreRAM pour que MASM la mette à jour
    void* adresseAllouee = AllouerMemoireRAM(g_RamPtr, &g_OffsetLibreRAM, TAILLE_RAM, prog->tailleRequise);

    if (adresseAllouee != nullptr) {
        // --- SUCCES ALLOCATION ---
        prog->adresseMemoire = adresseAllouee;
        prog->etat = EN_RAM;

        // 3. Préparation des données brutes (Instructions)
        std::string rawData = "";
        for (const auto& line : prog->instructions) {
            rawData += line + "\n"; // On sépare par des sauts de ligne dans la mémoire
        }

        // 4. Copie via MASM (Simulation du chargement des instructions) [cite: 26]
        // Note: On ne copie que la taille des instructions réelles, ou la taille réservée ?
        // Pour la simulation, copions le texte des instructions.
        CopierDonneesASM(prog->adresseMemoire, rawData.c_str(), rawData.length());

        std::cout << "[SUCCES] Programme charge a l'adresse : " << adresseAllouee << "\n";

        // Afficher les instructions comme demandé [cite: 30]
        std::cout << "Instructions chargees en memoire :\n";
        for (const auto& line : prog->instructions) std::cout << " > " << line << "\n";

    }
    else {
        // --- ECHEC (RAM PLEINE) ---
        std::cout << "[ALERTE] RAM Pleine ! Impossible d'allouer " << prog->tailleRequise << " octets.\n";
        std::cout << ">> Mecanisme de SWAPPING requis (Sera implémenté apres validation).\n";
        // C'est ici que nous appellerons la fonction de swapping à la prochaine étape
    }
    system("pause");
}

int main() {
    g_RamPtr = VirtualAlloc(NULL, TAILLE_RAM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_MvPtr = VirtualAlloc(NULL, TAILLE_MV, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    // Initialisation technique
    InitialiserMemoireASM(g_RamPtr);
    ChargerFichiersProgrammes();

    while (true) {
        AfficherEtatSysteme();
        std::cout << "\nMENU:\n";
        std::cout << "1-3. Lancer Programme (ID)\n";
        std::cout << "9. Quitter\n";
        std::cout << "Choix: ";

        int choix;
        std::cin >> choix;

        if (choix == 9) break;
        if (choix >= 1 && choix <= 3) {
            LancerProgramme(choix);
        }
    }

    VirtualFree(g_RamPtr, 0, MEM_RELEASE);
    VirtualFree(g_MvPtr, 0, MEM_RELEASE);
    return 0;
}