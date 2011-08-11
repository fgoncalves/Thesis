################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include tests/subdir.mk
-include sax_parser/subdir.mk
-include localregistry/user/subdir.mk
-include localregistry/shell/subdir.mk
-include localregistry/daemon/subdir.mk
-include kernel/subdir.mk
-include kernel/interceptors/synchronization/subdir.mk
-include kernel/interceptors/reliable_transport/subdir.mk
-include kernel/interceptors/deaggregation/subdir.mk
-include kernel/interceptors/aggregation/subdir.mk
-include interceptor_shell/subdir.mk
-include handler/subdir.mk
-include datastructures/subdir.mk
-include configuration/user/subdir.mk
-include configuration/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: Platform

# Tool invocations
Platform: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	gcc  -o "Platform" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) Platform
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
