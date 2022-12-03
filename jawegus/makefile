TARGET   = jawegus

INCLUDE  = ..

AS       = nasm.exe
CC       = wcc386.exe
CP       = wpp386.exe
CP16     = wpp.exe
LD       = wlink.exe
CFLAGS   = -3r -zp16 -zl -zls -mf -onhasbmir -s -I=$(INCLUDE)
CFLAGS16 = -ms -s -I=$(INCLUDE)
LFLAGS   =
AFLAGS   = -f win32 -l $<.lst

OBJS     = $(TARGET).obj console.obj cmdline.obj jlm.obj utils.obj iotrap.obj gusemu.obj emu8000.obj
OBJSTR   = file {$(OBJS)}

STUBOBJS = stub16.obj

LIBS     = 
LIBSTR   = library {$(LIBS)}

all: stub16.exe $(TARGET).exe .symbolic

stub16.exe: $(STUBOBJS) .symbolic
	%write stub16.lnk name stub16
	%write stub16.lnk file {$(STUBOBJS)}
	%write stub16.lnk system dos
	$(LD) @stub16.lnk $(LFLAGS)
	del stub16.lnk

$(TARGET).exe : $(OBJS) .symbolic
	%write $(TARGET).lnk name $(TARGET).exe
	%write $(TARGET).lnk system nt_dll
	%write $(TARGET).lnk runtime console
	%write $(TARGET).lnk option map=$(TARGET).map
	%write $(TARGET).lnk option eliminate
	%write $(TARGET).lnk option start='_DllMain@12'
#	%write $(TARGET).lnk option stub=stub16.exe
	%write $(TARGET).lnk export _ddb.1
	%write $(TARGET).lnk $(OBJSTR) 
	$(LD) @$(TARGET).lnk $(LFLAGS)
	del $(TARGET).lnk
	python patchpe.py $(TARGET).exe

stub16.obj: stub16.cpp
	$(CP16) $< $(CFLAGS16)

.c.obj:
	$(CC) $< $(CFLAGS)

.cpp.obj:
	$(CP) $< $(CFLAGS)

.asm.obj:
	$(AS) $< $(AFLAGS)
	
# clean all
clean: .symbolic
	del $(TARGET).dll
	del *.map
	del *.obj
	del *.err