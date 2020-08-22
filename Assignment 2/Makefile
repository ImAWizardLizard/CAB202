# CAB202 Teensy Makefile
# Lawrence Buckingham, September 2017.
# Queensland University of Technology.

# Replace these targets with your target (hex file) name, including the .hex part.

TARGETS = a2_n10133810.hex


# Set the name of the folder containing libcab202_teensy.a

CAB202_TEENSY_FOLDER = cab202_teensy 
USB_SERIAL_FOLDER = usb_serial
ADC_FOLDER=cab202_adc
ADC_OBJ=$(ADC_FOLDER)/cab202_adc.o
USB_SERIAL_OBJ = usb_serial/usb_serial.o

# ---------------------------------------------------------------------------
#	Leave the rest of the file alone.
# ---------------------------------------------------------------------------

all: $(TARGETS)

TEENSY_LIBS =  $(USB_SERIAL_OBJ) $(ADC_OBJ) -lcab202_teensy  -lprintf_flt -lm  
TEENSY_DIRS =-I$(CAB202_TEENSY_FOLDER) -I$(USB_SERIAL_FOLDER) -I$(ADC_FOLDER) -L$(CAB202_TEENSY_FOLDER)
TEENSY_FLAGS = \
	-std=gnu99 \
	-mmcu=atmega32u4 \
	-DF_CPU=8000000UL \
	-funsigned-char \
	-funsigned-bitfields \
	-ffunction-sections \
	-fpack-struct \
	-fshort-enums \
	-Wall \
	-Werror \
	-Wl,-u,vfprintf \
	-Os 

clean:
	for f in $(TARGETS); do \
		if [ -f $$f ]; then rm $$f; fi; \
		if [ -f $$f.elf ]; then rm $$f.elf; fi; \
		if [ -f $$f.obj ]; then rm $$f.obj; fi; \
	done

rebuild: clean all

upload-arch:
	teensy-loader-cli -w -mmcu=atmega32u4 $(TARGETS)

upload-deb:
	teensy_loader_cli -w -mmcu=atmega32u4 $(TARGETS)

play: rebuild upload

%.hex : %.c
	avr-gcc $< $(TEENSY_FLAGS) $(TEENSY_DIRS) $(TEENSY_LIBS) -o $@.obj
	avr-objcopy -O ihex $@.obj $@
