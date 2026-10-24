################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/TransformFunctions.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/bitreversal.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/cfft.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/dct4.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/rfft.c 

OBJS += \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/TransformFunctions.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/bitreversal.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/cfft.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/dct4.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/rfft.o 

C_DEPS += \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/TransformFunctions.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/bitreversal.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/cfft.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/dct4.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/rfft.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/%.o Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/%.su Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/%.cyclo: ../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/%.c Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H753xx -c -I../Core/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-CMSIS-2f-DSP-2f-DSP_Lib_TestSuite-2f-RefLibs-2f-src-2f-TransformFunctions

clean-Drivers-2f-CMSIS-2f-DSP-2f-DSP_Lib_TestSuite-2f-RefLibs-2f-src-2f-TransformFunctions:
	-$(RM) ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/TransformFunctions.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/TransformFunctions.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/TransformFunctions.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/TransformFunctions.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/bitreversal.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/bitreversal.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/bitreversal.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/bitreversal.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/cfft.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/cfft.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/cfft.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/cfft.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/dct4.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/dct4.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/dct4.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/dct4.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/rfft.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/rfft.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/rfft.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/RefLibs/src/TransformFunctions/rfft.su

.PHONY: clean-Drivers-2f-CMSIS-2f-DSP-2f-DSP_Lib_TestSuite-2f-RefLibs-2f-src-2f-TransformFunctions

