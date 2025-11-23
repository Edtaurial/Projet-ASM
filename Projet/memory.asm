; =========================================================
; Module ASM - Noyau Final (Swapping Intelligent + Compactage)
; =========================================================

extern VirtualAlloc : proc
extern VirtualFree  : proc

.data
    align 8
    ptrRAM      QWORD 0         
    ptrMV       QWORD 0         
    offsetRAM   QWORD 0         
    offsetMV    QWORD 0         
    
    MAX_PROGS   EQU 10
    TablePCB    BYTE 320 DUP(0) 
    NbProgs     DWORD 0  
    ActiveProgID DWORD 0       
    
    ; --- PARAMETRES DE TEST ---
    TAILLE_RAM_BYTES EQU 4194304      
    TAILLE_MV_BYTES  EQU 10485760  
    
    MEM_COMMIT_RESERVE EQU 3000h
    MEM_RELEASE        EQU 8000h
    PAGE_READWRITE     EQU 4

.code

; --- GETTERS ---
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

; --- INIT & LIBERATION ---
InitNoyauASM proc
    sub rsp, 28h
    mov rcx, 0
    mov rdx, TAILLE_RAM_BYTES
    mov r8, MEM_COMMIT_RESERVE
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov [ptrRAM], rax

    mov rcx, 0
    mov rdx, TAILLE_MV_BYTES
    mov r8, MEM_COMMIT_RESERVE
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov [ptrMV], rax

    mov [offsetRAM], 0
    mov [offsetMV], 0
    mov [NbProgs], 0
    mov [ActiveProgID], 0

    lea rdi, TablePCB
    mov rcx, 40
    xor rax, rax
    rep stosq
    add rsp, 28h
    ret
InitNoyauASM endp

LibererMemoireASM proc
    sub rsp, 28h
    mov rcx, [ptrRAM]
    test rcx, rcx
    jz FreeMV
    xor rdx, rdx
    mov r8, MEM_RELEASE
    call VirtualFree
FreeMV:
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

; --- GESTION PCB ---
EnregistrerProcessusASM proc
    shl edx, 10
    mov r8d, [NbProgs]
    cmp r8d, MAX_PROGS
    jge FinEnreg
    mov rax, r8
    shl rax, 5
    lea r9, TablePCB
    add r9, rax
    mov [r9], ecx
    mov [r9+4], edx
    mov dword ptr [r9+8], 0
    mov qword ptr [r9+16], 0
    inc dword ptr [NbProgs]
FinEnreg:
    ret
EnregistrerProcessusASM endp

GetProcessInfoASM proc
    cmp ecx, [NbProgs]
    jge ErreurIndex
    mov rax, rcx
    shl rax, 5
    lea r10, TablePCB
    add r10, rax
    mov eax, [r10]
    mov [rdx], eax
    mov eax, [r10+4]
    mov [r8], eax
    mov eax, [r10+8]
    mov [r9], eax
    mov r11, [rsp + 40] 
    mov rax, [r10+16]
    mov [r11], rax 
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
    mov rdi, [r10+16]
    mov ecx, [r10+4]
    test rdi, rdi
    jz Marquer
    xor eax, eax
    cld
    rep stosb
Marquer:
    mov dword ptr [r10+8], 0
    mov qword ptr [r10+16], 0
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
; EjecterPremierProgramme (NOUVEAU - SWAP INTELLIGENT)
; Rôle : Trouve le programme situé tout au début de la RAM,
;        le déplace en MV, et DÉCALE (Compacte) le reste.
; =========================================================
EjecterPremierProgramme proc
    push rsi
    push rdi
    push rbx
    push r12
    push r13

    ; 1. Trouver la "Victime" : C'est celui dont Adresse == ptrRAM
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB
    mov r12, [ptrRAM]       ; Adresse cible (Début RAM)
    xor r13, r13            ; Pointeur vers PCB victime (0 = pas trouvé)

ChercherVictime:
    cmp r8d, r9d
    jge FinRechercheVictime
    
    cmp dword ptr [r10+8], 1 ; Est-il en RAM ?
    jne SuivantVictime
    
    cmp [r10+16], r12       ; Est-il au tout début (offset 0) ?
    je TrouveVictime
    
SuivantVictime:
    add r10, 32
    inc r8
    jmp ChercherVictime

TrouveVictime:
    mov r13, r10            ; R13 contient le PCB du programme à éjecter
    
FinRechercheVictime:
    test r13, r13
    jz FinEjection          ; Si pas trouvé (RAM vide ou bug), on sort

    ; --- 2. COPIER VICTIME VERS MV ---
    mov rsi, [r13+16]       ; Source (RAM)
    mov rdi, [ptrMV]        ; Dest (MV)
    add rdi, [offsetMV]     ; Offset MV
    mov ecx, [r13+4]        ; Taille
    
    ; Copie RAM -> MV
    push rcx                ; Sauve taille pour plus tard
    cld
    rep movsb
    pop rcx                 ; Récupère taille (dans RCX)

    ; Mise à jour PCB Victime
    mov rax, [ptrMV]
    add rax, [offsetMV]
    mov [r13+16], rax       ; Nouvelle adresse (MV)
    mov dword ptr [r13+8], 2 ; ETAT = SWAPPE
    
    ; Si c'était l'actif, reset
    mov eax, [r13]
    cmp [ActiveProgID], eax
    jne UpdateMV
    mov [ActiveProgID], 0

UpdateMV:
    mov eax, [r13+4]
    add [offsetMV], rax     ; Avance Offset MV

    ; --- 3. COMPACTAGE DE LA RAM (SHIFT) ---
    ; On doit décaler tout ce qui était APRES la victime vers le HAUT (vers 0)
    ; Taille du trou = Taille Victime (dans [r13+4])
    mov r12d, [r13+4]       ; R12 = Taille du trou (Shift Amount)
    
    ; Calculer combien d'octets il faut bouger
    ; BytesToMove = offsetRAM - TailleVictime
    mov rcx, [offsetRAM]
    sub rcx, r12
    
    cmp rcx, 0
    jle FinCompactage       ; Rien à bouger (la RAM est vide après ce prog)

    ; Déplacement Mémoire (memmove)
    ; Destination = ptrRAM
    mov rdi, [ptrRAM]
    ; Source = ptrRAM + TailleVictime
    mov rsi, [ptrRAM]
    add rsi, r12
    
    cld
    rep movsb               ; Décale tout vers le bas

    ; --- 4. MISE A JOUR DES AUTRES PCB ---
    ; Tous les programmes qui sont EN RAM doivent voir leur adresse reculer de R12
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB

BoucleUpdateAdresse:
    cmp r8d, r9d
    jge FinUpdateAdresses
    
    cmp dword ptr [r10+8], 1 ; Si EN RAM
    jne NextUpdate
    
    ; Adresse = Adresse - TailleTrou
    mov rax, [r10+16]
    sub rax, r12
    mov [r10+16], rax

NextUpdate:
    add r10, 32
    inc r8
    jmp BoucleUpdateAdresse

FinUpdateAdresses:

FinCompactage:
    ; --- 5. REDUIRE OFFSET RAM ---
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
; LancerProcessusASM
; =========================================================
LancerProcessusASM proc
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
    je DejaOuvert

    mov r11d, [r10+4]           ; Taille demandée
    cmp r11d, TAILLE_RAM_BYTES
    ja TropGros

VerifierPlaceLancer:
    mov rax, [offsetRAM]
    mov rdx, rax
    add rdx, r11
    cmp rdx, TAILLE_RAM_BYTES
    ja PasDePlaceLancer         ; -> EJECTION INTELLIGENTE

    ; Place OK
    mov rax, [ptrRAM]
    add rax, [offsetRAM]
    mov [r10+16], rax
    mov dword ptr [r10+8], 1
    add [offsetRAM], r11
    mov [ActiveProgID], ecx
    ret

PasDePlaceLancer:
    ; Au lieu de tout vider, on éjecte juste le premier (FIFO)
    push r10
    push r11
    call EjecterPremierProgramme
    pop r11
    pop r10
    jmp VerifierPlaceLancer     ; On réessaie (peut-être qu'il faut en éjecter un 2ème ?)

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
; UtiliserProcessusASM
; =========================================================
UtiliserProcessusASM proc
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

    ; Swap In requis
    mov r11d, [r10+4]       ; Taille prog à ramener

VerifierPlaceUtil:
    mov rax, [offsetRAM]
    add rax, r11
    cmp rax, TAILLE_RAM_BYTES
    ja PasDePlaceUtil       ; -> EJECTION INTELLIGENTE

    ; Copie MV -> RAM
    mov rsi, [r10+16]       
    mov rdi, [ptrRAM]
    add rdi, [offsetRAM]
    mov ecx, [r10+4]
    cld
    rep movsb

    mov rax, [ptrRAM]
    add rax, [offsetRAM]
    mov [r10+16], rax
    mov dword ptr [r10+8], 1
    mov eax, [r10+4]
    add [offsetRAM], rax
    mov rax, [r10+16]
    mov [ActiveProgID], ecx
    jmp FinUtil

PasDePlaceUtil:
    push r10
    push r11
    call EjecterPremierProgramme ; Libère juste ce qu'il faut
    pop r11
    pop r10
    jmp VerifierPlaceUtil

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
    jne PasEnRam
    mov rax, [r10+16]
    cmp rdx, rax
    jb AccesRefuse
    mov r11d, [r10+4]
    add rax, r11
    cmp rdx, rax
    jae AccesRefuse
    mov rax, 1
    ret
AccesRefuse:
    xor rax, rax
    ret
PasEnRam:
    mov rax, -1
    ret
IntrouvableAcces:
    mov rax, -2
    ret
VerifierAccesASM endp

end