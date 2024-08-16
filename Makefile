CC = gcc
CFLAGS = -O3
LDFLAGS = -lX11 -lpng

TARGET = screenshotr

all: $(TARGET)

$(TARGET): main.c
	$(CC) -o $(TARGET) main.c $(CFLAGS) $(LDFLAGS)

install: $(TARGET)
	@echo "Installing $(TARGET) to /sbin/"
	@sudo mkdir -p /sbin
	@sudo mv $(TARGET) /sbin/

clean:
	@echo "Cleaning up..."
	@rm -f $(TARGET)

uninstall:
	@echo "Removing $(TARGET) from /sbin/"
	@sudo rm -f /sbin/$(TARGET)

help:
	@echo "Makefile options:"
	@echo "  all         - Build the program"
	@echo "  install     - Install the program to /sbin/"
	@echo "  clean       - Remove build artifacts"
	@echo "  uninstall   - Uninstall the program from /sbin/"
	@echo "  help        - Show this help message"

.PHONY: all install clean uninstall help
