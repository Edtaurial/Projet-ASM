; =========================================================
; Module ASM - Noyau Final (Correction Bug R10 + Infos MV)
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
    
    ; --- PARAMETRES DE TEST ---
    TAILLE_RAM_BYTES EQU 2000      ; 2000 octets (Test Swap)
    TAILLE_MV_BYTES  EQU 10485760  ; 10 Mo
    
    MEM_COMMIT_RESERVE EQU 3000h
    MEM_RELEASE        EQU 8000h
    PAGE_READWRITE     EQU 4

.code

; --- GETTERS POUR C++ ---
GetRamUsageASM proc
    mov rax, [offsetRAM]
    ret
GetRamUsageASM endp

GetRamTotalASM proc
    mov rax, TAILLE_RAM_BYTES
    ret
GetRamTotalASM endp

GetMvUsageASM proc            ; <--- NOUVEAU
    mov rax, [offsetMV]
    ret
GetMvUsageASM endp

GetMvTotalASM proc            ; <--- NOUVEAU
    mov rax, TAILLE_MV_BYTES
    ret
GetMvTotalASM endp

; --- INITIALISATION ---
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

; --- LIBERATION ---
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

; --- SWAPPING ---
SwapperToutVersMV proc
    push rsi
    push rdi
    push rbx
    ; Note: R10 est utilisé ici comme curseur
    xor r8, r8
    mov r9d, [NbProgs]
    lea r10, TablePCB 

BoucleSwap:
    cmp r8d, r9d
    jge FinSwap

    cmp dword ptr [r10+8], 1 ; Si EN RAM
    jne Prochain

    ; Copie
    mov rsi, [r10+16]
    mov rdi, [ptrMV]
    add rdi, [offsetMV]
    mov ecx, [r10+4]
    cld
    rep movsb

    ; Update PCB
    mov rax, [ptrMV]
    add rax, [offsetMV]
    mov [r10+16], rax
    mov dword ptr [r10+8], 2 ; ETAT SWAPPE

    ; Update Offset MV
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

; --- LANCEMENT (CORRIGÉ) ---
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

    mov r11d, [r10+4] ; R11 = Taille requise

    ; Sécurité taille > RAM totale
    cmp r11d, TAILLE_RAM_BYTES
    ja TropGros

VerifierPlace:
    mov rax, [offsetRAM]
    mov rdx, rax
    add rdx, r11
    
    cmp rdx, TAILLE_RAM_BYTES
    ja PasDePlace

    ; --- ALLOCATION OK ---
    ; R10 pointe bien vers le bon PCB ici
    mov rax, [ptrRAM]
    add rax, [offsetRAM]
    mov [r10+16], rax        ; Adresse
    mov dword ptr [r10+8], 1 ; Etat = 1 (EN RAM)
    add [offsetRAM], r11
    ret

PasDePlace:
    ; === CORRECTION DU BUG ===
    ; On sauvegarde R10 (Pointeur PCB) et R11 (Taille)
    ; car SwapperToutVersMV va modifier R10 !
    push r10
    push r11
    
    call SwapperToutVersMV
    
    pop r11
    pop r10
    ; R10 pointe à nouveau correctement vers notre programme
    ; =========================
    
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