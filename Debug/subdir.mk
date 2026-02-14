################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../conetree.cpp \
../tinyxml2.cpp 

CPP_DEPS += \
./conetree.d \
./tinyxml2.d 

OBJS += \
./conetree.o \
./tinyxml2.o 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean--2e-

clean--2e-:
	-$(RM) ./conetree.d ./conetree.o ./tinyxml2.d ./tinyxml2.o

.PHONY: clean--2e-

