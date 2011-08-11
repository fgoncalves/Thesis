################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../handler/handler.c \
../handler/message_buffer.c \
../handler/pdu_transport_fcntl.c \
../handler/reliable_transport.c \
../handler/sockiface.c \
../handler/timestamp.c \
../handler/unreliable_transport.c 

OBJS += \
./handler/handler.o \
./handler/message_buffer.o \
./handler/pdu_transport_fcntl.o \
./handler/reliable_transport.o \
./handler/sockiface.o \
./handler/timestamp.o \
./handler/unreliable_transport.o 

C_DEPS += \
./handler/handler.d \
./handler/message_buffer.d \
./handler/pdu_transport_fcntl.d \
./handler/reliable_transport.d \
./handler/sockiface.d \
./handler/timestamp.d \
./handler/unreliable_transport.d 


# Each subdirectory must supply rules for building sources it contributes
handler/%.o: ../handler/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


