CDEBUGFLAGS = -g
CFLAGS = $(INCLUDES) $(DEFINES) $(CDEBUGFLAGS)
OBJS = simumalloc.o malloc.o amalloc.o
SRCS = simumalloc.c malloc.c amalloc.c
PROGS = simumalloc

all: $(PROGS)

simumalloc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

lint: $(SRCS)
	lint -u $(SRCS)

clean:
	rm -f $(PROGS) *.o \#* *~ core a.out *.BAK *.ckp
