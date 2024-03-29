/*******************************************************************************
*
* E M B E D D E D   W I Z A R D   P R O J E C T
*
*                                                Copyright (c) TARA Systems GmbH
*                                    written by Paul Banach and Manfred Schweyer
*
********************************************************************************
*
* This software is delivered "as is" and shows the usage of other software
* components. It is provided as an example software which is intended to be
* modified and extended according to particular requirements.
*
* TARA Systems hereby disclaims all warranties and conditions with regard to the
* software, including all implied warranties and conditions of merchantability
* and non-infringement of any third party IPR or other rights which may result
* from the use or the inability to use the software.
*
********************************************************************************
*
* DESCRIPTION:
*   This file implements an interface between an Embedded Wizard generated UI
*   application and a certain device. Please consider this file only as template
*   that is intended to show the binding between an UI application and the
*   underlying system (e.g. middleware, BSP, hardware driver, protocol, ...).
*
*   This device driver is the counterpart to a device class implemented within
*   your Embedded Wizard project.
*
*   Feel free to adapt this file according your needs!
*
*   Within this sample, we demonstrate the access to the board LED and the
*   hardware button. Furthemore, the console interface is used to print a string.
*
*   This file assumes to be the counterpart of the device class 'DeviceClass'
*   within the unit 'Application'.
*   In order to ensure that this file can be compiled for all projects with or
*   without the device class, the generated define '_ApplicationDeviceClass_'
*   is used.
*
*******************************************************************************/

/*
   Include all necessary files to access the real device and/or to get access
   to the required operating system calls.
*/
#include "ewrte.h"
#include "ew_bsp_inout.h"
#include "ew_bsp_clock.h"

/*
   Include the generated header file to access the device class, for example to
   access the class 'DeviceClass' from the unit 'Application' include the
   generated file 'Application.h'.
*/
#include "Application.h"

#ifdef _ApplicationDeviceClass_

/*
   Create a static variable to keep the global instance (autoobject) of the
   device class. The type of the variable has to correspond to the device class
   and the unit name, for example the static variable of the class 'DeviceClass'
   from the unit 'Application' has the type 'ApplicationDeviceClass'.
*/
  static ApplicationDeviceClass DeviceObject = 0;

  /* variable to store the current hardware button state */
  static int IsHardButtonDown = 0;

  /* variable to count while Button is pressed */
  static int ButtonCounter = 0;

#endif


#ifdef _ApplicationDHT11_

#include "stm32f7xx_hal.h"
#include "ew_bsp_event.h"

TIM_HandleTypeDef htim1;
DMA_HandleTypeDef hdma_tim1_ch1;
uint16_t DHT_reply[42];
XBool DHT_reply_ready = 0;
static ApplicationDHT11 dht11Object = 0;

void DMA2_Stream1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_tim1_ch1);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    /* TIM1 DMA DeInit */
    HAL_TIM_IC_Stop_DMA(&htim1, TIM_CHANNEL_1);
    HAL_TIM_Base_DeInit(&htim1);

    /* notify the GUI thread about the reading completed */
    DHT_reply_ready = 1;
    EwBspEventTrigger();
}

void initiate_DHT11_counting(ApplicationDHT11 aThis)
{
    htim1.Instance = TIM1;

    // make sure timer is reset
    HAL_TIM_Base_DeInit(&htim1);

    /* DMA controller clock enable */
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA2_Stream1_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);

    /* TIM1 DMA Init */
    /* TIM1_CH1 Init */
    hdma_tim1_ch1.Instance = DMA2_Stream1;
    hdma_tim1_ch1.Init.Channel = DMA_CHANNEL_6;
    hdma_tim1_ch1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_tim1_ch1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tim1_ch1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim1_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_tim1_ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_tim1_ch1.Init.Mode = DMA_NORMAL;
    hdma_tim1_ch1.Init.Priority = DMA_PRIORITY_LOW;
    hdma_tim1_ch1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_tim1_ch1) != HAL_OK)
    {
       return;
    }

    __HAL_LINKDMA(&htim1,hdma[TIM_DMA_ID_CC1],hdma_tim1_ch1);

    __HAL_RCC_TIM1_CLK_ENABLE();

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_IC_InitTypeDef sConfigIC = {0};

	htim1.Init.Prescaler = (SystemCoreClock / 1000000) - 1;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = 65535;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
	{
	    return;
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
	{
	    HAL_TIM_Base_DeInit(&htim1);
	    return;
	}
	if (HAL_TIM_IC_Init(&htim1) != HAL_OK)
	{
        HAL_TIM_Base_DeInit(&htim1);
	    return;
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
	{
        HAL_TIM_Base_DeInit(&htim1);
	    return;
	}
	sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
	sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
	sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
	sConfigIC.ICFilter = 0;
	if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
	{
        HAL_TIM_Base_DeInit(&htim1);
	    return;
	}


	//HAL_TIM_Base_Start(&htim1);
	HAL_TIM_IC_Stop_DMA(&htim1, TIM_CHANNEL_1);
	HAL_TIM_IC_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)DHT_reply, 42);

	dht11Object = aThis;
	EwLockObject(dht11Object);
}

void cancel_DHT11_counting()
{
    // stop DMA
    HAL_TIM_IC_Stop_DMA(&htim1, TIM_CHANNEL_1);
    HAL_TIM_Base_DeInit(&htim1);

    if (dht11Object != 0)
    {
        EwUnlockObject(dht11Object);
        dht11Object = 0;
    }
}


#endif

/*******************************************************************************
* FUNCTION:
*   HardButtonIsrCallback
*
* DESCRIPTION:
*   Callback function for the hardware button. This function is called every time
*   the hardware button is pressed or released. Please note, that this function is
*   called from the interrupt service routine.
*
* ARGUMENTS:
*   aButtonPresssed - 1, if hardware button is pressed and 0, if it is released.
*
*******************************************************************************/
void HardButtonIsrCallback( int aButtonPresssed )
{

#ifdef _ApplicationDeviceClass_

  /* store the current button state */
  IsHardButtonDown = aButtonPresssed;

  /*
     Important note: This function is called from an interrupt handler and not
     in the context of the main GUI task/thread/process. NEVER make a direct
     function call to a method of the driver class or any other generated code
     from an interrupt handler or another task/thread/process.
  */

#endif

}


/*******************************************************************************
* FUNCTION:
*   DeviceDriver_Initialize
*
* DESCRIPTION:
*   The function DeviceDriver_Initialize() initializes the module and prepares all
*   necessary things to access or communicate with the real device.
*   The function has to be called from your main module, after the initialization
*   of your GUI application.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
void DeviceDriver_Initialize( void )
{
  /*
     You can implement here the necessary code to initialize your particular
     hardware, to open needed devices, to open communication channels, etc.
  */

  /* configure LED */
  EwBspInOutInitLed();

  /* Configure interrupt for hardware button */
  EwBspInOutInitButton( HardButtonIsrCallback );

#ifdef _ApplicationDeviceClass_

  /*
     Get access to the counterpart of this device driver: get access to the
     device class that is created as autoobject within your Embedded Wizard
     project. For this purpose you can call the function EwGetAutoObject().
     This function contains two paramters: The pointer to the autoobject and
     the class of the autoobject.
     Assuming you have implemented the class 'DeviceClass' within the unit
     'Application' and you have an autoobject with the name 'Device', make
     the following call:
     EwGetAutoObject( &ApplicationDevice, ApplicationDeviceClass );
  */

  DeviceObject = EwGetAutoObject( &ApplicationDevice, ApplicationDeviceClass );

  /*
     Once you have the access to the autoobject, lock it as long as you need
     the access (typically, until your device driver is closed). By locking
     the autoobject, you can ensure that the object of the device class will
     be kept within the memory and not freed by the Garbage Collector.
     Once the device class is locked, you can easily store it within a static
     variable to access the driver class during the runtime.
  */

  EwLockObject( DeviceObject );

#endif

}


/*******************************************************************************
* FUNCTION:
*   DeviceDriver_Deinitialize
*
* DESCRIPTION:
*   The function DeviceDriver_Deinitialize() deinitializes the module and
*   finalizes the access or communication with the real device.
*   The function has to be called from your main module, before the GUI
*   application will be deinitialized.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   None
*
*******************************************************************************/
void DeviceDriver_Deinitialize( void )
{
  /*
     You can implement here the necessary code to deinitialize your particular
     hardware, to close used devices, to close communication channels, etc.
  */

#ifdef _ApplicationDeviceClass_

  /*
     If you have access to the device class autoobject, don't forget to unlock
     it. Clear the static variable to avoid later usage.
  */

  if ( DeviceObject )
    EwUnlockObject( DeviceObject );

  DeviceObject = 0;

#endif

}


/*******************************************************************************
* FUNCTION:
*   DeviceDriver_ProcessData
*
* DESCRIPTION:
*   The function DeviceDriver_ProcessData() is called from the main UI loop, in
*   order to process data and events from your particular device.
*   This function is responisble to update properties within the device class
*   if the corresponding state or value of the real device has changed.
*   This function is also responsible to trigger system events if necessary.
*
* ARGUMENTS:
*   None
*
* RETURN VALUE:
*   The function returns a non-zero value if a property has changed or if a
*   system event was triggered. If nothing happens, the function returns 0.
*
*******************************************************************************/
int DeviceDriver_ProcessData( void )
{
  int needUpdate = 0;

  /*
     Get the data you want to provide to the GUI application.
     In case your are working with an operating system and your device is
     controlled from a separate task/thread/process, take all information
     from your device driver out of the message queue.
     Please note, that this function is called within the context of the main
     GUI thread.
     If you control your system by direct register access or some BSP functions,
     get all necessary data you want to provide to the GUI application.
  */

#ifdef _ApplicationDeviceClass_

  /* here we just evaluate the current hardware button state */
  if ( IsHardButtonDown )
    ButtonCounter++;

  /* check for a valid access to the autoobject of the device class */
  if ( DeviceObject == 0 )
    return 0;

  /*
     For each device paramter, that is represented by a property within the
     Embedded Wizard device class and that you want to update, you have to call
     the appropriate UpdateProperty() method.

     The following examples assumes, that you have a device class with the
     name 'DeviceClass' within the unit 'Application'.
  */

  /* Update the property HardButtonCounter within the class Application::DeviceClass
     by calling the method 'UpdateHardButtonCounter' - the generated define is
     evaluated to ensures that the method is available within the generated code. */
  if ( ButtonCounter > 0 )
  {
    /* clear counter if button is released */
    if ( !IsHardButtonDown )
      ButtonCounter = 0;

    #ifdef _ApplicationDeviceClass__UpdateHardButtonCounter_

      ApplicationDeviceClass__UpdateHardButtonCounter( DeviceObject, (XInt32)ButtonCounter );
      needUpdate = 1;

    #endif
  }

  /*
     Trigger system events if necessary, e.g. if a certain situation happens,
     if an error occurs or just if a certain value has changed...
  */

  /* When the hardware button is pressed, call the method 'TriggerHardButtonEvent()' of the
     device class 'DeviceClass' within the unit 'Application' - the generated define is
     evaluated to ensure that the method is available within the generated code. */
  if ( ButtonCounter == 1 )
  {
    #ifdef _ApplicationDeviceClass__TriggerHardButtonEvent_

      ApplicationDeviceClass__TriggerHardButtonEvent( DeviceObject );
      needUpdate = 1;

    #endif
  }

#endif


#ifdef _ApplicationDHT11_
  if (DHT_reply_ready)
  {
      uint32_t bits = 0;
      uint8_t crc = 0;
      XBool success;
      uint32_t i;

      DHT_reply_ready = 0;

      for (i = 0; i < 32; i++)
      {
          if (DHT_reply[i+2] - DHT_reply[i+1] > 90)
              bits |= 1 << (31 -i);
      }
      for (i = 0; i < 8; i++)
      {
          if (DHT_reply[i+34] - DHT_reply[i+33] > 90)
              crc |= 1 << (7 -i);
      }
      success = crc == (uint8_t)(bits >> 24) + (uint8_t)(bits >> 16) + (uint8_t)(bits >> 8) + (uint8_t)bits;

      if (dht11Object)
      {
          EwUnlockObject(dht11Object);
          ApplicationDHT11__receiveFinished(dht11Object, success, (XInt32)((bits >> 8) & 0xFF), (XInt32)((bits >> 24) & 0xFF));
          dht11Object = 0;
          needUpdate = 1;
      }
  }
#endif
  /*
     Return a value != 0 if there is at least one property changed or if a
     system event was triggered. The return value is used by the main loop, to
     decide whether the GUI application has changed or not.
  */

  return needUpdate;
}


/*******************************************************************************
* FUNCTION:
*   DeviceDriver_SetLedStatus
*
* DESCRIPTION:
*   This is a sample for a function called from the device class, when a
*   property has changed. As a result, the corresponding value of the real
*   device should be changed.
*   In this implementation simply the LED is switched on or off.
*
*******************************************************************************/
void DeviceDriver_SetLedStatus( XInt32 aValue )
{
  /*
     In case you are using an operating system to communicate with your
     device driver that is running within its own thread/task/process,
     send a message to the device driver and transmit the new value.
     Please note, that this function is called within the context of the main
     GUI thread.
  */

  /*
     Here we are accessing directly the device driver by calling a certain
     BSP / driver function.
  */

  if ( aValue )
    EwBspInOutLedOn();
  else
    EwBspInOutLedOff();
}


/*******************************************************************************
* FUNCTION:
*   DeviceDriver_PrintMessage
*
* DESCRIPTION:
*   This is a sample for a function that is called directly from a 'Command'
*   method of the device class. As a result, the corresponding action should
*   happen.
*   In this implementation the given message is printed via the console interface.
*
*******************************************************************************/
void DeviceDriver_PrintMessage( XString aText )
{
  /* just print the given 16bit string... */
  EwPrint( "The string is: %S\n", aText );
}


/*******************************************************************************
* FUNCTION:
*   DeviceDriver_SetTime
*
* DESCRIPTION:
*   This is a sample for a function called from the device class, when the
*   system time (RTC time) should be changed.
*
*******************************************************************************/
void DeviceDriver_SetTime( XUInt32 aTime )
{
  /*
     In case you are using an operating system to communicate with your
     device driver that is running within its own thread/task/process,
     send a message to the device driver and transmit the new value.
     Please note, that this function is called within the context of the main
     GUI thread.
  */

  /*
     Here we are accessing directly the device driver by calling a certain
     BSP / driver function.
  */

  EwBspClockSetTime( aTime );
}


/* msy */
