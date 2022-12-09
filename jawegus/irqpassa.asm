; awegus - jemm edition - IRQ pass-up handler
; wbcbz7 o5.12.2o22

        section .text align 16
        use32
        
        ; we hack directly into Jemm's IDT, and for most cases, IRQ and exception vectors coincide
        ; we have to do the following:
        ; - we are running on ring0 stack, with r3->r0, V86->r0 and r0->r0 IRET frames possible
        ; - query PIC for "in service" IRQ mask, check if our IRQ needs attention
        ; - if not, restore stach frame and jump to old handler
        ; - otherwise:
        ;   if (interrupt occured in V86) 
        ;     format stack content as Client_Reg_Struc;
        ;   else (interrupt occured in PM ring3)
        ;     TODO (Jemm doesn't support PM clients for now, so ignore)
        ;   else // ring0->ring0 case, this should never happen but ignore
        ;     TODO 
        ;     
        ; - set flat DS/ES, jump to 32bit handler
        ; - restore stack and IRET back

struc   V86_stack_frame
    .EIP    resd 1
    .CS     resd 1
    .EFL    resd 1
    .ESP    resd 1      ; present if r3->r0 or v86->r0 switch
    .SS     resd 1      ; --//--
    .ES     resd 1      ; present if v86->r0
    .DS     resd 1      ; --//--
    .FS     resd 1      ; --//--
    .GS     resd 1      ; --//--
endstruc

struc Client_Reg_Struc
    .EDI    resd 1
    .ESI    resd 1
    .EBP    resd 1
    .n_ESP  resd 1
    .EBX    resd 1
    .EDX    resd 1
    .ECX    resd 1
    .EAX    resd 1
    .IntNo  resd 1
    .Error  resd 1
    .EIP    resd 1
    .CS     resd 1
    .EFL    resd 1
    .ESP    resd 1
    .SS     resd 1
    .ES     resd 1
    .DS     resd 1
    .FS     resd 1
    .GS     resd 1
endstruc

        
gusemu_irq_passup_:
        ; alright
        ; save EAX for start, hoping we have at least 4 bytes valid stack
        push        eax

        ; test PIC
        mov         al, 0x0B
        out         0x20, al
.pic_base       equ $-1             ; used for primary/secondary PIC select
        in          al, 0x20
.pic_base2      equ $-1
        test        al, 0
.irq_mask       equ $-1
        jz          .not_an_irq     ; alright, not an IRQ

        ; not an exception. assume stack is valid and large enough to hold Client_Reg_Struc
        ; store (null) error code and interrupt number
        xor         eax, eax
        xchg        eax, [esp]
        push        dword 0x12345678
.intr_num       equ $-4
        pushad
        mov         ebp, esp

        ; restore segment registers. since SS is flat 4gb 32bit, we are ok with that
        mov         eax, ss
        push        es
        push        ds
        mov         ds, eax
        mov         es, eax
        
        ; call our flat handler. make sure it sends EOI!
        mov         ebx, ebp
        ;call        near [_gusemu_irq_passup_handler]    ; EBX = EBP = Client_Reg_Struc
        call        near $+0x12345678
.passup_handler equ $-4

        ; pop stack frame and return
        pop         ds
        pop         es
        popad
        add         esp, 8   ; strip interrupt number and error code
        iretd

        ; not an IRQ
        ; restore valid stack frame and jump to old handler
.not_an_irq:
        pop         eax
        jmp         dword 0x0000:0x00000000
.oldhandler     equ $-6

;-------------
; exports
;-------------
global gusemu_irq_passup_
global _gusemu_irq_passup_pic_base
global _gusemu_irq_passup_pic_base2
global _gusemu_irq_passup_irq_mask
global _gusemu_irq_passup_intr_num
global _gusemu_irq_passup_oldhandler
global _gusemu_irq_passup_handler

_gusemu_irq_passup_pic_base     equ gusemu_irq_passup_.pic_base
_gusemu_irq_passup_pic_base2    equ gusemu_irq_passup_.pic_base2
_gusemu_irq_passup_irq_mask     equ gusemu_irq_passup_.irq_mask
_gusemu_irq_passup_intr_num     equ gusemu_irq_passup_.intr_num
_gusemu_irq_passup_oldhandler   equ gusemu_irq_passup_.oldhandler
_gusemu_irq_passup_handler      equ gusemu_irq_passup_.passup_handler

