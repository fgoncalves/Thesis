################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../localregistry/daemon/communication.c \
../localregistry/daemon/exec.c \
../localregistry/daemon/main.c \
../localregistry/daemon/registry.c \
../localregistry/daemon/registry_error.c 

OBJS += \
./localregistry/daemon/communication.o \
./localregistry/daemon/exec.o \
./localregistry/daemon/main.o \
./localregistry/daemon/registry.o \
./localregistry/daemon/registry_error.o 

C_DEPS += \
./localregistry/daemon/communication.d \
./localregistry/daemon/exec.d \
./localregistry/daemon/main.d \
./localregistry/daemon/registry.d \
./localregistry/daemon/registry_error.d 


# Each subdirectory must supply rules for building sources it contributes
localregistry/daemon/%.o: ../localregistry/daemon/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -I/lib/modules/2.6.24.4/build/include -I/home/fred/tese/src/include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


