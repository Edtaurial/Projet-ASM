; =========================================================
; Module ASM - Noyau Final
; Intègre : Alloc, Swap, Fermeture (Step 5) et VirtualFree
; =========================================================

; --- Importation des APIs Windows ---
extern VirtualAlloc : proc
extern VirtualFree  : proc  ; <--- OBLIGATOIRE POUR LIBERER

.data
    align 8
    ptrRAM      QWORD 0         
    ptrMV       QWORD 0         
    
    offsetRAM   QWORD 0         
    offsetMV    QWORD 0         
    
    MAX_PROGS   EQU 10
    TablePCB    BYTE 320 DUP(0) 
    NbProgs     DWORD 0         
    
    ; --- PARAMETRES ---
    TAILLE_RAM_BYTES EQU 2000      ; Petite taille pour tester le Swap
    TAILLE_MV_BYTES  EQU 10485760  
    
    MEM_COMMIT_RESERVE EQU 3000h
    MEM_RELEASE        EQU 8000h   ; Code pour VirtualFree
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

; =========================================================
; InitNoyauASM : Allocation (Step 1)
; =========================================================
InitNoyauASM proc
    sub rsp, 28h

    ; Alloc RAM
    mov rcx, 0
    mov rdx, TAILLE_RAM_BYTES
    mov r8, MEM_COMMIT_RESERVE
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov [ptrRAM], rax

    ; Alloc MV
    mov rcx, 0
    mov rdx, TAILLE_MV_BYTES
    mov r8, MEM_COMMIT_RESERVE
    mov r9, PAGE_READWRITE
    call VirtualAlloc
    mov [ptrMV], rax

    mov [offsetRAM], 0
    mov [offsetMV], 0
    mov [NbProgs], 0

    lea rdi, TablePCB
    mov rcx, 40
    xor rax, rax
    rep stosq

    add rsp, 28h
    ret
InitNoyauASM endp

; =========================================================
; LibererMemoireASM : Le vrai VirtualFree (Step 5 / Fin)
; C'est ici qu'on rend physiquement la mémoire à Windows
; =========================================================
LibererMemoireASM proc
    sub rsp, 28h
    
    ; 1. Libération RAM
    mov rcx, [ptrRAM]
    test rcx, rcx
    jz FreeMV
    xor rdx, rdx            ; Taille = 0 (Car MEM_RELEASE libère tout le bloc)
    mov r8, MEM_RELEASE     ; Flag MEM_RELEASE
    call VirtualFree        ; <--- APPEL OFFICIEL

FreeMV:
    ; 2. Libération MV
    mov rcx, [ptrMV]
    test rcx, rcx
    jz FinLib
    xor rdx, rdx
    mov r8, MEM_RELEASE
    call VirtualFree        ; <--- APPEL OFFICIEL

FinLib:
    add rsp, 28h
    ret
LibererMemoireASM endp

; --- GESTION PCB ---
EnregistrerProcessusASM proc
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

; =========================================================
; FermerProcessusASM (Step 5)
; Marque l'espace comme libre (Mise à zéro)
; Note: On ne peut pas utiliser VirtualFree ici sur un sous-bloc
; sans détruire toute la RAM allouée en Step 1.
; =========================================================
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
    ; On efface les données (remplit de 0) pour simuler la libération
    mov rdi, [r10+16]
    mov ecx, [r10+4]
    test rdi, rdi
    jz Marquer
    xor eax, eax
    cld
    rep stosb

Marquer:
    mov dword ptr [r10+8], 0 ; ETAT = FERME
    mov qword ptr [r10+16], 0; ADRESSE = NULL
    
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

; --- SWAPPING ---
SwapperToutVersMV proc
    push rsi
    push rdi
    push rbx
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB 

BoucleSwap:
    cmp r8d, r9d
    jge FinSwap

    cmp dword ptr [r10+8], 1
    jne Prochain

    mov rsi, [r10+16]
    mov rdi, [ptrMV]
    add rdi, [offsetMV]
    mov ecx, [r10+4]
    cld
    rep movsb

    mov rax, [ptrMV]
    add rax, [offsetMV]
    mov [r10+16], rax
    mov dword ptr [r10+8], 2 

    mov eax, [r10+4]
    add [offsetMV], rax

Prochain:
    add r10, 32
    inc r8
    jmp BoucleSwap

FinSwap:
    mov [offsetRAM], 0
    pop rbx
    pop rdi
    pop rsi
    ret
SwapperToutVersMV endp

; --- LANCEMENT ---
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

    mov r11d, [r10+4]
    cmp r11d, TAILLE_RAM_BYTES
    ja TropGros

VerifierPlace:
    mov rax, [offsetRAM]
    mov rdx, rax
    add rdx, r11
    cmp rdx, TAILLE_RAM_BYTES
    ja PasDePlace

    mov rax, [ptrRAM]
    add rax, [offsetRAM]
    mov [r10+16], rax
    mov dword ptr [r10+8], 1
    add [offsetRAM], r11
    ret

PasDePlace:
    push r10
    push r11
    call SwapperToutVersMV
    pop r11
    pop r10
    jmp VerifierPlace

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

end