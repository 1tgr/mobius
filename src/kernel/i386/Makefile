TARGET=	libi386.lib
OBJS=	isr.o i386.o memory.o arch.o scdsptch.o gdb.o gdb-stub.o
DEFINES=-Di386 -DKERNEL

include ../../make.actions

$(TARGET):	$(OBJS)
	ar rcs $(TARGET) $(OBJS)

clean:
	rm $(OBJS:.o=.d)
	rm $(OBJS)
	rm $(TARGET)

include $(OBJS:.o=.d)
