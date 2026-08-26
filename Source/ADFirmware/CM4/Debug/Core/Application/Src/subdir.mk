################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Application/Src/main.c \
../Core/Application/Src/stm32h7xx_hal_msp.c \
../Core/Application/Src/stm32h7xx_it.c \
../Core/Application/Src/syscalls.c \
../Core/Application/Src/sysmem.c 

OBJS += \
./Core/Application/Src/main.o \
./Core/Application/Src/stm32h7xx_hal_msp.o \
./Core/Application/Src/stm32h7xx_it.o \
./Core/Application/Src/syscalls.o \
./Core/Application/Src/sysmem.o 

C_DEPS += \
./Core/Application/Src/main.d \
./Core/Application/Src/stm32h7xx_hal_msp.d \
./Core/Application/Src/stm32h7xx_it.d \
./Core/Application/Src/syscalls.d \
./Core/Application/Src/sysmem.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Application/Src/%.o Core/Application/Src/%.su Core/Application/Src/%.cyclo: ../Core/Application/Src/%.c Core/Application/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32H755xx -c -I"/Drivers/STM32H7xx_HAL_Driver/Inc" -I"/Drivers/CMSIS/Include" -I"/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy" -I"/Drivers/CMSIS/Device/ST/STM32H7xx/Include" -I../Core/Application/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Application-2f-Src

clean-Core-2f-Application-2f-Src:
	-$(RM) ./Core/Application/Src/main.cyclo ./Core/Application/Src/main.d ./Core/Application/Src/main.o ./Core/Application/Src/main.su ./Core/Application/Src/stm32h7xx_hal_msp.cyclo ./Core/Application/Src/stm32h7xx_hal_msp.d ./Core/Application/Src/stm32h7xx_hal_msp.o ./Core/Application/Src/stm32h7xx_hal_msp.su ./Core/Application/Src/stm32h7xx_it.cyclo ./Core/Application/Src/stm32h7xx_it.d ./Core/Application/Src/stm32h7xx_it.o ./Core/Application/Src/stm32h7xx_it.su ./Core/Application/Src/syscalls.cyclo ./Core/Application/Src/syscalls.d ./Core/Application/Src/syscalls.o ./Core/Application/Src/syscalls.su ./Core/Application/Src/sysmem.cyclo ./Core/Application/Src/sysmem.d ./Core/Application/Src/sysmem.o ./Core/Application/Src/sysmem.su

.PHONY: clean-Core-2f-Application-2f-Src

