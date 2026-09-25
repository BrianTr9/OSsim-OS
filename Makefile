
INC = -Iinclude
LIB = -lpthread

SRC = src
OBJ = obj
INCLUDE = include

CC = gcc
DEBUG = -g
CFLAGS = -Wall -c $(DEBUG)
LFLAGS = -Wall $(DEBUG)

vpath %.c $(SRC)
vpath %.h $(INCLUDE)

COMPILE = $(CC) $(INC)

# Object files needed by modules
MEM_OBJ = $(addprefix $(OBJ)/, paging.o mem.o cpu.o loader.o)
SYSCALL_OBJ = $(addprefix $(OBJ)/, syscall.o sys_killall.o sys_mem.o sys_listsyscall.o)
OS_OBJ = $(addprefix $(OBJ)/, cpu.o mem.o loader.o queue.o os.o sched.o timer.o mm-vm.o mm.o mm-memphy.o libstd.o libmem.o)
OS_OBJ += $(SYSCALL_OBJ)
SCHED_OBJ = $(addprefix $(OBJ)/, cpu.o loader.o)
HEADER = $(wildcard $(INCLUDE)/*.h)
 
all: os
#mem sched os

# Just compile memory management modules
mem: $(MEM_OBJ)
	$(COMPILE) $(LFLAGS) $(MEM_OBJ) -o mem $(LIB)

# Just compile scheduler
sched: $(SCHED_OBJ)
	$(COMPILE) $(LFLAGS) $(MEM_OBJ) -o sched $(LIB)

# Compile syscall
syscalltbl.lst: $(SRC)/syscall.tbl
	@echo $(OS_OBJ)
	chmod +x $(SRC)/syscalltbl.sh
	$(SRC)/syscalltbl.sh $< $(SRC)/$@ 
#	mv $(OBJ)/syscalltbl.lst $(INCLUDE)/

# Compile the whole OS simulation
os: $(OBJ) syscalltbl.lst $(OS_OBJ)
	$(COMPILE) $(LFLAGS) $(OS_OBJ) -o os $(LIB)

$(OBJ)/%.o: %.c ${HEADER} $(OBJ)
	$(COMPILE) $(CFLAGS) $< -o $@

# Prepare objectives container
$(OBJ):
	mkdir -p $(OBJ)

# Run the smoke tests (tests/run_tests.sh)
test: os
	tests/run_tests.sh

# Rebuild with AddressSanitizer + UndefinedBehaviorSanitizer and run the tests
asan: clean
	$(MAKE) os DEBUG="-g -fsanitize=address,undefined -fno-omit-frame-pointer"
	tests/run_tests.sh

# Rebuild with ThreadSanitizer and run the tests
tsan: clean
	$(MAKE) os DEBUG="-g -fsanitize=thread"
	TIMEOUT=180 tests/run_tests.sh

.PHONY: all test asan tsan clean

clean:
	rm -f $(SRC)/*.lst
	rm -f $(OBJ)/*.o os sched mem
	rm -rf $(OBJ)
