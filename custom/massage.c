#include "ql_type.h"
#include "ql_gpio.h"
#include "massage.h"


/*
* Function Declaration
*/
static void MassageStateMgr(void);    /* Period 1s */
static bool HasServerHeartBeatAck(void);
static bool IsServerHeartBeatAckErr(void);
static void MotorControl(void);    /*50ms*/
static void LedControl(void);    /*1s*/
static void MotorDriver(u32 right, u32 left);
static s8 MotorStop(u32 right, u32 left);
static s8 MotorPositiveRun(bool init, u32 right);
static s8 MotorReverseRun(bool init, u32 left);
static void LedDriver(s8 state, u8 on, u8 off);  /* Period 1s */
static void IncreaseNoServerHeartBeatAckCnt(void);
static void SetNoServerHeartBeatAckCnt(u32 set);
static bool HasLedOnCmd(void);
static bool HasMassageStartCmd(void);
static bool HasMassageStopCmd(void);


/*
* Variable
*/
static char HeartBeatBuff[19u];
static char SendBuff[128u];
static u32 SendBuffLength;

static char GprsSendBuff[128u];
static u32 GprsSendBuffLength;

static char GprsReceiveBuff[128u];
static u32 GprsReceiveBuffLength;


/*
* extern variable
*/
extern Enum_PinName m_gpioMotorP;   
extern Enum_PinName m_gpioMotorN;   
extern Enum_PinName m_gpioLight; 


static s8 MassageState = CONST_MASSAGE_POWER_ON;
static u32 PowerOnTimerCnt = 0u;
static u32 MassageRunTime;

static u8 MotorPositiveRunTime = 5u;
static u8 MotorReverseRunTime = 5u;

static u8 DeviceId[6u];


static u32 NoServerHeartBeatAckCnt;
static bool LedOnCmd;
static bool MassageStartCmd;
static bool MassageStopCmd;



/*
* Function
*/
void MassageTask1s(void)
{
  MassageStateMgr();
  LedControl();
  IncreaseNoServerHeartBeatAckCnt();
}

void MassageTask50ms(void)
{
  MotorControl();
}

void ClearNoServerHeartBeatAckCnt(void)
{
  SetNoServerHeartBeatAckCnt(0u);
}

void SetMassageRunTime(u32 set)      /* unit is second*/
{
  MassageRunTime = set;
}

void SetMotorPositiveRunTime(u32 set)
{
  MotorPositiveRunTime = set;
}

void SetMotorReverseRunTime(u32 set)
{
  MotorReverseRunTime = set;
}

s8 GetMassageState(void)
{
  return MassageState;
}

void SetLedOnCmd(bool set)
{
  LedOnCmd = set;
}

void SetMassageStartCmd(bool set)
{
  MassageStartCmd = set;
}

void SetMassageStopCmd(bool set)
{
  MassageStopCmd = set;
}

char * GetGprsSendBuffPtr(void)
{
    return GprsSendBuff;
}

u32 GetGprsSendBuffLength(void)
{
    return GprsSendBuffLength;
}

char * GetHeartBeatBuffPtr(void)    /* Length is 16 */
{
    return HeartBeatBuff;
}

void TransferGprsSendBuff(char * buff, u32 len)
{
    u32 index = 1u;

    GprsSendBuff[0u] = 0x8E;

	for(u8 i = 0u; i < len; i++)
	{
		if((buff[i] == 0x8D) || (buff[i] == 0x8E) || (buff[i] == 0x8F))
		{
			GprsSendBuff[index] = 0x8D;
			GprsSendBuff[index + 1u] = (buff[i] ^ 0x20);
			index += 2u;
		}
		else
		{
			GprsSendBuff[index] = buff[i];
			index++;
		}
	}

	GprsSendBuff[index] = 0x8F;

	GprsSendBuffLength = index + 1u;
}

void TransferGprsReceiveBuff(char * buff, u32 len)
{
    u32 index = 0u;
	bool skip = FALSE;
	
	for(u8 i = 1u; i < (len-1); i++)
	{
		if(buff[i] == 0x8D)
		{
			skip = TRUE;
			continue;
		}
		else
		{
			if(skip)
			{
				GprsReceiveBuff[index] = (buff[i] ^ 0x20);
			}
			else
			{
				GprsReceiveBuff[index] = buff[i];
			}

			skip = FALSE;
			index++;
		}
	}

	GprsReceiveBuffLength = index;
}

void CalcCommunicationID(u8 * idPtr, char *phoneNum, u8 deviceNum)  /*4 and 5 ID may be 8D 8E 8F*/
{
    u8 tmp;

    for(u8 i = 0; i < 5u; i++)
   	{
   	    tmp = phoneNum[2 * i] - '0';
		tmp *= 10u;
		tmp += phoneNum[(2 * i)+1u] - '0';
		//tmp = phoneNum[2 * i] * 10u + phoneNum[(2 * i)+1u];
		idPtr[i] = tmp;
	}
	
	if((deviceNum & 0x02u) != 0u)
	{
		idPtr[3u] |= 0x80u;
	}
	
	if((deviceNum & 0x01u) != 0u)
	{
		idPtr[4u] |= 0x80u;
	}
}

void GenerateHeartBeatData(u8 * idPtr, u8 status)   
{
	HeartBeatBuff[0u] = 0x88u;
	for(u8 i = 0u; i < 5u; i++)
	{
		HeartBeatBuff[i+1u] = idPtr[i];
	}
	HeartBeatBuff[6u] = 0x00u;
	HeartBeatBuff[7u] = 0xBBu;
	HeartBeatBuff[8u] = 0x02u;
	HeartBeatBuff[9u] = status;
	HeartBeatBuff[10u] = 0u;
	HeartBeatBuff[11u] = CalcCheckSum(HeartBeatBuff, 10u);
}

void GenerateDeviceStartWorkAck(u8 * idPtr, u8 paraLen, u8 res)
{
	SendBuff[0u] = 0x88u;
	for(u8 i = 0u; i < 5u; i++)
	{
		SendBuff[i+1u] = idPtr[i];
	}
	SendBuff[6u] = 0x00u;
	SendBuff[7u] = 0xABu;
	SendBuff[8u] = 0x00u;
	SendBuff[9u] = paraLen + 5u;
	SendBuff[10u] = 0xAAu;
	SendBuff[11u] = 0x01u;
	SendBuff[12u] = 0x00u;
	SendBuff[13u] = 0xA1u;
	SendBuff[14u] = 0x01u;
	SendBuff[15u] = res;
	SendBuff[16u] = CalcCheckSum(SendBuff, 16u);

	SendBuffLength = 17u;	
}

void GenerateDeviceStopWorkAck(u8 * idPtr, u8 paraLen, u8 res)
{
	SendBuff[0u] = 0x88u;
	for(u8 i = 0u; i < 5u; i++)
	{
		SendBuff[i+1u] = idPtr[i];
	}
	SendBuff[6u] = 0x00u;
	SendBuff[7u] = 0xABu;
	SendBuff[8u] = 0x00u;
	SendBuff[9u] = paraLen + 5u;
	SendBuff[10u] = 0xAAu;
	SendBuff[11u] = 0x01u;
	SendBuff[12u] = 0x00u;
	SendBuff[13u] = 0xA2u;
	SendBuff[14u] = 0x01u;
	SendBuff[15u] = res;
	SendBuff[16u] = CalcCheckSum(SendBuff, 16u);

	SendBuffLength = 17u;	
}

char * GetProtocolBuffPtr(void)
{
    return SendBuff;
}

u32 GetProtocolBuffLength(void)
{
	return SendBuffLength;
}

u8 CalcCheckSum(u8 *buf, u32 len)
{
	u8 sum = 0u;
	
	for(u32 i = 0u; i < len; i++)
	{
		sum ^= buf[i];
	}
	
	return sum;
}

void MyHexCopy(char* dest, const char* src, u32 len)
{
    for(u32 i = 0u; i < len; i++)
    {
        dest[i] = src[i];
    }
}

/*Private function*/
static void MassageStateMgr(void)    /* Period 1s */
{
  PowerOnTimerCnt++;

  switch(MassageState)
  {
    case CONST_MASSAGE_OFF_LINE:
      if(HasServerHeartBeatAck())
      {
        MassageState = CONST_MASSAGE_ON_LINE;
      }
      break;

    case CONST_MASSAGE_ON_LINE:
      if(IsServerHeartBeatAckErr())
      {
        MassageState = CONST_MASSAGE_OFF_LINE;
      }
      else if(HasMassageStartCmd())
      {
        MassageState = CONST_MASSAGE_RUN;
      }
      else
      {
        ;/*Do Nothing*/
      }
      break;

    case CONST_MASSAGE_RUN:
      if(HasMassageStopCmd())
      {
        MassageState = CONST_MASSAGE_ON_LINE;
      }
      else
      {
        if(MassageRunTime > 0u)
        {
          MassageRunTime--;
          if(MassageRunTime == 0u)
          {
            MassageState = CONST_MASSAGE_ON_LINE;
          }
        }
        else
        {
          MassageState = CONST_MASSAGE_ON_LINE;
        }
      }
      break;

    case CONST_MASSAGE_POWER_ON:
    default:
      if(PowerOnTimerCnt > 4u)
      {
        MassageState = CONST_MASSAGE_OFF_LINE;
        SetNoServerHeartBeatAckCnt(1u);
        SetMassageStartCmd(FALSE);
        SetMassageStopCmd(TRUE);
      }
      break;
  }
}

static bool HasServerHeartBeatAck(void)
{
  return ((NoServerHeartBeatAckCnt == 0u) ? TRUE : FALSE);
}

static bool IsServerHeartBeatAckErr(void)
{
  return ((NoServerHeartBeatAckCnt >= 50u) ? TRUE : FALSE);
}

static void MotorControl(void)    /*50ms*/
{
  s8 state = GetMassageState();

  if(state == CONST_MASSAGE_POWER_ON)
  {
    MotorDriver(20u, 20u); /* motor pos and neg turn */
  }
  else if(state == CONST_MASSAGE_RUN)
  {
    MotorDriver(20*MotorPositiveRunTime, 20*MotorReverseRunTime);;
  }
  else
  {
    MotorDriver(0u, 0u); /* motor stop */
  }  
}

static void LedControl(void)    /*1s*/
{
  s8 state = GetMassageState();

  if(state == CONST_MASSAGE_POWER_ON)
  {
    LedDriver(CONST_LED_ON, 0u, 0u); /*led on 4s*/
  }
  else if(state == CONST_MASSAGE_ON_LINE)
  {
    LedDriver(CONST_LED_TOGGLE, 2u, 2u); /* led toggle 3s */
  }
  else if(state == CONST_MASSAGE_RUN)
  {
    if(HasLedOnCmd())
    {
      LedDriver(CONST_LED_ON, 0u, 0u);
    }
    else
    {
      LedDriver(CONST_LED_OFF, 0u, 0u);
    }
  }
  else
  {
    LedDriver(CONST_LED_OFF, 0u, 0u); /* led off */
  }
}

static void MotorDriver(u32 right, u32 left)
{
  static s8 MotorControlState;
  
  if((right == 0u) || (left == 0u))
  {
    MotorControlState = CONST_MOTOR_STOP;
  }

  switch(MotorControlState)
  {
    case CONST_MOTOR_POS_RUN:
      MotorControlState = MotorPositiveRun(FALSE, right);
      break;

    case CONST_MOTOR_NEG_RUN:
      MotorControlState = MotorReverseRun(FALSE, left);
      break;

    case CONST_MOTOR_STOP:
    default:
      MotorControlState = MotorStop(right, left);
      break;
  }  
}

static s8 MotorStop(u32 right, u32 left)
{
  s8 state = CONST_MOTOR_STOP;
  static bool neg;

  Ql_GPIO_SetLevel(m_gpioMotorP, PINLEVEL_LOW);
  Ql_GPIO_SetLevel(m_gpioMotorN, PINLEVEL_LOW);

  if((right != 0u) && (left != 0u))
  {
    if(!neg)
    {
      state = MotorPositiveRun(TRUE, right);
      neg = TRUE;
    }
    else
    {
      state = MotorReverseRun(TRUE, left);
      neg = FALSE;
    }
  }
  return state;
}

static s8 MotorPositiveRun(bool init, u32 right)
{
  s8 state = CONST_MOTOR_POS_RUN;
  static u32 MotorPosRunCnt;

  if(init)
  {
    MotorPosRunCnt = 0u;
  }
  else
  {
    Ql_GPIO_SetLevel(m_gpioMotorP, PINLEVEL_HIGH);
    MotorPosRunCnt++;
    if(MotorPosRunCnt >= right)
    {
      state = CONST_MOTOR_STOP;
    }
  }

  return state;
}

static s8 MotorReverseRun(bool init, u32 left)
{
  s8 state = CONST_MOTOR_NEG_RUN;
  static u32 MotorNegRunCnt;

  if(init)
  {
    MotorNegRunCnt = 0u;
  }
  else
  {
    Ql_GPIO_SetLevel(m_gpioMotorN, PINLEVEL_HIGH);
    MotorNegRunCnt++;
    if(MotorNegRunCnt >= left)
    {
      state = CONST_MOTOR_STOP;
    }
  }

  return state;
}

static void LedDriver(s8 state, u8 on, u8 off)  /* Period 1s */
{
  static bool LedOn;
  static u8 LedOnCnt;
  static u8 LedOffCnt;

  if(state == CONST_LED_ON)
  {
    Ql_GPIO_SetLevel(m_gpioLight, PINLEVEL_HIGH);  /* On */
  }
  else if(state == CONST_LED_TOGGLE)
  {
    if(LedOn)
    {
      Ql_GPIO_SetLevel(m_gpioLight, PINLEVEL_HIGH);  /* On */
      if(LedOnCnt >= on)
      {
        LedOn = FALSE;
      }
      LedOnCnt++;
      LedOffCnt = 0u;
    }
    else
    {
      Ql_GPIO_SetLevel(m_gpioLight, PINLEVEL_LOW);   /* Off */
      if(LedOffCnt >= off)
      {
        LedOn = TRUE;
      }
      LedOffCnt++;
      LedOnCnt = 0u;
    }
  }
  else
  {
    Ql_GPIO_SetLevel(m_gpioLight, PINLEVEL_LOW);   /* Off */
  }
}

static void IncreaseNoServerHeartBeatAckCnt(void)
{
  NoServerHeartBeatAckCnt++;
}

static void SetNoServerHeartBeatAckCnt(u32 set)
{
  NoServerHeartBeatAckCnt = set;
}

static void ConvertU32ToLSB(u8 * buf, u32 data)
{
	buf[0u] = (u8)(data & 0xFFu);
	data >>= 8u;
	buf[1u] = (u8)(data & 0xFFu);
	data >>= 8u;
	buf[2u] = (u8)(data & 0xFFu);
	data >>= 8u;
	buf[3u] = (u8)(data & 0xFFu);
}

static bool HasLedOnCmd(void)
{
  return LedOnCmd;
}

static bool HasMassageStartCmd(void)
{
  return MassageStartCmd;
}

static bool HasMassageStopCmd(void)
{
  return MassageStopCmd;
}

