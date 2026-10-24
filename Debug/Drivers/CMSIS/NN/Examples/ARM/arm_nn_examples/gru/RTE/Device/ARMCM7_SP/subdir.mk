################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/startup_ARMCM7.s 

C_SRCS += \
../Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/startup_ARMCM7.c \
../Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/system_ARMCM7.c 

OBJS += \
./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/startup_ARMCM7.o \
./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/system_ARMCM7.o 

S_DEPS += \
./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/startup_ARMCM7.d 

C_DEPS += \
./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/startup_ARMCM7.d \
./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/system_ARMCM7.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/%.o Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/%.su Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/%.cyclo: ../Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/%.c Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H753xx -c -I../Core/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/%.o: ../Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/%.s Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m7 -g3 -DDEBUG -c -I../Core/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Drivers-2f-CMSIS-2f-NN-2f-Examples-2f-ARM-2f-arm_nn_examples-2f-gru-2f-RTE-2f-Device-2f-ARMCM7_SP

clean-Drivers-2f-CMSIS-2f-NN-2f-Examples-2f-ARM-2f-arm_nn_examples-2f-gru-2f-RTE-2f-Device-2f-ARMCM7_SP:
	-$(RM) ./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/startup_ARMCM7.cyclo ./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/startup_ARMCM7.d ./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/startup_ARMCM7.o ./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/startup_ARMCM7.su ./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/system_ARMCM7.cyclo ./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/system_ARMCM7.d ./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/system_ARMCM7.o ./Drivers/CMSIS/NN/Examples/ARM/arm_nn_examples/gru/RTE/Device/ARMCM7_SP/system_ARMCM7.su

.PHONY: clean-Drivers-2f-CMSIS-2f-NN-2f-Examples-2f-ARM-2f-arm_nn_examples-2f-gru-2f-RTE-2f-Device-2f-ARMCM7_SP

