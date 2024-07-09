/*
 * Copyright (c) 2020 Nanjing Xiaoxiongpai Intelligent Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "iot_adc.h"
#include "iot_errno.h"
#include "iot_gpio_ex.h"
#include "ohos_init.h"
#include "iot_gpio.h"
#include"iot_gpio_ex.h"

#define heart_TASK_STACK_SIZE (1024 * 8)
#define heart_TASK_PRIO 24
#define heart_GPIO 11
#define heart_CHANNEL 5
#define heart_TASK_DELAY_1S 1000000
#define heart_VREF_VOL 1.8
#define heart_COEFFICIENT 4
#define heart_RATIO 4096
#define Size 66
#define Period 15

/**
 * @brief get ADC sampling value and convert it to voltage
 *
 */
uint16_t GetVoltage(void)
{   
    IoTGpioInit(heart_GPIO);
    IoSetFunc(heart_GPIO,5);
   
    IoTGpioSetDir(heart_GPIO, IOT_GPIO_DIR_IN);
   
    unsigned int ret;
    static uint16_t data;
    

    ret = hi_adc_read(5, &data, IOT_ADC_EQU_MODEL_8, IOT_ADC_CUR_BAIS_DEFAULT, 0xff);
    if (ret != IOT_SUCCESS) {
        printf("ADC Read Fail\n");
    }

    return (float)data;//数据未处理
}

/**
 * @brief Adc task get adc sampling voltage
 *
 */
/**
 *@brief     寻找当前数组的最大值
 *@attention 无
 *@param     data[]  16位无符号整型数组
 *@retval    当前数组最大值
 */
static uint16_t MaxElement(uint16_t data[])
{
    uint8_t i;
    uint16_t max = data[0];
    for(i = 1; i < Size; i++)
    {
        if(data[i] >= max)
        {
            max = data[i];
        }
    }
    return max;
}

/**
 *@brief     寻找当前数组的最小值
 *@attention 无
 *@param     data[]  16位无符号整型数组
 *@retval    当前数组最小值
 */
static uint16_t MinElement(uint16_t data[])
{
    uint8_t i;
    uint16_t min = data[0];
    for(i = 1; i < Size; i++)
    {
        if(data[i] <= min)
        {
            min = data[i];
        }
    }
    return min;
}
    
/**
 *@brief     计算心率函数
 *@attention 在主函数中以15Hz频率工作
 *@param     data  16位无符号整形数据
 *@retval    无
 */
uint16_t Rate_Calculate(uint16_t data)
{
    static uint8_t index; //当前数组下标
    static uint8_t pulseflag; //特征点数目
    static uint16_t Input_Data[Size]; //缓存数据数组
    static uint16_t predata, redata; //一个波形周期内读取上一次值、当前值 
    uint16_t Maxdata, Mindata; //最大值，最小值
    static uint16_t Middata; //平均值
    static uint16_t range = 1024; //幅度
    static uint8_t prepulse, pulse; //衡量指标值，用以标记特征点
    static uint16_t time1, time2, time; //检测时间
    uint16_t IBI, BIM; //两个特征点间隔时间，心率大小
    
    predata = redata; //保存上一次的值
    redata = data; //读取当前值
    if(redata - predata < range) //滤除突变噪声信号干扰
    {
        Input_Data[index] = redata; //填充缓存数组
        index++;
    }
    if(index >= Size)
    {
        index = 0; //覆盖数组
        Maxdata = MaxElement(Input_Data); 
        Mindata = MinElement(Input_Data);
        Middata = (Maxdata + Mindata) / 2; //更新参考阈值
        range = (Maxdata - Mindata) / 2; //更新突变阈值
    }
    
    prepulse = pulse; //保存当前脉冲状态
    pulse = (redata > Middata) ? 1 : 0; //采样值大于中间值为有效脉冲
    if(prepulse == 0 && pulse == 1) //当检测到两个有效脉冲时为一个有效心率周期
    {
        pulseflag++;
        pulseflag = pulseflag % 2;
        
        if(pulseflag == 1) //检测到第一个有效脉冲
        {
            time1 = time; //标记第一个特征点检测时间
        }
        if(pulseflag == 0) //检测到第二个有效脉冲
        {
            time2 = time; //标记第二个特征点检测时间
            time = 0; //清空计时
            if(time1 < time2)
            {
                IBI = (time2 - time1) * Period; //计算两个有效脉冲的时间间隔（单位：ms）
                BIM = 60000 / IBI; //1分钟有60000ms
                
                /**限制BIM最高/最低值*/
                if(BIM > 200)
                {
                    BIM = 200;
                }
                if(BIM < 30)
                {
                    BIM = 30;
                }
                
                printf("Current Heart Rate: %d\r\n", BIM);
                usleep(heart_TASK_DELAY_1S);
                
                return BIM; // Return the calculated Heart Rate Value
            }
        }
    }
    time++;
    
    return 0; // Default return value in case BIM is not calculated
}


uint16_t heartTask(void)
{
    uint16_t voltage;
    uint16_t BPM;
    voltage=GetVoltage();
    BPM=Rate_Calculate(voltage);
    return BPM;
}


/**
 * @brief Main Entry of the Adc Example
 *
 */
static void heartExampleEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "heartTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = heart_TASK_STACK_SIZE;
    attr.priority = heart_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)heartTask, NULL, &attr) == NULL) {
        printf("Failed to create AdcTask!\n");
    }
}

APP_FEATURE_INIT(heartExampleEntry);