
/* 包含头文件 ----------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "usart/bsp_usartx.h"
#include "string.h"
#include "led/bsp_led.h"
#include "StepMotor/bsp_StepMotor.h"
#include "key/bsp_key.h"
#include "AdvancedTIM/bsp_AdvancedTIM.h"
#include "GeneralTIM/bsp_GeneralTIM.h"
#include "drv8825/drv8825.h"
#include "i2c/bsp_EEPROM.h"
#include <math.h>
#include "GeneralTIM/bsp_GeneralTIM.h"
#include "lamp/bsp_lamp.h"
#include "ds18b20/bsp_ds18b20.h"
#include "spi/bsp_spi.h"
#include "spi_comm/spi_comm.h"
#include "i2c_slave/bsp_I2C.h"
#include "hextodec/hextodec.h"
#include "a1_fun/a1_fun.h"
#include "CAN/bsp_CAN.h"

#define SENDBUFF_SIZE             100 // 串口DMA发送缓冲区大小
#define STEPMOTOR_MICRO_STEP      32  // 步进电机驱动器细分，必须与驱动器实际设置对应

/* 私有变量 ------------------------------------------------------------------*/
uint8_t aRxBuffer[256];                      // 接收数据 
uint8_t aTxBuffer[SENDBUFF_SIZE];       // 串口DMA发送缓冲区
uint8_t repcdata[3]={0x00,0x00,0x00};                    //从上位机接收到的3个字节数据。
extern uint8_t SPI_aRxBuffer[7];
extern uint8_t SPI_aTxBuffer[7];
uint8_t I2C_RX_Buffer[3];  //I2C 接收到的数据
uint8_t I2C_RX_SAVE_Buffer[3];
/* 扩展变量 ------------------------------------------------------------------*/
extern __IO uint16_t Toggle_Pulse; /* 步进电机速度控制，可调节范围为 650 -- 3500 ，值越小速度越快 */
__IO uint32_t pulse_count; /*  脉冲计数，一个完整的脉冲会增加2 */
extern  __IO uint16_t  home_position    ; 
extern __IO uint8_t Brightness;   
extern __IO uint8_t save_flag;
__IO uint8_t re_intrrupt_flag=0; //从上位机，接收信号标志位，进入中断标志位
__IO uint16_t judge_data;    //接收到数据的功能码数据。
extern __IO uint8_t key_stop;
__IO uint8_t PB8_flag=0;
extern __IO uint8_t stop_key_flag;
uint8_t SPI_TX_FLAG=0;
extern uint8_t test_aTxBuffer[7];
uint8_t i2c_rx_data;
static uint8_t j=0;
extern __IO uint8_t END_STOP_FLAG;  //马达运行到终点，停止标志�
__IO uint8_t A1_CONTROL_A2_FLAG=0;  //A1 鎺у埗A2 鏍囧織浣�
__IO uint8_t A1_Read_FLAG=0; //A1 璇诲彇鍙傛暟鍊�
__IO uint8_t A1_Read_Temp=0;
__IO uint8_t MOTOR_STOP_FLAG=0;  //杈撳叆椹揪鍋滄鎸囦护锛屾爣蹇椾綅
__IO uint8_t A1_ReadData_A2_Flag=0; //璇诲彇绗簩涓┈杈炬暟鎹�

__IO uint16_t A1_Read_A2_Judge; //椹揪1璇诲彇椹揪2 鍒よ鍊兼寚浠�
extern __IO uint8_t  END_A1_READ_A2_RealTime_FLAG; //椹揪1 璇诲彇椹揪2 鍋滄鏍囧織浣�
uint8_t CAN_TX_BUF[4]={0X00,0X00,0X00,0X00};
uint8_t CAN_RX_BUF[4];

/* 扩展变量 ------------------------------------------------------------------*/
/* 私有函数原形 --------------------------------------------------------------*/
/* 函数体 --------------------------------------------------------------------*/
/**
  * 函数功能: 系统时钟配置
  * 输入参数: 无
  * 返 回 值: 无
  * 说    明: 无
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;  // 外部晶振，8MHz
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;  // 9倍频，得到72MHz主时钟
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;       // 系统时钟：72MHz
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;              // AHB时钟：72MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;               // APB1时钟：36MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;               // APB2时钟：72MHz
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

 	// HAL_RCC_GetHCLKFreq()/1000    1ms中断一次
	// HAL_RCC_GetHCLKFreq()/100000	 10us中断一次
	// HAL_RCC_GetHCLKFreq()/1000000 1us中断一次
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);  // 配置并启动系统滴答定时器
 // HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/100000);  // 配置并启动系统滴答定时器
  /* 系统滴答定时器时钟源 */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
  /* 系统滴答定时器中断优先级配置 */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/*********************************************************
  * 函数功能: 主函数.
  * 输入参数: 无
  * 返 回 值: 无
  * 说    明: 无   SPI1 通信从机
  *********************************************************/
int main(void)
{
  uint8_t txbuf[100],Mode_Count=0;
  uint8_t i, DS18B20ID[8];
  float temperature;
  uint8_t power_on=0,temp,temp1;
	
  /* 复位所有外设，初始化Flash接口和系统滴答定时器 */
  HAL_Init();
  /* 配置系统时钟 */
  SystemClock_Config();
  LED_GPIO_Init();
  KEY_GPIO_Init();
  GENERAL_TIMx_Init();
  MX_USARTx_Init();
	
  CAN1_Mode_Init(CAN_SJW_1TQ,CAN_BS1_5TQ,CAN_BS2_2TQ,36,CAN_MODE_NORMAL ); //CAN初始化,波特率125Kbps
  STEPMOTOR_TIMx_Init();
  MX_I2C_EEPROM_Init();
  /* 确定定时器 */

	
  SPIx_Init();
  HAL_TIM_Base_Start(&htimx_STEPMOTOR);

  // __HAL_UART_ENABLE_IT(&husartx, UART_IT_IDLE);  //wt.edit 11.07
  memcpy(txbuf,"SPI_MASTER version 10.0 2018-05-20 \n",100);
  HAL_UART_Transmit(&husartx,txbuf,strlen((char *)txbuf),1000);
  Brightness=LAMP_Read_BrightValue(); 
 // GENERAL_TIMx_Init();
 // HAL_TIM_PWM_Start(&htimx,TIM_CHANNEL_1); 
  /* 使能接收，进入中断回调函数 */
  HAL_UART_Receive_IT(&husartx,aRxBuffer,7);
  HAL_TIM_PWM_Start(&htimx,TIM_CHANNEL_1);//HAL_TIM_PWM_Start(&htimx,TIM_CHANNEL_1);
   /* 使能接收，进入中断回调函数 */
  //Brightness=LAMP_Read_BrightValue(); 
  GENERAL_TIMx_Init();
  HAL_TIM_PWM_Start(&htimx,TIM_CHANNEL_1); 
  printf("DS18B20温度传感器信息读取\n");
  while(DS18B20_Init())	
  {
		printf("DS18B20温度传感器不存在\n");    
    HAL_Delay(1000);
  }
  printf("检测到DS18B20温度传感器，并初始化成功\n");
  DS18B20_ReadId(DS18B20ID);
  printf("DS18B20的序列号是： 0x");  
	for ( i = 0; i < 8; i ++ )             
	  printf ( "%.2X", DS18B20ID[i]);
  printf("\n");  
  
  DS18B20_GetTemp_MatchRom(DS18B20ID);
  HAL_Delay(100);
  
  SPIx_CS_ENABLE() ;
  HAL_Delay(600);
  MX_I2C_EEPROM_Init();
  
  HAL_I2C_Slave_Receive_IT(&I2cHandle,&i2c_rx_data,4);

  
  while (1)
  {
		
	     DRV8825_SLEEP_DISABLE() ; //高电平开始工作
	     A2_Data_Save_To_EEPROM();
	     A1_Read_A2_Data_EEPROM();
	     if(power_on==0)
		 {
		      SPI_TX_FLAG=0;
			  SPI_aTxBuffer[1]=0x00;
		      temp=SPI_COMM_Function(0xee,repcdata[0],repcdata[1],repcdata[2]);
		     if(temp)
			 {
			     temp1=SPI1_ReadWriteByte(0xab);
				 if(temp1==0xab)
					 power_on=1;
			 }
			 else
			 {
			   SPI_COMM_Function(0xee,repcdata[0],repcdata[1],repcdata[2]);
			   HAL_Delay(500);
			   SPI_COMM_Function(0xee,repcdata[0],repcdata[1],repcdata[2]);
			   HAL_Delay(500);
			    SPI_COMM_Function(0xee,repcdata[0],repcdata[1],repcdata[2]);
			 
			 }
		 
		 }
         if(KEY1_StateRead()==KEY_DOWN)
		 {
		     Mode_Count++;
			   if(Mode_Count ==4&&Mode_Count!=0)
               Mode_Count = 0;	 
			   STEPMOTOR_AxisMoveRel(-5*SPR, Toggle_Pulse);
			  
		}
		if(KEY2_StateRead() == KEY_DOWN)
		{
			switch(Mode_Count)
			{
				case 0:
				{
				 	//STEPMOTOR_AxisHome(0x03); //搜索原点
				}break;
				case 1://CW顺时针方向旋转n*SPR表示旋转3圈
				{
					STEPMOTOR_AxisMoveRel(1*SPR, Toggle_Pulse);
				}break;
				case 2://CCW逆时针方向旋转
				{
						STEPMOTOR_AxisMoveRel(-1*SPR, Toggle_Pulse);
				}break;
				case 3://回坐标原点功能,自动回到坐标
				{
					STEPMOTOR_AxisMoveAbs(0*SPR,Toggle_Pulse);
				}
				default :break;			
			}
		}
		
		if((HAL_GPIO_ReadPin(GPIO_PB8,GPIO_PB8_PIN)==0))
		{ 
		   
			DRV8825_StopMove();
			HAL_Delay(50);
		 }
		if((KEY3_StateRead()==KEY_DOWN)||MOTOR_STOP_FLAG==1)
		{
			MOTOR_STOP_FLAG=0;
		    DRV8825_StopMove();
			HAL_Delay(50);
		}
		if(re_intrrupt_flag==1) 
		{
            re_intrrupt_flag=0;
            A1_FUN();
		}
		if(A1_Read_FLAG==1)
		{
            A1_Read_FLAG=0;
			A1_ReadData_FUN();
		}
		if(A1_CONTROL_A2_FLAG==1)
		{
            A1_CONTROL_A2_FLAG=0;
			
			A1_CONTROL_A2_FUN();
		}
		if( END_STOP_FLAG==1)  //椹揪鍋滄鏍囧織浣�
		{
		   END_STOP_FLAG=0;
		   END_A1_READ_A2_RealTime_FLAG=1;
		   Motor_Save_EndPosition();
        }
		if(A1_Read_Temp==1)
		{
		    A1_Read_Temp=0;
			temperature=DS18B20_GetTemp_MatchRom(DS18B20ID);
           /* 鎵撳嵃鍑轰竴涓俯搴﹀�� */
          printf("DS18B20 temperature =%.1f\n",temperature);
		  Dec_To_Hex(temperature);
		  LED1_ON;
          HAL_Delay(1000);
		}
		if(A1_ReadData_A2_Flag==1)
        {
            A1_ReadData_A2_Flag=0;
			A1_ReadData_A2_Fun();

		}
		
	}
	
}
/*****************************end main()************************************************/
/**************************************************************************************/
/**************************************************************************************/
/*************************mian function over**********************************************/


/*******************************************************************************
   *
   * 鍑芥暟鍚嶏細
   * 鍑芥暟鍔熻兘锛氫覆鍙ｅ洖璋冨嚱鏁�
   * 杈撳叆鍙傛暟锛氬彞鏌�
   * 杩斿洖鍊硷細鏃�
   *
  ******************************************************************************/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
   
    __HAL_GPIO_EXTI_CLEAR_IT(KEY3_GPIO_PIN);
	if((HAL_GPIO_ReadPin(GPIO_PB8,GPIO_PB8_PIN)==0)||(KEY3_StateRead()==KEY_DOWN))
		{
		  DRV8825_StopMove();
		}
  
if(HAL_UART_Receive_IT(&husartx,aRxBuffer,7)==HAL_OK )
 {
	  //HAL_Delay(50);
		if((HAL_GPIO_ReadPin(GPIO_PB8,GPIO_PB8_PIN)==0)||(KEY3_StateRead()==KEY_DOWN))
		{
		  DRV8825_StopMove();
		}

	  if(aRxBuffer[0]==0xa1)
		{
          if(aRxBuffer[1]==0x00)
		   {
			switch(aRxBuffer[2])
            	{
                      case 0xff :
                         if(aRxBuffer[6]==0x0b)
                         	{
                               re_intrrupt_flag=1;
				               judge_data= 0xff;
					
					          __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						     }
					  
					  	break;
					  case 0x33 :
						  if(aRxBuffer[6]==0x0b)
                         	{
                                re_intrrupt_flag=1;
								repcdata[0]=	aRxBuffer[3]; //最高字节
							    repcdata[1]=	aRxBuffer[4]; //中间字节
							    repcdata[2]=	aRxBuffer[5]; //最低字节
							    judge_data= 0x33;
							    __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23 	  
						     
							}
						  
						  break;
						case 0xb0 :
						  if(aRxBuffer[6]==0x0b)
                         	{
                               re_intrrupt_flag=1;
				              judge_data= 0xb0;
				
				               __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23 	 
						    
							}
						  
						  break;
						case 0x02 :
							 if(aRxBuffer[6]==0x0b)
								{
									re_intrrupt_flag=1;
								   repcdata[0]=	aRxBuffer[3]; //最高字节
								   repcdata[1]=	aRxBuffer[4]; //中间字节
								   repcdata[2]=	aRxBuffer[5]; //最低字节
								   judge_data= 0x02;
								 
								   __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23 
								}
							
							break;
						case 0x82 :
							if(aRxBuffer[6]==0x0b)
								{
									re_intrrupt_flag=1;
									repcdata[0]=	aRxBuffer[3]; //最高字节
								   repcdata[1]=	aRxBuffer[4]; //中间字节
								   repcdata[2]=	aRxBuffer[5]; //最低字节
								   judge_data= 0x82;
								 
								   __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23 	 
								}
							
							break;
						case 0x90 :
							if(aRxBuffer[6]==0x0b)
								{	  
									re_intrrupt_flag=1;
								   repcdata[1]=	aRxBuffer[4]; //中间字节
								   repcdata[2]=	aRxBuffer[5]; //最低字节
								   judge_data= 0x90;
									
									__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
								}
							
							break;
						case 0xa0 :
							if(aRxBuffer[6]==0x0b)   //重新设置原点
								{
									//DRV8825_SLEEP_DISABLE(); //高电平开始工作,解除休眠状态
									 re_intrrupt_flag=1;
									 judge_data= 0xa0;
									__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
									
								}
							break;
						case 0x00 :
							if(aRxBuffer[6]==0x0b)
								{
                                    MOTOR_STOP_FLAG=1;
									__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
								}
							break;
						case 0xc0 :
							 if(aRxBuffer[6]==0x0b)
								{
								   re_intrrupt_flag=1;
								   judge_data= 0xc0;
                                   __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
								}
							break;
						case 0xd0 :
							 if(aRxBuffer[6]==0x0b)
								{
								   re_intrrupt_flag=1;
								   judge_data= 0xd0;
									 __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23	
								}
							  break;
			    }
			} //end if(aRxBuffer[1]==0x00)
		/*************************A1璇诲彇椹揪鍙傛暟鍊�********************************/
		   if(aRxBuffer[1]==0x01)   
		   {
		     switch(aRxBuffer[2])
			 {
				 case 0x03:   //读取马达实时位置脉冲数
				    if(aRxBuffer[6]==0x0b)
						{
						   A1_Read_FLAG=1;
						   judge_data= 0x103;
						  //Display_CurrentPosition();
							__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
				  break; 
				case 0xe0:   //璇诲彇A1 EEPROM 鍊�
				    if(aRxBuffer[6]==0x0b)
						{
						   A1_Read_FLAG=1;
						   judge_data= 0x1e0;
						   __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
				  break; 
				 case 0x04:      /* 读取LED 灯亮度值 */
					 {
						if(aRxBuffer[6]==0x0b)
						{

							A1_Read_FLAG=1;
						   judge_data= 0x104;
							
						
						__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
					 }
				    break;
				  case 0x01 ://case 0x01 :     /*if(aRxBuffer[2]==0x01)读取马达转速值*/
					 {
						if(aRxBuffer[6]==0x0b)
						{

							A1_Read_FLAG=1;
						   judge_data= 0x101;
							__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
					 }
					 break;
					 case 0x02:     /*读取环境温度值*/
					 {
						if(aRxBuffer[6]==0x0b)
						{
							A1_Read_Temp=1;
						   __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
					 }
                     break;
                   					 
				   
		   }
	     
	    }
  }// end if(aRxBuffer[0]==0xa1)

  /*****************控制马达A2******************************/
   if(aRxBuffer[0]==0xA2)
   {
    if(aRxBuffer[1]==0x00)
		   {
			switch(aRxBuffer[2])
            	{
                      case 0xff :
                         if(aRxBuffer[6]==0x0b)
                         	{
                               A1_CONTROL_A2_FLAG=1;
				               judge_data= 0x2ff;
					
					          __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						     }
					  
					  	break;
					  case 0x33 :
						  if(aRxBuffer[6]==0x0b)
                         	{
                                 A1_CONTROL_A2_FLAG=1;
								repcdata[0]=	aRxBuffer[3]; //最高字节
							    repcdata[1]=	aRxBuffer[4]; //中间字节
							    repcdata[2]=	aRxBuffer[5]; //最低字节
							    judge_data= 0x233;
							    __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23 	  
						     
							}
						  
						  break;
						case 0xb0 :
						  if(aRxBuffer[6]==0x0b)
                         	{
                               A1_CONTROL_A2_FLAG=1;
				              judge_data= 0x2b0;
				
				               __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23 	 
						    
							}
						  
						  break;
						case 0x02 :
							 if(aRxBuffer[6]==0x0b)
								{
									 A1_CONTROL_A2_FLAG=1;
								   repcdata[0]=	aRxBuffer[3]; //最高字节
								   repcdata[1]=	aRxBuffer[4]; //中间字节
								   repcdata[2]=	aRxBuffer[5]; //最低字节
								   judge_data= 0x202;
								 
								   __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23 
								}
							
							break;
						case 0x82 :
							if(aRxBuffer[6]==0x0b)
								{
									 A1_CONTROL_A2_FLAG=1;
								   repcdata[0]=aRxBuffer[3]; //最高字节
								   repcdata[1]=	aRxBuffer[4]; //中间字节
								   repcdata[2]=	aRxBuffer[5]; //最低字节
								   judge_data= 0x282;
								 
								   __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23 	 
								}
							
							break;
						case 0x90 :
							if(aRxBuffer[6]==0x0b)
								{	  
									 A1_CONTROL_A2_FLAG=1;
								   repcdata[1]=	aRxBuffer[4]; //中间字节
								   repcdata[2]=	aRxBuffer[5]; //最低字节
								   judge_data= 0x290;
									
									__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
								}
							
							break;
						case 0xa0 :
							if(aRxBuffer[6]==0x0b)   //重新设置原点
								{
									//DRV8825_SLEEP_DISABLE(); //高电平开始工作,解除休眠状态
									  A1_CONTROL_A2_FLAG=1;
									 judge_data= 0x2a0;
									__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
									
								}
							break;
						case 0x00 :   //控制第二马达，停止。
							if(aRxBuffer[6]==0x0b)
								{
									 A1_CONTROL_A2_FLAG=1;
									judge_data= 0x00;
									__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
								}
							break;
						case 0xc0 :
							 if(aRxBuffer[6]==0x0b)
								{
								    A1_CONTROL_A2_FLAG=1;
								   judge_data= 0x2c0;
                                   __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
								}
							break;
						case 0xd0 :  //清楚EEPROM数据
							 if(aRxBuffer[6]==0x0b)
								{
								    A1_CONTROL_A2_FLAG=1;
								   judge_data= 0x2d0;
									 __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23	
								}
							  break;
					case 0xee:   //测试A1 与A2 板通讯同步。A1,A2 LED同时闪烁
							  A1_CONTROL_A2_FLAG=1;
							  judge_data= 0x2ee;
						       __HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23	
							  break;
						
			    }
			} //end if(aRxBuffer[1]==0x00)
		   /*****************************读取第二个马达A2数据*******************************/
		   if(aRxBuffer[1]==0x01)   
		   {
		     switch(aRxBuffer[2])
			 {
				 case 0x03:   //读取马达实时位置脉冲数
				    if(aRxBuffer[6]==0x0b)
						{
						    HAL_Delay(100);
							A1_ReadData_A2_Flag=1;
						    A1_Read_A2_Judge= 0x2103;
							__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
				  break; 
				 case 0x04:      /* 读取LED 灯亮度值 */
					 {
						if(aRxBuffer[6]==0x0b)
						{

							   A1_ReadData_A2_Flag=1;
						   A1_Read_A2_Judge= 0x2104;
							__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
					 }
				    break;
				  case 0x01 ://case 0x01 :     /*if(aRxBuffer[2]==0x01)读取马达转速值*/
					 {
						if(aRxBuffer[6]==0x0b)
						{
                           A1_ReadData_A2_Flag=1;
						   A1_Read_A2_Judge= 0x2101;
							__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
					 }
					 break;
					 case 0x02:    //if(aRxBuffer[2]==0x02)  /*读取环境温度值*/
					 {
						if(aRxBuffer[6]==0x0b)
						{
							
						    A1_ReadData_A2_Flag=1;
						   A1_Read_A2_Judge= 0x2102;
						  
							__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
					 }
                     break;	
					 case 0xe0 :
						 if(aRxBuffer[6]==0x0b)
						{
							
						     A1_ReadData_A2_Flag=1;
						   A1_Read_A2_Judge= 0x21e0;
						  
							__HAL_UART_CLEAR_IDLEFLAG(&husartx); //edit 18.02.23
						}
						 break;
                   					 
				   
		   }
	     
	    }
    }  //end if(aRxBuffer[0]==0xa2)

}

}
/****************************************************************
*
*函数功能：SPI中断，回调函数
*参数：无
*返回值：无
*
*****************************************************************/
#if 1
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  
   SPI_TX_FLAG=1;
   
  #if 0
  HAL_SPI_Transmit(&hspi_SPI,&SPI_aTxBuffer[0],7,0XFFFF);
  {
   if(aRxBuffer[0]==0xa2)
	
	 {
		printf("%#x\n",aRxBuffer[0]);  
		printf("%#x\n",aRxBuffer[1]);  
		printf("%#x\n",aRxBuffer[2]);  
		printf("%#x\n",aRxBuffer[3]);  
		printf("%#x\n",aRxBuffer[4]);  
		printf("%#x\n",aRxBuffer[5]);
		printf("%#x\n",aRxBuffer[6]);
	 }

	}
  
  #endif
  
  HAL_SPI_Transmit_IT(&hspi_SPI,&SPI_aTxBuffer[0],7);
  
	
}
#endif
/***************************************************************
*
*函数名：
*函数功能：I2C中断接收回调函数
*参数：无
*返回值：无
*
****************************************************************/
#if 1
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
         I2C_RX_SAVE_Buffer[j]=i2c_rx_data;
        
		 if(j==0)
		 {
		   j++;
           I2C_RX_SAVE_Buffer[0]=i2c_rx_data;
          // printf("rxcpltcall[0]：%#x\n",I2C_RX_SAVE_Buffer[0]);
           
		 }
		 else if(j==1)
		 {
             j++;
             I2C_RX_SAVE_Buffer[1]=i2c_rx_data;
			// printf("rxcpltcall[1]：%#x\n",I2C_RX_SAVE_Buffer[1]);
			 
		 }
		 else if(j==2)
		 {
		   j=0;
           I2C_RX_SAVE_Buffer[2]=i2c_rx_data;
		  // printf("rxcpltcall[2]：%#x\n",I2C_RX_SAVE_Buffer[2]);
		 }
      
		 HAL_I2C_Slave_Receive_IT(&I2cHandle,&i2c_rx_data,4);
	 
}
#endif 
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
