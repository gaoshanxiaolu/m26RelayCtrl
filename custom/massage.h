#ifndef _MASSAGE_H
#define _MASSAGE_H


enum
{
  CONST_MASSAGE_POWER_ON = 1,
  CONST_MASSAGE_OFF_LINE,
  CONST_MASSAGE_ON_LINE,
  CONST_MASSAGE_RUN  
};

enum
{
  CONST_LED_OFF = 1,
  CONST_LED_ON,
  CONST_LED_TOGGLE
};

enum
{
  CONST_MOTOR_STOP = 1,
  CONST_MOTOR_POS_RUN,
  CONST_MOTOR_NEG_RUN
};

#define MASSAGE_START_CMD 0xA1u
#define MASSAGE_STOP_CMD  0xA2u


/*
* Function Declaration
*/
void MassageTask1s(void);
void MassageTask50ms(void);
void ClearNoServerHeartBeatAckCnt(void);
void SetMassageRunTime(u32 set);     /* unit is second*/
void SetMotorPositiveRunTime(u32 set);
void SetMotorReverseRunTime(u32 set);
s8 GetMassageState(void);
void SetLedOnCmd(bool set);
void SetMassageStartCmd(bool set);
void SetMassageStopCmd(bool set);

char * GetGprsSendBuffPtr(void);
u32 GetGprsSendBuffLength(void);
char * GetHeartBeatBuffPtr(void);
void TransferGprsSendBuff(char * buff, u32 len);
void TransferGprsReceiveBuff(char * buff, u32 len);
void CalcCommunicationID(u8 * idPtr, char *phoneNum, u8 deviceNum);  /*4 and 5 ID may be 8D 8E 8F*/
void GenerateHeartBeatData(u8 * idPtr, u8 status);
void GenerateDeviceStartWorkAck(u8 * idPtr, u8 paraLen, u8 res);
void GenerateDeviceStopWorkAck(u8 * idPtr, u8 paraLen, u8 res);
u8 CalcCheckSum(u8 *buf, u32 len);
void MyHexCopy(char* dest, const char* src, u32 len);
char * GetProtocolBuffPtr(void);
u32 GetProtocolBuffLength(void);


#endif