; =========================================================
; Gestionnaire de mémoire simulant une RAM et une MV.
; Gère l'allocation contiguë et le swapping FIFO.
; =========================================================

extern VirtualAlloc : proc
extern VirtualFree  : proc

.data
    align 8
    ; --- Pointeurs vers la mémoire réelle allouée par Windows ---
    ptrRAM      QWORD 0          ; Adresse de début de notre bloc RAM simulé
    ptrMV       QWORD 0          ; Adresse de début de notre bloc MV simulé
    
    ; --- Curseurs (Offset) ---
    offsetRAM   QWORD 0          ; Position du prochain octet libre en RAM (Pile)
    offsetMV    QWORD 0          ; Position du prochain octet libre en MV
    
    ; --- Gestion des Processus (PCB) ---
    MAX_PROGS   EQU 10
    ; TablePCB : Tableau de 10 structures de 32 octets.
    ; Structure PCB (32 octets) :
    ;   [0-3]   ID (int)
    ;   [4-7]   Taille (int)
    ;   [8-11]  Etat (int : 0=Ferme, 1=RAM, 2=Swap)
    ;   [16-23] Adresse (void*)
    TablePCB    BYTE 320 DUP(0) 
    NbProgs     DWORD 0  
    ActiveProgID DWORD 0       
    
    ; --- PARAMETRES ---
    TAILLE_RAM_BYTES EQU 4194304      ; 4 Mo
    TAILLE_MV_BYTES  EQU 10485760     ; 10 Mo
    
    ; --- Constantes Windows API ---
    MEM_COMMIT_RESERVE EQU 3000h
    MEM_RELEASE        EQU 8000h
    PAGE_READWRITE     EQU 4

.code

; =========================================================
; SECTION : GETTERS / SETTERS
; Rôle : permettre au C++ de lire les variables ASM
; =========================================================
GetRamUsageASM proc
    mov rax, [offsetRAM]
    ret
GetRamUsageASM endp

GetRamTotalASM proc
    mov rax, TAILLE_RAM_BYTES
    ret
GetRamTotalASM endp

GetMvUsageASM proc
    mov rax, [offsetMV]
    ret
GetMvUsageASM endp

GetMvTotalASM proc
    mov rax, TAILLE_MV_BYTES
    ret
GetMvTotalASM endp

GetPtrRAM proc
    mov rax, [ptrRAM]
    ret
GetPtrRAM endp

GetPtrMV proc
    mov rax, [ptrMV]
    ret
GetPtrMV endp

GetActiveProgIDASM proc
    mov eax, [ActiveProgID]
    ret
GetActiveProgIDASM endp

SetActiveProgIDASM proc
    mov [ActiveProgID], ecx
    ret
SetActiveProgIDASM endp

; =========================================================
; SECTION : INITIALISATION & NETTOYAGE
; Rôle : Demande la mémoire réelle à l'OS (Windows) au démarrage
; =========================================================
InitNoyauASM proc
    sub rsp, 28h             ; Alignement de la pile (Shadow Space)

    ; 1. Allouer le bloc RAM (4 Mo)
    mov rcx, 0               
    mov rdx, TAILLE_RAM_BYTES
    mov r8, MEM_COMMIT_RESERVE
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov [ptrRAM], rax        ; Sauvegarde l'adresse retournée

    ; 2. Allouer le bloc MV (10 Mo)
    mov rcx, 0
    mov rdx, TAILLE_MV_BYTES
    mov r8, MEM_COMMIT_RESERVE
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov [ptrMV], rax

    ; 3. Initialiser les variables à 0
    mov [offsetRAM], 0
    mov [offsetMV], 0
    mov [NbProgs], 0
    mov [ActiveProgID], 0

    ; 4. Nettoyer la table PCB (mettre tout à 0)
    lea rdi, TablePCB
    mov rcx, 40              ; 40 qwords = 320 octets
    xor rax, rax
    rep stosq                ; Ecrit 0 partout
    
    add rsp, 28h
    ret
InitNoyauASM endp

LibererMemoireASM proc
    sub rsp, 28h
    ; Libère RAM
    mov rcx, [ptrRAM]
    test rcx, rcx
    jz FreeMV
    xor rdx, rdx             ; Taille 0 pour MEM_RELEASE
    mov r8, MEM_RELEASE
    call VirtualFree
FreeMV:
    ; Libère MV
    mov rcx, [ptrMV]
    test rcx, rcx
    jz FinLib
    xor rdx, rdx
    mov r8, MEM_RELEASE
    call VirtualFree
FinLib:
    add rsp, 28h
    ret
LibererMemoireASM endp

; =========================================================
; SECTION : GESTION PCB
; =========================================================
EnregistrerProcessusASM proc
    ; Entrée : ECX = ID, EDX = Taille en Ko
    
    ; 1. Conversion Ko -> Octets
    ; On décale de 10 bits à gauche (équivaut à multiplier par 1024)
    shl edx, 10

    ; 2. Vérifier si on a atteint la limite de processus
    mov r8d, [NbProgs]
    cmp r8d, MAX_PROGS
    jge FinEnreg

    ; 3. Calculer l'adresse dans le tableau PCB
    ; Adresse = TablePCB + (Index * 32)
    mov rax, r8
    shl rax, 5               ; x32 (taille d'une struct PCB)
    lea r9, TablePCB
    add r9, rax

    ; 4. Remplir le PCB
    mov [r9], ecx            ; ID
    mov [r9+4], edx          ; Taille (Octets)
    mov dword ptr [r9+8], 0  ; Etat (0 = Fermé)
    mov qword ptr [r9+16], 0 ; Adresse (NULL)

    inc dword ptr [NbProgs]
FinEnreg:
    ret
EnregistrerProcessusASM endp

GetProcessInfoASM proc
    ; Récupère les infos d'un PCB pour les donner au C++
    cmp ecx, [NbProgs]
    jge ErreurIndex

    mov rax, rcx
    shl rax, 5               ; Index * 32
    lea r10, TablePCB
    add r10, rax

    mov eax, [r10]
    mov [rdx], eax           ; Retourne ID
    mov eax, [r10+4]
    mov [r8], eax            ; Retourne Taille
    mov eax, [r10+8]
    mov [r9], eax            ; Retourne Etat
    
    mov r11, [rsp + 40]      ; 5ème argument (passed on stack)
    mov rax, [r10+16]
    mov [r11], rax           ; Retourne Adresse
    mov rax, 1
    ret
ErreurIndex:
    xor rax, rax
    ret
GetProcessInfoASM endp

GetNbProgsASM proc
    mov eax, [NbProgs]
    ret
GetNbProgsASM endp

FermerProcessusASM proc
    ; ... (Code de recherche du PCB standard) ...
    push rdi
    push rax
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB
BoucleRechFermer:
    cmp r8d, r9d
    jge IntrouvableFermer
    cmp [r10], ecx
    je TrouveFermer
    add r10, 32
    inc r8
    jmp BoucleRechFermer

TrouveFermer:
    ; Simulation de "Secure Wipe" : on remplit la mémoire de 0
    mov rdi, [r10+16]        ; Adresse mémoire du prog
    mov ecx, [r10+4]         ; Taille
    test rdi, rdi            ; Si adresse NULL, rien à faire
    jz Marquer
    xor eax, eax
    cld
    rep stosb                ; Remplit de zéros
Marquer:
    mov dword ptr [r10+8], 0 ; Etat = Fermé
    mov qword ptr [r10+16], 0; Adresse = NULL
    
    ; Si c'était le prog actif, on le désactive
    mov eax, [r10]
    cmp [ActiveProgID], eax
    jne PasActif
    mov [ActiveProgID], 0
PasActif:
    mov rax, 1
    pop rax
    pop rdi
    ret
IntrouvableFermer:
    xor rax, rax
    pop rax
    pop rdi
    ret
FermerProcessusASM endp

; =========================================================
; SECTION CRITIQUE : SWAP INTELLIGENT & COMPACTAGE
; Rôle : Libère de l'espace en enlevant le PREMIER programme (FIFO).
;        Ensuite, COMPACTE la RAM pour boucher le trou.
; =========================================================
EjecterPremierProgramme proc
    push rsi
    push rdi
    push rbx
    push r12
    push r13

    ; --- ETAPE 1 : IDENTIFIER LA VICTIME ---
    ; La victime est le programme situé à l'adresse ptrRAM (offset 0)
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB
    mov r12, [ptrRAM]        ; On cherche qui est ici (Début RAM)
    xor r13, r13             ; R13 stockera le PCB de la victime

ChercherVictime:
    cmp r8d, r9d
    jge FinRechercheVictime
    
    cmp dword ptr [r10+8], 1 ; Doit être EN RAM
    jne SuivantVictime
    
    cmp [r10+16], r12        ; Son adresse == Début RAM ?
    je TrouveVictime
    
SuivantVictime:
    add r10, 32
    inc r8
    jmp ChercherVictime

TrouveVictime:
    mov r13, r10             ; On a trouvé !

FinRechercheVictime:
    test r13, r13
    jz FinEjection           ; Sécurité : Si RAM vide, on sort

    ; --- ETAPE 2 : SAUVEGARDER LA VICTIME EN MV ---
    mov rsi, [r13+16]        ; Source : RAM
    mov rdi, [ptrMV]         ; Dest   : MV
    add rdi, [offsetMV]      ; Position libre en MV
    mov ecx, [r13+4]         ; Taille
    
    ; Copie (memcpy)
    push rcx                 ; Sauve taille
    cld
    rep movsb
    pop rcx                  ; Restaure taille

    ; Mise à jour du PCB Victime
    mov rax, [ptrMV]
    add rax, [offsetMV]
    mov [r13+16], rax        ; Nouvelle adresse (MV)
    mov dword ptr [r13+8], 2 ; Etat = SWAPPE (2)
    
    ; Mise à jour Offset MV
    mov eax, [r13+4]
    add [offsetMV], rax

    ; --- ETAPE 3 : COMPACTAGE (DEFRAGMENTATION) ---
    ; On a un trou au début de la RAM. Il faut décaler tout le reste vers le haut.
    
    mov r12d, [r13+4]        ; R12 = Taille du trou (Taille victime)
    
    ; Calculer la quantité de mémoire restante à déplacer
    ; Reste = OffsetActuel - TailleVictime
    mov rcx, [offsetRAM]
    sub rcx, r12
    
    cmp rcx, 0
    jle FinCompactage        ; Si la RAM est vide après lui, rien à bouger

    ; Déplacement (memmove)
    mov rdi, [ptrRAM]        ; Destination : Tout début de la RAM (on écrase la victime)
    mov rsi, [ptrRAM]
    add rsi, r12             ; Source : Ce qui venait juste après la victime
    
    cld
    rep movsb                ; On décale tout vers le bas (vers 0)

    ; --- ETAPE 4 : METTRE A JOUR LES ADRESSES DES AUTRES ---
    ; Puisqu'on a bougé les données physiques, les pointeurs dans TablePCB sont faux.
    ; Il faut soustraire "TailleVictime" à tous les programmes restants en RAM.
    
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB

BoucleUpdateAdresse:
    cmp r8d, r9d
    jge FinUpdateAdresses
    
    cmp dword ptr [r10+8], 1 ; Si EN RAM
    jne NextUpdate
    
    ; NouvelleAdresse = AncienneAdresse - TailleTrou
    mov rax, [r10+16]
    sub rax, r12
    mov [r10+16], rax        ; Sauvegarde nouvelle adresse

NextUpdate:
    add r10, 32
    inc r8
    jmp BoucleUpdateAdresse

FinUpdateAdresses:

FinCompactage:
    ; --- ETAPE 5 : METTRE A JOUR L'OFFSET GLOBAL ---
    ; La RAM est maintenant plus vide, on recule le curseur de fin.
    sub [offsetRAM], r12

FinEjection:
    pop r13
    pop r12
    pop rbx
    pop rdi
    pop rsi
    ret
EjecterPremierProgramme endp

; =========================================================
; SECTION : ALLOCATION ET LANCEMENT
; =========================================================
LancerProcessusASM proc
    ; ... (Recherche PCB standard) ...
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB

BoucleRech:
    cmp r8d, r9d
    jge Introuvable
    cmp [r10], ecx
    je Trouve
    add r10, 32
    inc r8
    jmp BoucleRech

Trouve:
    cmp dword ptr [r10+8], 1
    je DejaOuvert            ; Déjà lancé

    mov r11d, [r10+4]        ; Taille requise
    cmp r11d, TAILLE_RAM_BYTES
    ja TropGros              ; Prog > RAM Totale

VerifierPlaceLancer:
    ; Vérifie si (OffsetRAM + Taille) > RAM_MAX
    mov rax, [offsetRAM]
    mov rdx, rax
    add rdx, r11
    cmp rdx, TAILLE_RAM_BYTES
    ja PasDePlaceLancer

    ; --- SUCCES : On alloue à la fin (offsetRAM) ---
    mov rax, [ptrRAM]
    add rax, [offsetRAM]
    mov [r10+16], rax        ; Adresse physique
    mov dword ptr [r10+8], 1 ; Etat RAM
    add [offsetRAM], r11     ; Avance curseur
    mov [ActiveProgID], ecx  ; Définit comme actif
    ret

PasDePlaceLancer:
    ; --- ECHEC : RAM pleine ---
    ; On appelle notre routine de compactage pour libérer de la place
    push r10                 ; Sauve contexte
    push r11
    call EjecterPremierProgramme 
    pop r11
    pop r10
    jmp VerifierPlaceLancer  ; On réessaie (boucle tant que pas assez de place)

Introuvable:
    xor rax, rax
    ret
DejaOuvert:
    xor rax, rax
    ret
TropGros:
    xor rax, rax
    ret
LancerProcessusASM endp

; =========================================================
; SECTION : SWAP IN (RAMENER UN PROG)
; =========================================================
UtiliserProcessusASM proc
    ; ... (Recherche PCB standard) ...
    push rsi
    push rdi
    push rbx
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB
BoucleRechUtil:
    cmp r8d, r9d
    jge IntrouvableUtil
    cmp [r10], ecx
    je TrouveUtil
    add r10, 32
    inc r8
    jmp BoucleRechUtil

TrouveUtil:
    cmp dword ptr [r10+8], 1
    je EstDejaEnRam
    cmp dword ptr [r10+8], 0
    je EstFerme

    ; --- LE PROGRAMME EST EN SWAP (MV) ---
    mov r11d, [r10+4]        ; Taille à ramener

VerifierPlaceUtil:
    ; A-t-on la place en RAM pour le ramener ?
    mov rax, [offsetRAM]
    add rax, r11
    cmp rax, TAILLE_RAM_BYTES
    ja PasDePlaceUtil

    ; --- OUI : On copie MV -> RAM ---
    mov rsi, [r10+16]        ; Source : MV
    mov rdi, [ptrRAM]
    add rdi, [offsetRAM]     ; Dest   : Fin RAM actuelle
    mov ecx, [r10+4]
    cld
    rep movsb

    ; Mise à jour PCB
    mov rax, [ptrRAM]
    add rax, [offsetRAM]
    mov [r10+16], rax        ; Nouvelle adresse RAM
    mov dword ptr [r10+8], 1 ; Etat RAM
    
    ; Mise à jour Offset RAM
    mov eax, [r10+4]
    add [offsetRAM], rax
    
    ; Définit comme actif
    mov rax, [r10+16]
    mov [ActiveProgID], ecx
    jmp FinUtil

PasDePlaceUtil:
    ; --- NON : On doit faire de la place ---
    push r10
    push r11
    call EjecterPremierProgramme ; FIFO + Compactage
    pop r11
    pop r10
    jmp VerifierPlaceUtil    ; On réessaie

EstDejaEnRam:
    mov rax, [r10+16]
    mov [ActiveProgID], ecx
    jmp FinUtil
EstFerme:
    xor rax, rax
    jmp FinUtil
IntrouvableUtil:
    xor rax, rax
FinUtil:
    pop rbx
    pop rdi
    pop rsi
    ret
UtiliserProcessusASM endp

; =========================================================
; SECTION : SECURITE / VERIFICATION ACCES
; Rôle : Simule une "Segmentation Fault" si on accède hors limites
; =========================================================
VerifierAccesASM proc
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB
BoucleRechAcces:
    cmp r8d, r9d
    jge IntrouvableAcces
    cmp [r10], ecx
    je TrouveAcces
    add r10, 32
    inc r8
    jmp BoucleRechAcces
TrouveAcces:
    cmp dword ptr [r10+8], 1
    jne PasEnRam             ; Erreur si pas en RAM

    ; Vérif Borne Inférieure
    mov rax, [r10+16]
    cmp rdx, rax
    jb AccesRefuse           ; Adresse < Debut

    ; Vérif Borne Supérieure
    mov r11d, [r10+4]
    add rax, r11
    cmp rdx, rax
    jae AccesRefuse          ; Adresse >= Fin

    mov rax, 1               ; Accès OK
    ret
AccesRefuse:
    xor rax, rax             ; Erreur SegFault
    ret
PasEnRam:
    mov rax, -1
    ret
IntrouvableAcces:
    mov rax, -2
    ret
VerifierAccesASM endp

end