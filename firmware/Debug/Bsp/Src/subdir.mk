################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Bsp/Src/led.c \
../Bsp/Src/led7.c \
../Bsp/Src/tm1637.c 

OBJS += \
./Bsp/Src/led.o \
./Bsp/Src/led7.o \
./Bsp/Src/tm1637.o 

C_DEPS += \
./Bsp/Src/led.d \
./Bsp/Src/led7.d \
./Bsp/Src/tm1637.d 


# Each subdirectory must supply rules for building sources it contributes
Bsp/Src/%.o Bsp/Src/%.su Bsp/Src/%.cyclo: ../Bsp/Src/%.c Bsp/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0 -std=gnu11 -g3 -DDEBUG -DUSE_FULL_LL_DRIVER -DSTM32F030x6 -DUSE_HAL_DRIVER -c -I../Core/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc -I../Drivers/STM32F0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F0xx/Include -I../Drivers/CMSIS/Include -I"E:/Project/BadmintonBoard/firmware/Bsp/Inc" -I"E:/Project/BadmintonBoard/firmware/App/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -ftls-model=local-exec -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Bsp-2f-Src

clean-Bsp-2f-Src:
	-$(RM) ./Bsp/Src/led.cyclo ./Bsp/Src/led.d ./Bsp/Src/led.o ./Bsp/Src/led.su ./Bsp/Src/led7.cyclo ./Bsp/Src/led7.d ./Bsp/Src/led7.o ./Bsp/Src/led7.su ./Bsp/Src/tm1637.cyclo ./Bsp/Src/tm1637.d ./Bsp/Src/tm1637.o ./Bsp/Src/tm1637.su

.PHONY: clean-Bsp-2f-Src

