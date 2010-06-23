TARGETS = rsd-simple-start

RSD_SIMPLE_START_TEST_OBJ = rsd-simple-start.o
LIBS = -lrsound

all: $(TARGETS)

rsd-simple-start : $(RSD_SIMPLE_START_TEST_OBJ)
	@$(CC) -o $@ $< -lm -lrsound
	@echo "LD $@"

%.o : %.c
	@$(CC) -c -o $@ $< $(CFLAGS)
	@echo "CC $<"

clean:
	rm -rf $(TARGETS)
	rm -rf *.o
