################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../sax_parser/attr_list.c \
../sax_parser/string_buffer.c \
../sax_parser/xml_sax_parser.c \
../sax_parser/y.tab.c 

OBJS += \
./sax_parser/attr_list.o \
./sax_parser/string_buffer.o \
./sax_parser/xml_sax_parser.o \
./sax_parser/y.tab.o 

C_DEPS += \
./sax_parser/attr_list.d \
./sax_parser/string_buffer.d \
./sax_parser/xml_sax_parser.d \
./sax_parser/y.tab.d 


# Each subdirectory must supply rules for building sources it contributes
sax_parser/%.o: ../sax_parser/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


