#include <stdint.h>
#include "jlm.h"

Client_Reg_Struc* jlm_BeginNestedExec() {
    Client_Reg_Struc *pcl = Get_Cur_VM_Handle()->CB_Client_Pointer;
    Begin_Nest_Exec(pcl);
    return pcl;
}

void jlm_EndNestedExec(Client_Reg_Struc* pcl) {
    End_Nest_Exec(pcl);
}

__declspec(naked) struct cb_s *Get_Cur_VM_Handle() {
  __asm {
    int 0x20
    dd 0x10001
    ret
  }
}

__declspec(naked) static void Begin_Nest_Exec(struct Client_Reg_Struc *pcl) {
  __asm {
    push ebp
    mov ebp, edi
    int 0x20
    dd 0x10017
    pop ebp
    ret
  }
}

__declspec(naked) void End_Nest_Exec(struct Client_Reg_Struc *pcl) {
  __asm {
    push ebp
    mov ebp, edi
    int 0x20
    dd 0x1001A
    pop ebp
    ret
  }
}

__declspec(naked) void Yield(struct Client_Reg_Struc *pcl) {
  __asm {
    push ebp
    mov ebp, edi
    int 0x20
    dd 0x1002B
    pop ebp
    ret
  }
}

__declspec(naked) void Exec_Int(struct Client_Reg_Struc *pcl, int intno) {
  __asm {
    push ebp
    mov ebp, edi
    int 0x20
    dd 0x10018
    pop ebp
    ret
  }
}

__declspec(naked) int Install_IO_Handler(int port, void (*handler)()) {
  __asm {
    int 0x20
    dd 0x1001F
    sbb eax, eax
    ret
  }
}

__declspec(naked) void Remove_IO_Handler(int port) {
  __asm {
    int 0x20
    dd 0x10022
    ret
  }
}

