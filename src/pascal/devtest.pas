program devtest;

function _cputws(Str: PWChar): Integer; cdecl; external;

procedure PascalToWide(var Wide: array of WChar; p: String);
var
	i: Integer;
begin
	for i := 1 to Integer(p[0]) do
		Wide[i - 1] := WChar(p[i]);
end;

var
	str: array[0..255] of WChar;
begin
	PascalToWide(str, 'Hello, world!'#13);
	_cputws(str);
end.