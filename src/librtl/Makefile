TARGET=	$(LIB)/librtl_s.lib
OBJS=	fullpath.o

include ../make.actions

$(TARGET):	$(OBJS)
	ar rcs $(TARGET) $(OBJS)

include $(OBJS:.o=.d)
