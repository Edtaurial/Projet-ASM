.code

; ---------------------------------------------------------
; Procedure : InitialiserMemoireASM (Déjà existante)
; ---------------------------------------------------------
InitialiserMemoireASM proc
    test rcx, rcx
    jz FinInit
    mov dword ptr [rcx], 0AAAAh
FinInit:
    ret
InitialiserMemoireASM endp

; ---------------------------------------------------------
; Procedure : AllouerMemoireRAM
; Description : Vérifie l'espace et réserve un bloc contigu
; Arguments :
;   RCX = Adresse Base RAM (void*)
;   RDX = Pointeur vers Offset Libre (int*) -> Variable C++ qui suit l'utilisation
;   R8  = Taille Totale RAM (size_t)
;   R9  = Taille Requise (int)
; Retour (RAX) : Adresse du bloc alloué ou 0 si pas de place
; ---------------------------------------------------------
AllouerMemoireRAM proc
    ; Sauvegarde des registres non volatils si nécessaire (pas ici pour ce code simple)
    
    ; 1. Charger la valeur de l'offset actuel (déréférencement de RDX)
    mov r10, [rdx]          ; R10 = Offset actuel (ex: 0)
    
    ; 2. Calculer la fin théorique : Offset + TailleRequise
    mov r11, r10            ; R11 = Offset
    add r11, r9             ; R11 = Offset + TailleRequise

    ; 3. Vérifier si ça dépasse la taille totale (R8)
    cmp r11, r8
    ja PasAssezEspace       ; Si R11 > R8, saut vers erreur

    ; 4. Si OK : Calculer l'adresse absolue de début
    mov rax, rcx            ; RAX = Base RAM
    add rax, r10            ; RAX = Base + AncienOffset (Adresse de retour)

    ; 5. Mettre à jour l'offset global (Variable C++)
    mov [rdx], r11          ; *OffsetLibre = NouveauOffset

    ret

PasAssezEspace:
    xor rax, rax            ; Retourne 0 (NULL)
    ret
AllouerMemoireRAM endp

; ---------------------------------------------------------
; Procedure : CopierDonneesASM
; Description : Copie N octets de Source vers Destination (memcpy simplifié)
; Arguments :
;   RCX = Destination (RAM)
;   RDX = Source (Buffer C++)
;   R8  = Nombre d'octets
; ---------------------------------------------------------
CopierDonneesASM proc
    ; On utilise RSI et RDI pour la copie, on doit les sauvegarder
    push rsi
    push rdi

    mov rdi, rcx    ; Destination
    mov rsi, rdx    ; Source
    mov rcx, r8     ; Compteur (Nombre d'octets)

    ; Copie octet par octet (rep movsb)
    rep movsb

    pop rdi
    pop rsi
    ret
CopierDonneesASM endp

end