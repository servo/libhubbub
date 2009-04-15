CC := gcc
LD := gcc

CFLAGS := `pkg-config --cflags libhubbub-0` `xml2-config --cflags`
LDFLAGS := `pkg-config --libs libhubbub-0` `xml2-config --libs`

SRC := libxml.c

libxml: $(SRC:.c=.o)
	@$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) libxml $(SRC:.c=.o)

%.o: %.c
	@$(CC) -c $(CFLAGS) -o $@ $<

