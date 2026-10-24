################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/max_tests.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/mean_tests.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/min_tests.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/power_tests.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/rms_tests.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_common_data.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_group.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/std_tests.c \
../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/var_tests.c 

OBJS += \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/max_tests.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/mean_tests.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/min_tests.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/power_tests.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/rms_tests.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_common_data.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_group.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/std_tests.o \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/var_tests.o 

C_DEPS += \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/max_tests.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/mean_tests.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/min_tests.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/power_tests.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/rms_tests.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_common_data.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_group.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/std_tests.d \
./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/var_tests.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/%.o Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/%.su Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/%.cyclo: ../Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/%.c Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_PWR_LDO_SUPPLY -DUSE_HAL_DRIVER -DSTM32H753xx -c -I../Core/Inc -I../LWIP/App -I../LWIP/Target -I../Middlewares/Third_Party/LwIP/src/include -I../Middlewares/Third_Party/LwIP/system -I../Drivers/STM32H7xx_HAL_Driver/Inc -I../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/BSP/Components/lan8742 -I../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../Middlewares/Third_Party/LwIP/src/include/lwip -I../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../Middlewares/Third_Party/LwIP/src/include/netif -I../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../Middlewares/Third_Party/LwIP/system/arch -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-CMSIS-2f-DSP-2f-DSP_Lib_TestSuite-2f-Common-2f-src-2f-statistics_tests

clean-Drivers-2f-CMSIS-2f-DSP-2f-DSP_Lib_TestSuite-2f-Common-2f-src-2f-statistics_tests:
	-$(RM) ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/max_tests.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/max_tests.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/max_tests.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/max_tests.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/mean_tests.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/mean_tests.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/mean_tests.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/mean_tests.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/min_tests.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/min_tests.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/min_tests.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/min_tests.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/power_tests.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/power_tests.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/power_tests.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/power_tests.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/rms_tests.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/rms_tests.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/rms_tests.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/rms_tests.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_common_data.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_common_data.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_common_data.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_common_data.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_group.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_group.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_group.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/statistics_test_group.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/std_tests.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/std_tests.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/std_tests.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/std_tests.su ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/var_tests.cyclo ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/var_tests.d ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/var_tests.o ./Drivers/CMSIS/DSP/DSP_Lib_TestSuite/Common/src/statistics_tests/var_tests.su

.PHONY: clean-Drivers-2f-CMSIS-2f-DSP-2f-DSP_Lib_TestSuite-2f-Common-2f-src-2f-statistics_tests

