/*****************************************************************************
	name:	memmove
	action:	moves Count bytes from address Src to address Dst
	returns:Dst
*****************************************************************************/
void *memmove(void *DstPtr, const void *SrcPtr, unsigned int Count)
{	void *RetVal=DstPtr;
	const char *Src=(const char *)SrcPtr;
	char *Dst=(char *)DstPtr;

	if(DstPtr < SrcPtr)	/* copy up */
	{	for(; Count != 0; Count--)
			*Dst++=*Src++; }
	else			/* copy down */
	{	Dst += (Count - 1);
		Src += (Count - 1);
		for(; Count != 0; Count--)
			*Dst--=*Src--; }
	return(RetVal); }
