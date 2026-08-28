################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../main.c \
../scenario_parser.c \
../sim_core.c \
../toml.c \
../track_engine.c 

OBJS += \
./main.o \
./scenario_parser.o \
./sim_core.o \
./toml.o \
./track_engine.o 

C_DEPS += \
./main.d \
./scenario_parser.d \
./sim_core.d \
./toml.d \
./track_engine.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: Cygwin C Compiler'
	gcc -std=c99 -DTESTS_BUILD -O0 -g -ftest-coverage -fprofile-arcs -Wall -c -fmessage-length=0 -fstack-protector-all -Wformat=2 -Wformat-security -Wstrict-overflow -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


