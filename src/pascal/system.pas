unit System;

interface

type
	shortint = -128..127;
	SmallInt = -32768..32767;
	Longint  = $80000000..$7fffffff; { $8000000 creates a longint overfow !! }
	byte     = 0..255;
	Word     = 0..65535;
	dword    = cardinal;
	longword = cardinal;
	Integer  = Longint;

	WChar = Word;
	PWChar = ^WChar;


implementation

procedure int_strcopy(len:longint;sstr,dstr:pointer);[public,alias:'FPC_SHORTSTR_COPY'];
{
  this procedure must save all modified registers except EDI and ESI !!!
}
begin
  asm
        pushl   %eax
        pushl   %ecx
        cld
        movl    dstr,%edi
        movl    sstr,%esi
        xorl    %eax,%eax
        movl    len,%ecx
        lodsb
        cmpl    %ecx,%eax
        jbe     .LStrCopy1
        movl    %ecx,%eax
.LStrCopy1:
        stosb
        cmpl    $7,%eax
        jl      .LStrCopy2
        movl    %edi,%ecx       { Align on 32bits }
        negl    %ecx
        andl    $3,%ecx
        subl    %ecx,%eax
        rep
        movsb
        movl    %eax,%ecx
        andl    $3,%eax
        shrl    $2,%ecx
        rep
        movsl
.LStrCopy2:
        movl    %eax,%ecx
        rep
        movsb
        popl    %ecx
        popl    %eax
  end ['ESI','EDI'];
end;

begin
end.