

obj-m		+= ktapvm.o
ktapvm-y	:= ktap.o loader.o object.o baselib.o oslib.o tstring.o table.o vm.o \
			syscalls.o trace.o opcode.o strfmt.o

all:
	make -C ../../.. M=`pwd` modules

UDIR = userspace
ktap: $(UDIR)/main.c $(UDIR)/lex.c $(UDIR)/parser.c $(UDIR)/code.c $(UDIR)/dump.c $(UDIR)/util.c \
	opcode.c table.c tstring.c object.c

	gcc -g -O2 -o ktap $(UDIR)/lex.c $(UDIR)/parser.c $(UDIR)/code.c $(UDIR)/dump.c $(UDIR)/main.c $(UDIR)/util.c  \
	opcode.c table.c tstring.c object.c

clean:
	rm -rf ktapvm ktap *.o *.out *.ko *.mod.c *.order *.o.cmd Module.symvers

