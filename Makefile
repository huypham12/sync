CC = gcc
CFLAGS = -Wall -Wextra -I./common/include -g
LDFLAGS = -lcrypto

# Danh sách các file source
SRCS = common/src/crypto.c common/src/hashmap.c tests/test_crypto.c
OBJS = $(SRCS:.c=.o)
TARGET = build/test_crypto

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) build/test_plain.txt build/test_enc.bin build/test_dec.txt dummy_key.bin
